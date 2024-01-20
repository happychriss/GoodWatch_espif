#ifndef weather_h
#define weather_h

#include <Arduino.h>
#include "pugixml.hpp"
#include "my_RTClib.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <time.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <map>
#include <algorithm>
#include <utility>


#define HOURS_FORECAST 49

#define DWD_URL "https://opendata.dwd.de/weather/local_forecasts/mos/MOSMIX_L/single_stations/H635/kml/MOSMIX_L_LATEST_H635.kmz"
//#define DWD_URL "https://opendata.dwd.de/weather/local_forecasts/mos/MOSMIX_L/single_stations/H635/kml/MOSMIX_L_2023092415_H635.kmz"
//#define DWD_URL "https://example.com/"

#define MOSMIX_ZIP_FILE  "/spiffs/MOSMIX_L_LATEST.kmz"
#define MOSMIX_FILE  "/spiffs/MOSMIX_L_LATEST.kml"

#define FORCAST_HORIZONT 2
struct struct_forecast_schedule {
    int check_hour;
    std::array<int, FORCAST_HORIZONT> forecast_hours;
};


#define NO_ICON_FOUND 999




const char * const weather_names[] = {
        "chanceflurries.bmp",   // index 0  // Flurries, ID: 0
        "chancerain.bmp",       // index 1  // Chance Rain, ID: 61
        "chancesleet.bmp",      // index 2  // Chance Sleet, ID: 56
        "chancesnow.bmp",       // index 3  // Chance Snow, ID: 85
        "chancetstorms.bmp",    // index 4  // Chance Thunderstorms, ID: 99
        "clear.bmp",            // index 5  // Clear, ID: 1
        "cloudy.bmp",           // index 6  // Cloudy, ID: 3
        "flurries.bmp",         // index 7  // Flurries, ID: 83
        "fog.bmp",              // index 8  // Fog, ID: 49
        "hazy.bmp",             // index 9  // Hazy, ID: 99
        "mostlycloudy.bmp",     // index 10 // Mostly Cloudy, ID: 99
        "mostlysunny.bmp",      // index 11 // Mostly Sunny, ID: 99
        "partlycloudy.bmp",     // index 12 // Partly Cloudy, ID: 99
        "partlysunny.bmp",      // index 13 // Partly Sunny, ID: 99
        "rain.bmp",             // index 14 // Rain, ID: 65
        "sleet.bmp",            // index 15 // Sleet, ID: 57
        "snow.bmp",             // index 16 // Snow, ID: 75
        "sunny.bmp",            // index 17 // Sunny, ID: 0
        "tstorms.bmp",          // index 18 // Thunderstorms, ID: 95
        "unknown.bmp"           // index 19 // Unknown, ID: 0
};

enum WeatherIcon {

    wi_ChanceFlurries,  // 0
    wi_ChanceRain,      // 1
    wi_ChanceSleet,     // 2
    wi_ChanceSnow,      // 3
    wi_ChanceTStorms,   // 4
    wi_Clear,           // 5
    wi_Cloudy,          // 6
    wi_Flurries,        // 7
    wi_Fog,             // 8
    wi_Hazy,            // 9
    wi_MostlyCloudy,    // 10
    wi_MostlySunny,     // 11
    wi_PartlyCloudy,    // 12
    wi_PartlySunny,     // 13
    wi_Rain,            // 14
    wi_Sleet,           // 15
    wi_Snow,            // 16
    wi_Sunny,           // 17
    wi_TStorms,         // 18
    wi_Unknown          // 19
};

struct WindScale {
        double lowerBound;
        double upperBound;
        int beaufortNumber;
};

static const std::vector<WindScale> beaufortScale = {
        {0.0, 0.2, 0},
        {0.3, 1.5, 1},
        {1.6, 3.3, 2},
        {3.4, 5.4, 3},
        {5.5, 7.9, 4},
        {8.0, 10.7, 5},
        {10.8, 13.8, 6},
        {13.9, 17.1, 7},
        {17.2, 20.7, 8},
        {20.8, 24.4, 9},
        {24.5, 28.4, 10},
        {28.5, 32.6, 11},
        {32.7, -1, 12} // -1 as upperBound denotes that it's open-ended
};

struct WeatherIDMap {
    uint8_t dwd_id;
    int icon_id;
};



