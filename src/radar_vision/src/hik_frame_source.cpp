#include "radar_vision/frame_source.hpp"

#include <chrono>
#include <string>
#include <thread>

#include "MvCameraControl.h"
#include "rclcpp/rclcpp.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace radar_vision
{
namespace
{

rclcpp::Logger camera_logger()
{
  return rclcpp::get_logger("radar_vision");
}

const char * text_field(const unsigned char * field)
{
  return reinterpret_cast<const char *>(field);
}

}  // namespace

class HikFrameSource : public FrameSource
{
public:
  explicit HikFrameSource(const FrameSourceConfig & config)
  : config_(config)
  {
  }

  ~HikFrameSource() override
  {
    close();
  }

  bool grab(Frame & frame) override
  {
    frame = Frame{};
    if (!grabbing_ && !open_throttled()) {
      return false;
    }

    MV_FRAME_OUT out_frame{};
    const int ret = MV_CC_GetImageBuffer(handle_, &out_frame, 1000);
    if (ret != MV_OK) {
      RCLCPP_WARN(
        camera_logger(), "MV_CC_GetImageBuffer failed: 0x%x", static_cast<unsigned int>(ret));
      return false;
    }

    frame.width = static_cast<int>(out_frame.stFrameInfo.nWidth);
    frame.height = static_cast<int>(out_frame.stFrameInfo.nHeight);
    frame.rgb.resize(static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height) * 3);

    MV_CC_PIXEL_CONVERT_PARAM convert{};
    convert.nWidth = out_frame.stFrameInfo.nWidth;
    convert.nHeight = out_frame.stFrameInfo.nHeight;
    convert.enSrcPixelType = out_frame.stFrameInfo.enPixelType;
    convert.pSrcData = out_frame.pBufAddr;
    convert.nSrcDataLen = out_frame.stFrameInfo.nFrameLen;
    convert.enDstPixelType = PixelType_Gvsp_RGB8_Packed;
    convert.pDstBuffer = frame.rgb.data();
    convert.nDstBufferSize = static_cast<unsigned int>(frame.rgb.size());

    const int convert_ret = MV_CC_ConvertPixelType(handle_, &convert);
    MV_CC_FreeImageBuffer(handle_, &out_frame);
    if (convert_ret != MV_OK) {
      frame = Frame{};
      RCLCPP_WARN(
        camera_logger(), "MV_CC_ConvertPixelType failed: 0x%x",
        static_cast<unsigned int>(convert_ret));
      return false;
    }
    return true;
  }

private:
  bool open_throttled()
  {
    const auto now = std::chrono::steady_clock::now();
    if (now - last_open_attempt_ < std::chrono::seconds(2)) {
      return false;
    }
    last_open_attempt_ = now;
    return open();
  }

  bool open()
  {
    MV_CC_DEVICE_INFO_LIST devices{};
    const int enum_ret = MV_CC_EnumDevices(MV_USB_DEVICE, &devices);
    if (enum_ret != MV_OK || devices.nDeviceNum == 0) {
      RCLCPP_ERROR(
        camera_logger(), "No USB camera. MV_CC_EnumDevices=0x%x count=%u",
        static_cast<unsigned int>(enum_ret), devices.nDeviceNum);
      return false;
    }

    MV_CC_DEVICE_INFO * info = devices.pDeviceInfo[0];
    const int create_ret = MV_CC_CreateHandleWithoutLog(&handle_, info);
    if (create_ret != MV_OK || handle_ == nullptr) {
      RCLCPP_ERROR(
        camera_logger(), "MV_CC_CreateHandleWithoutLog failed: 0x%x",
        static_cast<unsigned int>(create_ret));
      handle_ = nullptr;
      return false;
    }

    const auto & usb = info->SpecialInfo.stUsb3VInfo;
    RCLCPP_INFO(
      camera_logger(), "Opening USB camera model=%s serial=%s",
      text_field(usb.chModelName), text_field(usb.chSerialNumber));

    const int open_ret = MV_CC_OpenDevice(handle_);
    if (open_ret != MV_OK) {
      RCLCPP_ERROR(
        camera_logger(), "MV_CC_OpenDevice failed: 0x%x", static_cast<unsigned int>(open_ret));
      close();
      return false;
    }

    set_int("OffsetX", 0);
    set_int("OffsetY", 0);
    set_int("Width", static_cast<unsigned int>(config_.width));
    set_int("Height", static_cast<unsigned int>(config_.height));
    const int trigger_ret = MV_CC_SetEnumValue(handle_, "TriggerMode", 0);
    if (trigger_ret != MV_OK) {
      RCLCPP_WARN(
        camera_logger(), "TriggerMode off failed: 0x%x", static_cast<unsigned int>(trigger_ret));
    }
    const int pixel_ret = MV_CC_SetPixelFormat(handle_, PixelType_Gvsp_BayerRG8);
    if (pixel_ret != MV_OK) {
      RCLCPP_WARN(
        camera_logger(), "BayerRG8 pixel format failed: 0x%x",
        static_cast<unsigned int>(pixel_ret));
    }
    const int exposure_ret = MV_CC_SetFloatValue(handle_, "ExposureTime", config_.exposure_us);
    if (exposure_ret != MV_OK) {
      RCLCPP_WARN(
        camera_logger(), "ExposureTime failed: 0x%x", static_cast<unsigned int>(exposure_ret));
    }
    const int gain_ret = MV_CC_SetFloatValue(handle_, "Gain", config_.gain);
    if (gain_ret != MV_OK) {
      RCLCPP_WARN(camera_logger(), "Gain failed: 0x%x", static_cast<unsigned int>(gain_ret));
    }
    const int rate_enable_ret = MV_CC_SetBoolValue(handle_, "AcquisitionFrameRateEnable", true);
    if (rate_enable_ret != MV_OK) {
      RCLCPP_WARN(
        camera_logger(), "AcquisitionFrameRateEnable failed: 0x%x",
        static_cast<unsigned int>(rate_enable_ret));
    }
    const int rate_ret = MV_CC_SetFloatValue(
      handle_, "AcquisitionFrameRate", static_cast<float>(config_.frame_rate_hz));
    if (rate_ret != MV_OK) {
      RCLCPP_WARN(
        camera_logger(), "AcquisitionFrameRate %.1f failed: 0x%x",
        config_.frame_rate_hz, static_cast<unsigned int>(rate_ret));
    }

    const int start_ret = MV_CC_StartGrabbing(handle_);
    if (start_ret != MV_OK) {
      RCLCPP_ERROR(
        camera_logger(), "MV_CC_StartGrabbing failed: 0x%x", static_cast<unsigned int>(start_ret));
      close();
      return false;
    }
    grabbing_ = true;
    return true;
  }

  void set_int(const char * key, unsigned int value)
  {
    const int ret = MV_CC_SetIntValue(handle_, key, value);
    if (ret != MV_OK) {
      RCLCPP_WARN(
        camera_logger(), "Set %s=%u failed: 0x%x", key, value, static_cast<unsigned int>(ret));
    }
  }

  void close()
  {
    if (handle_ == nullptr) {
      grabbing_ = false;
      return;
    }
    if (grabbing_) {
      MV_CC_StopGrabbing(handle_);
    }
    MV_CC_CloseDevice(handle_);
    MV_CC_DestroyHandle(handle_);
    handle_ = nullptr;
    grabbing_ = false;
  }

  FrameSourceConfig config_;
  void * handle_{nullptr};
  bool grabbing_{false};
  std::chrono::steady_clock::time_point last_open_attempt_{};
};

