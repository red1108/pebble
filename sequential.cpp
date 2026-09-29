#include "pebbling_common.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
using namespace std;
using pebbling::MAX_H;
using pebbling::Metrics;
using pebbling::nodes;
using pebbling::require;

// Every vertex, including a leaf and the retained output, costs one pebble.
// Only pebble and unpebble count toward work. No parallel rounds here.
Metrics metrics;

int h, n, space_limit;
bool placed[1 << MAX_H];
bool is_ghost[1 << MAX_H];

// An independent ledger consumes each primitive P/U/G event, not the planner's
// cost formula or the simulator's bool arrays. Checks remain active with NDEBUG.
struct Verifier {
    vector<unsigned char> state;  // 0: empty, 1: pebble, 2: ghost
    int vertices = 0, cap = 0;
    Metrics count;

    void reset(int size, int budget) {
        vertices = size;
        cap = budget;
        state.assign(size + 1, 0);
        count = {};
    }

    void step(char op, int x) {
        require(1 <= x && x <= vertices, "invalid vertex");
        bool ready = x > vertices / 2 ||
                     (state[2*x] == 1 && state[2*x+1] == 1);
        if (op == 'P') {
            require(ready && state[x] != 1, "illegal pebble");
            state[x] = 1;
            count.place++;
            count.live++;
            count.peak = max(count.peak, count.live);
        } else if (op == 'U') {
            require(ready && state[x] == 1, "illegal unpebble");
            state[x] = 0;
            count.unpebble++;
            count.live--;
        } else {
            require(op == 'G' && state[x] == 1, "illegal ghost");
            state[x] = 2;
            count.ghost++;
            count.live--;
        }
        require(count.live <= cap, "space limit exceeded");
    }

    void finish() const {
        require(state[1] == 1 && count.live == 1, "output missing");
        for (int x = 2; x <= vertices; x++)
            require(state[x] == 0, "unfinished cleanup");
        require(count.place == metrics.place &&
                count.unpebble == metrics.unpebble &&
                count.ghost == metrics.ghost &&
                count.live == metrics.live && count.peak == metrics.peak,
                "simulator/verifier metrics disagree");
    }
} verifier;

bool leaf(int x) {
    return pebbling::leaf(x, n);
}

bool child_ready(int x) {
    return leaf(x) || (placed[2*x] && placed[2*x+1]);
}

void pebble(int x) {
    assert(child_ready(x) && !placed[x]);
    verifier.step('P', x);
    placed[x] = true;
    is_ghost[x] = false;
    metrics.place++;
    metrics.live++;
    metrics.peak = max(metrics.peak, metrics.live);
}

void unpebble(int x) {
    assert(child_ready(x) && (placed[x] || is_ghost[x]));
    if (is_ghost[x]) {
        pebble(x);  // Recompute, including its work and temporary pebble.
        metrics.exorcise++;
    }
    verifier.step('U', x);
    placed[x] = false;
    metrics.unpebble++;
    metrics.live--;
}

void ghost(int x) {
    assert(placed[x]);
    verifier.step('G', x);
    placed[x] = false;
    is_ghost[x] = true;
    metrics.ghost++;
    metrics.live--;
}

// Unpebble when its inputs are still live (including a leaf); otherwise ghost.
void release(int x) {
    assert(placed[x]);
    if (child_ready(x)) unpebble(x);
    else ghost(x);
}

// The user's KSS implementation, unchanged in its scheduling decisions.
void fast_pebble(int x) {
    auto place = [](int v) { pebble(v); };
    auto remove = [](int v) { release(v); };
    pebbling::fast_pebble_traversal(x, n, place, remove);
}

void cleanup_kss(int x) {
    assert(!placed[x]);
    if (leaf(x)) {
        if (is_ghost[x]) unpebble(x);
        return;
    }
    assert(is_ghost[x]);
    fast_pebble(2*x);
    fast_pebble(2*x+1);
    unpebble(x);
    release(2*x);
    release(2*x+1);
    cleanup_kss(2*x);
    cleanup_kss(2*x+1);
}

void kss_algorithm() {
    fast_pebble(1);
    if (n > 1) {
        cleanup_kss(2);
        cleanup_kss(3);
    }
}

