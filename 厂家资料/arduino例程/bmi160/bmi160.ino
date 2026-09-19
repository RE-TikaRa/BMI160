#include <DFRobot_BMI160.h>

DFRobot_BMI160 bmi160;
const int8_t i2c_addr = 0x69;  // 根据模块AD0引脚修改（0x68或0x69）

// 姿态角变量（单位：度）
float roll = 0.0, pitch = 0.0, yaw = 0.0;
// 时间变量（用于陀螺仪积分）
unsigned long preTime = 0;
float dt = 0.0;

// 加速度计量程：±8g（对应转换系数）
const float ACCEL_SCALE = 16384.0;
// 陀螺仪量程：±2000°/s（对应转换系数）
const float GYRO_SCALE = 16.4;

void setup() {
  Serial.begin(115200);
  delay(100);
  
  // 传感器软复位
  if (bmi160.softReset() != BMI160_OK) {
    Serial.println("传感器复位失败！");
    while(1);
  }
  
  // 初始化I2C通信
  if (bmi160.I2cInit(i2c_addr) != BMI160_OK) {
    Serial.println("传感器初始化失败！");
    while(1);
  }
  Serial.println("传感器初始化成功！");
  preTime = millis();  // 记录初始时间
}

void loop() {
  // 计算时间间隔（秒）
  unsigned long now = millis();
  dt = (now - preTime) / 1000.0;
  preTime = now;

  int16_t accelGyro[6] = {0};
  // 读取加速度和陀螺仪原始数据
  if (bmi160.getAccelGyroData(accelGyro) == 0) {
    // 提取数据并转换单位
    // 陀螺仪数据（°/s）：x,y,z（前3个值）
    float gx = accelGyro[0] / GYRO_SCALE;
    float gy = accelGyro[1] / GYRO_SCALE;
    float gz = accelGyro[2] / GYRO_SCALE;
    
    // 加速度计数据（g）：x,y,z（后3个值）
    float ax = accelGyro[3] / ACCEL_SCALE;
    float ay = accelGyro[4] / ACCEL_SCALE;
    float az = accelGyro[5] / ACCEL_SCALE;

    // 计算姿态角（互补滤波融合加速度计和陀螺仪数据）
    calculateAttitude(ax, ay, az, gx, gy, gz, dt);

    // 输出姿态角
    Serial.print("Roll: ");
    Serial.print(roll, 1);
    Serial.print(" °, Pitch: ");
    Serial.print(pitch, 1);
    Serial.print(" °, Yaw: ");
    Serial.print(yaw, 1);
    Serial.println(" °");
  } else {
    Serial.println("数据读取失败！");
  }
  delay(10);  // 控制刷新频率
}

// 互补滤波计算姿态角
void calculateAttitude(float ax, float ay, float az, float gx, float gy, float gz, float dt) {
  // 1. 从加速度计计算初始姿态（静态时可靠）
  float accel_roll = atan2(ay, az) * 180.0 / PI;
  float accel_pitch = atan2(-ax, sqrt(ay*ay + az*az)) * 180.0 / PI;

  // 2. 从陀螺仪积分更新姿态（动态时可靠）
  roll += gx * dt;    // 横滚角 = 角速度x * 时间
  pitch += gy * dt;   // 俯仰角 = 角速度y * 时间
  yaw += gz * dt;     // 偏航角 = 角速度z * 时间（无磁力计会漂移）

  // 3. 互补滤波融合（加权合并两种结果，减少漂移和噪声）
  roll = 0.96 * roll + 0.04 * accel_roll;
  pitch = 0.96 * pitch + 0.04 * accel_pitch;
}