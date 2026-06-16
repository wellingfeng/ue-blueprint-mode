# FreeUltraCode BluePrint Mode

一个 UE 编辑器插件，提供一对生命周期命令 `/FreeUltraCodeBluePrintMode.BlueprintMode.Start` 与 `/FreeUltraCodeBluePrintMode.BlueprintMode.End`，把"自然语言生成/编辑蓝图"框成一个显式模式。设计参考 [Ludus AI](https://ludusengine.com/) 的自然语言蓝图编辑，落地通道为插件内直接调用 UE C++ API（`UEdGraph` / `UK2Node_*` / `FBlueprintEditorUtils`），并在本地无法完成时通过 MCP 桥接自动补全。

## 核心特性

- **模式生命周期**：`start` 进入蓝图编排模式，其后所有自然语言被解释为蓝图意图；`end` 收尾提交或丢弃。
- **上下文分级加载**：Overview / Header / Balanced / Full / Properties 五级，控制 token 与成本（对齐 Ludus 分层）。默认 `balanced`。
- **AI 创建蓝图**：目标蓝图不存在时，默认弹确认让用户选择是否创建（`--create-if-missing Ask`）；也可 `--create` 自动创建或 `Never` 报错。
- **实时编排**：通过结构化操作计划（`FFreeUltraCodeBluePrintModeOp`）spawn 节点、连线、设属性，用户实时看到逻辑生长。
- **checkpoint 回滚**：`start` 建事务，`end --discard` 回滚，每个操作可 Ctrl+Z 撤销。
- **校验**：`end --verify --compile` 触发编译与静态校验，失败则留在模式内继续修。
- **MCP 自动补全**：本地 API 覆盖不到的执行性动作（放置 actor、改工程设置等）转发给 MCP server。
- **插件自检**：模块启动时自动检测是否已启用、是否有新版本，发现更新弹通知附下载入口，全程非阻塞。

## 命令

```
FreeUltraCodeBluePrintMode.BlueprintMode.Start [--target <名字>] [--context overview|header|balanced|full|properties]
                     [--agent lite|full] [--dry-run]
                     [--create] [--create-parent <类>] [--create-path <路径>]
                     [--create-if-missing Ask|Always|Never]

FreeUltraCodeBluePrintMode.BlueprintMode.End   [--commit | --discard] [--verify] [--compile]
```

- `--target`：点名目标蓝图，缺省取当前聚焦的蓝图编辑器。
- `--context`：上下文加载等级，控制成本/深度主旋钮。
- `--dry-run`：只产计划与预览，不落盘。
- `--create*`：蓝图不存在时的创建策略。

## 目录结构

```
FreeUltraCodeBluePrintMode.uplugin              插件描述
Source/FreeUltraCodeBluePrintMode/
  FreeUltraCodeBluePrintMode.Build.cs           模块依赖
  Public/Private/
    FreeUltraCodeBluePrintModeModule.*          模块入口，注册 start/end 控制台命令
    FreeUltraCodeBluePrintModeTypes.h           枚举与结构（上下文等级、操作原语、参数）
    FreeUltraCodeBluePrintModeSubsystem.*       核心编排：生命周期、targeting、创建、回滚
    FreeUltraCodeBluePrintModeContextLoader.*  按等级提取蓝图上下文为 JSON
    FreeUltraCodeBluePrintModeOpExecutor.*     操作计划 -> UE 图表 API
    FreeUltraCodeBluePrintModeMcpBridge.*      MCP 桥接，本地无法完成时远程补全
    FreeUltraCodeBluePrintModePluginUpdateChecker.*  安装/版本自检与更新通知
```

## 安装

### 从 GitHub 自动下载

仓库地址：`https://github.com/wellingfeng/ue-blueprint-mode`

1. 在 UE 项目里打开 `项目设置 → 插件 → FreeUltraCode 蓝图模式`。
2. 保持 `GitHub 仓库` 为 `https://github.com/wellingfeng/ue-blueprint-mode.git`。
3. 点击 `一键安装（从 GitHub 下载）`。
4. 安装完成后重启编辑器，在 `编辑 → 插件` 中启用 FreeUltraCode BluePrint Mode。

自动安装会把插件下载到当前 UE 工程的 `Plugins/FreeUltraCodeBluePrintMode`。如果 GitHub 下载失败，会尝试使用设置页里的本地源目录作为兜底。

### 手动安装

1. 下载或 clone 本仓库。
2. 把仓库目录放到工程的 `Plugins/FreeUltraCodeBluePrintMode` 下（或引擎 `Engine/Plugins/Marketplace/`）。
3. 重新生成工程文件并编译。
4. 在 `编辑 → 插件` 中启用 FreeUltraCode BluePrint Mode，重启编辑器。
5. 启动后插件会自动自检安装状态与版本。

## 状态机

```
idle ──start──▶ Active ──end --commit──▶ Verifying ──ok──▶ idle
                  │                          └─fail─▶ Active(继续修)
                  └──end --discard──▶ 回滚 ──▶ idle
```

## 支持范围

对齐 Ludus 已验证类型：Actor / Pawn / Character / GameMode、Level BP、Editor Utility BP、Function Library、UMG 逻辑（不含可视化布局）。Material / Niagara / Behavior Tree / Animation BP 后续扩展。

## 备注

- 所有图操作在 game thread 执行；MCP/HTTP 回调通过 `AsyncTask(GameThread)` 切回主线程。
- `FFreeUltraCodeBluePrintModeOpExecutor::DoSpawnNode` 当前以 `UK2Node_CallFunction` 为骨架示例，完整实现需按 `NodeType` 前缀分派到各 `UK2Node_*` 子类。
- MCP endpoint 默认 `https://mcp.ludusengine.com/mcp`，可在 `EditorPerProjectUserSettings` 的 `[FreeUltraCodeBluePrintMode.BlueprintMode] McpEndpoint` 覆盖；旧的 `[BlueprintMode]` 仍兼容读取。
