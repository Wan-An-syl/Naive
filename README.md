# Naive Refined ITMCD (C++)

这是一个围绕图片中“Refined Incremental Top-k Temporal Maximal Clique Discovery”算法构建的最小可运行项目。

## 项目结构

- `include/refined_itmcd_data_structures.h`：算法数据结构层（含 `TemporalGraph` 时序图结构体、`Clique` 团结构体）
- `include/refined_itmcd_data_structures.h`：算法数据结构层
- `include/refined_itmcd_algorithm.h`：算法接口
- `src/refined_itmcd_algorithm.cpp`：算法实现（含最大团搜索与增量 top-k 主流程）
- `src/main.cpp`：示例程序
- `tests/test_refined_itmcd.cpp`：基础自检

## 构建与运行

```bash
cmake -S . -B build
cmake --build build
./build/refined_itmcd_demo
./build/test_refined_itmcd
```

## 额外说明

- 支持边时间区间模型：同一条边可拥有多个不连续活跃区间（例如 `[0,1]` 与 `[4,6]`）。
- 时间区间保存在边上（`TemporalEdgeIntervalGraph`），并可按时间窗口展开为 `TemporalGraphSequence`。

- 团的生命周期按“连续时间”累计：若某团在中间时刻消失，再出现时会从 1 重新计数。


## 伪代码行号映射清单

- 行 1：`TopKCliqueQueue q(k_)` 初始化最小优先队列。  
- 行 2：`m_prev = FindMaximalCliques(sequence[0])`。  
- 行 3：初始化 `l(c)=1` 并推入 `Q`。  
- 行 4：`for t in [1, n]` 主循环。  
- 行 5：`GetNodeChange` 生成 `ΔV_t`。  
- 行 6：`m_new` + `invalidated`（基于 `ΔV_t` 与当前快照有效性）。  
- 行 7：`m_curr = (m_prev \ invalidated) ∪ m_new`，并初始化 `P`。  
- 行 8-16：逐个 `c_new` 做匹配分支：  
  - `c_new == c_prev`；  
  - `c_prev ⊆ c_new`；  
  - 未匹配分支。  
- 行 17-21：遍历 `m_curr`，未在 `P` 中的团做 `l(c)+=1`，并按 `F(c)=l(c)*|c|` 更新 `Q`。  
- 行 22：`m_prev = m_curr`。  
- 行 23：返回 `SortDescending(Q)`。

- 数据结构层显式定义了 `TemporalGraph`（时序图结构体）与 `Clique`（团结构体），便于直接与伪代码对象对应。
