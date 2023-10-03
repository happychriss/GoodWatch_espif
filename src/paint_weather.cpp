//
// Created by development on 16.09.23.
//

#include "paint_weather.h"
//
// Created by development on 25.07.21.
//
#include <Arduino.h>
#include <StreamString.h>

#include <ctime>

#include "paint_watch.h"
#include "support.h"
#include "fonts/FreeSansNumOnly70.h"
#include "rtc_support.h"
#include "paint_alarm.h"
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <my_RTClib.h>
#include <paint_support.h>
#include "weather.h"
#include "time.h"
#include "SPIFFS.h"
#include "fonts/FreeSans9pt7b.h"

std::vector<struct_forecast_schedule> schedule = {
        {6,  {8,  18}},
        {8,  {10, 19}},
        {10, {13, 19}},
        {12, {14, 19}},
        {15, {18, 8}},
        {20, {8,  18}},
        {23, {8,  18}},
};

uint16_t read16(fs::File &f) {
    // BMP data is stored little-endian, same as Arduino.
    uint16_t result;
    ((uint8_t *) &result)[0] = f.read(); // LSB
    ((uint8_t *) &result)[1] = f.read(); // MSB
    return result;
}

uint32_t read32(fs::File &f) {
    // BMP data is stored little-endian, same as Arduino.
    uint32_t result;
    ((uint8_t *) &result)[0] = f.read(); // LSB
    ((uint8_t *) &result)[1] = f.read();
    ((uint8_t *) &result)[2] = f.read();
    ((uint8_t *) &result)[3] = f.read(); // MSB
    return result;
}

static const uint16_t input_buffer_pixels = 800; // may affect performance

static const uint16_t max_row_width = 1872; // for up to 7.8" display 1872x1404
static const uint16_t max_palette_pixels = 256; // for depth <= 8

uint8_t input_buffer[3 * input_buffer_pixels]; // up to depth 24
uint8_t output_row_mono_buffer[max_row_width / 8]; // buffer for at least one row of b/w bits
uint8_t output_row_color_buffer[max_row_width / 8]; // buffer for at least one row of color bits
uint8_t mono_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 b/w
uint8_t color_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 c/w
uint16_t rgb_palette_buffer[max_palette_pixels]; // palette buffer for depth <= 8 for buffered graphics, needed for 7-color display

void my_drawBMP(GxEPD2_GFX &display, const char *path, int16_t x_offset, int16_t y_offset) {

    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }
    File file = SPIFFS.open(String("/") + path, "r");

    if (!file || file.isDirectory()) {
        Serial.println("Failed to open file for reading");
        return;
    }

    // Read BMP Header
    file.seek(18);
    int16_t width, height;
    file.read((uint8_t *) &width, 4); // Width is at 18th byte
    file.read((uint8_t *) &height, 4); // Height is at 22nd byte

    uint16_t bitDepth;
    file.seek(28);
    file.read((uint8_t *) &bitDepth, 2); // Bit depth is at 28th byte

    int dataStart;
    file.seek(10);
    file.read((uint8_t *) &dataStart, 4); // Data start is at 10th byte

    // Check whether the bitDepth is 1 bit or not.
    if (bitDepth != 1) {
        Serial.println("Image is not 1-bit depth");
        file.close();
        return;
    }

    Serial.printf("Width: %d, Height: %d, Bit Depth: %d, Data Start: %d\n", width, height, bitDepth, dataStart);

    // Read Pixel Data
    file.seek(dataStart);

    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            byte pixel;
            if ((x % 8) == 0)
                file.read(&pixel, 1); // In 1-bit BMP, 1 byte = 8 pixels.
            int color = ((pixel >> (7 - (x % 8))) & 0x1);
            display.drawPixel(x + x_offset, y + y_offset, color);
        }
    }

    file.close();
}


#define ICON_WIDTH 90
#define WF_HEIGHT 26

#define COLUMN_0_Time 5
#define COLUMN_1_ICON 70
#define COLUMN_2_FORECAST (COLUMN_1_ICON+ICON_WIDTH+25)

#define X_OFF 10
#define Y_OFF 20


void printOnForecast(GxEPD2_GFX &d, struct_HourlyWeather fc, int y_offset) {

    // Forcast Time
    char timeStr[50];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &fc.time);

    d.setFont(&FreeSans12pt7b);
    d.setCursor(COLUMN_0_Time,  WF_HEIGHT * 3 + y_offset);
    d.print(timeStr);

    // Forecast
    d.setCursor(COLUMN_2_FORECAST,  WF_HEIGHT * 3 + y_offset);
    d.print((String(fc.temperature) + " °C   " + String(fc.rain) + " mm").c_str());

    d.setFont(&FreeSans9pt7b);
    if (fc.forecast_id > 3) {
        d.setCursor(COLUMN_1_ICON + X_OFF,  WF_HEIGHT * 5 + y_offset);
        d.print(getWeatherString(fc.forecast_id).c_str());
    }

}


