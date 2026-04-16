/*
 * SparkMidiControl.cpp
 *
 * MIDI button configuration manager.
 * Stores per-button MIDI mappings in /config/midi.json on LittleFS.
 */

#include "SparkMidiControl.h"

const char *SparkMidiControl::CONFIG_FILE = "/config/midi.json";

SparkMidiControl::SparkMidiControl() {
    loadDefaultConfig();
}

SparkMidiControl::~SparkMidiControl() {}

void SparkMidiControl::init() {
    loadConfig();
    bleMidi_.init("Ignitron MIDI");
}

void SparkMidiControl::loadDefaultConfig() {
    // Default: buttons 1-8 send CC messages on channel 1
    // CC numbers 1-8, value 127, toggle mode
    for (int i = 0; i < MIDI_NUM_BUTTONS; i++) {
        buttonConfigs_[i].messageType = MIDI_MSG_CC;
        buttonConfigs_[i].channel = MIDI_DEFAULT_CHANNEL;
        buttonConfigs_[i].data1 = i + 1; // CC# 1-8
        buttonConfigs_[i].data2 = 127;
        buttonConfigs_[i].toggle = true;
        buttonConfigs_[i].state = false;
    }
}

void SparkMidiControl::loadConfig() {
    if (!LittleFS.exists(CONFIG_FILE)) {
        Serial.println("MIDI config not found, using defaults.");
        saveConfig(); // Save defaults
        return;
    }

    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file) {
        Serial.println("Failed to open MIDI config file.");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) {
        Serial.printf("MIDI config parse error: %s\n", err.c_str());
        return;
    }

    JsonArray buttons = doc["buttons"].as<JsonArray>();
    if (buttons.isNull()) {
        return;
    }

    int i = 0;
    for (JsonObject btn : buttons) {
        if (i >= MIDI_NUM_BUTTONS)
            break;

        String type = btn["type"] | "CC";
        if (type == "NoteOn")
            buttonConfigs_[i].messageType = MIDI_MSG_NOTE_ON;
        else if (type == "NoteOff")
            buttonConfigs_[i].messageType = MIDI_MSG_NOTE_OFF;
        else if (type == "PC")
            buttonConfigs_[i].messageType = MIDI_MSG_PROGRAM_CHANGE;
        else
            buttonConfigs_[i].messageType = MIDI_MSG_CC;

        buttonConfigs_[i].channel = btn["channel"] | MIDI_DEFAULT_CHANNEL;
        buttonConfigs_[i].data1 = btn["data1"] | (i + 1);
        buttonConfigs_[i].data2 = btn["data2"] | 127;
        buttonConfigs_[i].toggle = btn["toggle"] | true;
        buttonConfigs_[i].state = false;

        i++;
    }

    Serial.printf("Loaded MIDI config with %d button mappings.\n", i);
}

void SparkMidiControl::saveConfig() {
    // Ensure config directory exists
    if (!LittleFS.exists("/config")) {
        LittleFS.mkdir("/config");
    }

    File file = LittleFS.open(CONFIG_FILE, "w");
    if (!file) {
        Serial.println("Failed to create MIDI config file.");
        return;
    }

    JsonDocument doc;
    JsonArray buttons = doc["buttons"].to<JsonArray>();

    for (int i = 0; i < MIDI_NUM_BUTTONS; i++) {
        JsonObject btn = buttons.add<JsonObject>();

        switch (buttonConfigs_[i].messageType) {
        case MIDI_MSG_NOTE_ON:
            btn["type"] = "NoteOn";
            break;
        case MIDI_MSG_NOTE_OFF:
            btn["type"] = "NoteOff";
            break;
        case MIDI_MSG_PROGRAM_CHANGE:
            btn["type"] = "PC";
            break;
        default:
            btn["type"] = "CC";
            break;
        }

        btn["channel"] = buttonConfigs_[i].channel;
        btn["data1"] = buttonConfigs_[i].data1;
        btn["data2"] = buttonConfigs_[i].data2;
        btn["toggle"] = buttonConfigs_[i].toggle;
    }

    serializeJson(doc, file);
    file.close();
    Serial.println("MIDI config saved.");
}

MidiButtonConfig &SparkMidiControl::getButtonConfig(int buttonIndex) {
    if (buttonIndex < 0 || buttonIndex >= MIDI_NUM_BUTTONS) {
        static MidiButtonConfig invalidConfig = {MIDI_MSG_CC, 0, 0, 0, false, false};
        return invalidConfig;
    }
    return buttonConfigs_[buttonIndex];
}

void SparkMidiControl::handleButtonPress(int buttonIndex) {
    if (buttonIndex < 0 || buttonIndex >= MIDI_NUM_BUTTONS) {
        return;
    }

    MidiButtonConfig &config = buttonConfigs_[buttonIndex];
    bleMidi_.processButtonAction(config);

    Serial.printf("MIDI Btn %d: type=0x%02X ch=%d d1=%d d2=%d toggle=%d state=%d\n",
                  buttonIndex + 1,
                  config.messageType, config.channel,
                  config.data1, config.data2,
                  config.toggle, config.state);
}

void SparkMidiControl::handleButtonRelease(int buttonIndex) {
    if (buttonIndex < 0 || buttonIndex >= MIDI_NUM_BUTTONS) {
        return;
    }

    MidiButtonConfig &config = buttonConfigs_[buttonIndex];

    // For momentary Note On, send Note Off on release
    if (!config.toggle && config.messageType == MIDI_MSG_NOTE_ON) {
        bleMidi_.sendNoteOff(config.channel, config.data1, 0);
    }
    // For momentary CC, send CC value 0 on release
    if (!config.toggle && config.messageType == MIDI_MSG_CC) {
        bleMidi_.sendControlChange(config.channel, config.data1, 0);
    }
}
