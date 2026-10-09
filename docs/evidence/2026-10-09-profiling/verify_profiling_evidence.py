#!/usr/bin/env python3
"""Read-only verification of the 2026-10-09 hot-path profiling evidence bundle.

Recomputes every quantitative claim in docs/profiling-report.md from the raw files in this
bundle and compares it with the published (rounded) value. Also checks file hashes, Git
visibility, derived-summary reproducibility, and the recorded integrity/correctness results.

Run from anywhere:  PYTHONDONTWRITEBYTECODE=1 python3 docs/evidence/2026-10-09-profiling/verify_profiling_evidence.py
Standard library only. Runs no benchmarks.
"""
import hashlib
import json
import re
import statistics as st
import subprocess
import sys
from pathlib import Path

B = Path(__file__).resolve().parent
REPO = B.parents[2]
failures = []


def check(label, ok, detail=""):
    if not ok:
        failures.append(f"{label}: {detail}")
    print(f"[{'ok' if ok else 'FAIL'}] {label}{(' — ' + detail) if detail else ''}")


def close(label, value, expected, decimals):
    ok = round(value, decimals) == round(expected, decimals)
    check(label, ok, f"computed {value:.{decimals}f}, published {expected:.{decimals}f}")


def run_py(*args):
    return subprocess.run([sys.executable, "-I", *map(str, args)], capture_output=True, text=True, check=True).stdout


# ── 1. Integrity of the bundle ──────────────────────────────────────────────
for line in (B / "SHA256SUMS").read_text().splitlines():
    digest, name = line.split("  ", 1)
    actual = hashlib.sha256((B / name).read_bytes()).hexdigest()
    if actual != digest:
        check(f"sha256 {name}", False, "hash mismatch")
listed = {l.split("  ", 1)[1] for l in (B / "SHA256SUMS").read_text().splitlines()}
present = {str(p.relative_to(B)) for p in B.rglob("*") if p.is_file()} - {"SHA256SUMS"}
check("SHA256SUMS covers every bundle file", listed == present, str(sorted(present ^ listed)))
ignored = [p for p in sorted(present) if subprocess.run(["git", "check-ignore", "-q", str(B / p)], cwd=REPO).returncode == 0]
check("no bundle file is Git-ignored", not ignored, str(ignored))

# ── 2. Recorded provenance and integrity results ────────────────────────────
pre, post = (B / "preflight.txt").read_text(), (B / "postflight.txt").read_text()
binary = "ce8ce5ee5a094b7be31889a2b927909e56619a55be096269e55994d540cb17b6"
check("profiled executable = archived throughput executable (before and after)",
      pre.count(binary) == 2 and post.count(binary) == 1 and
      binary in (REPO / "docs/evidence/2026-10-09-throughput/binary-sha256.txt").read_text())
check("throughput evidence verifier passed before and after profiling", "verify exit=0" in pre and "verify exit=0" in post)
check("archived files unchanged during profiling", "UNCHANGED (1024 files)" in post)
check("profiling source tree clean at 0bad3d2",
      "0bad3d2c2bb9e9e1802c96f3cc8a7bc7bec0bd7f" in pre and "(end porcelain)" in post and
      post.split("== git\n")[1].split("\n")[1] == "(end porcelain)")
check("addLimitOrder machine code identical in executable and harness builds",
      "RESULT: IDENTICAL" in (B / "disassembly/codegen-identity.txt").read_text())
check("engine sources token-identical (NDEBUG) between a8edeb4 and 0bad3d2",
      (B / "source-equivalence.txt").read_text().count("identical:") == 3)
asan = (B / "asan/summary.txt").read_text()
check("all 13 harness workloads pass ASan/UBSan with assertions", asan.count("PASS") == 13 and "FAIL" not in asan)
dup = (B / "correctness/output-asan-debug.txt").read_text()
check("duplicate-ID probe: all 6 behaviors observed, sanitizer-clean",
      dup.count("[observed]") == 6 and "0 expectation(s) not observed" in dup)

