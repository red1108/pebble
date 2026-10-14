#include "../sequential_schedule.hpp"
#include <array>
#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <utility>
using namespace std;

// Translate shared scheduling calls into the public circuit operations.
struct CircuitTrace {
    int root;
    vector<array<int, 2>> tree;
    vector<pair<char, int>> trace;

    bool leaf(int x) const { return tree[x][0]==0; }
    array<int, 2> children(int x) const { return tree[x]; }
    void pebble(int x) { trace.push_back({x==root ? 'R' : 'P', x}); }
    void unpebble(int x) { trace.push_back({'U', x}); }
    void measure(int x) { trace.push_back({'G', x}); }
    void clear(int x) { trace.push_back({'C', x}); } // direct phase correction
};

void top_block(const CircuitTrace& qc, int x, const vector<bool>& boundary, vector<int>& top) {
    if(boundary[x]) return;
    assert(!qc.leaf(x));
    auto [left, right] = qc.children(x);
    top_block(qc, left, boundary, top);
    top_block(qc, right, boundary, top);
    top.push_back(x);
}

Block read_block(const CircuitTrace& qc, int x) {
    int m;
    cin >> m;
    assert(m>=2 && m<(int)qc.tree.size());
    Block block;
    vector<bool> boundary(qc.tree.size());
    for(int i=0; i<m; i++) {
        int v;
        cin >> v;
        assert(v>0 && v<(int)qc.tree.size() && !boundary[v] && v!=x);
        boundary[v] = true;
        block.boundary.push_back(v);
    }
    top_block(qc, x, boundary, block.top);
    return block;
}

int main() {
    int n;
    CircuitTrace qc;
    if(!(cin >> n >> qc.root)) return 1;
    assert(n>0 && qc.root>0 && qc.root<=n);
    qc.tree.resize(n+1);
    for(int x=1; x<=n; x++) {
        auto& [left, right] = qc.tree[x];
        cin >> left >> right;
        assert(left>=0 && left<=n && right>=0 && right<=n);
        assert((left==0)==(right==0));
    }

    string command;
    while(cin >> command) {
        int x;
        cin >> x;
        assert(x>0 && x<=n);
        qc.trace.clear();
        if(command=="fast") sequential_pebble(qc, x);
        else if(command=="block") {
            bool rho;
            cin >> rho;
            Block block = read_block(qc, x);
            contract(qc, x, block, rho);
        } else if(command=="clean") {
            bool rho;
            int count;
            cin >> rho >> count;
            map<int, Block> blocks;
            for(int i=0; i<count; i++) {
                int v;
                cin >> v;
                assert(v>0 && v<=n);
                blocks[v] = read_block(qc, v);
            }
            auto choose = [&](int v, bool) { return blocks.at(v); };
            fast_clean(qc, x, rho, choose);
        } else return 1;
        if(!cin) return 1;
        cout << qc.trace.size() << '\n';
        for(auto [op, v] : qc.trace) cout << op << ' ' << v << '\n';
        cout << flush;
    }
    return 0;
}
