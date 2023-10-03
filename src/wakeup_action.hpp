#include "Arduino.h"
#include "Audio.h"
#include "wakeup_action.h"
#include "esp_wifi.h"
#include "SPIFFS.h"
#include "weather.h"
#include "global_display.h"
#include "paint_support.h"


void audio_info(const char *info) {
    Serial.print("info        ");
    Serial.println(info);
}

void audio_eof_mp3(const char *info) {  //end of file
    Serial.print("eof_mp3     ");
    Serial.println(info);
    b_audio_end_of_mp3 = true;
}

void fade_in(void *parameter) {
    for (int i = 0; i < 21; i++) {
        audio.setVolume(i);
        vTaskDelay(pdMS_TO_TICKS(1000));
        DPF("INT Fade-In Volume: %i\n", i);
        if (b_pir_wave) break;
    }
    vTaskSuspend(NULL);
}

void fade_out(void *parameter) {
    int vol = audio.getVolume();
    DPF("End of Song, PIR was raised with Volume: %i\n", vol);

    for (int i = vol; i > 0; i--) {
        audio.setVolume(i);
        vTaskDelay(pdMS_TO_TICKS(200));
        DPF("INT Fade-Out Volume: %i\n", i);
    }
    b_audio_finished = true;
    vTaskDelete(NULL);
}


void WakeUpRoutine(GxEPD2_GFX &d) {

    // Check if Weather Forecast exists and display message on screen
    String str_weather="Kein Wetter!";
    bool b_weather = CheckWeatherInSPIFF();
    if (b_weather) {
        str_weather = "*Wetter*";
    }
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeSans12pt7b);
    display.firstPage();
    do {
        PL(display  , CONFIRM_LINE, 0, str_weather, true, false);
    } while (display.nextPage());


    /* Play music song   ***************************************************************************/
//    audio.setPinout(I2S_NUM_1_BCLK, I2S_NUM_1_LRC, I2S_NUM_1_DOUT);
    audio.setVolume(0); // 0...21
    audio.forceMono(true);
    audio.connecttoFS(SPIFFS, "/carmen.mp3");
//        audio.connecttohost("https://storage.googleapis.com/gw_wakeup_sounds/beat.mp3");
//         audio.connecttohost("http://mp3.ffh.de/radioffh/hqlivestream.aac");
//    audio.connecttohost("http://www.wdr.de/wdrlive/media/einslive.m3u");

    TaskHandle_t xHandle = NULL;
    xTaskCreate(
            fade_in,              /* Task function. */
            "fade_in",            /* String with name of task. */
            10000,                     /* Stack size in words. */
            NULL,       /* Parameter passed as input of the task */
            1,                         /* Priority of the task. */
            &xHandle);                     /* Task handle. */
    configASSERT(xHandle);

    DPL("Play the song!!!");
    b_audio_finished = false;

    // Check if Weather Forecast exists


    while ((!b_audio_end_of_mp3) && (!b_audio_finished)) {

        audio.loop();

        // Alarm music is playing now in the audio loop, when pir is waved - sounds fades out , weather is shown
        // until next pir wave
        if (b_pir_wave) {
            if (xHandle != NULL) { vTaskDelete(xHandle); }

            b_pir_wave = false;
            xTaskCreate(
                    fade_out,              /* Task function. */
                    "fade_out",            /* String with name of task. */
                    10000,                     /* Stack size in words. */
                    NULL,       /* Parameter passed as input of the task */
                    1,                         /* Priority of the task. */
                    NULL);                     /* Task handle. */

        }

        if (Serial.available()) { // put streamURL in serial monitor
            audio.stopSong();
            String r = Serial.readString();
            r.trim();
            if (r.length() > 5) audio.connecttohost(r.c_str());
            log_i("free heap=%i", ESP.getFreeHeap());
        }
    }

    if (b_weather) {
        DP("Showing Weather - give some time to look and wait for PIR Wave to finish");
        PaintWeather(d);

        while (digitalRead(PIR_INT) == true) {
            delay(100);
        }
        DPL("Wait for the next PIR Wave to return to normal display");

        // Wait for PIR wave or max 5 seconds
        b_pir_wave = false;
        int wait_count=250;
        while ((!b_pir_wave) && (wait_count>0)) {
            delay(100);
            DP(".");
            wait_count--;
        }
        DP("\n");
        DeleteWeatherFromSPIFF();
    }

    DPL("DONE");

}


