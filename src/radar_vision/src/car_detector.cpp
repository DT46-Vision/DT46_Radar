#include "radar_vision/car_detector.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include <onnxruntime_cxx_api.h>
#include <opencv2/imgproc.hpp>

#include "radar_vision/ort_session.hpp"

namespace radar_vision
{
namespace
{

const char * kClassNames[] = {"car", "armor", "ignore", "watcher", "base"};
constexpr int kNumClasses = 5;

cv::Mat letterbox(const cv::Mat & src, int size, float & scale, int & pad_x, int & pad_y)
{
  scale = std::min(static_cast<float>(size) / src.cols, static_cast<float>(size) / src.rows);
  const int new_w = static_cast<int>(std::round(src.cols * scale));
  const int new_h = static_cast<int>(std::round(src.rows * scale));
  pad_x = (size - new_w) / 2;
  pad_y = (size - new_h) / 2;

  cv::Mat resized;
  cv::resize(src, resized, cv::Size(new_w, new_h));
  cv::Mat out(size, size, CV_8UC3, cv::Scalar(114, 114, 114));
  resized.copyTo(out(cv::Rect(pad_x, pad_y, new_w, new_h)));
  return out;
}

float iou(const Detection & a, const Detection & b)
{
  const float x1 = std::max(a.x1, b.x1);
  const float y1 = std::max(a.y1, b.y1);
  const float x2 = std::min(a.x2, b.x2);
  const float y2 = std::min(a.y2, b.y2);
  const float inter = std::max(0.0f, x2 - x1) * std::max(0.0f, y2 - y1);
  const float area_a = std::max(0.0f, a.x2 - a.x1) * std::max(0.0f, a.y2 - a.y1);
  const float area_b = std::max(0.0f, b.x2 - b.x1) * std::max(0.0f, b.y2 - b.y1);
  return inter / (area_a + area_b - inter + 1e-6f);
}

std::vector<Detection> nms(std::vector<Detection> dets, float iou_thres)
{
  std::sort(
    dets.begin(), dets.end(),
    [](const Detection & a, const Detection & b) {return a.score > b.score;});
  std::vector<Detection> kept;
  std::vector<bool> removed(dets.size(), false);
  for (size_t i = 0; i < dets.size(); ++i) {
    if (removed[i]) {
      continue;
    }
    kept.push_back(dets[i]);
    for (size_t j = i + 1; j < dets.size(); ++j) {
      if (!removed[j] && iou(dets[i], dets[j]) > iou_thres) {
        removed[j] = true;
      }
    }
  }
  return kept;
}

}  // namespace

struct CarDetector::Impl
{
  std::unique_ptr<Ort::Session> session;
  Ort::AllocatorWithDefaultOptions allocator;
  std::string input_name;
  std::string output_name;
  std::string backend;
  float conf_thres{0.5f};
  float iou_thres{0.45f};
  int input_size{640};
};

CarDetector::CarDetector(
  const std::string & model_path, float conf_thres, float iou_thres, int input_size)
: impl_(std::make_unique<Impl>())
{
  impl_->conf_thres = conf_thres;
  impl_->iou_thres = iou_thres;
  impl_->input_size = input_size;
  auto & factory = OrtSessionFactory::instance();
  impl_->session = factory.create_session(model_path, 2);
  impl_->backend = factory.backend();

  Ort::AllocatedStringPtr in_name =
    impl_->session->GetInputNameAllocated(0, impl_->allocator);
  Ort::AllocatedStringPtr out_name =
    impl_->session->GetOutputNameAllocated(0, impl_->allocator);
  impl_->input_name = in_name.get();
  impl_->output_name = out_name.get();
}

CarDetector::~CarDetector() = default;

bool CarDetector::ok() const
{
  return impl_ && impl_->session;
}

const std::string & CarDetector::backend() const
{
  return impl_->backend;
}

std::vector<Detection> CarDetector::infer(const cv::Mat & bgr)
{
  if (!ok() || bgr.empty()) {
    return {};
  }

  float scale = 1.0f;
  int pad_x = 0;
  int pad_y = 0;
  const cv::Mat boxed = letterbox(bgr, impl_->input_size, scale, pad_x, pad_y);

  cv::Mat float_img;
  boxed.convertTo(float_img, CV_32FC3, 1.0 / 255.0);
  std::vector<cv::Mat> channels(3);
  cv::split(float_img, channels);

  std::vector<float> input(3 * impl_->input_size * impl_->input_size);
  const size_t plane = static_cast<size_t>(impl_->input_size) * impl_->input_size;
  // OpenCV BGR channels -> model expects RGB order after swap in preprocess.
  // SHARK letterbox path uses RGB; our Frame is RGB then converted to BGR for display.
  // Here input is BGR Mat: map R=channels[2], G=channels[1], B=channels[0].
  std::memcpy(input.data() + 0 * plane, channels[2].data, plane * sizeof(float));
  std::memcpy(input.data() + 1 * plane, channels[1].data, plane * sizeof(float));
  std::memcpy(input.data() + 2 * plane, channels[0].data, plane * sizeof(float));

  const std::array<int64_t, 4> shape{
    1, 3, impl_->input_size, impl_->input_size};
  Ort::MemoryInfo memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  Ort::Value tensor = Ort::Value::CreateTensor<float>(
    memory, input.data(), input.size(), shape.data(), shape.size());

  const char * input_names[] = {impl_->input_name.c_str()};
  const char * output_names[] = {impl_->output_name.c_str()};
  auto outputs = impl_->session->Run(
    Ort::RunOptions{nullptr}, input_names, &tensor, 1, output_names, 1);

  float * data = outputs[0].GetTensorMutableData<float>();
  const auto info = outputs[0].GetTensorTypeAndShapeInfo();
  const auto dims = info.GetShape();
  // Expected [1, 25200, 10]
  if (dims.size() != 3) {
    return {};
  }
  const int rows = static_cast<int>(dims[1]);
  const int cols = static_cast<int>(dims[2]);
  if (cols < 5 + kNumClasses) {
    return {};
  }

  std::vector<Detection> raw;
  float best_any = 0.0f;
  int best_any_cls = -1;
  for (int i = 0; i < rows; ++i) {
    const float * row = data + static_cast<size_t>(i) * cols;
    const float obj = row[4];
    int best_cls = 0;
    float best_cls_score = row[5];
    for (int c = 1; c < kNumClasses; ++c) {
      if (row[5 + c] > best_cls_score) {
        best_cls_score = row[5 + c];
        best_cls = c;
      }
    }
    const float score = obj * best_cls_score;
    if (score > best_any) {
      best_any = score;
      best_any_cls = best_cls;
    }
    if (score < impl_->conf_thres) {
      continue;
    }
    // Keep vehicle-like classes for the car model stage.
    if (best_cls != 0 && best_cls != 3) {
      continue;
    }

    const float cx = row[0];
    const float cy = row[1];
    const float w = row[2];
    const float h = row[3];
    Detection det;
    det.x1 = (cx - w * 0.5f - pad_x) / scale;
    det.y1 = (cy - h * 0.5f - pad_y) / scale;
    det.x2 = (cx + w * 0.5f - pad_x) / scale;
    det.y2 = (cy + h * 0.5f - pad_y) / scale;
    det.score = score;
    det.class_id = best_cls;
    det.label = kClassNames[best_cls];
    raw.push_back(det);
  }

  auto kept = nms(std::move(raw), impl_->iou_thres);
  for (auto & det : kept) {
    det.x1 = std::clamp(det.x1, 0.0f, static_cast<float>(bgr.cols - 1));
    det.y1 = std::clamp(det.y1, 0.0f, static_cast<float>(bgr.rows - 1));
    det.x2 = std::clamp(det.x2, 0.0f, static_cast<float>(bgr.cols - 1));
    det.y2 = std::clamp(det.y2, 0.0f, static_cast<float>(bgr.rows - 1));
  }
  last_max_score_ = best_any;
  last_max_class_ = best_any_cls;
  return kept;
}

}  // namespace radar_vision
