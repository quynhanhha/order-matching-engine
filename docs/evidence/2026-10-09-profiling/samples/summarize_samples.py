"""Aggregate macOS `sample` call graphs for the timed call sites of the archived benchmark.
Usage: python3 -I summarize_samples.py <sample.txt> <BM_name> <timed_offset> [<other_offset>=<label> ...]"""
import re, sys
from collections import Counter

path, bm, timed = sys.argv[1], sys.argv[2], int(sys.argv[3])
labels = {int(k): v for k, v in (a.split("=") for a in sys.argv[4:])}
labels[timed] = "TIMED"
rx = re.compile(r"^(\s*[+!:| ]*)(\d+) (.*)$")
text = open(path).read()
graph = text[text.index("Call graph:"):text.index("Total number in stack")]
nodes = []  # (depth, count, sym, children)
stack = []
for line in graph.splitlines()[1:]:
    m = rx.match(line)
    if not m:
        continue
    node = {"d": len(m.group(1)), "n": int(m.group(2)), "s": m.group(3), "c": []}
    # Strip image names: "(in libsystem_malloc.dylib)" must not classify free() as malloc.
    node["k"] = re.sub(r"\(in [^)]*\)", "", node["s"])
    while stack and stack[-1]["d"] >= node["d"]:
        stack.pop()
    if stack:
        stack[-1]["c"].append(node)
    else:
        nodes.append(node)
    stack.append(node)

def cat(path_syms):
    j = " | ".join(path_syms)
    if re.search(r"operator new|_malloc|\bmalloc", j): return "malloc (operator new path)"
    if re.search(r"operator delete|_xzm_free|\b_?free\b|\$\$free", j):
        return "free: memset/bzero inside free" if re.search(r"memset|bzero", path_syms[-1]) else "free (operator delete path)"
    if "__hash_table" in j: return "unordered_map code (excl. malloc/free)"
    if "vector<PriceLevel>::insert" in j: return "vector<PriceLevel>::insert"
    if "noOpCallback" in j: return "trade callback"
    if "addLimitOrder" in j: return "addLimitOrder self (inlined engine code)"
    if "cancelOrder" in j: return "cancelOrder self"
    return "other: " + path_syms[-1][:60]

totals = {}
def walk(node, syms, acc):
    syms = syms + [node["k"]]
    self_n = node["n"] - sum(c["n"] for c in node["c"])
    if self_n:
        acc[cat(syms)] += self_n
    for c in node["c"]:
        walk(c, syms, acc)

site_rx = re.compile(re.escape(bm) + r"\(benchmark::State&\)\s+\(in [^)]*\)\s+\+\s+(\d+)\s")
def find(node):
    m = site_rx.search(node["s"])
    if m:
        off = int(m.group(1))
        lab = labels.get(off, f"offset {off}")
        acc = totals.setdefault(lab, Counter())
        for c in node["c"]:
            walk(c, [], acc)
        acc["(site self)"] += node["n"] - sum(c["n"] for c in node["c"])
        return
    for c in node["c"]:
        find(c)
for n in nodes:
    find(n)
grand = sum(sum(a.values()) for a in totals.values())
print(f"# {path}\n# total samples under {bm} call sites: {grand}")
for lab, acc in sorted(totals.items(), key=lambda kv: -sum(kv[1].values())):
    t = sum(acc.values())
    print(f"\n## {lab}: {t} samples ({100*t/grand:.1f}% of {bm} samples)")
    for k, v in acc.most_common():
        print(f"  {v:7d} {100*v/t:6.1f}%  {k}")
