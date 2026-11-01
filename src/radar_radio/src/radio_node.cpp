#include "rclcpp/rclcpp.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("radar_radio");
  RCLCPP_INFO(
    node->get_logger(),
    "Phase 2 placeholder. This node does not demodulate 0x0A01-0x0A06 yet.");
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
