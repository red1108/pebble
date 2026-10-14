#pragma once
#include "pebbling_common.hpp"

// Move independent actions from the next round into the current round.
// Measurements stay with the operation that uses or produces their value.
class RoundPacker {
public:
    explicit RoundPacker(QuantumComputer& computer)
        : qc(computer), active(qc.size()+1), measured(qc.size()+1),
          action(qc.size()+1), owner(qc.size()+1, -1) {}

    void push(const Round& next) {
        vector<Round> groups;
        vector<int> used;
        auto make_group = [&](int x, bool remove) {
            int id = (int)groups.size();
            groups.push_back({});
            if(remove) groups.back().unpebble.push_back(x);
            else groups.back().pebble.push_back(x);
            auto mark = [&](int v) { owner[v] = id; used.push_back(v); };
            mark(x);
            if(!qc.leaf(x)) {
                auto [left, right] = qc.children(x);
                mark(left);
                mark(right);
            }
        };
        for(int x : next.pebble) make_group(x, false);
        for(int x : next.unpebble) make_group(x, true);
        for(int x : next.measure) {
            if(owner[x]>=0) groups[owner[x]].measure.push_back(x);
            else groups.push_back({{}, {}, {x}});
        }
        for(int x : used) owner[x] = -1;

        vector<int> remaining;
        for(int i=0; i<(int)groups.size(); i++) {
            if(fits(groups[i])) add(groups[i]);
            else remaining.push_back(i);
        }
        if(!remaining.empty()) {
            flush();
            for(int i : remaining) {
                assert(fits(groups[i]));
                add(groups[i]);
            }
        }
    }

    void flush() {
        if(pending.pebble.empty() && pending.unpebble.empty() && pending.measure.empty()) return;
        qc.execute(pending);
        for(int x : touched) active[x] = false;
        for(int x : pending.pebble) action[x] = 0;
        for(int x : pending.unpebble) action[x] = 0;
        for(int x : pending.measure) measured[x] = false;
        pending = {};
        touched.clear();
    }

private:
    QuantumComputer& qc;
    Round pending;
    vector<bool> active, measured;
    vector<unsigned char> action;
    vector<int> owner, touched;

    bool fits(const Round& group) const {
        using State = QuantumComputer::State;
        if(qc.metrics().occupied+(int)(pending.pebble.size()+group.pebble.size())>qc.space_limit())
            return false;
        auto ready = [&](int x, bool remove) {
            if(active[x] || measured[x] || (qc.state(x)==State::pebble)!=remove) return false;
            if(!qc.leaf(x)) {
                for(int child : qc.children(x))
                    if(active[child] || measured[child] || qc.state(child)!=State::pebble) return false;
            }
            return true;
        };
        for(int x : group.pebble) if(!ready(x, false)) return false;
        for(int x : group.unpebble) if(!ready(x, true)) return false;
        for(int x : group.measure) {
            if(measured[x] || action[x]==2) return false;
            bool placed = !group.pebble.empty() && group.pebble[0]==x;
            if(qc.state(x)!=State::pebble && action[x]!=1 && !placed) return false;
        }
        return true;
    }

    void add(const Round& group) {
        auto mark = [&](int x, unsigned char op) {
            action[x] = op;
            active[x] = true;
            touched.push_back(x);
            if(!qc.leaf(x)) {
                for(int child : qc.children(x)) {
                    active[child] = true;
                    touched.push_back(child);
                }
            }
        };
        for(int x : group.pebble) { mark(x, 1); pending.pebble.push_back(x); }
        for(int x : group.unpebble) { mark(x, 2); pending.unpebble.push_back(x); }
        for(int x : group.measure) { measured[x] = true; pending.measure.push_back(x); }
    }
};

inline void run_packed(QuantumComputer& qc, const Schedule& schedule) {
    RoundPacker packer(qc);
    for(const auto& round : schedule) packer.push(round);
    packer.flush();
}
