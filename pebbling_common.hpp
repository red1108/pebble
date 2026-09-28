#pragma once

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace pebbling {

constexpr int MAX_H = 20;

inline int nodes(int height) { return (1 << height) - 1; }

inline bool leaf(int vertex, int vertex_count) {
    assert(1 <= vertex && vertex <= vertex_count);
    return vertex > vertex_count / 2;
}

inline void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct Metrics {
    std::uint64_t place = 0, unpebble = 0;
    std::uint64_t ghost = 0, exorcise = 0;
    int live = 0, peak = 0;
    std::uint64_t work() const { return place + unpebble; }
};

// Both schedulers use this dependency order. Their release actions differ:
// sequential Fast chooses unpebble/ghost, while parallel Sweep ghosts children.
template<class Place, class Release>
void fast_traversal(int vertex, int vertex_count, Place& place, Release& release) {
    if (!leaf(vertex, vertex_count)) {
        fast_traversal(2 * vertex, vertex_count, place, release);
        fast_traversal(2 * vertex + 1, vertex_count, place, release);
    }
    place(vertex);
    if (!leaf(vertex, vertex_count)) {
        release(2 * vertex);
        release(2 * vertex + 1);
    }
}

}  // namespace pebbling
