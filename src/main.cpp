#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>       // https://github.com/tzapu/WiFiManager
#include <LiquidCrystal_I2C.h> // https://github.com/johnrickman/LiquidCrystal_I2C
#include <Adafruit_MCP23X17.h> // https://github.com/adafruit/Adafruit-MCP23017-Arduino-Library
#include <AccelStepper.h>      // https://github.com/waspinator/AccelStepper

#define LCD_I2C_ADDRESS 0x27
#define LCD_COLUMNS 20
#define LCD_ROWS 4

#define MCP23017_INPUTS_ADDRESS 0x20
#define MCP23017_INPUTS_INT_PIN 25
#define MCP23017_RELAY_ADDRESS 0x21

#define MCP23017_SENSOR_PROBE_HOME_PIN 1 // A1
#define MCP23017_FOOD_HOPPER_COVER_PIN 2 // A2
#define MCP23017_SOIL_HOPPER_COVER_PIN 3 // A3
#define MCP23017_GATE_HOME_PIN 4         // A4
#define MCP23017_MIXER_SAFE_PIN 5        // A5

#define RELAY_SOIL_DISPENSER_POSITIVE 13 // GPB5
#define RELAY_SOIL_DISPENSER_NEGATIVE 5  // GPA5
#define RELAY_BUZZER 11                  // GPB3
#define RELAY_MEAT_GRINDER 12            // GPB4

#define SENSOR_STEPPER_MAX_SPEED 1000
#define SENSOR_STEPPER_ACCELERATION 1000
#define GATE_STEPPER_MAX_SPEED 1000
#define GATE_STEPPER_ACCELERATION 1000

#define SENSOR_STEPPER_MAX_STEPS 25000
#define GATE_STEPPER_MAX_STEPS 8000

#define SENSOR_STEPPER_PULSE_PIN 32
#define SENSOR_STEPPER_DIR_PIN 33

#define MIXER_STEPPER_PULSE_PIN 26
#define MIXER_STEPPER_DIR_PIN 27

#define GATE_STEPPER_PULSE_PIN 12
#define GATE_STEPPER_DIR_PIN 14

Adafruit_MCP23X17 mcpInputs, mcpRelay;                                                              // Create instances of Adafruit_MCP23X17 for inputs and relay
WiFiManager wifiManager;                                                                            // Create an instance of WiFiManager
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS);                                      // Create an instance of LiquidCrystal_I2C
AccelStepper sensorStepper(AccelStepper::DRIVER, SENSOR_STEPPER_PULSE_PIN, SENSOR_STEPPER_DIR_PIN); // Create an instance of AccelStepper for the sensor
AccelStepper mixerStepper(AccelStepper::DRIVER, MIXER_STEPPER_PULSE_PIN, MIXER_STEPPER_DIR_PIN);    // Create an instance of AccelStepper for the mixer
AccelStepper gateStepper(AccelStepper::DRIVER, GATE_STEPPER_PULSE_PIN, GATE_STEPPER_DIR_PIN);       // Create an instance of AccelStepper for the gate

bool isBuzzerOn = false;        // Flag to track the buzzer state
bool isSoilDispenserOn = false; // Flag to track the soil dispenser state
bool isMeatGrinderOn = false;   // Flag to track the meat grinder state

void initMcp(Adafruit_MCP23X17 &mcp, uint8_t address, const char *name)
{
  if (!mcp.begin_I2C(address))
  {
    Serial.printf("[MCP23017] Failed to initialize MCP23017 for %s at address 0x%02X!\n", name, address);
  }
  else
  {
    Serial.printf("[MCP23017] Initialized MCP23017 for %s at address 0x%02X\n", name, address);
  }
}

void setupMcp23017()
{
  initMcp(mcpInputs, MCP23017_INPUTS_ADDRESS, "inputs");
  initMcp(mcpRelay, MCP23017_RELAY_ADDRESS, "relay");

  // Configure input pins on mcpInputs
  mcpInputs.pinMode(MCP23017_SENSOR_PROBE_HOME_PIN, INPUT);
  mcpInputs.pinMode(MCP23017_FOOD_HOPPER_COVER_PIN, INPUT);
  mcpInputs.pinMode(MCP23017_SOIL_HOPPER_COVER_PIN, INPUT);
  mcpInputs.pinMode(MCP23017_GATE_HOME_PIN, INPUT);
  mcpInputs.pinMode(MCP23017_MIXER_SAFE_PIN, INPUT);

  // Configure relay pin on mcpRelay
  mcpRelay.pinMode(RELAY_SOIL_DISPENSER_POSITIVE, OUTPUT);
  mcpRelay.pinMode(RELAY_SOIL_DISPENSER_NEGATIVE, OUTPUT);
  mcpRelay.pinMode(RELAY_BUZZER, OUTPUT);
  mcpRelay.pinMode(RELAY_MEAT_GRINDER, OUTPUT);

  // Set initial states for relay pins, ensuring they are HIGH (off) to prevent accidental activation
  mcpRelay.digitalWrite(RELAY_SOIL_DISPENSER_POSITIVE, HIGH);
  mcpRelay.digitalWrite(RELAY_SOIL_DISPENSER_NEGATIVE, HIGH);
  mcpRelay.digitalWrite(RELAY_BUZZER, HIGH);
  mcpRelay.digitalWrite(RELAY_MEAT_GRINDER, HIGH);
}

