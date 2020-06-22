/*
  Toaster Hack (servo controlled)
  This sketch is part of a project to hack a toaster oven and use it as a reflow oven for assembling electronics.
  It does require some tweaking to work with any toaster oven, and so please use at your own risk.
  This project uses a hobby servo to control the movement of the temperature knob on the toaster oven.
  It also uses a thermocouple to sense the temperature inside the oven.
  It also provides streaming serial data, so you can watch your profile using the Arduino Serial Plotter.

  See the complete blog post about this project here:
  https://www.sparkfun.com/news/3319

  By: Pete Lewis
  SparkFun Electronics
  Date: June 22nd, 2020
  License: This code is public domain but you can buy me a beer if you use this and we meet someday (Beerware license).

  Feel like supporting our work? Please buy a board from SparkFun!
  https://www.sparkfun.com/products/16295

  Some of this code is a modified version of the example provided by the SparkFun MCP9600
  Arduino Library which can be found here:
  https://github.com/sparkfun/SparkFun_MCP9600_Arduino_Library

  Some of the code is a modified version of the examples provided by the Arduino Servo Library which can be found here:
  https://www.arduino.cc/en/reference/servo

*/


#include <SparkFun_MCP9600.h>
MCP9600 tempSensor;

#include <Servo.h>
Servo myServo;
int pos = 0;
int userInput = 0;
int ovenSetting = 0;

// Note, these two following arrays are only used as a reference of a good starting point. 
// These arrays are drawn on the serial plotter as real-readings are also plotted.
// These are NOT used to actually adjust the servo.
int profileTimeStamp[] = {0,   90,   180,   225,   255,   256}; // for reference only
int profileTemp[] =      {0,   150,  180,   245,   245,   0}; // for reference only

byte servoTimeStamp[] = {};
byte servoPos[] = {};

byte soakSeconds = 0; // active soak time counter
#define SOAK_TIME 90
byte soakTemp = 150;
boolean soakState = false; // used to toggle on/off during soak

byte reflowSeconds = 0; // active reflow time counter
byte reflowTemp = 245;

byte zone = 0;
#define ZONE_preheat 0
#define ZONE_soak 1
#define ZONE_ramp 2
#define ZONE_reflow 3
#define ZONE_cool 4



/*
   the plan
   full power
   wait to see 140, kill power, enter soak time
   toggle power on/off to try to climb 150 to 180
   once soak is over, turn on full power
   wait to see reflow temp (220C)
   wait 35 seconds, turn servo to off, indicating user should open door.

*/

int totalTime = 0;
float currentTargetTemp;
float currentTemp;

#define COOL_DOWN_SECONDS 240 // monitor actual cool down temps for 4 mintues (240 sec), then stop graph

void setup() {
  Serial.begin(115200);
  labels(); // send some label names for serial plotter to look nice
  Wire.begin();
  Wire.setClock(100000);
  tempSensor.begin();       // Uses the default address (0x60) for SparkFun Thermocouple Amplifier
  //tempSensor.begin(0x66); // Default address (0x66) for SparkX Thermocouple Amplifier

  //check if the sensor is connected
  if (tempSensor.isConnected()) {
    //Serial.println("Device will acknowledge!");
  }
  else {
    Serial.println("Device did not acknowledge! Freezing.");
    while (1); //hang forever
  }

  //check if the Device ID is correct
  if (tempSensor.checkDeviceID()) {
    //Serial.println("Device ID is correct!");
  }
  else {
    Serial.println("Device ID is not correct! Freezing.");
    while (1);
  }

  myServo.attach(8);
  myServo.write(90);
  //delay(1000);
  pinMode(A0, INPUT);

}

