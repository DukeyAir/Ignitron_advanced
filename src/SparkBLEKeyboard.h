/*
 * SparkBLEKeyboard.h
 *
 *  Created on: 27.12.2021
 *      Author: steffen
 */

#ifndef SPARKBLEKEYBOARD_H_
#define SPARKBLEKEYBOARD_H_

#include "Config_Definitions.h"

#ifndef BOARD_LILYGO_T_DISPLAY_S3

#include <Arduino.h>
#include <BleKeyboard.h>
#include <NimBLEDevice.h>

using namespace std;

class SparkBLEKeyboard: public BleKeyboard {
public:
	SparkBLEKeyboard();
	virtual ~SparkBLEKeyboard();

	void start();
	void end();
};

#endif // !BOARD_LILYGO_T_DISPLAY_S3

#endif /* SPARKBLEKEYBOARD_H_ */
