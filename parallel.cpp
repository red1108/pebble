#include "pebbling_common.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>
using namespace std;
using pebbling::MAX_H;
using pebbling::Metrics;
using pebbling::nodes;
using pebbling::require;

// A round has simultaneous P/U actions, followed by unrestricted ghosting.
struct Round {
    vector<int> place, unpebble, ghost;
};
using Schedule = vector<Round>;

class ParallelSimulator {
    int n, cap, stamp = 0;
    vector<unsigned char> state;  // 0: empty, 1: pebble, 2: ghost
    vector<int> active_stamp;
    Metrics count;
    uint64_t depth = 0, area = 0;

    bool ready(int x) const {
        return pebbling::leaf(x, n) ||
               (state[2*x] == 1 && state[2*x+1] == 1);
    }

    void claim_active_set(int x) {
        auto claim = [this](int v) {
            require(active_stamp[v] != stamp, "overlapping active sets in a round");
            active_stamp[v] = stamp;
        };
        claim(x);
        if (!pebbling::leaf(x, n)) {
            claim(2*x);
            claim(2*x+1);
        }
    }

public:
    ParallelSimulator(int height, int budget)
        : n(nodes(height)), cap(budget), state(n+1), active_stamp(n+1) {}

    void execute(const Round& round) {
        require(!round.place.empty() || !round.unpebble.empty() ||
                !round.ghost.empty(), "empty round");
        ++stamp;
        // Check every first-phase precondition against the same old state.
        for (int x : round.place) {
            require(1 <= x && x <= n, "invalid placement vertex");
            require(state[x] != 1 && ready(x), "illegal parallel placement");
            claim_active_set(x);
        }
        for (int x : round.unpebble) {
            require(1 <= x && x <= n, "invalid unpebble vertex");
            require(state[x] == 1 && ready(x), "illegal parallel unpebble");
            claim_active_set(x);
        }

        // U_t includes old pebbles even when they are uncomputed this round.
        int occupied = count.live + static_cast<int>(round.place.size());
        require(occupied <= cap, "parallel round exceeds space budget");
        count.peak = max(count.peak, occupied);
        area += occupied;
        ++depth;

        for (int x : round.place) {
            if (state[x] == 2) ++count.exorcise;
            state[x] = 1;
            ++count.place;
            ++count.live;
        }
        for (int x : round.unpebble) {
            state[x] = 0;
            ++count.unpebble;
            --count.live;
        }
        for (int x : round.ghost) {
            require(1 <= x && x <= n && state[x] == 1,
                    "illegal parallel ghost");
            state[x] = 2;
            ++count.ghost;
            --count.live;
        }
    }

    void execute(const Schedule& schedule) {
        for (const Round& round : schedule) execute(round);
    }

    void check_sweep(int root, int height) const {
        require(state[root] == 1, "Sweep root missing");
        for (int level = 1; level < height; ++level)
            for (int v = root << level; v < (root+1) << level; ++v)
                require(state[v] == 2, "Sweep left a non-root value unghosted");
    }

    void finish() const {
        require(state[1] == 1 && count.live == 1, "output root missing");
        for (int x = 2; x <= n; ++x)
            require(state[x] == 0, "pebble or ghost left behind");
        require(count.place == count.unpebble + count.ghost + 1,
                "pebble conservation failed");
    }

    const Metrics& metrics() const { return count; }
    uint64_t rounds() const { return depth; }
    uint64_t storage_area() const { return area; }
};

void append_level(vector<int>& out, int root, int depth) {
    for (int v = root << depth; v < (root+1) << depth; ++v)
        out.push_back(v);
}

void append_schedule(Schedule& out, Schedule&& tail) {
    out.insert(out.end(), make_move_iterator(tail.begin()),
               make_move_iterator(tail.end()));
}

Schedule parallelize(const vector<Schedule>& jobs) {
    size_t length = 0;
    for (const Schedule& job : jobs) length = max(length, job.size());
    Schedule result;
    result.reserve(length);
    for (size_t t = 0; t < length; ++t) {
        Round round;
        for (const Schedule& job : jobs) {
            if (t >= job.size()) continue;
            const Round& part = job[t];
            round.place.insert(round.place.end(), part.place.begin(), part.place.end());
            round.unpebble.insert(round.unpebble.end(), part.unpebble.begin(),
                                   part.unpebble.end());
            round.ghost.insert(round.ghost.end(), part.ghost.begin(), part.ghost.end());
        }
        result.push_back(std::move(round));
    }
    return result;
}