// Make the depth-k boundary roots, retain them, compute the top block, and
// remove the top block while its inputs still exist. Only the global root
// may be retained. On return, ghosts can remain only in boundary subtrees.
void block(int x, int k, bool keep_root) {
    assert(k >= 1);
    for (int v = x << k; v < (x+1) << k; v++) fast_pebble(v);

    for (int d = k-1; d >= 0; d--)
        for (int v = x << d; v < (x+1) << d; v++) pebble(v);

    for (int d = keep_root ? 1 : 0; d < k; d++)
        for (int v = x << d; v < (x+1) << d; v++) unpebble(v);

    for (int v = x << k; v < (x+1) << k; v++) release(v);
}

void fill_subtree(int x) {
    if (!leaf(x)) {
        fill_subtree(2*x);
        fill_subtree(2*x+1);
    }
    pebble(x);
}

void drain_subtree(int x) {
    unpebble(x);
    if (!leaf(x)) {
        drain_subtree(2*x);
        drain_subtree(2*x+1);
    }
}

// Equal-depth sequential construction from the archived manuscript:
// initial fast sweep, largest feasible cut, then full-tree terminal cleanup.
void level_control() {
    fast_pebble(1);
    if (h == 1) return;
    int r = h-1, B = space_limit-1;  // The global root stays live.
    vector<int> jobs = {2, 3};
    while (nodes(r) > B) {
        int k = 0;
        for (int candidate = 1; candidate < r; candidate++) {
            int m = 1 << candidate;
            if (m+r-candidate <= B && 2*m-1 <= B) k = candidate;
        }
        require(k > 0, "no feasible paper cut");
        vector<int> next;
        for (int x : jobs) {
            block(x, k, false);
            for (int v = x << k; v < (x+1) << k; v++) next.push_back(v);
        }
        jobs = std::move(next);
        r -= k;
    }
    for (int x : jobs) {
        fill_subtree(x);
        drain_subtree(x);
    }
}

// Finite optimization ONLY within the above flat-boundary block family.
// Inputs to cleanup have ghosts at every internal vertex and empty leaves.
// This invariant is established by fast/block; this is not a general DAG DP.
struct Plan {
    uint64_t work = numeric_limits<uint64_t>::max() / 4;
    int peak = numeric_limits<int>::max(), cut = -1;
};
vector<Plan> clean_plan;
Plan root_plan;

int fast_peak(int r) { return r == 1 ? 1 : r+1; }
uint64_t fast_work(int r) {
    return uint64_t(nodes(r)) + (r == 1 ? 0 : (1 << (r-1)));
}

int block_peak(int r, int k) {
    int m = 1 << k;
    return max(m-1 + fast_peak(r-k), 2*m-1);
}

uint64_t block_work(int r, int k, bool keep_root) {
    int m = 1 << k, q = r-k;
    // Boundary preparation + top P/U + supported removals of boundary leaves.
    return m*fast_work(q) + 2*uint64_t(m-1) - (keep_root ? 1 : 0)
           + (q == 1 ? m : 0);
}

void consider(Plan& best, Plan candidate) {
    if (tie(candidate.work, candidate.peak, candidate.cut) <
        tie(best.work, best.peak, best.cut)) best = candidate;
}

void make_uniform_plan() {
    clean_plan.assign(h+1, {});
    clean_plan[1] = {0, 0, 0};  // Leaves are already empty, not ghosted.
    int B = space_limit-1;
    for (int r = 2; r < h; r++) {
        for (int k = 1; k < r; k++) {
            int q = r-k, m = 1 << k;
            if (block_peak(r, k) > B || clean_plan[q].cut < 0) continue;
            consider(clean_plan[r], {
                block_work(r, k, false) + m*clean_plan[q].work,
                max(block_peak(r, k), clean_plan[q].peak), k});
        }
    }
    root_plan = {};
    if (h == 1) {
        root_plan = {1, 1, 0};
        return;
    }
    for (int k = 1; k < h; k++) {
        int q = h-k, m = 1 << k;
        if (block_peak(h, k) > space_limit || clean_plan[q].cut < 0) continue;
        consider(root_plan, {
            block_work(h, k, true) + m*clean_plan[q].work,
            max(block_peak(h, k), 1+clean_plan[q].peak), k});
    }
    require(root_plan.cut >= 0, "no feasible root plan");
}

