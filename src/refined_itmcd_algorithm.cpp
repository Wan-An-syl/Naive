#include "refined_itmcd_algorithm.h"

#include <algorithm>

namespace mbes {

namespace {

std::unordered_set<NodeId> Intersect(const std::unordered_set<NodeId>& a,
                                     const std::unordered_set<NodeId>& b) {
  const auto* small = &a;
  const auto* large = &b;
  if (small->size() > large->size()) {
    std::swap(small, large);
  }

  std::unordered_set<NodeId> out;
  for (NodeId u : *small) {
    if (large->find(u) != large->end()) {
      out.insert(u);
    }
  }
  return out;
}

bool TouchesDelta(const Clique& clique, const NodeDelta& delta) {
  for (NodeId u : clique.vertices) {
    if (delta.inserted_nodes.count(u) || delta.removed_nodes.count(u)) {
      return true;
    }
  }
  return false;
}

bool IsCliqueActiveInSnapshot(const Clique& clique, const GraphSnapshot& snapshot) {
  const auto& nodes = clique.vertices;
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    for (std::size_t j = i + 1; j < nodes.size(); ++j) {
      if (!snapshot.HasEdge(nodes[i], nodes[j])) {
        return false;
      }
    }
  }
  return true;
}

CliqueSet Invalidate(const CliqueSet& m_prev, const GraphSnapshot& snapshot, const NodeDelta& delta) {
  CliqueSet invalidated;
  for (const auto& c_prev : m_prev) {
    if (TouchesDelta(c_prev, delta) || !IsCliqueActiveInSnapshot(c_prev, snapshot)) {
      invalidated.insert(c_prev);
    }
  }
  return invalidated;
}

void BronKerboschPivot(const GraphSnapshot& graph,
                       std::unordered_set<NodeId> r,
                       std::unordered_set<NodeId> p,
                       std::unordered_set<NodeId> x,
                       CliqueSet* cliques) {
  if (p.empty() && x.empty()) {
    std::vector<NodeId> nodes(r.begin(), r.end());
    cliques->insert(Clique(std::move(nodes)));
    return;
  }

  NodeId pivot = 0;
  bool has_pivot = false;
  std::size_t best_degree = 0;
  std::unordered_set<NodeId> union_px = p;
  union_px.insert(x.begin(), x.end());
  for (NodeId u : union_px) {
    std::size_t deg = graph.Degree(u);
    if (!has_pivot || deg > best_degree) {
      pivot = u;
      best_degree = deg;
      has_pivot = true;
    }
  }

  std::unordered_set<NodeId> candidates;
  if (!has_pivot) {
    candidates = p;
  } else {
    const auto& neigh = graph.Neighbors(pivot);
    for (NodeId u : p) {
      if (neigh.find(u) == neigh.end()) {
        candidates.insert(u);
      }
    }
  }

  std::vector<NodeId> ordered(candidates.begin(), candidates.end());
  for (NodeId v : ordered) {
    auto r_next = r;
    r_next.insert(v);

    auto p_next = Intersect(p, graph.Neighbors(v));
    auto x_next = Intersect(x, graph.Neighbors(v));

    BronKerboschPivot(graph, std::move(r_next), std::move(p_next), std::move(x_next), cliques);

    p.erase(v);
    x.insert(v);
  }
}

}  // namespace

NodeDelta GetNodeChange(const GraphSnapshot& curr, const GraphSnapshot& prev) {
  NodeDelta delta;

  const auto curr_nodes = curr.Nodes();
  const auto prev_nodes = prev.Nodes();

  std::unordered_set<NodeId> curr_set(curr_nodes.begin(), curr_nodes.end());
  std::unordered_set<NodeId> prev_set(prev_nodes.begin(), prev_nodes.end());

  for (NodeId u : curr_set) {
    if (prev_set.find(u) == prev_set.end()) {
      delta.inserted_nodes.insert(u);
    }
  }

  for (NodeId u : prev_set) {
    if (curr_set.find(u) == curr_set.end()) {
      delta.removed_nodes.insert(u);
    }
  }

  return delta;
}

CliqueSet FindMaximalCliques(const GraphSnapshot& graph) {
  CliqueSet out;
  std::unordered_set<NodeId> r;
  std::unordered_set<NodeId> p;
  std::unordered_set<NodeId> x;

  for (NodeId u : graph.Nodes()) {
    p.insert(u);
  }

  BronKerboschPivot(graph, std::move(r), std::move(p), std::move(x), &out);
  return out;
}

TemporalGraphSequence BuildTemporalGraphSequence(const TemporalEdgeIntervalGraph& graph,
                                                 Timestamp begin,
                                                 Timestamp end) {
  return graph.BuildSequence(begin, end);
}

std::vector<RankedClique> RefinedIncrementalTopK::Run(const TemporalGraphSequence& sequence) {
  // Pseudocode line 1: Q <- Min-Priority Queue of capacity k
  TopKCliqueQueue q(k_);
  if (sequence.Empty() || k_ == 0) {
    return q.SortDescending();
  }

  CliqueLifetimeMap lifetime;

  // Pseudocode line 2: M_prev <- RMCE(G0)
  CliqueSet m_prev = FindMaximalCliques(sequence[0]);

  // Pseudocode line 3: initialize l(c), push first snapshot candidates
  for (const auto& c : m_prev) {
    lifetime[c] = 1;
    q.Push(RankedClique::Build(c, 1));
  }

  // Pseudocode line 4: for t <- 1 to n do
  for (Timestamp t = 1; t < sequence.Size(); ++t) {
    // Line 5
    const NodeDelta delta = GetNodeChange(sequence[t], sequence[t - 1]);
    // Line 6
    const CliqueSet m_new = FindMaximalCliques(sequence[t]);
    const CliqueSet invalidated = Invalidate(m_prev, sequence[t], delta);

    // Line 7
    CliqueSet m_curr;
    for (const auto& c_prev : m_prev) {
      if (!invalidated.count(c_prev)) {
        m_curr.insert(c_prev);
      }
    }
    for (const auto& c_new : m_new) {
      m_curr.insert(c_new);
    }

    CliqueSet p;

    // Line 8-16
    for (const auto& c_new : m_new) {
      if (p.count(c_new)) {
        continue;
      }

      bool matched = false;
      for (const auto& c_prev : m_prev) {
        if (c_new == c_prev) {
          lifetime[c_new] = lifetime[c_prev] + 1;
          p.insert(c_new);
          matched = true;
          break;
        }

        if (c_prev.IsSubsetOf(c_new)) {
          lifetime[c_new] = 1;
          lifetime[c_prev] = lifetime[c_prev] + 1;
          p.insert(c_new);
          p.insert(c_prev);
          matched = true;
          break;
        }
      }

      if (!matched) {
        lifetime[c_new] = 1;
        p.insert(c_new);
      }
    }

    // Line 17-21
    for (const auto& c : m_curr) {
      if (!p.count(c)) {
        lifetime[c] = lifetime[c] + 1;
        p.insert(c);
      }
      q.Push(RankedClique::Build(c, lifetime[c]));
    }

    // Line 22
    m_prev = std::move(m_curr);
  }

  // Line 23
  return q.SortDescending();
}

std::vector<RankedClique> RefinedIncrementalTopK::Run(const TemporalEdgeIntervalGraph& graph,
                                                      Timestamp begin,
                                                      Timestamp end) {
  return Run(BuildTemporalGraphSequence(graph, begin, end));
}

}  // namespace mbes
