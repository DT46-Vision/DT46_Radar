# 上手：编译与 launch

来源：[`livox-mid70`](../.agents/skills/livox-mid70/SKILL.md)、[`radar-station`](../.agents/skills/radar-station/SKILL.md)、根 [README.md](../README.md)。实测接线与广播码数字见 `Livox_MID-70` README。

## 环境

- Ubuntu 22.04 + ROS 2 Humble
- 工作区：本仓库根目录（`colcon` 工作空间）

## 检测模型（已在 Git）

`src/radar_vision/model/car.onnx`、`armor.onnx` 随仓库跟踪。换机 clone 后即可用，无需另下模型。

## ONNX Runtime（`third_party/`，不进 Git）

`radar_vision` 链的是仓库根下 `third_party/` 里的官方 C++ 包，**不是**模型目录。该目录在 `.gitignore` 中，换机要自己准备。版本与选用顺序以 [`radar-station` 技能](../.agents/skills/radar-station/SKILL.md) 和 `src/radar_vision/CMakeLists.txt` 为准。

CMake 查找顺序（有就用，否则下一项）：

1. `third_party/onnxruntime-linux-x64-gpu-1.28.0`（CUDA 13 EP）
2. `third_party/onnxruntime-linux-x64-gpu-1.19.2`（CUDA 12 EP）
3. `third_party/onnxruntime-linux-x64-1.19.2`（CPU；有显卡也会走 CPU）

### 推荐：本机 CUDA 13 → GPU 1.28.0

```bash
cd <本仓库根目录>
mkdir -p third_party && cd third_party
curl -L -o ort-gpu.tgz \
  https://github.com/microsoft/onnxruntime/releases/download/v1.28.0/onnxruntime-linux-x64-gpu-1.28.0.tgz
tar -xzf ort-gpu.tgz
# 解压后目录名须为 onnxruntime-linux-x64-gpu-1.28.0
```

CUDA / cuDNN 运行库（与 ORT 大版本一致，**不要**把 12 和 13 混进同一 RPATH）：

```bash
pip install --user 'nvidia-cuda-runtime-cu13' 'nvidia-cudnn-cu13'
# CMake 用 python3 探测 site-packages/nvidia（不写死 3.10），再挂 cu13/lib、cudnn/lib 到 RPATH
```


### 只要 CPU / 无合适 GPU 包

```bash
cd <本仓库根目录>/third_party
curl -L -o ort-cpu.tgz \
  https://github.com/microsoft/onnxruntime/releases/download/v1.19.2/onnxruntime-linux-x64-1.19.2.tgz
tar -xzf ort-cpu.tgz
```

缺包时 `radar_vision` 配置会直接失败（`ONNX Runtime not found`）。

### 为何不把 ORT 提交进 Git

体积约数百 MB，且绑定 CUDA 大版本；换机复用靠「固定版本 + 本页步骤」比塞二进制更稳。模型已在 Git，ORT 本机装即可。

## Mid-70 驱动

```bash
# 驱动已在 src/livox_ros2_driver/；脚本主要挂 gcc 11 补丁（缺目录时才克隆）
./tools/setup_livox_driver.sh

source /opt/ros/humble/setup.bash
cd <本仓库根目录>
colcon build --packages-up-to livox_ros2_driver
source install/setup.bash
ros2 launch radar_bringup livox_only.launch.py
```


点云话题：`/livox/lidar`。

注意：

- `radar_bringup/config/livox_lidar_config.json` 里 15 位广播码须与实测 SN 一致，且 `enable_connect` 为 true（默认示例多为 false，会连上扫到的所有雷达）。
- 若系统里已有 `/usr/local/lib/liblivox_sdk_static.a` 且未带 `-fPIC`，链驱动会失败；编驱动前先挪走该库。
- 未做 PTP/GPS 时，点云 `header.stamp` 是设备计数，不是 ROS 时间。

## 仅视觉

需本机已按上文放好 ORT，且海康相机可用：

```bash
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 launch radar_vision vision.launch.py
```

话题：`/radar/targets`。`camera.yaml` 里 `show_image: true` 时会开预览窗。取图在进程内，不发布 `/image_raw`。

## 第一阶段

```bash
ros2 launch radar_bringup phase1.launch.py
```

拉起 Mid-70 + `radar_vision` + `radar_fusion`（视觉目标转发到 `/radar/marks`）。裁判串口依赖外部 `rm_referee`，本仓库不另写同名包。

## 目录速查

| 路径 | 用途 |
|---|---|
| `src/radar_*` | 本仓库包 |
| `src/radar_vision/model/` | 检测 ONNX（已跟踪） |
| `third_party/` | ORT 预编译包（gitignore，本机按上文准备） |
| `src/radar_bringup/launch/` | `livox_only.launch.py`、`phase1.launch.py` |
| `src/radar_vision/launch/` | `vision.launch.py`（仅视觉） |
| `src/radar_bringup/config/livox_lidar_config.json` | Mid-70 连接配置 |
| `tools/setup_livox_driver.sh` | 驱动补丁（缺目录时才克隆） |
| `docs/` | 本目录：协作者说明 |
