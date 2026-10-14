#pragma once
#include "quantum_computer.hpp"
#include "sequential_schedule.hpp"
#include <iostream>
#include <string>
#include <utility>
using namespace std;

using Round = QuantumComputer::Round;
using Schedule = vector<Round>;

inline void run(QuantumComputer& qc, const Schedule& schedule) {
    for(const auto& round : schedule) qc.execute(round);
}

inline void append(Schedule& out, Schedule next) {
    for(auto& round : next) out.push_back(std::move(round));
}

// Overlay schedules on disjoint subtrees, one round at a time.
inline void parallelize(Schedule& out, const Schedule& next) {
    out.resize(max(out.size(), next.size()));
    for(size_t t=0; t<next.size(); t++) {
        for(int x : next[t].pebble) out[t].pebble.push_back(x);
        for(int x : next[t].unpebble) out[t].unpebble.push_back(x);
        for(int x : next[t].measure) out[t].measure.push_back(x);
    }
}

inline int ceil_log2(int x) {
    int k = 0;
    while((1<<k) < x) k++;
    return k;
}

inline bool read_input(int argc, char** argv, int& height, int& space) {
    try {
        if(argc > 3) throw invalid_argument("too many arguments");
        int values[2] = {10, 11};
        for(int i=1; i<argc; i++) {
            size_t used = 0;
            values[i-1] = stoi(argv[i], &used);
            if(argv[i][used]) throw invalid_argument("expected an integer");
        }
        height = values[0];
        if(height<1 || height>QuantumComputer::max_height)
            throw invalid_argument("height must be in [1, 20]");
        space = argc>2 ? values[1] : (height==1 ? 1 : height+1);
        if(space<(height==1 ? 1 : height+1) || space>(1<<height)-1)
            throw invalid_argument("space must be between the minimum and n");
        return true;
    } catch(const exception& e) {
        cerr << e.what() << "\nusage: " << argv[0] << " [height [space]]" << endl;
        return false;
    }
}

inline void check_final(const QuantumComputer& qc) {
    const auto& metrics = qc.metrics();
    assert(qc.finished());
    assert(metrics.peak <= qc.space_limit());
    assert(metrics.place == metrics.unpebble+metrics.measure+1);
    assert(metrics.correction == metrics.measure);
}

inline void print_metrics(const QuantumComputer& qc, bool show_tree = true) {
    const auto& metrics = qc.metrics();
    if(show_tree) cout << "h: " << qc.height() << ", n: " << qc.size() << ", space: " << qc.space_limit() << endl;
    cout << "place: " << metrics.place << endl;
    cout << "unpebble: " << metrics.unpebble << endl;
    cout << "total work: " << metrics.work() << endl;
    cout << "measure: " << metrics.measure << endl;
    cout << "correction: " << metrics.correction << endl;
    cout << "peak live: " << metrics.peak << endl;
}
