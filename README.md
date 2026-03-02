# Naive Refined ITMCD (C++)

这是一个围绕图片中“Refined Incremental Top-k Temporal Maximal Clique Discovery”算法构建的最小可运行项目。

## 项目结构

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
