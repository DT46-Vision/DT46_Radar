cmake_minimum_required(VERSION 3.16)
project(radar_vision)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(radar_interfaces REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(camera_calibration_parsers REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc calib3d highgui imgcodecs)

# Link against hikSDK under src/radar_hik_SDK (headers/libs only; not a ROS node).
set(_HIK_SDK_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/../radar_hik_SDK/hikSDK")
if(EXISTS "${_HIK_SDK_DEFAULT}/include/MvCameraControl.h")
  set(HIK_SDK_DIR "${_HIK_SDK_DEFAULT}" CACHE PATH
    "Hikrobot MVS tree for radar_vision link only." FORCE)
endif()
if(NOT EXISTS "${HIK_SDK_DIR}/include/MvCameraControl.h")
  message(FATAL_ERROR "Hikrobot MVS headers not found: ${HIK_SDK_DIR}")
endif()


if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
  set(HIK_SDK_LIB "${HIK_SDK_DIR}/lib/arm64")
else()
  set(HIK_SDK_LIB "${HIK_SDK_DIR}/lib/amd64")
endif()

# Prefer CUDA13 ORT GPU (matches driver CUDA 13 + pip nvidia/cu13). Then CUDA12 GPU, then CPU.
set(_ORT_ROOT "${CMAKE_SOURCE_DIR}/../../third_party")
set(_ORT_GPU13 "${_ORT_ROOT}/onnxruntime-linux-x64-gpu-1.28.0")
set(_ORT_GPU12 "${_ORT_ROOT}/onnxruntime-linux-x64-gpu-1.19.2")
set(_ORT_CPU "${_ORT_ROOT}/onnxruntime-linux-x64-1.19.2")
if(EXISTS "${_ORT_GPU13}/include/onnxruntime_cxx_api.h" AND
   EXISTS "${_ORT_GPU13}/lib/libonnxruntime_providers_cuda.so")
  set(ONNXRUNTIME_ROOT "${_ORT_GPU13}" CACHE PATH "Local ONNX Runtime C++ package (not committed)." FORCE)
  set(RADAR_VISION_ORT_CUDA_MAJOR 13)
elseif(EXISTS "${_ORT_GPU12}/include/onnxruntime_cxx_api.h" AND
       EXISTS "${_ORT_GPU12}/lib/libonnxruntime_providers_cuda.so")
  set(ONNXRUNTIME_ROOT "${_ORT_GPU12}" CACHE PATH "Local ONNX Runtime C++ package (not committed)." FORCE)
  set(RADAR_VISION_ORT_CUDA_MAJOR 12)
else()
  set(ONNXRUNTIME_ROOT "${_ORT_CPU}" CACHE PATH "Local ONNX Runtime C++ package (not committed).")
  set(RADAR_VISION_ORT_CUDA_MAJOR 0)
endif()
if(NOT EXISTS "${ONNXRUNTIME_ROOT}/include/onnxruntime_cxx_api.h")
  message(FATAL_ERROR "ONNX Runtime not found at ${ONNXRUNTIME_ROOT}")
endif()
message(STATUS "ONNX Runtime: ${ONNXRUNTIME_ROOT} (cuda_major=${RADAR_VISION_ORT_CUDA_MAJOR})")

# Pip NVIDIA CUDA / cuDNN libs. Match ORT CUDA major; do not mix 12 and 13 on RPATH.
# Discover site-packages/nvidia via python3 (any minor), then ~/.local glob fallback.
set(_NVIDIA_PY "")
find_program(RADAR_VISION_PYTHON3 NAMES python3)
if(RADAR_VISION_PYTHON3)
  execute_process(
    COMMAND "${RADAR_VISION_PYTHON3}" -c
      "from pathlib import Path; import site;\nroots=[]\ntry:\n roots.append(Path(site.getusersitepackages()))\nexcept Exception:\n pass\ntry:\n roots += [Path(p) for p in site.getsitepackages()]\nexcept Exception:\n pass\nfor r in roots:\n n=r/'nvidia'\n if n.is_dir():\n  print(n); break"
    OUTPUT_VARIABLE _NVIDIA_PY
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
endif()
if(NOT _NVIDIA_PY)
  file(GLOB _NVIDIA_GLOB
    "$ENV{HOME}/.local/lib/python*/site-packages/nvidia"
    "$ENV{HOME}/.local/lib/python*/dist-packages/nvidia")
  if(_NVIDIA_GLOB)
    list(GET _NVIDIA_GLOB 0 _NVIDIA_PY)
  endif()
endif()
message(STATUS "NVIDIA pip libs root: ${_NVIDIA_PY}")

set(RADAR_VISION_CUDA_RPATH "")
if(RADAR_VISION_ORT_CUDA_MAJOR EQUAL 13)
  foreach(_sub IN ITEMS cu13/lib cudnn/lib)
    if(_NVIDIA_PY AND EXISTS "${_NVIDIA_PY}/${_sub}")
      list(APPEND RADAR_VISION_CUDA_RPATH "${_NVIDIA_PY}/${_sub}")
    endif()
  endforeach()
elseif(RADAR_VISION_ORT_CUDA_MAJOR EQUAL 12)
  foreach(_sub IN ITEMS
      cuda_runtime/lib cublas/lib cufft/lib curand/lib cudnn/lib cuda_nvrtc/lib)
    if(_NVIDIA_PY AND EXISTS "${_NVIDIA_PY}/${_sub}")
      list(APPEND RADAR_VISION_CUDA_RPATH "${_NVIDIA_PY}/${_sub}")
    endif()
  endforeach()
endif()
list(REMOVE_DUPLICATES RADAR_VISION_CUDA_RPATH)


add_executable(radar_vision_node
  src/vision_node.cpp
  src/camera_intrinsics.cpp
  src/hik_frame_source.cpp
  src/ort_session.cpp
  src/car_detector.cpp
  src/armor_detector.cpp
)
target_include_directories(radar_vision_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  "${HIK_SDK_DIR}/include"
  "${ONNXRUNTIME_ROOT}/include"
)
target_link_directories(radar_vision_node PRIVATE
  "${HIK_SDK_LIB}"
  "${ONNXRUNTIME_ROOT}/lib"
)
target_link_libraries(radar_vision_node
  MvCameraControl
  FormatConversion
  MediaProcess
  MVRender
  MvUsb3vTL
  onnxruntime
  ${OpenCV_LIBS}
)
target_link_options(radar_vision_node PRIVATE "-Wl,--disable-new-dtags")

set(_RPATH_LIST "${HIK_SDK_LIB};${ONNXRUNTIME_ROOT}/lib")
foreach(_p IN LISTS RADAR_VISION_CUDA_RPATH)
  string(APPEND _RPATH_LIST ";${_p}")
endforeach()
set_target_properties(radar_vision_node PROPERTIES
  BUILD_RPATH "${_RPATH_LIST}"
  INSTALL_RPATH "${_RPATH_LIST}"
)
ament_target_dependencies(radar_vision_node
  rclcpp
  sensor_msgs
  radar_interfaces
  ament_index_cpp
  camera_calibration_parsers
)

install(TARGETS radar_vision_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY config launch model DESTINATION share/${PROJECT_NAME})

ament_package()
