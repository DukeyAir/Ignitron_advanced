/*
 * SparkButtonHandler.h
 *
 *  Created on: 23.08.2021
 *      Author: stangreg
 */

#ifndef SPARKBUTTONHANDLER_H_
#define SPARKBUTTONHANDLER_H_

#include "Config_Definitions.h"
#include "SparkDataControl.h"
#include "SparkPresetControl.h"
#include "SparkTypes.h"
#include <BfButton.h> //https://github.com/mickey9801/ButtonFever

#ifdef BOARD_LILYGO_T_DISPLAY_S3
#include "SparkMidiControl.h"
#endif

using namespace std;

class SparkButtonHandler {
public:
    SparkButtonHandler();
    SparkButtonHandler(SparkDataControl *dc);
    virtual ~SparkButtonHandler();

    void readButtons();
    OperationMode checkBootOperationMode();
    static void configureButtons();
    void setDataControl(SparkDataControl *dc) {
        spark_dc_ = dc;
    }

private:
    static BfButton btn_preset1_;
    static BfButton btn_preset2_;
    static BfButton btn_preset3_;
    static BfButton btn_preset4_;
    static BfButton btn_bank_up_;
    static BfButton btn_bank_down_;

#ifdef BOARD_LILYGO_T_DISPLAY_S3
    static BfButton btn_extra1_;
    static BfButton btn_extra2_;
    static SparkMidiControl *midiControl_;
public:
    void setMidiControl(SparkMidiControl *mc) { midiControl_ = mc; }
private:
#endif

    static SparkDataControl *spark_dc_;
    static SparkPresetControl &presetControl_;
    static KeyboardMapping currentKeyboard_;

    // BUTTON Handlers
    static void btnPresetHandler(BfButton *btn, BfButton::press_pattern_t pattern);
    static void btnLooperPresetHandler(BfButton *btn, BfButton::press_pattern_t pattern);
    static void btnSpark2LooperHandler(BfButton *btn, BfButton::press_pattern_t pattern);
    static void btnSpark2LooperConfigHandler(BfButton *btn, BfButton::press_pattern_t pattern);
    static void btnBankHandler(BfButton *btn, BfButton::press_pattern_t pattern);
    static void btnSwitchModeHandler(BfButton *btn, BfButton::press_pattern_t pattern);
    static void btnDeletePresetHandler(BfButton *btn, BfButton::press_pattern_t pattern);
    static void btnToggleFXHandler(BfButton *btn, BfButton::press_pattern_t pattern);
    static void btnResetHandler(BfButton *btn, BfButton::press_pattern_t pattern);
    static void btnKeyboardHandler(BfButton *btn, BfButton::press_pattern_t pattern);
    static void btnKeyboardSwitchHandler(BfButton *btn, BfButton::press_pattern_t pattern);
    static void btnToggleLoopHandler(BfButton *btn, BfButton::press_pattern_t pattern);
    static void btnToggleBTModeHandler(BfButton *btn, BfButton::press_pattern_t pattern);
    static void btnSwitchTunerModeHandler(BfButton *btn, BfButton::press_pattern_t pattern);
    static void doNothing(BfButton *btn, BfButton::press_pattern_t pattern);

    static void configureLooperButtons();
    static void configureSpark2LooperControlButtons();
    static void configureSpark2LooperConfigButtons();
    static void configureAppButtonsPreset();
    static void configureAppButtonsFX();
    static void configureAmpButtons();
    static void configureKeyboardButtons();
    static void configureTunerButtons();
#ifdef BOARD_LILYGO_T_DISPLAY_S3
    static void configureMidiButtons();
    static void btnMidiHandler(BfButton *btn, BfButton::press_pattern_t pattern);
    static void btnExtraHandler(BfButton *btn, BfButton::press_pattern_t pattern);
#endif
};

#endif /* SPARKBUTTONHANDLER_H_ */
