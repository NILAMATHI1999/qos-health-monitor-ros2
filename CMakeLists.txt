cmake_minimum_required(VERSION 3.8)
project(qos_health_monitor_demo)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

# Node A
add_executable(sensor_a_node src/sensor_a_node.cpp)
ament_target_dependencies(sensor_a_node rclcpp std_msgs)

# Node B
add_executable(sensor_b_node src/sensor_b_node.cpp)
ament_target_dependencies(sensor_b_node rclcpp std_msgs)

# Node 3
add_executable(safety_decision_node src/safety_decision_node.cpp)
ament_target_dependencies(safety_decision_node rclcpp std_msgs)

# Health Monitor Node
add_executable(health_monitor_node src/health_monitor_node.cpp)
ament_target_dependencies(health_monitor_node rclcpp std_msgs)

# Install all nodes
install(TARGETS
  sensor_a_node
  sensor_b_node
  safety_decision_node
  health_monitor_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
