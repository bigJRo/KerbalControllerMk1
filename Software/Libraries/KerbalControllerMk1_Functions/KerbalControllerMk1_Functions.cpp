#include "KerbalControllerMk1_Function.h"

/**********************************************************************************************************************************
Register Bit Support Functions
**********************************************************************************************************************************/
/***************************************************************************************
   BUTTON PRESSED CHECK
   Function to check whether a button (already debounced) has transitioned from off
   (returned 0) to on (returned 1) indicating user has pushed it down
   - INPUTS:
    - {uint16_t} prevButton = 16 bit register of previous button conditions
    - {uint16_t} newButton = 16 bit register of updated button conditions
    - {uint16_t} mask = mask to use for the compare; ensure coorect function
      definition is being operated on
   - Returns bool
****************************************************************************************/
bool buttonPressed(uint16_t prevButton, uint16_t newButton, uint16_t mask) {
  //logic check that the toggle has been activated but that the previous state was 0
  if (newButton & mask && ~prevButton & mask) {
    return true;
  } else {
    return false;
  }
}

/***************************************************************************************
   BUTTON RELEASED CHECK
   Function to check whether a button (already debounced) has transitioned from on
   (returned 1) to off (returned 0) indicating user has let it go
   - INPUTS:
    - {uint16_t} prevButton = 16 bit register of previous button conditions
    - {uint16_t} newButton = 16 bit register of updated button conditions
    - {uint16_t} mask = mask to use for the compare; ensure coorect function
      definition is being operated on
   - Returns bool
****************************************************************************************/
bool buttonReleased(uint16_t prevButton, uint16_t newButton, uint16_t mask) {
  //logic check that the toggle has been activated but that the previous state was 1
  if (~newButton & mask && prevButton & mask) {
    return true;
  } else {
    return false;
  }
}

/***************************************************************************************
   TOGGLE REGISTER BIT
   Function to change bit state in declared register. Uses logical XOR operator
   - INPUTS:
    - {uint16_t} registerInput = value of register that needs to be operated on
    - {uint16_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - OUTPUTS:
    - {uint16_t} registerOutput = resultant register with modified bit
****************************************************************************************/
uint16_t toggleBit(uint16_t registerInput, uint16_t mask) {
  uint16_t registerOutput;
  registerOutput = registerInput ^ mask;

  return registerOutput;
}


/***************************************************************************************
   TOGGLE REGISTER BIT
   Function to change bit state in declared register. Uses logical XOR operator
   - INPUTS:
    - {uint8_t} registerInput = value of register that needs to be operated on
    - {uint8_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - OUTPUTS:
    - {uint8_t} registerOutput = resultant register with modified bit
****************************************************************************************/
uint8_t toggleBit(uint8_t registerInput, uint8_t mask) {
  uint8_t registerOutput;
  registerOutput = registerInput ^ mask;

  return registerOutput;
}

/***************************************************************************************
   SET REGISTER BIT
   Function to change bit state to 1 in declared register. Uses logical OR operator
   - INPUTS:
    - {uint16_t} registerInput = value of register that needs to be operated on
    - {uint16_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - OUTPUTS:
    - {uint16_t} registerOutput = resultant register with modified bit
****************************************************************************************/
uint16_t setBit(uint16_t registerInput, uint16_t mask) {
  uint16_t registerOutput;
  registerOutput = registerInput | mask;

  return registerOutput;
}

/***************************************************************************************
   SET REGISTER BIT
   Function to change bit state to 1 in declared register. Uses logical OR operator
   - INPUTS:
    - {uint8_t} registerInput = value of register that needs to be operated on
    - {uint8_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - OUTPUTS:
    - {uint8_t} registerOutput = resultant register with modified bit
****************************************************************************************/
uint8_t setBit(uint8_t registerInput, uint8_t mask) {
  uint8_t registerOutput;
  registerOutput = registerInput | mask;

  return registerOutput;
}

/***************************************************************************************
   CLEAR REGISTER BIT
   Function to change bit state to 0 in declared register. Uses logical AND operator
   - INPUTS:
    - {uint16_t} registerInput = value of register that needs to be operated on
    - {uint16_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - OUTPUTS:
    - {uint16_t} registerOutput = resultant register with modified bit
****************************************************************************************/
uint16_t clearBit(uint16_t registerInput, uint16_t mask) {
  uint16_t registerOutput;
  registerOutput = registerInput & ~mask;

  return registerOutput;
}

/***************************************************************************************
   CLEAR REGISTER BIT
   Function to change bit state to 0 in declared register. Uses logical AND operator
   - INPUTS:
    - {uint8_t} registerInput = value of register that needs to be operated on
    - {uint8_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - OUTPUTS:
    - {uint8_t} registerOutput = resultant register with modified bit
****************************************************************************************/
uint8_t clearBit(uint8_t registerInput, uint8_t mask) {
  uint8_t registerOutput;
  registerOutput = registerInput & ~mask;

  return registerOutput;
}

