#include <chrono>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

class Node8DeadlineOnly : public rclcpp::Node
{
public:
  Node8DeadlineOnly() : Node("node8_deadline_only_node")
  {
    auto qos = rclcpp::QoS(10);
    qos.reliable();
    qos.deadline(500ms);

    heartbeat_pub_ = this->create_publisher<std_msgs::msg::String>(
      "/node8/heartbeat", qos);

    register_pub_ = this->create_publisher<std_msgs::msg::String>(
      "/register_node", 10);

    timer_ = this->create_wall_timer(
      500ms,
      std::bind(&Node8DeadlineOnly::timer_callback, this));

    RCLCPP_INFO(this->get_logger(), "Node 8 deadline-only node started");
  }

private:
  void timer_callback()
  {
    if (registration_count_ < 5)
    {
      std_msgs::msg::String register_msg;

      register_msg.data =
        "Node 8|/node8/heartbeat|NODE8_DEADLINE_FAILURE|"
        "reliability=reliable;"
        "history=keep_last;"
        "depth=10;"
        "durability=volatile;"
        "deadline_ms=500";

      register_pub_->publish(register_msg);

      registration_count_++;
    }

    std_msgs::msg::String heartbeat_msg;
    heartbeat_msg.data = "node8 alive";

    heartbeat_pub_->publish(heartbeat_msg);
  }

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr heartbeat_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr register_pub_;

  rclcpp::TimerBase::SharedPtr timer_;

  int registration_count_ = 0;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Node8DeadlineOnly>());
  rclcpp::shutdown();
  return 0;
}