void cleanup_uniform(int x, int r) {
    assert(!placed[x]);
    if (r == 1) {
        assert(leaf(x) && !is_ghost[x]);
        return;
    }
    assert(is_ghost[x]);
    int k = clean_plan[r].cut;
    block(x, k, false);
    for (int v = x << k; v < (x+1) << k; v++) cleanup_uniform(v, r-k);
}

void uniform_control() {
    if (h == 1) {
        pebble(1);
        return;
    }
    int k = root_plan.cut;
    block(1, k, true);  // Clear the top block already during first evaluation.
    for (int v = 1 << k; v < 2 << k; v++) cleanup_uniform(v, h-k);
}

// A boundary need not have 2^k roots. For m roots its two depths are k and
// k+1, with a=2^(k+1)-m taller jobs and b=2*(m-2^k) shorter jobs.
// Produce taller jobs first; this minimizes the peak while retaining roots.
struct Shape {
    int q, a, b;
};

Shape shape(int r, int m) {
    int k = 0;
    while ((1 << (k+1)) <= m) k++;
    return {r-k, (1 << (k+1))-m, 2*(m-(1 << k))};
}

int mixed_peak(int r, int m) {
    int ceil_log = 0;
    while ((1 << ceil_log) < m) ceil_log++;
    return max(2*m-1, r+m-ceil_log);
}

// Peak is nondecreasing in m. No work optimization/DP is needed here.
int choose_boundary_size(int r, int B) {
    if (r == 1) return 1;
    int lo = 2, hi = min(1 << (r-1), (B+1)/2), answer = 0;
    while (lo <= hi) {
        int mid = lo+(hi-lo)/2;
        if (mixed_peak(r, mid) <= B) {
            answer = mid;
            lo = mid+1;
        } else hi = mid-1;
    }
    require(answer >= 2, "no feasible mixed boundary");
    return answer;
}

// In these plans 'cut' records the boundary size m, not a depth k.
vector<Plan> mixed_clean_plan;
Plan mixed_root_plan;

// Cost prediction for validation only. It does not choose the schedule.
Plan mixed_cost(int r, int m, bool keep, const vector<Plan>& clean) {
    auto [q, a, b] = shape(r, m);
    uint64_t work = 2*uint64_t(m-1) - (keep ? 1 : 0);
    int peak = mixed_peak(r, m);
    for (auto [count, height] : {pair<int, int>{a, q}, {b, q-1}}) {
        if (!count) continue;
        work += count*(fast_work(height) + (height == 1 ? 1 : 0)
                       + clean[height].work);
        peak = max(peak, int(keep)+clean[height].peak);
    }
    return {work, peak, m};
}

void make_mixed_plan() {
    mixed_clean_plan.assign(h+1, {});
    mixed_clean_plan[1] = {0, 0, 1};
    for (int r = 2; r < h; r++)
        mixed_clean_plan[r] = mixed_cost(r, choose_boundary_size(r, space_limit-1),
                                        false, mixed_clean_plan);
    mixed_root_plan = h == 1 ? Plan{1, 1, 1} :
        mixed_cost(h, choose_boundary_size(h, space_limit), true, mixed_clean_plan);
}

struct BoundaryJob { int x, height; };

void collect_boundary(int x, int r, int m, vector<BoundaryJob>& boundary,
                      vector<int>& top) {
    if (m == 1) {
        boundary.push_back({x, r});
        return;
    }
    collect_boundary(2*x, r-1, (m+1)/2, boundary, top);
    collect_boundary(2*x+1, r-1, m/2, boundary, top);
    top.push_back(x);  // Children before parent.
}

// The same routine computes the output root and cleans ghost-only subtrees.
// Its schedule is: widest feasible boundary, top block, smaller cleanup jobs.
void our_algorithm(int x, int r, bool keep_root) {
    assert(!placed[x]);
    if (r == 1) {
        if (keep_root) pebble(x);
        else assert(!placed[x] && !is_ghost[x]);
        return;
    }
    assert(keep_root || is_ghost[x]);
    int B = keep_root ? space_limit : space_limit-1;
    int m = choose_boundary_size(r, B);
    vector<BoundaryJob> boundary;
    vector<int> top;
    collect_boundary(x, r, m, boundary, top);
    // Boundary jobs have at most two heights; keep left-to-right order within each.
    int tall_height = shape(r, m).q;
    stable_partition(boundary.begin(), boundary.end(),
                     [tall_height](const BoundaryJob& j) { return j.height == tall_height; });
    for (BoundaryJob j : boundary) fast_pebble(j.x);
    for (int v : top) pebble(v);
    for (auto it = top.rbegin(); it != top.rend(); ++it)
        if (!keep_root || *it != x) unpebble(*it);
    for (BoundaryJob j : boundary) release(j.x);
    for (BoundaryJob j : boundary) our_algorithm(j.x, j.height, false);
}