// Binary counting combines completed left-to-right subtree roots.
void carry_root(Schedule& out, vector<int>& stack, int incoming) {
    while (!stack.empty() && stack.back()%2 == 0 &&
           stack.back()+1 == incoming) {
        int left = stack.back();
        stack.pop_back();
        Round round;
        round.place.push_back(left/2);
        round.ghost = {left, incoming};
        out.push_back(std::move(round));
        incoming = left/2;
    }
    stack.push_back(incoming);
}

// Sweep leaves only its root pebbled; every other vertex is a ghost.
Schedule ghost_fast(int root, int vertex_count) {
    Schedule out;
    auto place = [&out](int v) {
        Round round;
        round.place.push_back(v);
        out.push_back(std::move(round));
    };
    auto release = [&out](int v) { out.back().ghost.push_back(v); };
    pebbling::fast_traversal(root, vertex_count, place, release);
    return out;
}

Schedule full_sweep(int root, int height) {
    Schedule out;
    for (int depth = height-1; depth >= 0; --depth) {
        Round round;
        append_level(round.place, root, depth);
        if (depth < height-1) append_level(round.ghost, root, depth+1);
        out.push_back(std::move(round));
    }
    return out;
}

Schedule sweep(int root, int height, int budget, int vertex_count);

// Stagger bottom-subtree starts by two advancement rounds. Carry rounds pause
// all unfinished jobs, as in the manuscript's b >= 2q construction.
Schedule wavefront_sweep(int root, int height, int budget) {
    int leaves = 1;
    while (2*leaves <= budget/8) leaves *= 2;
    int ell = 1;
    while ((1 << (ell-1)) < leaves) ++ell;
    int d = height-ell, jobs = 1 << d;
    require(ell >= 2 && d >= 1, "invalid wavefront split");
    Schedule out;
    vector<int> stack;
    for (int t = 0; t <= 2*(jobs-1)+ell-1; ++t) {
        Round round;
        int completed = -1;
        int first = max(0, (t-ell+2)/2);
        int last = min(jobs-1, t/2);
        for (int j = first; j <= last; ++j) {
            int age = t-2*j;
            int subtree = (root << d)+j;
            int level = ell-1-age;
            append_level(round.place, subtree, level);
            if (age > 0) append_level(round.ghost, subtree, level+1);
            if (age == ell-1) completed = j;
        }
        out.push_back(std::move(round));
        if (completed >= 0)
            carry_root(out, stack, (root << d)+completed);
    }
    require(stack.size() == 1 && stack[0] == root,
            "wavefront did not produce its root");
    return out;
}

Schedule near_height_sweep(int root, int height, int budget, int vertex_count) {
    int ell = height/4, d = height-ell, jobs = 1 << d;
    require(ell >= 2, "invalid near-height split");
    Schedule out;
    vector<int> stack;
    for (int j = 0; j < jobs; ++j) {
        int local_budget = budget-static_cast<int>(stack.size());
        require(local_budget >= ell+1, "near-height subtree lacks space");
        int subtree = (root << d)+j;
        if (local_budget >= 2*ell)
            append_schedule(out, sweep(subtree, ell, local_budget, vertex_count));
        else
            append_schedule(out, ghost_fast(subtree, vertex_count));
        carry_root(out, stack, subtree);
    }
    require(stack.size() == 1 && stack[0] == root,
            "near-height sweep did not produce its root");
    return out;
}

Schedule sweep(int root, int height, int budget, int vertex_count) {
    require(height >= 1 && budget >= 1, "invalid Sweep parameters");
    if (height == 1) {
        Round round;
        round.place.push_back(root);
        return {std::move(round)};
    }
    require(budget >= height+1, "Sweep lacks minimum space");
    if (budget >= nodes(height)) return full_sweep(root, height);
    if (budget < 16) return ghost_fast(root, vertex_count);
    if (budget >= 2*height) return wavefront_sweep(root, height, budget);
    return near_height_sweep(root, height, budget, vertex_count);
}

