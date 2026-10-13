# IoT-Based Water Level Indicator: Project Report

*Suraj Shah, IIT Delhi, 2024*

## Abstract

Overhead and underground water tanks are usually checked by eye, which leads to overflow, dry-running pumps and wasted water. This project builds a low-cost IoT water level indicator around an ESP32 microcontroller and a waterproof ultrasonic sensor. The system measures the water level continuously, shows it on a local LCD, uploads it to the Blynk cloud for remote viewing and data logging, sends mobile alerts when the tank is nearly full or nearly empty, and can switch the pump automatically through a relay.

## Problem Statement

Manual tank monitoring is unreliable. A forgotten pump overflows the tank and wastes water and electricity, while an unnoticed empty tank causes the pump to run dry and burn out. The aim is a cheap, reliable system that tells the user the water level from anywhere and protects the pump automatically.

## Objectives

- Measure tank water level in real time without touching the water.
- Display the level locally on an LCD and remotely on a mobile dashboard.
- Send mobile alerts for tank-full and tank-low conditions to prevent overflow and wastage.
- Log level data to the cloud so usage trends can be reviewed.
- Control the pump automatically with hysteresis, with a manual override from the phone.
- Keep the total cost low enough for household use.

## Components Used

| Component | Specification | Purpose |
|---|---|---|
| ESP32 DevKit V1 | Dual-core, built-in Wi-Fi | Main controller and cloud link |
| JSN-SR04T | Waterproof ultrasonic sensor, 5 V, about 20 cm to 600 cm range | Non-contact distance to water surface |
| 16x2 LCD with I2C backpack | PCF8574, address 0x27 | Local level display |
| Relay module | 5 V, single channel | Switches the pump |
| DC water pump | 12 V submersible | Fills the tank |
| Active buzzer | 5 V | Local audible alarm |
| LEDs (green, red) | 5 mm | Normal and alert indication |
| Resistors | 1 k, 2 k (divider), 2 x 220 ohm | Echo level shifting and LED current limiting |
| Diode | 1N4007 | Flyback protection across the pump |
| Power supply | 12 V adapter and 5 V buck converter or USB | Powers pump and electronics |

## System Architecture

The ultrasonic sensor sits at the top of the tank facing down. The ESP32 triggers the sensor, times the returning echo, converts the time into distance, and turns the distance into a percentage level. The result drives the LCD, LEDs and buzzer locally, and is pushed over Wi-Fi to the Blynk cloud. The Blynk mobile app shows the live level, receives alert notifications, and lets the user change pump mode. Data flows in one line: sensor, ESP32, Wi-Fi, Blynk cloud, mobile app, with the pump relay controlled back from the ESP32.

## Circuit Connections

![Circuit diagram](../hardware/circuit_diagram.png)

| Device pin | ESP32 pin | Note |
|---|---|---|
| JSN-SR04T VCC | VIN (5 V) | Sensor needs 5 V |
| JSN-SR04T GND | GND | |
| JSN-SR04T TRIG | GPIO 5 | 3.3 V output is accepted by the sensor |
| JSN-SR04T ECHO | GPIO 18 | Through 1 k / 2 k divider, since echo is 5 V and ESP32 pins tolerate only 3.3 V |
| LCD SDA | GPIO 21 | LCD powered from 5 V |
| LCD SCL | GPIO 22 | |
| Relay IN | GPIO 26 | Relay VCC to 5 V, GND common |
| Buzzer (+) | GPIO 27 | Other leg to GND |
| Green LED | GPIO 25 | Through 220 ohm |
| Red LED | GPIO 33 | Through 220 ohm |

The pump is wired through the relay's normally-open contacts from the 12 V supply, with a 1N4007 diode across the pump terminals.

## Working Principle

The ESP32 sends a 10 microsecond pulse to the TRIG pin. The sensor emits an ultrasonic burst and raises ECHO for as long as the sound takes to travel to the water and back. The distance is

```
distance (cm) = echo time (us) x 0.0343 / 2
```

Five readings are taken and the median is used, which removes spikes caused by ripples and false echoes. With the sensor fixed at the top, the water level percentage is

```
level (%) = (empty distance - measured distance) / (empty distance - full distance) x 100
```

where the empty distance is the sensor-to-bottom distance and the full distance is the sensor-to-water distance when the tank is full. The full distance must be at least about 20 cm because the JSN-SR04T cannot measure closer than its blind zone, so the sensor is mounted at least that far above the highest water line.

## Control Logic and Alerts

The pump uses hysteresis so the relay does not chatter. In automatic mode the pump switches on when the level falls below 20 percent and off when it rises above 90 percent. A tank-low alert is sent at 20 percent and below, and a tank-full alert at 95 percent and above. Each alert is sent once per event and re-armed only after the level moves back into the normal band, so the phone is not flooded with notifications. The buzzer and red LED give the same warnings at the tank, and the green LED shows normal operation. A manual switch in the app overrides automatic mode.

## Software

The firmware is written in Arduino C++ for the ESP32 using the Blynk and LiquidCrystal_I2C libraries (see `firmware/water_level_indicator.ino`). Sensor reading, display refresh and cloud upload run on timers so the loop never blocks, and the device keeps measuring, displaying and controlling the pump even when Wi-Fi drops, then reconnects to the cloud automatically. Blynk virtual pins carry the level percentage, distance, pump state, automatic-mode switch and manual pump switch.

## Setup and Calibration

1. Mount the sensor at the top of the tank, pointing straight down and clear of pipes and the tank wall.
2. With the tank empty, read the distance on the LCD or serial monitor and enter it as the empty distance in the code.
3. Fill the tank to the desired maximum, read the distance, and enter it as the full distance.
4. Create a Blynk template with the virtual pins listed in the code, add the two events (tank_full and tank_low), and paste the template ID, device name and auth token.
5. Enter the Wi-Fi name and password, upload the code, and compare the displayed level against a ruler measurement at a few water heights to confirm accuracy.

## Cost Effectiveness and Reliability

The whole system uses commonly available modules and a single microcontroller, with a free cloud tier for the dashboard, so it costs far less than commercial tank controllers. Reliability comes from the waterproof non-contact sensor, which avoids corrosion, median filtering for stable readings, hysteresis for the pump, and offline operation when the network is down.

## Limitations and Future Scope

Ultrasonic readings can be disturbed by heavy foam, condensation on the sensor face and strong turbulence while the tank is filling. Future work includes adding a dry-run current sensor on the pump, a float switch as a hardware backup for overflow, temperature compensation of the speed of sound, solar power, and multi-tank support on the same dashboard.

## Conclusion

The project delivers a complete smart water management solution: contactless level sensing, local and remote display, mobile alerts, cloud data logging and automatic pump control, built with low-cost parts and an easily reproduced design.
