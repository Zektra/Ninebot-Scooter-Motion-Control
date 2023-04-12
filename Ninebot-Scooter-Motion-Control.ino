#include <Arduino.h>
#include <SoftwareSerial.h>

// OTA imports
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// OTA constants
const char* ssid = "SSID";
const char* password = "PASSWORD";

// DAC set-up
#include <Adafruit_MCP4725.h>
Adafruit_MCP4725 dac;
#define DAC_RESOLUTION (12)


// +-============================================================================-+
// |-================================ SETTINGS ==================================-|
// +-============================================================================-+

//Defines how much km/u is detected as a kick.
const int speedBump = 3;

//Defines what the minimal increasement every kick should make.
const int minimumSpeedIncreasment = 6;

//From which speed should the minimumSpeedIncreasment be activated.
const int enforceMinimumSpeedIncreasmentFrom = 16;

//Defines how much percent the speedBump should go down per km in speed. (it's harder to speed up when driving 20km/u.)
const float lowerSpeedBump = 0.9839;

//After how many X amount of km/u dropped, should the remembered speed to catch up to be forgotten.
const int forgetSpeed = 10;

//Define the base throttle. between 0 and baseThrottle.
const int baseThrottle = 600;
const int maxThrottle = 3500;

//Additional speed to be added on top of the requested speed. Leave at 4 for now.
const int additionalSpeed = 0;

//Minimum and maximum speed of your scooter. (You can currently use this for just one mode)
const int minimumSpeed = 7;
const int maximumSpeed = 25;

//Minimum speed before throtteling
const int startThrottle = 5;

//Amount of kicks it takes to switch over the INCREASING state.
const int kicksBeforeIncreasment = 1;

//Don't touch unless you know what you are doing.
const int brakeTriggered = 45;

//Defines how long one kick should take.
const int drivingTime = 8000;

//Defines how much time the amount of kicks before increasment can take up.
const int kickResetTime = 2000;

//Defines how much time should be in between two kicks.
const int kickDelay = 200;

//Defines the amount of time the INCREASING state will wait for a new kick.
const int increasmentTime = 2000;

//used to calculate the average speed.
const int historySize = 20;

//TX & RX pin
SoftwareSerial SoftSerial(D2, 3); // RX, TX

//END OF SETTINGS

// +-============================================================================-+
// |-=============================== VARIABLES ==================================-|
// +-============================================================================-+
// |-========================== !!! DO NOT EDIT !!! =============================-|
// +-============================================================================-+

unsigned long currentTime = 0;
unsigned long drivingTimer = 0;
unsigned long kickResetTimer = 0;
unsigned long kickDelayTimer = 0;
unsigned long increasmentTimer = 0;
unsigned long blinkTimer = 0;
bool blink = false;
bool kickAllowed = true;

int BrakeHandle;
int Speed; //current speed
int temporarySpeed = 0;
int expectedSpeed = 0;
int kickCount = 0;

int historyTotal = 0;
int history[historySize];
int historyIndex = 0;
int averageSpeed = 0;

//motionmodes
uint8_t State = 0;
#define READYSTATE 0
#define INCREASINGSTATE 1
#define DRIVINGSTATE 2
#define BRAKINGSTATE 3
#define DRIVEOUTSTATE 4

uint8_t readBlocking() {
    while (!SoftSerial.available()) delay(1);
    return SoftSerial.read();
}

void setup() {
    // DAC
    dac.begin(0x62);
    delay(100);
    dac.setVoltage(600, false);
    pinMode(D1, OUTPUT);

    // OTA setup
    Serial.begin(115200);
    Serial.println("Starting Logging data...");
    SoftSerial.begin(115200);
    Serial.println("UART started");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type);
      })
      .onEnd([]() {
        Serial.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
      });

    ArduinoOTA.begin();

    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    // end OTA setup

    digitalWrite(D1, LOW);
    currentTime = millis();
    while(millis()-currentTime < 10000)
    {
        ArduinoOTA.handle();
        digitalWrite(D1, HIGH);
    }
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    delay(100);
    digitalWrite(D1, LOW);

    for (int i = 0; i < historySize; i++) history[i] = 0;
}

