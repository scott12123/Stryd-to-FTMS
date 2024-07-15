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

### Other
I also had a MPU6050 to record the incline however this has not been reporting live data accurately. Set MPU6050_Installed to false if you dont want to record incline data.

<a href="https://www.buymeacoffee.com/scott12123" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/default-orange.png" alt="Buy Me A Coffee" height="41" width="174"></a>
