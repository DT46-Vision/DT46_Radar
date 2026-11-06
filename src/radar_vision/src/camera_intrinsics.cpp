#include "radar_vision/camera_intrinsics.hpp"

#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>

#include "camera_calibration_parsers/parse.hpp"

namespace radar_vision
{

bool CameraIntrinsics::build_undistort_maps(cv::Mat & map1, cv::Mat & map2) const
{
  if (!valid || K.empty() || width <= 0 || height <= 0) {
    return false;
  }
  cv::initUndistortRectifyMap(
    K, D, cv::Mat(), K, cv::Size(width, height), CV_16SC2, map1, map2);
  return !map1.empty() && !map2.empty();
}

bool load_camera_intrinsics(const std::string & path, CameraIntrinsics & out)
{
  out.valid = false;
  out.camera_name.clear();
  out.width = 0;
  out.height = 0;
  out.K.release();
  out.D.release();
  out.info = sensor_msgs::msg::CameraInfo{};
  if (path.empty()) {
    return false;
  }

  std::string name;
  sensor_msgs::msg::CameraInfo info;
  if (!camera_calibration_parsers::readCalibration(path, name, info)) {
    return false;
  }

  out.camera_name = name;
  out.width = static_cast<int>(info.width);
  out.height = static_cast<int>(info.height);
  out.info = info;

  out.K = cv::Mat(3, 3, CV_64F);
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      out.K.at<double>(r, c) = info.k[static_cast<size_t>(r * 3 + c)];
    }
  }

  const auto n = info.d.size();
  out.D = cv::Mat(1, static_cast<int>(n), CV_64F);
  for (size_t i = 0; i < n; ++i) {
    out.D.at<double>(0, static_cast<int>(i)) = info.d[i];
  }

  out.valid = (out.width > 0 && out.height > 0 && out.fx() > 0.0 && out.fy() > 0.0);
  return out.valid;
}

}  // namespace radar_vision