int resolver_space(int height) {
    int power = 1 << height;
    return min(power-1, (16*power+height-1)/height);
}

int ceil_log2(int value) {
    int power = 1, result = 0;
    while (power < value) {
        power *= 2;
        ++result;
    }
    return result;
}

int largest_equal_cut(int height, int budget) {
    int best = 0;
    for (int k = 1; k < height; ++k) {
        int m = 1 << k, q = height-k;
        if (m+q <= budget && 2*m-1 <= budget) best = k;
    }
    require(best >= 1, "no feasible equal-depth frontier");
    return best;
}

// Prepare an equal-depth frontier, clear its top block levelwise, then release
// its roots. The caller subsequently handles the resulting ghost subtrees.
Schedule block_contract(int root, int height, int cut_depth,
                        int budget, int vertex_count) {
    int m = 1 << cut_depth, q = height-cut_depth;
    require(q >= 1 && m+q <= budget && 2*m-1 <= budget,
            "infeasible parallel block cut");
    int batch_size, local_budget;
    if (m*(q+1) <= budget) {
        batch_size = m;
        local_budget = budget/m;
    } else {
        batch_size = (budget-m+1)/(q+1);
        local_budget = q+1;
    }
    require(batch_size >= 1, "frontier batch has no capacity");

    Schedule out;
    for (int first = 0; first < m; first += batch_size) {
        vector<Schedule> jobs;
        for (int j = first; j < min(m, first+batch_size); ++j)
            jobs.push_back(sweep((root << cut_depth)+j, q,
                                 local_budget, vertex_count));
        append_schedule(out, parallelize(jobs));
    }

    for (int depth = cut_depth-1; depth >= 0; --depth) {
        Round round;
        append_level(round.place, root, depth);
        out.push_back(std::move(round));
    }
    for (int depth = 0; depth < cut_depth; ++depth) {
        Round round;
        append_level(round.unpebble, root, depth);
        out.push_back(std::move(round));
    }
    Round release;
    for (int v = root << cut_depth; v < (root+1) << cut_depth; ++v) {
        if (q == 1) release.unpebble.push_back(v);
        else release.ghost.push_back(v);
    }
    out.push_back(std::move(release));
    return out;
}

Schedule clean_small(int root, int height) {
    Schedule out;
    for (int depth = height-1; depth >= 0; --depth) {
        Round round;
        append_level(round.place, root, depth);
        out.push_back(std::move(round));
    }
    for (int depth = 0; depth < height; ++depth) {
        Round round;
        append_level(round.unpebble, root, depth);
        out.push_back(std::move(round));
    }
    return out;
}

struct Result {
    Metrics metrics;
    uint64_t depth = 0, area = 0;
};

