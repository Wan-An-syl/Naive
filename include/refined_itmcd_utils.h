#pragma once

#include <algorithm>
#include <cstdint>
#include <queue>
#include <vector>

#include "refined_itmcd_data_structures.h"

namespace mbes {

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

// 工具类：维护容量为 k 的 top-k 最小优先队列。
class TopKCliqueQueue {
 public:
  explicit TopKCliqueQueue(std::size_t capacity) : capacity_(capacity) {}

  std::size_t Size() const { return heap_.size(); }
  bool Empty() const { return heap_.empty(); }
  std::size_t Capacity() const { return capacity_; }

  const RankedClique& Min() const { return heap_.top(); }

  bool Contains(const Clique& clique) const { return in_queue_.find(clique) != in_queue_.end(); }

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
    if (Contains(item.clique)) {
      return;
    }
    if (!ShouldInsert(item)) {
      return;
    }
    heap_.push(item);
    in_queue_.insert(item.clique);
    if (heap_.size() > capacity_) {
      in_queue_.erase(heap_.top().clique);
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
  CliqueSet in_queue_;
};

struct IterationWorkspace {
  CliqueSet m_prev;
  CliqueSet m_new;
  CliqueSet m_curr;
  CliqueSet p;
};

}  // namespace mbes
