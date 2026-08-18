# ESP32 Bowling Speed Detector

## Overview
This project is an automated bowling speed detector built using ESP32 microcontrollers (TinyS3), IR LEDs, and receivers. By setting up two separate IR beams at a known distance, the system detects when an object breaks the beams and calculates its speed using the time difference. The system supports manual distance entry via a capacitive keypad or automatic calculation using an ultrasonic sensor.

## Table of Contents
- [Features](#features)
- [Hardware Setup](#hardware-setup)
- [Software Architecture](#software-architecture)
- [Usage & State Machine](#usage--state-machine)
- [Development Challenges](#development-challenges)
- [Future Improvements](#future-improvements)

## Features
* **Real-Time Processing:** Utilizes FreeRTOS to assign tasks to specific CPU cores. Core 0 handles the main state machine and debug LED, while Core 1 is dedicated to ESP-NOW wireless communication to minimize latency.
* **Wireless Sync:** Implements the ESP-NOW protocol on WiFi channel 1 to wirelessly transmit the exact microsecond timestamp of the second beam break to the main board.
* **Capacitive Touch:** Uses an Adafruit MPR121 I2C capacitive keypad for navigating menus and inputting distances.
* **Auto-Distance Calculation:** Includes an ultrasonic sensor to automatically calculate the distance between the two IR beams.
* **Non-Volatile Leaderboard:** Saves the top 5 fastest recorded speeds to EEPROM (64 bytes) so data persists across power cycles.
* **Thread Safety:** Implements FreeRTOS mutex semaphores (`ready2_mutex`, `break2_mutex`) to protect timing variables shared between the ESP-NOW task and the main state machine.

## Hardware Setup
The physical setup consists of 4 breadboards arranged to create two parallel IR beams across a track:

### Primary Breadboard (Main System)
* **Microcontroller:** ESP32 TinyS3
* **Display:** 16x2 LiquidCrystal Display (Pins 3, 6, 7, 34, 35, 36)
* **Ultrasonic Sensor:** TRIG on pin 43, ECHO on pin 44
* **Keypad:** MPR121 Capacitive Touch via I2C (Pins 8, 9 at 400kHz)
* **Beam 1 Receiver:** IR Receiver on pin 5, Debug LED on pin 4

### Secondary Boards
* **Transmitters (x2):** Two breadboards positioned across the track, each housing an IR LED to emit the invisible beams.
* **Secondary Receiver (x1):** Placed down the track, housing the second IR receiver and an additional ESP32. This board captures the second beam break and sends the timestamp to the main board's MAC address (`10:20:BA:EF:05:4C`) via ESP-NOW.

## Software Architecture

### FreeRTOS Task Allocation
* **`state_machine` (Core 0):** Handles LCD rendering, keypad inputs, EEPROM saving, and speed calculation.
* **`ESPNOW_task` (Core 1):** Actively listens for the ping from the secondary receiver and records the exact `micros()` timestamp.
* **`debug_led_task` (Core 0):** Continuously syncs the debug LED state with the IR receiver pin to aid in physical alignment.

## Usage & State Machine
The system operates on a state machine controlled via the MPR121 keypad.
* **State 0 (Main Menu):** Select operating mode.
* **State 2 (Leaderboard):** Scroll through the top 5 saved speeds in EEPROM.
* **State 3 (Manual Entry):** Use the keypad to enter the distance in cm (`XX.XX`).
* **State 4 (Armed):** System waits for the first beam to be broken via hardware interrupt.
* **State 7 (Waiting):** Beam 1 broken. System waits for the ESP-NOW ping indicating Beam 2 was broken.
* **State 5 (Result):** Calculates and displays the final speed in mph, updates EEPROM if it's a high score.

## Development Challenges
* **Screen Failures:** We initially designed the system around a uLCD screen, but after multiple screens fried inexplicably, we pivoted to a standard 16x2 LiquidCrystal display, which proved much more reliable.
* **Sensor Alignment:** Because IR light is invisible to the human eye, aiming the IR LEDs perfectly at the receivers was extremely difficult. We programmed a `debug_led_task` to light up whenever the receiver lost contact with the beam, providing real-time visual feedback for alignment.

## Future Improvements
This project mirrors how speed detection works in professional bowling and industrial conveyor belts. If we had more time and resources, we would improve the project by:
* Upgrading to a much larger display mounted on a physical stand at eye level.
* Adding mechanical locks to secure the breadboards in place, ensuring the IR beams cannot be easily bumped out of alignment.
