## A simple standalone motion activated controller for WS2812b LED lights

This repository contains the code for an ESP8266 controller (D1 Mini or NodeMCU) that supports up to two independent motion controllers to turn on a strip of LED lights.  It does not require any additional automation system (e.g. Home Assistant, NodeRED, IFTTT, etc.) and works standalone.  The only requirement is wifi (and that is only needed for initial setup or to make configuration changes) and software to load the initial .bin file to the controller (e.g. NodeMCU PyFlasher, ESPHome-Flasher).  After initial load, any additional updates can be performed over-the-air (OTA).

For controller build instructions, please see either the (YouTube video) or the related (Blog article)

For complete installation and configuration info, please see the (WIKI)
