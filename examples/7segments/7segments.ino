/*
  Programmer for flash memory SST39SF010
  Written by Andrea Marini in 2022

  For more information, see https://github.com/mariotorrone/SST39SF010_flash_programmer
*/

// Enables serial debug traces
#define DBG 0
#if DBG
void printBits(byte myByte) {
  // https://forum.arduino.cc/t/serial-print-n-bin-displaying-all-bits/46461/2
  for (byte mask = 0x80; mask; mask >>= 1) {
    if (mask & myByte)
      Serial.print('1');
    else
      Serial.print('0');
  }
}
#endif // DBG

// Pin definitions
#define DATA0       2
#define DATA7       9
#define ADDR_16     10      // Driven directly by Arduino
#define WRITE_EN    11      // Write to memory, negative logic
#define OUT_EN      12      // Read from memory, negative logic
#define ADDR_SER    A0
#define CLK         A1
#define LATCH       A2      // Basically the 595 output enable

// Enables and configures write and clear check
#define WRITE_CHK   1
#define CLR_CHK     5
#define CLR_CHK_MAX 131071  // (2^17)-1

// Pins initialization
void initPins() {
  // Address pins
  pinMode(ADDR_SER, OUTPUT);
  pinMode(ADDR_16, OUTPUT);
  digitalWrite(ADDR_SER, LOW);
  digitalWrite(ADDR_16, LOW);

  // Pins to drive 74HC595
  // Setup the clock pin low as per Arduino reference
  digitalWrite(CLK, LOW);
  pinMode(CLK, OUTPUT);
  digitalWrite(LATCH, LOW);
  pinMode(LATCH, OUTPUT);

  // Pins to drive flash r/w
  digitalWrite(WRITE_EN, HIGH);
  pinMode(WRITE_EN, OUTPUT);
  digitalWrite(OUT_EN, HIGH);
  pinMode(OUT_EN, OUTPUT);
}

// Sets the 8 data pins as output if argument is 1, as inputs if 0
void setDataPinsIO(int output) {
  for (int pin = DATA0; pin <= DATA7; pin++) {
    digitalWrite(pin, HIGH);
    if (output)
      pinMode(pin, OUTPUT);
    else
      pinMode(pin, INPUT);
  }
}

// Sets the address pins
void setAddress(unsigned long addr) {
  // Convert the long in two bytes that we'll send to the 595,
  // plus one bit that we'll drive directy from the Arduino,
  // since the SST39SF010 has 17 bit for addressing memory.
  byte byte0 = (addr & 0x000FF);       // LSB
  byte byte1 = (addr & 0x0FF00) >> 8;
  byte byte2 = (addr & 0x10000) >> 16; // MSB

#if DBG // Some debug traces that can be useful to electrically debug the circuit
  char buf[80];
  sprintf(buf, "Address %05lx with bits ", addr);
  Serial.print(buf);
  printBits(byte2);
  Serial.print(" ");
  printBits(byte1);
  Serial.print(" ");
  printBits(byte0);
  Serial.println();
#endif // DBG

  // Write the address bits
  digitalWrite(ADDR_16, byte2 & 1);
  shiftOut(ADDR_SER, CLK, LSBFIRST, byte1);
  shiftOut(ADDR_SER, CLK, LSBFIRST, byte0);
  // Pulse on the latch to output the value
  digitalWrite(LATCH, LOW);
  digitalWrite(LATCH, HIGH);
  digitalWrite(LATCH, LOW);
  // Reset serial to high, otherwise it will pull down the outputs of 595 (not sure why tho)
  digitalWrite(ADDR_SER, HIGH);
}

// Sets the data pins
void setData(byte data) {
#if DBG // Some debug traces that can be useful to electrically debug the circuit
  char buf[80];
  sprintf(buf, "Writing data %02x with bits ", data);
  Serial.print(buf);
  printBits(data);
  Serial.println();
#endif // DBG

  setDataPinsIO(1);
  for (int pin = DATA0; pin <= DATA7; pin++) {
    digitalWrite(pin, data & 1);
    data = data >> 1;
  }
}

