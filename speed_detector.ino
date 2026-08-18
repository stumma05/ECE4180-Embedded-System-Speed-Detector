#include <Wire.h>
#include <Adafruit_MPR121.h>
#include <LiquidCrystal.h>
#include <Arduino.h>
#include "ESP32_NOW_Serial.h"
#include "MacAddress.h"
#include "WiFi.h"
#include <stdio.h>
#include <EEPROM.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_wifi.h"


//pin defs
#define TRIG     43
#define ECHO     44
#define RECPIN    5
#define LED       4

#define LCD_RS    3
#define LCD_EN    6
#define LCD_D4    7
#define LCD_D5   34
#define LCD_D6   35
#define LCD_D7   36


///esp now
#define ESPNOW_WIFI_MODE_STATION 1
#define ESPNOW_WIFI_CHANNEL 1

#if ESPNOW_WIFI_MODE_STATION
  #define ESPNOW_WIFI_MODE WIFI_STA
  #define ESPNOW_WIFI_IF   WIFI_IF_STA
#else
  #define ESPNOW_WIFI_MODE WIFI_AP
  #define ESPNOW_WIFI_IF   WIFI_IF_AP
#endif

#ifndef _BV
  #define _BV(bit) (1 << (bit))
#endif

//non volatile mem
#define EEPROM_SIZE 64
#define KNOWNA      0
#define KNOWNV      0xA5
#define SCORES_ADDR 1

//16x2 lcd
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

double savedScores[5];

//rtos semaphores
SemaphoreHandle_t ready2_mutex;
SemaphoreHandle_t break2_mutex;

Adafruit_MPR121 cap = Adafruit_MPR121();

const MacAddress peer_mac({0x10, 0x20, 0xba, 0xef, 0x05, 0x4c});
ESP_NOW_Serial_Class NowSerial(peer_mac, ESPNOW_WIFI_CHANNEL, ESPNOW_WIFI_IF);

uint16_t lasttouched = 0;
uint16_t currtouched = 0;

//state machine
int number = 0;
// number = state holder
//  0 = main menu
//  2 = leaderboard
//  3 = distance entry
//  4 = armed, waiting for first beam break
//  5 = show result
//  7 = waiting for second beam break via ESP-NOW

//charater buffers for printing various things to lcd and serial monitor
char   buf;
char   disty[6];   // "XX.XX\0"
double holder;
bool   done[4];

//variables to signal to isr and esp now task to read beam break
volatile bool          ready1 = false;
volatile bool          ready2 = false;
//hold the time of beam break event
volatile unsigned long break1 = 0;
volatile unsigned long break2 = 0;

//calculated speed and char buffer for lcd
double speed;
char   speedBuff[8];

//bool to signal main menu reset
bool   resetFlag = false;
//bool to signal lcd refresh
bool   firstTime = false;
// leaderboard scroll index (0–4)
int leaderboard_idx = 0;

//interrupt for beam break 1
void IRAM_ATTR isr() {
  if (ready1) {
    break1 = micros();
    ready1 = false;
  }
}

//lcd refresh helper
void draw_screen(int state) {
  lcd.clear();

  if (state == 0) { //main menu
    lcd.setCursor(0, 0);
    lcd.print("Speed Detector");
    lcd.setCursor(0, 1);
    lcd.print("11M 10Sc 9Auto");
  } else if (state == 2) { //leaderboard of previously saved speeds
    lcd.setCursor(0, 0);
    lcd.print("Leaderboard:");
    lcd.setCursor(0, 1);
    char lbBuf[17];
    snprintf(lbBuf, sizeof(lbBuf), "%d. %.2f mph", leaderboard_idx + 1, savedScores[leaderboard_idx]);
    lcd.print(lbBuf);
  } else if (state == 3) { //maunal distance entering UI
    lcd.setCursor(0, 0);
    lcd.print("Dist (cm):");
    lcd.setCursor(0, 1);
    lcd.print(disty);
  } else if (state == 4) { //ready for beam break
    lcd.setCursor(0, 0);
    lcd.print("Armed.");
    lcd.setCursor(0, 1);
    lcd.print("Waiting beam 1..");
  } else if (state == 5) { //show speed
    lcd.setCursor(0, 0);
    lcd.print("Speed:");
    lcd.print(speedBuff);
    lcd.print(" mph");
    lcd.setCursor(0, 1);
    lcd.print("1=Again 0=Menu");
  } else if (state == 7) { //waiting for second beam break
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Beam 1 broken!");
    lcd.setCursor(0, 1);
    lcd.print("Waiting beam 2..");
  }
}

//calculate auto distance
float getDistance() {
  float dist = -1;

  while (dist <= 0) { // if calculated distance is negative, re-measure
    //send pulse over TRIG pin
    digitalWrite(TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG, LOW);

    //wait for pulse over ECHO pin
    float duration = pulseIn(ECHO, HIGH, 30000);
    dist = (duration * 0.0343f) / 2.0f;
    dist = dist - 9.906;
    Serial.print("Distance: ");
    Serial.println(dist);
  }
  return dist;
}

