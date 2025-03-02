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
   RECEIVE 16-BIT INTEGER
   Read in from wire read 2 bytes to form a single output long
   See following site for explanation of technique:
    https://medium.com/@sandhan.sarma/sending-floats-over-the-i2c-bus-between-two-arduinos-part-2-486db6dc479f
   - No inputs
   - Returns 16-bit integer
****************************************************************************************/
int32_t receiveInt16() {
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
   RECEIVE 32-BIT INTEGER
   Read in from wire read 4 bytes to form a single output long
   See following site for explanation of technique:
    https://medium.com/@sandhan.sarma/sending-floats-over-the-i2c-bus-between-two-arduinos-part-2-486db6dc479f
   - No inputs
   - Returns 32-bit integer
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
