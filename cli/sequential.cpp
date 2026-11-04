#include "complete_tree_schedule.hpp"

int main(int argc, char** argv) {
    int h, s;
    if(!read_input(argc, argv, h, s)) return 1;
    QuantumComputer qc(h, s);
    fast_clean(qc, 1, h, s, true);
    check_final(qc);

    cout << "Sequential Metrics: " << endl;
    print_metrics(qc);
    return 0;
}
