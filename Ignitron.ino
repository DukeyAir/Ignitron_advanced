#define CONFIG_LITTLEFS_SPIFFS_COMPAT

#include <Arduino.h>
#include <NimBLEDevice.h> // github NimBLE
#include <SPI.h>
#include <Wire.h>
#include <string>

#include "src/SparkButtonHandler.h"
#include "src/SparkDataControl.h"
#include "src/SparkDisplayControl.h"
#include "src/SparkLEDControl.h"
#include "src/SparkPresetControl.h"

using namespace std;

// Device Info Definitions
const string DEVICE_NAME = "Ignitron";

// Control classes
SparkDataControl spark_dc;
SparkButtonHandler spark_bh;
SparkLEDControl spark_led;
SparkDisplayControl sparkDisplay;
SparkPresetControl &presetControl = SparkPresetControl::getInstance();

unsigned long lastInitialPresetTimestamp = 0;
unsigned long currentTimestamp = 0;
int initialRequestInterval = 3000;

// Check for initial boot
bool isInitBoot;
OperationMode operationMode = SPARK_MODE_APP;

/////////////////////////////////////////////////////////
//
// INIT AND RUN
//
/////////////////////////////////////////////////////////

void setup() {

#ifdef BOARD_LILYGO_T_DISPLAY_S3
    // T-Display S3: Power on LCD (GPIO 15 must be HIGH)
    pinMode(15, OUTPUT);
    digitalWrite(15, HIGH);
    // Backlight on
    pinMode(38, OUTPUT);
    digitalWrite(38, HIGH);
#endif

    Serial.begin(115200);
#ifdef BOARD_LILYGO_T_DISPLAY_S3
    // On ESP32-S3 USB CDC, Serial may never become ready without a host
    delay(1000);
#else
    while (!Serial)
        ;
#endif

    Serial.println("Initializing");
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount failed");
        return;
    }
    // Filesystem diagnostic
    {
        File f1 = LittleFS.open("/PresetList.txt");
        File f2 = LittleFS.open("/PresetListUUIDs.txt");
        Serial.printf("FS CHECK: PresetList.txt=%s(%d) PresetListUUIDs.txt=%s(%d)\n",
                      f1 ? "OK" : "MISSING", f1 ? (int)f1.size() : -1,
                      f2 ? "OK" : "MISSING", f2 ? (int)f2.size() : -1);
        if (f1) f1.close();
        if (f2) f2.close();
    }
    // spark_dc = new SparkDataControl();
    spark_bh.setDataControl(&spark_dc);
    operationMode = spark_bh.checkBootOperationMode();

    // Setting operation mode before initializing
    operationMode = spark_dc.init(operationMode);
    spark_bh.configureButtons();
    Serial.printf("Operation mode: %d\n", operationMode);

    switch (operationMode) {
    case SPARK_MODE_APP:
        Serial.println("======= Entering APP mode =======");
        break;
    case SPARK_MODE_AMP:
        Serial.println("======= Entering AMP mode =======");
        break;
    case SPARK_MODE_KEYBOARD:
        Serial.println("======= Entering Keyboard mode =======");
        break;
    }

    sparkDisplay.setDataControl(&spark_dc);
    spark_dc.setDisplayControl(&sparkDisplay);
    sparkDisplay.init(operationMode);
    // Assigning data control to buttons;
    spark_bh.setDataControl(&spark_dc);
    // Initializing control classes
    spark_led.setDataControl(&spark_dc);
    spark_led.init();

    Serial.println("Initialization done.");
}

void loop() {

    // Methods to call only in APP mode
    if (operationMode == SPARK_MODE_APP) {
        while (!(spark_dc.checkBLEConnection())) {
            sparkDisplay.update(spark_dc.isInitBoot());
            spark_led.updateLEDs();
            spark_bh.readButtons();
        }

        // After connection is established, continue.
        //  On first boot, get the amp type and set the preset to Hardware setting 1.

        if (spark_dc.isInitBoot()) { // && !spark_dc.isInitHWRead()) {
            // This is only done once after the connection has been established
            // Read AMP name to determine special parameters
            spark_dc.getSerialNumber();
            //spark_dc.getAmpName();
            // delay(100);
            // spark_dc.getCurrentPresetFromSpark();
            spark_dc.isInitBoot() = false;
            // spark_dc.configureLooper();
        }
    }

    // Check if presets have been updated (not needed in Keyboard mode)
    if (operationMode != SPARK_MODE_KEYBOARD) {
        spark_dc.checkForUpdates();
    }
    // Reading button input
    spark_bh.configureButtons();
    spark_bh.readButtons();
#ifdef ENABLE_BATTERY_STATUS_INDICATOR
    // Update battery level
    spark_dc.updateBatteryLevel();
#endif
    // Update LED status
    spark_led.updateLEDs();
    // Update display
    sparkDisplay.update();
}