uint8_t buff[256];
void loop() {
    handleBlink();

    int w = 0;
    while (readBlocking() != 0x5A);
    if (readBlocking() != 0xA5) return;
    uint8_t len = readBlocking();
    buff[0] = len;
    if (len > 254) return;
    uint8_t addr = readBlocking();
    buff[1] = addr;
    uint16_t sum = len + addr;
    for (int i = 0; i < len + 3; i++) { uint8_t curr = readBlocking(); buff[i + 2] = curr; sum += curr; }
    uint16_t checksum = (uint16_t) readBlocking() | ((uint16_t) readBlocking() << 8);
    if (checksum != (sum ^ 0xFFFF)) return;
    //Serial.println("endHex");
    if (buff[2] == 0x20 && buff[3] == 0x65) BrakeHandle = buff[7];
    if (buff[2] == 0x21 && buff[3] == 0x64 && buff[9] != 0) Speed = buff[9];

    //Update the current index in the history and recalculate the average speed.
    historyTotal = historyTotal - history[historyIndex];
    history[historyIndex] = Speed;
    historyTotal = historyTotal + history[historyIndex];
    historyIndex = historyIndex + 1;
    if (historyIndex >= historySize) historyIndex = 0;

    //Recalculate the average speed.
    averageSpeed = historyTotal / historySize;

    motion_control();
    currentTime = millis();
    if (kickResetTimer != 0 && kickResetTimer + kickResetTime < currentTime && State == DRIVINGSTATE) resetKicks();
    if (increasmentTimer != 0 && increasmentTimer + increasmentTime < currentTime && State == INCREASINGSTATE) endIncrease();
    if (drivingTimer != 0 && drivingTimer + drivingTime < currentTime && State == DRIVINGSTATE) endDrive();
    if (kickDelayTimer == 0 || (kickDelayTimer != 0 && kickDelayTimer + kickDelay < currentTime)) {
        kickAllowed = true;
        kickDelayTimer = 0;
    } else {
        kickAllowed = false;
    }
}

void motion_control() {
    if ((Speed != 0) && (Speed < startThrottle)) {
        // If speed is under 5 km/h, stop throttle
        throttleWrite(baseThrottle); //  0% throttle
    }

    if (BrakeHandle > brakeTriggered) {
        throttleWrite(baseThrottle); //close throttle directly when brake is touched. 0% throttle
        if (State != BRAKINGSTATE) {
            State = BRAKINGSTATE;
            drivingTimer = 0; kickResetTimer = 0; increasmentTimer = 0;
            Serial.println("BRAKING ~> Handle pulled.");
        }
    }
    switch(State) {
        case READYSTATE:
            if (Speed > startThrottle) {
                if (Speed > minimumSpeed) {
                    if (averageSpeed > Speed) {
                        temporarySpeed = averageSpeed;
                    } else {
                        temporarySpeed = Speed;
                    }
                } else {
                    temporarySpeed = minimumSpeed;
                }
                throttleSpeed(temporarySpeed);
                State = INCREASINGSTATE;
                Serial.println("INCREASING ~> The speed has exceeded the minimum throttle speed.");
            }

            break;
        case INCREASINGSTATE:
            if (Speed >= temporarySpeed + calculateSpeedBump(temporarySpeed) && kickAllowed) {
                kickCount++;
                increasmentTimer = 0;
                kickDelayTimer = currentTime;
            } else if (averageSpeed >= Speed && kickCount < 1 && increasmentTimer == 0) {
                increasmentTimer = currentTime;
            }

            if (kickCount >= 1) {
                if (Speed > temporarySpeed + calculateMinimumSpeedIncreasment(Speed)) {
                    temporarySpeed = validateSpeed(Speed);
                } else {
                    temporarySpeed = validateSpeed(temporarySpeed + calculateMinimumSpeedIncreasment(Speed));
                }

                throttleSpeed(temporarySpeed);
            }

            kickCount = 0;
            break;
        case DRIVINGSTATE:
            if (Speed >= expectedSpeed + calculateSpeedBump(expectedSpeed) && kickAllowed) {
                kickCount++;
                drivingTimer = currentTime + increasmentTime;
                kickResetTimer = currentTime;
                kickDelayTimer = currentTime;
            }

            if (kickCount >= kicksBeforeIncreasment) {
                if (Speed > expectedSpeed + calculateMinimumSpeedIncreasment(Speed)) {
                    temporarySpeed = validateSpeed(Speed);
                } else {
                    temporarySpeed = validateSpeed(expectedSpeed + calculateMinimumSpeedIncreasment(Speed));
                }

                throttleSpeed(temporarySpeed);
                State = INCREASINGSTATE;
                drivingTimer = 0; kickResetTimer = 0;
                Serial.println("INCREASING ~> The least amount of kicks before increasment has been reached.");
            }

            break;
        case BRAKINGSTATE:
        case DRIVEOUTSTATE:
            if (BrakeHandle > brakeTriggered) break;
            if (State == DRIVEOUTSTATE && Speed + forgetSpeed <= expectedSpeed) {
                Serial.println("DRIVEOUT ~> Speed has dropped too far under expectedSpeed. Dumping expected speed.");
                expectedSpeed = 0;
            }


            if (Speed < startThrottle) {
                State = READYSTATE;
                Serial.println("READY ~> Speed has dropped under the minimum throttle speed.");
            } else if (Speed >= averageSpeed + calculateSpeedBump(averageSpeed)) {
                if (State == DRIVEOUTSTATE) {
                    if (Speed > averageSpeed + calculateMinimumSpeedIncreasment(averageSpeed)) {
                        temporarySpeed = validateSpeed(Speed);
                    } else {
                        if (Speed > expectedSpeed) {
                            temporarySpeed = validateSpeed(averageSpeed + calculateMinimumSpeedIncreasment(averageSpeed));
                        } else {
                            temporarySpeed = validateSpeed(expectedSpeed);
                        }
                    }
                } else {
                    temporarySpeed = validateSpeed(Speed);
                }

                kickDelayTimer = currentTime;
                throttleSpeed(temporarySpeed);
                State = INCREASINGSTATE;
                Serial.println("INCREASING ~> Break released and speed increased.");
            }

            break;
        default:
            throttleWrite(baseThrottle);
            State = BRAKINGSTATE;
            drivingTimer = 0; kickResetTimer = 0; increasmentTimer = 0;
            Serial.println("BRAKING ~> Unknown state detected");
    }

}