Result run_parallel(int height, int space_limit) {
    require(1 <= height && height <= MAX_H, "height must be in [1, 20]");
    int n = nodes(height);
    int minimum = height == 1 ? 1 : height+1;
    require(minimum <= space_limit && space_limit <= n,
            "space must be in [minimum, n]");
    ParallelSimulator sim(height, space_limit);
    sim.execute(sweep(1, height, space_limit, n));
    sim.check_sweep(1, height);
    require(sim.metrics().place == uint64_t(n), "Sweep did not pebble each vertex once");

    if (height > 1) {
        int budget = space_limit-1;
        int remaining_height = height-1;
        vector<int> forest{2, 3};

        // Large jobs use one subtree at a time while p(r) exceeds the budget.
        while (resolver_space(remaining_height) > budget) {
            int k = largest_equal_cut(remaining_height, budget);
            vector<int> next;
            next.reserve(forest.size() * (1 << k));
            for (int root : forest) {
                sim.execute(block_contract(root, remaining_height, k, budget, n));
                for (int j = 0; j < (1 << k); ++j)
                    next.push_back((root << k)+j);
            }
            forest = std::move(next);
            remaining_height -= k;
        }

        // Resolve whole batches of independent subtrees at equal height.
        while (remaining_height > 16) {
            int per_tree = resolver_space(remaining_height);
            int batch_size = budget/per_tree;
            int q = ceil_log2(remaining_height)+2;
            int k = remaining_height-q;
            int m = 1 << k;
            require(batch_size >= 1 && k >= 1, "invalid forest reduction");
            vector<int> next;
            next.reserve(forest.size() * m);
            for (size_t first = 0; first < forest.size(); first += batch_size) {
                vector<Schedule> jobs;
                for (size_t i = first;
                     i < min(forest.size(), first+static_cast<size_t>(batch_size)); ++i) {
                    int root = forest[i];
                    jobs.push_back(block_contract(root, remaining_height, k,
                                                  per_tree, n));
                    for (int j = 0; j < m; ++j)
                        next.push_back((root << k)+j);
                }
                sim.execute(parallelize(jobs));
            }
            forest = std::move(next);
            remaining_height = q;
        }

        int per_tree = nodes(remaining_height);
        int batch_size = budget/per_tree;
        require(batch_size >= 1, "small forest lacks space");
        for (size_t first = 0; first < forest.size(); first += batch_size) {
            vector<Schedule> jobs;
            for (size_t i = first;
                 i < min(forest.size(), first+static_cast<size_t>(batch_size)); ++i)
                jobs.push_back(clean_small(forest[i], remaining_height));
            sim.execute(parallelize(jobs));
        }
    }

    sim.finish();
    return {sim.metrics(), sim.rounds(), sim.storage_area()};
}

int parse_int(const char* text) {
    size_t used = 0;
    int value = stoi(text, &used);
    require(text[used] == '\0', "expected an integer");
    return value;
}

void self_test() {
    {
        ParallelSimulator active_check(2, 3);
        Round leaves;
        leaves.place = {2, 3};
        active_check.execute(leaves);
        Round conflict;
        conflict.place = {1};
        conflict.unpebble = {2};
        bool rejected = false;
        try { active_check.execute(conflict); }
        catch (const runtime_error&) { rejected = true; }
        require(rejected, "overlapping active sets were accepted");

        ParallelSimulator space_check(2, 2);
        space_check.execute(leaves);
        Round parent;
        parent.place = {1};
        rejected = false;
        try { space_check.execute(parent); }
        catch (const runtime_error&) { rejected = true; }
        require(rejected, "transient round space was not charged");
    }
    int cases = 0;
    for (int height = 1; height <= 8; ++height) {
        int n = nodes(height);
        for (int space = height == 1 ? 1 : height+1; space <= n; ++space) {
            Result result = run_parallel(height, space);
            require(result.metrics.peak <= space && result.depth > 0,
                    "parallel resource check failed");
            if (space == n) {
                require(result.depth == uint64_t(3*height-2),
                        "full-space depth mismatch");
                require(result.metrics.work() == uint64_t(3*n-2),
                        "full-space work mismatch");
            }
            ++cases;
        }
    }
    for (auto [height, space] : {pair<int,int>{10,11}, {10,20},
                                 {16,17}, {18,nodes(18)}}) {
        Result result = run_parallel(height, space);
        require(result.metrics.peak <= space, "large-case space mismatch");
        ++cases;
    }
    cout << "PASS: " << cases << " parallel height/space cases; "
         << "round legality, active sets, transient space, and cleanup checked.\n";
}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && string(argv[1]) == "--test") {
            self_test();
            return 0;
        }
        require(argc <= 3, "usage: ./parallel [height [space]] or ./parallel --test");
        int height = argc > 1 ? parse_int(argv[1]) : 10;
        int space = argc > 2 ? parse_int(argv[2]) :
                    (height == 1 ? 1 : height+1);
        Result result = run_parallel(height, space);
        cout << "h=" << height << ", n=" << nodes(height)
             << ", space_limit=" << space << '\n';
        cout << "depth=" << result.depth << ", work=" << result.metrics.work()
             << ", pebble=" << result.metrics.place
             << ", unpebble=" << result.metrics.unpebble
             << ", ghost=" << result.metrics.ghost
             << ", peak=" << result.metrics.peak
             << ", cumulative_storage=" << result.area << '\n';
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
