# 一辆车在场地坐标系下的位置。槽位顺序与 0x0305 一致：
# 对方英雄、工程、3 号步兵、4 号步兵、空中、哨兵，然后己方同样六台。
uint8 ENEMY_HERO = 0
uint8 ENEMY_ENGINEER = 1
uint8 ENEMY_INFANTRY_3 = 2
uint8 ENEMY_INFANTRY_4 = 3
uint8 ENEMY_AERIAL = 4
uint8 ENEMY_SENTRY = 5
uint8 OUR_HERO = 6
uint8 OUR_ENGINEER = 7
uint8 OUR_INFANTRY_3 = 8
uint8 OUR_INFANTRY_4 = 9
uint8 OUR_AERIAL = 10
uint8 OUR_SENTRY = 11

uint8 SOURCE_VISION = 0
uint8 SOURCE_RADIO = 1
uint8 SOURCE_FUSED = 2

builtin_interfaces/Time stamp
uint8 robot_id
int16 x_cm
int16 y_cm
float32 confidence
uint8 source
# false：这帧没有这辆车。打包 0x0305 时写成 x=0,y=0。
bool observed
