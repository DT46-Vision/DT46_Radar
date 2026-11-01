#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ament_index_cpp/get_package_share_directory.hpp"
#include "radar_interfaces/msg/target.hpp"
#include "radar_interfaces/msg/target_array.hpp"
#include "radar_vision/armor_detector.hpp"
#include "radar_vision/car_detector.hpp"
#include "radar_vision/frame_source.hpp"
#include "radar_vision/ort_session.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

namespace radar_vision
{
namespace
{

constexpr size_t kNumSlots = 12;

// Armor label like B1/R7 -> 0x0305 slot index, or -1 if unknown.
// our_is_blue: B* maps to OUR (6..11), R* to ENEMY (0..5); red team flips.
int armor_label_to_slot(const std::string & label, bool our_is_blue)
{
  if (label.size() < 2) {
    return -1;
  }
  const char team = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
  int number = 0;
  try {
    number = std::stoi(label.substr(1));
  } catch (...) {
    return -1;
  }
  int offset = -1;
  switch (number) {
    case 1: offset = 0; break;  // hero
    case 2: offset = 1; break;  // engineer
    case 3: offset = 2; break;  // infantry 3
    case 4: offset = 3; break;  // infantry 4
    case 5: offset = 4; break;  // aerial
    case 7: offset = 5; break;  // sentry
    default: return -1;
  }
  const bool is_blue = (team == 'B');
  const bool is_red = (team == 'R');
  if (!is_blue && !is_red) {
    return -1;
  }
  const bool is_ours = our_is_blue ? is_blue : is_red;
  return offset + (is_ours ? 6 : 0);
}

}  // namespace

class VisionNode : public rclcpp::Node
{
public:
  VisionNode()
  : Node("radar_vision")
  {
    const auto pointcloud_topic = declare_parameter<std::string>("pointcloud_topic", "/livox/lidar");
    targets_topic_ = declare_parameter<std::string>("targets_topic", "/radar/targets");
    const double publish_rate_hz = declare_parameter<double>("publish_rate_hz", 5.0);
    FrameSourceConfig camera;
    camera.width = declare_parameter<int>("image_width", 1440);
    camera.height = declare_parameter<int>("image_height", 1080);
    camera.exposure_us = declare_parameter<double>("exposure_time", 8000.0);
    camera.gain = declare_parameter<double>("gain", 16.0);
    camera.frame_rate_hz = declare_parameter<double>("grab_rate_hz", 30.0);
    camera.image_file = declare_parameter<std::string>("image_file", "");
    show_image_ = declare_parameter<bool>("show_image", false);
    enable_detect_ = declare_parameter<bool>("enable_detect", true);
    enable_armor_ = declare_parameter<bool>("enable_armor", true);
    const float car_conf = static_cast<float>(declare_parameter<double>("car_conf", 0.35));
    const float car_iou = static_cast<float>(declare_parameter<double>("car_iou", 0.45));
    const float armor_conf = static_cast<float>(declare_parameter<double>("armor_conf", 0.40));
    const float armor_iou = static_cast<float>(declare_parameter<double>("armor_iou", 0.45));
    auto car_model = declare_parameter<std::string>("car_model", "");
    auto armor_model = declare_parameter<std::string>("armor_model", "");
    dump_dir_ = declare_parameter<std::string>("dump_dir", "/tmp/radar_vision_dump");
    auto our_color = declare_parameter<std::string>("our_color", "blue");
    std::transform(
      our_color.begin(), our_color.end(), our_color.begin(),
      [](unsigned char c) {return static_cast<char>(std::tolower(c));});
    our_is_blue_ = (our_color != "red");
    if (our_color != "blue" && our_color != "red") {
      RCLCPP_WARN(
        get_logger(), "our_color=%s invalid; using blue (OUR=B*, ENEMY=R*)",
        our_color.c_str());
      our_is_blue_ = true;
    }

    frame_source_ = create_frame_source(camera);
    image_width_ = camera.width;
    image_height_ = camera.height;

    const auto share = ament_index_cpp::get_package_share_directory("radar_vision");
    if (enable_detect_) {
      if (car_model.empty()) {
        car_model = share + "/model/car.onnx";
      }
      try {
        car_detector_ = std::make_unique<CarDetector>(car_model, car_conf, car_iou);
        RCLCPP_INFO(
          get_logger(), "Loaded car model: %s (ORT %s)",
          car_model.c_str(), car_detector_->backend().c_str());
      } catch (const std::exception & ex) {
        RCLCPP_ERROR(get_logger(), "Failed to load car model: %s", ex.what());
        enable_detect_ = false;
      }
    }
    if (enable_detect_ && enable_armor_) {
      if (armor_model.empty()) {
        armor_model = share + "/model/armor.onnx";
      }
      try {
        armor_detector_ = std::make_unique<ArmorDetector>(armor_model, armor_conf, armor_iou, 1280);
        RCLCPP_INFO(
          get_logger(), "Loaded armor model: %s (ORT %s)",
          armor_model.c_str(), armor_detector_->backend().c_str());
      } catch (const std::exception & ex) {
        RCLCPP_ERROR(get_logger(), "Failed to load armor model: %s", ex.what());
        enable_armor_ = false;
      }
    }

    cloud_sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
      pointcloud_topic, rclcpp::SensorDataQoS(),
      [this](sensor_msgs::msg::PointCloud2::ConstSharedPtr) {
        ++cloud_count_;
      });

