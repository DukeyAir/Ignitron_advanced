/*
 * SparkLEDControl.cpp
 *
 *  Created on: 23.08.2021
 *      Author: stangreg
 */

#include "SparkLEDControl.h"

#ifdef USE_NEOPIXEL_LEDS
#include <FastLED.h>
CRGB SparkLEDControl::leds_[NEOPIXEL_NUM_LEDS];
#endif

SparkLEDControl::SparkLEDControl() {
    sparkDC = nullptr;
    // Don't call init() here - hardware not ready during static construction
}

SparkLEDControl::SparkLEDControl(SparkDataControl *dc) {
    sparkDC = dc;
    // Don't call init() here - hardware not ready during static construction
}

SparkLEDControl::~SparkLEDControl() {
}

void SparkLEDControl::init() {
#ifdef USE_NEOPIXEL_LEDS
    FastLED.addLeds<WS2812B, NEOPIXEL_DATA_PIN, GRB>(leds_, NEOPIXEL_NUM_LEDS);
    FastLED.setBrightness(50);
    allLedOff();
#else
    // Preset LEDs
    pinMode(LED_PRESET1_GPIO, OUTPUT);
    pinMode(LED_PRESET2_GPIO, OUTPUT);
    pinMode(LED_PRESET3_GPIO, OUTPUT);
    pinMode(LED_PRESET4_GPIO, OUTPUT);
    pinMode(LED_BANK_DOWN_GPIO, OUTPUT);
    pinMode(LED_BANK_UP_GPIO, OUTPUT);

    // FX LEDS
    pinMode(LED_DRIVE_GPIO, OUTPUT);
    pinMode(LED_MOD_GPIO, OUTPUT);
    pinMode(LED_DELAY_GPIO, OUTPUT);
    pinMode(LED_REVERB_GPIO, OUTPUT);
    pinMode(LED_NOISEGATE_GPIO, OUTPUT);
    pinMode(LED_COMP_GPIO, OUTPUT);

#ifndef DEDICATED_PRESET_LEDS
    pinMode(OPTIONAL_GPIO_1, OUTPUT);
    pinMode(OPTIONAL_GPIO_2, OUTPUT);
    pinMode(OPTIONAL_GPIO_3, OUTPUT);
    pinMode(OPTIONAL_GPIO_4, OUTPUT);
#endif
    allLedOff();
#endif // USE_NEOPIXEL_LEDS
}

void SparkLEDControl::updateLEDs() {

    operationMode = sparkDC->operationMode();
    activePreset = SparkPresetControl::getInstance().activePreset();
    activePresetNum = SparkPresetControl::getInstance().activePresetNum();

    switch (operationMode) {

    case SPARK_MODE_APP: {
        if (sparkDC->isInitBoot()) {
            allLedOff();
            break;
        }
        SubMode subMode = sparkDC->subMode();
        // Show only active LEDs
        switch (subMode) {
        case SUB_MODE_LOOPER:
        case SUB_MODE_SPK_LOOPER:
        case SUB_MODE_PRESET:
            updateLedAppPresetMode();
#ifndef DEDICATED_PRESET_LEDS
            // Do not break if DEDICATED_PRESET_LEDS is defined
            // Will thus fall through to next case and also update FX LEDs
            break;
#endif
        case SUB_MODE_FX:
            updateLedAppFXMode();
            break;

        case SUB_MODE_LOOP_CONFIG:
        case SUB_MODE_LOOP_CONTROL:
            updateLedLooperMode();
            break;

        case SUB_MODE_TUNER:
            updateLedTuner();
            break;
        }

    } break;

    case SPARK_MODE_AMP:
        updateLedAmp();
        break;
    case SPARK_MODE_KEYBOARD:
        updateLedKeyboard();
        break;
    }
}

void SparkLEDControl::updateLedAppPresetMode() {
    allLedOff();
    switchLed(activePresetNum, true, false);
}

void SparkLEDControl::updateLedAppFXMode() {
    if (!activePreset.isEmpty) {
        for (int btnNumber = 1; btnNumber <= 6; btnNumber++) {
            FxLedButtonNumber fxButton = static_cast<FxLedButtonNumber>(btnNumber);
            FxType fxIndex = SparkHelper::getFXIndexFromButtonNumber(fxButton);
            Pedal currentFX = activePreset.pedals[(int)fxIndex];
            switchLed(btnNumber, currentFX.isOn, true);
        }
    }
}

void SparkLEDControl::updateLedAmp() {
    unsigned long currentMillis = millis();

    int presetNumToEdit = SparkPresetControl::getInstance().presetNumToEdit();
    const PresetEditMode presetEditMode = SparkPresetControl::getInstance().presetEditMode();

    if (presetEditMode != PRESET_EDIT_NONE) {

        if (presetNumToEdit == 0) {
            for (int i = 1; i <= PRESETS_PER_BANK; i++) {
                switchLed(i, true);
            }
        } else if (currentMillis - previousMillis >= blinkInterval_ms) {
            // save the last time you blinked the LED
            previousMillis = currentMillis;

            allLedOff();
            // if the LED is off turn it on and vice-versa:
            if (ledState == LOW) {
                ledState = HIGH;
                switchLed(presetNumToEdit, true);
            } else {
                ledState = LOW;
            }
        }

    } else {
        allLedOff();
        switchLed(activePresetNum, true);
    }
}

void SparkLEDControl::updateLedKeyboard() {

    uint8_t pressedKey = sparkDC->lastKeyboardButtonPressed();
    if (pressedKey == 0) {
        allLedOff();
        return;
    }

    // Map index of pressed key from 1 to 6
    int index = mapping.indexOfKey(pressedKey);
    switchLed(index, HIGH);
}

