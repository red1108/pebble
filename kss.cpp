#include "pebbling_common.hpp"

void cleanup(int x) {
    // assumptions: subtree of x is not pebbled.
    assert(!placed[x]);
    if(leaf(x)) {
        if(is_ghost[x]) {
            pebble(x);
            unpebble(x);
        }
        return;
    }
    assert(is_ghost[x]);

    sequential_pebble(2*x);
    sequential_pebble(2*x+1);
    pebble(x); // exorcise, then uncompute
    unpebble(x);

    release(2*x);
    release(2*x+1);
    cleanup(2*x);
    cleanup(2*x+1);
}

void kss_algorithm() {
    sequential_pebble(1);
    if(n>1) {
        cleanup(2);
        cleanup(3);
    }
}

int main(int argc, char** argv) {
    if(!read_input(argc, argv)) return 1;
    kss_algorithm();
    check_final();

    cout << "KSS Metrics: " << endl;
    print_metrics();
    return 0;
}