const WeatherIDMap weather_id_maps[] = {
        { 0, NO_ICON_FOUND }, // unknown
        { 61, 1 }, // chance rain
        { 56, 2 }, // chance sleet
        { 66, 2 }, // chance sleet
        { 68, 2 }, // chance sleet
        { 53, 2 }, // chance sleet
        { 51, 2 }, // chance sleet
        { 85, 3 }, // chance snow
        { 99, 4 }, // chance thunderstorms
        { 1, 5 }, // clear
        { 3, 6 }, // cloudy
        { 2, 6 }, // cloudy
        { 83, 7 }, // flurries
        { 49, 8 }, // fog
        { 99, 9 }, // hazy
        { 99, 10 }, // mostly cloudy
        { 99, 11 }, // mostly sunny
        { 99, 12 }, // partly cloudy
        { 99, 13 }, // partly sunny
        { 65, 14 }, // rain
        { 63, 14 }, // rain
        { 61, 14 }, // rain
        { 57, 15 }, // sleet
        { 67, 15 }, // sleet
        { 55, 15 }, // sleet
        { 75, 16 }, // snow
        { 73, 16 }, // snow
        { 71, 16 }, // snow
        { 69, 16 }, // snow
        { 0, 17 }, // sunny
        { 95, 18 }, // thunderstorms
        { 0, 19 }  // unknown
};




#pragma once
#include <map>
#include <string>

namespace Weather {
    // This std::map holds the mapping between weather codes and their descriptions
    static std::map<int, String> weatherData = {
            {95, "leichtes  Gewitter mit Regen oder Schnee"},
            {57, "etwas oder starker gefrierender Spruehregen"},
            {56, "leichter gefrierender Spruehregen"},
            {67, "etwas bis starker gefrierender Regen"},
            {66, "leichter gefrierender Regen"},
            {86, "etwas bis starker Schneeschauer"},
            {85, "leichter Schneeschauer"},
            {84, "etwas oder starker Schneeregenschauer"},
            {83, "leichter Schneeregenschauer"},
            {82, "sehr heftige Regenschauer"},
            {81, "etwas oder starker Regenschauer"},
            {80, "leichter Regenschauer"},
            {75, "durchgehend starker Schneefall"},
            {73, "durchgehend mäßiger Schneefall"},
            {71, "durchgehend leichter Schneefall"},
            {69, "etwas oder starker Schneeregen"},
            {68, "leichter Schneeregen"},
            {55, "durchgehend starker Spruehregen"},
            {53, "durchgehend mäßiger Spruehregen"},
            {51, "durchgehend leichter Spruehregen"},
            {65, "durchgehend starker Regen"},
            {63, "durchgehend mäßiger Regen"},
            {61, "durchgehend leichter Regen"},
            {49, "Nebel mit Reifansatz, Himmel nicht erkennbar"},
            {45, "Nebel, Himmel nicht erkennbar"},
            {3,  "Wolken zunehmend"},
            {2,  "Wolken gleichbleibent"},
            {1,  "Wolken abnehmend"},
            {0,  "keine Wolken"}
    };
}


//struct std::tm TimeSteps[HOURS_FORECAST];




struct struct_HourlyWeather {
    tm time;
    int hour_index;
    double temperature;
    double wind;
    double rain;
    double clouds;
    double sun;
    int forecast_id;
};

struct struct_Weather {
    tm publish_time;
    std::array<struct_HourlyWeather, HOURS_FORECAST> HourlyWeather;
    uint32_t dummy; // CRC field
    uint32_t crc; // CRC field
};

struct struct_AlarmWeather {
    tm publish_time;
    tm alarm_time;
    std::array<struct_HourlyWeather, 2> HourlyWeather;
    uint32_t dummy; // CRC field
    uint32_t crc; // CRC field
};


// extern struct_HourlyWeather HourlyWeather[HOURS_FORECAST];
void GetWeather(struct_Weather *ptrWeather);
void GetAlarmWeather(const DateTime alarm_time,
                     struct_Weather *ptr_Weather,
                     struct_AlarmWeather *ptr_AlarmWeather);
uint32_t calculateWeatherCRC(struct_AlarmWeather &weather);
void DWD_Weather(struct_Weather*  ptr_myWF);
void coreTask( void * pvParameters );
int determineWeatherIcon(const struct_HourlyWeather &hw);
void determineWeatherString(const struct_HourlyWeather &weather, String &line_1, String &line_2);
void printHourlyWeather(struct_HourlyWeather hw);
bool returnHourIndexFromForecast(
        tm check_time,
        const std::vector<struct_forecast_schedule>& schedule,
        const std::array<struct_HourlyWeather,HOURS_FORECAST> * ptr_hourly_weather,
        std::vector <int> *hours_index);

String getWeatherString(int number);

#endif



