# IoT-Based Water Level Indicator

A low-cost smart water management system built on an **ESP32**. A waterproof ultrasonic sensor measures the water level in a tank without touching the water, the level is shown on a local LCD and on a live mobile dashboard, mobile alerts warn about low and full tanks, data is logged to the cloud, and a relay switches the pump automatically.

![Animated preview](media/preview.gif)

Full animated walkthrough: [`media/water_level_indicator_animation.mp4`](media/water_level_indicator_animation.mp4)

## Features

- Contactless level measurement with a JSN-SR04T waterproof ultrasonic sensor
- Median filtering of five readings per sample to remove ripple spikes and false echoes
- Local display on a 16x2 I2C LCD, with green (normal) and red (alert) LEDs and a buzzer
- Live level, distance and pump state on a Blynk mobile dashboard, with cloud data logging
- Mobile alerts for tank low (20% or below) and tank full (95% or above), sent once per event
- Automatic pump control with hysteresis (ON below 20%, OFF above 90%) and a manual override from the app
- Keeps measuring and controlling the pump when Wi-Fi is down, and reconnects automatically
- Fail-safe: the pump is switched off if the sensor stops returning echoes

## Hardware

| Component | Purpose |
|---|---|
| ESP32 DevKit V1 | Controller and Wi-Fi link |
| JSN-SR04T waterproof ultrasonic sensor (5 V) | Distance from sensor to water surface |
| 16x2 LCD with I2C backpack (0x27) | Local level display |
| 5 V single-channel relay module | Switches the pump |
| 12 V DC water pump | Fills the tank |
| Active buzzer, green LED, red LED | Local alarm and status |
| 1 k and 2 k resistors, 2 x 220 ohm resistors | Echo voltage divider, LED current limiting |
| 1N4007 diode | Flyback protection across the pump |
| 12 V adapter and 5 V supply | Power |

### Pin connections

| Device pin | ESP32 pin | Note |
|---|---|---|
| JSN-SR04T VCC / GND | VIN (5 V) / GND | |
| JSN-SR04T TRIG | GPIO 5 | |
| JSN-SR04T ECHO | GPIO 18 | Through a 1 k / 2 k divider (echo is 5 V, ESP32 is 3.3 V) |
| LCD SDA / SCL | GPIO 21 / GPIO 22 | LCD on 5 V |
| Relay IN | GPIO 26 | Active-LOW by default, see `RELAY_ACTIVE_LOW` in the code |
| Buzzer (+) | GPIO 27 | |
| Green LED | GPIO 25 | Through 220 ohm |
| Red LED | GPIO 33 | Through 220 ohm |

The pump runs from the 12 V supply through the relay's COM and NO contacts, with a 1N4007 diode across the pump. See the full schematic in [`hardware/circuit_diagram.png`](hardware/circuit_diagram.png) (editable [SVG](hardware/circuit_diagram.svg) also included).

![Circuit diagram](hardware/circuit_diagram.png)

## How it works

The ESP32 sends a 10 microsecond pulse to TRIG, times the echo, and computes

```
distance (cm) = echo_time (us) x 0.0343 / 2
level (%)     = (EMPTY_DIST - distance) / (EMPTY_DIST - FULL_DIST) x 100
```

`EMPTY_DIST` is the sensor-to-bottom distance and `FULL_DIST` is the sensor-to-water distance when the tank is full. The JSN-SR04T has a blind zone of about 20 cm, so mount the sensor at least that far above the highest water line.

Sensor data flows from the sensor to the ESP32, over Wi-Fi to the Blynk cloud, and on to the mobile app. The pump relay is controlled back from the ESP32.

## Getting started

1. Install the Arduino IDE with ESP32 board support, and these libraries from the Library Manager: **Blynk** (Volodymyr Shymanskyy) and **LiquidCrystal I2C** (Frank de Brabander).
2. Build the circuit as shown in the diagram.
3. Create a Blynk template with these datastreams and two events:

   | Virtual pin | Meaning | Type |
   |---|---|---|
   | V0 | Water level % | Double, 0 to 100 |
   | V1 | Distance in cm | Double, 0 to 600 |
   | V2 | Pump state | Integer, 0 or 1 |
   | V3 | Auto mode switch (from app) | Integer, 0 or 1 |
   | V4 | Manual pump switch (from app) | Integer, 0 or 1 |

   Events: `tank_full` and `tank_low`.
4. Open [`firmware/water_level_indicator.ino`](firmware/water_level_indicator.ino) and fill in `BLYNK_TEMPLATE_ID`, `BLYNK_AUTH_TOKEN`, `WIFI_SSID` and `WIFI_PASS`. Do not commit your real credentials to a public repository.
5. Calibrate: with the tank empty, read the distance and set `EMPTY_DIST`. With the tank full, set `FULL_DIST`. Upload the code and compare the displayed level with a ruler at a few water heights.

## Repository structure

```
water-level-indicator/
  firmware/water_level_indicator.ino   ESP32 firmware (Arduino C++)
  hardware/circuit_diagram.png|svg     Wiring diagram with component list
  docs/Project_Report.md               Full project report
  media/water_level_indicator_animation.mp4   Animated explainer video
  media/preview.gif                    README preview
```

## Limitations and future scope

Heavy foam, condensation on the sensor face and strong turbulence while filling can disturb ultrasonic readings. Planned improvements: a pump current sensor for dry-run protection, a float switch as a hardware overflow backup, temperature compensation of the speed of sound, solar power, and several tanks on one dashboard.

## Author

Suraj Shah. Project developed at IIT Delhi, 2024. Released under the MIT License.
