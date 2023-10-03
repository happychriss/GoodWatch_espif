// check out https://rudo.info/mosmix.php
// https://github.com/jolichter/dwdWeather/blob/master/dwdWeather.php
#include "weather.h"
#include "pugixml.hpp"
#include <iostream>
#include <string>
#include <cmath>
#include <HTTPClient.h>
#include "miniz.h"
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "support.h"
#include "wifi_support.h"
#include "rtc_support.h"
#include "paint_weather.h"
#include "my_RTClib.h"
#include "rtc_support.h"
#include "paint_alarm.h"

#define DEST_FS_USES_SPIFFS


#include "support.h"

bool b_wait_weather_data = true;
extern RTC_DS3231 rtc_watch;

int mps_to_bft(double windSpeed) {
    for (const auto &scale: beaufortScale) {
        if ((windSpeed >= scale.lowerBound) && (scale.upperBound == -1 || windSpeed <= scale.upperBound)) {
            return scale.beaufortNumber;
        }
    }
    return -1; // This line should never be reached, added for completeness
}



void printHourlyWeather(struct_HourlyWeather hw) {
    PrintSerialTime(hw.time);
    DPF(" (%d)\t", hw.hour_index);
    DPF("(%d): %s\t", hw.forecast_id, getWeatherString(hw.forecast_id).c_str());
    DPF(" %f C\t", hw.temperature);
    DPF(" %f Bft\t", hw.wind);
    DPF("Rain: %f mm\t", hw.rain);
    DPF("Sun: %f min\t", hw.sun);
    DPF("Clouds: %f %\n", hw.clouds);
}

String getWeatherString(int number) {
    auto it = Weather::weatherData.find(number);
    if (it != Weather::weatherData.end()) {
        return it->second;
    } else {
        return "Unknown weather code";
    }
}


//  pio run -t downloadfs
// this functions downloads zip file from DWD and zips the file to memory

