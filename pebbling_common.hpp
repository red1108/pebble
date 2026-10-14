#pragma once
#include "quantum_computer.hpp"
#include <iostream>
#include <string>
#include <utility>
using namespace std;

inline void release(QuantumComputer& qc, int x) {
    if(qc.leaf(x)) qc.unpebble(x);
    else qc.measure(x);
}

inline void sequential_pebble(QuantumComputer& qc, int x) {
    if(!qc.leaf(x)) {
        sequential_pebble(qc, 2*x);
        sequential_pebble(qc, 2*x+1);
    }

    qc.pebble(x);

    if(!qc.leaf(x)) {
        release(qc, 2*x);
        release(qc, 2*x+1);
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

inline void print_metrics(const QuantumComputer& qc) {
    const auto& metrics = qc.metrics();
    cout << "h: " << qc.height() << ", n: " << qc.size() << ", space: " << qc.space_limit() << endl;
    cout << "place: " << metrics.place << endl;
    cout << "unpebble: " << metrics.unpebble << endl;
    cout << "total work: " << metrics.work() << endl;
    cout << "measure: " << metrics.measure << endl;
    cout << "correction: " << metrics.correction << endl;
    cout << "peak live: " << metrics.peak << endl;
}