# ── 3. Derived summaries are reproducible from raw data ─────────────────────
sites = {
    "add10000": ["BM_AddOnly_Resting", "2508", "2464=ctor(untimed)", "2544=teardown(untimed)"],
    "add1000": ["BM_AddOnly_Resting", "2508", "2464=ctor(untimed)", "2544=teardown(untimed)"],
    "match10000": ["BM_MatchOneToOne", "2436", "2348=ctor(untimed)", "2380=prepopulate(untimed)", "2604=teardown(untimed)", "2456=PauseTiming"],
    "match1000": ["BM_MatchOneToOne", "2436", "2348=ctor(untimed)", "2380=prepopulate(untimed)", "2604=teardown(untimed)", "2456=PauseTiming"],
}
for t, a in sites.items():
    out = subprocess.run([sys.executable, "-I", str(B / "samples/summarize_samples.py"), f"{t}.sample.txt", *a],
                         cwd=B / "samples", capture_output=True, text=True, check=True).stdout
    check(f"samples/{t}.summary.txt regenerates byte-for-byte", out == (B / f"samples/{t}.summary.txt").read_text())
check("rusage/analysis.txt regenerates byte-for-byte", run_py(B / "rusage/analyze.py", B) == (B / "rusage/analysis.txt").read_text())
regions = subprocess.run([sys.executable, "-I", str(B / "pcsamp/regions.py"), "pcsamp/add-10000.txt", "pcsamp/match-10000.txt"],
                         cwd=B, capture_output=True, text=True, check=True).stdout
check("pcsamp/regions.txt regenerates byte-for-byte", regions == (B / "pcsamp/regions.txt").read_text())

# ── 4. Published values ─────────────────────────────────────────────────────
print("\n# Sampling of the archived executable (timed call-site shares, %)")
def timed_shares(t):
    text = (B / f"samples/{t}.summary.txt").read_text()
    sec = text.split("## TIMED:")[1].split("\n## ")[0]
    total = int(re.match(r"\s*(\d+)", sec).group(1))
    return total, {k.strip(): int(n) for n, _, k in re.findall(r"^\s+(\d+)\s+([\d.]+)%\s+(.*)$", sec, re.M)}
def pct(d, total, *keys): return 100 * sum(d.get(k, 0) for k in keys) / total
M, F, Z, H, I, C = ("malloc (operator new path)", "free (operator delete path)", "free: memset/bzero inside free",
                    "unordered_map code (excl. malloc/free)", "addLimitOrder self (inlined engine code)", "trade callback")
for t, exp in {"add10000": (14041, 44.9, 0, 0, 11.1, 43.9, 0), "add1000": (12931, 43.9, 0, 0, 10.3, 45.4, 0),
               "match10000": (5866, 0, 54.1, 10.8, 20.4, 23.9, 1.6), "match1000": (5538, 0, 55.8, 10.7, 19.7, 23.0, 1.5)}.items():
    total, d = timed_shares(t)
    check(f"{t} timed samples", total == exp[0], f"{total}")
    close(f"{t} malloc share", pct(d, total, M), exp[1], 1)
    close(f"{t} free share incl. memset", pct(d, total, F, Z), exp[2], 1)
    close(f"{t} memset-inside-free share", pct(d, total, Z), exp[3], 1)
    close(f"{t} unordered_map code share", pct(d, total, H), exp[4], 1)
    close(f"{t} inlined engine share", pct(d, total, I), exp[5], 1)
    close(f"{t} callback share", pct(d, total, C), exp[6], 1)
for t, label, exp in [("add10000", "teardown(untimed)", 28.4), ("match10000", "prepopulate(untimed)", 49.7)]:
    m = re.search(rf"## {re.escape(label)}: \d+ samples \(([\d.]+)%", (B / f"samples/{t}.summary.txt").read_text())
    close(f"{t} {label} share of benchmark samples", float(m.group(1)), exp, 1)
td = (B / "samples/add10000.summary.txt").read_text().split("## teardown(untimed):")[1].split("\n## ")[0]
tdt = int(re.match(r"\s*(\d+)", td).group(1)); tz = int(re.search(r"(\d+)\s+[\d.]+%\s+free: memset", td).group(1))
close("add10000 memset share of teardown samples", 100 * tz / tdt, 22.4, 1)
for t, exp in {"add10000": 41.486, "add1000": 39.067, "match10000": 53.462, "match1000": 48.948}.items():
    b = json.loads((B / f"samples/{t}.bench.json").read_text())["benchmarks"][0]
    close(f"{t} rate during sampling (M/s)", b["items_per_second"] / 1e6, exp, 3)