// Sends a write pulse to flash
void writePulse() {
  digitalWrite(WRITE_EN, HIGH);
  delayMicroseconds(1);
  digitalWrite(WRITE_EN, LOW);
  delayMicroseconds(1);
  digitalWrite(WRITE_EN, HIGH);
}

// Writes the byte data to memory at the specified address
bool writeMem(unsigned long addr, byte data) {
  setDataPinsIO(1);

  // Software data protection procedure, datasheet page 10
  setAddress(0x5555);
  setData(0xAA);
  writePulse();
  setAddress(0x2AAA);
  setData(0x55);
  writePulse();
  setAddress(0x5555);
  setData(0xA0);
  writePulse();

  // Write actual data
  setAddress(addr);
  setData(data);
  writePulse();

#if WRITE_CHK
  // Read back the data just written and compare
  delay(1);
  byte readData = readMem(addr);
  if (readData != data) {
    char buf[80];
    sprintf(buf, "ERROR! Data written at %05lx was corrupted: was ", addr);
    Serial.print(buf);
    sprintf(buf, "%02x instead of %02x", readData, data);
    Serial.println(buf);
    return false;
  }
#endif // WRITE_CHK
  return true;
}

// Returns the byte written in memory at the specified address
byte readMem(unsigned long addr) {
  setAddress(addr);
  setDataPinsIO(0);

  byte data = 0;
  digitalWrite(OUT_EN, LOW);
  for (int pin = DATA7; pin >= DATA0; pin--)
    data = (data << 1) + digitalRead(pin);
  digitalWrite(OUT_EN, HIGH);

#if DBG // Some debug traces that can be useful to electrically debug the circuit
  char buf[80];
  sprintf(buf, "Reading data %02hhx with bits ", data);
  Serial.print(buf);
  printBits(data);
  Serial.println();
#endif // DBG

  return data;
}

// Erase all the contents of the flash, defaults to all 1s
bool eraseAll() {
  // Chip erase procedure, datasheet page 10
  Serial.print("Clearing memory... ");
  setAddress(0x5555);
  setData(0xAA);
  writePulse();
  setAddress(0x2AAA);
  setData(0x55);
  writePulse();
  setAddress(0x5555);
  setData(0x80);
  writePulse();
  setAddress(0x5555);
  setData(0xAA);
  writePulse();
  setAddress(0x2AAA);
  setData(0x55);
  writePulse();
  setAddress(0x5555);
  setData(0x10);
  writePulse();

  // Chip erase takes max 100 ms by datasheet specifications
  delay(100);
  Serial.println("Done.");

#if CLR_CHK
  randomSeed(analogRead(A4));
  // Read CLR_CHK number of random addresses and checks if they are all 1s
  Serial.print("Verifying clear... ");
  for (int i = 0; i < CLR_CHK; i++) {
    unsigned long addr = random(CLR_CHK_MAX);
    byte data = readMem(addr);
    if (data != 0xFF) {
      // If the data is not all 1s, return a failure
      char buf[80];
      sprintf(buf, "ERROR! Chip erase was not successful, data at %05lx was %02hhx", addr, data);
      Serial.println(buf);
      return false;
    }
  }
  Serial.println("Success!");
#endif // CLR_CHK

  return true;
}

// Prints to serial the contents of memory between the given addresses
void dumpContent(unsigned long startAddr, unsigned long endAddr) {
  // Normalizing the addresses
  startAddr -= startAddr % 16;
  endAddr += 15 - (endAddr % 16);

  char buf[80];
  sprintf(buf, "Dumping content from %05lx ", startAddr);
  Serial.print(buf);
  sprintf(buf, "to %05lx:", endAddr);
  Serial.println(buf);

  // Serial print is grouped in 16 bytes long lines
  for (unsigned long base = startAddr; base <= endAddr; base += 16) {
    byte data[16];
    for (int offset = 0; offset < 16; offset++) {
      data[offset] = readMem(base + offset);
    }
    sprintf(buf, "0x%05lx:\t", base);
    Serial.print(buf);
    sprintf(buf, "%02x %02x %02x %02x %02x %02x %02x %02x\t\t%02x %02x %02x %02x %02x %02x %02x %02x",
            data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
            data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);
    Serial.println(buf);
  }
}

