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
    QuantumComputer qc(height, space);
    kss::kss_algorithm(qc);
    check_final(qc);
    auto metrics = qc.metrics();
    uint64_t kss_work = metrics.work(), half = 1ULL<<(height-1);
    assert(metrics.place == (height==1 ? 1 : (2*height-3)*half+1));
    assert(metrics.unpebble == (height==1 ? 0 : height*half-2));
    assert(metrics.peak == (height==1 ? 1 : height+1));
    assert(metrics.depth == metrics.work()+metrics.measure);

    qc = QuantumComputer(height, space);
    sequential::fast_clean(qc, 1, height, space, true);
    check_final(qc);
    metrics = qc.metrics();
    assert(metrics.work()<=kss_work);
    if(height<=12) assert(metrics.work()==best_work(height, space));
    if(space==qc.size()) assert(metrics.work()==2ULL*qc.size()-1);
    assert(metrics.depth == metrics.work()+metrics.measure);

    qc = QuantumComputer(height, space);
    parallel::parallel_algorithm(qc);
    check_final(qc);
    metrics = qc.metrics();
    assert(metrics.depth>0);
    assert(metrics.place<=metrics.area);
    assert(metrics.area<=uint64_t(space)*metrics.depth);
    if(space==qc.size() && height<=17) { // Full-tree cleanup through r=16.
        assert(metrics.depth==uint64_t(3*height-2));
        assert(metrics.work()==3ULL*qc.size()-2);
    }
}

void check_parallel_pebble(int height, int space) {
    QuantumComputer qc(height, space);
    parallel::run(qc, parallel::parallel_pebble(1, height, space));
    const auto& metrics = qc.metrics();
    assert(metrics.place==uint64_t(qc.size()) && metrics.unpebble==0);
    assert(metrics.occupied==1 && qc.state(1)==QuantumComputer::State::pebble);
    for(int x=2; x<=qc.size(); x++) assert(qc.state(x)==QuantumComputer::State::ghost);
    assert(metrics.measure==uint64_t(qc.size()-1));
    assert(metrics.peak<=space);
}

// Check the paper's one-block formulas, including mixed-depth boundaries.
void check_contracts() {
    for(int h=2; h<=8; h++) {
        for(int m=2; m<=(1<<(h-1)); m++) {
            for(bool rho : {false, true}) {
                QuantumComputer qc(h, (1<<h)-1);
                sequential::contract(qc, 1, h, m, rho);
                const auto& metrics = qc.metrics();
                assert(metrics.work()==3ULL*(1<<(h-1))+m-2-rho);
                assert(metrics.peak==sequential::boundary_space(h, m));
                assert(metrics.occupied==rho);
                assert((qc.state(1)==QuantumComputer::State::pebble)==rho);
            }
        }
    }
}

void check_round_metrics() {
    QuantumComputer qc(3, 4);
    qc.execute({{4}, {}, {}});
    qc.execute({{6}, {4}, {}}); // old and new pebbles both occupy U_t
    assert(qc.metrics().depth==2 && qc.metrics().area==3 && qc.metrics().peak==2);
    assert(qc.metrics().occupied==1);
    qc.execute({{}, {}, {6}}); // ghost-only rounds still take time
    assert(qc.metrics().depth==3 && qc.metrics().area==4 && qc.metrics().occupied==0);
    qc.execute({{6}, {}, {}});
    assert(qc.metrics().correction==1);

    QuantumComputer small(2, 3);
    small.execute({{2, 3}, {}, {}});
    small.execute({{1}, {}, {2, 3}});
    assert(small.metrics().peak==3 && small.metrics().area==5 && small.metrics().occupied==1);
}

void check_interface() {
    QuantumComputer qc(2, 3), other(1, 1);
    assert(!qc.finished() && !other.finished());
    qc.pebble(vector<int>{2, 3});
    other.pebble(1);
    qc.execute({{1}, {}, {2, 3}});
    assert(!qc.finished()); // ghosts still need correction
    qc.correction(vector<int>{2, 3});
    assert(qc.state(2)==QuantumComputer::State::pebble);
    assert(qc.metrics().place==5 && qc.metrics().correction==2);
    qc.unpebble(vector<int>{2, 3});
    check_final(qc);
    assert(qc.metrics().depth==4 && qc.metrics().area==11 && qc.metrics().work()==7);
    assert(other.finished() && other.metrics().place==1 && other.metrics().depth==1);

    // Scalar calls and one-element vectors have identical semantics.
    QuantumComputer scalar(2, 3), batch(2, 3);
    scalar.pebble(2);
    scalar.measure(2);
    scalar.correction(2);
    scalar.unpebble(2);
    batch.pebble(vector<int>{2});
    batch.measure(vector<int>{2});
    batch.correction(vector<int>{2});
    batch.unpebble(vector<int>{2});
    for(const QuantumComputer* computer : {&scalar, &batch}) {
        const auto& m = computer->metrics();
        for(int x=1; x<=3; x++) assert(computer->state(x)==QuantumComputer::State::empty);
        assert(m.place==2 && m.unpebble==1 && m.measure==1 && m.correction==1);
        assert(m.work()==3 && m.depth==4 && m.area==4 && m.peak==1 && m.occupied==0);
    }

    // A vector measurement releases the whole level in one round.
    QuantumComputer measured(2, 3);
    measured.pebble(vector<int>{2, 3});
    measured.measure(vector<int>{2, 3});
    measured.pebble(vector<int>{2, 3}); // implicit correction
    assert(measured.metrics().correction==2 && measured.metrics().depth==3);
    assert(measured.metrics().area==6 && measured.metrics().occupied==2);
    measured.execute({{1}, {}, {1, 2, 3}}); // even a new pebble can be measured
    assert(measured.metrics().occupied==0 && measured.metrics().area==9);
}

// The Python runner expects these invalid operations to fail an assertion.
void invalid_round(const string& name) {
    QuantumComputer qc(3, 4);
    if(name=="dependency") qc.execute({{2, 4, 5}, {}, {}});
    else if(name=="conflict") {
        qc.pebble(vector<int>{4, 5});
        qc.execute({{2}, {4}, {}});
    } else if(name=="space") {
        qc.pebble(vector<int>{4, 5, 6, 7});
        qc.execute({{2}, {6}, {}});
    } else if(name=="ghost") qc.measure(4);
    else if(name=="duplicate") qc.pebble(vector<int>{4, 4});
    else if(name=="unpebble_ghost") {
        qc.execute({{4}, {}, {4}});
        qc.unpebble(4);
    } else if(name=="correction_empty") qc.correction(4);
    else if(name=="correction_pebble") {
        qc.pebble(4);
        qc.correction(4);
    } else if(name=="correction_batch") {
        qc.execute({{4}, {}, {4}});
        qc.correction(vector<int>{4, 5});
    } else if(name=="measure_removed") {
        qc.pebble(4);
        qc.execute({{}, {4}, {4}});
    } else if(name=="measure_duplicate") {
        qc.pebble(4);
        qc.measure(vector<int>{4, 4});
    } else if(name=="empty") qc.execute({{}, {}, {}});
    else if(name=="index") qc.pebble(8);
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
    check_interface();
    cout << "PASS: " << cases << " height/space cases, all three schedules." << endl;
    cout << "KSS counts, balanced-boundary work, parallel rounds, space, and cleanup checked." << endl;
    return 0;
}
