#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <sstream>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "yaml-cpp/yaml.h"

class HealthMonitorNode : public rclcpp::Node
{
public:
  HealthMonitorNode() : Node("health_monitor_node")
  {
    health_pub_ = this->create_publisher<std_msgs::msg::String>("/health_status", 10);
    reason_pub_ = this->create_publisher<std_msgs::msg::String>("/health_reason", 10);

    register_sub_ = this->create_subscription<std_msgs::msg::String>(
      "/register_node",
      10,
      std::bind(&HealthMonitorNode::register_node_callback, this, std::placeholders::_1));

    this->declare_parameter<std::string>("config_file", "config/health_monitor.yaml");

    std::string config_file;
    this->get_parameter("config_file", config_file);

    load_config(config_file);

    RCLCPP_INFO(this->get_logger(), "Health Monitor node started");
  }

private:
  struct QoSConfig
  {
    bool use_default = true;

    bool has_reliability = false;
    std::string reliability;

    bool has_history = false;
    std::string history;
    int depth = 0;

    bool has_durability = false;
    std::string durability;

    bool has_deadline = false;
    int deadline_ms = 0;

    bool has_liveliness = false;
    std::string liveliness;
    int liveliness_lease_ms = 0;
  };

  struct MonitoredNode
  {
    std::string name;
    std::string heartbeat_topic;
    std::string failure_reason;

    QoSConfig qos_config;

    bool alive = false;

    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription;
  };

  void load_config(const std::string & config_file)
  {
    YAML::Node config = YAML::LoadFile(config_file);

    for (const auto & node_config : config["nodes"])
    {
      std::string name = node_config["name"].as<std::string>();
      std::string heartbeat_topic = node_config["heartbeat_topic"].as<std::string>();
      std::string failure_reason = node_config["failure_reason"].as<std::string>();

      QoSConfig qos_config;

      if (node_config["qos"])
      {
        qos_config.use_default = false;
        YAML::Node qos_node = node_config["qos"];

        if (qos_node["use_default"]) {
          qos_config.use_default = qos_node["use_default"].as<bool>();
        }

        if (qos_node["reliability"])
        {
          qos_config.has_reliability = true;
          qos_config.reliability = qos_node["reliability"].as<std::string>();
        }

        if (qos_node["history"])
        {
          qos_config.has_history = true;
          qos_config.history = qos_node["history"].as<std::string>();
        }

        if (qos_node["depth"]) {
          qos_config.depth = qos_node["depth"].as<int>();
        }

        if (qos_node["durability"])
        {
          qos_config.has_durability = true;
          qos_config.durability = qos_node["durability"].as<std::string>();
        }

        if (qos_node["deadline_ms"])
        {
          qos_config.has_deadline = true;
          qos_config.deadline_ms = qos_node["deadline_ms"].as<int>();
        }

        if (qos_node["liveliness"])
        {
          qos_config.has_liveliness = true;
          qos_config.liveliness = qos_node["liveliness"].as<std::string>();
        }

        if (qos_node["liveliness_lease_ms"]) {
          qos_config.liveliness_lease_ms = qos_node["liveliness_lease_ms"].as<int>();
        }
      }

      add_monitored_node(name, heartbeat_topic, failure_reason, qos_config);
    }
  }

  void register_node_callback(const std_msgs::msg::String::SharedPtr msg)
  {
    std::stringstream ss(msg->data);

    std::string name;
    std::string heartbeat_topic;
    std::string failure_reason;
    std::string qos_string;

    std::getline(ss, name, '|');
    std::getline(ss, heartbeat_topic, '|');
    std::getline(ss, failure_reason, '|');
    std::getline(ss, qos_string, '|');

    if (name.empty() || heartbeat_topic.empty() || failure_reason.empty())
    {
      RCLCPP_WARN(
        this->get_logger(),
        "Invalid registration message: %s",
        msg->data.c_str());
      return;
    }

    QoSConfig qos_config;

    if (!qos_string.empty())
    {
      qos_config.use_default = false;

      std::stringstream qos_stream(qos_string);
      std::string item;

      while (std::getline(qos_stream, item, ';'))
      {
        auto equal_pos = item.find('=');

        if (equal_pos == std::string::npos) {
          continue;
        }

        std::string key = item.substr(0, equal_pos);
        std::string value = item.substr(equal_pos + 1);

        if (key == "use_default") {
          qos_config.use_default = (value == "true");
        }
        else if (key == "reliability")
        {
          qos_config.has_reliability = true;
          qos_config.reliability = value;
        }
        else if (key == "history")
        {
          qos_config.has_history = true;
          qos_config.history = value;
        }
        else if (key == "depth") {
          qos_config.depth = std::stoi(value);
        }
        else if (key == "durability")
        {
          qos_config.has_durability = true;
          qos_config.durability = value;
        }
        else if (key == "deadline_ms")
        {
          qos_config.has_deadline = true;
          qos_config.deadline_ms = std::stoi(value);
        }
        else if (key == "liveliness")
        {
          qos_config.has_liveliness = true;
          qos_config.liveliness = value;
        }
        else if (key == "liveliness_lease_ms") {
          qos_config.liveliness_lease_ms = std::stoi(value);
        }
      }
    }

    add_monitored_node(name, heartbeat_topic, failure_reason, qos_config);
  }

