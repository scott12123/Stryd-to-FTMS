#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <math.h> 
#include <Wire.h>
#include <MPU6050.h>

#define FTMS_SERVICE_UUID "1826"
#define TREADMILL_DATA_CHAR_UUID "2ACD"
#define FEATURE_CHAR_UUID "2ACC"
MPU6050 mpu;
BLEClient* pClient;
BLEUUID serviceUUID("00001814-0000-1000-8000-00805f9b34fb");
BLEUUID charUUID("00002a53-0000-1000-8000-00805f9b34fb");
BLEServer* pServer = NULL;
BLECharacteristic* pTreadmillDataCharacteristic = NULL;
BLECharacteristic* pFeatureCharacteristic = NULL;
bool deviceConnected = false;

////Customised data////
String stryd_mac = "00:00:00:00:00:00"; // Add MAC Address of Stryd Footpod (May be lower case)
bool MPU6050_Installed = true; //Update whether MPU6050 is connected via SCL/SDA
///////////////////////

// Stryd Data
void onNotifyCallback(BLERemoteCharacteristic* pCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  if (length < 3) {
    Serial.println("Unexpected notification length.");
    return;
  }

  uint8_t flags = pData[0];
  bool strideLengthPresent = flags & 0x01;
  bool totalDistancePresent = flags & 0x02;
  bool isRunning = flags & 0x04;

  // Instantaneous Cadence
  uint8_t cadence = pData[3];

  // Update offset for optional fields
  int offset = 4;

  // Instantaneous Stride Length (if present)
  uint16_t strideLength = 0;
  if (strideLengthPresent) {
    if (length >= offset + 2) {
      strideLength = (pData[offset + 1] << 8) | pData[offset];
      offset += 2;
    } else {
      Serial.println("Unexpected length for Stride Length field.");
    }
  }

  // Total Distance (if present)
  uint32_t totalDistance = 0;
  if (totalDistancePresent) {
    if (length >= offset + 4) {
      totalDistance = (pData[offset + 3] << 24) | (pData[offset + 2] << 16) | (pData[offset + 1] << 8) | pData[offset];
      offset += 4;
    } else {
      Serial.println("Unexpected length for Total Distance field.");
    }
  }

  // Instantaneous Speed
  uint16_t speed = (pData[2] << 8) | pData[1];
  float speedMps = static_cast<float>(speed) / 256.0;
  float speedKmh = speedMps * 3.6;
  Serial.print("Distance: ");
  Serial.print(totalDistance);
  Serial.println("m");
  Serial.print("Speed: ");
  Serial.print(speedKmh);
  Serial.println("km/h");

  // Update treadmill data with the stryd speed and total distance
  updateTreadmillData(speedKmh, totalDistance);
}

void updateTreadmillData(float speedKmh, float totalDistance) {
  uint16_t flags = 0b0000000000001100; // Flags for speed, distance and incline/ramp 

  uint16_t speedInHundredths = static_cast<uint16_t>(speedKmh * 100);
  uint32_t totalDistanceMeters = totalDistance / 10;  

  float incline = readIncline();
  Serial.print("Incline: ");
  Serial.println(incline, 1);
  
  uint16_t inclineInt = static_cast<uint16_t>(incline * 10);

  // Assemble the data payload
  uint8_t data[11];
  data[0] = flags & 0xFF;
  data[1] = (flags >> 8) & 0xFF;
  data[2] = speedInHundredths & 0xFF;
  data[3] = (speedInHundredths >> 8) & 0xFF;
  data[4] = totalDistanceMeters & 0xFF;
  data[5] = (totalDistanceMeters >> 8) & 0xFF;
  data[6] = (totalDistanceMeters >> 16) & 0xFF;
  data[7] = inclineInt & 0xFF; //incline 
  data[8] = (inclineInt >> 8) & 0xFF; //incline
  data[9] = inclineInt & 0xFF; // ramp
  data[10] = (inclineInt >> 8) & 0xFF; //ramp

  // Update the characteristic value with the generated treadmill data
  pTreadmillDataCharacteristic->setValue(data, sizeof(data));
  pTreadmillDataCharacteristic->notify();
}


