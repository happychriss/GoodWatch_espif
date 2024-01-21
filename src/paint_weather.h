//
// Created by development on 16.09.23.
//

#ifndef GOODWATCH_ESPIF_WEATHER_PAINT_WEATHER_H
#define GOODWATCH_ESPIF_WEATHER_PAINT_WEATHER_H
#include "Arduino.h"
#include "weather.h"
#include <GxEPD2_BW.h>
#include <GxEPD2_GFX.h>
void Load_PaintWeather(GxEPD2_GFX &d);
void StoreWeatherToSpiffs(struct_AlarmWeather *pWeather);

void DrawWeatherToDisplay(GxEPD2_GFX &d, struct_AlarmWeather *Weather);
void DeleteWeatherFromSPIFF();
bool GetValidForecastFromSpiff(DateTime set_alarm_time, struct_AlarmWeather *pWeather, String &resultMessage);

#endif //GOODWATCH_ESPIF_WEATHER_PAINT_WEATHER_H
