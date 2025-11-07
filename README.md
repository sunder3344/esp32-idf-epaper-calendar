# esp32-idf-epaper-calendar

a calendar widget which based on esp32(esp-idf) and waveshare 2.9' e-paper.

## Hardware

- Espressif ESP32-S3 chip
- Waveshare 2.9 inch ink screen(epaper)

## Software
- ESP-IDF
- waveshare epaper offical driver(they only have Arduino version, I've embed it into ESP-IDF([see more](https://github.com/sunder3344/esp32-idf-epaper-embed)))

## video

[Click to watch ▶️](https://youtube.com/shorts/KrNlSNJvBJo?si=0qHzqggTdYRxzSBc)


## How to run it

- open file `sdkconfig` and config AP

<p align="center">
<img src="./pic/ap_config.png" width="900" height="400" alt="Alt Text"/>
</p>

this is the default password and name of AP, when esp32 run successful, you can connect to the AP to config WiFi.

----- 

- open file `sdkconfig` and config e-paper pin

<p align="center">
<img src="./pic/param_config.png" width="900" height="400" alt="Alt Text"/>
</p>

please config the appropriate pin number of your esp32 and e-paper

----- 

- compile and upload to esp32 and run it

<p align="center">
<img src="./pic/connect_ap.png" width="900" height="400" alt="Alt Text"/>
</p>

when esp32 runing, you must connect to the AP WiFi(in my demo this is `derek_ap`)

----- 

- config WiFi

<p align="center">
<img src="./pic/wifi_interface.png" width="900" height="400" alt="Alt Text"/>
</p>

when esp32 running, you can see this prompt in e-paper, it means you should config the wifi throught out the url given(in my demo, this is http://192.168.4.1:8888)

<p align="center">
<img src="./pic/wifi_config_input.png" width="900" height="400" alt="Alt Text"/>
</p>

input `SSID` and `Password`(available wifi in your home or office) and submit


<p align="center">
<img src="./pic/wifi_config.png" width="900" height="400" alt="Alt Text"/>
</p>

if you see this, it means WiFi has config successful, just wait a moment, esp32 will restart.

<p align="center">
<img src="./pic/wifi_success.png" width="800" height="400" alt="Alt Text"/>
</p>

----- 

- esp32 work

when esp32 restart automatically, it will work normally

<p align="center">
<img src="./pic/calendar.png" width="800" height="400" alt="Alt Text"/>
</p>

----- 

- message prompt

this demo also support message prompt(refresh partially), first input 

<p align="center">
<img src="./pic/prompt_input.png" width="800" height="400" alt="Alt Text"/>
</p>

##### *time* : time to prompt

##### *content* : prompt content(only support English characters)

I highly recommend you build your own server API
