#ifndef RADAR_VISION__FRAME_SOURCE_HPP_
#define RADAR_VISION__FRAME_SOURCE_HPP_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace radar_vision
{

struct Frame
{
  std::vector<uint8_t> rgb;
  int width{0};
  int height{0};
};

struct FrameSourceConfig
{
  int width{1440};
  int height{1080};
  double exposure_us{8000.0};
  double gain{16.0};
  double frame_rate_hz{30.0};
  // Non-empty: read this image instead of the USB camera (offline ONNX check).
  std::string image_file;
};

// 取图在 radar_vision 进程内完成。海康实现应调用 MV_CC_GetImageBuffer，
// 需要的像素拷进 Frame 后立刻 MV_CC_FreeImageBuffer。不要把缓冲交给另一个节点。
class FrameSource
{
public:
  virtual ~FrameSource() = default;
  virtual bool grab(Frame & frame) = 0;
};

std::unique_ptr<FrameSource> create_frame_source(const FrameSourceConfig & config);

}  // namespace radar_vision

#endif  // RADAR_VISION__FRAME_SOURCE_HPP_
