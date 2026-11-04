#pragma once
#include "pebbling_common.hpp"

// F stores (root, height). top is in bottom-up order.
inline void boundary(int x, int r, int m, vector<pair<int,int>>& F, vector<int>& top) {
    if(m==1) {
        F.push_back({x, r});
        return;
    }
    boundary(2*x, r-1, (m+1)/2, F, top);
    boundary(2*x+1, r-1, m/2, F, top);
    top.push_back(x);
}

inline int boundary_space(int r, int m) {
    return max(2*m-1, r+m-ceil_log2(m));
}

inline Block choose_boundary(int x, int r, int b) {
    assert(b >= r+1);
    int left = 2, right = 1<<(r-1), m = 2;
    while(left <= right) {
        int mid = (left+right)/2;
        if(boundary_space(r, mid) <= b) {
            m = mid;
            left = mid+1;
        } else right = mid-1;
    }
    vector<pair<int,int>> F;
    Block block;
    boundary(x, r, m, F, block.top);
    stable_sort(F.begin(), F.end(), [](auto a, auto b) { return a.second>b.second; });
    for(auto [v, q] : F) block.boundary.push_back(v);
    return block;
}

template<class Computer>
void fast_clean(Computer& qc, int x, int r, int b, bool rho) {
    assert(qc.state(x)!=QuantumComputer::State::pebble);
    auto choose = [&](int v, bool) {
        int q = r;
        for(int u=v; u!=x; u/=2) {
            assert(u>x);
            q--;
        }
        return choose_boundary(v, q, b-(rho && v!=x));
    };
    fast_clean(qc, x, rho, choose);
}
