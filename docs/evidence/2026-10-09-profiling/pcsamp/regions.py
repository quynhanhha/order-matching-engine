"""Group in-process PC samples inside addLimitOrder into source-level regions.
Region boundaries (byte offsets) were read from the disassembly of addLimitOrder, which is
instruction-identical in the measured benchmark binary and in prof_pcsamp."""
import re, sys
regions = [  # [start, end) offsets; Side::Buy == 0, so the cbz at +0x8c branches to the Buy path
    (0x000, 0x090, "prologue + OrderPool::allocate (free-list pop, isAllocated_ store) + Order::init"),
    (0x090, 0x1e0, "Sell incoming: crossing check + matchSell loop (not exercised by Buy workloads)"),
    (0x1e0, 0x29c, "Buy incoming: crossing check + matchBuy loop head, SMP check, fill arithmetic, callback dispatch"),
    (0x29c, 0x2e8, "matchBuy: exhausted check + FIFO unlink (PriceLevel::remove) + index erase call"),
    (0x2e8, 0x324, "matchBuy: resting OrderPool::deallocate + level-empty check + loop back-edge"),
    (0x324, 0x3d8, "matchBuy/matchSell: asks_/bids_.resize(numLevels) bookkeeping"),
    (0x3d8, 0x438, "Sell resting: findOrCreateAskLevel lower_bound loop"),
    (0x438, 0x468, "fully filled incoming: OrderPool::deallocate"),
    (0x468, 0x4b4, "Buy resting: findOrCreateBidLevel lower_bound loop"),
    (0x4b4, 0x528, "level hit/insert + PriceLevel::addToTail + index emplace call"),
    (0x528, 0x550, "epilogue (incl. return from index emplace)"),
]
for f in sys.argv[1:]:
    txt = open(f).read()
    tot = int(re.search(r"samples in timed region: (\d+)", txt).group(1))
    acc = {r[2]: 0 for r in regions}
    for n, o in re.findall(r"^\s*(\d+)\s+[\d.]+% _ZN9OrderBookIPFvRK5TradeEE13addLimitOrderE4Sidejjyy\+0x([0-9a-f]+)$", txt, re.M):
        o = int(o, 16)
        for s, e, name in regions:
            if s <= o < e:
                acc[name] += int(n); break
    print(f"== {f} ({tot} timed samples)")
    for name, n in sorted(acc.items(), key=lambda kv: -kv[1]):
        if n: print(f"  {n:6d} {100*n/tot:5.1f}%  {name}")
