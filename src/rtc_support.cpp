//
// Created by development on 17.10.21.
//

#include "rtc_support.h"
#include <Arduino.h>
#include <time.h>                       // time() ctime()
#include <sys/time.h>

#include "support.h"
#include <global_hardware.h>

#include <my_RTClib.h>
#include <global_display.h>

extern RTC_DS3231 rtc_watch;

String DateTimeString(DateTime dt) {
    char str_format[] = "hh:mm:ss - DDD, DD.MM.YYYY";
    String str_time_now = dt.toString(str_format);
    return str_time_now;
}


void I2C_Scanner() {
    byte error, address; //variable for error and I2C address
    int nDevices;
    DPL("Scanning I2 Bus...");

    nDevices = 0;
    for (address = 1; address < 127; address++) {
// The i2c_scanner uses the return value of
// the Write.endTransmisstion to see if
// a device did acknowledge to the address.
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0) {
            DP("I2C device found at address 0x");
            if (address < 16)
                DP("0");
            DPF("%x",address);
            DPL("  !");
            nDevices++;
        } else if (error == 4) {
            DP("Unknown error at address 0x");
            if (address < 16)
                DP("0");
            DPF("%x",address);
        }
    }
    if (nDevices == 0)
        DPL("No I2C devices found\n");
    else
        DPL("done\n");

}

/*| specifier | output                                                 |
|-----------|--------------------------------------------------------|
| YYYY      | the year as a 4-digit number (2000--2099)              |
| YY        | the year as a 2-digit number (00--99)                  |
| MM        | the month as a 2-digit number (01--12)                 |
| MMM       | the abbreviated English month name ("Jan"--"Dec")      |
| DD        | the day as a 2-digit number (01--31)                   |
| DDD       | the abbreviated English day of the week ("Mon"--"Sun") |
| AP        | either "AM" or "PM"                                    |
| ap        | either "am" or "pm"                                    |
| hh        | the hour as a 2-digit number (00--23 or 01--12)        |
| mm        | the minute as a 2-digit number (00--59)                |
| ss        | the second as a 2-digit number (00--59)                |*/

DateTime tm_2_datetime(tm timeinfo) {
    return DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

// Three different ways to store time:
// time_t is a 32 bit integer
// tm is a struct with 9 members
// DateTime is a struct with 6 members - adjusted to print with 1900 correction


tm datetime_to_tm(const DateTime& dt) {
    tm timeinfo = {};
    timeinfo.tm_year = dt.year() - 1900; // Years since 1900
    timeinfo.tm_mon = dt.month() - 1;    // Months since January [0-11]
    timeinfo.tm_mday = dt.day();         // Day of the month [1-31]
    timeinfo.tm_hour = dt.hour();        // Hours since midnight [0-23]
    timeinfo.tm_min = dt.minute();       // Minutes after the hour [0-59]
    timeinfo.tm_sec = dt.second();       // Seconds after the minute [0-60]

    // Call mktime to fill in the tm_yday field
    mktime(&timeinfo);

    return timeinfo;
}

tm now_tm() {
    struct tm timeinfo = {};
    time_t now;
    time(&now);
    localtime_r(&now, &timeinfo); //dont apply local time a 2nd time, rtc clock is already in local time.
    return timeinfo;
}

DateTime now_datetime() {
   return tm_2_datetime(now_tm());
}

void espPrintTimeNow() {

    DP("*local* Time from ESP32: ");
    DPL(DateTimeString(now_datetime()));
}

void PrintSerialTime(tm tm) {
    DP(DateTimeString(tm_2_datetime(tm)));
}

String GetSerialTime(tm tm) {
    return DateTimeString(tm_2_datetime(tm));
}

void rtcInit() {
    // initializing the rtc
    if (!rtc_watch.begin()) {
        DPL("Couldn't find RTC!");
        abort();
    }

    // stop oscillating signals at SQW Pin
    // otherwise setAlarm1 will fail
    rtc_watch.writeSqwPinMode(DS3231_OFF);
    rtc_watch.disable32K();

    DPL("RTC Init: OK");
}

void rtcSetRTCFromInternet() {
    // RTC is programmed in *local time*
    // Set timezone to Berlin Standard Time

    espPrintTimeNow();
    struct tm timeinfo = {};
    time_t now;
    time(&now);
    localtime_r(&now, &timeinfo);
    rtc_watch.adjust(tm_2_datetime(timeinfo));
    delay(100);
    DP("RTC Time Set: ");DPL(DateTimeString(rtc_watch.now()));
}

void rtsSetEspTime(DateTime dt) {
    timeval tv{};
    const timezone tz = {0, 0};
    tv.tv_sec = dt.unixtime();
    tv.tv_usec = 0;  // keine Mikrosekunden setzen
    settimeofday(&tv, &tz);
    espPrintTimeNow();

}


/**************************************************************************/
/*!
    @brief  Get the current date/time
    @return DateTime object with the current date/time
*/
/**************************************************************************/
static uint8_t bcd2bin(uint8_t val) { return val - 6 * (val >> 4); }