// state machine / main code
void state_machine(void *pvParameters) {
  while (1) {

    // Redraw screen on state change
    if (firstTime) {
      draw_screen(number);
      firstTime = false;
    }

    // Read capacitive touch
    currtouched = cap.touched();
    vTaskDelay(pdMS_TO_TICKS(1));

    for (int i = 0; i < 12; i++) {
      if ((currtouched & _BV(i)) && !(lasttouched & _BV(i))) {
        Serial.print(i); Serial.println(" touched");

        // always reset to main menu on 0
        if (i == 0) {
          number    = 0;
          ready1    = false;
          resetFlag = false;
          if (xSemaphoreTake(ready2_mutex, portMAX_DELAY)) {
            ready2 = false;
            xSemaphoreGive(ready2_mutex);
          }
          firstTime = true;
          lasttouched = currtouched;
          continue;
        }

        if (number == 0) { //main menu
          buf = '0' + i;

        } else if (number == 2) { //leaderboard
          if (i == 10) {
            leaderboard_idx = (leaderboard_idx + 1) % 5;
            draw_screen(2);
          } else if (i == 11) {
            leaderboard_idx = (leaderboard_idx + 4) % 5;  //+4 mod 5 circaular array
            draw_screen(2);
          }
        //manual enter distance
        } else if (number == 3) {
          if (!done[0] && i != 11 && i != 10) {
            disty[0] = '0' + i;
            holder += i * 10;
            done[0] = true;
            draw_screen(3);

          } else if (!done[1] && i != 11 && i != 10) {
            disty[1] = '0' + i;
            holder += i;
            done[1] = true;
            draw_screen(3);

          } else if (!done[2] && i != 11 && i != 10) {
            disty[3] = '0' + i;
            holder += i / 10.0;
            done[2] = true;
            draw_screen(3);

          } else if (!done[3] && i != 11 && i != 10) {
            disty[4] = '0' + i;
            holder += i / 100.0;
            done[3] = true;

            Serial.print("Distance: "); Serial.println(holder);

            //finished distance
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Distance set:");
            lcd.setCursor(0, 1);
            lcd.print(disty);
            vTaskDelay(pdMS_TO_TICKS(800));

            number    = 4;
            ready1    = true;
            firstTime = true;
          }

          Serial.printf("done: %d%d%d%d\n", done[0], done[1], done[2], done[3]);

        //this is waiting for beam break
        } else if (number == 5) {
          if (i == 1) {
            ready1    = true;
            number    = 4;
            firstTime = true;
          }
        }

        //touchpad entries
        //auto distance
        if (i == 9 && number != 3 && number != 2 && number != 5 && number != 4 && number != 7) {
          holder = (double) getDistance();
          ready1 = true;
          number = 4;
          firstTime = true;
        }
        //view leaderboard
        if (i == 10 && number != 3 && number != 1 && number != 5 && number != 4 && number != 7 && number != 2) {
          leaderboard_idx = 0;
          number = 2;
          firstTime = true;
        }
        //manual distance
        if (i == 11 && number != 2 && number != 1 && number != 5 && number != 4 && number != 7) {
          number = 3;
          holder = 0;
          done[0] = done[1] = done[2] = done[3] = false;
          disty[0] = 'X';
          disty[1] = 'X';
          disty[2] = '.';
          disty[3] = 'X';
          disty[4] = 'X';

          firstTime = true;
        }
      }
    }
    lasttouched = currtouched;

    //wait for first beam break
    if (number == 4) {
      if (resetFlag) { //cancel and return to main menu
        number = 0; ready1 = false; resetFlag = false; firstTime = true;
        vTaskDelay(pdMS_TO_TICKS(20));
        continue;
      }

      if (!ready1) {   //beam break1
        noInterrupts();
        Serial.print("Break1: "); Serial.println(break1);
        interrupts();

        if (xSemaphoreTake(ready2_mutex, portMAX_DELAY)) {
          ready2 = true;
          xSemaphoreGive(ready2_mutex);
        }
        number = 7;
        draw_screen(7);
      }

    //beam 1 broken waiting for beam 2
    } else if (number == 7) {
      if (resetFlag) { //cancel and return to main menu
        number = 0;
        if (xSemaphoreTake(ready2_mutex, portMAX_DELAY)) {
          ready2 = false;
          xSemaphoreGive(ready2_mutex);
        }
        resetFlag = false; firstTime = true;
        vTaskDelay(pdMS_TO_TICKS(20));
        continue;
      }

      bool ready2_temp;
      unsigned long break2_temp;
      unsigned long break1_temp;

      noInterrupts();
      break1_temp = break1;
      interrupts();

      if (xSemaphoreTake(ready2_mutex, portMAX_DELAY)) {
        ready2_temp = ready2;
        xSemaphoreGive(ready2_mutex);
      }
      if (xSemaphoreTake(break2_mutex, portMAX_DELAY)) {
        break2_temp = break2;
        xSemaphoreGive(break2_mutex);
      }

      if (!ready2_temp) { //esp now recieved a ping
        if (break2_temp > break1_temp) {
          speed = (holder / ((break2_temp - break1_temp) / 1000000.0)) / 44.704f; //convert cm/us to mph
          snprintf(speedBuff, sizeof(speedBuff), "%.2f", speed);
          Serial.print("Speed: ");
          Serial.print(speed);
          Serial.println(" mph");

          // Save to mem if score is in top 5
          for (int i = 0; i < 5; i++) {
            if (speed >= savedScores[i]) {
              Serial.println("New high score!");
              for (int j = 4; j > i; j--) savedScores[j] = savedScores[j - 1];
              savedScores[i] = speed;
              for (int p = 0; p < 5; p++) EEPROM.put(SCORES_ADDR + p * sizeof(double), savedScores[p]);
              EEPROM.commit();
              break;
            }
          }
        } else { //time measurement error (stored break1 time is after stored break2 time)
          Serial.println("Timing error: break2 before break1");
          snprintf(speedBuff, sizeof(speedBuff), "ERR");
        }

        break1 = 0;
        number = 5;
        firstTime = true;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ESP NOW for ping on beam 2 break
void ESPNOW_task(void *pvParameters) {
  while (1) {
    while (NowSerial.available()) {
      NowSerial.read();
      unsigned long tim = micros(); //get the time of the ping
      if (xSemaphoreTake(ready2_mutex, portMAX_DELAY)) {
        if (ready2) {
          Serial.print("Break2: "); Serial.println(tim);
          if (xSemaphoreTake(break2_mutex, portMAX_DELAY)) {
            break2 = tim;
            xSemaphoreGive(break2_mutex);
          }
          ready2 = false;
        }
        xSemaphoreGive(ready2_mutex);
        break;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

//  debug led to line up receiver and IR led
void debug_led_task(void *pvParameters) {
  while (1) {
    digitalWrite(LED, digitalRead(RECPIN));
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);

  delay(100);
  Serial.println("pls");

  //temporary flash
  // EEPROM
  EEPROM.begin(EEPROM_SIZE);

  //this to reset scores (flash this, then comment and flash)
  //EEPROM.write(KNOWNA, 0xA4);

  if (EEPROM.read(KNOWNA) != KNOWNV) {
    Serial.println("First boot — initialising EEPROM");
    EEPROM.write(KNOWNA, KNOWNV);
    EEPROM.put(SCORES_ADDR + 0 * sizeof(double), 5.0);
    EEPROM.put(SCORES_ADDR + 1 * sizeof(double), 4.0);
    EEPROM.put(SCORES_ADDR + 2 * sizeof(double), 3.0);
    EEPROM.put(SCORES_ADDR + 3 * sizeof(double), 2.0);
    EEPROM.put(SCORES_ADDR + 4 * sizeof(double), 1.0);
    EEPROM.commit();
  }
  for (int i = 0; i < 5; i++) EEPROM.get(SCORES_ADDR + i * sizeof(double), savedScores[i]);

  // SONAR for automatic distance mode
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  // Display
  Serial.println("Starting display...");
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initialising...");
  Serial.println("Display init OK");

  // ESP-NOW / WiFi
  WiFi.disconnect(true);
  WiFi.mode(ESPNOW_WIFI_MODE);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  while (!(WiFi.STA.started() || WiFi.AP.started())) delay(100);

  Serial.print("MAC: ");
  Serial.println(ESPNOW_WIFI_MODE == WIFI_AP ? WiFi.softAPmacAddress() : WiFi.macAddress());
  NowSerial.begin(115200);
  Serial.printf("ESP-NOW v%d, max len %d\n", ESP_NOW.getVersion(), ESP_NOW.getMaxDataLen());
  delay(500);

  // pinouts
  pinMode(RECPIN, INPUT);
  pinMode(LED, OUTPUT);
  attachInterrupt(RECPIN, isr, RISING);

  // I2C & capacitive keypad
  Wire.setPins(8, 9);
  Wire.begin();
  Wire.setClock(400000);
  cap.begin(0x5A);
  cap.setAutoconfig(true);

  //char buffer for manual distance entering UI
  disty[0] = 'X'; 
  disty[1] = 'X'; 
  disty[2] = '.';
  disty[3] = 'X'; 
  disty[4] = 'X'; 
  disty[5] = '\0';
  done[0] = done[1] = done[2] = done[3] = false;

  //mutexes to protect variables shared with esp now and state machine tasks
  ready2_mutex = xSemaphoreCreateMutex();
  break2_mutex = xSemaphoreCreateMutex();

  firstTime = true;

  /*
    Create RTOS tasks
    Run state machine and debug led on core 0
    Run esp now on core 1
    This allows for minimum latency from esp now receiving a ping to getting the time from micros()
  */
  xTaskCreatePinnedToCore(
    state_machine, 
    "State Machine",
    16384, 
    NULL,
    2,
    NULL,
    0
  );

  xTaskCreatePinnedToCore(
    ESPNOW_task,
    "ESP NOW",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  xTaskCreatePinnedToCore(
    debug_led_task,
    "Debug LED",
    1024,
    NULL,
    1,
    NULL,
    0
  );
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}
