#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>
using namespace std;

class QuantumComputer {
public:
    static constexpr int max_height = 20;
    enum class State { empty, pebble, ghost };

    struct Metrics {
        uint64_t place = 0, unpebble = 0;
        uint64_t measure = 0, correction = 0;
        uint64_t depth = 0, area = 0;
        int occupied = 0, peak = 0;

        uint64_t work() const { return place + unpebble; }
    };

    struct Round {
        vector<int> pebble, unpebble; // first phase
        vector<int> measure;         // second phase
    };

    QuantumComputer(int height, int space_limit)
        : height_(height), space_limit_(space_limit) {
        if(height<1 || height>max_height)
            throw invalid_argument("height must be in [1, 20]");
        n_ = (1<<height)-1;
        if(space_limit<(height==1 ? 1 : height+1))
            throw invalid_argument("insufficient space for the computation");
        if(space_limit>n_)
            throw invalid_argument("too much space for the computation");
        state_.assign(n_+1, State::empty);
        active_.assign(n_+1, false);
        action_.assign(n_+1, 0);
    }

    int height() const { return height_; }
    int size() const { return n_; }
    int space_limit() const { return space_limit_; }
    const Metrics& metrics() const { return metrics_; }

    State state(int ind) const {
        assert(1<=ind && ind<=n_);
        return state_[ind];
    }

    bool leaf(int ind) const {
        assert(1<=ind && ind<=n_);
        return ind > n_/2;
    }

    void pebble(int ind) { execute({{ind}, {}, {}}); }
    void unpebble(int ind) { execute({{}, {ind}, {}}); }
    void measure(int ind) { execute({{}, {}, {ind}}); }
    void correction(int ind) {
        assert(state(ind)==State::ghost);
        pebble(ind);
    }

    void pebble(const vector<int>& inds) { execute({inds, {}, {}}); }
    void unpebble(const vector<int>& inds) { execute({{}, inds, {}}); }
    void measure(const vector<int>& inds) { execute({{}, {}, inds}); }

    // Recompute and correct a ghost, leaving its value pebbled.
    void correction(const vector<int>& inds) {
        for(int ind : inds) assert(state(ind)==State::ghost);
        pebble(inds);
    }

    void execute(const Round& round) {
        validate(round);

        // U_t includes old pebbles, even those unpebbled in this round.
        int occupied = metrics_.occupied+(int)round.pebble.size();
        metrics_.peak = max(metrics_.peak, occupied);
        metrics_.area += occupied;
        metrics_.depth++;

        for(int ind : round.pebble) {
            if(state_[ind]==State::ghost) metrics_.correction++;
            state_[ind] = State::pebble;
            metrics_.place++;
            metrics_.occupied++;
        }
        for(int ind : round.unpebble) {
            state_[ind] = State::empty;
            metrics_.unpebble++;
            metrics_.occupied--;
        }
        for(int ind : round.measure) {
            state_[ind] = State::ghost;
            metrics_.measure++;
            metrics_.occupied--;
        }
    }

    bool finished() const {
        if(state_[1]!=State::pebble || metrics_.occupied!=1) return false;
        for(int ind=2; ind<=n_; ind++)
            if(state_[ind]!=State::empty) return false;
        return true;
    }

private:
    vector<State> state_;
    Metrics metrics_;
    int height_, space_limit_, n_;
    vector<bool> active_;
    vector<unsigned char> action_; // temporary marks: 1 pebble, 2 unpebble, 3 measure

    bool child_ready(int ind) const {
        return leaf(ind) || (state_[2*ind]==State::pebble && state_[2*ind+1]==State::pebble);
    }

    void validate(const Round& round) {
        assert(!round.pebble.empty() || !round.unpebble.empty() || !round.measure.empty());
        vector<int> used;
        used.reserve(3*(round.pebble.size()+round.unpebble.size()));
        auto claim = [&](int ind) {
            assert(!active_[ind]);
            active_[ind] = true;
            used.push_back(ind);
        };
        auto claim_inputs = [&](int ind) {
            claim(ind);
            if(!leaf(ind)) {
                claim(2*ind);
                claim(2*ind+1);
            }
        };

        // All first-phase preconditions use the same initial state.
        for(int ind : round.pebble) {
            assert(state(ind)!=State::pebble && child_ready(ind));
            claim_inputs(ind);
            action_[ind] = 1;
        }
        for(int ind : round.unpebble) {
            assert(state(ind)==State::pebble && child_ready(ind));
            claim_inputs(ind);
            action_[ind] = 2;
        }
        for(int ind : round.measure) {
            State current = state(ind);
            assert(action_[ind]!=2 && action_[ind]!=3);
            assert(current==State::pebble || action_[ind]==1);
            action_[ind] = 3;
        }
        assert(metrics_.occupied+(int)round.pebble.size() <= space_limit_);

        for(int ind : used) active_[ind] = false;
        for(int ind : round.pebble) action_[ind] = 0;
        for(int ind : round.unpebble) action_[ind] = 0;
        for(int ind : round.measure) action_[ind] = 0;
    }
};