void beep(int durationMs, int times = 1, int intervalMs = 100)
{
  for (int i = 0; i < times; i++)
  {
    mcpRelay.digitalWrite(RELAY_BUZZER, LOW); // Turn on the buzzer
    delay(durationMs);
    mcpRelay.digitalWrite(RELAY_BUZZER, HIGH); // Turn off the buzzer
    if (i < times - 1)
    {
      delay(intervalMs);
    }
  }
}

void turnOnSoilDispenser()
{
  if (isSoilDispenserOn)
  {
    Serial.println("[SOIL DISPENSER] Soil dispenser is already on. Ignoring the request to turn it on again.");
    return;
  }
  isSoilDispenserOn = true;
  mcpRelay.digitalWrite(RELAY_SOIL_DISPENSER_POSITIVE, LOW);
  mcpRelay.digitalWrite(RELAY_SOIL_DISPENSER_NEGATIVE, LOW);
  Serial.println("[SOIL DISPENSER] Soil dispenser turned on.\n");
}

void turnOffSoilDispenser()
{
  if (!isSoilDispenserOn)
  {
    Serial.println("[SOIL DISPENSER] Soil dispenser is already off. Ignoring the request to turn it off again.");
    return;
  }
  isSoilDispenserOn = false;
  mcpRelay.digitalWrite(RELAY_SOIL_DISPENSER_POSITIVE, HIGH);
  mcpRelay.digitalWrite(RELAY_SOIL_DISPENSER_NEGATIVE, HIGH);

  Serial.println("[SOIL DISPENSER] Soil dispenser turned off.\n");
}

void turnOnMeatGrinder()
{
  if (isMeatGrinderOn)
  {
    Serial.println("[MEAT GRINDER] Meat grinder is already on. Ignoring the request to turn it on again.");
    return;
  }
  isMeatGrinderOn = true;
  mcpRelay.digitalWrite(RELAY_MEAT_GRINDER, LOW); // Activate meat grinder
  Serial.println("[MEAT GRINDER] Meat grinder turned on.\n");
}

void turnOffMeatGrinder()
{
  if (!isMeatGrinderOn)
  {
    Serial.println("[MEAT GRINDER] Meat grinder is already off. Ignoring the request to turn it off again.");
    return;
  }
  isMeatGrinderOn = false;
  mcpRelay.digitalWrite(RELAY_MEAT_GRINDER, HIGH); // Deactivate meat grinder
  Serial.println("[MEAT GRINDER] Meat grinder turned off.\n");
}

void setupWifi()
{
  WiFi.mode(WIFI_STA);
  // reset settings - wipe credentials for testing
  // wifiManager.resetSettings();

  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConfigPortalTimeout(60);
  // automatically connect using saved credentials if they exist
  // If connection fails it starts an access point with the specified name
  if (wifiManager.autoConnect("ECOCYCLE_AP"))
  {
    Serial.println("Connected to WiFi network successfully!");
  }
  else
  {
    Serial.println("Configportal running");
  }
}

void lcdPrint(const String &message, int row, bool onCenter = true, bool clearRow = true, bool allCaps = true)
{
  if (clearRow)
  {
    lcd.setCursor(0, row);
    lcd.print("                    "); // Clear the row (20 spaces for a 20-column LCD)
  }

  String displayMessage = message;
  if (allCaps)
  {
    displayMessage.toUpperCase();
  }

  int messageLength = displayMessage.length();
  int startColumn = onCenter ? (LCD_COLUMNS - messageLength) / 2 : 0;

  lcd.setCursor(startColumn, row);
  lcd.print(displayMessage);

  Serial.printf("[LCD] Row %d: %s\n", row, displayMessage.c_str());
}