void SparkLEDControl::updateLedLooperMode() {

    const bool onOff = sparkDC->looperControl().beatOnOff();
    allLedOff();
    // if the LED is off turn it on and vice-versa:
    if (onOff) {
        ledState = HIGH;
        switchLed(SPK_LOOPER_BPM_LED_ID, true);
    } else {
        ledState = LOW;
    }

    const bool isRecRunning = sparkDC->looperControl().isRecRunning();
    if (isRecRunning) {
        switchLed(SPK_LOOPER_REC_DUB_LED_ID, true);
    } else {
        switchLed(SPK_LOOPER_REC_DUB_LED_ID, false);
    }

    const bool isPlaying = sparkDC->looperControl().isPlaying();
    const bool isRecAvailable = sparkDC->looperControl().isRecAvailable();

    if (isPlaying) {
        switchLed(SPK_LOOPER_PLAY_STOP_LED_ID, true);
    } else if (isRecAvailable) {
        switchLed(SPK_LOOPER_PLAY_STOP_LED_ID, onOff);
    } else {
        switchLed(SPK_LOOPER_PLAY_STOP_LED_ID, false);
    }

    // TODO: Check why LED is not on when Undo is available during Dub/Play. Seems that 0278 is ignored during play.
    const bool canUndo = sparkDC->looperControl().canUndo();
    const bool canRedo = sparkDC->looperControl().canRedo();
    if (canRedo) {
        switchLed(SPK_LOOPER_UNDO_REDO_LED_ID, false);
    } else if (canUndo) {
        switchLed(SPK_LOOPER_UNDO_REDO_LED_ID, true);
    } else {
        switchLed(SPK_LOOPER_UNDO_REDO_LED_ID, false);
    }
}

void SparkLEDControl::updateLedTuner() {
    allLedOff();

    SparkStatus &status = SparkStatus::getInstance();

    int noteOffsetCents = status.noteOffsetCents();
    int centsTolerance = 5;

    if (noteOffsetCents > -50 && noteOffsetCents < -20) {
        switchLed(PRESET1_NUM, true);
    }
    if (noteOffsetCents > -50 && noteOffsetCents < centsTolerance) {
        switchLed(PRESET2_NUM, true);
    }

    if (noteOffsetCents > -centsTolerance) {
        switchLed(PRESET3_NUM, true);
    }
    if (noteOffsetCents > 20) {
        switchLed(PRESET4_NUM, true);
    }
}

void SparkLEDControl::allLedOff() {
#ifdef USE_NEOPIXEL_LEDS
    fill_solid(leds_, NEOPIXEL_NUM_LEDS, CRGB::Black);
    FastLED.show();
#else
    // Preset LEDs
    digitalWrite(LED_PRESET1_GPIO, LOW);
    digitalWrite(LED_PRESET2_GPIO, LOW);
    digitalWrite(LED_PRESET3_GPIO, LOW);
    digitalWrite(LED_PRESET4_GPIO, LOW);

    digitalWrite(LED_BANK_DOWN_GPIO, LOW);
    digitalWrite(LED_BANK_UP_GPIO, LOW);

    // FX LEDS
    digitalWrite(LED_DRIVE_GPIO, LOW);
    digitalWrite(LED_MOD_GPIO, LOW);
    digitalWrite(LED_DELAY_GPIO, LOW);
    digitalWrite(LED_REVERB_GPIO, LOW);

#ifndef DEDICATED_PRESET_LEDS
    digitalWrite(OPTIONAL_GPIO_1, LOW);
    digitalWrite(OPTIONAL_GPIO_2, LOW);
    digitalWrite(OPTIONAL_GPIO_3, LOW);
    digitalWrite(OPTIONAL_GPIO_4, LOW);
#endif
#endif // USE_NEOPIXEL_LEDS
}

void SparkLEDControl::switchLed(int num, bool on, bool fxMode) {
    unsigned long currentMillis = millis();
    int STATE;

    // Blink if enabled and in FX modification mode
    if (ENABLE_FX_BLINK && fxMode && sparkDC->operationMode() == SPARK_MODE_APP && sparkDC->subMode() == SUB_MODE_FX) {
        if (currentMillis - previousMillis >= blinkInterval_ms) {
            blinkInvert = !blinkInvert;
            previousMillis = currentMillis;
        }
        STATE = (on && !blinkInvert) ? HIGH : LOW;
    } else {
        STATE = (on) ? HIGH : LOW; // Normal non blink action
    }

#ifdef USE_NEOPIXEL_LEDS
    // Map button number to physical NeoPixel index.
    // Strip wiring: Top L→R = LED 0-3, Bottom R→L = LED 4-7
    // Button layout: Top L→R = BankUp(6), P4(4), (unused), (unused)
    //                Bot L→R = BankDown(5), P1(1), P2(2), P3(3)
    //              num:  1  2  3  4  5  6
    const int map[] = { 6, 5, 4, 1, 7, 0 };
    int ledIndex = (num >= 1 && num <= 6) ? map[num - 1] : -1;
    if (ledIndex >= 0 && ledIndex < NEOPIXEL_NUM_LEDS) {
        leds_[ledIndex] = (STATE == HIGH) ? CRGB::Green : CRGB::Black;
        FastLED.show();
    }
#else
    int ledGpio = SparkHelper::getLedGpio(num, fxMode);
    if (ledGpio != LED_GPIO_INVALID) {
        digitalWrite(ledGpio, STATE);
    }
#endif
}
