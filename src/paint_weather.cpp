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
    d.setCursor(COLUMN_0_Time, WF_HEIGHT * 3 + y_offset);
    d.print(timeStr);

    // Forecast
    d.setCursor(COLUMN_2_FORECAST, WF_HEIGHT * 3 + y_offset);
    d.print((String(fc.temperature) + " °C   " + String(fc.rain) + " mm").c_str());

    d.setFont(&FreeSans9pt7b);
    if (fc.forecast_id > 3) {
        d.setCursor(COLUMN_1_ICON + X_OFF, WF_HEIGHT * 5 + y_offset);
        d.print(getWeatherString(fc.forecast_id).c_str());
    }

}


void DrawWeatherToDisplay(GxEPD2_GFX &d, struct_AlarmWeather *Weather) {

    DPL("Draw WeatherToDisplay:");
    printHourlyWeather(Weather->HourlyWeather[0]);
    printHourlyWeather(Weather->HourlyWeather[1]);

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
        d.setCursor(d.width() - (tbw + X_OFF), Y_OFF);
        d.print(str_time_publish_time);

        // First Weather Entry
        fc = Weather->HourlyWeather[0];
        printOnForecast(d, fc, 0);
        int weather_icon = determineWeatherIcon(fc);
        DPF("Painting Weather icon: %d\n", weather_icon);
        my_drawBMP(d, weather_names[weather_icon], COLUMN_1_ICON + 8, WF_HEIGHT);

        // Second Weather Entry
        fc = Weather->HourlyWeather[1];
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
// add string to return a result message


bool GetValidForecast(DateTime set_alarm_time, struct_AlarmWeather *pWeather, String &resultMessage) {
    DPL("Checking and Loading valid forecast from Spiffs");

    if (!SPIFFS.begin(true)) {
        resultMessage = "MountError";
        return false;
    }

    if (!SPIFFS.exists("/weather.bin")) {
        resultMessage = "No Spiffs";
        return false;
    }

    File file = SPIFFS.open("/weather.bin", FILE_READ);

    if (!file) {
        resultMessage = "Read Error\n";
        return false;
    }

    size_t length = file.read((uint8_t *) pWeather, sizeof(struct_AlarmWeather));
    resultMessage = resultMessage + "Read " + String(length) + " bytes from SPIFFS\n";
    file.close();

    DPF("Validating CRC Code: %lu\n ",pWeather->crc);
    if (pWeather->crc != calculateWeatherCRC(*pWeather)) {
        resultMessage += "CRC invalid\n";
        return false;
    }

    // Check Alarm time matches set_alarm   time
    DPL("Validating Alarm time");
    std::tm set_alarm_tm = datetime_to_tm(set_alarm_time);
    if ((pWeather->alarm_time.tm_yday != set_alarm_tm.tm_yday) ||
        (pWeather->alarm_time.tm_hour != set_alarm_tm.tm_hour)) {
        resultMessage += "Alarm time not valid\n";
        return false;
    }

    resultMessage += "Wetter\n";
    return true;
}


// Read Weather fom Spiffs
void ReadWeatherFromSPIFF(struct_AlarmWeather *pWeather) {

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

    size_t length = file.read((uint8_t *) pWeather, sizeof(struct_Weather));
    DPF("Read %d bytes from SPIFFS\n", length);

    file.close();

    DPF("Read Weather from Spiffs with CRC: %lu\n", pWeather->crc);

    DPL("Done");
}


void StoreWeatherToSpiffs(struct_AlarmWeather *pWeather) {


    DPF("Store Weather to Spiffs with CRC: %lu\n", pWeather->crc);

    if (!SPIFFS.begin(true)) {
        DPL("An Error has occurred while mounting SPIFFS");
        return;
    }

    File file = SPIFFS.open("/weather.bin", FILE_WRITE);

    if (!file) {
        DPL("Failed to open file for writing");
        return;
    }

    size_t length = file.write((uint8_t *) pWeather, sizeof(struct_Weather));
    DPF("Wrote %d bytes to SPIFFS\n", length);

    file.close();

    DPL("Done");
}

//*void Load_PaintWeather(GxEPD2_GFX &d) {
//
//    DPL("Load_Paint Weather");
//
//    auto *ptr_Weather = (struct_Weather *) ps_malloc(sizeof(struct_Weather));
//    if (ptr_Weather == nullptr) {
//        DPL("Error allocating memory for Weather");
//    }
//
//    DPF("Free heap: %d/%d %d max block\n", ESP.getFreeHeap(), ESP.getHeapSize(), ESP.getMaxAllocHeap());
//    DPF("Free PSRAM: %d/%d %d max block\n", ESP.getFreePsram(), ESP.getPsramSize(), ESP.getMaxAllocPsram());
//
//
//    GetWeather(ptr_Weather);
//
//
//    DPL("Build index");
//    std::vector<int> hours_for_index;
//    if (returnHourIndexFromForecast(schedule, &(ptr_Weather->HourlyWeather), &hours_for_index)) {
//        DrawWeatherToDisplay(d, ptr_Weather, hours_for_index);
//    } else {
//        DPL("Paint weather failed - didnt find an index");
//    }
//
//    free(ptr_Weather);
//    DPL("Done");
//}*/

