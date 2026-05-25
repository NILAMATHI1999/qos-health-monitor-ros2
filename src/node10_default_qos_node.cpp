#include <chrono>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

class Node10QoSMismatch : public rclcpp::Node
{
public:
  Node10QoSMismatch() : Node("node10_qos_mismatch_node")
  {
    auto qos = rclcpp::QoS(10);
    qos.best_effort();

    heartbeat_pub_ = this->create_publisher<std_msgs::msg::String>(
      "/node10/heartbeat", qos);

    register_pub_ = this->create_publisher<std_msgs::msg::String>(
      "/register_node", 10);

    timer_ = this->create_wall_timer(
      500ms,
      std::bind(&Node10QoSMismatch::timer_callback, this));

    RCLCPP_INFO(this->get_logger(), "Node 10 QoS mismatch node started");
  }

private:
  void timer_callback()
  {
    if (registration_count_ < 5)
    {
      std_msgs::msg::String register_msg;

      register_msg.data =
        "Node 10|/node10/heartbeat|NODE10_QOS_MISMATCH_FAILURE|"
        "reliability=reliable;"
        "history=keep_last;"
        "depth=10;"
        "durability=volatile";

      register_pub_->publish(register_msg);
      registration_count_++;
    }

    std_msgs::msg::String heartbeat_msg;
    heartbeat_msg.data = "node10 alive with best_effort publisher";

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
  rclcpp::spin(std::make_shared<Node10QoSMismatch>());
  rclcpp::shutdown();
  return 0;
}
