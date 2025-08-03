#ifndef BATTERY_PREDICTION_H
#define BATTERY_PREDICTION_H

#include <vector>
#include <cmath>
#include <algorithm>
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "battery_constants.h"


// --- Constants ---
#include "battery_constants.h"
#include "WString.h"

class RtcData;

float getBatteryVoltage();


struct BatteryStatus {
    float ema_capacity_pct;
    bool sharp_drop_warning;
    bool low_voltage_warning;
    float ema_voltage;
    float trend_slope;
};

void send_battery_status(float batteryVoltage = 0, BatteryStatus& status = *(new BatteryStatus()));
void manage_battery();

class BatteryMonitor {
public:
    BatteryMonitor();
    BatteryStatus feedVoltage(float voltage, RtcData &rtcData);
    String getStatusString(const BatteryStatus &status);
    BatteryStatus getStatus(const RtcData &rtcData); // New function



private:
    float ema;
    float initial_voltage;

    float calculateEMA(float voltage);
    float calculateCapacity(float ema);
    float calculateTrendSlope(const std::vector<float> &voltage_buffer);
};

#endif // BATTERY_PREDICTION_H