# Stryd-to-FTMS
Allows an ESP32 to connect to your Stryd Footpod and report the data as a Treadmill FTMS device to your 3rd party apps.

Apps such as Zwift and Peloton allow FTMS connectivity to record treadmill data however without an FTMS treadmill I needed another way to send the data from my Stryd to the app.

The ESP device must have BLE, ensure your BLE is enabled and setup correctly.

## Setup
Retrieve the MAC Address of your Stryd by using nRF Connect on your phone.
Update the script with your Stryd MAC Address.
Upload the script to your ESP32 device.

## Operation
On boot the ESP should detect the Stryd if awake/available and will begin broadcasting the ESP as an FTMS device.
