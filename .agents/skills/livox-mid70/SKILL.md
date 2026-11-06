---
name: livox-mid70
description: 决定 Livox Mid-70 的 SDK 版本、ROS 2 驱动和打包方式。在接入点云、编译 livox_ros2_driver、比较海康随包 SDK、或有人提议用 Livox-SDK2 / livox_ros_driver2 时使用。
---

# Livox Mid-70

实测接线、IP、点率和固件只看 `Livox_MID-70` 的 README。这里只保留选型。

## 用哪套 SDK

Mid-70 只用 Livox-SDK v1。SDK2 的枚举里有 Mid-70，数据链路没有实现。

ROS 2 Humble 用官方 [livox_ros2_driver](https://github.com/Livox-SDK/livox_ros2_driver)（基于 SDK v1）。不用 `livox_ros_driver2`，它的设备链路是 HAP / Mid-360 / Avia 2。

这个驱动是 ROS 1 移植，版本 0.0.1。它负责出点云，不负责第 0 回波过滤，也不把未同步的设备时间戳换成 ROS 时间。

## 不要做成海康包

`DT46_V` 的 `hik_camera` 把闭源 `.so` 按 amd64/arm64 放进仓库，是因为 MVS 不能从源码编译。Livox-SDK 是开源的，本机产物是静态库 `liblivox_sdk_static.a`，没有可随包分发的共享库。

继续用 `Livox_MID-70/tools/build_tools.sh` 的方式：固定 commit，打 `SDK/livox-sdk-ubuntu2204-gcc11.patch`。不要把 SDK 二进制提交进雷达站仓库。

## Ubuntu 22.04 上编译驱动

官方 `livox_ros2_driver` 以源码树放在 `src/livox_ros2_driver/`（随本仓跟踪）。`tools/setup_livox_driver.sh` 在缺目录时才克隆，平时只把 gcc 11 补丁挂进 `livox_sdk_vendor` 的 ExternalProject。然后：

```bash
source /opt/ros/humble/setup.bash
cd <本仓库根目录>
colcon build --packages-up-to livox_ros2_driver
source install/setup.bash
ros2 launch radar_bringup livox_only.launch.py
```


`livox_sdk_vendor` 会拉取 Livox-SDK **v2.3.1**，并打开 `-fPIC`。该 tag 已经补了 `thread_base.h` 的 `<memory>`，`sdk_core/CMakeLists.txt` 仍是 `-Wall -Werror -Wno-c++11-long-long`。gcc 11 不认识这个选项，源码里再有别的警告就会编失败。补丁在 `tools/patches/livox-sdk-ubuntu2204-gcc11.patch`，由 setup 脚本挂上。本机若是 CMake 4.x，ExternalProject 还需 `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`（setup 脚本已加）。

如果 `/usr/local/lib/liblivox_sdk_static.a` 已存在，`find_package` 会用它并跳过 vendor。那份库若没有 `-fPIC`，链进 `liblivox_ros2_driver.so` 会报 `R_X86_64_TPOFF32`。编驱动前先把这份系统静态库挪走。

`radar_bringup/config/livox_lidar_config.json` 的 15 位广播码必须与 `Livox_MID-70` README 实测 SN 一致；本技能不复制该数字。

## 连这台雷达时的接口约定

- 广播码正好 15 个字符。`mid70_dump` 日志里的 `SN=` 打印的就是广播码。16 位的「序列号再加一个 1」会被驱动丢掉。
- 默认 json 里示例设备的 `enable_connect` 都是 false，结果是连上扫到的所有雷达。这台雷达要写上 15 位广播码并设为 true。
- `imu_rate` 对 Mid-70 无效。驱动对 Mid-70 / Mid-40 不会调用 `LidarSetImuPushFrequency`。
- 默认 `xfer_format = 0` 的 `PointCloud2` 带 `x y z intensity tag line`。`xfer_format = 2` 的标准 PCL `PointXYZI` 在驱动里没有创建 publisher。
- 未做 PTP/GPS 时，`header.stamp` 是设备自己的计数，不是 ROS 时间。
- 驱动默认大约 10 Hz 攒一帧。帧内时间在 CustomMsg 的 `offset_time`。
- 视场是 70.4° 的圆，盖不住整场。它提供检测框内的深度，不代替俯视相机。
