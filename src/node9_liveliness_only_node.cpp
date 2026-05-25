#include <chrono>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

class Node9LivelinessOnly : public rclcpp::Node
{
public:
  Node9LivelinessOnly() : Node("node9_liveliness_only_node")
  {
    auto qos = rclcpp::QoS(10);
    qos.reliable();
    qos.liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC);
    qos.liveliness_lease_duration(1000ms);

    heartbeat_pub_ = this->create_publisher<std_msgs::msg::String>(
      "/node9/heartbeat", qos);

    register_pub_ = this->create_publisher<std_msgs::msg::String>(
      "/register_node", 10);

    timer_ = this->create_wall_timer(
      500ms,
      std::bind(&Node9LivelinessOnly::timer_callback, this));

    RCLCPP_INFO(this->get_logger(), "Node 9 liveliness-only node started");
  }

private:
  void timer_callback()
  {
    if (registration_count_ < 5)
    {
      std_msgs::msg::String register_msg;

      register_msg.data =
        "Node 9|/node9/heartbeat|NODE9_LIVELINESS_FAILURE|"
        "reliability=reliable;"
        "history=keep_last;"
        "depth=10;"
        "durability=volatile;"
        "liveliness=manual_by_topic;"
        "liveliness_lease_ms=1000";

      register_pub_->publish(register_msg);
      registration_count_++;
    }

    std_msgs::msg::String heartbeat_msg;
    heartbeat_msg.data = "node9 alive";

    heartbeat_pub_->publish(heartbeat_msg);
    heartbeat_pub_->assert_liveliness();
  }

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr heartbeat_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr register_pub_;
  rclcpp::TimerBase::SharedPtr timer_;

  int registration_count_ = 0;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Node9LivelinessOnly>());
  rclcpp::shutdown();
  return 0;
}
