---
name: project-context-docs
description: 修改任何项目文件前读取相关项目文档；文档与代码冲突时报告；改动行为、接口或工作流时在同一任务里同步最小相关文档。
---

# 项目上下文与文档同步

1. MUST 修改前读取 [AGENTS.md](../../../AGENTS.md)、[README.md](../../../README.md) 和 [.claude/conventions/docs.md](../../../.claude/conventions/docs.md)，再读负责该事实的技能。不借此通读无关仓库。
2. MUST 用代码或原文核对文档。冲突时报告位置和影响，不自行选一边。
3. MUST 在改变行为、接口、配置或已记录结论的同一次任务里，更新负责该事实的那一处。纯机械修改且文档仍对时不改文档。

## 本仓库的边界

- Mid-70 的接线、IP、点率和固件以 `Livox_MID-70` README 的实测记录为准。本仓库技能不复制这些数字。
- `DT46_V` 的 `rm_detector`、`rm_tracker`、`rm_serial` 是云台链路。雷达站不继承它们的消息和串口语义。
- 海康包可以说明「闭源 SDK 为什么要随包提交二进制」。Mid-70 不是这种 SDK，打包决策只维护在 [livox-mid70](../livox-mid70/SKILL.md)。
- 协议字段只维护在 [radar-referee-link](../radar-referee-link/SKILL.md)。

找不到专属入口时，补进最接近的现有文档，不另开一份重复说明。结束前核对代码和文档是否还一致，并写明哪一项还没验证。