enum Algorithm { KSS, CONTROL_LEVEL, CONTROL_UNIFORM, BLOCK };

Metrics run(Algorithm algorithm, int height, int cap) {
    require(1 <= height && height <= MAX_H, "height must be in [1, 20]");
    h = height;
    n = nodes(h);
    int minimum = h == 1 ? 1 : h+1;
    require(minimum <= cap && cap <= n, "space must be in [minimum, n]");
    space_limit = cap;
    fill_n(placed, n+1, false);
    fill_n(is_ghost, n+1, false);
    metrics = {};
    verifier.reset(n, cap);

    if (algorithm == KSS) kss_algorithm();
    else if (algorithm == CONTROL_LEVEL) level_control();
    else if (algorithm == CONTROL_UNIFORM) {
        make_uniform_plan();
        uniform_control();
        require(metrics.work() == root_plan.work && metrics.peak == root_plan.peak,
                "planned/replayed work or peak disagree");
    } else {
        make_mixed_plan();  // Classical bookkeeping is not quantum work.
        our_algorithm(1, h, true);
        require(metrics.work() == mixed_root_plan.work &&
                metrics.peak == mixed_root_plan.peak,
                "mixed boundary prediction/replay disagree");
    }
    verifier.finish();
    require(placed[1] && !is_ghost[1], "wrong root state");
    for (int x = 2; x <= n; x++)
        require(!placed[x] && !is_ghost[x], "nonempty final vertex");
    require(metrics.place == metrics.unpebble + metrics.ghost + 1,
            "pebble conservation failed");
    return metrics;
}

void print_row(const string& name, const Metrics& m, uint64_t baseline) {
    cout << left << setw(15) << name << right
         << setw(12) << m.place << setw(12) << m.unpebble
         << setw(12) << m.work() << setw(12) << m.ghost
         << setw(8) << m.peak << setw(11) << fixed << setprecision(2)
         << 100.0*(1.0-double(m.work())/double(baseline)) << "%\n";
}

void compare(int height, int cap, bool show_controls = false) {
    Metrics kss = run(KSS, height, cap);
    Metrics paper, uniform;
    if (show_controls) {
        paper = run(CONTROL_LEVEL, height, cap);
        uniform = run(CONTROL_UNIFORM, height, cap);
    }
    Metrics ours = run(BLOCK, height, cap);
    cout << "h=" << height << ", n=" << nodes(height) << ", space_limit=" << cap
         << "\nwork = pebble + unpebble; ghost excluded; output root retained\n";
    cout << left << setw(15) << "algorithm" << right
         << setw(12) << "pebble" << setw(12) << "unpebble"
         << setw(12) << "work" << setw(12) << "ghost"
         << setw(8) << "peak" << setw(12) << "reduction" << '\n';
    print_row("KSS", kss, kss.work());
    if (show_controls) {
        print_row("level_control", paper, kss.work());
        print_row("uniform_control", uniform, kss.work());
    }
    print_row("Block cleanup", ours, kss.work());
    cout << "Verified: every primitive, space limit, counters, final cleanup.\n";
    if (height > 1)
        cout << "Block cleanup: initial boundary size=" << mixed_root_plan.cut
             << "; KSS is the fixed h+1-space schedule, not a budget-tuned baseline.\n";
}

