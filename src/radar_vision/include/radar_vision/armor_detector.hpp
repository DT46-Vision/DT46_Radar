#ifndef RADAR_VISION__ARMOR_DETECTOR_HPP_
#define RADAR_VISION__ARMOR_DETECTOR_HPP_

#include <memory>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "radar_vision/car_detector.hpp"

namespace radar_vision
{

class ArmorDetector
{
public:
  ArmorDetector(
    const std::string & model_path, float conf_thres, float iou_thres, int input_size = 1280);
  ~ArmorDetector();

  ArmorDetector(const ArmorDetector &) = delete;
  ArmorDetector & operator=(const ArmorDetector &) = delete;

  bool ok() const;
  const std::string & backend() const;
  // Detect armors inside one vehicle ROI. Returned boxes are in full-image coordinates.
  std::vector<Detection> infer_roi(
    const cv::Mat & full_bgr, const Detection & car_box, float expand_ratio = 0.1f);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace radar_vision

#endif  // RADAR_VISION__ARMOR_DETECTOR_HPP_
