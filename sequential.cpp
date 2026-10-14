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

vector<pair<int,int>> contract(int x, int r, int m, bool rho) {
    vector<pair<int,int>> F;
    vector<int> top;
    boundary(x, r, m, F, top);

    // Taller subtrees first. Their roots stay live during the next calls.
    stable_sort(F.begin(), F.end(), [](auto a, auto b) { return a.second>b.second; });
    for(auto [v, q] : F) sequential_pebble(v);
    for(int v : top) pebble(v);
    for(int i=(int)top.size()-1; i>=0; i--) {
        if(rho && top[i]==x) continue;
        unpebble(top[i]);
    }
    for(auto [v, q] : F) release(v);
    return F;
}

void fast_clean(int x, int r, int b, bool rho) {
    assert(!placed[x]);
    if(r==1) {
        assert(!is_ghost[x]);
        if(rho) pebble(x);
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

    auto F = contract(x, r, m, rho);
    for(auto [v, q] : F) fast_clean(v, q, b-rho, false);
}

int main(int argc, char** argv) {
    if(!read_input(argc, argv)) return 1;
    fast_clean(1, h, s, true);
    check_final();

    cout << "Sequential Metrics: " << endl;
    print_metrics();
    return 0;
}
