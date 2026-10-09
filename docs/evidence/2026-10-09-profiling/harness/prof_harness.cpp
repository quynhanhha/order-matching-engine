// Phase 3 profiling-only harness. NOT part of the project; lives in an ignored directory.
// Build variants (see build.sh):
//   prof_timing : primary wall-clock timing (no allocation hooks)
//   prof_rusage : timing + proc_pid_rusage instruction/cycle deltas around each timed batch
//   prof_alloc  : global operator new/delete counting inside timed regions only (timing NOT reported)
//   prof_pcsamp : SIGPROF in-process PC sampler restricted to timed regions
#include "order_book.h"
#include "throughput_workloads.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <libproc.h>
#include <mach/mach_time.h>
#include <new>
#include <numeric>
#include <random>
#include <signal.h>
#include <string>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

using throughput_workloads::OrderInput;

// ───────────────────────────── allocation counting ─────────────────────────────
#ifdef COUNT_ALLOCS
namespace alloc_count {
// Plain globals: the harness is single-threaded. Recording does no allocation and no I/O.
bool enabled = false;
uint64_t news = 0, deletes = 0, bytes = 0;
constexpr int kBins = 33;               // bins of 16 bytes up to 512, last bin = larger
uint64_t sizeBins[kBins] = {};
uint64_t maxSize = 0;
inline void onNew(std::size_t n) {
    if (!enabled) return;
    ++news; bytes += n;
    int b = static_cast<int>((n + 15) / 16);
    if (b >= kBins) b = kBins - 1;
    ++sizeBins[b];
    if (n > maxSize) maxSize = n;
}
inline void onDelete(void* p) { if (enabled && p) ++deletes; }
void reset() { news = deletes = bytes = maxSize = 0; std::memset(sizeBins, 0, sizeof sizeBins); }
}  // namespace alloc_count

