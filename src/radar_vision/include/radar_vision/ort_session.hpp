#ifndef RADAR_VISION__ORT_SESSION_HPP_
#define RADAR_VISION__ORT_SESSION_HPP_

#include <memory>
#include <string>

#include <onnxruntime_cxx_api.h>

namespace radar_vision
{

// Shared ORT env. Prefers CUDA when ORT exposes CUDAExecutionProvider and an
// NVIDIA device is present; otherwise creates a CPU session.
class OrtSessionFactory
{
public:
  static OrtSessionFactory & instance();

  Ort::Env & env();
  // Last successful backend for sessions created through create_session().
  const std::string & backend() const {return backend_;}
  bool using_cuda() const {return backend_ == "cuda";}
  bool prefers_cuda() const {return prefer_cuda_;}

  // Try CUDA session first when hardware + ORT EP allow it; fall back to CPU.
  std::unique_ptr<Ort::Session> create_session(
    const std::string & model_path, int intra_op_threads = 2);

private:
  OrtSessionFactory();
  Ort::Env env_;
  std::string backend_{"cpu"};
  bool prefer_cuda_{false};
};

}  // namespace radar_vision

#endif  // RADAR_VISION__ORT_SESSION_HPP_
