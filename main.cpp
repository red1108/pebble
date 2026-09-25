#include <bits/stdc++.h>
using namespace std;

// ==== metrics =====
struct Metrics {
    std::uint64_t place = 0, unpebble = 0; // only these counted for total work
    std::uint64_t ghost = 0, exorcise = 0;
    int live = 0, peak = 0;
} metrics;

// ==================
const int h = 10, n = (1<<h)-1;
bool placed[1<<h]; // 1-based complete binary tree
bool is_ghost[1<<h]; // 1-based complete binary tree

bool child_ready(int x){
    assert(1<=x && x<=n);
    return x > n / 2 || (placed[2*x] && placed[2*x+1]);
}
bool leaf(int x) {
    assert(1<=x && x<=n);
    return x > n / 2;
}

void pebble(int x) {
    // x*2, x*2+1 must exist.
    assert(child_ready(x));
    assert(!placed[x]);

    placed[x] = true;
    is_ghost[x] = false;
    metrics.place++;
    metrics.live++;
    metrics.peak = max(metrics.peak, metrics.live);
}

void unpebble(int x) {
    assert(child_ready(x));
    assert(placed[x] || is_ghost[x]);

    if(is_ghost[x]) { // exorcising
        pebble(x);
        metrics.exorcise++;
    }
    
    placed[x] = false;
    metrics.unpebble++;
    metrics.live--;
}

void ghost(int x) {
    assert(placed[x]);
    
    placed[x] = false;
    is_ghost[x] = true;
    metrics.ghost++;
    metrics.live--;
}

void remove_kss(int x) {
    assert(placed[x]);
    if(child_ready(x)) unpebble(x);
    else ghost(x);
}

void fast(int x) {
    if (!leaf(x)) {
        fast(2*x);
        fast(2*x+1);
    }

    pebble(x);

    if (!leaf(x)) {
        remove_kss(2*x);
        remove_kss(2*x+1);
    }
}

void cleanup(int x) {
    // assumptions: subtree of x is not pebbled.
    assert(!placed[x]);
    if(leaf(x)) {
        if(is_ghost[x]) unpebble(x);
        return ;
    }
    assert(is_ghost[x]);
    //prepare for unpebbling x
    fast(2*x);
    fast(2*x+1);
    unpebble(x);

    // make children ghost
    remove_kss(2*x);
    remove_kss(2*x+1);

    cleanup(2*x);
    cleanup(2*x+1);
}

void kss_algorithm() {
    fast(1);
    if(n>1) {
        cleanup(2);
        cleanup(3);
    }
}

int main() {
    // ios::sync_with_stdio(0);
    // cin.tie(0);
    memset(placed, 0, sizeof(placed));
    memset(is_ghost, 0, sizeof(is_ghost));
    
    kss_algorithm();
    for(int i=2; i<=n; i++) {
        assert(!placed[i]);
        assert(!is_ghost[i]);
    }
    assert(placed[1]);
    assert(!is_ghost[1]);
    

    cout<<"KSS Metrics: "<<endl;
    cout<<"place: "<<metrics.place<<endl;
    cout<<"unpebble: "<<metrics.unpebble<<endl;
    cout<<"total work: "<<metrics.place+metrics.unpebble<<endl;
    cout<<"peak live: "<<metrics.peak<<endl;

    cout<<n/2+1<<endl;
    return 0;
}