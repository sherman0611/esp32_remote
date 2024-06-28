#include <Arduino.h>
#include <Wire.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#include <Adafruit_DotStar.h>

// #include <WiFi.h>
// #include <WiFiUdp.h>
// #include <HTTPClient.h>
// #include <ArduinoOTA.h>

// int firmwareVersion = 1; // Used to check for updates

// String ssid = "uos-other";
// String password = "shefotherkey05";

// // IP address and port number
// #define FIRMWARE_SERVER_IP_ADDR "0.0.0.0" // CHANGE TO SELF DEVICE IP!!!
// #define FIRMWARE_SERVER_PORT    "8000"

// int loopIteration = 0; // A control iterator for slicing up the main loop

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

bool connected = false;
BLEServer *pServer;
BLEService *pService;
BLECharacteristic *pCharacteristic;

#define NUM_PIXELS 72  // Change this to the number of LEDs in your strip
#define DATA_PIN 8     // A5
#define CLOCK_PIN 36    // SCK
#define COLOR_ORDER DOTSTAR_BGR // Color order of your LED strip

Adafruit_DotStar strip = Adafruit_DotStar(NUM_PIXELS, DATA_PIN, CLOCK_PIN, COLOR_ORDER);

std::string value;
int currentColorIndex = 0;
bool lightsOff = true;

class MyServerCallback : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        connected = true;
        Serial.println("Connected to client!");
    }

    void onDisconnect(BLEServer* pServer) {
        connected = false;
        Serial.println("Disconnected from client!");
    }
};

void turnOffAll() {
    for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 0)); // Black (off)
    }
    strip.show();
}

void turnOnAll() {
    for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(255, 255, 255)); // white
    }
    strip.show();
}

void cycleColors() {
    for (int i = 0; i < 256; i++) {
        for (int j = 0; j < strip.numPixels(); j++) {
            if (lightsOff) {
                return;
            }
            strip.setPixelColor(j, strip.Color(i, 255 - i, 0));
        }
        strip.show();
        delay(10);
    }
}


// // helper for downloading from cloud firmware server; for experimental
// // purposes just use a hard-coded IP address and port (defined above)
// int doCloudGet(HTTPClient *http, String fileName) {
//     // build up URL from components; for example:
//     // http://192.168.4.2:8000/Thing.bin
//     String url =
//         String("http://") + FIRMWARE_SERVER_IP_ADDR + ":" +
//         FIRMWARE_SERVER_PORT + "/" + fileName;
//     Serial.printf("getting %s\n", url.c_str());

//     // make GET request and return the response code
//     http->begin(url);
//     http->addHeader("User-Agent", "ESP32");
//     return http->GET();
// }

// void OTAUpdate() {
//     // materials for doing an HTTPS GET on github from the BinFiles/ dir
//     HTTPClient http;
//     int respCode;
//     int highestAvailableVersion = -1;

//     // read the version file from the cloud
//     respCode = doCloudGet(&http, "version.txt");
//     if(respCode > 0) // check response code (-ve on failure)
//         highestAvailableVersion = atoi(http.getString().c_str());
//     else
//         Serial.printf("couldn't get version! rtn code: %d\n", respCode);
//     http.end(); // free resources

//     // do we know the latest version, and does the firmware need updating?
//     if(respCode < 0) {
//         return;
//     } else if(firmwareVersion >= highestAvailableVersion) {
//         Serial.printf("firmware is up to date\n");
//         return;
//     }

//     // do a firmware update
//     Serial.printf(
//         "upgrading firmware from version %d to version %d\n",
//         firmwareVersion, highestAvailableVersion
//     );

//     // do a GET for the .bin, e.g. "23.bin" when "version.txt" contains 23
//     String binName = String(highestAvailableVersion);
//     binName += ".bin";
//     respCode = doCloudGet(&http, binName);
//     int updateLength = http.getSize();

//     // possible improvement: if size is improbably big or small, refuse
//     if(respCode > 0 && respCode != 404) { // check response code (-ve on failure)
//         Serial.printf(".bin code/size: %d; %d\n\n", respCode, updateLength);
//     } else {
//         Serial.printf("failed to get .bin! return code is: %d\n", respCode);
//         http.end(); // free resources
//         return;
//     }

//     // write the new version of the firmware to flash
//     WiFiClient stream = http.getStream();
//     if(Update.begin(updateLength)) {
//         Serial.printf("starting OTA may take a minute or two...\n");
//         Update.writeStream(stream);
//         if(Update.end()) {
//             Serial.printf("update done, now finishing...\n");
//             Serial.flush();
//             if(Update.isFinished()) {
//                 Serial.printf("update successfully finished; rebooting...\n\n");
//                 ESP.restart();
//             } else {
//                 Serial.printf("update didn't finish correctly :(\n");
//                 Serial.flush();
//             }
//         } else {
//             Serial.printf("an update error occurred, #: %d\n" + Update.getError());
//             Serial.flush();
//         }
//     } else {
//         Serial.printf("not enough space to start OTA update :(\n");
//         Serial.flush();
//     }
//     stream.flush();
// }


void setup() {
    Serial.begin(115200);
    delay(3000);
    Serial.println("Beginning setup...");

    // Serial.printf("Running firmware is at version %d\n", firmwareVersion);
    // // connect to wifi
    // Serial.printf("Connecting to %s ", ssid);
    // WiFi.begin(ssid, password);
    // while (WiFi.status() != WL_CONNECTED) {
    //     delay(500);
    //     Serial.print(".");
    // }
    // Serial.println("WiFi connected!");
    
    // OTAUpdate(); // Check for firmware version and update if required

    // ble 
    BLEDevice::init("ESP32-BLE-Server");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallback());
    pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
                                          CHARACTERISTIC_UUID,
                                          BLECharacteristic::PROPERTY_READ |
                                          BLECharacteristic::PROPERTY_WRITE
                                        );
    pCharacteristic->setValue("0"); // default light off value
    
    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    strip.begin();
    turnOffAll();// light off
    strip.setBrightness(1);  // Set brightness (0-255)

    Serial.println("Setup complete!");
}

void loop() {
    // // check for ota update every specified slice
    // int sliceSize = 500000;
    // loopIteration++;
    // if(loopIteration % sliceSize == 0) {
    //     OTAUpdate();
    // }

    if (!connected) {
        BLEDevice::startAdvertising();
    } else {
        value = pCharacteristic->getValue();

        if (value == "0") {
            // lights off
            lightsOff = true;
            turnOffAll(); 
        } else {
            lightsOff = false;
            if (value == "1") {
                // lights on
                turnOnAll();
            } else if (value == "2") {
                // cycle lights colour
                cycleColors();
            }
        }
    }
    delay(100);
}