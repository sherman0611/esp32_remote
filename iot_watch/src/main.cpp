#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#include <WiFi.h>
#include "time.h"

#include "config.h"

TTGOClass *ttgo;

// wifi
String ssid = "uos-other";
String password = "shefotherkey05";

// time
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;
lv_obj_t *time_label;
time_t now;

// battery
AXP20X_Class *power;
lv_obj_t *battery_label;

// pek button power on/off
bool irq = false;

// led light control
bool lightOff = true;
String ledVal = "0";

// ble
BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
BLEUUID charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
bool foundServer = false;
bool btConnected = false;
BLERemoteCharacteristic* pRemoteCharacteristic;
BLEAdvertisedDevice* pAdvertisedDevice;
BLEScan* pBLEScan;

class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pClient) {
        btConnected = true;
        Serial.println("Connected to server!");
    }

    void onDisconnect(BLEClient* pClient) {
        btConnected = false;
        Serial.println("Disconnected from server!");
    }
};

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        // Serial.print("BLE Advertised Device found: ");
        // Serial.println(advertisedDevice.getAddress().toString().c_str());

        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
            BLEDevice::getScan()->stop();
            pAdvertisedDevice = new BLEAdvertisedDevice(advertisedDevice);
            foundServer = true;
        }
    }
};

void connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(pAdvertisedDevice->getAddress().toString().c_str());
        
    BLEClient* pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());
    pClient->connect(pAdvertisedDevice);

    /* Obtain a reference to the service we are after in the remote BLE server */
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
        Serial.print("Failed to find our service UUID: ");
        Serial.println(serviceUUID.toString().c_str());
        pClient->disconnect();
        return;
    }
    Serial.println(" - Found our service");

    /* Obtain a reference to the characteristic in the service of the remote BLE server */
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
        Serial.print("Failed to find our characteristic UUID: ");
        Serial.println(charUUID.toString().c_str());
        pClient->disconnect();
        return;
    }
    Serial.println(" - Found our characteristic");

    btConnected = true;
    foundServer = false;
    return;
}

void event_handler_click(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        Serial.println("2");
        ledVal = "2";
        pRemoteCharacteristic->writeValue(ledVal.c_str(), ledVal.length());
    }
}

void event_handler_toggle(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        if (lightOff) {
            Serial.println("1");
            ledVal = "1";
            lightOff = false;
        } else {
            ledVal = "0";
            Serial.println("0");
            lightOff = true;
        }
        pRemoteCharacteristic->writeValue(ledVal.c_str(), ledVal.length());
    }
}

void update_battery_display() {
    if (power->isBatteryConnect()) {
        char status[20];
        char percentage[10];
        sprintf(percentage, "%d%%", power->getBattPercentage());
        if (power->isChargeing()) {
            sprintf(status, "Charging: %s", percentage);
        } else {
            sprintf(status, "Battery: %s", percentage);
        }
        lv_label_set_text(battery_label, status);
    } else {
        lv_label_set_text(battery_label, "Battery Here");
    }
}

void setup() {
    Serial.begin(9600);
    // while (!Serial);
    delay(3000);
    Serial.println("Starting setup...");

    ttgo = TTGOClass::getWatch();
    ttgo->begin();
    ttgo->openBL();

    // create power and cycle buttons
    ttgo->lvgl_begin();

    lv_obj_t *label;

    lv_obj_t *btn1 = lv_btn_create(lv_scr_act(), NULL);
    lv_obj_set_event_cb(btn1, event_handler_click);
    lv_obj_align(btn1, NULL, LV_ALIGN_CENTER, 0, -40);

    label = lv_label_create(btn1, NULL);
    lv_label_set_text(label, "Cycle");

    lv_obj_t *btn2 = lv_btn_create(lv_scr_act(), NULL);
    lv_obj_set_event_cb(btn2, event_handler_toggle);
    lv_obj_align(btn2, NULL, LV_ALIGN_CENTER, 0, 40);
    lv_btn_set_checkable(btn2, true);
    lv_btn_set_fit2(btn2, LV_FIT_NONE, LV_FIT_TIGHT);

    label = lv_label_create(btn2, NULL);
    lv_label_set_text(label, "Power");

    // create time label
    time_label = lv_label_create(lv_scr_act(), NULL);
    lv_obj_align(time_label, NULL, LV_ALIGN_IN_TOP_LEFT, 10, 10);
    lv_label_set_text(time_label, "Time Here"); // default text

    //create battery label
    battery_label = lv_label_create(lv_scr_act(), NULL);
    lv_obj_align(battery_label, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 10, -10);
    lv_label_set_text(battery_label, "Battery Here"); // default text

    // power on/off
    pinMode(AXP202_INT, INPUT_PULLUP);
    attachInterrupt(AXP202_INT, [] {
        irq = true;
    }, FALLING);
    // !Clear IRQ unprocessed first
    ttgo->power->enableIRQ(AXP202_PEK_SHORTPRESS_IRQ, true);
    ttgo->power->clearIRQ();

    pinMode(TOUCH_INT, INPUT);

    // battery
    power = ttgo->power;
    power->adc1Enable(
        AXP202_VBUS_VOL_ADC1 |
        AXP202_VBUS_CUR_ADC1 |
        AXP202_BATT_CUR_ADC1 |
        AXP202_BATT_VOL_ADC1,
        true);

    // wifi
    Serial.print("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    while(WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    delay(500); // let things settle for half a second
    Serial.println("");
    Serial.println("WiFi connected!");

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); // initialise time

    // wait for time to synchronise
    while (!time(nullptr)) {
        Serial.println("Waiting for time to sync...");
        delay(1000);
    }
    Serial.println("Time synced!");

    // continue until current time retreived
    bool timeGet = false;
    while (!timeGet) {
        now = time(nullptr);
        // Check if time is synced
        if (now > 100000) { 
            lv_label_set_text(time_label, ctime(&now)); // set time
            timeGet = true;
            Serial.println("Current time retrieved!");
        }
    }

    // disconnect wifi after retrieving time
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi disconnected!");

    // ble
    BLEDevice::init("ESP32-BLE-Client");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());

    Serial.println("Setup complete!");
}

void loop() {
    // power on/off
    if (irq) {
        irq = false;
        ttgo->power->readIRQ();
        if (ttgo->power->isPEKShortPressIRQ()) {
            // Clean power chip irq status
            ttgo->power->clearIRQ();

            // Set  touchscreen sleep
            ttgo->displaySleep();

            ttgo->powerOff();

            //Set all channel power off
            ttgo->power->setPowerOutPut(AXP202_LDO3, false);
            ttgo->power->setPowerOutPut(AXP202_LDO4, false);
            ttgo->power->setPowerOutPut(AXP202_LDO2, false);
            ttgo->power->setPowerOutPut(AXP202_EXTEN, false);
            ttgo->power->setPowerOutPut(AXP202_DCDC2, false);

            // PEK KEY  Wakeup source
            esp_sleep_enable_ext1_wakeup(GPIO_SEL_35, ESP_EXT1_WAKEUP_ALL_LOW);
            esp_deep_sleep_start();
        }
        ttgo->power->clearIRQ();
    }

    update_battery_display();

    lv_task_handler();

    // try to connect to ble server if disconnected
    if (!btConnected) {
        pBLEScan->start(3);

        if (foundServer) {
            connectToServer();
        }
    }
}
