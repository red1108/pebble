#include "parallel_schedule.hpp"
#include "round_packing.hpp"
#include <limits>
#include <map>

struct ParallelPlan {
    uint64_t depth;
    int cut;
};

// Choose cuts by their scheduled depth. Each batch uses the available space,
// accounting for completed boundary roots that must remain pebbled.
class ParallelPlanner {
public:
    Schedule pebble(int x, int r, int b) {
        int k = plan(r, b, false).cut;
        Schedule out;
        if(k==0) {
            for(int d=r-1; d>=0; d--) {
                Round round;
                add_level(round.pebble, x, d);
                if(d<r-1) add_level(round.measure, x, d+1);
                out.push_back(std::move(round));
            }
            return out;
        }
        prepare(out, x, r, k, b);
        for(int d=k-1; d>=0; d--) {
            Round round;
            add_level(round.pebble, x, d);
            add_level(round.measure, x, d+1);
            out.push_back(std::move(round));
        }
        return out;
    }

    Schedule clean(int x, int r, int b) {
        int k = plan(r, b, true).cut;
        if(k==0) return clean_small(x, r);
        Schedule out;
        prepare(out, x, r, k, b);
        for(int d=k-1; d>=0; d--) {
            Round round;
            add_level(round.pebble, x, d);
            out.push_back(std::move(round));
        }
        for(int d=0; d<k; d++) {
            Round round;
            add_level(round.unpebble, x, d);
            out.push_back(std::move(round));
        }
        Round release;
        if(r-k==1) add_level(release.unpebble, x, k);
        else add_level(release.measure, x, k);
        out.push_back(std::move(release));
        if(r-k>1) append(out, clean_forest(x<<k, 1<<k, r-k, b));
        return out;
    }

    Schedule clean_forest(int first, int count, int r, int b) {
        Schedule out;
        for(int done=0; done<count;) {
            int batch = min(count-done, b/min_space(r));
            int local = min((1<<r)-1, b/batch);
            Schedule next;
            for(int i=0; i<batch; i++) parallelize(next, clean(first+done+i, r, local));
            append(out, std::move(next));
            done += batch;
        }
        return out;
    }

    ParallelPlan plan(int r, int b, bool cleanup) {
        b = min(b, (1<<r)-1);
        assert(b>=min_space(r));
        auto& cache = cleanup ? clean_cache : pebble_cache;
        auto key = make_pair(r, b);
        auto found = cache.find(key);
        if(found!=cache.end()) return found->second;
        int full_space = cleanup ? (1<<r)-1 : (r==1 ? 1 : 3*(1<<(r-2)));
        if(b>=full_space) return cache[key] = {uint64_t((cleanup ? 2 : 1)*r), 0};

        ParallelPlan best = {numeric_limits<uint64_t>::max(), -1};
        for(int k=1; k<r; k++) {
            int m = 1<<k, q = r-k;
            int top_space = cleanup ? 2*m-1 : m+m/2;
            if(top_space>b || m-1+min_space(q)>b) continue;
            uint64_t depth = prepare_depth(m, q, b)+(cleanup ? 2*k+1 : k);
            if(cleanup && q>1) depth += forest_depth(m, q, b);
            if(depth<best.depth) best = {depth, k};
        }
        assert(best.cut>0);
        return cache[key] = best;
    }

private:
    map<pair<int, int>, ParallelPlan> pebble_cache, clean_cache;

    static int min_space(int r) { return r==1 ? 1 : r+1; }

    uint64_t prepare_depth(int count, int r, int b) {
        uint64_t depth = 0;
        for(int done=0; done<count;) {
            int batch = min(count-done, (b-done)/min_space(r));
            int local = min((1<<r)-1, (b-done)/batch);
            depth += plan(r, local, false).depth;
            done += batch;
        }
        return depth;
    }

    uint64_t forest_depth(int count, int r, int b) {
        uint64_t depth = 0;
        for(int done=0; done<count;) {
            int batch = min(count-done, b/min_space(r));
            int local = min((1<<r)-1, b/batch);
            depth += plan(r, local, true).depth;
            done += batch;
        }
        return depth;
    }

    void prepare(Schedule& out, int x, int r, int k, int b) {
        int count = 1<<k, q = r-k;
        for(int done=0; done<count;) {
            int batch = min(count-done, (b-done)/min_space(q));
            int local = min((1<<q)-1, (b-done)/batch);
            Schedule next;
            for(int i=0; i<batch; i++) parallelize(next, pebble((x<<k)+done+i, q, local));
            append(out, std::move(next));
            done += batch;
        }
    }
};

void parallel_optimized(QuantumComputer& qc) {
    ParallelPlanner planner;
    Schedule schedule;
    if(qc.size()<=qc.space_limit()) {
        schedule = clean_small(1, qc.height());
        schedule.erase(schedule.begin()+qc.height()); // keep the root
    } else {
        schedule = planner.pebble(1, qc.height(), qc.space_limit());
        if(qc.height()>1)
            append(schedule, planner.clean_forest(2, 2, qc.height()-1, qc.space_limit()-1));
    }
    run_packed(qc, schedule);
}

int main(int argc, char** argv) {
    int h, s;
    if(!read_input(argc, argv, h, s)) return 1;
    QuantumComputer qc(h, s);
    parallel_optimized(qc);
    check_final(qc);
    cout << "Optimized Parallel Metrics: " << endl;
    print_metrics(qc);
    cout << "depth: " << qc.metrics().depth << endl;
    cout << "cumulative storage: " << qc.metrics().area << endl;
    return 0;
}
