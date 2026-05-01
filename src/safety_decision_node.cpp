#include <chrono>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float32.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

class SafetyDecisionNode : public rclcpp::Node
{
public:
  SafetyDecisionNode() : Node("safety_decision_node")
  {
    // Match Node A QoS
    auto data_a_qos = rclcpp::QoS(rclcpp::KeepLast(10));
    data_a_qos.reliable();
    data_a_qos.deadline(100ms);
    data_a_qos.liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC);
    data_a_qos.liveliness_lease_duration(300ms);

    // Match Node B QoS
    auto data_b_qos = rclcpp::QoS(rclcpp::KeepLast(10));
    data_b_qos.reliable();
    data_b_qos.deadline(500ms);
    data_b_qos.liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC);
    data_b_qos.liveliness_lease_duration(1000ms);

    // Node 3 output QoS
    auto node3_qos = rclcpp::QoS(rclcpp::KeepLast(10));
    node3_qos.reliable();
    node3_qos.deadline(200ms);
    node3_qos.liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC);
    node3_qos.liveliness_lease_duration(500ms);

    data_a_sub_ = this->create_subscription<std_msgs::msg::Float32>(
      "/data_a", data_a_qos,
      std::bind(&SafetyDecisionNode::data_a_callback, this, std::placeholders::_1));

    data_b_sub_ = this->create_subscription<std_msgs::msg::Float32>(
      "/data_b", data_b_qos,
      std::bind(&SafetyDecisionNode::data_b_callback, this, std::placeholders::_1));

    status_pub_ = this->create_publisher<std_msgs::msg::String>("/system_status", node3_qos);
    speed_pub_ = this->create_publisher<std_msgs::msg::Float32>("/adjusted_speed", node3_qos);
    heartbeat_pub_ = this->create_publisher<std_msgs::msg::String>("/node3/heartbeat", node3_qos);

    timer_ = this->create_wall_timer(
      200ms,
      std::bind(&SafetyDecisionNode::timer_callback, this));

    RCLCPP_INFO(this->get_logger(), "Safety Decision node started");
  }

private:
  void data_a_callback(const std_msgs::msg::Float32::SharedPtr msg)
  {
    distance_ = msg->data;
  }

  void data_b_callback(const std_msgs::msg::Float32::SharedPtr msg)
  {
    normal_speed_ = msg->data;
  }

  void timer_callback()
  {
    const float safety_radius = 2.0f;

    std_msgs::msg::String status_msg;
    std_msgs::msg::Float32 speed_msg;

    if (distance_ >= safety_radius)
    {
      speed_msg.data = normal_speed_;
      status_msg.data = "SAFE_NORMAL_SPEED";
    }
    else
    {
      float reduced_speed = normal_speed_ * (distance_ / safety_radius);
      speed_msg.data = reduced_speed;
      status_msg.data = "UNSAFE_REDUCE_SPEED";
    }

    status_pub_->publish(status_msg);
    speed_pub_->publish(speed_msg);

    std_msgs::msg::String heartbeat_msg;
    heartbeat_msg.data = "safety_decision_node alive";
    heartbeat_pub_->publish(heartbeat_msg);

    bool heartbeat_ok = heartbeat_pub_->assert_liveliness();
    if (!heartbeat_ok) {
      RCLCPP_WARN(this->get_logger(), "Failed to assert Node 3 heartbeat liveliness");
    }

    RCLCPP_INFO(
      this->get_logger(),
      "Distance=%.2f | Radius=%.2f | Normal speed =%.2f | Adjusted speed =%.2f | Status=%s",
      distance_,
      safety_radius,
      normal_speed_,
      speed_msg.data,
      status_msg.data.c_str());
  }

  rclcpp::Subscription<std_msgs::msg::Float32>::SharedPtr data_a_sub_;
  rclcpp::Subscription<std_msgs::msg::Float32>::SharedPtr data_b_sub_;

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_;
  rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr speed_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr heartbeat_pub_;

  rclcpp::TimerBase::SharedPtr timer_;

  float distance_ = 2.0f;
  float normal_speed_ = 0.5f;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SafetyDecisionNode>());
  rclcpp::shutdown();
  return 0;
}
