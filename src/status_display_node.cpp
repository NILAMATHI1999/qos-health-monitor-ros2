#include <chrono>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float32.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

class StatusDisplayNode : public rclcpp::Node
{
public:
  StatusDisplayNode() : Node("status_display_node")
  {
    auto qos = rclcpp::QoS(rclcpp::KeepLast(10));
    qos.reliable();

    health_sub_ = this->create_subscription<std_msgs::msg::String>(
      "/health_status", qos,
      std::bind(&StatusDisplayNode::health_callback, this, std::placeholders::_1));

    reason_sub_ = this->create_subscription<std_msgs::msg::String>(
      "/health_reason", qos,
      std::bind(&StatusDisplayNode::reason_callback, this, std::placeholders::_1));

    system_sub_ = this->create_subscription<std_msgs::msg::String>(
      "/system_status", qos,
      std::bind(&StatusDisplayNode::system_callback, this, std::placeholders::_1));

    speed_sub_ = this->create_subscription<std_msgs::msg::Float32>(
      "/adjusted_speed", qos,
      std::bind(&StatusDisplayNode::speed_callback, this, std::placeholders::_1));

    timer_ = this->create_wall_timer(
      500ms,
      std::bind(&StatusDisplayNode::timer_callback, this));

  }

private:
  void health_callback(const std_msgs::msg::String::SharedPtr msg)
  {
    health_status_ = msg->data;
    last_health_time_ = this->now();
    received_health_ = true;
  }

  void reason_callback(const std_msgs::msg::String::SharedPtr msg)
  {
    health_reason_ = msg->data;
  }

  void system_callback(const std_msgs::msg::String::SharedPtr msg)
  {
    system_status_ = msg->data;
    received_system_ = true;
  }

  void speed_callback(const std_msgs::msg::Float32::SharedPtr msg)
  {
    adjusted_speed_ = msg->data;
    received_speed_ = true;
  }

  void timer_callback()
  {
    if (!received_system_ || !received_speed_) return;

    bool health_monitor_failed = false;

    if (!received_health_) {
      health_monitor_failed = true;
    } else if ((this->now() - last_health_time_).seconds() > 2.0) {
      health_monitor_failed = true;
    }

    if (health_monitor_failed)
    {
      RCLCPP_ERROR(this->get_logger(),
        "\033[31mRED | Health=UNHEALTHY | System=UNSAFE_STOP | Speed=0.00 | Action=STOP | Reason=HEALTH_MONITOR_FAILURE\033[0m");
    }
    else if (health_status_ == "UNHEALTHY")
    {
      RCLCPP_ERROR(this->get_logger(),
        "\033[31mRED | Health=UNHEALTHY | System=UNSAFE_STOP | Speed=0.00 | Action=STOP | Reason=%s\033[0m",
        health_reason_.c_str());
    }
    else if (system_status_.find("UNSAFE") != std::string::npos)
    {
      RCLCPP_WARN(this->get_logger(),
        "\033[33mYELLOW | Health=%s | System=%s | Speed=%.2f | Action=REDUCE_SPEED\033[0m",
        health_status_.c_str(), system_status_.c_str(), adjusted_speed_);
    }
    else
    {
      RCLCPP_INFO(this->get_logger(),
        "\033[32mGREEN | Health=%s | System=%s | Speed=%.2f | Action=NORMAL\033[0m",
        health_status_.c_str(), system_status_.c_str(), adjusted_speed_);
    }
  }

  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr health_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr reason_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr system_sub_;
  rclcpp::Subscription<std_msgs::msg::Float32>::SharedPtr speed_sub_;

  rclcpp::TimerBase::SharedPtr timer_;

  std::string health_status_ = "UNHEALTHY";
  std::string health_reason_ = "NONE";
  std::string system_status_ = "UNSAFE_STOP";
  float adjusted_speed_ = 0.0f;

  bool received_health_ = false;
  bool received_system_ = false;
  bool received_speed_ = false;

  rclcpp::Time last_health_time_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<StatusDisplayNode>());
  rclcpp::shutdown();
  return 0;
}
