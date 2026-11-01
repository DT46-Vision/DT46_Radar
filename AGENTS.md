# DT46_Radar 协作入口

项目是什么、待确认项见 [README.md](README.md)。文档归属见 [.claude/conventions/docs.md](.claude/conventions/docs.md)。技能正文只维护在 `.agents/skills/`，不在其他工具目录复制。

## 必须执行的技能路由

- 查资料、核实来源、证据不足或来源冲突时，先执行 [research-with-evidence](.agents/skills/research-with-evidence/SKILL.md)。已知文件的普通读取不额外检索。
- 修改任何项目文件前执行 [project-context-docs](.agents/skills/project-context-docs/SKILL.md)。创建或编辑文档时执行 [single-source-docs](.agents/skills/single-source-docs/SKILL.md)。
- 修改前执行 [align-before-act](.agents/skills/align-before-act/SKILL.md)，修改后执行 [report-after-change](.agents/skills/report-after-change/SKILL.md)。同一任务里用户已经明确授权的步骤不重复确认。
- 实验或实机印证执行 [verify-to-conclusion](.agents/skills/verify-to-conclusion/SKILL.md)。Git 操作执行 [git-collab-docs](.agents/skills/git-collab-docs/SKILL.md)。
- Mid-70、Livox SDK、ROS 2 驱动执行 [livox-mid70](.agents/skills/livox-mid70/SKILL.md)。
- 雷达站包边界、参考仓库和实现顺序执行 [radar-station](.agents/skills/radar-station/SKILL.md)。
- 信息波、干扰波、标记、双倍易伤和裁判串口执行 [radar-referee-link](.agents/skills/radar-referee-link/SKILL.md)。

路由不扩展用户授权。流程以对应 `SKILL.md` 为准。
