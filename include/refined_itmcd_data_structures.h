#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mbes {

using NodeId = std::int32_t;
using Timestamp = std::size_t;

class GraphSnapshot {
 public:
  GraphSnapshot() = default;

  void AddNode(NodeId u) { adjacency_[u]; }

  void AddEdge(NodeId u, NodeId v) {
    if (u == v) {
      return;
    }
    adjacency_[u].insert(v);
    adjacency_[v].insert(u);
  }

  void RemoveEdge(NodeId u, NodeId v) {
    auto it_u = adjacency_.find(u);
    if (it_u != adjacency_.end()) {
      it_u->second.erase(v);
    }
    auto it_v = adjacency_.find(v);
    if (it_v != adjacency_.end()) {
      it_v->second.erase(u);
    }
  }

  void RemoveNode(NodeId u) {
    auto it = adjacency_.find(u);
    if (it == adjacency_.end()) {
      return;
    }
    for (NodeId v : it->second) {
      auto neighbor = adjacency_.find(v);
      if (neighbor != adjacency_.end()) {
        neighbor->second.erase(u);
      }
    }
    adjacency_.erase(it);
  }

  bool HasNode(NodeId u) const { return adjacency_.find(u) != adjacency_.end(); }

  bool HasEdge(NodeId u, NodeId v) const {
    auto it = adjacency_.find(u);
    if (it == adjacency_.end()) {
      return false;
    }
    return it->second.find(v) != it->second.end();
  }

  const std::unordered_set<NodeId>& Neighbors(NodeId u) const {
    static const std::unordered_set<NodeId> kEmpty;
    auto it = adjacency_.find(u);
    return it == adjacency_.end() ? kEmpty : it->second;
  }

  std::size_t Degree(NodeId u) const {
    auto it = adjacency_.find(u);
    if (it == adjacency_.end()) {
      return 0;
    }
    return it->second.size();
  }

  std::size_t NodeCount() const { return adjacency_.size(); }

  std::vector<NodeId> Nodes() const {
    std::vector<NodeId> nodes;
    nodes.reserve(adjacency_.size());
    for (const auto& [u, _] : adjacency_) {
      nodes.push_back(u);
    }
    return nodes;
  }

 private:
  std::unordered_map<NodeId, std::unordered_set<NodeId>> adjacency_;
};

struct TemporalGraphSequence {
  std::vector<GraphSnapshot> snapshots;

  std::size_t Size() const { return snapshots.size(); }
  bool Empty() const { return snapshots.empty(); }

  GraphSnapshot& operator[](Timestamp t) { return snapshots[t]; }
  const GraphSnapshot& operator[](Timestamp t) const { return snapshots[t]; }
};

struct Clique {
  std::vector<NodeId> vertices;

  Clique() = default;

  explicit Clique(std::vector<NodeId> nodes) : vertices(std::move(nodes)) { Normalize(); }

  void Normalize() {
    std::sort(vertices.begin(), vertices.end());
    vertices.erase(std::unique(vertices.begin(), vertices.end()), vertices.end());
  }

  std::size_t Size() const { return vertices.size(); }
  bool Empty() const { return vertices.empty(); }

  bool operator==(const Clique& other) const { return vertices == other.vertices; }

  bool IsSubsetOf(const Clique& other) const {
    return std::includes(other.vertices.begin(), other.vertices.end(),
                         vertices.begin(), vertices.end());
  }
};

struct CliqueHash {
  std::size_t operator()(const Clique& c) const {
    std::size_t seed = 0;
    for (NodeId u : c.vertices) {
      seed ^= std::hash<NodeId>{}(u) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    return seed;
  }
};

using CliqueSet = std::unordered_set<Clique, CliqueHash>;
using CliqueLifetimeMap = std::unordered_map<Clique, std::int32_t, CliqueHash>;

struct NodeDelta {
  std::unordered_set<NodeId> inserted_nodes;
  std::unordered_set<NodeId> removed_nodes;

  bool Empty() const { return inserted_nodes.empty() && removed_nodes.empty(); }
};

struct RankedClique {
  Clique clique;
  std::int32_t lifetime = 0;
  std::int32_t score = 0;

  RankedClique() = default;

  RankedClique(Clique c, std::int32_t l)
      : clique(std::move(c)), lifetime(l), score(static_cast<std::int32_t>(clique.Size()) * lifetime) {}

  static RankedClique Build(Clique c, std::int32_t lifetime) {
    return RankedClique(std::move(c), lifetime);
  }
};

inline bool IsBetterRank(const RankedClique& lhs, const RankedClique& rhs) {
  if (lhs.score != rhs.score) {
    return lhs.score > rhs.score;
  }
  if (lhs.clique.Size() != rhs.clique.Size()) {
    return lhs.clique.Size() > rhs.clique.Size();
  }
  return lhs.clique.vertices < rhs.clique.vertices;
}

struct MinScoreCompare {
  bool operator()(const RankedClique& lhs, const RankedClique& rhs) const {
    return IsBetterRank(lhs, rhs);
  }
};

class TopKCliqueQueue {
 public:
  explicit TopKCliqueQueue(std::size_t capacity) : capacity_(capacity) {}

  std::size_t Size() const { return heap_.size(); }
  bool Empty() const { return heap_.empty(); }
  std::size_t Capacity() const { return capacity_; }

  const RankedClique& Min() const { return heap_.top(); }

  bool ShouldInsert(const RankedClique& item) const {
    if (capacity_ == 0) {
      return false;
    }
    if (heap_.size() < capacity_) {
      return true;
    }
    return IsBetterRank(item, heap_.top());
  }

  void Push(const RankedClique& item) {
    if (!ShouldInsert(item)) {
      return;
    }
    heap_.push(item);
    if (heap_.size() > capacity_) {
      heap_.pop();
    }
  }

  std::vector<RankedClique> SortDescending() const {
    auto copy = heap_;
    std::vector<RankedClique> result;
    result.reserve(copy.size());
    while (!copy.empty()) {
      result.push_back(copy.top());
      copy.pop();
    }
    std::sort(result.begin(), result.end(), [](const RankedClique& a, const RankedClique& b) {
      return IsBetterRank(a, b);
    });
    return result;
  }

 private:
  std::size_t capacity_ = 0;
  std::priority_queue<RankedClique, std::vector<RankedClique>, MinScoreCompare> heap_;
};

struct IterationWorkspace {
  CliqueSet m_prev;
  CliqueSet m_new;
  CliqueSet m_curr;
  CliqueSet p;
};

}  // namespace mbes
