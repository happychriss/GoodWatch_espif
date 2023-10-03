//
// Created by development on 05.11.21.
//

#ifndef GOODWATCH_WIFI_SUPPORT_H
#define GOODWATCH_WIFI_SUPPORT_H
#include <WiFi.h>
#include "WiFiCredentials.h"
#include "WiFiClient.h"
#include <HTTPClient.h>
#include <lwip/apps/sntp.h>
#include "support.h"
#include "my_RTClib.h"

void SetupWifi() ;
void SetupSNTP();
void sendData(uint8_t *bytes, size_t count);
tm now_tm();
DateTime now_datetime();

#endif //GOODWATCH_WIFI_SUPPORT_H