char *downloadKML(const String fetch_url, size_t *buffer_len) {


    DPL("Enabling Wifi");
    SetupWifi();


//    Serial.printf("Fetching URL: %s\n", url.c_str());
    DPL("Creating HTTP client...");
    auto *client = new WiFiClientSecure;
    client->setInsecure();
    HTTPClient http;
    http.begin(*client, DWD_URL);
    int httpCode = http.GET();
    DP("HTTP client created.");

    if (httpCode != HTTP_CODE_OK) {
        DPL("Failed to fetch the URL.");
        return nullptr;
    }

    // get length of document (is -1 when Server sends no Content-Length header)
    int len = http.getSize();
    int file_len = len;


    if (len == -1) {
        DPL("Server did not send Content-Length header.");
        return nullptr;
    }

    DPF("Content-Length: %d\n", len);

    // Allocate buffer dynamically and use free pointer (tip from chatpgt)
    auto freeMemory = [](void *ptr) { free(ptr); };

    std::unique_ptr<char, decltype(freeMemory)> buffer(static_cast<char *>(ps_calloc(file_len + 1, sizeof(char))),
                                                       freeMemory);

    if (buffer == nullptr) {
        DPL("Failed to allocate memory for the file.");
        return nullptr;
    }

    char *ptr_buffer = buffer.get();
    FILE *file = fmemopen(ptr_buffer, file_len, "w");


    //     FILE *file = fopen(MOSMIX_ZIP_FILE, FILE_WRITE);

    if (!file) {
        DPL("Failed to create the file.");
        return nullptr;
    }

    DP("Opened downloaded zip file...read file");
    // create buffer for read
    uint8_t buff[128] = {0};

    // get tcp stream
    WiFiClient *stream = http.getStreamPtr();

    // read all data from server
    while (http.connected() && (len > 0 || len == -1)) {
        // get available data size
        size_t size = stream->available();

        if (size) {
            // read up to 128 byte
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
            DP(".");
            fwrite(buff, 1, static_cast<size_t>(c), file);
            if (len > 0) {
                len -= c;
            }
        }
        delay(1);
    }
    DPL("Done reading file.");

    fclose(file);

    http.end();
    DPL("Done saving the file");

#undef print_file
#ifdef print_file
    DPL("Printing first 10 bytes of the file: ");

    for(int i = 0; i < 50; i++) {
        char c = *(buffer+ i);
        DPF("%d: ",i);
        Serial.println(c,HEX);
    }

    DPL("Printing last 10 bytes of the file:");
    for(int i = 0; i <50; i++) {
        char c = *(buffer+file_len-(50 - i));
        DPF("%d: ",i);
        Serial.print(c,HEX);
        Serial.print("-");
        Serial.println(c);
    }
    DPL("Done printing file.");
#endif
    FILE *file2 = fmemopen(ptr_buffer, file_len, "r");
    // Unzipping using miniz
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));
    DPL("Initializing miniz...");
    // https://www.esp32.com/viewtopic.php?f=13&t=1076

    if (!mz_zip_reader_init_file_v3(&zip_archive, file2, 0, 0, 0)) {
        DPL("Error initializing miniz.");
        mz_zip_error mzerr = mz_zip_get_last_error(&zip_archive);
        DP(mz_zip_get_error_string(mzerr));
        return nullptr;
    }

    DPL("Initialized miniz.");
    mz_uint num_files = mz_zip_reader_get_num_files(&zip_archive);
    if (num_files <= 0) {
        DPL("The zip file does not contain any files.");
        mz_zip_reader_end(&zip_archive);
        return nullptr;
    }

    DPF("The zip file contains %d files.\n", num_files);
    // Extract the first file
    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(&zip_archive, 0, &file_stat)) {
        DPL("Error retrieving the file information from ZIP archive.");

        mz_zip_reader_end(&zip_archive);
        return nullptr;
    }
    DPF("The first file name in the zip is: %s\n", file_stat.m_filename);
    mz_zip_error mzZipError;
    size_t heapSize = esp_get_free_heap_size();
    DPF("Heap size: %d\n", heapSize);

    // Allocate buffer dynamically
    char *unzip_buffer = (char *) calloc(file_stat.m_uncomp_size + 1, sizeof(char));  // +1 for null-terminator
    if (unzip_buffer == nullptr) {
        DPL("Failed to allocate memory for the unzip buffer.");
        return nullptr;
    }

    if (!mz_zip_reader_extract_to_mem_no_alloc(
            &zip_archive,
            0,
            unzip_buffer,
            file_stat.m_uncomp_size,
            0,
            nullptr,
            0)) {
        DPL("Error extracting the file: ");
        DPL(mz_zip_get_error_string(mzZipError));
        mz_zip_error mzerr = mz_zip_get_last_error(&zip_archive);
        DPL(mz_zip_get_error_string(mzerr));
        mz_zip_reader_end(&zip_archive);
        return nullptr;
    }

    *buffer_len = file_stat.m_uncomp_size;

    DPL("Done");

    return unzip_buffer;
}


// DWD data has index of hours from last forecast time - identify index of hours to use for accessing forecast data
// also
std::pair<int, int>
get_hour_index_for_time(struct_Weather *ptr_weather,
                        std::tm *forecast_time,
                        pugi::xml_node dt_root) {

    DPL("get_hour_index_for_time");

    int hours_index = 0;
    int hours_index_result = 0;
    int update_index = 0;
    bool b_found = false;

    struct tm TimeStep{};

    time_t fc_time = mktime(forecast_time);

    for (pugi::xml_node dt: dt_root.children("dwd:TimeStep")) {

        std::istringstream w_time(dt.first_child().value());
        DP(w_time.str().c_str());
        w_time >> std::get_time(&TimeStep, "%Y-%m-%dT%H:%M:%S"); // or just %T in this case
        time_t read_time = mktime(&TimeStep);

        DPF(" ----------- %d:%d:%d - Yday: %d ", TimeStep.tm_hour, TimeStep.tm_min, TimeStep.tm_sec, TimeStep.tm_yday);

        if (!b_found) {
            double seconds = difftime(read_time, fc_time);
            DPF(" - Seconds: %dn", seconds);
            if (seconds > 0) {
                DPF("  <<<<<< **Found** with Seconds: %d", seconds);
                b_found = true;
                hours_index_result = hours_index;
            } else {
                DP("  <<<<<< Not found");
            }
        }

        if (b_found) {
            DPF(" - Update index: %d for hour: %d\n", update_index, TimeStep.tm_hour);
            ptr_weather->HourlyWeather[update_index].time = TimeStep;
            ptr_weather->HourlyWeather[update_index].hour_index = hours_index;
            update_index++;
        } else {
            DPF(" - Skip index:  %d for hour: %d\n", update_index, TimeStep.tm_hour);
        }

        if (update_index == HOURS_FORECAST) {
            break;
        }

        hours_index++;

    }

    return std::make_pair(hours_index_result, TimeStep.tm_hour);
}


