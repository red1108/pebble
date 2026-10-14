#include "pebbling_common.hpp"

void cleanup(QuantumComputer& qc, int x) {
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

void kss_algorithm(QuantumComputer& qc) {
    sequential_pebble(qc, 1);
    if(qc.height()>1) {
        cleanup(qc, 2);
        cleanup(qc, 3);
    }
}

int main(int argc, char** argv) {
    int h, s;
    if(!read_input(argc, argv, h, s)) return 1;
    QuantumComputer qc(h, s);
    kss_algorithm(qc);
    check_final(qc);

    cout << "KSS Metrics: " << endl;
    print_metrics(qc);
    return 0;
}
