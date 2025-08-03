//
// Created by development on 21.01.24.
//

#ifndef GOODWATCH_ESPIF_WEATHER_DISPLAY_SUPPORT_H
#define GOODWATCH_ESPIF_WEATHER_DISPLAY_SUPPORT_H

class BitmapDisplay {
private:
    GxEPD2_GFX &display;
public:
    BitmapDisplay(GxEPD2_GFX &_display) : display(_display) {};

    void drawBitmaps();

private:
    void drawBitmaps400x300();
};


uint16_t ReadSensorDistance();
void dim_light_up_down_esp32(boolean direction_up);
int GetMaxLightLevel();
void DistanceSensorSetup();

#endif //GOODWATCH_ESPIF_WEATHER_DISPLAY_SUPPORT_H
