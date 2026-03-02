#pragma once  // 头文件只包含一次，防止重复定义。

#include <algorithm>      // std::sort / std::unique / std::includes。
#include <cstddef>        // std::size_t。
#include <cstdint>        // 固定宽度整数类型。
#include <functional>     // std::hash。
#include <queue>          // std::priority_queue。
#include <unordered_map>  // std::unordered_map。
#include <unordered_set>  // std::unordered_set。
#include <utility>        // std::move。
#include <vector>         // std::vector。

namespace mbes {  // 与算法模块对应的命名空间。

using NodeId = std::int32_t;    // 节点编号类型。
using Timestamp = std::size_t;  // 时间戳（快照下标）类型。

// ------------------------------
// 图快照与时间图序列
// ------------------------------

// 表示某一时刻 t 的无向图快照 G_t。
class GraphSnapshot {
 public:
  GraphSnapshot() = default;  // 默认构造。

  // 添加一个独立节点（若已存在则无副作用）。
  void AddNode(NodeId u) { adjacency_[u]; }

  // 添加一条无向边 (u, v)。
  void AddEdge(NodeId u, NodeId v) {
    if (u == v) {  // 忽略自环，保持简单图语义。
      return;
    }
    adjacency_[u].insert(v);  // u 的邻居加入 v。
    adjacency_[v].insert(u);  // v 的邻居加入 u。
  }

  // 判断节点是否存在。
  bool HasNode(NodeId u) const { return adjacency_.find(u) != adjacency_.end(); }

  // 判断无向边 (u, v) 是否存在。
  bool HasEdge(NodeId u, NodeId v) const {
    auto it = adjacency_.find(u);  // 先找到 u 的邻接表。
    if (it == adjacency_.end()) {  // u 不存在则边必不存在。
      return false;
    }
    return it->second.find(v) != it->second.end();  // 判断 v 是否在 u 的邻居中。
  }

  // 返回节点 u 的度（不存在则返回 0）。
  std::size_t Degree(NodeId u) const {
    auto it = adjacency_.find(u);   // 查找节点。
    if (it == adjacency_.end()) {   // 不存在。
      return 0;                     // 度为 0。
    }
    return it->second.size();       // 存在时，邻居数量即度。
  }

  // 返回所有节点（不保证顺序）。
  std::vector<NodeId> Nodes() const {
    std::vector<NodeId> nodes;                 // 输出容器。
    nodes.reserve(adjacency_.size());          // 预留空间，减少扩容。
    for (const auto& [u, _] : adjacency_) {    // 遍历所有键（节点）。
      nodes.push_back(u);                      // 收集节点编号。
    }
    return nodes;                              // 返回节点数组。
  }

 private:
  // 邻接表：node -> 邻居集合。
  std::unordered_map<NodeId, std::unordered_set<NodeId>> adjacency_;
};

// 表示时间图序列 G = {G0, G1, ..., Gn}。
struct TemporalGraphSequence {
  std::vector<GraphSnapshot> snapshots;  // 每个下标对应一个时间快照。

  std::size_t Size() const { return snapshots.size(); }     // 快照数量。
  bool Empty() const { return snapshots.empty(); }          // 是否为空。

  GraphSnapshot& operator[](Timestamp t) { return snapshots[t]; }              // 可写访问。
  const GraphSnapshot& operator[](Timestamp t) const { return snapshots[t]; }  // 只读访问。
};

// ------------------------------
// 团（clique）与集合索引
// ------------------------------

// 团的规范表示：顶点集合始终排序且去重。
struct Clique {
  std::vector<NodeId> vertices;  // 团内节点列表（规范化后有序、唯一）。

  Clique() = default;  // 默认构造空团。

  // 以节点列表构造团，并在构造时规范化。
  explicit Clique(std::vector<NodeId> nodes) : vertices(std::move(nodes)) {
    Normalize();  // 排序+去重，保证同一团的唯一表示。
  }

  // 规范化函数：排序并去重。
  void Normalize() {
    std::sort(vertices.begin(), vertices.end());                             // 升序排序。
    vertices.erase(std::unique(vertices.begin(), vertices.end()),            // 去重。
                   vertices.end());
  }

  std::size_t Size() const { return vertices.size(); }  // 团大小 |c|。
  bool Empty() const { return vertices.empty(); }       // 是否为空团。

  // 团相等：规范表示下，直接比较向量即可。
  bool operator==(const Clique& other) const { return vertices == other.vertices; }