bool
extractNumbersFromString(const std::string &inputString, std::vector<double> &numbers, int hours_index, bool b_print) {

    int i = 0;
    size_t len = inputString.length();
    std::string numStr;
    bool b_has_sign = false;
    bool b_has_number = false;
    int index_numbers_found = 0;

    while (i < len && numbers.size() < HOURS_FORECAST) {

        // skip spaces
        while (i < len) {
            if (inputString[i] != ' ')
                break;
            ++i;
        }

        // check for + or minus signs
        if (i < len && (inputString[i] == '+' || inputString[i] == '-')) {
            numStr += inputString[i];
            b_has_sign = true;
            i++;
        }

        // check for numbers with digit separator "."
        while (i < len && (std::isdigit(inputString[i]) || inputString[i] == '.')) {
            numStr += inputString[i];
            b_has_number = true;
            i++;
        }

        if (b_has_sign && !b_has_number) {
            numStr = "0";
            b_has_number = true;
        }

        if (b_has_number) {

            size_t pos;
            double number = std::stod(numStr, &pos);

            if (pos == numStr.length()) {

                if (index_numbers_found >= hours_index) {
                    numbers.push_back(number);
                    if (b_print)
                        DPF("Get[%d] - Result Number %d - %f \n", index_numbers_found, numbers.size(), number);

                } else {
                    if (b_print)
                        DPF("Skip[%d] Number: %f\n", index_numbers_found, number);
                    index_numbers_found++;
                }

            } else {
                DPF("!!!!!!!!!!!!!!!! Error converting string to double: %s no number!\n", numStr.c_str());
                return false;
            }

            numStr.clear();
            b_has_number = false;
            b_has_sign = false;
        }
    }

    return true;
}

std::vector<double>
getForcast(pugi::xml_node dforecast_root, int hours_index, const std::string &search_elementName) {

    std::vector<double> forecast{};
    DPF("getForecast: %s - %d\n", search_elementName.c_str(), hours_index);

    for (pugi::xml_node dt_fc: dforecast_root.children("dwd:Forecast")) {

        std::string elementName = dt_fc.attribute("dwd:elementName").value();
        if (elementName == search_elementName) {

            std::string data = dt_fc.child("dwd:value").text().get();
            // all forecast values are in one string - extract the numbers

            // compare elementName with string SunD1
            if (elementName == "Neff") {
                extractNumbersFromString(data, forecast, hours_index, true);
            } else {
                extractNumbersFromString(data, forecast, hours_index, false);
            }

            break;
        }

    }

    return forecast;
}


