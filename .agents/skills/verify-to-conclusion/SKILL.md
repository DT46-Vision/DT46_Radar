---
name: verify-to-conclusion
description: 用户要求实验、验证或印证假设，或需要实机确认点云、相机、串口时使用。开始前写明假设、方法、预期和停止条件。资料检索用 research-with-evidence，结果报告用 report-after-change。
---

# 实验验证到结论

## 必须遵守

1. MUST 只验证和当前问题有关的假设。先写到结论，再做。
2. MUST 在开始前写明假设、方法、预期现象、结果怎样支持或否定假设、何时停止。缺一项不开始。
3. MUST 按已确认流程做。结果不符预期时如实记录。要改已确认方案才能继续时，先走 [align-before-act](../align-before-act/SKILL.md)。

## 本仓库常见验证

- Mid-70 是否出点云：用 `Livox_MID-70` 里已经实测过的 `verify_mid70.sh` / `mid70_dump`，不新写一套发现协议。Viewer 和命令行工具不能同时占同一台雷达。
- ROS 2 驱动是否编过：先确认没有拿 `/usr/local/lib` 里未加 `-fPIC` 的 `liblivox_sdk_static.a` 去链共享库。编译约束见 [livox-mid70](../livox-mid70/SKILL.md)。
- 裁判命令：用虚拟串口或协议测试确认组帧和命令码。没接裁判系统时，不把「节点能启动」写成「标记已上图」。
- 无线电：没接 SDR 时，不把信息波解析写成已经收到官方广播。

改变接口、预计做很久、或要动实机连接时，执行前走 align-before-act。查已有证据用 [research-with-evidence](../research-with-evidence/SKILL.md)，检索不能代替这次实验。

完成后走 [report-after-change](../report-after-change/SKILL.md)。得不出结论就写「无法得出结论」，并停止依赖该结论。
