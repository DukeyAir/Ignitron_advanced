/*
 * SparkBLEMidi.cpp
 *
 * BLE MIDI service implementation using NimBLE.
 * Follows the BLE MIDI specification (Apple/MMA).
 */

#include "SparkBLEMidi.h"

// Standard BLE MIDI UUIDs
const char *SparkBLEMidi::SERVICE_UUID = "03B80E5A-EDE8-4B33-A751-6CE34EC4C700";
const char *SparkBLEMidi::CHARACTERISTIC_UUID = "7772E5DB-3868-4112-A1A9-F2669D106BF3";

SparkBLEMidi::SparkBLEMidi() {}

SparkBLEMidi::~SparkBLEMidi() {
    end();
}

void SparkBLEMidi::init(const string &deviceName) {
    if (isInitialized_) {
        return;
    }

    Serial.println("Initializing BLE MIDI...");

    NimBLEDevice::init(deviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setSecurityAuth(false, false, false);

    pServer_ = NimBLEDevice::createServer();
    pServer_->setCallbacks(this);

    NimBLEService *pService = pServer_->createService(SERVICE_UUID);

    pCharacteristic_ = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::WRITE |
            NIMBLE_PROPERTY::WRITE_NR |
            NIMBLE_PROPERTY::NOTIFY);

    pService->start();

    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setAppearance(0x0000);
    pAdvertising->start();

    isInitialized_ = true;
    Serial.println("BLE MIDI initialized and advertising.");
}

void SparkBLEMidi::end() {
    if (!isInitialized_) {
        return;
    }
    if (pServer_) {
        for (int i = 0; i < pServer_->getConnectedCount(); i++) {
            pServer_->disconnect(pServer_->getPeerInfo(i).getConnHandle());
        }
        NimBLEDevice::getAdvertising()->stop();
    }
    isInitialized_ = false;
    isConnected_ = false;
    Serial.println("BLE MIDI stopped.");
}

void SparkBLEMidi::onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc) {
    isConnected_ = true;
    Serial.println("BLE MIDI client connected.");
    // Allow multiple connections
    NimBLEDevice::getAdvertising()->start();
}

void SparkBLEMidi::onDisconnect(NimBLEServer *pServer) {
    isConnected_ = false;
    Serial.println("BLE MIDI client disconnected");
    NimBLEDevice::getAdvertising()->start();
}

void SparkBLEMidi::sendBLEMidiPacket(byte *midiData, size_t length) {
    if (!isConnected_ || !pCharacteristic_) {
        return;
    }

    // BLE MIDI packet format:
    // [header] [timestamp] [midi status] [midi data...]
    // Header:   1ttttttt (bit 7 = 1, bits 6-0 = timestamp high)
    // Timestamp: 1ttttttt (bit 7 = 1, bits 6-0 = timestamp low)
    unsigned long ms = millis();
    byte header = 0x80 | ((ms >> 7) & 0x3F);
    byte timestamp = 0x80 | (ms & 0x7F);

    byte packet[2 + length];
    packet[0] = header;
    packet[1] = timestamp;
    memcpy(&packet[2], midiData, length);

    pCharacteristic_->setValue(packet, 2 + length);
    pCharacteristic_->notify();
}

void SparkBLEMidi::sendNoteOn(byte channel, byte note, byte velocity) {
    byte msg[3] = {(byte)(0x90 | (channel & 0x0F)), (byte)(note & 0x7F), (byte)(velocity & 0x7F)};
    sendBLEMidiPacket(msg, 3);
}

void SparkBLEMidi::sendNoteOff(byte channel, byte note, byte velocity) {
    byte msg[3] = {(byte)(0x80 | (channel & 0x0F)), (byte)(note & 0x7F), (byte)(velocity & 0x7F)};
    sendBLEMidiPacket(msg, 3);
}

void SparkBLEMidi::sendControlChange(byte channel, byte ccNumber, byte ccValue) {
    byte msg[3] = {(byte)(0xB0 | (channel & 0x0F)), (byte)(ccNumber & 0x7F), (byte)(ccValue & 0x7F)};
    sendBLEMidiPacket(msg, 3);
}

void SparkBLEMidi::sendProgramChange(byte channel, byte program) {
    byte msg[2] = {(byte)(0xC0 | (channel & 0x0F)), (byte)(program & 0x7F)};
    sendBLEMidiPacket(msg, 2);
}

void SparkBLEMidi::sendMidiMessage(byte status, byte data1, byte data2) {
    byte msgType = status & 0xF0;
    if (msgType == 0xC0 || msgType == 0xD0) {
        // Program Change and Channel Pressure are 2-byte messages
        byte msg[2] = {status, (byte)(data1 & 0x7F)};
        sendBLEMidiPacket(msg, 2);
    } else {
        byte msg[3] = {status, (byte)(data1 & 0x7F), (byte)(data2 & 0x7F)};
        sendBLEMidiPacket(msg, 3);
    }
}

void SparkBLEMidi::processButtonAction(MidiButtonConfig &config) {
    byte channel = config.channel & 0x0F;

    switch (config.messageType) {
    case MIDI_MSG_NOTE_ON:
        if (config.toggle) {
            config.state = !config.state;
            if (config.state) {
                sendNoteOn(channel, config.data1, config.data2);
            } else {
                sendNoteOff(channel, config.data1, 0);
            }
        } else {
            sendNoteOn(channel, config.data1, config.data2);
        }
        break;

    case MIDI_MSG_NOTE_OFF:
        sendNoteOff(channel, config.data1, config.data2);
        break;

    case MIDI_MSG_CC:
        if (config.toggle) {
            config.state = !config.state;
            sendControlChange(channel, config.data1, config.state ? config.data2 : 0);
        } else {
            sendControlChange(channel, config.data1, config.data2);
        }
        break;

    case MIDI_MSG_PROGRAM_CHANGE:
        sendProgramChange(channel, config.data1);
        break;
    }
}
