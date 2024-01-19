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
void StoreWeatherToSpiffs(struct_Weather *pWeather);
void ReadWeatherFromSPIFF(struct_Weather *pWeather);
void DrawWeatherToDisplay(GxEPD2_GFX &d, struct_Weather *Weather, std::vector<int> hours_for_index);
void DeleteWeatherFromSPIFF();
bool CheckWeatherInSPIFF();
void PaintWeather(GxEPD2_GFX &d);
#endif //GOODWATCH_ESPIF_WEATHER_PAINT_WEATHER_H
