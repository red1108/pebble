#include "pebbling_common.hpp"
#include <iostream>

int main() {
    int n,s;
    std::cin >> n >> s;
    std::vector<std::array<int,2>> tree(n+1);
    for(int v=1;v<=n;v++)std::cin >> tree[v][0] >> tree[v][1];
    QuantumComputer qc(tree,s);
    char op;
    int v;
    while(std::cin >> op >> v) {
        if(op=='P')qc.pebble(v);
        else if(op=='U')qc.unpebble(v);
        else { assert(op=='G'); qc.measure(v); }
    }
    check_final(qc);
    std::cout << qc.metrics().work() << ' ' << qc.metrics().peak << ' '
              << qc.metrics().depth << '\n';
}
