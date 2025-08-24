//
// Created by development on 05.11.21.
//


#include "wifi_support.h"
#include "esp_sntp.h"


#include <WiFi.h>
#include <esp_wifi.h>


// Your configurable schedule (ms)
static const uint32_t DELAYS[] = {10, 10, 10, 10, 15,15,15,5, 9, 9, 9, 10,10 , 10,15,15,15,15, 30, 30,5,5,10,10,30,30,5,5,5,5,15,15,15,};
static const size_t DELAY_INDEX_LEN = sizeof(DELAYS) / sizeof(DELAYS[0]);
static const uint8_t RADIO_RESET_EVERY = 10; // reconnects before radio reset

void SetupWifi()
{
    DP("Starting Wifi v3");
    unsigned long start_time = millis();
    WiFi.disconnect(false, true); // clean disconnect, keep radio on
    WiFi.mode(WIFI_STA); // set to station mode
    WiFi.setSleep(false); // disable WiFi sleep (optional)
    WiFi.begin(SSID, PASSWORD); // start connection

    unsigned long startAttemptTime = millis();
    const unsigned long timeout = 40000; // total wait time in ms
    unsigned long checkInterval ;

    int delay_count = 0;
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout)
    {
        checkInterval=DELAYS[delay_count]*100;
        DPF("Waiting for %u\n", checkInterval);
        delay(checkInterval);
        delay_count++;
        // every RADIO_RESET_EVERY delay count reset the wifi radio
        if (((delay_count % RADIO_RESET_EVERY) == 0) or (delay_count==5)) // also reset after 3 tries
        {
            DPL("Reset Wifi Radio");
            WiFi.disconnect(true, true); // clean disconnect, keep radio on
            WiFi.mode(WIFI_STA); // set to station mode
            WiFi.setSleep(false); // disable WiFi sleep (optional)
            WiFi.begin(SSID, PASSWORD); // start connection
        }
        if (delay_count>=DELAY_INDEX_LEN)break;

    }

    DPF("Attempted for %u ms\n", millis() - start_time);

    if (WiFi.status() != WL_CONNECTED)
    {
        DPL("\nConnection Failed! Rebooting...");
        WiFi.disconnect(true, true);
        delay(1000);
        ESP.restart(); // fail fast and restart
    }

    // Connected successfully
    DPL("\nWifi Connected!");
    DP("IP: ");
    DPL(WiFi.localIP());
    DP("DNS: ");
    DPL(WiFi.dnsIP());
    DPL("WIFI DONE");
    delay(500);

}


void SetupWifi_old()
{
    DP("Starting Wifi");

    WiFi.disconnect(false, true); // clean disconnect, keep radio on
    WiFi.mode(WIFI_STA); // set to station mode
    WiFi.setSleep(false); // disable WiFi sleep (optional)
    WiFi.begin(SSID, PASSWORD); // start connection

    unsigned long startAttemptTime = millis();
    const unsigned long timeout = 30000; // total wait time in ms
    const unsigned long checkInterval = 500; // check every 100 ms

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout)
    {
        DP(".");
        delay(checkInterval);
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        DPL("\nConnection Failed! Rebooting...");
        delay(1000);
        ESP.restart(); // fail fast and restart
    }

    // Connected successfully
    DPL("\nWifi Connected!");
    DP("IP: ");
    DPL(WiFi.localIP());
    DP("DNS: ");
    DPL(WiFi.dnsIP());
    DPL("WIFI DONE");
    delay(500);
}


/*
    char ntpServerName[] = "mp3.ffh.de";
    IPAddress ntpServerIP;
    WiFi.hostByName(ntpServerName, ntpServerIP);
    DP("WDR IP:");
    DPL(ntpServerIP);

    uint32_t t = millis();
    WiFiClient        client;
    if(client.connect(ntpServerName, 80, 1000)) {
        uint32_t dt = millis() - t;
        DPF("Connected in: %i\n", dt);
    }*/


void SetupSNTP()
{
    DP("SNT Server Setup:");

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, (char*)"pool.ntp.org");
    sntp_init();

    sntp_sync_status_t st;
    int count = 0;
    do
    {
        st = sntp_get_sync_status();
        count++;
        DP(".");
        delay(500);
    }
    while (st != SNTP_SYNC_STATUS_COMPLETED && count < 20);
    DPL();
    if (st != SNTP_SYNC_STATUS_COMPLETED)
    {
        DP("ERROR TIME SYNC:");
        DPL(st);
        delay(5000);
        ESP.restart();
    }

    setenv("TZ", "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00", 1);
    tzset();

    delay(500); //needed until time is available
    DPL("SNTP DONE");
}

void sendData(uint8_t* bytes, size_t count)
{
    // send them off to the server
    //digitalWrite(LED_BUILTIN, HIGH);

    HTTPClient httpClientI2S;

    if (httpClientI2S.begin(I2S_SERVER_URL))
    {
        httpClientI2S.addHeader("content-type", "application/octet-stream");
        httpClientI2S.POST(bytes, count);
        httpClientI2S.end();
    }
    else
        DPL("Http Client not connected!");

    // digitalWrite(LED_BUILTIN, LOW);
}
