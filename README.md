# SmartSwitch compatible with Apple HomeKit Protocol

<p align="center">
  <img src="images/switch.png" alt="drawing" width="400"/>
</p>

<br/>

# Project overview

The remotely operated SmartSwitch is designed to turn electrical devices or circuits on and off. The compact size of the device allows for the product to be installed in the housings of other devices. Thanks to its miniature size it fits most of the flush-mounted junction boxes and light switch sockets. The devices can be controlled either via the Apple HomeKit or with buttons connected directly to them.
Using Apple Home app enables automations, scenes and integration with Siri.

<br/>

# Electrical schematic
I have started with an electrical schematic of my device. The basic connections are as specified by the chip manufacturer in [ESP32-WROOM-32d-Datasheet] and the rest depends on my knowledge, experience, and prototyping. 
After creating a schematic I connected all elements on a breadboard. When the breadboard prototype worked successfully I attempted to make PCB.


## Hardware files (v4.0)

- [Eagle schematic (.sch)](hardware/v4.0/smart%20switch.sch)
- [Eagle PCB layout (.brd)](hardware/v4.0/smart%20switch.brd)

## Parts list

| Reference | Qty | Model / value | Function |
| --- | --- | --- | --- |
| U3 | 1 | ESP-WROOM-32 | Wi-Fi and HomeKit controller. |
| HLK1 | 1 | HLK-PM03, 3.3 V (README); HLK-PM01 library device in Eagle | AC-to-DC supply feeding the ESP32 3V3 rail. |
| U2 | 1 | SN74LVC2G17DBV (schematic) | Schmitt-trigger buffer for the physical-switch input to GPIO13. |
| K2 | 1 | OJE-SS-103HM,000 | Normally open load-switching relay. |
| Q1 | 1 | BC849 family, SOT-23 | Relay-coil driver controlled by GPIO27. |
| D1 (formerly R3) | 1 | Diode; model not specified | Coil flyback protection: cathode to VCC, anode to Q1 collector. |
| R1 | 1 | 10 kΩ, 1206 | Physical-switch input pull-down; forms an RC network with C5/C6. |
| R2 | 1 | 10 kΩ, 1206 | ESP32 EN pull-up; forms the reset RC network with C3. |
| R4 | 1 | 4.7 kΩ, 1206 | Q1 base-current limiting. |
| R5 | 1 | 10 kΩ, 1206 | Q1 base-to-emitter pull-down. |
| C1, C2 | 2 | Not specified, 1206 | Supply filtering between VCC and GND. |
| C3 | 1 | Not specified, 1206 | EN-to-GND capacitor in the reset RC network. |
| C4 | 1 | Not specified, polarized, E2-5 footprint | Supply reservoir capacitor between VCC and GND. |
| C5, C6 | 2 | Not specified; C5: C025-024X044, C6: 1206 | Parallel capacitors from switch input to VCC; input filtering with R1. |
| JP1 | 1 | 1×4 header | Programming: GND, RXD0, TXD0, VCC (3.3 V). |
| JP2 | 1 | 1×2 header | GPIO0-to-GND boot-mode jumper. |
| X1 | 1 | AK500/5 terminal block | AC supply, relay contacts, and physical-switch connections. |

The capacitors provide basic power-supply and input filtering, with C3 forming the ESP32 enable/reset RC network. For typical ESP32 supply-decoupling and EN/reset capacitor values, refer to Espressif’s basic ESP32 reference schematics.

<p align="center">
  <img src="images/electrical_schematic.png" alt="drawing" width="400"/>
</p>

<br/>

# PCB designing
The PCB design is based on my electrical schematic. The first step was creating a homemade PCB prototype. When the whole device was mounted, flashed, tested, and running finely I've ordered PCB from [ALLPCB] 

<br/>

## PCB design

<p align="center">
<img src="images/PCB_schematic.png" alt="drawing" width="400"/>
</p>
<br/>

## Ordered PCB

<p align="center">
<img src="images/PCB.JPG" alt="drawing" width="400"/>
</p>

<br/>

## Prototype & manufactured PCB

<p align="center">
<img src="images/prototype.png" alt="drawing" width="400"/>
</p>
<br/><br/>

# Assembly
I have all needed tools and skills to build and flash my device at home .


<p align="center">
<img src="images/workstation.JPG" alt="drawing" width="400"/>
</p>

<p align="center">
<img src="images/assembledSwitches.JPG" alt="drawing" width="400"/>
</p>
<br/>


# Writing program 
When I started this project HomeKit Accessory Development Kit (ADK) was available only for MFi Program, also ESP32 manufacture didn't provide any information on how to establish HomeKit connection. I decided to use and modify code from: 
[maximkulkin/esp-homekit]

The whole communication is based on HomeKit Accessory Server from Maxim, but how physically device is working is all my job.
Because Apple doesn't allow to use Apple's WAC (Wireless Accessory Configuration) feature for devices without MFi license we need to provide WiFi credentials in another way.
In Maxim's project the WiFi Credentials are hardcoded and cannot be modified after flashing the device. I have added support for [ESP-IDF Wi-Fi Provisioning], now user can provide WiFi credentials via [ESP SoftAP Provisioning] app even after flashing accessory. The next feature I have added is support for two ways of controlling the accessory. First directly from HomeKit app and the second one by the current wall-mounted physical switch.

## Flashing
<p align="center">
<img src="images/flashing.jpg" alt="drawing" width="400"/>
</p>
<br/>

# Final Mounting
<p align="center">
  <img src="images/disclaimer.jpg" alt="drawing" width="500"/>
</p>


>
* The device is designed to control other electrical devices and
operate in their housings. Faulty connection or use may result in fire or electric shock.

* All works on the device may be performed only by a qualified and licensed electrician. Observe national regulations.

* Even when the device is turned off, voltage may be present at
its terminals. Any maintenance introducing changes into the configuration of connections or the load must be always performed with the disabled fuse or power supply.

<br/>

<p align="center">
<img src="images/mounting.gif" alt="drawing" width="900"/>
</p>
<br/>

## Provisioning WiFi credentials & adding to HomeKit
As I mentioned before Apple doesn't allow to use Apple's WAC (Wireless Accessory Configuration) feature for devices without MFi license so we will use an official application which is available in AppStore: [ESP SoftAP Provisioning] from [ESP32] manufacturer. After successfully connecting to WiFi the HomeKit Accessory is ready to add to your home.

<p align="center">
<img src="images/addingToHomeKit.gif" alt="drawing" width="300"/>
</p>
<br/><br/>


<p align="center">
<img src="images/switchHome.PNG" alt="drawing" width="400"/>
<img src="images/switchSettings.PNG" alt="drawing" width="400"/>
</p>




[ESP32]: <https://www.espressif.com/en/products/socs/esp32>
[ESP-IDF]: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/>
[ESP-IDF Wi-Fi Provisioning]: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/provisioning/wifi_provisioning.html#>

[ESP SoftAP Provisioning]: <https://apps.apple.com/us/app/esp-softap-provisioning/id1474040630>
[LEDC documentation]: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html>
[Bluetooth Example Walkthrough]: <https://github.com/espressif/esp-idf/blob/1067b28707e527f177752741e3aa08b5dc64a4d7/examples/bluetooth/bluedroid/ble/gatt_server_service_table/tutorial/Gatt_Server_Service_Table_Example_Walkthrough.md>


[ALLPCB]: <https://www.allpcb.com>
[maximkulkin/esp-homekit]: <https://github.com/maximkulkin/esp-homekit>
[ESP32-WROOM-32d-Datasheet]: <https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32d_esp32-wroom-32u_datasheet_en.pdf>


