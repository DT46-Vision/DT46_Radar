from launch import LaunchDescription
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    vision_config = PathJoinSubstitution([
        FindPackageShare('radar_vision'),
        'config',
        'camera.yaml',
    ])
    fusion_config = PathJoinSubstitution([
        FindPackageShare('radar_fusion'),
        'config',
        'fusion.yaml',
    ])
    livox_config = PathJoinSubstitution([
        FindPackageShare('radar_bringup'),
        'config',
        'livox_lidar_config.json',
    ])
    # Broadcast code / IP / wiring: Livox_MID-70 README. Do not invent SN here.
    # 裁判串口用 XDU rm_referee，只由那个进程写串口。0x0305 以后走 /rm_referee/tx。
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
        Node(
            package='radar_vision',
            executable='radar_vision_node',
            name='radar_vision',
            parameters=[vision_config],
            output='screen',
        ),
        Node(
            package='radar_fusion',
            executable='radar_fusion_node',
            name='radar_fusion',
            parameters=[fusion_config],
            output='screen',
        ),
    ])
