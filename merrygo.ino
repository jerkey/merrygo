// merrygo v0.1 by jake@spaz.org
// drives (and reports from) a Pololu #1457
// https://www.pololu.com/product/1457
#define BAUDRATE 57600 // set serial baud rate here
#define FAULT1 7 // FF1 pin from Pololu #1457
#define FAULT2 8 // FF2 pin from Pololu #1457
#define RESETMOTOR 6 // active low to reset motor controller
#define PWMHI 11 // active high to enable motor drive
#define DIRECTION 4 // HIGH =  LOW = 
#define MERRYSPEED 3 // (int1) reed switch shorting to ground (needs pullup)
#define HALLEFFECT 2 // (int0) fed through a 2N3904 from sensor (needs pullup)
#define LEDPIN 13 // this LED follows the hall effect signal by software
#define CURRENT A0 // motor current sense ADC pin
#define CURRENTZERO 512 // this value equals zero current
#define CURRENTCOEFFICIENT 13.517 // 512 = 2.5v, @ 0.066V per A, 512 / (2.5V / 0.066V) = 13.517
#define OVERSAMPLING 25 // how many times to average analogReads

volatile int hallCounter = 0; // stores the count from the hall effect sensor (relative brake position)
volatile unsigned long merryCounter = 0; // stores the count from the reed switch sensor (odometer :)
volatile unsigned long merryLastTime; // stores the last time merryCount() was called
volatile unsigned long merrySpeed; // stores the count from the hall effect sensor
bool directionFwd = true; // stores the direction we are going (for hallCounter ISR)

void setup() {
  Serial.begin(BAUDRATE);
  Serial.println("merrogo speed regulator v0.1");
  pinMode(RESETMOTOR,OUTPUT);
  digitalWrite(RESETMOTOR,HIGH); // active low to reset motor controller
  pinMode(PWMHI,OUTPUT);
  pinMode(DIRECTION,OUTPUT);
  digitalWrite(MERRYSPEED,HIGH); // enable internal pullup resistor
  digitalWrite(HALLEFFECT,HIGH); // enable internal pullup resistor
  attachInterrupt(1, merryCount, FALLING); // call merryCount() on falling edge
  attachInterrupt(0, hallCount, CHANGE); // call hallCount() on state change
}

void loop() {
  delay(100); // keep from jamming the serial port
  Serial.print("hallCounter = ");
  Serial.print(hallCounter);
  Serial.print("   merrySpeed = ");
  Serial.print(merrySpeed);
  Serial.print("   merryCounter = ");
  Serial.println(merryCounter);
  if (faultNum()) {
    Serial.print("FAULT!  ");
    Serial.println(faultMessage());
  }
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    Serial.println(inByte); // print the char you hit
    if (inByte == 'f') { // forward
      directionFwd = true; // set direction
      runMotor(1000); // run motor for 1000 milliseconds
    } else
    if (inByte == 'r') { // reverse
      directionFwd = false; // set direction
      runMotor(1000); // run motor for 1000 milliseconds
    } else
    if (inByte == 'F') { // FORWARD
      directionFwd = true; // set direction
      runMotor(10000); // run motor for 10 seconds
    } else
    if (inByte == 'R') { // REVERSE
      directionFwd = false; // set direction
      runMotor(10000); // run motor for 10 seconds
    } else {
      Serial.println("commands are [f]orward 1 second, [r]everse 1 second, [F]ORWARD 10 sec, [R]EVERSE 10 sec");
      while (Serial.available() > 0) inByte = Serial.read(); // empty the serial buffer
    }
  }
}

void merryCount() {
  merryCounter++; // tick the odometer
  merrySpeed = millis() - merryLastTime; // store the time since last tick
  merryLastTime = millis(); // update the timestamp
}

void hallCount() {
  if (directionFwd) { // we know the direction because we're turning it
    hallCounter++;
  } else {
    hallCounter--;
  }
  digitalWrite(LEDPIN,digitalRead(HALLEFFECT)); // make LED follow hall effect input pin
}

void runMotor(unsigned long timeToRun) {
  unsigned long motorStart = millis(); // time when we started running the motor
  digitalWrite(DIRECTION,directionFwd); // change this if the hardware goes backwards
  digitalWrite(PWMHI,HIGH); // turn motor on full power (you could do analogWrite() to PWM it)
  while (millis() - motorStart < timeToRun) {
    Serial.print("current = ");
    Serial.println(getCurrent(),1);
    delay(10);
  }
  digitalWrite(PWMHI,LOW); // turn off the motor, we're done
}

byte faultNum() {
  byte _fault = 0;
  if (digitalRead(FAULT1)) _fault += 1;
  if (digitalRead(FAULT2)) _fault += 2;
  return _fault;
}

String faultMessage() {
  switch (faultNum()) {
    case 0:
      return "No fault";
    case 1:
      return "Over temperature";
    case 2:
      return "Short circuit";
    case 3:
      return "Under voltage";
  }
}

float getCurrent() {
  int adder = 0;
  for (int i = 0; i < OVERSAMPLING; i++) {
    adder += analogRead(CURRENT) - CURRENTZERO; // sign corresponds to current direction
  }
  float _current = ((float)adder / (float)OVERSAMPLING) / CURRENTCOEFFICIENT;
  return _current; // don't try return with a long equation, it's flaky
}