int determineWeatherIcon(const struct_HourlyWeather &hw) {
//    struct struct_HourlyWeather {
//        tm time;
//        int hour;
//        double temperature;
//        double wind;
//        double rain;
//        double clouds;
//        double sun;
//        std::string forecast;
//        int forecast_id;
//    };

    int fc_icon = NO_ICON_FOUND;

    // check in DWD weather mapping if we find a fitting icon for DWD weather - if yes-take this
    for (auto weather_id_map: weather_id_maps) {
        if (weather_id_map.dwd_id == hw.forecast_id) {
            DPF("Checking Icon from DWD:  %d with forecast %d\n", weather_id_map.icon_id, hw.forecast_id);
            fc_icon = weather_id_map.icon_id;
            break;
        }
    }

    if (fc_icon != NO_ICON_FOUND) {
        DPF("Found Icon from DWD:  %d\n", fc_icon);
        return fc_icon;
    }

    DPL("No Icon found from DWD - checking alternative icons");

    if (hw.rain == 0) {

        // Check for the sun - measured in minutes
        if ((hw.sun > 55) || (hw.clouds < 5)) {
            return wi_Sunny;
        }

        if ((hw.sun > 45) || (hw.clouds < 10)) {
            return wi_MostlySunny;
        }

        if ((hw.sun > 30) || (hw.clouds < 20)) {
            return wi_PartlySunny;
        }

        if ((hw.sun > 10) || (hw.clouds < 40)) {
            return wi_PartlyCloudy;
        }

        if (hw.clouds < 60) {
            return wi_PartlyCloudy;
        }

        return wi_Cloudy;
    }

    if (hw.rain < 0.2) {
        return wi_ChanceRain;
    }

    if (hw.rain < 0.5) {
        return wi_Rain;
    }

    return wi_Cloudy;

}


