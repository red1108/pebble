#include "pebbling_common.hpp"

// First pebble/unpebble together, then ghost.
struct Round {
    vector<int> place, unpebble, ghost;
};
using Schedule = vector<Round>;
bool active[1<<max_h];

void claim(int x, vector<int>& used) {
    assert(!active[x]);
    active[x] = true;
    used.push_back(x);
}

void run_round(const Round& round) {
    assert(!round.place.empty() || !round.unpebble.empty() || !round.ghost.empty());
    vector<int> used;

    // All preconditions use the state at the start of the round.
    for(int x : round.place) {
        assert(child_ready(x) && !placed[x]);
        claim(x, used);
        if(!leaf(x)) {
            claim(2*x, used);
            claim(2*x+1, used);
        }
    }
    for(int x : round.unpebble) {
        assert(child_ready(x) && placed[x]);
        claim(x, used);
        if(!leaf(x)) {
            claim(2*x, used);
            claim(2*x+1, used);
        }
    }
    for(int x : used) active[x] = false;

    // U_t includes old pebbles, even those unpebbled in this round.
    int occupied = metrics.live+(int)round.place.size();
    assert(occupied <= s);
    metrics.peak = max(metrics.peak, occupied);
    metrics.area += occupied;
    metrics.depth++;

    for(int x : round.place) pebble(x);
    for(int x : round.unpebble) unpebble(x);
    for(int x : round.ghost) ghost(x);
}

void run(const Schedule& schedule) {
    for(const auto& round : schedule) run_round(round);
}

void add_level(vector<int>& out, int x, int d) {
    for(int v=x<<d; v<(x+1)<<d; v++) out.push_back(v);
}

void append(Schedule& out, Schedule next) {
    for(auto& round : next) out.push_back(std::move(round));
}

// Overlay schedules on disjoint subtrees, one round at a time.
void parallelize(Schedule& out, const Schedule& next) {
    out.resize(max(out.size(), next.size()));
    for(size_t t=0; t<next.size(); t++) {
        for(int x : next[t].place) out[t].place.push_back(x);
        for(int x : next[t].unpebble) out[t].unpebble.push_back(x);
        for(int x : next[t].ghost) out[t].ghost.push_back(x);
    }
}

void depth_first(int x, Schedule& out) {
    if(!leaf(x)) {
        depth_first(2*x, out);
        depth_first(2*x+1, out);
    }
    Round round;
    round.place = {x};
    if(!leaf(x)) round.ghost = {2*x, 2*x+1};
    out.push_back(std::move(round));
}

// Combine adjacent completed roots, like carries in a binary counter.
void carry(int x, vector<int>& roots, Schedule& out) {
    while(!roots.empty() && roots.back()%2==0 && roots.back()+1==x) {
        int left = roots.back();
        roots.pop_back();
        out.push_back({{x/2}, {}, {left, x}});
        x /= 2;
    }
    roots.push_back(x);
}

Schedule parallel_pebble(int x, int r, int b) {
    assert(r>=1 && b>=(r==1 ? 1 : r+1));
    Schedule out;
    if((1<<r)-1 <= b) {
        for(int d=r-1; d>=0; d--) {
            Round round;
            add_level(round.place, x, d);
            if(d<r-1) add_level(round.ghost, x, d+1);
            out.push_back(std::move(round));
        }
        return out;
    }
    if(b<16) {
        depth_first(x, out);
        return out;
    }

    vector<int> roots;
    if(b>=2*r) {
        int p = 1, ell = 1;
        while(2*p<=b/8) {
            p *= 2;
            ell++;
        }
        int d = r-ell, jobs = 1<<d;
        // Start one bottom subtree every two advancement rounds.
        for(int t=0; t<=2*(jobs-1)+ell-1; t++) {
            Round round;
            int completed = -1;
            int first = max(0, (t-ell+2)/2), last = min(jobs-1, t/2);
            for(int j=first; j<=last; j++) {
                int age = t-2*j, v = (x<<d)+j;
                add_level(round.place, v, ell-1-age);
                if(age>0) add_level(round.ghost, v, ell-age);
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
            else depth_first(v, out);
            carry(v, roots, out);
        }
    }
    assert(roots.size()==1 && roots[0]==x);
    return out;
}

int resolve_space(int r) {
    return min((1<<r)-1, (16*(1<<r)+r-1)/r); // p(r)
}

int choose_cut(int r, int b) {
    int k = 0;
    for(int i=1; i<r; i++) {
        int m = 1<<i, q = r-i;
        if(m+q<=b && 2*m-1<=b) k = i;
    }
    assert(k>0);
    return k;
}

Schedule contract(int x, int r, int k, int b) {
    int m = 1<<k, q = r-k;
    assert(q>=1 && m+q<=b && 2*m-1<=b);
    int batch_size, local_b;
    if(m*(q+1)<=b) {
        batch_size = m;
        local_b = b/m;
    } else {
        batch_size = (b-m+1)/(q+1);
        local_b = q+1;
    }
    assert(batch_size>0);

    Schedule out;
    for(int first=0; first<m; first+=batch_size) {
        Schedule batch;
        for(int j=first; j<min(m, first+batch_size); j++)
            parallelize(batch, parallel_pebble((x<<k)+j, q, local_b));
        append(out, std::move(batch));
    }
    for(int d=k-1; d>=0; d--) {
        Round round;
        add_level(round.place, x, d);
        out.push_back(std::move(round));
    }
    for(int d=0; d<k; d++) {
        Round round;
        add_level(round.unpebble, x, d);
        out.push_back(std::move(round));
    }
    Round release;
    if(q==1) add_level(release.unpebble, x, k);
    else add_level(release.ghost, x, k);
    out.push_back(std::move(release));
    return out;
}

Schedule clean_small(int x, int r) {
    Schedule out;
    for(int d=r-1; d>=0; d--) {
        Round round;
        add_level(round.place, x, d);
        out.push_back(std::move(round));
    }
    for(int d=0; d<r; d++) {
        Round round;
        add_level(round.unpebble, x, d);
        out.push_back(std::move(round));
    }
    return out;
}

void resolve(vector<int> forest, int r, int b) {
    assert(b >= resolve_space(r));
    while(true) {
        int local_b = resolve_space(r), batch_size = b/local_b;
        int q = r>16 ? ceil_log2(r)+2 : 0, k = r-q;
        vector<int> next;
        for(int first=0; first<(int)forest.size(); first+=batch_size) {
            Schedule batch;
            for(int i=first; i<min((int)forest.size(), first+batch_size); i++) {
                int x = forest[i];
                if(r<=16) parallelize(batch, clean_small(x, r));
                else {
                    parallelize(batch, contract(x, r, k, local_b));
                    add_level(next, x, k);
                }
            }
            run(batch);
        }
        if(r<=16) return;
        forest = std::move(next);
        r = q;
    }
}

void parallel_algorithm() {
    run(parallel_pebble(1, h, s));
    if(h==1) return;

    int b = s-1, r = h-1;
    vector<int> forest = {2, 3};
    while(resolve_space(r)>b) {
        int k = choose_cut(r, b);
        vector<int> next;
        for(int x : forest) {
            run(contract(x, r, k, b));
            add_level(next, x, k);
        }
        forest = std::move(next);
        r -= k;
    }
    resolve(std::move(forest), r, b);
}

int main(int argc, char** argv) {
    if(!read_input(argc, argv)) return 1;
    parallel_algorithm();
    check_final();

    cout << "Parallel Metrics: " << endl;
    print_metrics();
    cout << "depth: " << metrics.depth << endl;
    cout << "cumulative storage: " << metrics.area << endl;
    return 0;
}
