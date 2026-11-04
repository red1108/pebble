#include "round_packing.hpp"
#include <cmath>

// KMRV, Algorithms 2.2 and 2.3: https://arxiv.org/abs/2510.08432
vector<int> A = {0, 1, 2, 2};

Schedule blast(int start, int k) {
    Schedule out;
    if(k==1) return out;
    if(k<=3) return {{{start+1}, {}, {}}};
    out.push_back({{start+1}, {}, {}});
    for(int j=start+2; j<=start+A[k-3]; j++)
        out.push_back({{j}, {}, {j-1}});
    append(out, blast(start+A[k-3], k-2));
    return out;
}

Schedule unblast(int start, int k) {
    if(k==1) return {};
    if(k<=3) return {{{}, {start+1}, {}}};
    int cut = start+A[k-3], prefix = A[k-2]-A[k-3];
    Schedule right = unblast(cut, k-2);
    Schedule out(right.begin(), right.begin()+prefix);
    Schedule tail(right.begin()+prefix, right.end());
    parallelize(tail, blast(start, k-3));
    append(out, std::move(tail));
    out.push_back({{}, {cut}, {}});
    append(out, unblast(start, k-3));
    return out;
}

Schedule kmrv_path(int length) {
    while(A.back()<length) {
        int k = (int)A.size();
        A.push_back(A[k-2]+A[k-3]);
    }
    int k = 1;
    while(A[k]<length) k++;
    Schedule full = blast(0, k);
    full.push_back({{A[k]}, {}, {}}); // retain the final endpoint
    append(full, unblast(0, k));

    int offset = A[k]-length;
    Schedule out;
    for(const auto& round : full) {
        Round next;
        for(int x : round.pebble) if(x>offset) next.pebble.push_back(x-offset);
        for(int x : round.unpebble) if(x>offset) next.unpebble.push_back(x-offset);
        for(int x : round.measure) if(x>offset) next.measure.push_back(x-offset);
        if(!next.pebble.empty() || !next.unpebble.empty() || !next.measure.empty())
            out.push_back(std::move(next));
    }
    assert(out.size()==size_t(2*length-1));
    return out;
}

// Prefix i maps to vertex N-i; inputs x_1,...,x_N occupy vertices N,...,2N-1.
vector<array<int, 2>> left_fold_tree(int inputs) {
    vector<array<int, 2>> tree(2*inputs, {0, 0});
    for(int x=1; x<inputs; x++) tree[x] = {x+1, 2*inputs-x};
    return tree;
}

// Validate the unexpanded path with read-only inputs already present.
// These input registers are excluded only from the reported raw path metrics.
QuantumComputer::Metrics check_path(int inputs, const Schedule& path) {
    QuantumComputer qc(left_fold_tree(inputs), 2*inputs-1);
    vector<int> leaves;
    for(int x=inputs; x<2*inputs; x++) leaves.push_back(x);
    qc.pebble(leaves);
    for(const auto& round : path) {
        Round mapped;
        for(int i : round.pebble) mapped.pebble.push_back(inputs-i);
        for(int i : round.unpebble) mapped.unpebble.push_back(inputs-i);
        for(int i : round.measure) mapped.measure.push_back(inputs-i);
        qc.execute(mapped);
    }
    auto raw = qc.metrics();
    raw.place -= inputs;
    raw.peak -= inputs;
    raw.occupied -= inputs;
    raw.area -= uint64_t(inputs)*raw.depth;
    raw.depth--;
    qc.unpebble(leaves);
    check_final(qc);
    return raw;
}

// Load side inputs before each batch, then measure them after use.
// Final input-ghost cleanup is included in the returned schedule.
Schedule prepare_inputs(int inputs, int space, const Schedule& path) {
    QuantumComputer qc(left_fold_tree(inputs), space);
    Schedule out;
    auto emit = [&](Round round) {
        qc.execute(round);
        out.push_back(std::move(round));
    };
    for(const auto& round : path) {
        vector<pair<char, int>> jobs;
        for(int i : round.unpebble) jobs.push_back({'U', inputs-i});
        for(int i : round.pebble) jobs.push_back({'P', inputs-i});
        vector<int> ghosts;
        for(int i : round.measure) ghosts.push_back(inputs-i);
        if(jobs.empty()) {
            emit({{}, {}, ghosts});
            continue;
        }
        size_t first = 0;
        while(first<jobs.size()) {
            Round operations;
            vector<int> leaves;
            int occupied = qc.metrics().occupied;
            size_t next = first;
            while(next<jobs.size()) {
                auto [op, x] = jobs[next];
                auto [left, right] = qc.children(x);
                int extra = int(qc.leaf(left))+int(qc.leaf(right))+(op=='P');
                if(occupied+extra>space) break;
                occupied += extra;
                if(qc.leaf(left)) leaves.push_back(left);
                if(qc.leaf(right)) leaves.push_back(right);
                if(op=='P') operations.pebble.push_back(x);
                else operations.unpebble.push_back(x);
                next++;
            }
            if(next==first) throw runtime_error("KMRV input preparation does not fit this space budget");
            emit({leaves, {}, {}});
            operations.measure = std::move(leaves);
            if(next==jobs.size())
                operations.measure.insert(operations.measure.end(), ghosts.begin(), ghosts.end());
            emit(std::move(operations));
            first = next;
        }
    }
    for(int first=inputs; first<2*inputs; first+=space-1) {
        vector<int> leaves;
        for(int x=first; x<min(2*inputs, first+space-1); x++) {
            assert(qc.state(x)==QuantumComputer::State::ghost);
            leaves.push_back(x);
        }
        emit({leaves, {}, {}});
        emit({{}, leaves, {}});
    }
    check_final(qc);
    return out;
}

int main(int argc, char** argv) {
    try {
        int h, s;
        if(!read_input(argc, argv, h, s)) return 1;
        if(h<3) throw invalid_argument("KMRV comparison needs height at least 3");
        int inputs = 1<<(h-1), length = inputs-1;
        if(argc<3) s = int(ceil(2.47*log2(length)));
        QuantumComputer qc(left_fold_tree(inputs), s);
        Schedule path = kmrv_path(length);
        auto raw = check_path(inputs, path);
        run_packed(qc, prepare_inputs(inputs, s, path));
        check_final(qc);
        cout << "KMRV retained-output construction:" << endl;
        cout << "h: " << h << ", inputs: " << inputs << ", path length: " << length << ", space: " << s << endl;
        print_metrics(qc, false);
        cout << "path rounds: " << raw.depth << endl;
        cout << "path work: " << raw.work() << endl;
        cout << "path peak: " << raw.peak << endl;
        cout << "depth: " << qc.metrics().depth << endl;
        cout << "cumulative storage: " << qc.metrics().area << endl;
    } catch(const exception& error) {
        cerr << error.what() << endl;
        return 1;
    }
    return 0;
}
