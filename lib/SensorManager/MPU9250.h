#pragma once
#include "SensorController.h"
#include <I2Cdev.h>
#include <MPU6050.h>  // From I2Cdevlib
#include <helper_3dmath.h>

class MPU9250Sensor {
public:
    enum class AccelRange {
        RANGE_2G = 0,
        RANGE_4G,
        RANGE_8G,
        RANGE_16G
    };

    enum class GyroRange {
        RANGE_250DPS = 0,
        RANGE_500DPS,
        RANGE_1000DPS,
        RANGE_2000DPS
    };

    bool begin(AccelRange aRange = AccelRange::RANGE_8G, 
               GyroRange gRange = GyroRange::RANGE_500DPS);
    bool readData();
    
    // Data access
    const VectorFloat& getAccel() const { return accel; }
    const VectorFloat& getGyro() const { return gyro; }
    float getTemperature() const { return temp; }

private:
    MPU6050 imu;
    VectorFloat accel;
    VectorFloat gyro;
    float temp = 0;
    bool initialized = false;
};
