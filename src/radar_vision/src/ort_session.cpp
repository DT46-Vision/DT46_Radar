#include "radar_vision/ort_session.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace radar_vision
{
namespace
{

bool nvidia_device_present()
{
  namespace fs = std::filesystem;
  std::error_code ec;
  if (fs::exists("/dev/nvidia0", ec) && !ec) {
    return true;
  }
  const int rc = std::system("nvidia-smi -L >/dev/null 2>&1");
  return rc == 0;
}

bool ort_lists_cuda()
{
  try {
    for (const auto & name : Ort::GetAvailableProviders()) {
      if (name == "CUDAExecutionProvider") {
        return true;
      }
    }
  } catch (...) {
    return false;
  }
  return false;
}

Ort::SessionOptions make_cpu_options(int intra_op_threads)
{
  Ort::SessionOptions options;
  options.SetIntraOpNumThreads(intra_op_threads);
  options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
  return options;
}

Ort::SessionOptions make_cuda_options(int intra_op_threads)
{
  Ort::SessionOptions options = make_cpu_options(intra_op_threads);
  OrtCUDAProviderOptions cuda_opts{};
  cuda_opts.device_id = 0;
  options.AppendExecutionProvider_CUDA(cuda_opts);
  return options;
}

}  // namespace

OrtSessionFactory & OrtSessionFactory::instance()
{
  static OrtSessionFactory factory;
  return factory;
}

OrtSessionFactory::OrtSessionFactory()
: env_(ORT_LOGGING_LEVEL_WARNING, "radar_vision")
{
  prefer_cuda_ = nvidia_device_present() && ort_lists_cuda();
  if (prefer_cuda_) {
    std::cerr << "[radar_vision] NVIDIA device + CUDA EP available; will try CUDA\n";
  } else if (!nvidia_device_present()) {
    std::cerr << "[radar_vision] No NVIDIA device; ORT will use CPU\n";
  } else {
    std::cerr <<
      "[radar_vision] GPU present but ORT has no CUDA EP "
      "(need onnxruntime-linux-x64-gpu package); ORT will use CPU\n";
  }
}

Ort::Env & OrtSessionFactory::env()
{
  return env_;
}

std::unique_ptr<Ort::Session> OrtSessionFactory::create_session(
  const std::string & model_path, int intra_op_threads)
{
  if (prefer_cuda_) {
    try {
      auto options = make_cuda_options(intra_op_threads);
      auto session = std::make_unique<Ort::Session>(env_, model_path.c_str(), options);
      backend_ = "cuda";
      return session;
    } catch (const Ort::Exception & ex) {
      std::cerr << "[radar_vision] CUDA session failed (" << ex.what()
                << "); falling back to CPU for " << model_path << '\n';
    }
  }

  auto options = make_cpu_options(intra_op_threads);
  auto session = std::make_unique<Ort::Session>(env_, model_path.c_str(), options);
  backend_ = "cpu";
  return session;
}

}  // namespace radar_vision