void loop() {

  for (int i = 0 ; i <= 4 ; i++) // cycle through all zones - 0 through 4.
  {
    int tempDelta = abs(profileTemp[i + 1] - profileTemp[i]);

    int timeDelta = abs(profileTimeStamp[i + 1] - profileTimeStamp[i]);

    float rampIncrementVal = float(tempDelta) / float(timeDelta);

    currentTargetTemp = profileTemp[i];

    //    delay(100);
    //    Serial.print("tempDelta:");
    //    Serial.print(tempDelta);
    //    delay(100);
    //    Serial.print("\t rampIncrementVal:");
    //    Serial.print(rampIncrementVal);
    //    delay(100);
    //    Serial.print("\t currentTargetTemp:");
    //    Serial.print(currentTargetTemp);
    //    delay(100);

    // Print data to serial for good plotter view in real time.
    // Print target (for visual reference only), currentTemp, ovenSetting (aka servo pos), zone
    for (int j = 0 ; j < (timeDelta - 1); j++)
    {
      currentTargetTemp += rampIncrementVal;
      Serial.print(currentTargetTemp, 2);
      Serial.print(",");

      if (tempSensor.available())
      {
        currentTemp = tempSensor.getThermocoupleTemp();
      }
      else {
        currentTemp = 0; // error
      }

      Serial.print(currentTemp);
      Serial.print(",");

      updateServoPos(); // Note, this will increment the zone when specific times or temps are reached.

      Serial.print(ovenSetting);
      Serial.print(",");

      Serial.println(zone * 20);
      delay(1000);
      totalTime++;
    }
  }

  // monitor actual temps during cooldown
  for (int i = 0 ; i < COOL_DOWN_SECONDS ; i++)
  {
    currentTargetTemp = 0;
    Serial.print(currentTargetTemp, 2);
    Serial.print(",");
    if (tempSensor.available())
    {
      currentTemp = tempSensor.getThermocoupleTemp();
    }
    else {
      currentTemp = 0; // error
    }
    Serial.print(currentTemp);
    Serial.print(",");
    updateServoPos();
    Serial.print(ovenSetting);
    Serial.print(",");
    Serial.println(zone * 20);
    delay(1000);
  }
  while (1); // end
}

void updateServoPos()
{
  //  Uncomment the following to control manually with a trimpot on A0.
  //  This is useful when first setting up a new oven/servo mount.
  //  userInput = analogRead(A0);
  //  pos = map(userInput, 0, 1023, 20, 180);
  //  myServo.write(pos);
  //  ovenSetting = map(pos, 20, 180, 0, 100); // as a % of position, "100C-450C" on the dial labels

  if ((zone == ZONE_preheat) && (currentTemp > 140)) // done with preheat, move onto soak
  {
    zone = ZONE_soak;
  }
  else if ((zone == ZONE_soak) && (soakSeconds > SOAK_TIME)) // done with soak, move onto ramp
  {
    zone = ZONE_ramp;
  }
  else if ((zone == ZONE_ramp) && (currentTemp > 215)) // done with ramp move onto reflow
  {
    zone = ZONE_reflow;
  }
  else if ((zone == ZONE_reflow) && (reflowSeconds > 30))
  {
    zone = ZONE_cool;
  }


  switch (zone) {
    case ZONE_preheat:
      setServo(41);
      break;
    case ZONE_soak:
      soakSeconds++;
      if ((soakSeconds % 15) == 0) // every 15 seconds toggle
      {
        soakState = !soakState;
      }
      if (soakState)
      {
        setServo(100);
      }
      else {
        setServo(0);
      }
      break;
    case ZONE_ramp:
      setServo(100);
      break;
    case ZONE_reflow:
      reflowSeconds++;
      if ((reflowSeconds > 5) && (reflowSeconds < 10)) // from 5-10 seconds drop to off
      {
        setServo(0);
      }
      else {
        setServo(100);
      }
      break;
    case ZONE_cool:
      setServo(0);
      break;
  }
}

void setServo(int percentage)
{
  ovenSetting = percentage;
  int toWrite = map(percentage, 0, 100, 20, 180);
  myServo.write(toWrite);
}

void labels()
{
  delay(100);
  Serial.println("target temp setting zone");
}
