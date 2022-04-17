//
// Created by development on 29.06.21.
//

#define UP true
#define DOWN false

#include <Arduino.h>
#include <StreamString.h>
//
#include <Arduino.h>
// EDP
#include <GxEPD2_BW.h>
#include <GxEPD2_GFX.h>
#include "GxEPD2_display_selection_new_style.h"
/*
#include <Fonts/FreeSerif24pt7b.h>
#include "fonts/FreeSansNumOnly35.h"
#include "fonts/FreeSansNumOnly40.h"
#include "fonts/FreeSansNumOnly45.h"
#include "fonts/FreeSansNumOnly48.h"
#include "fonts/FreeSansNumOnly50.h"
#include "fonts/FreeSansNumOnly55.h"
*/

#include "custom_fonts/FreeSansNumOnly70.h"

#include <global_hardware.h>

#include <analogWrite.h>



const uint16_t pwmtable_16[256] PROGMEM =
        {
                0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3,
                3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 7,
                7, 7, 8, 8, 8, 9, 9, 10, 10, 10, 11, 11, 12, 12, 13, 13, 14, 15,
                15, 16, 17, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
                31, 32, 33, 35, 36, 38, 40, 41, 43, 45, 47, 49, 52, 54, 56, 59,
                61, 64, 67, 70, 73, 76, 79, 83, 87, 91, 95, 99, 103, 108, 112,
                117, 123, 128, 134, 140, 146, 152, 159, 166, 173, 181, 189, 197,
                206, 215, 225, 235, 245, 256, 267, 279, 292, 304, 318, 332, 347,
                362, 378, 395, 412, 431, 450, 470, 490, 512, 535, 558, 583, 609,
                636, 664, 693, 724, 756, 790, 825, 861, 899, 939, 981, 1024, 1069,
                1117, 1166, 1218, 1272, 1328, 1387, 1448, 1512, 1579, 1649, 1722,
                1798, 1878, 1961, 2048, 2139, 2233, 2332, 2435, 2543, 2656, 2773,
                2896, 3025, 3158, 3298, 3444, 3597, 3756, 3922, 4096, 4277, 4467,
                4664, 4871, 5087, 5312, 5547, 5793, 6049, 6317, 6596, 6889, 7194,
                7512, 7845, 8192, 8555, 8933, 9329, 9742, 10173, 10624, 11094,
                11585, 12098, 12634, 13193, 13777, 14387, 15024, 15689, 16384,
                17109, 17867, 18658, 19484, 20346, 21247, 22188, 23170, 24196,
                25267, 26386, 27554, 28774, 30048, 31378, 32768, 34218, 35733,
                37315, 38967, 40693, 42494, 44376, 46340, 48392, 50534, 52772,
                55108, 57548, 60096, 62757, 65535
        };

// EPD GX Config

class BitmapDisplay {
private:
    GxEPD2_GFX &display;
public:
    BitmapDisplay(GxEPD2_GFX &_display) : display(_display) {};

    void drawBitmaps();

private:
    void drawBitmaps400x300();
};

void pwm_up_down(boolean direction_up, const uint16_t pwm_table[], int16_t size, uint16_t delay_ms) {
    int16_t tmp;
    analogWriteResolution(12);
    if (direction_up) {

        Serial.println("Start-UP");

        for (tmp = 0; tmp < size; tmp++) {
            uint16_t out = pgm_read_word (&pwm_table[tmp]);
            analogWrite(DISPLAY_CONTROL, out);
            delay(delay_ms);
        }
    } else {

        Serial.println("Start-Down");

        for (tmp = size - 1; tmp >= 0; tmp--) {
            uint16_t out = pgm_read_word (&pwm_table[tmp]);
            analogWrite(DISPLAY_CONTROL, out);
            delay(delay_ms);
        }
    }
}

void run_up_down_task( void * parameter ){
#define MAX_LIGHT_LEVEL 128

    bool direction_up = *((bool*)parameter);
    const int freq = 120;
    const int ledChannel = 0;
    const int resolution = 8;
    ledcSetup(ledChannel, freq, resolution);
    ledcAttachPin(DISPLAY_CONTROL, ledChannel);

    if (direction_up) {
        for (int dutyCycle = 0; dutyCycle <= MAX_LIGHT_LEVEL; dutyCycle++) {
            // changing the LED brightness with PWM
            ledcWrite(ledChannel, dutyCycle);
            delay(10);
        }
    } else {
        for(int dutyCycle = MAX_LIGHT_LEVEL; dutyCycle >= 0; dutyCycle--){
            // changing the LED brightness with PWM
            ledcWrite(ledChannel, dutyCycle);
            delay(3);
        }
    }
    vTaskDelete(NULL);
}


void pwm_up_down_esp32(boolean direction_up) {

    xTaskCreate(
            run_up_down_task,    // Function that should be called
            "run_up_down_task",  // Name of the task (for debugging)
            1000,            // Stack size (bytes)
            (void *) &direction_up,            // Parameter to pass
            1,               // Task priority
            NULL             // Task handle
    );
}



void helloValue(GxEPD2_GFX &display, double v, int digits) {
    Serial.print("helloValue:");
    Serial.println(v);
    display.setRotation(0);
    display.setFont(&FreeSans70pt7b);
    display.setTextColor(GxEPD_BLACK);
    StreamString valueString;
    valueString.print(v, digits);
    Serial.println(valueString);
    int16_t tbx, tby;
    uint16_t tbw, tbh;
    display.getTextBounds(valueString, 0, 0, &tbx, &tby, &tbw, &tbh);
    uint16_t x = ((display.width() - tbw) / 2) - tbx;
    uint16_t y = (display.height() * 2 / 4) + tbh / 2; // y is base line!
    // show what happens, if we use the bounding box for partial window
    uint16_t wx = (display.width() - tbw) / 2;
    uint16_t wy = (display.height() * 2 / 4) - tbh / 2;
    display.setPartialWindow(wx, wy, tbw, tbh);
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(x, y);
        display.print(valueString);
    } while (display.nextPage());

}
