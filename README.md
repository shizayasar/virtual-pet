# Virtual Pet

A pocket virtual pet that reacts to light, touch, and motion. Built in C++ on an ESP32 with a custom 3D-printed shell.

The pet is Danny, a fluffy tabby cat drawn in pixel art.

Work in progress. Danny has his own face for every mood, reacts to touch, and falls asleep when the room gets dark. Next up is adding physical buttons and shake to play.

## Progress

### Shake to play

Danny now reacts to motion using an MPU-6050 accelerometer. A gentle shake plays with him and makes him happier, but shake him too hard and he gets dizzy for a few seconds.

https://github.com/user-attachments/assets/a0f8d460-bec1-4a83-8c1a-839e7890bdde

### Sleep mode using the light sensor

Danny now falls asleep on his own when the room gets dark and wakes up when the lights come back on, using the light sensor built into the board. The room has to stay dark (or lit) for 3 seconds before he reacts, so a passing shadow won't wake him or put him to sleep. The Sleep and Wake buttons still work too: if you wake him up in the dark, he stays awake until the lights go on and off again.

https://github.com/user-attachments/assets/3fa5249c-69ae-496c-986d-d7fa691413f8

### All of Danny's moods

Danny now has his own face for every mood: happy, okay, hungry, tired, sad, and asleep, and he blinks every few seconds.

https://github.com/user-attachments/assets/103a85be-bb7f-4663-baca-28e2d0b830b6

### Danny on screen

Replaced the placeholder circle with Danny's pixel-art sprite. Only his neutral face is finished so far, so the other moods still use it for now.

![Danny on screen](images/danny-on-screen.jpg)

### Stats, moods, and touch buttons

Danny has hunger, fun, and energy stats that drop over time. You can feed him, play with him, or put him to sleep using the buttons on the touchscreen, and his mood changes based on his stats.

https://github.com/user-attachments/assets/03c6d61e-519e-468a-8375-0ab2ed8aa140

### First working display

![First working display](images/display-test.jpg)

## Hardware

- ESP32-2432S028R (the "Cheap Yellow Display")
- MPU-6050 accelerometer
- PCF8574 I/O expander
- 12x12mm tactile buttons
- 8 ohm speaker
- 3.7V LiPo battery, TP4056 charger, MT3608 boost converter

## Setup

Built with the Arduino IDE using the ESP32 board package, the TFT_eSPI library for the display, and the XPT2046_Touchscreen library for touch.

## To do

- [x] Get the display working
- [x] Touch input
- [x] Pet stats and moods
- [x] Draw sprites for every mood
- [x] Sleep mode using the light sensor
- [x] Shake to play
- [ ] Buttons
- [ ] Sound
- [ ] Battery power
- [ ] Case