print("\n# Harness timing (median ns/op over repetitions)")
def med(name): return float(re.search(r"median_ns_per_op=([\d.]+)", (B / f"timing/{name}.txt").read_text()).group(1))
timing = {"add-100": 23.11, "add-1000": 23.99, "add-10000": 23.69, "add-100000": 23.85, "add-1000000": 23.83,
          "match-100": 18.68, "match-1000": 18.31, "match-10000": 18.31, "match-100000": 18.83, "match-1000000": 18.96,
          "partial-10000": 4.77, "sweep1-2000": 18.63, "sweep10-2000": 160.62, "sweep100-2000": 1507.38,
          "cancel-fifo-10000": 23.49, "cancel-fifo-100000": 23.41, "cancel-fifo-1000000": 25.27,
          "cancel-shuffled-10000": 38.53, "cancel-shuffled-100000": 44.35, "cancel-shuffled-1000000": 186.22,
          "iso-new32-10000": 10.55, "iso-delete32-10000.run2": 10.46, "iso-umap-insert-10000.run2": 13.05,
          "iso-umap-erase-10000": 13.55, "levels1-10000": 17.18, "levels64-10000": 19.96, "levels256-10000": 24.59,
          "levels2048-10000": 90.41}
for k, v in timing.items():
    close(f"timing {k}", med(k), v, 2)
arch = json.loads((REPO / "docs/evidence/2026-10-09-throughput/summary.json").read_text())["benchmarks"]
for case, h, a in [("add-10000", 42.21, 41.478), ("match-10000", 54.63, 53.245), ("add-100", 43.27, 31.293), ("match-100", 53.54, 27.217)]:
    bm = ("BM_AddOnly_Resting/" if case.startswith("add") else "BM_MatchOneToOne/") + case.split("-")[1] + "/real_time"
    close(f"harness {case} M/s", 1e3 / med(case), h, 2)
    close(f"archived {bm} median M/s", arch[bm]["median_orders_per_second"] / 1e6, a, 3)
close("partial vs full fill gap (ns)", med("match-10000") - med("partial-10000"), 13.5, 1)

print("\n# Instruction/cycle counters (bracket-corrected, gated; from rusage/analysis.txt)")
an = (B / "rusage/analysis.txt").read_text()
oi, oc = map(float, re.search(r"bracket overhead per window: ([\d.]+) instructions, ([\d.]+) cycles", an).groups())
close("bracket overhead instructions", oi, 6024.6, 1); close("bracket overhead cycles", oc, 1378.8, 1)
def ctr(name):
    m = re.search(rf"^{re.escape(name)}\s.*instr/op=\s*([\d.]+).*cycles/op=\s*([\d.]+).*IPC=([\d.]+)", an, re.M)
    return tuple(map(float, m.groups())) if m else None
counters = {"add-10000": (417.2, 90.3, 4.6), "match-10000": (420.3, 70.0, 6.0), "partial-10000": (127.0, 18.7, 6.8),
            "cancel-fifo-10000": (343.5, 86.5, 4.0), "cancel-shuffled-10000": (359.5, 145.1, 2.5),
            "cancel-shuffled-100000": (359.6, 168.7, 2.1), "add-100000": (416.5, 91.0, 4.6),
            "match-100000": (420.7, 69.8, 6.0), "match-1000000": (420.7, 69.5, 6.1),
            "iso-new32-10000": (232.2, 39.6, 5.9), "iso-delete32-10000.run2": (203.3, 38.7, 5.3),
            "iso-umap-insert-10000.run2": (297.7, 48.9, 6.1), "iso-umap-erase-10000": (278.3, 51.2, 5.4),
            "sweep10-2000": (3399.1, 620.8, 5.5), "sweep100-2000": (33130.0, 5914.7, 5.6),
            "levels1-10000": (393.1, 65.4, 6.0), "levels2048-10000": (1274.5, 341.5, 3.7),
            "control-chain-10000000": (4.00, 2.00, 2.0), "control-chain-100000": (4.00, 2.01, 2.0)}
