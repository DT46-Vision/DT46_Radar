# 雷达站概览

来源：[`radar-station` 技能](../.agents/skills/radar-station/SKILL.md)。协作者可读说明；改包边界或顺序时先改该技能。

## 这是什么

RoboMaster 雷达站工程。硬件是 Livox Mid-70 + 俯视相机；软件是 ROS 2 Humble。

单独仓库，分层沿用 `DT46_V`（接口 / 驱动 / 算法 / 通信 / bringup），**不**沿用云台包：

- `DT46_V` 的 `rm_detector`、`rm_tracker`、`rm_serial` 是云台链路（装甲板 PnP → 跟一个目标 → 电控串口）。
- 那些消息和串口语义不进本仓库。

## 包职责

| 包 | 职责 |
|---|---|
| `radar_interfaces` | 场地目标、来源、时间戳 |
| `radar_vision` | 进程内取图、检测、投影、跟踪；只发布场地坐标 |
| `radar_radio` | 先搁置；解调信息波和干扰波 |
| `radar_fusion` | 选用来源，决定何时消耗双倍易伤机会 |
| `rm_referee` | 使用与现行协议对齐的 [XDU rm_referee_ros2](https://github.com/XDU-IRobot/rm_referee_ros2)；不自写第二套裁判串口，不用 `DT46_V` 的 `rm_serial` |
| `radar_bringup` | launch、场地标定、udev |

点云进程用官方 `livox_ros2_driver`，约束见 [sensing.md](sensing.md)。

裁判串口只由一个进程写。无线与标记命令见 [referee-link.md](referee-link.md)。

## 实现顺序

1. **相机检测 + Mid-70 框内深度** → 投影到场地 → 经 `0x0305` 上报坐标。没有 SDR 也能做标记。
2. **信息波 / 干扰波先搁置**。接入后，融合节点再为每个目标选来源。
3. **按队伍策略消耗双倍易伤**，并提交干扰密钥。协议只认计数与校验，不规定触发策略。

## 和视觉仓库的关系

| 本仓库 | `DT46_V` |
|---|---|
| 俯视雷达站：场地坐标、小地图标记 | 步兵云台：装甲板、电控 |
| 取图在 `radar_vision` 进程内，不发布 `/image_raw` | `hik_camera` 发整幅图给检测 |
| 裁判用 `rm_referee` | 电控用 `rm_serial` |
