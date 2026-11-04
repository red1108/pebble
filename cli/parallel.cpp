#include "parallel_schedule.hpp"

int main(int argc, char** argv) {
    int h, s;
    if(!read_input(argc, argv, h, s)) return 1;
    QuantumComputer qc(h, s);
    parallel_algorithm(qc);
    check_final(qc);

    cout << "Parallel Metrics: " << endl;
    print_metrics(qc);
    cout << "depth: " << qc.metrics().depth << endl;
    cout << "cumulative storage: " << qc.metrics().area << endl;
    return 0;
}