for k, (i, c, ipc) in counters.items():
    got = ctr(k)
    check(f"counters {k} not suppressed", got is not None)
    if got:
        close(f"{k} instr/op", got[0], i, 1); close(f"{k} cycles/op", got[1], c, 1); close(f"{k} IPC", got[2], ipc, 1)
suppressed = {m.group(1) for m in re.finditer(r"^(\S+)\s.*COUNTERS SUPPRESSED", an, re.M)}
check("suppressed metrics are exactly the published list", suppressed == {
    "control-chain-1000", "sweep1-2000", "add-1000000", "cancel-fifo-1000000", "cancel-shuffled-1000000",
    "iso-delete32-10000", "iso-umap-insert-10000"}, str(sorted(suppressed)))
raw10m = [float(x) for x in re.findall(r"instr_per_op=([\d.]+)", (B / "rusage/control-chain-10000000.txt").read_text())]
cyc10m = [float(x) for x in re.findall(r"cycles_per_op=([\d.]+)", (B / "rusage/control-chain-10000000.txt").read_text())]
close("control-chain 10M raw instr/iteration (median)", st.median(raw10m), 4.001, 3)
close("control-chain 10M raw cycles/iteration (median)", st.median(cyc10m), 2.004, 3)

def scan(path):
    rows = [dict((k, float(v)) for k, v in re.findall(r"(\w+)=([0-9.]+)", l)) for l in path.read_text().splitlines() if " rep=" in l]
    I = st.median([(r["instr_per_op"] * r["ops"] - r["batches"] * oi) / r["ops"] for r in rows])
    C = st.median([(r["cycles_per_op"] * r["ops"] - r["batches"] * oc) / r["ops"] for r in rows])
    return I, C
print("\n# Level-count scan and seed check (bracket-corrected medians)")
for k, (i, c) in {1: (393.6, 65.8), 2: (402.9, 64.6), 3: (402.2, 65.3), 4: (406.6, 86.5), 6: (411.8, 67.8), 8: (413.4, 80.0),
                  10: (416.7, 90.3), 12: (419.1, 77.7), 16: (422.0, 76.4), 24: (427.8, 85.3), 32: (430.4, 75.3),
                  48: (436.4, 93.6), 64: (439.8, 75.2), 96: (447.6, 106.2), 128: (452.4, 80.7), 256: (473.3, 92.3),
                  512: (524.2, 116.9), 1024: (708.2, 178.6)}.items():
    I, C = scan(B / f"rusage/scan-levels{k}.txt")
    close(f"scan K={k} instr/op", I, i, 1); close(f"scan K={k} cycles/op", C, c, 1)
for k, (lo, hi) in {4: (87.3, 88.0), 6: (68.9, 70.0), 10: (91.1, 96.3), 16: (78.2, 83.2), 48: (96.8, 97.1), 64: (76.0, 81.6)}.items():
    cs = [scan(B / f"rusage/seed-levels{k}-s{s}.txt")[1] for s in (42, 7, 1234, 99)]
    close(f"seed K={k} min cycles/op", min(cs), lo, 1); close(f"seed K={k} max cycles/op", max(cs), hi, 1)

print("\n# Allocation counts inside timed regions (per API call)")
for name, (a, f, maxsz) in {"add-100": (1, 0, 32), "add-1000": (1, 0, 32), "add-10000": (1, 0, 32),
                             "match-100": (0, 1, 0), "match-1000": (0, 1, 0), "match-10000": (0, 1, 0),
                             "partial-10000": (0, 0, 0), "cancel-fifo-10000": (0, 1, 0), "cancel-shuffled-10000": (0, 1, 0),
                             "sweep1-2000": (0, 1, 0), "sweep10-2000": (0, 10, 0), "sweep100-2000": (0, 100, 0)}.items():
    for line in (B / f"alloc/{name}.txt").read_text().splitlines():
        if "alloc_per_op" in line:
            g = dict(re.findall(r"(\w+)=([\d.]+)", line))
            check(f"alloc {name} {line.split()[1]}", float(g["alloc_per_op"]) == a and float(g["free_per_op"]) == f and int(g["max_size"]) == maxsz,
                  f"alloc/op {g['alloc_per_op']} free/op {g['free_per_op']} max size {g['max_size']}")

