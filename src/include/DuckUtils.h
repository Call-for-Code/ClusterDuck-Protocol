/**
 * @file DuckUtils.h
 * @brief This file is internal to CDP and provides some common utility methods.
 * @version
 * @date 2020-09-16
 *
 * @copyright
 */

#ifndef DUCKUTILS_H_
#define DUCKUTILS_H_

#include "cdpcfg.h"
#include "timer.h"
#include <Arduino.h>
#include <WString.h>
namespace duckutils {

extern volatile bool enableDuckInterrupt;
extern Timer<> duckTimer;

/**
 * @brief Create a uuid string.
 *
 * @param length the length of the UUID (defaults to CDPCFG_UUID_LEN)
 * @returns A string representing a unique id.
 */
String createUuid(int length = CDPCFG_UUID_LEN);

/**
 * @brief Convert a byte array into a hex string.
 * 
 * @param data a byte array to convert
 * @param size the size of the array
 * @returns A string representating the by array in hexadecimal.
 */
String convertToHex(byte* data, int size);

/**
 * @brief Get the Duck Interrupt state
 * 
 * @returns true if interrupt is enabled, false otherwise.
 */
volatile bool getDuckInterrupt();

/**
 * @brief Toggle the duck Interrupt
 * 
 * @param interrupt true to enable interrupt, false otherwise.
 */
void setDuckInterrupt(bool interrupt);

Timer<> getTimer();

} // namespace duckutils
#endif