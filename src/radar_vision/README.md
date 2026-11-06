# radar_vision

进程内取图、整车/装甲检测、框内深度与场地坐标；只发布场地目标，不发 `/image_raw`。

内参：`config/camera_info.yaml`（参数 `camera_info_path`）。当前为 hero 占位，俯视相机需重标后覆盖。检测前去畸变：`enable_undistort`（默认 true）。

协作者说明见 [docs/sensing.md](../../docs/sensing.md)。
