cmake_minimum_required(VERSION 3.16)
project(radar_radio)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)

add_executable(radar_radio_node src/radio_node.cpp)
ament_target_dependencies(radar_radio_node rclcpp)

install(TARGETS radar_radio_node DESTINATION lib/${PROJECT_NAME})

ament_package()
