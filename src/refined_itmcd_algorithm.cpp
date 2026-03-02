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
  TopKCliqueQueue q(k_);
  if (sequence.Empty() || k_ == 0) {
    return q.SortDescending();
  }

  CliqueLifetimeMap lifetime;
  CliqueSet m_prev = FindMaximalCliques(sequence[0]);

  for (const auto& c : m_prev) {
    lifetime[c] = 1;
    q.Push(RankedClique::Build(c, 1));
  }

  for (Timestamp t = 1; t < sequence.Size(); ++t) {
    CliqueSet m_new = FindMaximalCliques(sequence[t]);
    CliqueSet m_curr;

    for (const auto& c_new : m_new) {
      const bool existed_last_step = m_prev.find(c_new) != m_prev.end();
      if (existed_last_step) {
        lifetime[c_new] = lifetime[c_new] + 1;
      } else {
        lifetime[c_new] = 1;
      }

      m_curr.insert(c_new);
      q.Push(RankedClique::Build(c_new, lifetime[c_new]));
    }

    m_prev = std::move(m_curr);
  }

  return q.SortDescending();
}

std::vector<RankedClique> RefinedIncrementalTopK::Run(const TemporalEdgeIntervalGraph& graph,
                                                      Timestamp begin,
                                                      Timestamp end) {
  return Run(BuildTemporalGraphSequence(graph, begin, end));
}

}  // namespace mbes
