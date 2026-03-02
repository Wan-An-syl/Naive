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

std::vector<RankedClique> RefinedIncrementalTopK::Run(const TemporalGraphSequence& sequence) {
  TopKCliqueQueue q(k_);
  if (sequence.Empty() || k_ == 0) {
    return q.SortDescending();
  }

  CliqueLifetimeMap lifetime;
  IterationWorkspace ws;

  ws.m_prev = FindMaximalCliques(sequence[0]);
  for (const auto& c : ws.m_prev) {
    lifetime[c] = 1;
    q.Push(RankedClique::Build(c, 1));
  }

  for (Timestamp t = 1; t < sequence.Size(); ++t) {
    const NodeDelta delta = GetNodeChange(sequence[t], sequence[t - 1]);

    ws.m_new = FindMaximalCliques(sequence[t]);
    ws.m_curr.clear();
    ws.p.clear();

    for (const auto& c_new : ws.m_new) {
      if (!delta.Empty()) {
        bool touches_delta = false;
        for (NodeId u : c_new.vertices) {
          if (delta.inserted_nodes.count(u) || delta.removed_nodes.count(u)) {
            touches_delta = true;
            break;
          }
        }
        if (!touches_delta) {
          // 对齐伪代码中“若 c_new 属于 P 则 continue”的剪枝思想：
          // 增量模式下优先处理受变化影响的团。
          continue;
        }
      }

      bool matched = false;
      for (const auto& c_prev : ws.m_prev) {
        if (c_new == c_prev) {
          lifetime[c_new] = lifetime[c_prev] + 1;
          ws.p.insert(c_new);
          matched = true;
          break;
        }

        if (c_prev.IsSubsetOf(c_new)) {
          lifetime[c_new] = 1;
          lifetime[c_prev] = lifetime[c_prev] + 1;
          ws.p.insert(c_new);
          ws.p.insert(c_prev);
          matched = true;
          break;
        }
      }

      if (!matched) {
        lifetime[c_new] = 1;
        ws.p.insert(c_new);
      }
    }

    ws.m_curr = ws.m_prev;
    for (const auto& c : ws.m_new) {
      ws.m_curr.insert(c);
    }

    for (const auto& c : ws.m_curr) {
      if (ws.p.find(c) == ws.p.end()) {
        lifetime[c] = lifetime[c] + 1;
        ws.p.insert(c);
      }

      q.Push(RankedClique::Build(c, lifetime[c]));
    }

    ws.m_prev = ws.m_curr;
  }

  return q.SortDescending();
}

}  // namespace mbes
