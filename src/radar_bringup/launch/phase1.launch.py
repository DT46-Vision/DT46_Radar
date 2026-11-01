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
    # livox_ros2_driver 从 Livox_MID-70 的固定 commit 编译进同一工作区后再启动。
    # 裁判串口用 XDU rm_referee，只由那个进程写串口。0x0305 以后走 /rm_referee/tx。
    return LaunchDescription([
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