float readIncline() {
  int16_t ax, ay, az;
  float sumX = 0.0;
  int samples = 0;

  unsigned long startTime = millis();
  while (millis() - startTime < 1000) {
    mpu.getAcceleration(&ax, &ay, &az);
    float accX = (float)ax / 16384.0;
    sumX += -accX; // Invert the signal if showing negative figure
    samples++;
    delay(10); // Adjust delay as needed to get desired sampling rate
  }

  if (samples > 0) {
    float avgX = sumX / samples;
    float avgIncline = avgX * 100; // Convert to percentage
    float incline = avgIncline + 7.5; // Adjust for calibration
    float inclineDegrees = incline / 0.886; // Convert to degrees

    // Round to the nearest 0.5
    inclineDegrees = round(inclineDegrees * 2) / 2.0;

    // Ensure the value does not go below zero
    inclineDegrees = max(inclineDegrees, 0.0f);

    return inclineDegrees; // Return the calculated and rounded inclination angle
  }
  return 0.0; // Default zero if no valid data
}



class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    Serial.println("Client connected");
    deviceConnected = true;
  }

  void onDisconnect(BLEServer* pServer) {
    Serial.println("Client disconnected");
    deviceConnected = false;
  }
};

void setup() {
  Serial.begin(115200);
  Wire.begin();
  mpu.initialize(); 

  BLEDevice::init("Stryd FTMS");

  // Create BLE server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create BLE service
  BLEService* pService = pServer->createService(FTMS_SERVICE_UUID);

  // Create BLE treadmill data characteristic
  pTreadmillDataCharacteristic = pService->createCharacteristic(
    TREADMILL_DATA_CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY);
  pTreadmillDataCharacteristic->addDescriptor(new BLE2902());

  // Create BLE feature characteristic
  pFeatureCharacteristic = pService->createCharacteristic(
    FEATURE_CHAR_UUID,
    BLECharacteristic::PROPERTY_READ);

  // Feature flags: bits indicating supported features
  uint32_t featureFlags = 0b00000000000000000000000111011111;
  uint8_t featureData[4];
  featureData[0] = featureFlags & 0xFF;
  featureData[1] = (featureFlags >> 8) & 0xFF;
  featureData[2] = (featureFlags >> 16) & 0xFF;
  featureData[3] = (featureFlags >> 24) & 0xFF;

  // Debug output
  Serial.print("Feature Data: ");
  for (int i = 0; i < 4; i++) {
    Serial.print(featureData[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  pFeatureCharacteristic->setValue(featureData, sizeof(featureData));

  // Start the service
  pService->start();

  // Start advertising with FTMS UUID
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(FTMS_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("Waiting for a client connection to notify...");
}

void loop() {
  if (!deviceConnected) {
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    BLEScanResults* pResults = pBLEScan->start(5);
    for (int i = 0; i < pResults->getCount(); i++) {
      BLEAdvertisedDevice advertisedDevice = pResults->getDevice(i);
      if (advertisedDevice.getAddress().toString() == stryd_mac) {
        pClient = BLEDevice::createClient();
        pClient->connect(&advertisedDevice);
        break;
      }
    }
  } else {
    if (pClient->isConnected()) {
      BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
      if (pRemoteService) {
        BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
        if (pRemoteCharacteristic) {
          // Enable notifications
          if (pRemoteCharacteristic->canNotify()) {
            pRemoteCharacteristic->registerForNotify(onNotifyCallback);
            // Write to CCCD to enable notifications
            uint8_t notificationOn[] = { 0x01, 0x00 };
            pRemoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue(notificationOn, 2, true);
          }
        }
      }
    } else {
      deviceConnected = false;
    }
  }
  delay(1000);
}
