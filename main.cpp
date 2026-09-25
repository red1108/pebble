#include <bits/stdc++.h>
using namespace std;

// ==== metrics =====
struct Metrics {
    std::uint64_t place = 0, unpebble = 0, ghost = 0;
    int live = 0, peak = 0;
} metrics_kss, metrics_ours;

// ==================
const int h = 10, n = (1<<h)-1;
bool placed[1<<h]; // 1-based complete binary tree
bool is_ghost[1<<h]; // 1-based complete binary tree


void pebble(int x) {
    // x*2, x*2+1 must exist.
    assert(placed[x*2] && placed[x*2+1]);
    placed[x] = true;
}

void unpebble(int x) {
    // x*2, x*2+1 must exist.
    assert(placed[x*2] && placed[x*2+1]);
    
    if(is_ghost[x]) { // exorcising
        is_ghost[x] = false;
    }
}

void ghost(int x) {
    assert(placed[x]);
    
    placed[x] = false;
    is_ghost[x] = true;
}

int main() {
    ios::sync_with_stdio(0);
    cin.tie(0);
    int n;
    cin>>n;
    
    kss_algorithm();


    cout<<n/2+1<<endl;
    return 0;
}