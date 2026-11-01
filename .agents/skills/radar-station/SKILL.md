---
name: radar-station
description: 规定 DT46_Radar 的包边界、参考仓库和实现顺序。在新建节点、拆包、对照 DT46_V、或引用华南虎、沈航、SHARK、HKUST 雷达开源时使用。
---

# 雷达站结构

单独建这个仓库。沿用 `DT46_V` 的分层：接口、驱动、算法、通信、bringup 分开。不沿用它的云台包。

`DT46_V` 是 `hik_camera` 发整幅图，`rm_detector` 做装甲板 PnP，`rm_tracker` 跟一个目标，`rm_serial` 把云台控制发给电控。那些消息和串口不进雷达站。

## 包

| 包 | 职责 |
|---|---|
| `radar_interfaces` | 场地目标、来源、时间戳 |
| `radar_vision` | 进程内取图、检测、投影、跟踪。只发布场地坐标 |
| `radar_radio` | 先搁置。解调信息波和干扰波，见 [radar-referee-link](../radar-referee-link/SKILL.md) |
| `radar_fusion` | 选用来源，决定何时消耗双倍易伤机会 |
| `rm_referee` | 使用与现行协议对齐的 [XDU rm_referee_ros2](https://github.com/XDU-IRobot/rm_referee_ros2)。不自写第二套裁判串口，不用 `DT46_V` 的 `rm_serial` |
| `radar_bringup` | launch、场地标定、udev |

点云进程用官方 `livox_ros2_driver`，约束见 [livox-mid70](../livox-mid70/SKILL.md)。

俯视相机是海康机器人 MV-CS016-10UC。取图用传感器全幅 1440×1080、1×1，不 binning，不裁 ROI。检测的训练图和推理图都用这个分辨率。取图在 `radar_vision` 进程内完成，不发布 `/image_raw`。采集线程只保留最新一帧，标记发布仍是 5 Hz。调试画面用进程内窗口，参数 `show_image`。MVS 链本机已有的 `hikSDK`，不把 `.so` 复制进本仓库。`DT46_V` 的 `hik_camera` 留在视觉仓库给步兵用。

检测用 SHARK 的 `car.onnx` / `armor.onnx`（YOLOv5），放在 `radar_vision/model/`。先整车，再在车框 ROI 里跑装甲板。`armor.onnx` 输入 1280，类别 B1–B5、B7、R1–R5、R7。OpenCV DNN 加载不了这份导出，推理用本机 `third_party` 里的 ONNX Runtime：有 NVIDIA 设备且 ORT 带 CUDA EP 时用 GPU，否则 CPU。本机驱动报 CUDA 13 时用 `onnxruntime-linux-x64-gpu-1.28.0`（CUDA 13 EP）加 pip 的 `nvidia/cu13` 与 `cudnn`；勿把 CUDA 12 与 13 的库混进同一 RPATH。只用 CPU 包时即使有显卡也会回退 CPU。不用步兵的灯条检测。

`radar_vision` 按 5 Hz 发布 12 槽 `TargetArray`（顺序同 `0x0305`）。装甲 ID 经 `our_color`（blue/red）映射到敌我槽位，写入 `observed` 与 `confidence`；`x_cm`/`y_cm` 等 Mid-70 框内深度与外参后再填，当前为 0。同一槽多块装甲保留最高分。

裁判串口只由一个进程写。

## 参考什么

| 问题 | 参考 | 不拿来用的部分 |
|---|---|---|
| Mid-70 与相机外参、框内深度、四点标定到场地 | [沈航 TUP RM_Radar2023](https://github.com/tup-robomaster/RM_Radar2023) | ROS 1 Noetic、仓库内的 `livox_ros_driver`。该仓库写明不再适配 2024 及以后 |
| 检测框里前景、机体、背景混在一起 | [华南虎 SRRS](https://github.com/Courteous121/SRRS) 的 K=3 聚类 | 他们的雷达是 Ouster，不是 Mid-70。框内直接取平均会把掩体和背景算进去 |
| 小陀螺、丢帧、两车交汇时编号锁死 | 华南虎对 SORT 的分档重分类：交汇时重认；置信度高则少认；连续结果一致再升档 | 不把整份 CMake 单体程序搬进来 |
| 训练图像和相机分辨率不一致会漏检 | 华南虎的记录：俯仰视角下要让训练图分辨率和相机一致 | |
| 2026 视觉加无线电的进程边界 | [SHARK radar](https://github.com/JNU-SHARK/shark-radar-system) | 不把他们的发射台和 Web 面板当作第一版 |
| 坡道和高地投影误差大了以后 | [HKUST 2025 射线投射](https://github.com/hkustenterprize/RM2025-Radar-Algorithm) | 第一版不和双层仿射一起做 |
| 三层透视变换 | 华南虎把它当作雷达故障时的备用，赛前要标 12 个点 | 不作为主定位 |

框内深度和场地坐标分开做。

Mid-70 照得到的车：用沈航的相机-雷达外参把检测框对到点云。框里有机体、掩体和背景，按华南虎做 K=3，取前景那一团的深度，不对整框取平均。再用四点外参把这个深度投到场地。

Mid-70 照不到的车：没有框内深度。用 SHARK 的地面 / 高地双层仿射，把像素落到场地平面。

坡道上这两步误差大了，再换 HKUST 的射线打到场地 mesh。三层透视变换只在雷达故障时备用。

敌方坐标在信息波接入后优先用信息波，断流再用视觉。己方坐标只有视觉，因为无线广播里没有己方位置。信息波和干扰波当前搁置，标记先只走视觉。

## 顺序

1. 相机检测加 Mid-70 框内深度，投影到场地，经 `0x0305` 上报坐标。没有 SDR 也能做标记。
2. 信息波和干扰波先搁置。接入之后，融合节点再选择每个目标的来源。
3. 按自己的策略消耗双倍易伤机会，并提交干扰密钥。触发条件是队伍策略；协议只认 [radar-referee-link](../radar-referee-link/SKILL.md) 里的计数和校验。