bool isFoodHopperCoverClosed()
{
  return mcpInputs.digitalRead(MCP23017_FOOD_HOPPER_COVER_PIN) == HIGH;
}

bool isSoilHopperCoverClosed()
{
  return mcpInputs.digitalRead(MCP23017_SOIL_HOPPER_COVER_PIN) == HIGH;
}

bool isSensorProbeHome()
{
  return mcpInputs.digitalRead(MCP23017_SENSOR_PROBE_HOME_PIN) == HIGH;
}

bool isGateHome()
{
  return mcpInputs.digitalRead(MCP23017_GATE_HOME_PIN) == HIGH;
}

void setupLcd()
{
  lcd.init();      // Initialize the LCD
  lcd.backlight(); // Turn on the backlight
  lcd.clear();     // Clear the display

  lcdPrint("ECOCYCLE", 0); // Display "ECOCYCLE" centered on the first row

  delay(2000); // Wait for 2 seconds before clearing the display
  lcd.clear(); // Clear the display
}

void testLimitSwitches()
{
  Serial.printf("Food Hopper Cover: %d \t ", isFoodHopperCoverClosed() ? 1 : 0);
  Serial.printf("Soil Hopper Cover: %d \t ", isSoilHopperCoverClosed() ? 1 : 0);
  Serial.printf("Sensor Probe Home: %d \t ", isSensorProbeHome() ? 1 : 0);
  Serial.printf("Gate Home: %d\n", isGateHome() ? 1 : 0);
  delay(1000);
}

void initStepper(AccelStepper &stepper, int maxSpeed, int acceleration)
{
  stepper.setMaxSpeed(maxSpeed);
  stepper.setAcceleration(acceleration);
}

void homeStepper(AccelStepper &stepper, bool (*isHome)(), const char *stepperName = "Stepper", int maxSteps = 10000)
{
  beep(100, 3, 100);
  // Move the stepper until the home switch is activated
  while (!isHome())
  {
    stepper.moveTo(-maxSteps); // Move in the negative direction
    stepper.run();
  }

  Serial.printf("[STEPPER] %s home position reached.\n", stepperName);
  // Stop the stepper and set its current position to zero

  stepper.stop();
  stepper.setCurrentPosition(0);
}

void moveStepperToPosition(AccelStepper &stepper, long targetPosition, const char *stepperName = "Stepper")
{
  beep(100, 3, 100);

  stepper.moveTo(targetPosition);
  while (stepper.distanceToGo() != 0)
  {
    stepper.run();
  }
  Serial.printf("[STEPPER] %s moved to position: %ld\n", stepperName, targetPosition);
}

void setupSteppers()
{
  // Initialize the stepper motors with their respective maximum speed and acceleration
  initStepper(sensorStepper, SENSOR_STEPPER_MAX_SPEED, SENSOR_STEPPER_ACCELERATION);
  initStepper(gateStepper, GATE_STEPPER_MAX_SPEED, GATE_STEPPER_ACCELERATION);

  // Home the steppers
  homeStepper(sensorStepper, isSensorProbeHome, "Sensor Stepper", SENSOR_STEPPER_MAX_STEPS);
  homeStepper(gateStepper, isGateHome, "Gate Stepper", GATE_STEPPER_MAX_STEPS);
}

void openGate()
{
  homeStepper(gateStepper, isGateHome, "Gate Stepper", GATE_STEPPER_MAX_STEPS);
  Serial.println("[GATE] Gate opened.\n");
}

void closeGate()
{
  moveStepperToPosition(gateStepper, GATE_STEPPER_MAX_STEPS, "Gate Stepper");
  Serial.println("[GATE] Gate closed.\n");
}

void lowerSensorProbe()
{
  moveStepperToPosition(sensorStepper, SENSOR_STEPPER_MAX_STEPS, "Sensor Stepper");
  Serial.println("[SENSOR PROBE] Sensor probe lowered.\n");
}

void raiseSensorProbe()
{
  homeStepper(sensorStepper, isSensorProbeHome, "Sensor Stepper", SENSOR_STEPPER_MAX_STEPS);
  Serial.println("[SENSOR PROBE] Sensor probe raised.\n");
}

void setup()
{
  Serial.begin(115200);

  setupWifi();
  setupLcd();
  setupMcp23017();
  delay(3000);
  setupSteppers();

  beep(150, 3, 100);

  beep(300);

  delay(5000);
  homeStepper(sensorStepper, isSensorProbeHome, "Sensor Stepper", SENSOR_STEPPER_MAX_STEPS);
  delay(5000);
}

void loop()
{
  wifiManager.process();

  testLimitSwitches();
}