    targets_pub_ = create_publisher<radar_interfaces::msg::TargetArray>(
      targets_topic_, rclcpp::QoS(10));

    const auto period = std::chrono::duration<double>(1.0 / publish_rate_hz);
    timer_ = create_wall_timer(
      std::chrono::duration_cast<std::chrono::nanoseconds>(period),
      std::bind(&VisionNode::publish_targets, this));

    if (show_image_) {
      cv::namedWindow("radar_vision", cv::WINDOW_NORMAL);
      cv::resizeWindow("radar_vision", 960, 720);
    }

    running_.store(true);
    capture_thread_ = std::thread([this]() {capture_loop();});
    detect_thread_ = std::thread([this]() {detect_loop();});

    RCLCPP_INFO(
      get_logger(),
      "Grab %.1f Hz, detect=%s armor=%s show=%s, ORT prefer=%s, our_color=%s.",
      camera.frame_rate_hz,
      enable_detect_ ? "yes" : "no",
      (enable_detect_ && enable_armor_ && armor_detector_) ? "yes" : "no",
      show_image_ ? "yes" : "no",
      OrtSessionFactory::instance().prefers_cuda() ? "cuda" : "cpu",
      our_is_blue_ ? "blue" : "red");
  }

  ~VisionNode() override
  {
    running_.store(false);
    latest_cv_.notify_all();
    if (capture_thread_.joinable()) {
      capture_thread_.join();
    }
    if (detect_thread_.joinable()) {
      detect_thread_.join();
    }
    if (show_image_) {
      cv::destroyWindow("radar_vision");
    }
  }

private:
  // Capture only: overwrite the latest slot and wake the detector.
  void capture_loop()
  {
    while (running_.load()) {
      Frame frame;
      if (!frame_source_->grab(frame)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        continue;
      }

      {
        std::lock_guard<std::mutex> lock(latest_mutex_);
        latest_frame_ = std::move(frame);
        ++latest_seq_;
        ++frame_count_;
        latest_width_ = latest_frame_.width;
        latest_height_ = latest_frame_.height;
      }
      latest_cv_.notify_one();
    }
  }