print("\n# In-process PC sampler")
def groups(f):
    txt = (B / f"pcsamp/{f}.txt").read_text()
    tot = int(re.search(r"samples in timed region: (\d+)", txt).group(1))
    sec = txt.split("## by symbol\n")[1].split("## by symbol+offset")[0]
    g = {}
    for n, img, sym in re.findall(r"^\s*(\d+)\s+[\d.]+% (\S+) (.*)$", sec, re.M):
        k = ("hash" if "__hash_table" in sym else "engine" if ("addLimitOrder" in sym or "CancelWorkload" in sym)
             else "callback" if "noOpCallback" in sym
             else "allocator" if img in ("libsystem_malloc.dylib", "libsystem_platform.dylib", "libc++abi.dylib") else "other")
        g[k] = g.get(k, 0) + int(n)
    return tot, {k: 100 * v / tot for k, v in g.items()}
for f, (n, eng, alloc, hsh) in {"add-10000": (4372, 54.9, 32.8, 10.7), "match-10000": (2178, 31.1, 47.9, 17.5),
                                 "cancel-fifo-10000": (2200, 54.5, 36.2, 8.1), "cancel-shuffled-10000": (3352, 73.3, 20.7, 5.3)}.items():
    tot, g = groups(f)
    check(f"pcsamp {f} timed samples", tot == n, str(tot))
    close(f"pcsamp {f} inlined engine", g["engine"], eng, 1); close(f"pcsamp {f} allocator libs", g["allocator"], alloc, 1)
    close(f"pcsamp {f} hash table", g["hash"], hsh, 1)
close("pcsamp match-10000 callback", groups("match-10000")[1]["callback"], 1.2, 1)
reg = (B / "pcsamp/regions.txt").read_text()
def region(wl, label):
    sec = reg.split(f"== pcsamp/{wl}.txt")[1].split("==")[0]
    return float(re.search(rf"([\d.]+)%\s+{re.escape(label)}", sec).group(1))
close("pcsamp add lower_bound (bid) share", region("add-10000", "Buy resting: findOrCreateBidLevel"), 19.9, 1)
close("pcsamp add lower_bound (ask) share", region("add-10000", "Sell resting: findOrCreateAskLevel"), 19.4, 1)
close("pcsamp match pool+init share", region("match-10000", "prologue + OrderPool::allocate"), 10.9, 1)
close("pcsamp match loop head/fill/dispatch share", region("match-10000", "Buy incoming: crossing check"), 8.6, 1)
# lower_bound loop bodies + exit instructions in the inlined cancelOrder (see disassembly excerpt):
# asks [0x1dc, 0x204), bids [0x220, 0x248). The hot side-dispatch load at +0x1c0 is deliberately excluded.
for f, exp in {"cancel-fifo-10000": 35.1, "cancel-shuffled-10000": 26.5}.items():
    txt = (B / f"pcsamp/{f}.txt").read_text(); tot = int(re.search(r"timed region: (\d+)", txt).group(1))
    s = sum(int(n) for n, o in re.findall(r"^\s*(\d+)\s+[\d.]+% _ZN14CancelWorkload5batch\S*\+0x([0-9a-f]+)$", txt, re.M)
            if 0x1dc <= int(o, 16) < 0x204 or 0x220 <= int(o, 16) < 0x248)
    close(f"pcsamp {f} lower_bound loop share", 100 * s / tot, exp, 1)
for f in ("add-10000", "match-10000", "cancel-fifo-10000", "cancel-shuffled-10000"):
    runs = (B / f"pcsamp/{f}.run.txt").read_text()
    secs = sum(int(x) for x in re.findall(r"timed_ns=(\d+)", runs)) / 1e9
    tot = groups(f)[0]
    check(f"pcsamp {f} effective timed-sample rate < 1 kHz (requested 4 kHz)", tot / secs < 1000, f"{tot / secs:.0f} samples per timed second")

