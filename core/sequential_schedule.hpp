#pragma once
#include <vector>
using namespace std;

struct Block {
    vector<int> boundary; // preparation order
    vector<int> top;      // children before parents
};

template<class Computer>
void release(Computer& qc, int x) {
    if(qc.leaf(x)) qc.unpebble(x);
    else qc.measure(x);
}

template<class Computer>
void sequential_pebble(Computer& qc, int x) {
    if(!qc.leaf(x)) {
        auto [left, right] = qc.children(x);
        sequential_pebble(qc, left);
        sequential_pebble(qc, right);
    }
    qc.pebble(x);
    if(!qc.leaf(x)) {
        auto [left, right] = qc.children(x);
        release(qc, left);
        release(qc, right);
    }
}

template<class Computer>
void contract(Computer& qc, int x, const Block& block, bool rho) {
    for(int v : block.boundary) sequential_pebble(qc, v);
    for(int v : block.top) {
        if(v!=x) qc.pebble(v);
    }
    if(rho) qc.pebble(x);
    else qc.clear(x);
    for(int i=(int)block.top.size()-1; i>=0; i--) {
        if(block.top[i]!=x) qc.unpebble(block.top[i]);
    }
    for(int v : block.boundary) release(qc, v);
}

template<class Computer, class ChooseBoundary>
void fast_clean(Computer& qc, int x, bool rho, ChooseBoundary& choose) {
    if(qc.leaf(x)) {
        if(rho) qc.pebble(x);
        return;
    }
    Block block = choose(x, rho);
    contract(qc, x, block, rho);
    for(int v : block.boundary) fast_clean(qc, v, false, choose);
}
