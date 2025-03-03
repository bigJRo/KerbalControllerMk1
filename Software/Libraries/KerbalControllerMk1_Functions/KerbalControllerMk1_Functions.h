#ifndef KERBALCONTROLLERMK1_FUNCTIONS_H
#define KERBALCONTROLLERMK1_FUNCTIONS_H

#include <Arduino.h>

/***************************************************************************************
   Register Bit Support Functions
****************************************************************************************/
bool buttonPressed(uint16_t prevButton, uint16_t newButton, uint16_t mask);
bool buttonReleased(uint16_t prevButton, uint16_t newButton, uint16_t mask);
uint16_t toggleBit(uint16_t registerInput, uint16_t mask);
uint8_t toggleBit(uint8_t registerInput, uint8_t mask);
uint16_t setBit(uint16_t registerInput, uint16_t mask);
uint8_t setBit(uint8_t registerInput, uint8_t mask);
uint16_t clearBit(uint16_t registerInput, uint16_t mask);
uint8_t clearBit(uint8_t registerInput, uint8_t mask);
bool isBitEnabled(uint16_t registerInput, uint16_t mask);
bool isBitEnabled(uint8_t registerInput, uint8_t mask);


/***************************************************************************************
I2C Support Functions
****************************************************************************************/
float receiveFloat();
int16_t receiveInt16();
int32_t receiveInt32();
void transmitFloat(float inputFloat);
void transmitInt16(int16_t inputInt);
void transmitInt32(int32_t inputInt);


#endif
