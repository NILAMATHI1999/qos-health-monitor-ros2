
#include <chrono>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

class HealthMonitorNode : public rclcpp::Node
{
public:
  HealthMonitorNode() : Node("health_monitor_node")
  {
    health_pub_ = this->create_publisher<std_msgs::msg::String>("/health_status", 10);

    node1_sub_ = create_heartbeat_subscription(
      "/node1/heartbeat", "Node 1", 250ms, 500ms, &node1_alive_);

    node2_sub_ = create_heartbeat_subscription(
      "/node2/heartbeat", "Node 2", 800ms, 1500ms, &node2_alive_);

    node3_sub_ = create_heartbeat_subscription(
      "/node3/heartbeat", "Node 3", 400ms, 800ms, &node3_alive_);

    RCLCPP_INFO(this->get_logger(), "Health Monitor node started");
  }

private:
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr create_heartbeat_subscription(
    const std::string & topic_name,
    const std::string & node_name,
    std::chrono::milliseconds deadline_time,
    std::chrono::milliseconds liveliness_time,
    bool * alive_flag)
  {
    auto qos = rclcpp::QoS(rclcpp::KeepLast(10));
    qos.reliable();
    qos.deadline(deadline_time);
    qos.liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC);
    qos.liveliness_lease_duration(liveliness_time);

    rclcpp::SubscriptionOptions options;

    options.event_callbacks.deadline_callback =
      [this, node_name, alive_flag](rclcpp::QOSDeadlineRequestedInfo & event)
      {
        (void)event;
        *alive_flag = false;
        RCLCPP_ERROR(this->get_logger(), "%s deadline missed", node_name.c_str());
        publish_health_status();
      };

    options.event_callbacks.liveliness_callback =
      [this, node_name, alive_flag](rclcpp::QOSLivelinessChangedInfo & event)
      {
        if (event.alive_count == 0) {
          *alive_flag = false;
          RCLCPP_ERROR(this->get_logger(), "%s liveliness lost", node_name.c_str());
        } else {
          *alive_flag = true;
          RCLCPP_INFO(this->get_logger(), "%s liveliness active", node_name.c_str());
        }

        publish_health_status();
      };

    return this->create_subscription<std_msgs::msg::String>(
      topic_name,
      qos,
      [this, alive_flag](const std_msgs::msg::String::SharedPtr msg)
      {
        (void)msg;
        *alive_flag = true;
        publish_health_status();
      },
      options);
  }

  void publish_health_status()
  {
    std_msgs::msg::String health_msg;

    if (node1_alive_ && node2_alive_ && node3_alive_) {
      health_msg.data = "HEALTHY";
    } else {
      health_msg.data = "UNHEALTHY";
    }

    health_pub_->publish(health_msg);

    RCLCPP_INFO_THROTTLE(
      this->get_logger(),
      *this->get_clock(),
      2000,
      "Health Status: %s | Node1=%s Node2=%s Node3=%s",
      health_msg.data.c_str(),
      node1_alive_ ? "OK" : "FAILED",
      node2_alive_ ? "OK" : "FAILED",
      node3_alive_ ? "OK" : "FAILED");
  }

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr health_pub_;

  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr node1_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr node2_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr node3_sub_;

  bool node1_alive_ = false;
  bool node2_alive_ = false;
  bool node3_alive_ = false;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<HealthMonitorNode>());
  rclcpp::shutdown();
  return 0;
}
