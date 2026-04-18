/*
 * SparkBLESerialBridge
 *
 * Transparent BLE <-> Bluetooth Serial bridge for Ignitron on ESP32-S3.
 * Runs on a standard ESP32-WROOM-32 board.
 *
 * Uses Bluedroid stack for BOTH classic BT (Serial) and BLE (client).
 *
 * BT Serial side: Advertises as "Spark 40 Audio" so the Android Spark app
 *                  connects to it as if it were a real Spark amp.
 * BLE side:        Connects as a client to the S3 pedal's BLE server
 *                  (advertised as "Spark 40 BLE", service FFC0).
 *
 * Data flows transparently in both directions:
 *   Android App -> BT Serial -> bridge -> BLE write (FFC1) -> S3 Pedal
 *   Android App <- BT Serial <- bridge <- BLE notify (FFC2) <- S3 Pedal
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BluetoothSerial.h>
#include <esp_gatt_common_api.h>
#include <esp_gattc_api.h>

// Spark BLE protocol UUIDs
static BLEUUID SERVICE_UUID("FFC0");
static BLEUUID WRITE_CHAR_UUID("FFC1");
static BLEUUID NOTIF_CHAR_UUID("FFC2");

// Target pedal name
static const char *TARGET_BLE_NAME = "Spark 40 BLE";
// BT Serial name Android app expects
static const char *BT_SERIAL_NAME = "Spark 40 Audio";

// Bridge states
enum BridgeState {
    STATE_IDLE,
    STATE_SCANNING,
    STATE_SCAN_DONE,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_WAIT_RETRY
};

// Globals
BluetoothSerial SerialBT;
BLEClient *bleClient = nullptr;
BLERemoteCharacteristic *writeChar = nullptr;
BLERemoteCharacteristic *notifChar = nullptr;
BLEAdvertisedDevice *targetDevice = nullptr;
BLEScan *bleScan = nullptr;

volatile BridgeState state = STATE_IDLE;
volatile bool bleConnected = false;
bool appConnected = false;
unsigned long retryTime = 0;
int retryCount = 0;

static const int BUF_SIZE = 1024;
uint8_t serialBuf[BUF_SIZE];
int serialBufLen = 0;

// BLE write chunk size - use negotiated MTU payload (MTU-3)
// The S3 pedal's NimBLE server needs complete Spark protocol headers
// in a single write for ACK detection and message parsing.
static int bleChunkSize = 247;  // default, updated after MTU negotiation
static const int INTER_CHUNK_DELAY = 10;  // ms between BLE writes
static const int INTER_MESSAGE_DELAY = 30;  // ms after last chunk (let S3 process & respond)

static const int SCAN_DURATION = 5;       // seconds per scan
static const int RETRY_DELAY_MS = 3000;
static const int MAX_RETRIES = 0;         // 0 = infinite

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// --- Callbacks ---

void btSerialCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    if (event == ESP_SPP_SRV_OPEN_EVT) {
        Serial.println("Android app connected via BT Serial");
        appConnected = true;
    }
    if (event == ESP_SPP_CLOSE_EVT) {
        Serial.println("Android app disconnected from BT Serial");
        appConnected = false;
    }
}

static void bleNotifyCallback(BLERemoteCharacteristic *pChar,
                               uint8_t *pData, size_t length, bool isNotify) {
    Serial.printf("<<< NOTIFY received: %d bytes\n", length);
    if (SerialBT.hasClient()) {
        SerialBT.write(pData, length);
        Serial.printf("BLE->Serial: forwarded %d bytes\n", length);
    } else {
        Serial.println("NOTIFY received but no BT Serial client connected");
    }
}

class ClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient *pClient) override {
        Serial.println("BLE onConnect callback");
        bleConnected = true;
    }
    void onDisconnect(BLEClient *pClient) override {
        Serial.println("BLE disconnected from S3 pedal");
        bleConnected = false;
        writeChar = nullptr;
        notifChar = nullptr;
        // Trigger rescan
        state = STATE_WAIT_RETRY;
        retryTime = millis() + RETRY_DELAY_MS;
        retryCount = 0;
    }
};

static ClientCallbacks clientCB;

class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        String name = advertisedDevice.getName().c_str();
        if (name.length() > 0) {
            Serial.printf("  Scan: %s\n", name.c_str());
        }
        // Match by name OR by service UUID
        bool nameMatch = (name == TARGET_BLE_NAME);
        bool svcMatch = advertisedDevice.haveServiceUUID() &&
                        advertisedDevice.isAdvertisingService(SERVICE_UUID);
        if (nameMatch || svcMatch) {
            Serial.printf(">>> Found S3 pedal! (name=%s, svcUUID=%s)\n",
                          nameMatch ? "yes" : "no",
                          svcMatch ? "yes" : "no");
            bleScan->stop();
            if (targetDevice) delete targetDevice;
            targetDevice = new BLEAdvertisedDevice(advertisedDevice);
            state = STATE_SCAN_DONE;
        }
    }
};

static ScanCallbacks scanCB;

// --- Connect ---

bool connectToPedal() {
    if (!targetDevice) return false;

    Serial.printf("Connecting to %s ...\n",
                  targetDevice->getAddress().toString().c_str());

    if (bleClient) {
        delete bleClient;
        bleClient = nullptr;
    }
    bleClient = BLEDevice::createClient();
    bleClient->setClientCallbacks(&clientCB);

    if (!bleClient->connect(targetDevice)) {
        Serial.println("BLE connect() failed");
        return false;
    }
    Serial.println("BLE connected!");

    // Wait for connection to fully stabilize
    delay(1000);

    // Request MTU exchange
    esp_ble_gattc_send_mtu_req(bleClient->getGattcIf(), bleClient->getConnId());
    delay(500);
    uint16_t mtu = bleClient->getMTU();
    bleChunkSize = mtu > 3 ? mtu - 3 : 20;
    Serial.printf("MTU after negotiation: %d (write chunk size: %d)\n", mtu, bleChunkSize);

    // Discover services
    Serial.println("Discovering services...");
    BLERemoteService *service = bleClient->getService(SERVICE_UUID);
    if (!service) {
        Serial.println("ERROR: FFC0 service not found");
        bleClient->disconnect();
        return false;
    }
    Serial.println("Found FFC0 service");

    writeChar = service->getCharacteristic(WRITE_CHAR_UUID);
    if (!writeChar) {
        Serial.println("ERROR: FFC1 write char not found");
        bleClient->disconnect();
        return false;
    }
    Serial.printf("Found FFC1 write char (handle: 0x%04X)\n", writeChar->getHandle());

    notifChar = service->getCharacteristic(NOTIF_CHAR_UUID);
    if (!notifChar) {
        Serial.println("ERROR: FFC2 notify char not found");
        bleClient->disconnect();
        return false;
    }
    Serial.printf("Found FFC2 notify char (handle: 0x%04X, canNotify: %s)\n",
                  notifChar->getHandle(),
                  notifChar->canNotify() ? "yes" : "no");

    // Enable notifications: write CCCD descriptor + register callback
    Serial.println("Enabling notifications...");
    BLERemoteDescriptor *cccd = notifChar->getDescriptor(BLEUUID((uint16_t)0x2902));
    if (cccd) {
        uint8_t notifOn[] = {0x01, 0x00};
        cccd->writeValue(notifOn, 2, true);
        Serial.println("CCCD descriptor written OK");
    } else {
        Serial.println("WARNING: CCCD descriptor not found, trying registerForNotify alone");
    }
    notifChar->registerForNotify(bleNotifyCallback);
    Serial.println("Registered for FFC2 notifications");

    delay(500);
    Serial.println("=== BLE bridge to S3 pedal READY ===");
    return true;
}

// --- Setup ---

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== Spark BLE <-> Serial Bridge v5 ===");
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    BLEDevice::init("SparkBridge");

    // Request large MTU so writes > 20 bytes don't get truncated
    esp_ble_gatt_set_local_mtu(517);
    Serial.println("Local MTU set to 517");

    bleScan = BLEDevice::getScan();
    bleScan->setAdvertisedDeviceCallbacks(&scanCB);
    bleScan->setInterval(100);
    bleScan->setWindow(99);
    bleScan->setActiveScan(true);

    SerialBT.register_callback(btSerialCallback);
    if (SerialBT.begin(BT_SERIAL_NAME, false)) {
        Serial.printf("BT Serial started as \"%s\"\n", BT_SERIAL_NAME);
    } else {
        Serial.println("BT Serial failed to start!");
    }

    state = STATE_IDLE;
    Serial.println("Setup complete, entering main loop");
}

// --- Main Loop ---

void loop() {
    // State machine
    switch (state) {

    case STATE_IDLE:
        Serial.println("Starting BLE scan...");
        bleScan->clearResults();
        bleScan->start(SCAN_DURATION, false); // blocking, finite duration
        // If scan callback found target, state is STATE_SCAN_DONE
        // Otherwise scan timed out with no match
        if (state != STATE_SCAN_DONE) {
            Serial.println("Scan complete, pedal not found. Retrying...");
            delay(1000);
            // stay in STATE_IDLE to scan again
        }
        break;

    case STATE_SCAN_DONE:
        Serial.println("Scan found pedal, waiting 1s before connect...");
        delay(1000);
        state = STATE_CONNECTING;
        break;

    case STATE_CONNECTING:
        if (connectToPedal()) {
            state = STATE_CONNECTED;
            retryCount = 0;
            Serial.println("Bridge active!");
        } else {
            retryCount++;
            Serial.printf("Connect failed (attempt %d)\n", retryCount);
            if (targetDevice) {
                delete targetDevice;
                targetDevice = nullptr;
            }
            state = STATE_WAIT_RETRY;
            retryTime = millis() + RETRY_DELAY_MS;
        }
        break;

    case STATE_CONNECTED:
        // Read incoming BT Serial data into buffer
        while (SerialBT.available() && serialBufLen < BUF_SIZE) {
            serialBuf[serialBufLen++] = SerialBT.read();
        }

        // Forward buffered data to BLE in small chunks (matching standard BLE behavior)
        // The S3 pedal's NimBLE server expects writes in BLE-sized pieces,
        // just like a direct BLE client would send at default MTU.
        if (bleConnected && writeChar && serialBufLen > 0) {
            int chunkSize = min(serialBufLen, bleChunkSize);
            writeChar->writeValue(serialBuf, chunkSize, false);  // write-without-response for speed
            Serial.printf("Serial->BLE: %d/%d bytes\n", chunkSize, serialBufLen);

            // Shift remaining data
            int remaining = serialBufLen - chunkSize;
            if (remaining > 0) {
                memmove(serialBuf, serialBuf + chunkSize, remaining);
            }
            serialBufLen = remaining;

            // Delay between chunks to let S3 pedal process
            if (serialBufLen > 0) {
                delay(INTER_CHUNK_DELAY);
            } else {
                // Buffer empty = end of a complete message.
                // Wait for S3 to process the request and send its response
                // before we forward the next message.
                delay(INTER_MESSAGE_DELAY);
            }
        }

        // Check if BLE dropped (onDisconnect sets state to WAIT_RETRY)
        if (!bleConnected) {
            Serial.println("BLE connection lost, will rescan");
            serialBufLen = 0;
            state = STATE_WAIT_RETRY;
            retryTime = millis() + RETRY_DELAY_MS;
            retryCount = 0;
        }
        break;

    case STATE_WAIT_RETRY:
        if (millis() >= retryTime) {
            state = STATE_IDLE;
        }
        break;
    }

    // LED indicator
    if (bleConnected && appConnected) {
        digitalWrite(LED_BUILTIN, HIGH);
    } else if (bleConnected || appConnected) {
        digitalWrite(LED_BUILTIN, (millis() / 500) % 2);
    } else {
        digitalWrite(LED_BUILTIN, LOW);
    }

    delay(1);
}
