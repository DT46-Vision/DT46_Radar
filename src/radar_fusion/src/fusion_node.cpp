#include <string>

#include "radar_interfaces/msg/target_array.hpp"
#include "rclcpp/rclcpp.hpp"

namespace radar_fusion
{

class FusionNode : public rclcpp::Node
{
public:
  FusionNode()
  : Node("radar_fusion")
  {
    const auto targets_topic = declare_parameter<std::string>("targets_topic", "/radar/targets");
    const auto marks_topic = declare_parameter<std::string>("marks_topic", "/radar/marks");

    marks_pub_ = create_publisher<radar_interfaces::msg::TargetArray>(marks_topic, rclcpp::QoS(10));
    targets_sub_ = create_subscription<radar_interfaces::msg::TargetArray>(
      targets_topic, rclcpp::QoS(10),
      [this](radar_interfaces::msg::TargetArray::ConstSharedPtr msg) {
        marks_pub_->publish(*msg);
      });

    RCLCPP_INFO(
      get_logger(),
      "Phase 1 forwards %s to %s. Serial stays on XDU rm_referee /rm_referee/tx.",
      targets_topic.c_str(), marks_topic.c_str());
  }

private:
  rclcpp::Publisher<radar_interfaces::msg::TargetArray>::SharedPtr marks_pub_;
  rclcpp::Subscription<radar_interfaces::msg::TargetArray>::SharedPtr targets_sub_;
};

}  // namespace radar_fusion

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<radar_fusion::FusionNode>());
  rclcpp::shutdown();
  return 0;
}