void resetKicks() {
    kickResetTimer = 0;
    kickCount = 0;
}

void endIncrease() {
    expectedSpeed = temporarySpeed;
    kickCount = 0;
    State = DRIVINGSTATE;
    drivingTimer = currentTime;
    increasmentTimer = 0; kickResetTimer = 0;
    Serial.println("DRIVING ~> The speed has been stabilized.");
}

void endDrive() {
    drivingTimer = 0; kickResetTimer = 0;
    throttleWrite(baseThrottle);
    State = DRIVEOUTSTATE;
    Serial.println("READY ~> Speed has dropped under the minimum throttle speed.");
}

int calculateSpeedBump(int requestedSpeed) {
  return round(speedBump * pow(lowerSpeedBump, requestedSpeed));
}

int calculateMinimumSpeedIncreasment(int requestedSpeed) {
    return (requestedSpeed > enforceMinimumSpeedIncreasmentFrom ? minimumSpeedIncreasment : 0);
}

int validateSpeed(int requestedSpeed) {
    if (requestedSpeed < minimumSpeed) {
        return minimumSpeed;
    } else if (requestedSpeed > maximumSpeed) {
        return maximumSpeed;
    } else {
        return requestedSpeed;
    }
}

void throttleSpeed(int requestedSpeed) {
    if (requestedSpeed == 0) {
        throttleWrite(baseThrottle);
    } else if (requestedSpeed == maximumSpeed) {
        throttleWrite(maxThrottle);
    } else {
        int throttleRange = maxThrottle - baseThrottle;
        float calculatedThrottle = baseThrottle + (((requestedSpeed + additionalSpeed) * throttleRange) / (float) maximumSpeed);
        Serial.println(calculatedThrottle);
        throttleWrite((int) calculatedThrottle);
    }
}

void throttleWrite(int value) {
    if (value < baseThrottle) {
        dac.setVoltage(600,false);
    } else if (value > maxThrottle) {
        dac.setVoltage(3500,false);
    } else {
        dac.setVoltage(value,false);
    }
}

void handleBlink() {
    if (currentTime - blinkTimer > 500){
      blinkTimer = currentTime;
      currentTime = millis();
      blink = !blink;
      digitalWrite(D1, blink ? HIGH : LOW);
    }
}