void DrawWeatherToDisplay(GxEPD2_GFX &d, struct_Weather *Weather, std::vector<int> hours_for_index) {

    DPL("Draw WeatherToDisplay");

    char str_time_publish_time[20];
    strftime(str_time_publish_time, sizeof(str_time_publish_time), "%H:%M %d.%m.%y", &(Weather->publish_time));
    DPF("Publish time: %s\n", str_time_publish_time);

    struct_HourlyWeather fc{};

    d.setTextColor(GxEPD_BLACK);
    d.setFullWindow();

    d.firstPage();
    do {

        d.drawFastHLine(0, d.height() / 2, d.width(), GxEPD_BLACK);
        d.drawFastVLine(COLUMN_1_ICON, 0, d.height(), GxEPD_BLACK);

        // Publish Time of Forecast
        int16_t tbx, tby;
        uint16_t tbw, tbh;
        d.setFont(&FreeSans9pt7b);
        d.getTextBounds(str_time_publish_time, 0, 0, &tbx, &tby, &tbw, &tbh);
        d.setCursor(d.width()-(tbw+X_OFF), Y_OFF);
        d.print(str_time_publish_time);

        // First Weather Entry
        fc = Weather->HourlyWeather[hours_for_index[0]];
        printOnForecast(d, fc, 0);
        int weather_icon = determineWeatherIcon(fc);
        DPF("Painting Weather icon: %d\n", weather_icon);
        my_drawBMP(d, weather_names[weather_icon], COLUMN_1_ICON + 8, WF_HEIGHT);

        // Second Weather Entry
        fc = Weather->HourlyWeather[hours_for_index[1]];
        printOnForecast(d, fc, 150);
        weather_icon = determineWeatherIcon(fc);
        DPF("Painting Weather icon: %d\n", weather_icon);
        my_drawBMP(d, weather_names[weather_icon], COLUMN_1_ICON + 8, WF_HEIGHT + 150);


    } while (d.nextPage());


}


// Delete Weather from Spiffs
void DeleteWeatherFromSPIFF() {

    DPL("Delete Weather from Spiffs");

    if (!SPIFFS.begin(true)) {
        DPL("An Error has occurred while mounting SPIFFS");
        return;
    }

    SPIFFS.remove("/weather.bin");

    DPL("Done");
}

// Check if Weather is in Spiffs
bool CheckWeatherInSPIFF() {

    DPL("Check Weather in Spiffs");

    if (!SPIFFS.begin(true)) {
        DPL("An Error has occurred while mounting SPIFFS");
        return false;
    }

    if (SPIFFS.exists("/weather.bin")) {
        DPL("Weather in Spiffs");
        return true;
    } else {
        DPL("Weather not in Spiffs");
        return false;
    }
}


// Read Weather fom Spiffs
void ReadWeatherFromSPIFF(struct_Weather *Weather) {

    DPL("Get Weather from Spiffs");

    if (!SPIFFS.begin(true)) {
        DPL("An Error has occurred while mounting SPIFFS");
        return;
    }

    File file = SPIFFS.open("/weather.bin", FILE_READ);

    if (!file) {
        DPL("Failed to open file for reading");
        return;
    }

    file.read((uint8_t *) Weather, sizeof(struct_Weather));

    file.close();

    DPL("Done");
}



void StoreWeatherToSpiffs(struct_Weather *Weather) {

    DPL("Store Weather to Spiffs");

    if (!SPIFFS.begin(true)) {
        DPL("An Error has occurred while mounting SPIFFS");
        return;
    }

    File file = SPIFFS.open("/weather.bin", FILE_WRITE);

    if (!file) {
        DPL("Failed to open file for writing");
        return;
    }

    file.write((uint8_t *) Weather, sizeof(struct_Weather));

    file.close();

    DPL("Done");
}

void Load_PaintWeather(GxEPD2_GFX &d) {

    DPL("Load_Paint Weather");

    auto *ptr_Weather = (struct_Weather *) ps_malloc(sizeof(struct_Weather));
    if (ptr_Weather == nullptr) {
        DPL("Error allocating memory for Weather");
    }

    DPF("Free heap: %d/%d %d max block\n", ESP.getFreeHeap(), ESP.getHeapSize(), ESP.getMaxAllocHeap());
    DPF("Free PSRAM: %d/%d %d max block\n", ESP.getFreePsram(), ESP.getPsramSize(), ESP.getMaxAllocPsram());


    GetWeather(ptr_Weather);


    DPL("Build index");
    std::vector<int> hours_for_index;
    if (returnHourIndexFromForecast(schedule, &(ptr_Weather->HourlyWeather), &hours_for_index)) {
        DrawWeatherToDisplay(d, ptr_Weather, hours_for_index);
    } else {
        DPL("Paint weather failed - didnt find an index");
    }

    free(ptr_Weather);
    DPL("Done");
}

void PaintWeather(GxEPD2_GFX &d) {

    DPL("Paint Weather");
    DPF("Free heap: %d/%d %d max block\n", ESP.getFreeHeap(), ESP.getHeapSize(), ESP.getMaxAllocHeap());
    DPF("Free PSRAM: %d/%d %d max block\n", ESP.getFreePsram(), ESP.getPsramSize(), ESP.getMaxAllocPsram());

    auto *ptr_Weather = (struct_Weather *) ps_malloc(sizeof(struct_Weather));
    if (ptr_Weather == nullptr) {
        DPL("Error allocating memory for Weather");
    }

    ReadWeatherFromSPIFF(ptr_Weather);

    DPL("Build index");
    std::vector<int> hours_for_index;
    if (returnHourIndexFromForecast(schedule, &(ptr_Weather->HourlyWeather), &hours_for_index)) {
        DrawWeatherToDisplay(d, ptr_Weather, hours_for_index);
    } else {
        DPL("Paint weather failed - didnt find an index");
    }

    free(ptr_Weather);
}