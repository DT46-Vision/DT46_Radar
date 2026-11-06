# 感知：Mid-70 与俯视相机

来源：[`radar-station`](../.agents/skills/radar-station/SKILL.md)、[`livox-mid70`](../.agents/skills/livox-mid70/SKILL.md)。接线、IP、点率、固件等实测数字只在相邻仓库 `Livox_MID-70` 的 README，这里不复制。

## Mid-70

- SDK：只用不 Livox-SDK **v1**。SDK2 枚举里有 Mid-70，数据链路未实现。
- ROS 2 驱动：官方 [livox_ros2_driver](https://github.com/Livox-SDK/livox_ros2_driver)。不用 `livox_ros_driver2`（面向 HAP / Mid-360 / Avia 2）。
- 不把 SDK 二进制提交进本仓库（和海康闭源 `.so` 随包不同）。本机编译方式见 [getting-started.md](getting-started.md)。
- 视场约 70.4° 圆，盖不住整场；作用是给检测框提供深度，不代替俯视相机。
- 广播码须正好 15 字符，与 `Livox_MID-70` 实测 SN 一致；写在 `radar_bringup/config/livox_lidar_config.json`。

## 俯视相机

- 型号：海康机器人 MV-CS016-10UC。
- 取图：传感器全幅 **1440×1080**、1×1，不 binning，不裁 ROI。训练与推理同分辨率。
- 取图在 `radar_vision` 进程内完成，**不**发布 `/image_raw`。采集线程只保留最新一帧；标记发布仍是 5 Hz。
- 内参在 `radar_vision/config/camera_info.yaml`（识别节点启动加载；当前为 hero 占位，俯视相机需重标）。`enable_undistort` 默认开，检测前按内参去畸变。场地投影还要相机–雷达 / 场地外参，内参 alone 不够。
- 调试画面用进程内窗口（参数 `show_image`）。MVS 链本仓 `src/radar_hik_SDK/hikSDK`（仅 SDK，非 ROS 节点）。

## 检测

- 模型：SHARK 的 `car.onnx` / `armor.onnx`（YOLOv5），放在 `radar_vision/model/`（已进 Git）。
- 流程：先整车，再在车框 ROI 里跑装甲板。
- `armor.onnx` 输入 1280；类别 B1–B5、B7、R1–R5、R7。
- 推理用本机 `third_party` 的 ONNX Runtime（OpenCV DNN 加载不了这份导出）。换机准备步骤、版本与 CUDA 注意见 [getting-started.md](getting-started.md) 的「ONNX Runtime」一节。
- 不用步兵的灯条检测。

`radar_vision` 按 5 Hz 发布 12 槽 `TargetArray`（顺序同 `0x0305`）。装甲 ID 经 `our_color`（blue/red）映射到敌我槽位；`x_cm` / `y_cm` 等 Mid-70 框内深度与外参后再填（当前为 0）。同一槽多块装甲保留最高分。

## 深度与场地投影

框内深度和场地坐标分开做。

| 情况 | 做法 | 主要参考 |
|---|---|---|
| Mid-70 照得到 | 相机–雷达外参把检测框对到点云；框内前景 / 掩体 / 背景做 K=3，取前景深度；再四点外参投到场地 | 沈航 RM_Radar2023；华南虎 SRRS 聚类 |
| Mid-70 照不到 | 无框内深度；地面 / 高地双层仿射，像素落到场地平面 | SHARK radar |
| 坡道误差大了以后 | 射线打到场地 mesh | HKUST 2025；第一版不和双层仿射一起做 |
| 雷达故障备用 | 三层透视变换（赛前标 12 点） | 华南虎；不作为主定位 |

敌方坐标：信息波接入后优先信息波，断流再用视觉。己方只有视觉（无线广播无己方位置）。信息波当前搁置时，标记先只走视觉。

## 不要拿来用的部分

对照开源时注意边界：沈航仓库内嵌的 ROS 1 `livox_ros_driver`、华南虎的 Ouster 整仓、SHARK 的发射台 / Web 面板第一版、HKUST 射线与双层仿射同时上——都不是本仓库当前范围。细节表见 `radar-station` 技能。
