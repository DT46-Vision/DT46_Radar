"""Mid-70 point cloud only. For verifying /livox/lidar before vision/fusion."""
from launch import LaunchDescription
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    livox_config = PathJoinSubstitution([
        FindPackageShare('radar_bringup'),
        'config',
        'livox_lidar_config.json',
    ])
    return LaunchDescription([
        Node(
            package='livox_ros2_driver',
            executable='livox_ros2_driver_node',
            name='livox_lidar_publisher',
            output='screen',
            parameters=[{
                'xfer_format': 0,
                'multi_topic': 0,
                'data_src': 0,
                'publish_freq': 10.0,
                'output_data_type': 0,
                'frame_id': 'livox_frame',
                'user_config_path': livox_config,
                'cmdline_input_bd_code': '3GGDJAS001J6411',
            }],
        ),
    ])