void setup() {
  initPins();
  Serial.begin(57600);

  if (!eraseAll())
    while(1);

  /*
  This code is made to program 4 7-seg displays to write in four different modes.
  Thanks to https://github.com/DerULF1/8bit-computer for the inspiration!
  */

  // Digits from 0 to f, with bits as abcdefgh
  byte digits[] = {0xfc, 0x60, 0xda, 0xf2, 0x66,         // 0-4
                   0xb6, 0xbe, 0xe0, 0xfe, 0xf6,         // 5-9
                   0xee, 0x3e, 0x9c, 0x7a, 0x9e , 0x8e}; // a-f
  byte binaryDigits[] = {0x28, 0x68,  // 0 = two short bar, 1 = short + long bar
                        0x2c, 0x6c}; // 2 = long + short bar, 3 = two long bar

  /*
  Addresses are done as follows:
  A0-A7:   abcdefgh LEDs of 7seg
  A8-A9:   7seg displays 0 through 3
  A10-A11: display modes in order: positive decimal, 2s complement,
           positive hex, binary
  */

  // Positive decimal
  Serial.println("Positive decimal");
  Serial.println("Programming ones place");
  for (int value = 0; value <= 255; value ++)
    writeMem(value, digits[value % 10]);
  Serial.println("Programming tens place");
  for (int value = 0; value <= 255; value ++)
    writeMem(value + 0x100, digits[(value / 10) % 10]);
  Serial.println("Programming hundreds place");
  for (int value = 0; value <= 255; value ++)
    writeMem(value + 0x200, digits[(value / 100) % 10]);
  Serial.println("Programming sign");
  for (int value = 0; value <= 255; value ++)
    writeMem(value + 0x300, 0x00);

  // 2s complement
  Serial.println("2s complement");
  Serial.println("Programming ones place");
  for (int value = -128; value <= 127; value ++)
    writeMem((byte) value + 0x400, digits[abs(value) % 10]);
  Serial.println("Programming tens place");
  for (int value = -128; value <= 127; value ++)
    writeMem((byte) value + 0x500, digits[(abs(value) / 10) % 10]);
  Serial.println("Programming hundreds place");
  for (int value = -128; value <= 127; value ++)
    writeMem((byte) value + 0x600, digits[(abs(value) / 100) % 10]);
  Serial.println("Programming sign");
  for (int value = -128; value <= 127; value ++) {
    if (value < 0)
      writeMem((byte) value + 0x700, 0x02);
    else
      writeMem((byte) value + 0x700, 0x00);
  }

  // Positive hex
  Serial.println("Positive hex");
  Serial.println("Programming ones place");
  for (int value = 0; value <= 255; value ++)
    writeMem(value + 0x800, digits[value % 16]);
  Serial.println("Programming tens place");
  for (int value = 0; value <= 255; value ++)
    writeMem(value + 0x900, digits[(value / 0xf) % 16]);
  Serial.println("Programming hundreds place");
  for (int value = 0; value <= 255; value ++)
    writeMem(value + 0xa00, digits[(value / 0xff) % 16]);
  Serial.println("Programming sign");
  for (int value = 0; value <= 255; value ++)
    writeMem(value + 0xb00, 0);

  // Binary
  Serial.println("Binary");
  Serial.println("Programming ones place");
  for (int value = 0; value <= 255; value ++)
    writeMem(value + 0xc00, binaryDigits[value % 4]);
  Serial.println("Programming tens place");
  for (int value = 0; value <= 255; value ++)
    writeMem(value + 0xd00, binaryDigits[(value / 4) % 4]);
  Serial.println("Programming hundreds place");
  for (int value = 0; value <= 255; value ++)
    writeMem(value + 0xe00, binaryDigits[(value / 16) % 4]);
  Serial.println("Programming thousands place");
  for (int value = 0; value <= 255; value ++)
    writeMem(value + 0xf00, binaryDigits[(value / 64) % 4]);

  dumpContent(0, 0xf00);

  // Put your code here
}

void loop() { /* Not used */ }
