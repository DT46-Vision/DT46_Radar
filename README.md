# DT46_Radar

RoboMaster 雷达站工程。硬件是 Livox Mid-70 加俯视相机，软件是 ROS 2 Humble。相机型号、分辨率、深度和投影见 [radar-station](.agents/skills/radar-station/SKILL.md)。目录名与包前缀 `radar_*` 对齐；Mid-70 实测仍在 `Livox_MID-70` 仓库。

当前事实按 [.claude/conventions/docs.md](.claude/conventions/docs.md) 分属维护。协作入口是 [AGENTS.md](AGENTS.md)。包边界在 [radar-station](.agents/skills/radar-station/SKILL.md)。

代码在 `src/`：`radar_interfaces`、`radar_vision`、`radar_fusion`、`radar_radio`、`radar_bringup`。第一阶段 launch 是 `radar_bringup` 的 `phase1.launch.py`。裁判串口用 XDU 的 `rm_referee`，不在本仓库再写同名包。

## 待确认

- 是否已有裁判串口。信息波和干扰波先搁置。
- XDU `rm_referee_ros2` 尚未放进工作区，也还没钉到协议 V2.0.0 对应的 tag。
- 本目录尚未初始化 Git。

点云上手和实测数字在 `Livox_MID-70` 的 README，不在这里复制。
