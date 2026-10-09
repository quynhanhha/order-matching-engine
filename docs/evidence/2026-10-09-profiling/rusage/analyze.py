"""Overhead-corrected rusage metrics with validation gates.
Bracket overhead comes from control-empty (per-window instructions/cycles).
Gates (metric suppressed if failed): window overhead < 5% of raw window instructions,
rep-to-rep CV of corrected instr/op and cycles/op < 2%, rusage-build ns/op within 3% of timing-build median."""
import re, statistics as st, sys, pathlib
d = pathlib.Path(sys.argv[1])
def parse(f):
    rows = []
    for l in f.read_text().splitlines():
        if " rep=" not in l: continue
        kv = dict(re.findall(r"(\w+)=([0-9.]+)", l))
        rows.append({k: float(v) for k, v in kv.items()})
    return rows
emp = parse(d / "rusage/control-empty-1.txt")
oi = st.median(r["instr_per_op"] for r in emp)      # ops == windows for control-empty
oc = st.median(r["cycles_per_op"] for r in emp)
print(f"bracket overhead per window: {oi:.1f} instructions, {oc:.1f} cycles")
for f in sorted((d / "rusage").glob("*.txt")):
    name = f.stem
    if name.startswith(("control-empty", "scan-", "seed-")) or name in ("analysis",) or name.endswith(".summary"): continue
    rows = parse(f)
    ci, cc, ns, ovf = [], [], [], []
    for r in rows:
        ops, b = r["ops"], r["batches"]
        raw_i, raw_c = r["instr_per_op"] * ops, r["cycles_per_op"] * ops
        ci.append((raw_i - b * oi) / ops); cc.append((raw_c - b * oc) / ops); ns.append(r["ns_per_op"])
        ovf.append(b * oi / raw_i)
    cv = lambda xs: st.pstdev(xs) / st.mean(xs) * 100
    tf = d / "timing" / (name + ".txt")
    tmed = None
    if tf.exists():
        m = re.search(r"median_ns_per_op=([0-9.]+)", tf.read_text()); tmed = float(m.group(1))
    nsdiff = (st.median(ns) / tmed - 1) * 100 if tmed else float("nan")
    gates = {"overhead<5%": max(ovf) < 0.05, "cv_instr<2%": cv(ci) < 2, "cv_cyc<2%": cv(cc) < 2,
             "ns_vs_timing<3%": (abs(nsdiff) < 3) if tmed else None}
    ok = all(v for v in gates.values() if v is not None)
    I, C = st.median(ci), st.median(cc)
    line = f"{name:28s} ns/op(rusage build)={st.median(ns):8.3f} vs timing={tmed if tmed else float('nan'):8.3f} ({nsdiff:+.1f}%)"
    if ok:
        line += f" | instr/op={I:9.2f} (cv {cv(ci):.2f}%) cycles/op={C:8.2f} (cv {cv(cc):.2f}%) IPC={I/C:.2f} cycles/ns={C/st.median(ns):.2f}"
    else:
        line += " | COUNTERS SUPPRESSED " + str({k: v for k, v in gates.items() if v is False})
    line += f" | max window overhead {100*max(ovf):.2f}%"
    print(line)