// Exhaustive optimization of the restricted mixed-boundary family, solely
// to check the greedy rule. This is not an exact search over all pebblings.
uint64_t check_greedy_choices(int height, int cap) {
    vector<Plan> optimal(height+1);
    optimal[1] = {0, 0, 1};
    for (int r = 2; r <= height; r++) {
        bool keep = r == height;
        int B = keep ? cap : cap-1;
        for (int m = 2; m <= min(1 << (r-1), (B+1)/2); m++) {
            if (mixed_peak(r, m) <= B)
                consider(optimal[r], mixed_cost(r, m, keep, optimal));
        }
        Plan greedy = mixed_cost(r, choose_boundary_size(r, B), keep, optimal);
        require(greedy.work == optimal[r].work, "greedy boundary is not family-optimal");
        if (!keep)
            require(optimal[r].work >= 2*optimal[r-1].work+2,
                    "cleanup projection inequality failed");
    }
    return optimal[height].work;
}

void self_test() {
    uint64_t cases = 0, events = 0;
    for (int height = 1; height <= 16; height++) {
        int size = nodes(height), minimum = height == 1 ? 1 : height+1;
        set<int> budgets;
        if (height <= 9) {
            for (int cap = minimum; cap <= size; cap++) budgets.insert(cap);
        } else {
            for (int cap : {minimum, minimum+1, 2*height, 4*height,
                            1 << (height/2), size-1, size})
                budgets.insert(min(size, max(minimum, cap)));
        }
        uint64_t previous = numeric_limits<uint64_t>::max();
        for (int cap : budgets) {
            Metrics kss = run(KSS, height, cap);
            Metrics paper = run(CONTROL_LEVEL, height, cap);
            Metrics uniform = run(CONTROL_UNIFORM, height, cap);
            Metrics ours = run(BLOCK, height, cap);
            uint64_t half = uint64_t(1) << (height-1);
            require(kss.place == (height == 1 ? 1 : (2*height-3)*half+1),
                    "KSS placement formula mismatch");
            require(kss.unpebble == (height == 1 ? 0 : height*half-2),
                    "KSS unpebble formula mismatch");
            require(kss.peak == minimum, "KSS peak formula mismatch");
            require(ours.work() <= kss.work() && ours.work() <= paper.work() &&
                    ours.work() <= uniform.work(),
                    "block family lost its baseline options");
            require(ours.work() <= previous, "more budget increased tuned work");
            if (cap == size)
                require(ours.work() == 2*uint64_t(size)-1,
                        "full-space compute/uncompute endpoint mismatch");
            previous = ours.work();
            if (height > 1)
                require(ours.work() == check_greedy_choices(height, cap),
                        "greedy execution/optimized family disagree");
            events += kss.work()+kss.ghost + paper.work()+paper.ghost
                      + uniform.work()+uniform.ghost + ours.work()+ours.ghost;
            cases++;
        }
    }
    require(run(KSS, 10, 11).work() == 13823, "user example mismatch");
    require(run(CONTROL_UNIFORM, 10, 11).work() == 9383, "old h10/s11 regression");
    require(run(BLOCK, 10, 11).work() == 7654, "new h10/s11 regression");
    uint64_t choice_cases = 0;
    for (int height = 2; height <= 12; height++)
        for (int cap = height+1; cap <= nodes(height); cap++) {
            check_greedy_choices(height, cap);
            choice_cases++;
        }
    cout << "PASS: " << cases << " (height, budget) cases, " << 4*cases
         << " complete schedules, " << events << " independently checked events.\n"
         << "Heights 1..9: every feasible budget; heights 10..16: sampled budgets.\n"
         << "Also passed: KSS closed forms, planner/replay equality, budget monotonicity,\n"
         << "full-space endpoint, and h10/s11 regression.\n"
         << "Greedy matched exhaustive cut-family DP in " << choice_cases
         << " further parameter cases (all budgets, heights 2..12).\n";
}

int parse_int(const char* text) {
    size_t used = 0;
    int value = stoi(text, &used);
    require(text[used] == '\0', "expected an integer");
    return value;
}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && string(argv[1]) == "--test") self_test();
        else {
            bool controls = argc > 1 && string(argv[1]) == "--controls";
            int first = controls ? 2 : 1;
            require(argc <= first+2,
                    "usage: ./sequential [--controls] [height [space]] or ./sequential --test");
            int height = argc > first ? parse_int(argv[first]) : 10;
            require(1 <= height && height <= MAX_H, "height must be in [1, 20]");
            int cap = argc > first+1 ? parse_int(argv[first+1]) : (height == 1 ? 1 : height+1);
            compare(height, cap, controls);
        }
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