print("\n# Derived figures quoted in the report")
total, d = timed_shares("match10000"); close("fill: free + hash-erase share", pct(d, total, F, Z, H), 74.5, 1)
total, d = timed_shares("add10000"); close("addition: allocation + hash share", pct(d, total, M, H), 56.0, 1)
g = groups("add-10000")[1]; close("PC sampler addition: allocator + hash share", g["allocator"] + g["hash"], 43.6, 1)
close("isolated new(32) as share of addition", 100 * med("iso-new32-10000") / med("add-10000"), 44.5, 1)
close("sweep10 ns per fill", med("sweep10-2000") / 10, 16.06, 2); close("sweep100 ns per fill", med("sweep100-2000") / 100, 15.07, 2)
close("sweep10 instructions per fill", ctr("sweep10-2000")[0] / 10, 339.9, 1); close("sweep100 instructions per fill", ctr("sweep100-2000")[0] / 100, 331.3, 1)
for name, lo, hi in [("cancel-fifo-1000000", 23.42, 28.16), ("cancel-shuffled-1000000", 182.00, 193.18)]:
    m = re.search(r"min=([\d.]+) max=([\d.]+)", (B / f"timing/{name}.txt").read_text())
    close(f"{name} min ns", float(m.group(1)), lo, 2); close(f"{name} max ns", float(m.group(2)), hi, 2)
sc = {k: scan(B / f"rusage/scan-levels{k}.txt") for k in (1, 2, 3, 4, 6, 8, 10, 12, 16, 24, 32, 48, 64, 96, 128)}
close("K<=128 instruction spread", max(v[0] for v in sc.values()) - min(v[0] for v in sc.values()), 58.8, 1)
close("K<=128 cycle min", min(v[1] for v in sc.values()), 64.6, 1); close("K<=128 cycle max", max(v[1] for v in sc.values()), 106.2, 1)
pf, cpn = [], []
for f in (B / "rusage").glob("*.txt"):
    for l in f.read_text().splitlines():
        m = re.search(r"p_instr_frac=([\d.]+)", l)
        if m: pf.append(float(m.group(1)))
        m = re.search(r"cycles_per_ns=([\d.]+)", l)
        if m and not f.name.startswith(("control-empty", "control-chain-1000.txt")): cpn.append(float(m.group(1)))
close("min performance-core instruction fraction (%)", 100 * min(pf), 95.7, 1); close("max performance-core instruction fraction (%)", 100 * max(pf), 100.0, 1)
close("derived cycles/ns min", min(cpn), 3.5, 1); close("derived cycles/ns max", max(cpn), 4.0, 1)
rates = []
for f in ("add-10000", "match-10000", "cancel-fifo-10000", "cancel-shuffled-10000"):
    secs = sum(int(x) for x in re.findall(r"timed_ns=(\d+)", (B / f"pcsamp/{f}.run.txt").read_text())) / 1e9
    rates.append(groups(f)[0] / secs)
close("PC sampler min samples per timed second", min(rates), 229, 0); close("PC sampler max samples per timed second", max(rates), 374, 0)
for bm, lo, hi in [("BM_AddOnly_Resting/10000/real_time", 39.871, 42.162), ("BM_AddOnly_Resting/1000/real_time", 38.723, 40.224),
                   ("BM_MatchOneToOne/10000/real_time", 51.133, 54.254), ("BM_MatchOneToOne/1000/real_time", 47.790, 49.690)]:
    close(f"archived {bm} min M/s", arch[bm]["min_orders_per_second"] / 1e6, lo, 3)
    close(f"archived {bm} max M/s", arch[bm]["max_orders_per_second"] / 1e6, hi, 3)
env = (B / "environment-before.txt").read_text() + (B / "environment-after.txt").read_text()
check("environment: M3 Pro, 6+6 cores, 128-B line, 128 KiB L1D / 16 MiB L2 (P cluster), Apple Clang 21",
      all(s in env for s in ("Apple M3 Pro", "hw.perflevel0.physicalcpu: 6", "hw.perflevel1.physicalcpu: 6", "hw.cachelinesize: 128",
                             "hw.perflevel0.l1dcachesize: 131072", "hw.perflevel0.l2cachesize: 16777216", "clang-2100.1.1.101")))
check("environment: AC power and no thermal/performance warning before and after",
      env.count("AC Power") == 2 and env.count("No thermal warning level") == 2 and env.count("No performance warning level") == 2)

