#pragma once
#include "pebbling_common.hpp"
#include <set>

inline void add_level(vector<int>& out, int x, int d) {
    for(int v=x<<d; v<(x+1)<<d; v++) out.push_back(v);
}

inline int minimum_space(int r) {
    return r==1 ? 1 : r+1;
}

// Prepare the root by merging ready siblings and reading leaves from the left.
inline Schedule parallel_pebble(int x, int h, int b) {
    assert(h>=1 && b>=minimum_space(h));
    if(h==1) return {{{x}, {}, {}}};

    int next = 1<<(h-1), end = 1<<h, occupied = 0;
    vector<bool> placed(end); // local heap indices; initial ghosts need no space
    set<pair<int, int>> ready; // leftmost leaf below the parent, parent index
    Schedule out;
    auto tree_index = [&](int v) {
        int d = 31-__builtin_clz(v);
        return (x<<d)+v-(1<<d);
    };

    while(!placed[1]) {
        int free = b-occupied;
        int count = min((int)ready.size(), free);
        vector<int> parents;
        for(int i=0; i<count; i++) {
            parents.push_back(ready.begin()->second);
            ready.erase(ready.begin());
        }

        // Keep space for the parents of the newly read leaves.
        int leaves = min({end-next, free-count, 2*(free+count)/3});
        vector<int> added = parents;
        for(int i=0; i<leaves; i++) added.push_back(next++);
        assert(!added.empty() && occupied+(int)added.size()<=b);

        Round round;
        for(int v : added) {
            placed[v] = true;
            round.pebble.push_back(tree_index(v));
        }
        for(int v : parents) {
            placed[2*v] = placed[2*v+1] = false;
            round.measure.push_back(tree_index(2*v));
            round.measure.push_back(tree_index(2*v+1));
        }
        for(int v : added) {
            if(v==1 || !placed[v^1]) continue;
            int p = v/2, d = 31-__builtin_clz(p);
            ready.insert({p<<(h-1-d), p});
        }
        occupied += leaves-count;
        out.push_back(std::move(round));
    }
    assert(occupied==1 && next==end);
    return out;
}

inline int stage_space(int r) {
    return r==1 ? 1 : max(r+1, ((1<<r)+r-1)/r); // p(r)
}

inline int choose_cut(int r, int b) {
    int k = 0;
    for(int i=1; i<r; i++) {
        int m = 1<<i, q = r-i;
        if(m+q<=b && 2*m-1<=b) k = i;
    }
    assert(k>0);
    return k;
}

inline Schedule contract(int x, int r, int k, int b) {
    int m = 1<<k, q = r-k;
    assert(q>=1 && m+q<=b && 2*m-1<=b);
    Schedule out;
    for(int first=0; first<m;) {
        int batch_size = min(m-first, (b-first)/minimum_space(q));
        int local_b = (b-first)/batch_size;
        Schedule batch;
        for(int j=first; j<min(m, first+batch_size); j++)
            parallelize(batch, parallel_pebble((x<<k)+j, q, local_b));
        append(out, std::move(batch));
        first += batch_size;
    }
    for(int d=k-1; d>=0; d--) {
        Round round;
        add_level(round.pebble, x, d);
        out.push_back(std::move(round));
    }
    for(int d=0; d<k; d++) {
        Round round;
        add_level(round.unpebble, x, d);
        out.push_back(std::move(round));
    }
    Round release;
    if(q==1) add_level(release.unpebble, x, k);
    else add_level(release.measure, x, k);
    out.push_back(std::move(release));
    return out;
}

inline Schedule clean_small(int x, int r) {
    Schedule out;
    for(int d=r-1; d>=0; d--) {
        Round round;
        add_level(round.pebble, x, d);
        out.push_back(std::move(round));
    }
    for(int d=0; d<r; d++) {
        Round round;
        add_level(round.unpebble, x, d);
        out.push_back(std::move(round));
    }
    return out;
}

template<class Computer>
inline void parallel_cleanup(Computer& qc, int b) {
    int h = qc.height();
    if(h==1) return;

    int r = h-1;
    vector<int> forest = {2, 3};
    while(true) {
        int batch_size = min((int)forest.size(), max(1, b/stage_space(r)));
        int local_b = b/batch_size;
        bool terminal = (1<<r)-1<=local_b;
        int k = terminal ? 0 : choose_cut(r, local_b);
        vector<int> next;
        for(int first=0; first<(int)forest.size(); first+=batch_size) {
            Schedule batch;
            for(int i=first; i<min((int)forest.size(), first+batch_size); i++) {
                int x = forest[i];
                if(terminal) parallelize(batch, clean_small(x, r));
                else {
                    parallelize(batch, contract(x, r, k, local_b));
                    add_level(next, x, k);
                }
            }
            run(qc, batch);
        }
        if(terminal) return;
        forest = std::move(next);
        r -= k;
    }
}

template<class Computer>
inline void parallel_algorithm(Computer& qc) {
    run(qc, parallel_pebble(1, qc.height(), qc.space_limit()));
    parallel_cleanup(qc, qc.space_limit()-1);
}
