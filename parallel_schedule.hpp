#pragma once
#include "pebbling_common.hpp"

inline void add_level(vector<int>& out, int x, int d) {
    for(int v=x<<d; v<(x+1)<<d; v++) out.push_back(v);
}

inline int minimum_space(int r) {
    return r==1 ? 1 : r+1;
}

// A fixed recursive fallback: run both children together when their budgets fit.
inline Schedule recursive_pebble(int x, int r, int b) {
    assert(b>=minimum_space(r));
    if(r==1) return {{{x}, {}, {}}};
    Schedule out;
    if(b>=2*minimum_space(r-1)) {
        out = recursive_pebble(2*x, r-1, b/2);
        parallelize(out, recursive_pebble(2*x+1, r-1, b/2));
    } else {
        out = recursive_pebble(2*x, r-1, b);
        append(out, recursive_pebble(2*x+1, r-1, b-1));
    }
    out.push_back({{x}, {}, {2*x, 2*x+1}});
    return out;
}

// Combine adjacent completed roots, like carries in a binary counter.
inline void carry(int x, vector<int>& roots, Schedule& out) {
    while(!roots.empty() && roots.back()%2==0 && roots.back()+1==x) {
        int left = roots.back();
        roots.pop_back();
        out.push_back({{x/2}, {}, {left, x}});
        x /= 2;
    }
    roots.push_back(x);
}

inline Schedule parallel_pebble(int x, int r, int b) {
    assert(r>=1 && b>=(r==1 ? 1 : r+1));
    Schedule out;
    if(r==1 || 3*(1<<(r-2))<=b) {
        for(int d=r-1; d>=0; d--) {
            Round round;
            add_level(round.pebble, x, d);
            if(d<r-1) add_level(round.measure, x, d+1);
            out.push_back(std::move(round));
        }
        return out;
    }
    if(b<16) return recursive_pebble(x, r, b);

    vector<int> roots;
    if(b>=2*r) {
        int p = 1, ell = 1;
        // Largest dyadic width satisfying the pipeline's space bound.
        while(ell+1<r && 4*p+(r-ell-1)+2<=b) {
            p *= 2;
            ell++;
        }
        int d = r-ell, jobs = 1<<d;
        assert(2*p+d+2<=b);
        // Start one bottom subtree every two advancement rounds.
        for(int t=0; t<=2*(jobs-1)+ell-1; t++) {
            Round round;
            int completed = -1;
            int first = max(0, (t-ell+2)/2), last = min(jobs-1, t/2);
            for(int j=first; j<=last; j++) {
                int age = t-2*j, v = (x<<d)+j;
                add_level(round.pebble, v, ell-1-age);
                if(age>0) add_level(round.measure, v, ell-age);
                if(age==ell-1) completed = v;
            }
            out.push_back(std::move(round));
            // Unfinished jobs wait during these carry rounds.
            if(completed!=-1) carry(completed, roots, out);
        }
    } else {
        int ell = r/4, d = r-ell;
        for(int j=0; j<(1<<d); j++) {
            int v = (x<<d)+j, local_b = b-(int)roots.size();
            assert(local_b >= ell+1);
            if(local_b>=2*ell) append(out, parallel_pebble(v, ell, local_b));
            else append(out, recursive_pebble(v, ell, local_b));
            carry(v, roots, out);
        }
    }
    assert(roots.size()==1 && roots[0]==x);
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

inline void parallel_algorithm(QuantumComputer& qc) {
    int h = qc.height(), s = qc.space_limit();
    run(qc, parallel_pebble(1, h, s));
    if(h==1) return;

    int b = s-1, r = h-1;
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