print("\n# Harness provenance (harness/PROVENANCE.md, harness/provenance-audit.txt)")
aud = (B / "harness/provenance-audit.txt").read_text()
check("rev3 rebuild matches all four archived final binaries", aud.count(" MATCH") == 4 and "DIFFER" not in aud)
check("rev1 rebuild reproduces archived first-build disassembly", "IDENTICAL (11731 instruction lines, addresses included)" in aud)
check("timed functions unchanged across revisions in all 8 comparisons", aud.count("unchanged: True") == 8 and "unchanged: False" not in aud)
check("levelsK inputs identical across revisions for all 18 K != 10", aud.count("identical in rev1, rev2, rev3") == 18)
check("K=10: rev2 = rev3 (rev1 path never run)", "K=10 " in aud and "rev2 = rev3; rev1 differs" in aud)
check("generic K=10 inputs = archived add/10000 inputs up to a price offset", aud.count("identical except a constant +9910 price offset per side") == 2)
check("GLEVELS_SEED=42 equals unset", "same inputs as with the variable unset" in aud)
check("published harness is rev3", hashlib.sha256((B / "harness/prof_harness.cpp").read_bytes()).hexdigest().startswith(
    re.search(r"rev3 ([0-9a-f]{16})", aud).group(1)))
trace = (B / "commands.sh").read_text().splitlines()
check("command trace marks both rebuilds at lines 41 and 60", trace[40].startswith("# harness rebuilt") and trace[59].startswith("# harness rebuilt"))
check("price-level output names match revisions (rev1 add-levels*, rev2/rev3 add-glevels*)",
      all((B / f"rusage/levels{k}-10000.txt").read_text().startswith(f"add-levels{k}/") for k in (1, 64, 256, 2048)) and
      all((B / f"rusage/scan-levels{k}.txt").read_text().startswith(f"add-glevels{k}/") for k in (1, 10, 1024)) and
      (B / "rusage/seed-levels10-s7.txt").read_text().startswith("add-glevels10/"))
for k, (a, b) in {4: (86.5, 87.4), 6: (67.8, 69.5), 10: (90.3, 91.1), 16: (76.4, 83.2), 48: (93.6, 96.8), 64: (75.2, 76.3)}.items():
    close(f"cross-run K={k} scan cycles", scan(B / f"rusage/scan-levels{k}.txt")[1], a, 1)
    close(f"cross-run K={k} seed-42 cycles", scan(B / f"rusage/seed-levels{k}-s42.txt")[1], b, 1)
for k, (a, b) in {1: (65.4, 65.8), 64: (75.0, 75.2), 256: (92.7, 92.3)}.items():
    close(f"cross-run K={k} rev1 levels cycles", scan(B / f"rusage/levels{k}-10000.txt")[1], a, 1)
    close(f"cross-run K={k} rev2 scan cycles", scan(B / f"rusage/scan-levels{k}.txt")[1], b, 1)
diffs = [abs(scan(B / f"rusage/seed-levels{k}-s42.txt")[1] - scan(B / f"rusage/scan-levels{k}.txt")[1]) for k in (4, 6, 10, 16, 48, 64)]
close("max cross-run cycle difference", max(diffs), 6.8, 1)
idiffs = [abs(scan(B / f"rusage/seed-levels{k}-s42.txt")[0] - scan(B / f"rusage/scan-levels{k}.txt")[0]) for k in (4, 6, 10, 16, 48, 64)]
check("cross-run instruction difference <= 0.7", max(idiffs) <= 0.75, f"{max(idiffs):.2f}")

print("\n# Additional report and methodology statements")
for bm, exp in [("BM_AddOnly_Resting", 24.6), ("BM_MatchOneToOne", 48.9)]:
    drop = 100 * (1 - arch[f"{bm}/100/real_time"]["median_orders_per_second"] / arch[f"{bm}/10000/real_time"]["median_orders_per_second"])
    close(f"archived {bm} N=100 rate below N=10000 (%)", drop, exp, 1)
for case, bm in [("add-10000", "BM_AddOnly_Resting/10000/real_time"), ("match-10000", "BM_MatchOneToOne/10000/real_time")]:
    rel = 100 * abs(1e3 / med(case) / (arch[bm]["median_orders_per_second"] / 1e6) - 1)
    check(f"harness {case} within 3% of archive", rel < 3, f"{rel:.2f}%")