  // Detect on the newest frame only; intermediate frames are dropped.
  void detect_loop()
  {
    uint64_t processed_seq = 0;
    while (running_.load()) {
      Frame frame;
      {
        std::unique_lock<std::mutex> lock(latest_mutex_);
        latest_cv_.wait(
          lock, [this, processed_seq]() {
            return !running_.load() || latest_seq_ > processed_seq;
          });
        if (!running_.load()) {
          break;
        }
        frame = std::move(latest_frame_);
        processed_seq = latest_seq_;
        // Leave an empty placeholder so capture can refill without copying old data.
        latest_frame_ = Frame{};
      }

      if (frame.rgb.empty()) {
        continue;
      }

      std::vector<Detection> car_dets;
      std::vector<Detection> armor_dets;
      if (enable_detect_ && car_detector_) {
        const cv::Mat rgb(frame.height, frame.width, CV_8UC3, frame.rgb.data());
        cv::Mat bgr;
        cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
        car_dets = car_detector_->infer(bgr);
        if (enable_armor_ && armor_detector_) {
          for (const auto & car : car_dets) {
            auto armors = armor_detector_->infer_roi(bgr, car, 0.1f);
            armor_dets.insert(armor_dets.end(), armors.begin(), armors.end());
          }
        }
        maybe_dump(bgr, car_dets, armor_dets);
        if (show_image_) {
          show_frame(bgr, car_dets, armor_dets);
        }
      } else if (show_image_) {
        show_frame(frame);
      }

      {
        std::lock_guard<std::mutex> lock(result_mutex_);
        latest_car_dets_ = std::move(car_dets);
        latest_armor_dets_ = std::move(armor_dets);
        if (car_detector_) {
          last_max_score_ = car_detector_->last_max_score();
          last_max_class_ = car_detector_->last_max_class();
        }
        ++detect_count_;
      }
    }
  }