class FileFrameSource : public FrameSource
{
public:
  explicit FileFrameSource(const FrameSourceConfig & config)
  {
    const double hz = config.frame_rate_hz > 0.0 ? config.frame_rate_hz : 30.0;
    period_ = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
      std::chrono::duration<double>(1.0 / hz));
    cv::Mat bgr = cv::imread(config.image_file, cv::IMREAD_COLOR);
    if (bgr.empty()) {
      RCLCPP_ERROR(camera_logger(), "Failed to read image_file=%s", config.image_file.c_str());
      return;
    }
    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
    frame_.width = rgb.cols;
    frame_.height = rgb.rows;
    frame_.rgb.assign(rgb.datastart, rgb.dataend);
    RCLCPP_INFO(
      camera_logger(), "Offline image %dx%d from %s",
      frame_.width, frame_.height, config.image_file.c_str());
  }

  bool grab(Frame & frame) override
  {
    if (frame_.rgb.empty()) {
      return false;
    }
    const auto now = std::chrono::steady_clock::now();
    if (next_frame_time_.time_since_epoch().count() != 0 && now < next_frame_time_) {
      std::this_thread::sleep_until(next_frame_time_);
    }
    next_frame_time_ = std::chrono::steady_clock::now() + period_;
    frame = frame_;
    return true;
  }

private:
  Frame frame_;
  std::chrono::steady_clock::duration period_{};
  std::chrono::steady_clock::time_point next_frame_time_{};
};

std::unique_ptr<FrameSource> create_frame_source(const FrameSourceConfig & config)
{
  if (!config.image_file.empty()) {
    return std::make_unique<FileFrameSource>(config);
  }
  return std::make_unique<HikFrameSource>(config);
}

}  // namespace radar_vision
