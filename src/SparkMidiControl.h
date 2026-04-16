/*
 * SparkMidiControl.h
 *
 * Manages configurable MIDI button mappings for BLE MIDI mode.
 * Configurations are stored in JSON on LittleFS.
 */

#ifndef SPARKMIDICONTROL_H_
#define SPARKMIDICONTROL_H_

#include "Config_Definitions.h"
#include "SparkBLEMidi.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <string>

using namespace std;

class SparkMidiControl {
public:
    SparkMidiControl();
    virtual ~SparkMidiControl();

    void init();
    void loadConfig();
    void saveConfig();
    void loadDefaultConfig();

    MidiButtonConfig &getButtonConfig(int buttonIndex);
    int getNumButtons() const { return MIDI_NUM_BUTTONS; }

    SparkBLEMidi &bleMidi() { return bleMidi_; }

    void handleButtonPress(int buttonIndex);
    void handleButtonRelease(int buttonIndex);

private:
    static const char *CONFIG_FILE;

    SparkBLEMidi bleMidi_;
    MidiButtonConfig buttonConfigs_[MIDI_NUM_BUTTONS];
};

#endif /* SPARKMIDICONTROL_H_ */
