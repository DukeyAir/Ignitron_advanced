/*
 * SparkBLEMidi.h
 *
 * BLE MIDI service for generic MIDI controller mode.
 * Uses standard BLE MIDI specification (Apple/MMA).
 */

#ifndef SPARKBLEMIDI_H_
#define SPARKBLEMIDI_H_

#include "Config_Definitions.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string>

using namespace std;

class SparkBLEMidi : public NimBLEServerCallbacks,
                     NimBLECharacteristicCallbacks {
public:
    SparkBLEMidi();
    virtual ~SparkBLEMidi();

    void init(const string &deviceName = "Ignitron MIDI");
    void end();
    bool isConnected() const { return isConnected_; }

    // Send MIDI messages
    void sendNoteOn(byte channel, byte note, byte velocity);
    void sendNoteOff(byte channel, byte note, byte velocity = 0);
    void sendControlChange(byte channel, byte ccNumber, byte ccValue);
    void sendProgramChange(byte channel, byte program);

    // Send a raw MIDI message (1-3 bytes)
    void sendMidiMessage(byte status, byte data1, byte data2 = 0);

    // Process a button config action
    void processButtonAction(MidiButtonConfig &config);

private:
    // BLE MIDI service UUID: 03B80E5A-EDE8-4B33-A751-6CE34EC4C700
    static const char *SERVICE_UUID;
    // BLE MIDI characteristic UUID: 7772E5DB-3868-4112-A1A9-F2669D106BF3
    static const char *CHARACTERISTIC_UUID;

    bool isConnected_ = false;
    bool isInitialized_ = false;

    NimBLEServer *pServer_ = nullptr;
    NimBLECharacteristic *pCharacteristic_ = nullptr;

    // NimBLE callbacks
    void onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc) override;
    void onDisconnect(NimBLEServer *pServer) override;

    // Build BLE MIDI packet with timestamp
    void sendBLEMidiPacket(byte *midiData, size_t length);
};

#endif /* SPARKBLEMIDI_H_ */