/***************************************************************************************
   IS A BIT ENABLED CHECK
   Function to check whether a bit in the referenced register is set
   - INPUTS:
    - {uint16_t} registerInput = value of register that needs to be operated on
    - {uint16_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - Returns bool
****************************************************************************************/
bool isBitEnabled(uint16_t registerInput, uint16_t mask) {
  if (registerInput & mask) {
    return true;
  } else {
    return false;
  }
}

/***************************************************************************************
   IS A BIT ENABLED CHECK
   Function to check whether a bit in the referenced register is set
   - INPUTS:
    - {uint8_t} registerInput = value of register that needs to be operated on
    - {uint8_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - Returns bool
****************************************************************************************/
bool isBitEnabled(uint8_t registerInput, uint8_t mask) {
  if (registerInput & mask) {
    return true;
  } else {
    return false;
  }
}


/**********************************************************************************************************************************
I2C Support Functions
**********************************************************************************************************************************/\
/***************************************************************************************
   RECEIVE 16-BIT INTEGER
   Read in from wire read 2 bytes to form a single output integer value
   See following site for explanation of technique:
    https://medium.com/@sandhan.sarma/sending-floats-over-the-i2c-bus-between-two-arduinos-part-2-486db6dc479f
   - No inputs
   - Returns 16-bit integer
****************************************************************************************/
int16_t receiveInt16() {
  int16_t index = 0;
  union intToBytes {

    char buffer[2];
    int16_t returnValue;

  } converter;

  while (index < 2) {
    converter.buffer[index] = Wire.read();
    index++;
  }

  return converter.returnValue;
}


/***************************************************************************************
   RECEIVE 32-BIT INTEGER
   Read in from wire read 4 bytes to form a single output long
   See following site for explanation of technique:
    https://medium.com/@sandhan.sarma/sending-floats-over-the-i2c-bus-between-two-arduinos-part-2-486db6dc479f
   - No inputs
   - Returns 16-bit integer
****************************************************************************************/
int32_t receiveInt32() {
  int16_t index = 0;
  union intToBytes {

    char buffer[4];
    int32_t returnValue;

  } converter;

  while (index < 4) {
    converter.buffer[index] = Wire.read();
    index++;
  }

  return converter.returnValue;
}


/***************************************************************************************
   RECEIVE FLOAT
   Read in from wire read 4 bytes to form a single output float value
   See following site for explanation of technique:
    https://medium.com/@sandhan.sarma/sending-floats-over-the-i2c-bus-between-two-arduinos-part-2-486db6dc479f
   - No inputs
   - Returns float
****************************************************************************************/
float receiveFloat() {
  uint16_t index = 0;
  union floatToBytes {

    char buffer[4];
    float returnValue;

  } converter;

  while (index < 4) {
    converter.buffer[index] = Wire.read();
    index++;
  }

  return converter.returnValue;
}


/***************************************************************************************
   TRANSMIT FLOAT
   Take in a float argument and and address and transmit via bytes using Wire library
   See following site for explanation of technique:
    https://medium.com/@sandhan.sarma/sending-floats-over-the-i2c-bus-between-two-arduinos-part-2-486db6dc479f
   - INPUTS:
     {float} inputFloat = value that needs to be transmitted
   - No returns
****************************************************************************************/
void transmitFloat(float inputFloat) {
  union floatToBytes {

    char buffer[4];
    float inputValue;

  } converter;

  converter.inputValue = inputFloat;

  Wire.write(converter.buffer, 4);
}


/***************************************************************************************
   TRANSMIT 16-BIT INTEGER
   Take in a integer argument and and address and transmit via bytes using Wire library
   See following site for explanation of technique:
    https://medium.com/@sandhan.sarma/sending-floats-over-the-i2c-bus-between-two-arduinos-part-2-486db6dc479f
   - INPUTS:
     {int16_t} inputInt = value that needs to be transmitted
   - No returns
****************************************************************************************/
void transmitInt16(int16_t inputInt) {
  union intToBytes {

    char buffer[2];
    int16_t inputValue;

  } converter;

  converter.inputValue = inputInt;

  Wire.write(converter.buffer, 2);
}


/***************************************************************************************
   TRANSMIT 32-BIT INTEGER
   Take in a integer argument and and address and transmit via bytes using Wire library
   See following site for explanation of technique:
    https://medium.com/@sandhan.sarma/sending-floats-over-the-i2c-bus-between-two-arduinos-part-2-486db6dc479f
   - INPUTS:
     {int32_t} inputInt = value that needs to be transmitted
   - No returns
****************************************************************************************/
void transmitInt32(int32_t inputInt) {
  union intToBytes {

    char buffer[4];
    int32_t inputValue;

  } converter;

  converter.inputValue = inputInt;

  Wire.write(converter.buffer, 4);
}
