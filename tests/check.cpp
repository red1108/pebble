#include "../pebbling_common.hpp"
#include <limits>

// Keep the checks separate from the three example programs.
namespace kss {
#define main kss_main
#include "../kss.cpp"
#undef main
}
namespace sequential {
#define main sequential_main
#include "../sequential.cpp"
#undef main
}
namespace parallel {
#define main parallel_main
#include "../parallel.cpp"
#undef main
}

uint64_t work() {
    return metrics.place+metrics.unpebble;
}

// Enumerate every balanced boundary size, independently of the greedy choice.
uint64_t best_work(int height, int space) {
    if(height==1) return 1;
    vector<uint64_t> clean(height+1);
    for(int r=2; r<=height; r++) {
        bool rho = r==height;
        int b = rho ? space : space-1;
        uint64_t best = numeric_limits<uint64_t>::max();
        for(int m=2; m<=(1<<(r-1)); m++) {
            int k = 0;
            while((1<<(k+1))<=m) k++;
            int shallow = (1<<(k+1))-m;
            int deep = 2*(m-(1<<k));
            int peak = max(2*m-1, r+m-k-(deep>0));
            if(peak>b) continue;
            uint64_t cost = 3ULL*(1<<(r-1))+m-2-rho;
            cost += shallow*clean[r-k];
            if(deep) cost += deep*clean[r-k-1];
            best = min(best, cost);
        }
        clean[r] = best;
    }
    return clean[height];
}

void check_case(int height, int space) {
    reset(height, space);
    kss::kss_algorithm();
    check_final();
    uint64_t kss_work = work(), half = 1ULL<<(height-1);
    assert(metrics.place == (height==1 ? 1 : (2*height-3)*half+1));
    assert(metrics.unpebble == (height==1 ? 0 : height*half-2));
    assert(metrics.peak == (height==1 ? 1 : height+1));

    reset(height, space);
    sequential::fast_clean(1, h, s, true);
    check_final();
    assert(work()<=kss_work);
    if(height<=12) assert(work()==best_work(height, space));
    if(space==n) assert(work()==2ULL*n-1);

    reset(height, space);
    parallel::parallel_algorithm();
    check_final();
    assert(metrics.depth>0);
    assert(metrics.place<=metrics.area);
    assert(metrics.area<=uint64_t(s)*metrics.depth);
    if(space==n && height<=17) { // Resolve uses full-tree cleanup only through r=16.
        assert(metrics.depth==uint64_t(3*height-2));
        assert(work()==3ULL*n-2);
    }
}

void check_parallel_pebble(int height, int space) {
    reset(height, space);
    parallel::run(parallel::parallel_pebble(1, h, s));
    assert(metrics.place==uint64_t(n) && metrics.unpebble==0);
    assert(metrics.live==1 && placed[1]);
    for(int x=2; x<=n; x++) assert(!placed[x] && is_ghost[x]);
    assert(metrics.ghost==uint64_t(n-1));
    assert(metrics.peak<=s);
}

// Check the paper's one-block formulas, including mixed-depth boundaries.
void check_contracts() {
    for(int height=2; height<=8; height++) {
        for(int m=2; m<=(1<<(height-1)); m++) {
            for(bool rho : {false, true}) {
                reset(height, (1<<height)-1);
                sequential::contract(1, h, m, rho);
                assert(work()==3ULL*(1<<(h-1))+m-2-rho);
                assert(metrics.peak==sequential::boundary_space(h, m));
                assert(metrics.live==rho);
                assert(placed[1]==rho);
            }
        }
    }
}

void check_round_metrics() {
    reset(3, 4);
    parallel::run_round({{4}, {}, {}});
    parallel::run_round({{6}, {4}, {}}); // old and new pebbles both occupy U_t
    assert(metrics.depth==2 && metrics.area==3 && metrics.peak==2);
    assert(metrics.live==1);
    parallel::run_round({{}, {}, {6}}); // ghost-only rounds still take time
    assert(metrics.depth==3 && metrics.area==4 && metrics.live==0);
    parallel::run_round({{6}, {}, {}});
    assert(metrics.exorcise==1);

    reset(2, 3);
    parallel::run_round({{2, 3}, {}, {}});
    parallel::run_round({{1}, {}, {2, 3}});
    assert(metrics.peak==3 && metrics.area==5 && metrics.live==1);
}

// The Python runner expects these invalid rounds to fail an assertion.
void invalid_round(const string& name) {
    reset(3, 4);
    if(name=="dependency") parallel::run_round({{2, 4, 5}, {}, {}});
    else if(name=="conflict") {
        parallel::run_round({{4, 5}, {}, {}});
        parallel::run_round({{2}, {4}, {}});
    } else if(name=="space") {
        parallel::run_round({{4, 5, 6, 7}, {}, {}});
        parallel::run_round({{2}, {6}, {}});
    } else if(name=="ghost") parallel::run_round({{}, {}, {4}});
    else if(name=="duplicate") parallel::run_round({{4, 4}, {}, {}});
    else if(name=="unpebble_ghost") {
        parallel::run_round({{4}, {}, {4}});
        parallel::run_round({{}, {4}, {}});
    }
}

int main(int argc, char** argv) {
    if(argc==2) {
        invalid_round(argv[1]);
        return 0;
    }
    int cases = 0;
    for(int height=1; height<=9; height++) {
        for(int space=(height==1 ? 1 : height+1); space<(1<<height); space++) {
            check_case(height, space);
            check_parallel_pebble(height, space);
            cases++;
        }
    }
    for(int height=10; height<=16; height++) {
        for(int space : {height+1, height+2, 2*height, 4*height,
                         1<<(height/2), (1<<height)-1}) {
            check_case(height, space);
            check_parallel_pebble(height, space);
            cases++;
        }
    }
    // Exercise near-height sweeps and the r>16 Resolve branch.
    for(auto [height, space] : {pair<int,int>{18, 19}, {18, (1<<18)-1}, {20, 21}}) {
        check_case(height, space);
        check_parallel_pebble(height, space);
        cases++;
    }
    check_contracts();
    check_round_metrics();
    cout << "PASS: " << cases << " height/space cases, all three schedules." << endl;
    cout << "KSS counts, balanced-boundary work, parallel rounds, space, and cleanup checked." << endl;
    return 0;
}