total, d = timed_shares("match10000"); close("memset share of free path (%)", 100 * d[Z] / (d[F] + d[Z]), 20.0, 0)
close("pcsamp add pool+init region", region("add-10000", "prologue + OrderPool::allocate"), 6.9, 1)
close("pcsamp add append/emplace region", region("add-10000", "level hit/insert"), 5.6, 1)
sh = (B / "pcsamp/cancel-shuffled-10000.txt").read_text()
top = [int(o, 16) for _, o in re.findall(r"^\s*(\d+)\s+[\d.]+% _ZN14CancelWorkload5batch\S*\+0x([0-9a-f]+)$", sh, re.M)[:12]]
check("shuffled-cancel hot offsets include bucket/node/Order/side loads (+0x130,+0x158,+0x1b4,+0x1c0)",
      all(o in top for o in (0x130, 0x158, 0x1b4, 0x1c0)), str([hex(o) for o in top]))
exc = (B / "disassembly/harness-pcsamp-addLimitOrder-CancelWorkload.txt").read_text()
check("those offsets are the documented loads in the disassembly excerpt",
      all(re.search(p, exc) for p in (r"\n\S+\s+ldr\tx2, \[x12\]", r"\n\S+\s+ldr\tx11, \[x2, #0x8\]", r"\n\S+\s+ldr\tx25, \[x2, #0x18\]", r"\n\S+\s+ldr\tw8, \[x25, #0x20\]")))
tools = (B / "tools-availability.txt").read_text()
check("tool record: xctrace/instruments/llvm-mca missing, SIP enabled, DTrace blocked, perf/valgrind absent",
      'unable to find utility "xctrace"' in tools and 'unable to find utility "instruments"' in tools and
      'unable to find utility "llvm-mca"' in tools and "status: enabled" in tools and "DTrace requires additional privileges" in tools
      and re.search(r"\$ command -v perf valgrind\n\n", tools) is not None)

close("pcsamp add lower_bound total (bids + asks)", region("add-10000", "Buy resting: findOrCreateBidLevel") + region("add-10000", "Sell resting: findOrCreateAskLevel"), 39.3, 1)

print("\n# Confirmed (code/disassembly) facts cited in the report")
mexc = (B / "disassembly/measured-addLimitOrder-cancelOrder.txt").read_text()
check("Order is 56 bytes: index = (ptr diff >> 3) * inverse(7)",
      "movk\tx25, #0x6db6, lsl #48" in mexc and "asr\tx8, x8, #3" in mexc)
check("PriceLevel is 24 bytes (0x18 stride in level search)", "mov\tw9, #0x18" in mexc)
check("bucket array holds 8-byte pointers (lsl #3 bucket load)", re.search(r"ldr\tx11, \[x11, x8, lsl #3\]", mexc) is not None)
check("bucket index uses a hardware divide", "udiv" in mexc)
check("trade callback is an indirect call (blr)", "blr\tx10" in mexc)
check("hash-table insert/erase are out-of-line calls; delete called on cancel",
      "__emplace_unique_key_args" in mexc and "__erase_unique" in mexc and "__ZdlPv" in mexc)
src = (REPO / "include/order_book.h").read_text()
check("4,096-level reservation and 0.7 load factor in source", "kDefaultMaxPriceLevels = 4096" in src and "max_load_factor(0.7f)" in src)
check("4,096-level capacity is assert-only (Debug)", 'assert(bids_.size() < bids_.capacity()' in src)
check("duplicate index entries are dropped by try_emplace", "orderIndex_.try_emplace(id, order);" in src)
check("README states the unique-ID precondition and the duplicate-ID limitation",
      "Callers supply unique order IDs" in (REPO / "README.md").read_text() and "Duplicate IDs are not detected" in (REPO / "README.md").read_text())

print()
if failures:
    print(f"{len(failures)} check(s) FAILED")
    sys.exit(1)
print("All profiling evidence checks passed: bundle hashes and Git visibility, recorded provenance and integrity,")
print("reproducible derived summaries, and every published sampling, timing, counter, allocation and PC-sampling value.")
