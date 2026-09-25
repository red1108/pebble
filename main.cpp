#include <bits/stdc++.h>
using namespace std;

// ==== metrics =====

int place;
int unpebble;


// ==================
bool placed[1<<20+1];
bool is_ghost[1<<20+1];

void unpebble(int x) {
    // x*2, x*2+1 must exist.
    assert(placed[x*2] && placed[x*2+1]);
    
    if(is_ghost[x]) { // exorcising
        is_ghost[x] = false;
    }
}

int main() {
    ios::sync_with_stdio(0);
    cin.tie(0);
    int n;
    cin>>n;
    

    cout<<n/2+1<<endl;
    return 0;
}