  void publish_targets()
  {
    uint64_t count = 0;
    uint64_t detect_count = 0;
    size_t cars = 0;
    size_t armors = 0;
    int width = 0;
    int height = 0;
    float max_score = 0.0f;
    std::vector<Detection> armor_dets;
    {
      std::lock_guard<std::mutex> lock(latest_mutex_);
      count = frame_count_;
      width = latest_width_;
      height = latest_height_;
    }
    {
      std::lock_guard<std::mutex> lock(result_mutex_);
      detect_count = detect_count_;
      cars = latest_car_dets_.size();
      armors = latest_armor_dets_.size();
      max_score = last_max_score_;
      armor_dets = latest_armor_dets_;
    }

    const auto stamp = this->now();
    radar_interfaces::msg::TargetArray msg;
    msg.header.stamp = stamp;
    msg.header.frame_id = "map";
    msg.targets.resize(kNumSlots);
    for (size_t i = 0; i < kNumSlots; ++i) {
      auto & t = msg.targets[i];
      t.stamp = stamp;
      t.robot_id = static_cast<uint8_t>(i);
      t.x_cm = 0;
      t.y_cm = 0;
      t.confidence = 0.0f;
      t.source = radar_interfaces::msg::Target::SOURCE_VISION;
      t.observed = false;
    }

    size_t observed = 0;
    for (const auto & det : armor_dets) {
      const int slot = armor_label_to_slot(det.label, our_is_blue_);
      if (slot < 0 || static_cast<size_t>(slot) >= kNumSlots) {
        continue;
      }
      auto & t = msg.targets[static_cast<size_t>(slot)];
      if (t.observed && det.score <= t.confidence) {
        continue;
      }
      const bool first = !t.observed;
      t.observed = true;
      t.confidence = det.score;
      t.source = radar_interfaces::msg::Target::SOURCE_VISION;
      // Field pose filled after Mid-70 box depth + extrinsic; keep 0 for now.
      t.x_cm = 0;
      t.y_cm = 0;
      if (first) {
        ++observed;
      }
    }
    targets_pub_->publish(msg);

    const auto now = std::chrono::steady_clock::now();
    const double dt = std::chrono::duration<double>(now - last_fps_time_).count();
    if (count == 0) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "Camera frame not received.");
    } else if (dt >= 1.0) {
      const double fps = static_cast<double>(count - last_fps_count_) / dt;
      const double det_fps = static_cast<double>(detect_count - last_detect_count_) / dt;
      RCLCPP_INFO(
        get_logger(),
        "Camera %dx%d grab %.1f fps detect %.1f fps cars=%zu armors=%zu "
        "targets=%zu/%zu car_max=%.3f clouds=%lu",
        width, height, fps, det_fps, cars, armors, observed, kNumSlots, max_score,
        cloud_count_);
      last_fps_count_ = count;
      last_detect_count_ = detect_count;
      last_fps_time_ = now;
    }
  }

  void show_frame(const Frame & frame)
  {
    if (frame.rgb.empty()) {
      return;
    }
    const cv::Mat rgb(
      frame.height, frame.width, CV_8UC3, const_cast<uint8_t *>(frame.rgb.data()));
    cv::Mat bgr;
    cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
    show_frame(bgr, {}, {});
  }

  void show_frame(
    cv::Mat & bgr, const std::vector<Detection> & cars, const std::vector<Detection> & armors)
  {
    draw_dets(bgr, cars, armors);
    cv::imshow("radar_vision", bgr);
    cv::waitKey(1);
  }

  static void draw_dets(
    cv::Mat & bgr, const std::vector<Detection> & cars, const std::vector<Detection> & armors)
  {
    for (const auto & det : cars) {
      cv::rectangle(
        bgr,
        cv::Point(static_cast<int>(det.x1), static_cast<int>(det.y1)),
        cv::Point(static_cast<int>(det.x2), static_cast<int>(det.y2)),
        cv::Scalar(0, 255, 0), 2);
      cv::putText(
        bgr, det.label + " " + cv::format("%.2f", det.score),
        cv::Point(static_cast<int>(det.x1), std::max(0, static_cast<int>(det.y1) - 5)),
        cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
    }
    for (const auto & det : armors) {
      const bool blue = !det.label.empty() && det.label[0] == 'B';
      const cv::Scalar color = blue ? cv::Scalar(255, 128, 0) : cv::Scalar(0, 0, 255);
      cv::rectangle(
        bgr,
        cv::Point(static_cast<int>(det.x1), static_cast<int>(det.y1)),
        cv::Point(static_cast<int>(det.x2), static_cast<int>(det.y2)),
        color, 2);
      cv::putText(
        bgr, det.label + " " + cv::format("%.2f", det.score),
        cv::Point(static_cast<int>(det.x1), std::max(0, static_cast<int>(det.y1) - 5)),
        cv::FONT_HERSHEY_SIMPLEX, 0.55, color, 2);
    }
  }

  void maybe_dump(
    const cv::Mat & bgr,
    const std::vector<Detection> & cars,
    const std::vector<Detection> & armors)
  {
    if (dump_dir_.empty()) {
      return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - last_dump_time_ < std::chrono::seconds(1)) {
      return;
    }
    last_dump_time_ = now;
    std::error_code ec;
    std::filesystem::create_directories(dump_dir_, ec);
    cv::Mat vis = bgr.clone();
    draw_dets(vis, cars, armors);
    cv::imwrite(dump_dir_ + "/latest.jpg", vis);
  }

  std::unique_ptr<FrameSource> frame_source_;
  std::unique_ptr<CarDetector> car_detector_;
  std::unique_ptr<ArmorDetector> armor_detector_;
  std::string targets_topic_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr cloud_sub_;
  rclcpp::Publisher<radar_interfaces::msg::TargetArray>::SharedPtr targets_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::thread capture_thread_;
  std::thread detect_thread_;

  std::mutex latest_mutex_;
  std::condition_variable latest_cv_;
  Frame latest_frame_;
  uint64_t latest_seq_{0};
  int latest_width_{0};
  int latest_height_{0};

  std::mutex result_mutex_;
  std::vector<Detection> latest_car_dets_;
  std::vector<Detection> latest_armor_dets_;
  float last_max_score_{0.0f};
  int last_max_class_{-1};

  int image_width_{1440};
  int image_height_{1080};
  bool show_image_{false};
  bool enable_detect_{true};
  bool enable_armor_{true};
  bool our_is_blue_{true};
  std::string dump_dir_;
  std::atomic<bool> running_{false};
  uint64_t cloud_count_{0};
  uint64_t frame_count_{0};
  uint64_t detect_count_{0};
  uint64_t last_fps_count_{0};
  uint64_t last_detect_count_{0};
  std::chrono::steady_clock::time_point last_fps_time_{std::chrono::steady_clock::now()};
  std::chrono::steady_clock::time_point last_dump_time_{};
};

}  // namespace radar_vision

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<radar_vision::VisionNode>());
  rclcpp::shutdown();
  return 0;
}
