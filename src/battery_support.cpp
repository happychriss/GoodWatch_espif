#include "battery_support.h"

#include "global_hardware.h"
#include "HTTPClient.h"
#include "rtc_support.h"
#include "wifi_support.h"

float getBatteryVoltage() {
    DPL("Sent battery update to GW Server");
    const int numSamples = 10; // Number of samples to average
    int adcSum = 0;

    for (int i = 0; i < numSamples; i++) {
        adcSum += analogRead(BATTERY_VOLTAGE);
        delay(10); // Small delay between readings to avoid rapid sampling
    }

    int adc = adcSum / numSamples; // Calculate the average ADC value
    float batteryVoltage = (adc * 7.445f) / 4096.0f;

    DP("Battery V: ");
    DPL(batteryVoltage);

    return batteryVoltage;
}

void send_battery_status(float batteryVoltage, BatteryStatus& status) {
    // Get battery status string
    BatteryMonitor monitor;
    String batteryStatusString = monitor.getStatusString(status);

    SetupWifi();
    WiFiClient client;
    HTTPClient http;
//    http.begin(client, "http://192.168.1.100:8080/voltage"); // donald
    http.begin(client, "http://192.168.1.110:8088/voltage"); // zo
    http.addHeader("Content-Type", "application/json");

    // Build JSON payload
    String source = "\"source\":\"GW_2\"";
    String voltage = "\"voltage\":\"" + String(batteryVoltage, 2) + "\"";
    String capacity = "\"capacity\":\"" + String(status.ema_capacity_pct, 1) + "\"";
    String warnings = "\"warnings\":\"" + String(status.low_voltage_warning ? "LOW " : "") +
                    String(status.sharp_drop_warning ? "DROP" : "") + "\"";
    String statusStr = "\"bat_status_string\":\"" + batteryStatusString + "\"";
    String slope = "\"bat_slope\":\"" + String(status.trend_slope, 3) + "\"";

    String httpRequestData = "{" + source + "," + voltage + "," + capacity + "," + warnings + "," + statusStr + "," + slope + "}";

    DP("RequestString:");
    DPL(httpRequestData);
    int httpResponseCode = http.POST(httpRequestData);
    DP("HTTP Response code: ");
    DPL(httpResponseCode);

    // Free resources
    http.end();
}

void manage_battery()
{
    // Measure battery voltage
    double voltage = getBatteryVoltage();

    // Feed the voltage to the BatteryMonitor
    BatteryMonitor monitor;
    RtcData rtcData;
    rtcData.getRTCData();
    BatteryStatus status = monitor.feedVoltage(voltage, rtcData);
    rtcData.writeRTCData();

    // Print the status using DPF
    DPF("Capacity: %.2f%%, Sharp Drop: %s, Low Voltage: %s, EMA: %.2f, Slope: %.6f\n",
        status.ema_capacity_pct,
        status.sharp_drop_warning ? "Yes" : "No",
        status.low_voltage_warning ? "Yes" : "No",
        status.ema_voltage,
        status.trend_slope);

    // Send voltage and battery status to server
    send_battery_status(voltage, status);
}



BatteryMonitor::BatteryMonitor() : ema(0), initial_voltage(0) {}

BatteryStatus BatteryMonitor::feedVoltage(float voltage, RtcData &rtcData) {
    DPF("Feeding voltage: %.2f\n", voltage);

    // Initialize initial voltage
    if (initial_voltage == 0) {
        initial_voltage = voltage;
        DPL("Initial voltage set.");
    }

    // Initialize or update EMA using persisted value
    if (rtcData.d.battery.ema_voltage == 0) {
        ema = voltage;
    } else {
        ema = EMA_ALPHA * voltage + (1 - EMA_ALPHA) * rtcData.d.battery.ema_voltage;
    }
    DPF("Updated EMA: %.2f\n", ema);

    // Get and update voltage buffer
    std::vector<float> voltage_buffer = rtcData.getVoltageBuffer();
    voltage_buffer.push_back(voltage);
    if (voltage_buffer.size() > TREND_WINDOW) {
        voltage_buffer.erase(voltage_buffer.begin());
    }
    rtcData.updateVoltageBuffer(voltage_buffer);
    DPF("Voltage buffer size: %lu\n", voltage_buffer.size());

    // Calculate capacity percentage
    float capacity_pct = calculateCapacity(ema);
    DPF("Capacity percentage: %.2f%%\n", capacity_pct);

    // Calculate trend slope
    float slope = calculateTrendSlope(voltage_buffer);
    DPF("Trend slope: %.6f\n", slope);

    // Check for sharp drop
    bool sharp_drop = slope < SHARP_DROP_THRESHOLD;
    if (sharp_drop) DPL("Sharp drop detected!");

    // Check for low voltage
    bool low_voltage = voltage < LOW_VOLTAGE_THRESHOLD;
    if (low_voltage) DPL("Low voltage warning!");

    // Update RTC data with new values
    rtcData.d.battery.ema_voltage = ema;
    rtcData.d.battery.ema_capacity_pct = capacity_pct;
    rtcData.d.battery.trend_slope = slope;
    rtcData.d.battery.sharp_drop_warning = sharp_drop;
    rtcData.d.battery.low_voltage_warning = low_voltage;

    return BatteryStatus{
        .ema_capacity_pct = capacity_pct,
        .sharp_drop_warning = sharp_drop,
        .low_voltage_warning = low_voltage,
        .ema_voltage = ema,
        .trend_slope = slope
    };

}
float BatteryMonitor::calculateEMA(float voltage) {
    return EMA_ALPHA * voltage + (1 - EMA_ALPHA) * ema;
}

float BatteryMonitor::calculateCapacity(float ema) {
    if (BATTERY_FULL_VOLTAGE == BATTERY_EMPTY_VOLTAGE) {
        return 0.0;
    }
    float capacity = 100 * (ema - BATTERY_EMPTY_VOLTAGE) / (BATTERY_FULL_VOLTAGE - BATTERY_EMPTY_VOLTAGE);
    return (capacity < 0.0f) ? 0.0f : (capacity > 100.0f ? 100.0f : capacity);
}

float BatteryMonitor::calculateTrendSlope(const std::vector<float> &voltage_buffer) {
    if (voltage_buffer.size() < 2) {
        return 0.0;
    }

    float sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    size_t n = voltage_buffer.size();

    for (size_t i = 0; i < n; ++i) {
        sum_x += i;
        sum_y += voltage_buffer[i];
        sum_xy += i * voltage_buffer[i];
        sum_x2 += i * i;
    }

    float slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    return slope;
}

String BatteryMonitor::getStatusString(const BatteryStatus &status) {
    char buffer[64];
    // Format: Voltage, Percentage, Warning indicators
    snprintf(buffer, sizeof(buffer), "%.1fV %.0f%% %s%s",
            status.ema_voltage,
            status.ema_capacity_pct,
            status.low_voltage_warning ? "LOW " : "",
            status.sharp_drop_warning ? "DROP" : "");
    return String(buffer);
}

BatteryStatus BatteryMonitor::getStatus(const RtcData &rtcData) {
    BatteryStatus status;

    // Get values directly from rtcData's battery structure
    status.ema_voltage = rtcData.d.battery.ema_voltage;
    status.ema_capacity_pct = rtcData.d.battery.ema_capacity_pct;
    status.trend_slope = rtcData.d.battery.trend_slope;
    status.sharp_drop_warning = rtcData.d.battery.sharp_drop_warning;
    status.low_voltage_warning = rtcData.d.battery.low_voltage_warning;

    return status;
}
