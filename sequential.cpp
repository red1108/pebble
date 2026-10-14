#include "pebbling_common.hpp"

// F stores (root, height). top is in bottom-up order.
void boundary(int x, int r, int m, vector<pair<int,int>>& F, vector<int>& top) {
    if(m==1) {
        F.push_back({x, r});
        return;
    }
    boundary(2*x, r-1, (m+1)/2, F, top);
    boundary(2*x+1, r-1, m/2, F, top);
    top.push_back(x);
}

int boundary_space(int r, int m) {
    return max(2*m-1, r+m-ceil_log2(m));
}

vector<pair<int,int>> contract(QuantumComputer& qc, int x, int r, int m, bool rho) {
    vector<pair<int,int>> F;
    vector<int> top;
    boundary(x, r, m, F, top);

    // Taller subtrees first. Their roots stay live during the next calls.
    stable_sort(F.begin(), F.end(), [](auto a, auto b) { return a.second>b.second; });
    for(auto [v, q] : F) sequential_pebble(qc, v);
    for(int v : top) qc.pebble(v);
    for(int i=(int)top.size()-1; i>=0; i--) {
        if(rho && top[i]==x) continue;
        qc.unpebble(top[i]);
    }
    for(auto [v, q] : F) release(qc, v);
    return F;
}

void fast_clean(QuantumComputer& qc, int x, int r, int b, bool rho) {
    assert(qc.state(x)!=QuantumComputer::State::pebble);
    if(r==1) {
        assert(qc.state(x)!=QuantumComputer::State::ghost);
        if(rho) qc.pebble(x);
        return;
    }
    assert(b >= r+1);

    int left = 2, right = 1<<(r-1), m = 2;
    while(left <= right) {
        int mid = (left+right)/2;
        if(boundary_space(r, mid) <= b) {
            m = mid;
            left = mid+1;
        } else right = mid-1;
    }

    auto F = contract(qc, x, r, m, rho);
    for(auto [v, q] : F) fast_clean(qc, v, q, b-rho, false);
}

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
