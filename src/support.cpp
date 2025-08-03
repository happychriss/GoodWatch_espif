//
// Created by development on 29.06.21.
//

#include "Arduino.h"
#include "support.h"
#include "freertos/FreeRTOS.h"
#include <my_RTClib.h>
#include <sys/time.h>

extern RTC_DS1307 rtc_watch;



int SerialKeyWait() {// Wait for Key
//    Serial.setDebugOutput(true);

    Serial.println("Hit a key to start...");
    Serial.flush();

    while (true) {
        int inbyte = Serial.available();
        delay(500);
        if (inbyte > 0) break;
    }

    return Serial.read();

}

esp_sleep_wakeup_cause_t print_wakeup_reason() {
    esp_sleep_wakeup_cause_t wakeup_reason;

    wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0 :
            DPL("Wakeup caused by external signal using RTC_IO");
            break;
        case ESP_SLEEP_WAKEUP_EXT1 :
            DPL("Wakeup caused by external signal using RTC_CNTL");
            break;
        case ESP_SLEEP_WAKEUP_TIMER :
            DPL("Wakeup caused by timer");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD :
            DPL("Wakeup caused by touchpad");
            break;
        case ESP_SLEEP_WAKEUP_ULP :
            DPL("Wakeup caused by ULP program");
            break;
        default :
            DPL("Normal Boot Process");
            break;
    }
    return wakeup_reason;
}

void waitAndPrint(const char* message) {
    Serial.print(message); // Write the message to the serial port
    Serial.println(" ...press key!");

    // Wait for any key press
    while (!Serial.available()) {
        delay(100);
        // Wait until at least one byte is available in the input buffer
    }

    // Read the pressed key
    char keyPressed = Serial.read(); // Read the first available byte


    // Clear the input buffer
    while (Serial.available()) {
        Serial.read(); // Clear any remaining bytes in the input buffer
    }
}