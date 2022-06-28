## A simple standalone motion activated controller for WS2812b LED lights

#### Version 0.45 adds some simple lighting effects and the ability to have different colors or effects based on which motion detector fired.

![stair_lights_Small](https://user-images.githubusercontent.com/55962781/176273263-bb53696a-65c7-401a-be7f-ef6ecfb2e0bc.jpg)

This repository contains the code for an ESP8266 controller (D1 Mini or NodeMCU) that supports up to two independent motion controllers to turn on a strip of LED lights.  It can be used for a stair lighting system or anywhere else where you want to control LED strip lighting by motion.  It does not require any additional automation systems (e.g. Home Assistant, NodeRED, IFTTT, etc.) and works standalone.  The only requirement is wifi (and that is only needed for initial setup or to make configuration changes) and software to load the initial .bin file to the controller (e.g. NodeMCU PyFlasher, ESPHome-Flasher).  After initial load, any additional updates can be performed over-the-air (OTA) with using a simple web browser.

For controller build instructions, please see either the [YouTube video](https://youtu.be/b4s_VEtVWY4) or the related [Blog article](https://resinchemtech.blogspot.com/2021/12/standalone-led-controller.html)
For complete installation and configuration info, please see the [wiki](https://github.com/Resinchem/standalone-led-controller/wiki)

For a more feature-rich version using WLED and Home Assistant, please see [this repository](https://github.com/Resinchem/LED-Stair-Lights) instead.

>*If you found this project helpful, would like to say thanks or help support future development:*<br>
>[![buy_me_a_coffee_sm](https://user-images.githubusercontent.com/55962781/159586675-7476e996-a990-4918-8825-aa6812f3ea28.jpg)](https://www.buymeacoffee.com/resinchemtech)
