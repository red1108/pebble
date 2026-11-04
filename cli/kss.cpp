#include "kss_schedule.hpp"

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
