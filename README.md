# DT46_Radar

RoboMaster 雷达站：Livox Mid-70 + 俯视海康相机，ROS 2 Humble。
包前缀 `radar_*`；Mid-70 接线与实测数字在相邻仓库 `Livox_MID-70`，不在此复制。

## 文档入口

| 用途 | 位置 |
|---|---|
| 协作者说明 | [docs/](docs/) |
| 协作 / 技能路由 | [AGENTS.md](AGENTS.md) |
| 文档归属 | [.claude/conventions/docs.md](.claude/conventions/docs.md) |
| 包边界与实现顺序 | [radar-station](.agents/skills/radar-station/SKILL.md) |
| 裁判链路与无线 | [radar-referee-link](.agents/skills/radar-referee-link/SKILL.md) |
| Mid-70 驱动约束 | [livox-mid70](.agents/skills/livox-mid70/SKILL.md) |
| 本机编译 / ORT | [docs/getting-started.md](docs/getting-started.md) |

## 环境依赖

一眼清单；安装步骤只维护在 [docs/getting-started.md](docs/getting-started.md)。

| 依赖 | 状态 | 说明 |
|---|---|---|
| Ubuntu 22.04 + ROS 2 Humble | 本机 | |
| `livox_ros2_driver` | 已随仓 | `./tools/setup_livox_driver.sh` 挂 gcc 补丁 |
| `radar_hik_SDK` | 已随仓 | 海康 MVS 头文件与库 |
| 检测 ONNX | 已随仓 | `src/radar_vision/model/` |
| ONNX Runtime | **本机装** | `third_party/`，不进 Git |
| CUDA / cuDNN（可选） | 本机 | GPU ORT 时用；见 getting-started |
| `rm_referee` | 外部 | 裁判串口；是否进工作区见待确认 |

## 仓库结构

| 包 / 路径 | 职责 |
|---|---|
| `radar_interfaces` | 场地目标消息 |
| `radar_vision` | 取图、检测、内参/去畸变；只发场地目标 |
| `radar_fusion` | 选用来源；一阶段只转发视觉 |
| `radar_radio` | 信息波/干扰波（搁置） |
| `radar_bringup` | launch / 配置；`livox_only`、`phase1` |
| `livox_ros2_driver` | 官方点云驱动（随仓） |
| `radar_hik_SDK` | 海康 MVS 头文件与库（非 ROS 节点） |

裁判串口用 XDU `rm_referee`，本仓库不另写同名包。

## 进度

摄像头与雷达驱动已能跑；**无稳固基座，尚未联合标定**。

- Mid-70：`/livox/lidar` 出点云
- `radar_vision`：取图、检测、内参加载、去畸变（内参仍为 hero 占位）
- `radar_fusion`：一阶段只转发 `/radar/targets` → `/radar/marks`
- 无线电、双倍易伤策略：搁置

## 待做

### 上场最低限度（视觉标记闭环）

| 缺口 | 说明 |
|---|---|
| 刚体支架 | 无稳固基座无法正经联合标定 |
| 俯视相机内参重标 | 替换 hero 占位 |
| 相机–Mid-70 外参 | 检测框对到点云 |
| 框内深度（K=3） | 当前 `x_cm`/`y_cm` 仍为 0 |
| 场地四点标定 | 投到地图厘米（赛场需重标） |
| 裁判串口 + `rm_referee` | 能发 `0x0305` |
| 跟踪 / 编号稳定 | 小陀螺、交汇、丢帧 |
| 实机联调 | phase1 全链路，小地图能看见点 |

无 SDR 也可靠视觉做标记；**没有串口和场地坐标则不能正式贡献标记进度**。

### 完整能力（可后做）

| 缺口 | 说明 |
|---|---|
| SDR + 解调 | 信息波 / 干扰波密钥（`radar_radio`） |
| 融合选源 | 敌方优先电波，断流回视觉；己方仅视觉 |
| `0x0121` 易伤 / 交密钥 | 策略 + 串口 |
| 坡道 / 高地 | 双层仿射或射线 |
| 模型匹配 | 确认训练分辨率；不足再微调 |

### 建议顺序

支架 → 相机实机稳 + 重标内参 → 相机–雷达外参 → 框内深度 → 场地标定 → `rm_referee` / `0x0305` →（有余力）SDR / 易伤。

## 待确认

- 是否已有裁判串口硬件，能否正常通信；信息波和干扰波先搁置。
- XDU `rm_referee_ros2` 是否进工作区，并钉到协议 V2.0.0 对应 tag。

## 快速命令

先按 [docs/getting-started.md](docs/getting-started.md) 准备 ORT（`third_party/`），再：

```bash
source /opt/ros/humble/setup.bash
# 首次或缺驱动补丁时：
./tools/setup_livox_driver.sh
colcon build --packages-up-to radar_bringup
source install/setup.bash

# Mid-70 点云
ros2 launch radar_bringup livox_only.launch.py
# 话题 /livox/lidar ；RViz Fixed Frame: livox_frame

# 仅视觉（进程内取图 + 检测；需相机与 ORT）
ros2 launch radar_vision vision.launch.py
# 话题 /radar/targets ；show_image 开时有预览窗

# 第一阶段：点云 + 视觉 + 融合转发
ros2 launch radar_bringup phase1.launch.py
# /radar/targets → /radar/marks ；深度与场地坐标仍未填
```
