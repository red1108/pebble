#include "../parallel_schedule.hpp"

struct TraceComputer {
    int h, s;
    QuantumComputer checker;
    Schedule rounds;
    TraceComputer(int height, int space)
        : h(height), s(space), checker(h, min((1<<h)-1,s+1)) {}
    int height() const { return h; }
    int space_limit() const { return s; }
    void execute(const Round& round) {
        checker.execute(round);
        rounds.push_back(round);
    }
};

int main(int argc, char** argv) {
    int h, s;
    if(!read_input(argc, argv, h, s)) return 1;
    TraceComputer qc(h, s);
    run(qc, parallel_pebble(1, h, s));
    // The CFS output accumulators are already included in fixed workspace.
    parallel_cleanup(qc, s);
    check_final(qc.checker);
    cout << qc.rounds.size() << '\n';
    for(const auto& round : qc.rounds) {
        for(const auto* actions : {&round.pebble, &round.unpebble, &round.measure}) {
            cout << actions->size();
            for(int v : *actions) cout << ' ' << v;
            cout << '\n';
        }
    }
}
