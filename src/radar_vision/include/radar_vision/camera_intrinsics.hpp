#ifndef RADAR_VISION__CAMERA_INTRINSICS_HPP_
#define RADAR_VISION__CAMERA_INTRINSICS_HPP_

#include <string>

#include <opencv2/core.hpp>
#include "sensor_msgs/msg/camera_info.hpp"

namespace radar_vision
{

// Camera intrinsics for detection / projection. Loaded from camera_info yaml.
struct CameraIntrinsics
{
  bool valid{false};
  std::string camera_name;
  int width{0};
  int height{0};
  // 3x3 camera matrix K, 1xN distortion (plumb_bob usually N=5).
  cv::Mat K;
  cv::Mat D;
  sensor_msgs::msg::CameraInfo info;

  double fx() const {return K.empty() ? 0.0 : K.at<double>(0, 0);}
  double fy() const {return K.empty() ? 0.0 : K.at<double>(1, 1);}
  double cx() const {return K.empty() ? 0.0 : K.at<double>(0, 2);}
  double cy() const {return K.empty() ? 0.0 : K.at<double>(1, 2);}

  // Pinhole maps with same K (undistorted pixels still use this K for rays).
  bool build_undistort_maps(cv::Mat & map1, cv::Mat & map2) const;
};

// Read camera_calibration_parsers yaml (same layout as DT46-V *_camera_info.yaml).
// Returns false on missing/invalid file; out.valid stays false.
bool load_camera_intrinsics(const std::string & path, CameraIntrinsics & out);

}  // namespace radar_vision

#endif  // RADAR_VISION__CAMERA_INTRINSICS_HPP_
