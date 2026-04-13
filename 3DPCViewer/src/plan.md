# src 代码可读性与 Qt 信号槽规范优化计划

## 问题与目标
- **问题**：`src` 下 Qt 信号槽链路相关代码（`ui/core/io` 的 QObject 类）存在可读性与一致性问题，重点体现在 `connect` 写法风格不统一、信号/槽命名规则混用、局部命名风格不一致。
- **目标**：在不改变业务行为的前提下，统一信号/槽命名约定与连接写法，提高代码可读性、可维护性和后续重构安全性。

## 当前状态（基于现有代码扫描）
1. `connect` 已普遍使用新语法，但存在大量“仅转发信号”的 lambda，可直接改为 signal-to-signal 连接以提升简洁性（如 `ControlPanelWidget.cpp`、`PCViewer.cpp`）。
2. 命名不统一：
   - `errorOccur` 与 `errorOccurred` 并存（`BagWorker/DataService/PCViewer` vs `DatabaseManager`）。
   - 信号中出现 `on...` 前缀（`ControlPanelWidget::onImageFrameReady`），与槽命名语义混淆。
   - `requestProcBag` 与 `requestProcessBag` 语义近似但缩写不统一（跨 `ui/core`）。
3. 代码风格不一致：`src/ui`、`src/core` 中存在大量 2 空格缩进，但仓库 `.clang-format` 约定为 4 空格和 80 列限制。

## 实施策略
- 先定义并固化“命名与 connect 风格规范”，再做批量重命名与连接语句整理，最后执行格式化与构建验证。
- 重命名采用“跨文件一次性联动”方式，避免局部修改导致信号/槽断连。
- 对外行为保持不变，仅调整可读性与一致性。

## 任务拆分（执行顺序）

### 任务 1：建立规范与改造清单
- **范围文件**：
  - `src/ui/*.h`, `src/core/*.h`, `src/io/*.h`
  - `.clang-format`
- **输出**：
  - 统一规则：
    - 信号：动词/状态语义，不使用 `on` 前缀。
    - 槽：`onXxx` 或 `handleXxx`（二选一并全局一致）。
    - 请求类信号命名统一（避免 `Proc`/`Process` 混用）。
  - 建立“旧名 -> 新名”映射表（用于后续批量替换）。

### 任务 2：统一信号/槽命名（跨层联动）
- **优先改造对象**：
  - `errorOccur` -> `errorOccurred`（`BagWorker`, `DataService`, `PCViewer` 及其连接点）
  - `ControlPanelWidget::onImageFrameReady`（信号）重命名为无 `on` 前缀形式（如 `imageFrameReady`）
  - `requestProcBag` / `requestProcessBag` 命名统一（`DataWidget`、`ControlPanelWidget`、`PCViewer`、`Controller`、`DataService`）
- **范围文件**：
  - `src/ui/{DataWidget,ControlPanelWidget,PCViewer,InteractionWidget}.*`
  - `src/core/{Controller,DataService}.*`
  - `src/io/{BagWorker,DatabaseManager}.*`
- **结果要求**：
  - 所有声明、定义、connect 连接点同步更新并可编译。

### 任务 3：优化 connect 可读性与美观性
- **优化内容**：
  - 将“仅 emit 转发”的 lambda 改为直接 signal-to-signal 连接。
  - 统一 connect 排版（参数换行、对齐、分组顺序：输入连接 -> 数据连接 -> 渲染连接）。
  - 去除冗余空行和不必要中间变量命名噪音。
- **重点文件**：
  - `src/ui/ControlPanelWidget.cpp`
  - `src/ui/PCViewer.cpp`
  - `src/core/Controller.cpp`
  - `src/core/DataService.cpp`
  - `src/ui/InteractionWidget.cpp`

### 任务 4：一致性清理与格式化
- **内容**：
  - 按 `.clang-format` 统一缩进、换行和 include 排序。
  - 清理空的 `public slots:`、无效注释与不一致签名（如 `const int&` 与 `int` 的使用边界）。
- **范围**：
  - 本次涉及修改的 Qt 信号槽链路文件（`src/ui`、`src/core`、`src/io`）。

### 任务 5：构建与回归检查
- **内容**：
  - 执行现有 CMake 构建流程，确认信号/槽重命名与 connect 调整未引入编译错误。
  - 手工检查关键链路：
    - 导入 bag -> topic 列表刷新
    - 进度条联动
    - 点云/图像/里程计帧联动渲染

## 关键注意事项
- 本次为“可读性与规范优化”，不改动业务逻辑与线程模型。
- 命名重构要一次完成同名链路，避免中间态破坏连接。
- 本次不覆盖 `src/osgQOpenGL` 与 `src/node`，除非它们被 Qt 信号槽重命名链路直接影响。
