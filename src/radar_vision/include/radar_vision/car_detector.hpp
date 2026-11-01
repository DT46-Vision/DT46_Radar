#ifndef RADAR_VISION__CAR_DETECTOR_HPP_
#define RADAR_VISION__CAR_DETECTOR_HPP_

#include <memory>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

namespace radar_vision
{

struct Detection
{
  float x1{0.0f};
  float y1{0.0f};
  float x2{0.0f};
  float y2{0.0f};
  float score{0.0f};
  int class_id{0};
  std::string label;
};

class CarDetector
{
public:
  CarDetector(
    const std::string & model_path, float conf_thres, float iou_thres, int input_size = 640);
  ~CarDetector();

  CarDetector(const CarDetector &) = delete;
  CarDetector & operator=(const CarDetector &) = delete;

  bool ok() const;
  std::vector<Detection> infer(const cv::Mat & bgr);
  float last_max_score() const {return last_max_score_;}
  int last_max_class() const {return last_max_class_;}
  const std::string & backend() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  float last_max_score_{0.0f};
  int last_max_class_{-1};
};

}  // namespace radar_vision

#endif  // RADAR_VISION__CAR_DETECTOR_HPP_