void DWD_Weather(struct_Weather *ptr_myWF) {

    DPL("DWD_Weather");
    struct_Weather &myWF = *ptr_myWF;

    DPL("Downloading KML file...");
    String localFilePath;
    size_t p_len;
    char *p = downloadKML(DWD_URL, &p_len);
//    char *str = (char *)p;
//    DPL(str);

    if (p) {
        DPL("Parsing KML file...");
        pugi::xml_document dwd_xml;

        // Assuming the data in the buffer is null-terminated, otherwise you'd also need to pass the size
        pugi::xml_parse_result result = dwd_xml.load_buffer(p, p_len);
        if (result) {
            DPL("KML file parsed successfully.");

            // Read Issue Time
            pugi::xml_node dt_time = dwd_xml.child("kml:kml").
                    child("kml:Document").
                    child("kml:ExtendedData").
                    child("dwd:ProductDefinition").
                    child("dwd:IssueTime");

            DPF("dt_time: %s\n", dt_time.first_child().value());

            struct tm IssueTime{};
            memset(&IssueTime, 0, sizeof(IssueTime));
            // Use sscanf to extract date and time information

// Use sscanf to extract date and time information
            sscanf(dt_time.first_child().value(), "%d-%d-%dT%d:%d:%d",
                   &IssueTime.tm_year,
                   &IssueTime.tm_mon,
                   &IssueTime.tm_mday,
                   &IssueTime.tm_hour,
                   &IssueTime.tm_min,
                   &IssueTime.tm_sec);

            // Adjust the values to fit the 'tm' structure
            IssueTime.tm_year -= 1900;
            IssueTime.tm_mon -= 1;

/*

            std::istringstream w_time(dt_time.first_child().value());


            std::tm IssueTime{};
            w_time >> std::get_time(&IssueTime, "%Y-%m-%dT%H:%M:%S"); // or just %T in this case
*/
            DP("IssueTime: ");
            PrintSerialTime(IssueTime);
            DPL("");

            myWF.publish_time = IssueTime;

            // Extract hours into hours_index

            DPL("Parsing Hours!");

            pugi::xml_node dt_root = dwd_xml.child("kml:kml").
                    child("kml:Document").
                    child("kml:ExtendedData").
                    child("dwd:ProductDefinition").
                    child("dwd:ForecastTimeSteps");


            tm now = now_tm();


            PrintSerialTime(now);
            DPL("<-Current time");

            int hours_index, hours_offset;
            std::tie(hours_index, hours_offset) = get_hour_index_for_time(ptr_myWF, &now, dt_root);

            // Extract weather data - jump to the right hour in

            pugi::xml_node dforecast_root = dwd_xml.
                    child("kml:kml").
                    child("kml:Document").
                    child("kml:Placemark").
                    child("kml:ExtendedData");
            DPF("dforecast_root name: %s\n", dforecast_root.name());
            DPF("Read the symbols!");
            // Get all the weather data for the next 24 hours and store in HourlyWeather

            // ************** Get Weather Symbol ************** by using the forecast value for ww

            std::vector<double> forecast_value = getForcast(dforecast_root, hours_index, "ww");

            for (int i = 0; i < forecast_value.size(); ++i) {
//                DPF("Hour %d: %s\n", i, getWeatherString((int) forecast_value[i]).c_str());
                myWF.HourlyWeather[i].forecast_id = (int) forecast_value[i];
            }


            // ************** Get Temperature ************** by using the forecast value for TTT
            forecast_value = getForcast(dforecast_root, hours_index, "TTT");

            for (int i = 0; i < forecast_value.size(); ++i) {
//                DPF("Hour %d: %f C\n", i, forecast_value[i] - 273.15);
                myWF.HourlyWeather[i].temperature = forecast_value[i] - 273.15;
            }


            // ************** Get Rain **************10 kg/m² Niederschlag entsprechen ungefähr 10 mm Niederschlag


            forecast_value = getForcast(dforecast_root, hours_index, "RR1c");
            for (int i = 0; i < forecast_value.size(); ++i) {
//                DPF("Hour %d: %f mm\n", i, forecast_value[i]);
                myWF.HourlyWeather[i].rain = forecast_value[i];
            }


            // ************** Get Wind **************

            forecast_value = getForcast(dforecast_root, hours_index, "FF");

            for (int i = 0; i < forecast_value.size(); ++i) {
                auto wind_beaufort = static_cast<double >(std::round(forecast_value[i] * forecast_value[i] / 3.01));
                // DPF( "Hour %d: %f Bft\n", i, wind_beaufort);
                myWF.HourlyWeather[i].wind = mps_to_bft(forecast_value[i]);
            }


            // ************** Get Sun **************
            forecast_value = getForcast(dforecast_root, hours_index, "SunD1");

            for (int i = 0; i < forecast_value.size(); ++i) {

                myWF.HourlyWeather[i].sun = static_cast<double >(std::round(forecast_value[i] / 60));
            }


            // ************** Get Cloud **************

            forecast_value = getForcast(dforecast_root, hours_index, "Neff");

            for (int i = 0; i < forecast_value.size(); ++i) {
                myWF.HourlyWeather[i].clouds = forecast_value[i];
            }


            for (auto &hw: myWF.HourlyWeather) {
                printHourlyWeather(hw);
            }

            DPF("Done parsing weather data.");

        } else {
            DPL("ERROR: Failed to load and parse the KML file:");
            DPL(result.description());
        }

        DPL("Done");


    } else {
        DPL("Failed to load and parse the KML file.");
    }

}


// Task to get weather data

void DWD_WeatherTask(void *pvParameters) {
    Serial.begin(115200);
    DPL("Getting weather data by running Task...");

    auto *ptr_my_Weather = (struct_Weather *) pvParameters;

    DWD_Weather(ptr_my_Weather);
    DPL("Task - done getting weather data.");
    b_wait_weather_data = true;
    delay(1000);
    vTaskDelete(NULL);

}

void GetWeather(struct_Weather *ptr_my_Weather) {
    DPL("Getting weather data by starting task...");

    b_wait_weather_data = false;

    xTaskCreatePinnedToCore(
            DWD_WeatherTask,   /* Function to implement the task */
            "DWD_WeatherTask", /* Name of the task */
            40000,      /* Stack size in words */
            (void *) ptr_my_Weather,       /* Task input parameter */
            0,          /* Priority of the task */
            NULL,       /* Task handle. */
            1);  /* Core where the task should run */


    while (!b_wait_weather_data) {
        delay(500);
    }

    DPL("Done getting weather data.");
    b_wait_weather_data = true;
}

