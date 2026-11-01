cmake_minimum_required(VERSION 3.16)
project(radar_fusion)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(radar_interfaces REQUIRED)

add_executable(radar_fusion_node src/fusion_node.cpp)
ament_target_dependencies(radar_fusion_node rclcpp radar_interfaces)

install(TARGETS radar_fusion_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY config DESTINATION share/${PROJECT_NAME})

ament_package()