void* operator new(std::size_t n) {
    alloc_count::onNew(n);
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
    alloc_count::onNew(n);
    return std::malloc(n ? n : 1);
}
void* operator new[](std::size_t n, const std::nothrow_t& t) noexcept { return ::operator new(n, t); }
void* operator new(std::size_t n, std::align_val_t a) {
    alloc_count::onNew(n);
    void* p = nullptr;
    std::size_t al = std::max<std::size_t>(static_cast<std::size_t>(a), sizeof(void*));
    if (posix_memalign(&p, al, n ? n : 1) != 0) throw std::bad_alloc();
    return p;
}
void* operator new[](std::size_t n, std::align_val_t a) { return ::operator new(n, a); }
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {
    alloc_count::onNew(n);
    void* p = nullptr;
    std::size_t al = std::max<std::size_t>(static_cast<std::size_t>(a), sizeof(void*));
    return posix_memalign(&p, al, n ? n : 1) == 0 ? p : nullptr;
}
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t& t) noexcept { return ::operator new(n, a, t); }
void operator delete(void* p) noexcept { alloc_count::onDelete(p); std::free(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { ::operator delete(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { ::operator delete(p); }
void operator delete(void* p, std::align_val_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::align_val_t) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { ::operator delete(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { ::operator delete(p); }
#endif

// ───────────────────────────── PC sampler ─────────────────────────────
#ifdef PC_SAMPLE
namespace pcsamp {
volatile sig_atomic_t inTimed = 0;
constexpr std::size_t kMax = 4'000'000;
uintptr_t* pcs = nullptr;
volatile std::size_t count = 0;
uint64_t outside = 0;
void handler(int, siginfo_t*, void* ctx) {
    if (!inTimed) { ++outside; return; }
    auto* uc = static_cast<ucontext_t*>(ctx);
    uintptr_t pc = static_cast<uintptr_t>(__darwin_arm_thread_state64_get_pc(uc->uc_mcontext->__ss));
    std::size_t c = count;
    if (c < kMax) { pcs[c] = pc; count = c + 1; }
}
void start(int usec) {
    pcs = static_cast<uintptr_t*>(std::malloc(kMax * sizeof(uintptr_t)));
    struct sigaction sa{};
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPROF, &sa, nullptr);
    itimerval tv{{0, usec}, {0, usec}};
    setitimer(ITIMER_PROF, &tv, nullptr);
}
void stopAndReport(const char* path) {
    itimerval tv{};
    setitimer(ITIMER_PROF, &tv, nullptr);
    FILE* f = std::fopen(path, "w");
    std::unordered_map<std::string, uint64_t> bySym;
    std::unordered_map<std::string, uint64_t> byOff;
    for (std::size_t i = 0; i < count; ++i) {
        Dl_info info{};
        std::string sym = "?", img = "?";
        uintptr_t off = 0;
        if (dladdr(reinterpret_cast<void*>(pcs[i]), &info) && info.dli_sname) {
            sym = info.dli_sname;
            off = pcs[i] - reinterpret_cast<uintptr_t>(info.dli_saddr);
        }
        if (info.dli_fname) { img = info.dli_fname; img = img.substr(img.rfind('/') + 1); }
        bySym[img + " " + sym]++;
        char buf[64];
        std::snprintf(buf, sizeof buf, "+0x%lx", static_cast<unsigned long>(off));
        byOff[sym + buf]++;
    }
    std::fprintf(f, "# samples in timed region: %zu; ticks outside timed region: %" PRIu64 "\n", static_cast<std::size_t>(count), outside);
    auto dump = [&](auto& m, const char* title) {
        std::vector<std::pair<std::string, uint64_t>> v(m.begin(), m.end());
        std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.second > b.second; });
        std::fprintf(f, "## %s\n", title);
        for (auto& [k, n] : v) std::fprintf(f, "%8" PRIu64 " %6.2f%% %s\n", n, 100.0 * n / count, k.c_str());
    };
    dump(bySym, "by symbol");
    dump(byOff, "by symbol+offset");
    std::fclose(f);
}
}  // namespace pcsamp
#endif

// ───────────────────────────── counters ─────────────────────────────
struct Counters { uint64_t instr = 0, cycles = 0, pinstr = 0, pcycles = 0; };
[[maybe_unused]] static inline Counters readCounters() {
    rusage_info_v6 ri{};
    proc_pid_rusage(getpid(), RUSAGE_INFO_V6, reinterpret_cast<rusage_info_t*>(&ri));
    return {ri.ri_instructions, ri.ri_cycles, ri.ri_pinstructions, ri.ri_pcycles};
}
static inline uint64_t nowNs() { return clock_gettime_nsec_np(CLOCK_UPTIME_RAW); }

struct Accum {
    uint64_t ops = 0, ns = 0;
    Counters c;
};

// A timed region: wraps exactly the measured API calls.
template<typename F>
static inline void timed(Accum& acc, uint64_t ops, F&& body) {
#ifdef COUNT_ALLOCS
    alloc_count::enabled = true;
#endif
#ifdef PC_SAMPLE
    pcsamp::inTimed = 1;
#endif
#ifdef USE_RUSAGE
    const Counters c0 = readCounters();
#endif
    const uint64_t t0 = nowNs();
    body();
    const uint64_t t1 = nowNs();
#ifdef USE_RUSAGE
    const Counters c1 = readCounters();
    acc.c.instr += c1.instr - c0.instr;
    acc.c.cycles += c1.cycles - c0.cycles;
    acc.c.pinstr += c1.pinstr - c0.pinstr;
    acc.c.pcycles += c1.pcycles - c0.pcycles;
#endif
#ifdef PC_SAMPLE
    pcsamp::inTimed = 0;
#endif
#ifdef COUNT_ALLOCS
    alloc_count::enabled = false;
#endif
    acc.ops += ops;
    acc.ns += t1 - t0;
}

static void noOpCallback(const Trade&) {}
using Book = OrderBook<void (*)(const Trade&)>;

[[noreturn]] static void die(const std::string& msg) { std::fprintf(stderr, "FATAL: %s\n", msg.c_str()); std::exit(1); }

// ───────────────────────────── workloads ─────────────────────────────
// Each workload: prepare() untimed, then batch(acc) builds a fresh book (untimed), runs the
// timed region, checks post-state (untimed) and destroys the book (untimed).
struct Workload {
    virtual ~Workload() = default;
    virtual void batch(Accum& acc) = 0;
    virtual const char* name() const = 0;
};

static void addAll(Book& b, const std::vector<OrderInput>& v) {
    for (const auto& i : v) b.addLimitOrder(i.side, i.price, i.quantity, i.id, i.participantId);
}

// (a/b) identical to BM_AddOnly_Resting inputs.
struct AddWorkload : Workload {
    std::vector<OrderInput> in;
    std::string nm;
    explicit AddWorkload(std::size_t n, uint32_t spreadLevels = 10, bool generic = false) {
        if (spreadLevels == 10 && !generic) {
            std::mt19937_64 rng(42);
            in = throughput_workloads::generateRestingOrders(n, rng);
            nm = "add/" + std::to_string(n);
        } else {
            // K distinct prices per side, non-crossing, deterministic (optional GLEVELS_SEED override).
            const char* seedEnv = std::getenv("GLEVELS_SEED");
            std::mt19937_64 rng(seedEnv ? std::strtoull(seedEnv, nullptr, 10) : 42);
            std::uniform_int_distribution<uint32_t> pd(0, spreadLevels - 1), qd(1, 100);
            std::uniform_int_distribution<uint64_t> partd(1, 100);
            const uint32_t bidBase = 10000, askBase = bidBase + spreadLevels + 10;
            for (std::size_t i = 0; i < n; ++i) {
                const bool buy = (i % 2 == 0);
                in.push_back({buy ? Side::Buy : Side::Sell, (buy ? bidBase : askBase) + pd(rng), qd(rng), i + 1, partd(rng)});
            }
            nm = "add-glevels" + std::to_string(spreadLevels) + "/" + std::to_string(n);
        }
        auto v = throughput_workloads::validateRestingOrders(in);
        if (!v.error.empty()) die(nm + ": " + v.error);
    }
    void batch(Accum& acc) override {
        Book book(in.size() + 100, noOpCallback);
        auto* obs = &book;
        asm volatile("" : : "r"(obs) : "memory");
        timed(acc, in.size(), [&] { addAll(book, in); asm volatile("" : : "r"(obs) : "memory"); });
    }
    const char* name() const override { return nm.c_str(); }
};

// (a) identical to BM_MatchOneToOne.
struct MatchWorkload : Workload {
    throughput_workloads::MatchingInputs in;
    std::string nm;
    explicit MatchWorkload(std::size_t n) : in(throughput_workloads::generateMatchingOrders(n)) {
        nm = "match/" + std::to_string(n);
        auto v = throughput_workloads::validateMatchingOrders(in);
        if (!v.error.empty()) die(nm + ": " + v.error);
    }
    void batch(Accum& acc) override {
        const std::size_t nr = in.resting.size(), nc = in.incoming.size();
        Book book(nr + nc + 100, noOpCallback);
        addAll(book, in.resting);
        auto* obs = &book;
        asm volatile("" : : "r"(obs) : "memory");
        timed(acc, nc, [&] { addAll(book, in.incoming); asm volatile("" : : "r"(obs) : "memory"); });
        const auto* a = book.bestAsk();
        if (book.bestBid() || !a || a->price != 100 || a->totalQuantity != nr - nc) die("match post-state");
    }
    const char* name() const override { return nm.c_str(); }
};

// (f) partial fills: same shape as match/N (N resting sells @100, N/2 incoming buys qty 1), but
// each resting order has qty N, so every incoming order partially fills resting order #1 and no
// resting order is exhausted: N/2 trades, zero index erases, zero resting-order deallocations.
struct PartialWorkload : Workload {
    throughput_workloads::MatchingInputs in;
    std::string nm;
    explicit PartialWorkload(std::size_t n) : in(throughput_workloads::generateMatchingOrders(n)) {
        for (auto& r : in.resting) r.quantity = static_cast<uint32_t>(n);
        nm = "partial/" + std::to_string(n);
        // Untimed validation replay with recording callback.
        std::vector<Trade> trades;
        trades.reserve(in.incoming.size());
        OrderBook vb(n + n / 2 + 100, [&trades](const Trade& t) { trades.push_back(t); });
        for (auto& r : in.resting) vb.addLimitOrder(r.side, r.price, r.quantity, r.id, r.participantId);
        for (auto& r : in.incoming) vb.addLimitOrder(r.side, r.price, r.quantity, r.id, r.participantId);
        if (trades.size() != in.incoming.size()) die("partial: trade count");
        for (std::size_t i = 0; i < trades.size(); ++i)
            if (trades[i].sellOrderId != 1 || trades[i].buyOrderId != in.incoming[i].id || trades[i].quantity != 1) die("partial: trade content");
        const auto* a = vb.bestAsk();
        if (vb.bestBid() || !a || a->head->orderId != 1 || a->head->quantity != n - n / 2 ||
            a->totalQuantity != static_cast<uint64_t>(n) * n - n / 2) die("partial: final state");
    }
    void batch(Accum& acc) override {
        const std::size_t nr = in.resting.size(), nc = in.incoming.size();
        Book book(nr + nc + 100, noOpCallback);
        addAll(book, in.resting);
        auto* obs = &book;
        asm volatile("" : : "r"(obs) : "memory");
        timed(acc, nc, [&] { addAll(book, in.incoming); asm volatile("" : : "r"(obs) : "memory"); });
        const auto* a = book.bestAsk();
        if (book.bestBid() || !a || a->head->quantity != nr - nc) die("partial post-state");
    }
    const char* name() const override { return nm.c_str(); }
};

// (e) level-removing sweeps: L ask levels with one qty-1 order each (prices 100..100+L-1).
// Timed: L/k incoming buys, each qty k at the k-th remaining level's price -> each call fully
// fills k orders and removes k levels. k = 1 is "one-to-one with level removal".
struct SweepWorkload : Workload {
    std::vector<OrderInput> resting, incoming;
    std::string nm;
    SweepWorkload(std::size_t levels, std::size_t k) {
        if (levels > detail::kDefaultMaxPriceLevels || levels % k) die("sweep params");
        for (std::size_t i = 0; i < levels; ++i) resting.push_back({Side::Sell, static_cast<uint32_t>(100 + i), 1, i + 1, 1});
        for (std::size_t j = 0; j < levels / k; ++j)
            incoming.push_back({Side::Buy, static_cast<uint32_t>(100 + (j + 1) * k - 1), static_cast<uint32_t>(k), levels + j + 1, 2});
        nm = "sweep" + std::to_string(k) + "/" + std::to_string(levels);
        std::vector<Trade> trades;
        trades.reserve(levels);
        OrderBook vb(levels * 2 + 100, [&trades](const Trade& t) { trades.push_back(t); });
        for (auto& r : resting) vb.addLimitOrder(r.side, r.price, r.quantity, r.id, r.participantId);
        for (auto& r : incoming) vb.addLimitOrder(r.side, r.price, r.quantity, r.id, r.participantId);
        if (trades.size() != levels || vb.bestAsk() || vb.bestBid()) die("sweep: validation");
        for (std::size_t i = 0; i < levels; ++i)
            if (trades[i].sellOrderId != i + 1 || trades[i].price != 100 + i || trades[i].buyOrderId != levels + i / k + 1) die("sweep: trade content");
    }
    void batch(Accum& acc) override {
        Book book(resting.size() * 2 + 100, noOpCallback);
        addAll(book, resting);
        auto* obs = &book;
        asm volatile("" : : "r"(obs) : "memory");
        timed(acc, incoming.size(), [&] { addAll(book, incoming); asm volatile("" : : "r"(obs) : "memory"); });
        if (book.bestAsk() || book.bestBid()) die("sweep post-state");
    }
    const char* name() const override { return nm.c_str(); }
};

// (d) cancellation of the resting-add book, FIFO (ascending id) or shuffled order.
struct CancelWorkload : Workload {
    std::vector<OrderInput> in;
    std::vector<uint64_t> ids;
    std::string nm;
    CancelWorkload(std::size_t n, bool shuffled, uint32_t spreadLevels = 10) {
        AddWorkload a(n, spreadLevels);   // reuses generator + validation
        in = a.in;
        ids.resize(n);
        std::iota(ids.begin(), ids.end(), 1);
        if (shuffled) std::shuffle(ids.begin(), ids.end(), std::mt19937_64(123));
        nm = std::string(shuffled ? "cancel-shuffled" : "cancel-fifo") +
             (spreadLevels == 10 ? "" : "-levels" + std::to_string(spreadLevels)) + "/" + std::to_string(n);
    }
    void batch(Accum& acc) override {
        Book book(in.size() + 100, noOpCallback);
        addAll(book, in);
        auto* obs = &book;
        asm volatile("" : : "r"(obs) : "memory");
        timed(acc, ids.size(), [&] { for (uint64_t id : ids) book.cancelOrder(id); asm volatile("" : : "r"(obs) : "memory"); });
        if (book.bestBid() || book.bestAsk()) die("cancel post-state");
    }
    const char* name() const override { return nm.c_str(); }
};

// (g1) isolated std::unordered_map replay: same key type, reserve, load factor, and the
// insert sequence of add/N (ids 1..N).
struct UmapInsertWorkload : Workload {
    std::size_t n; std::string nm; std::vector<Order> dummy;
    explicit UmapInsertWorkload(std::size_t n_) : n(n_), dummy(1) { nm = "iso-umap-insert/" + std::to_string(n); }
    void batch(Accum& acc) override {
        std::unordered_map<uint64_t, Order*> m;
        m.max_load_factor(0.7f);
        m.reserve(n + 100);
        Order* p = dummy.data();
        timed(acc, n, [&] { for (uint64_t i = 1; i <= n; ++i) m.try_emplace(i, p); asm volatile("" : : "r"(&m) : "memory"); });
        if (m.size() != n) die("umap size");
    }
    const char* name() const override { return nm.c_str(); }
};
// (g2) isolated erase replay matching match/N: erase ids 1..N/2 from a map holding 1..N.
struct UmapEraseWorkload : Workload {
    std::size_t n; std::string nm; std::vector<Order> dummy;
    explicit UmapEraseWorkload(std::size_t n_) : n(n_), dummy(1) { nm = "iso-umap-erase/" + std::to_string(n); }
    void batch(Accum& acc) override {
        std::unordered_map<uint64_t, Order*> m;
        m.max_load_factor(0.7f);
        m.reserve(n + n / 2 + 100);
        for (uint64_t i = 1; i <= n; ++i) m.try_emplace(i, dummy.data());
        timed(acc, n / 2, [&] { for (uint64_t i = 1; i <= n / 2; ++i) m.erase(i); asm volatile("" : : "r"(&m) : "memory"); });
        if (m.size() != n - n / 2) die("umap erase size");
    }
    const char* name() const override { return nm.c_str(); }
};
// (g3) bare 32-byte operator new / delete replay (same counts as add/N and match/N index traffic).
struct MallocWorkload : Workload {
    std::size_t n; bool del; std::string nm; std::vector<void*> ptrs;
    MallocWorkload(std::size_t n_, bool d) : n(n_), del(d), ptrs(n_) { nm = std::string(d ? "iso-delete32/" : "iso-new32/") + std::to_string(n); }
    void batch(Accum& acc) override {
        if (!del) {
            timed(acc, n, [&] { for (std::size_t i = 0; i < n; ++i) ptrs[i] = ::operator new(32); asm volatile("" : : "r"(ptrs.data()) : "memory"); });
            for (void* p : ptrs) ::operator delete(p);
        } else {
            for (std::size_t i = 0; i < n; ++i) ptrs[i] = ::operator new(32);
            timed(acc, n / 2, [&] { for (std::size_t i = 0; i < n / 2; ++i) ::operator delete(ptrs[i]); asm volatile("" : : "r"(ptrs.data()) : "memory"); });
            for (std::size_t i = n / 2; i < n; ++i) ::operator delete(ptrs[i]);
        }
    }
    const char* name() const override { return nm.c_str(); }
};

// Controls for counter validation.
struct EmptyWorkload : Workload {   // timed region with no work: measures bracket overhead
    void batch(Accum& acc) override { timed(acc, 1, [] { asm volatile("" ::: "memory"); }); }
    const char* name() const override { return "control-empty"; }
};
struct ChainWorkload : Workload {   // known instruction count: K iterations of a 4-instruction loop body
    uint64_t k; std::string nm;
    explicit ChainWorkload(uint64_t k_) : k(k_) { nm = "control-chain/" + std::to_string(k); }
    void batch(Accum& acc) override {
        timed(acc, k, [&] {
            uint64_t x = 1, i = k;
            // body: add, add, subs, b.ne = 4 instructions/iteration; dependent add chain.
            asm volatile("1:\n add %0, %0, #1\n add %0, %0, #1\n subs %1, %1, #1\n b.ne 1b\n" : "+r"(x), "+r"(i) : : "cc");
            asm volatile("" : : "r"(x));
        });
    }
    const char* name() const override { return nm.c_str(); }
};

// ───────────────────────────── driver ─────────────────────────────
static Workload* make(const std::string& w, std::size_t n) {
    if (w == "add") return new AddWorkload(n);
    if (w == "match") return new MatchWorkload(n);
    if (w == "partial") return new PartialWorkload(n);
    if (w.rfind("levels", 0) == 0) return new AddWorkload(n, static_cast<uint32_t>(std::stoul(w.substr(6))), true);
    if (w == "cancel-fifo") return new CancelWorkload(n, false);
    if (w == "cancel-shuffled") return new CancelWorkload(n, true);
    if (w.rfind("cancel-shuffled-levels", 0) == 0) return new CancelWorkload(n, true, static_cast<uint32_t>(std::stoul(w.substr(22))));
    if (w.rfind("sweep", 0) == 0) return new SweepWorkload(n, std::stoul(w.substr(5)));
    if (w == "iso-umap-insert") return new UmapInsertWorkload(n);
    if (w == "iso-umap-erase") return new UmapEraseWorkload(n);
    if (w == "iso-new32") return new MallocWorkload(n, false);
    if (w == "iso-delete32") return new MallocWorkload(n, true);
    if (w == "control-empty") return new EmptyWorkload();
    if (w == "control-chain") return new ChainWorkload(n);
    die("unknown workload " + w);
}

int main(int argc, char** argv) {
    if (argc < 3) die("usage: harness <workload> <n> [seconds_per_rep=2] [reps=5] [pcsample_out]");
    const std::string w = argv[1];
    const std::size_t n = std::stoul(argv[2]);
    const double secs = argc > 3 ? std::atof(argv[3]) : 2.0;
    const int reps = argc > 4 ? std::atoi(argv[4]) : 5;
    Workload* wl = make(w, n);

    { Accum warm; const uint64_t end = nowNs() + 1'000'000'000ull; while (nowNs() < end) wl->batch(warm); }

#ifdef PC_SAMPLE
    pcsamp::start(250);
#endif
#ifdef COUNT_ALLOCS
    alloc_count::reset();
#endif
    std::vector<double> nsPerOp;
    for (int r = 0; r < reps; ++r) {
        Accum acc;
        uint64_t batches = 0;
        const uint64_t wallEnd = nowNs() + static_cast<uint64_t>(secs * 1e9);
        while (nowNs() < wallEnd) { wl->batch(acc); ++batches; }
        const double nso = static_cast<double>(acc.ns) / static_cast<double>(acc.ops);
        nsPerOp.push_back(nso);
#ifdef COUNT_ALLOCS
        std::printf("%s rep=%d batches=%" PRIu64 " ops=%" PRIu64 " (timing not reported in allocation-counting build)",
                    wl->name(), r, batches, acc.ops);
#else
        std::printf("%s rep=%d batches=%" PRIu64 " ops=%" PRIu64 " timed_ns=%" PRIu64 " ns_per_op=%.4f Mops=%.3f",
                    wl->name(), r, batches, acc.ops, acc.ns, nso, 1e3 / nso);
#endif
#ifdef USE_RUSAGE
        std::printf(" instr_per_op=%.3f cycles_per_op=%.3f ipc=%.3f cycles_per_ns=%.3f p_instr_frac=%.4f p_cycles_frac=%.4f",
                    double(acc.c.instr) / acc.ops, double(acc.c.cycles) / acc.ops,
                    double(acc.c.instr) / double(acc.c.cycles), double(acc.c.cycles) / double(acc.ns),
                    double(acc.c.pinstr) / double(acc.c.instr), double(acc.c.pcycles) / double(acc.c.cycles));
#endif
        std::printf("\n");
#ifdef COUNT_ALLOCS
        std::printf("%s rep=%d alloc_per_op=%.6f free_per_op=%.6f bytes_per_op=%.3f max_size=%" PRIu64 " sizes:",
                    wl->name(), r, double(alloc_count::news) / acc.ops, double(alloc_count::deletes) / acc.ops,
                    double(alloc_count::bytes) / acc.ops, alloc_count::maxSize);
        for (int b = 0; b < alloc_count::kBins; ++b)
            if (alloc_count::sizeBins[b]) std::printf(" <=%d:%" PRIu64, b == alloc_count::kBins - 1 ? 99999 : b * 16, alloc_count::sizeBins[b]);
        std::printf("\n");
        alloc_count::reset();
#endif
    }
    std::sort(nsPerOp.begin(), nsPerOp.end());
    [[maybe_unused]] const double med = nsPerOp[nsPerOp.size() / 2];
#ifndef COUNT_ALLOCS
    std::printf("%s SUMMARY median_ns_per_op=%.4f median_Mops=%.3f min=%.4f max=%.4f\n", wl->name(), med, 1e3 / med, nsPerOp.front(), nsPerOp.back());
#endif
#ifdef PC_SAMPLE
    pcsamp::stopAndReport(argc > 5 ? argv[5] : "pcsamples.txt");
#endif
    delete wl;
    return 0;
}
