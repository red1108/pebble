#include "pebbling_common.hpp"
#include <string>

struct Recorder : QuantumComputer {
    using QuantumComputer::QuantumComputer;
    Schedule trace;
    bool recording = false;
    void execute(const Round& r) {
        QuantumComputer::execute(r);
        if(recording) trace.push_back(r);
    }
    void pebble(int v) { execute({{v}, {}, {}}); }
    void unpebble(int v) { execute({{}, {v}, {}}); }
    void measure(int v) { execute({{}, {}, {v}}); }
    void correction(int v) {
        assert(state(v)==State::ghost);
        pebble(v);
    }
    void clear(int v) { pebble(v); unpebble(v); }
};

#define main unused_sequential_main
#define QuantumComputer Recorder
#include "sequential.cpp"
#undef QuantumComputer
#undef main
#define main unused_kss_main
#define QuantumComputer Recorder
#include "kss.cpp"
#undef QuantumComputer
#undef main
#include "parallel_schedule.hpp"

int main(int argc, char** argv) {
    assert(argc==4 || argc==5);
    std::string kind=argv[1];
    int h=std::stoi(argv[2]), s=std::stoi(argv[3]);
    Recorder qc(h,s);
    qc.recording=argc==5;
    if(kind=="kss") kss_algorithm(qc);
    else if(kind=="sequential") fast_clean(qc,1,h,s,true);
    else { assert(kind=="parallel"); parallel_algorithm(qc); }
    check_final(qc);
    auto m=qc.metrics();
    std::cout << "{\"work\":" << m.work() << ",\"depth\":" << m.depth
              << ",\"peak\":" << m.peak << ",\"place\":" << m.place
              << ",\"unpebble\":" << m.unpebble
              << ",\"ghost\":" << m.measure << ",\"area\":" << m.area << "}\n";
    if(qc.recording) {
        assert(qc.trace.size()==m.depth);
        for(const auto& r:qc.trace) {
            for(int v:r.pebble) std::cout << 'P' << v << ' ';
            for(int v:r.unpebble) std::cout << 'U' << v << ' ';
            for(int v:r.measure) std::cout << 'G' << v << ' ';
            std::cout << '\n';
        }
    }
}
