#include <Arduino.h>

#include <Adafruit_NeoPixel.h>
#include <ESP32_NOW_Serial.h>
#include <WiFi.h>
#include <MacAddress.h>
#include <esp_wifi.h>

//ESP NOW SET UP AS A Station
#define ESPNOW_WIFI_MODE_STATION 1
#define ESPNOW_WIFI_CHANNEL 1

#if ESPNOW_WIFI_MODE_STATION
  #define ESPNOW_WIFI_MODE WIFI_STA
  #define ESPNOW_WIFI_IF   WIFI_IF_STA
#else
  #define ESPNOW_WIFI_MODE WIFI_AP
  #define ESPNOW_WIFI_IF   WIFI_IF_AP
#endif

const MacAddress peer_mac({0x10, 0x20, 0xba, 0xef, 0x05, 0x4c}); //mac address of other ESP
ESP_NOW_Serial_Class NowSerial(peer_mac, ESPNOW_WIFI_CHANNEL, ESPNOW_WIFI_IF);

//boolean to tell if the beam is broken
bool broken = 0;

//interrupt when the beam is broken
void IRAM_ATTR beam_break() {
  broken = 1;
}

void setup() {
  // put your setup code here, to run once:

  //set up ESPNOW to the other ESP
  WiFi.disconnect(true);
  WiFi.mode(ESPNOW_WIFI_MODE);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  while (!(WiFi.STA.started() || WiFi.AP.started())) delay(100);

  //IR receiver for beam break
  attachInterrupt(3, beam_break, RISING);
  //Debug LED to line up IR led and Receiver pair
  pinMode(2, OUTPUT);

  NowSerial.begin(115200);
}

void loop() {
  // put your main code here, to run repeatedly:

  //if broken and connected to other ESP, let it know that beam was broken
  if (broken) {
    if (NowSerial.availableForWrite()) {
      if (NowSerial.write(1) <= 0) {
        Serial.println("Failed");
      }
    } else {
      Serial.println("Not available for Write");
    }
    broken = 0;
    digitalWrite(2, HIGH);
  } else {
    digitalWrite(2, LOW);
  }
}
