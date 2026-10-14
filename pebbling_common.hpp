#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using namespace std;

// ==== metrics =====
struct Metrics {
    std::uint64_t place = 0, unpebble = 0; // only these counted for total work
    std::uint64_t ghost = 0, exorcise = 0;
    std::uint64_t depth = 0, area = 0; // parallel rounds and cumulative storage
    int live = 0, peak = 0;
} metrics;

// ==================
const int max_h = 20;
int h = 10, n = (1<<h)-1, s = h+1;
bool placed[1<<max_h]; // 1-based complete binary tree
bool is_ghost[1<<max_h];

bool leaf(int x) {
    assert(1<=x && x<=n);
    return x > n/2;
}

bool child_ready(int x) {
    return leaf(x) || (placed[2*x] && placed[2*x+1]);
}

void pebble(int x) {
    assert(child_ready(x));
    assert(!placed[x]);

    if(is_ghost[x]) metrics.exorcise++;
    placed[x] = true;
    is_ghost[x] = false;
    metrics.place++;
    metrics.live++;
    metrics.peak = max(metrics.peak, metrics.live);
    assert(metrics.live <= s);
}

void unpebble(int x) {
    assert(child_ready(x));
    assert(placed[x]);

    placed[x] = false;
    metrics.unpebble++;
    metrics.live--;
}

void ghost(int x) {
    assert(1<=x && x<=n);
    assert(placed[x]);
    placed[x] = false;
    is_ghost[x] = true;
    metrics.ghost++;
    metrics.live--;
}

void release(int x) {
    if(leaf(x)) unpebble(x);
    else ghost(x);
}

void sequential_pebble(int x) {
    if(!leaf(x)) {
        sequential_pebble(2*x);
        sequential_pebble(2*x+1);
    }

    pebble(x);

    if(!leaf(x)) {
        release(2*x);
        release(2*x+1);
    }
}

int ceil_log2(int x) {
    int k = 0;
    while((1<<k) < x) k++;
    return k;
}

void reset(int height, int space) {
    assert(1<=height && height<=max_h);
    assert((height==1 ? 1 : height+1)<=space && space<=(1<<height)-1);
    h = height;
    n = (1<<h)-1;
    s = space;
    memset(placed, 0, sizeof(placed));
    memset(is_ghost, 0, sizeof(is_ghost));
    metrics = {};
}

bool read_input(int argc, char** argv) {
    try {
        if(argc > 3) throw invalid_argument("too many arguments");
        int values[2] = {10, 11};
        for(int i=1; i<argc; i++) {
            size_t used = 0;
            values[i-1] = stoi(argv[i], &used);
            if(argv[i][used]) throw invalid_argument("expected an integer");
        }
        int height = values[0];
        if(height<1 || height>max_h) throw invalid_argument("height must be in [1, 20]");
        int space = argc>2 ? values[1] : (height==1 ? 1 : height+1);
        if(space<(height==1 ? 1 : height+1) || space>(1<<height)-1)
            throw invalid_argument("space must be between the minimum and n");
        reset(height, space);
        return true;
    } catch(const exception& e) {
        cerr << e.what() << "\nusage: " << argv[0] << " [height [space]]" << endl;
        return false;
    }
}

void check_final() {
    for(int i=2; i<=n; i++) {
        assert(!placed[i]);
        assert(!is_ghost[i]);
    }
    assert(placed[1]);
    assert(!is_ghost[1]);
    assert(metrics.live == 1);
    assert(metrics.peak <= s);
    assert(metrics.place == metrics.unpebble+metrics.ghost+1);
    assert(metrics.exorcise == metrics.ghost);
}

void print_metrics() {
    cout << "h: " << h << ", n: " << n << ", space: " << s << endl;
    cout << "place: " << metrics.place << endl;
    cout << "unpebble: " << metrics.unpebble << endl;
    cout << "total work: " << metrics.place+metrics.unpebble << endl;
    cout << "ghost: " << metrics.ghost << endl;
    cout << "exorcise: " << metrics.exorcise << endl;
    cout << "peak live: " << metrics.peak << endl;
}