bool returnHourIndexFromForecast(
        const std::vector<struct_forecast_schedule> &schedule,
        const std::array<struct_HourlyWeather, HOURS_FORECAST> *ptr_hourly_weather,
        std::vector<int> *hours_index) {

    // loop through schedule and find based on current time the right forecast

    tm now= now_tm();

    const struct_forecast_schedule *forecasts_ptr = nullptr;

    bool b_found = false;
    for (auto &schedule_line: schedule) {
        DPF("Checking schedule line: %d\n", schedule_line.check_hour);
        if (now.tm_hour <= schedule_line.check_hour) {
            DPL("Found schedule line");
            forecasts_ptr = &schedule_line;
            b_found = true;
            break;
        }
    }

    if (!b_found) {
        DPL("No schedule line found - woke up too early");
        return false;
    }

    for (const auto &hour_forecast: forecasts_ptr->forecast_hours) {

        int check_yday = now.tm_yday;
        if (hour_forecast < now.tm_hour) {
            check_yday++;
            DPF("Adding 1 to yday\n");
        }
        DPF("Looking for forecast at: %d YDay: %d\n", hour_forecast, check_yday);

        int hour_index = 0;
        bool b_found_index = false;
        for (auto &hw: *ptr_hourly_weather) {

            if ((hw.time.tm_hour == hour_forecast) && (hw.time.tm_yday == check_yday)) {
                DPF("FOUND: Index: %d - Hour: %d - Wday: %d\n", hour_index, hw.time.tm_hour, hw.time.tm_yday);
                b_found_index = true;
                break;

            } else {
                DPF("Check: Index: %d - Hour: %d - Wday: %d\n", hour_index, hw.time.tm_hour, hw.time.tm_yday);
            }

            hour_index++;
        }

        if (b_found_index) {
            hours_index->push_back(hour_index);
            DPF("Added index: %d\n", hour_index);
        } else {
            DPL("No index found");
        }

    }

    return true;
}

void CheckAndPrepareWeather() {
#define PREPARE_WAKEUP 15
    /* Prepare for Alarm wakeup ***************************************************************************/
    DateTime rtc_alarm = {};
    DateTime now = now_datetime();

    if (rtc_watch.getAlarm2(&rtc_alarm, now)) {
        DateTime prepare_wakeuptime = rtc_alarm - TimeSpan(0, 0, PREPARE_WAKEUP, 0);
        DPF("Time Now: %s\n",DateTimeString(now).c_str());
        DPF("Prep Wakeup time: %s\n",DateTimeString(prepare_wakeuptime).c_str());
        TimeSpan diff = now -prepare_wakeuptime ;

        DPF("Time for prepare-wakupe: Days: %i Hours: %i Minutes:%i\n", diff.days(), diff.hours(),
            diff.minutes());

        // If we have x minute before alarm and dont have any weather data in SPIFFS - get it

        if (
                (diff.minutes()>=0) && (diff.minutes()< PREPARE_WAKEUP) &&
                (diff.hours() == 0) && (diff.days() == 0))
        {

            if (!CheckWeatherInSPIFF()) {
                DPL("!!! No Weather in SPIFF - get fresh weather data");
                auto *ptr_Weather = (struct_Weather *) ps_malloc(sizeof(struct_Weather));
                if (ptr_Weather == nullptr) {
                    DPL("Error allocating memory for Weather");
                }

                DPL("!!! Prepare Wakeup activities");
                DPF("Free heap: %d/%d %d max block\n", ESP.getFreeHeap(), ESP.getHeapSize(), ESP.getMaxAllocHeap());
                DPF("Free PSRAM: %d/%d %d max block\n", ESP.getFreePsram(), ESP.getPsramSize(), ESP.getMaxAllocPsram());

                GetWeather(ptr_Weather);
                StoreWeatherToSpiffs(ptr_Weather);

            } else {
                DPL("!!! Prepare Wakeup activities - Weather data already in SPIFFS");
            }

        } else {
            DPL("!!! Alarm Found - not time do prepare wakeup");
        }
    } else {
        DPL("!!! No Alarm found on RTC-Data - dont do anything");
    }
}
