#pragma once
#include "pebbling_common.hpp"

template<class Computer>
void cleanup(Computer& qc, int x) {
    // assumptions: subtree of x is not pebbled.
    assert(qc.state(x)!=QuantumComputer::State::pebble);
    if(qc.leaf(x)) {
        if(qc.state(x)==QuantumComputer::State::ghost) {
            qc.correction(x);
            qc.unpebble(x);
        }
        return;
    }
    assert(qc.state(x)==QuantumComputer::State::ghost);

    sequential_pebble(qc, 2*x);
    sequential_pebble(qc, 2*x+1);
    qc.correction(x);
    qc.unpebble(x);

    release(qc, 2*x);
    release(qc, 2*x+1);
    cleanup(qc, 2*x);
    cleanup(qc, 2*x+1);
}

template<class Computer>
void kss_algorithm(Computer& qc) {
    sequential_pebble(qc, 1);
    if(qc.height()>1) {
        cleanup(qc, 2);
        cleanup(qc, 3);
    }
}
