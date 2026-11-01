from launch import LaunchDescription
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    config = PathJoinSubstitution([
        FindPackageShare('radar_vision'),
        'config',
        'camera.yaml',
    ])
    return LaunchDescription([
        Node(
            package='radar_vision',
            executable='radar_vision_node',
            name='radar_vision',
            parameters=[config],
            output='screen',
        ),
    ])