  // 判断当前团是否是 other 的子集：c ⊆ other。
  bool IsSubsetOf(const Clique& other) const {
    return std::includes(other.vertices.begin(), other.vertices.end(),  // 大集合区间。
                         vertices.begin(), vertices.end());              // 小集合区间。
  }
};

// 团的哈希函数，使 Clique 可作为 unordered_* 的键。
struct CliqueHash {
  std::size_t operator()(const Clique& c) const {
    std::size_t seed = 0;  // 累计哈希种子。
    for (NodeId u : c.vertices) {
      // 标准 hash combine 形式，降低碰撞概率。
      seed ^= std::hash<NodeId>{}(u) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    return seed;  // 返回最终哈希值。
  }
};

using CliqueSet = std::unordered_set<Clique, CliqueHash>;           // 团集合类型。
using CliqueLifetimeMap = std::unordered_map<Clique, std::int32_t, CliqueHash>;  // l(c) 映射。

// 节点变化集 ΔV_t（相邻快照之间）。
struct NodeDelta {
  std::unordered_set<NodeId> inserted_nodes;  // 新增节点。
  std::unordered_set<NodeId> removed_nodes;   // 删除节点。

  bool Empty() const { return inserted_nodes.empty() && removed_nodes.empty(); }
};

// ------------------------------
// 评分函数与 top-k 队列
// ------------------------------

// 带评分的团：F(c) = l(c) * |c|。
struct RankedClique {
  Clique clique;             // 团 c。
  std::int32_t lifetime = 0; // l(c)。
  std::int32_t score = 0;    // F(c)。

  RankedClique() = default;  // 默认构造。

  // 用团和寿命构造评分对象。
  RankedClique(Clique c, std::int32_t l)
      : clique(std::move(c)),
        lifetime(l),
        score(static_cast<std::int32_t>(clique.Size()) * lifetime) {}

  // 便捷构造器，提升调用可读性。
  static RankedClique Build(Clique c, std::int32_t lifetime) {
    return RankedClique(std::move(c), lifetime);
  }
};

// 判断 lhs 是否比 rhs“更好”（用于 top-k 保留策略）。
inline bool IsBetterRank(const RankedClique& lhs, const RankedClique& rhs) {
  if (lhs.score != rhs.score) {                   // 先比较 F(c)。
    return lhs.score > rhs.score;                 // 分值高者更好。
  }
  if (lhs.clique.Size() != rhs.clique.Size()) {   // 再比较 |c|。
    return lhs.clique.Size() > rhs.clique.Size(); // 团更大者更好。
  }
  return lhs.clique.vertices < rhs.clique.vertices;  // 最后字典序稳定打破平局。
}

// 最小堆比较器：让堆顶成为当前 top-k 中“最差”的元素。
struct MinScoreCompare {
  bool operator()(const RankedClique& lhs, const RankedClique& rhs) const {
    return IsBetterRank(lhs, rhs);  // better 元素“优先级更低”，从而 top() 为 worst。
  }
};

// Q：容量为 k 的最小优先队列（维护 top-k）。
class TopKCliqueQueue {
 public:
  explicit TopKCliqueQueue(std::size_t capacity) : capacity_(capacity) {}

  std::size_t Size() const { return heap_.size(); }   // 当前元素数。
  bool Empty() const { return heap_.empty(); }        // 是否为空。
  std::size_t Capacity() const { return capacity_; }  // 队列容量 k。

  // 访问当前最小（最差）元素；调用方应先确保非空。
  const RankedClique& Min() const { return heap_.top(); }

  // 判断新元素是否应进入 top-k。
  bool ShouldInsert(const RankedClique& item) const {
    if (capacity_ == 0) {          // k=0 时任何元素都不保留。
      return false;
    }
    if (heap_.size() < capacity_) {  // 未满则直接可插入。
      return true;
    }
    return IsBetterRank(item, heap_.top());  // 满了时仅替换更好者。
  }

  // 插入一个候选团，并保持容量不超过 k。
  void Push(const RankedClique& item) {
    if (!ShouldInsert(item)) {  // 不满足进入条件直接返回。
      return;
    }
    heap_.push(item);           // 先入堆。
    if (heap_.size() > capacity_) {  // 超容时移除最差元素。
      heap_.pop();
    }
  }

  // 以 F(c) 递减顺序导出结果。
  std::vector<RankedClique> SortDescending() const {
    auto copy = heap_;                     // 拷贝堆，避免修改原结构。
    std::vector<RankedClique> result;      // 输出数组。
    result.reserve(copy.size());           // 预分配空间。
    while (!copy.empty()) {                // 逐个弹出。
      result.push_back(copy.top());
      copy.pop();
    }
    std::sort(result.begin(), result.end(),  // 按 better 规则降序排。
              [](const RankedClique& a, const RankedClique& b) {
                return IsBetterRank(a, b);
              });
    return result;  // 返回排序后的 top-k 列表。
  }

 private:
  std::size_t capacity_ = 0;  // 容量 k。
  std::priority_queue<RankedClique, std::vector<RankedClique>, MinScoreCompare> heap_;  // 最小堆。
};

// 每一轮 t 的工作区（与伪代码变量同名）。
struct IterationWorkspace {
  CliqueSet m_prev;  // 上一轮（或基础轮）最大团集合 M_prev。
  CliqueSet m_new;   // 当前增量发现集合 M_new。
  CliqueSet m_curr;  // 当前有效集合 M_curr。
  CliqueSet p;       // 标记集合 P。
};

}  // namespace mbes