  bool is_already_monitored(const std::string & heartbeat_topic)
  {
    for (const auto & node : monitored_nodes_)
    {
      if (node->heartbeat_topic == heartbeat_topic) {
        return true;
      }
    }

    return false;
  }

  rclcpp::QoS build_qos(const QoSConfig & qos_config)
  {
    if (qos_config.use_default) {
      return rclcpp::QoS(10);
    }

    int depth = 10;

    if (qos_config.has_history && qos_config.depth > 0) {
      depth = qos_config.depth;
    }

    rclcpp::QoS qos(depth);

    if (qos_config.has_history)
    {
      if (qos_config.history == "keep_all") {
        qos.keep_all();
      } else if (qos_config.history == "keep_last") {
        qos.keep_last(depth);
      }
    }

    if (qos_config.has_reliability)
    {
      if (qos_config.reliability == "reliable") {
        qos.reliable();
      } else if (qos_config.reliability == "best_effort") {
        qos.best_effort();
      }
    }

    if (qos_config.has_durability)
    {
      if (qos_config.durability == "volatile") {
        qos.durability_volatile();
      } else if (qos_config.durability == "transient_local") {
        qos.transient_local();
      }
    }

    if (qos_config.has_deadline)
    {
      qos.deadline(std::chrono::milliseconds(qos_config.deadline_ms));
    }

    if (qos_config.has_liveliness)
    {
      if (qos_config.liveliness == "manual_by_topic") {
        qos.liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC);
      } else if (qos_config.liveliness == "automatic") {
        qos.liveliness(RMW_QOS_POLICY_LIVELINESS_AUTOMATIC);
      }

      if (qos_config.liveliness_lease_ms > 0)
      {
        qos.liveliness_lease_duration(
          std::chrono::milliseconds(qos_config.liveliness_lease_ms));
      }
    }

    return qos;
  }

  void add_monitored_node(
    const std::string & name,
    const std::string & heartbeat_topic,
    const std::string & failure_reason,
    const QoSConfig & qos_config)
  {
    if (is_already_monitored(heartbeat_topic))
    {
      RCLCPP_INFO(
        this->get_logger(),
        "Node already monitored: %s",
        heartbeat_topic.c_str());
      return;
    }

    auto monitored_node = std::make_shared<MonitoredNode>();

    monitored_node->name = name;
    monitored_node->heartbeat_topic = heartbeat_topic;
    monitored_node->failure_reason = failure_reason;
    monitored_node->qos_config = qos_config;

    auto qos = build_qos(qos_config);

    rclcpp::SubscriptionOptions options;

    if (qos_config.has_deadline)
    {
      options.event_callbacks.deadline_callback =
        [this, monitored_node](rclcpp::QOSDeadlineRequestedInfo &)
        {
          monitored_node->alive = false;
          publish_health_status();
        };
    }

    if (qos_config.has_liveliness)
    {
      options.event_callbacks.liveliness_callback =
        [this, monitored_node](rclcpp::QOSLivelinessChangedInfo & event)
        {
          monitored_node->alive = (event.alive_count > 0);
          publish_health_status();
        };
    }

    options.event_callbacks.incompatible_qos_callback =
      [this, monitored_node](rclcpp::QOSRequestedIncompatibleQoSInfo & event)
      {
        monitored_node->alive = false;

        RCLCPP_WARN(
          this->get_logger(),
          "QoS incompatibility detected for %s. Total count: %d, Last policy kind: %d",
          monitored_node->name.c_str(),
          event.total_count,
          event.last_policy_kind);

        publish_health_status();
      };

    monitored_node->subscription =
      this->create_subscription<std_msgs::msg::String>(
        heartbeat_topic,
        qos,
        [this, monitored_node](const std_msgs::msg::String::SharedPtr)
        {
          monitored_node->alive = true;
          publish_health_status();
        },
        options);

    monitored_nodes_.push_back(monitored_node);

    RCLCPP_INFO(
      this->get_logger(),
      "Monitoring %s on %s",
      name.c_str(),
      heartbeat_topic.c_str());
  }

  void publish_health_status()
  {
    std_msgs::msg::String health_msg;
    std_msgs::msg::String reason_msg;

    bool all_ok = true;
    reason_msg.data = "";

    for (const auto & node : monitored_nodes_)
    {
      if (!node->alive)
      {
        all_ok = false;

        if (!reason_msg.data.empty()) {
          reason_msg.data += ",";
        }

        reason_msg.data += node->failure_reason;
      }
    }

    if (all_ok) {
      health_msg.data = "HEALTHY";
      reason_msg.data = "NONE";
    } else {
      health_msg.data = "UNHEALTHY";
    }

    health_pub_->publish(health_msg);
    reason_pub_->publish(reason_msg);

    std::string node_states;

    for (const auto & node : monitored_nodes_)
    {
      if (!node_states.empty()) {
        node_states += " ";
      }

      node_states += node->name + "=" + (node->alive ? "OK" : "FAILED");
    }

    RCLCPP_INFO_THROTTLE(
      this->get_logger(),
      *this->get_clock(),
      2000,
      "Health Status: %s | Reason=%s | %s",
      health_msg.data.c_str(),
      reason_msg.data.c_str(),
      node_states.c_str());
  }

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr health_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr reason_pub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr register_sub_;

  std::vector<std::shared_ptr<MonitoredNode>> monitored_nodes_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<HealthMonitorNode>());
  rclcpp::shutdown();
  return 0;
}
