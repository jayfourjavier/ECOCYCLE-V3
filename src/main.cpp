#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <WiFiManager.h>       // https://github.com/tzapu/WiFiManager
#include <LiquidCrystal_I2C.h> // https://github.com/johnrickman/LiquidCrystal_I2C
#include <Adafruit_MCP23X17.h> // https://github.com/adafruit/Adafruit-MCP23017-Arduino-Library
#include <AccelStepper.h>      // https://github.com/waspinator/AccelStepper
#include <HX711.h>             // https://github.com/RobTillaart/HX711
#include <Adafruit_VL53L0X.h>  // https://github.com/adafruit/Adafruit_VL53L0X
#include <Adafruit_MAX31865.h> // https://github.com/adafruit/Adafruit_MAX31865
#include <time.h>

#define ENABLE_USER_AUTH
#define ENABLE_DATABASE
#include <FirebaseClient.h>
#include "ExampleFunctions.h" // Provides SSL_CLIENT and set_ssl_client_insecure_and_buffer()
#include "FirebaseConfig.h"

// Conversation log: update ../ai_agent.md after every user request and each change to this file.

/**
 * @brief Enum representing the different states of the machine.
 *
 * 0 - IDLE: The machine is idle and not performing any operations.<br>
 * 1 - GRINDING: The machine is currently grinding the material.<br>
 * 2 - GETTING_WEIGHT: The machine is measuring the weight of the material.<br>
 * 3 - ADDING_SOIL: The machine is adding soil to the mixture.<br>
 * 4 - LOWERING_SENSOR_PROBE: The machine is lowering the sensor probe.<br>
 * 5 - RAISING_SENSOR_PROBE: The machine is raising the sensor probe.<br>
 * 6 - GETTING_TEMPERATURE: The machine is measuring the temperature.<br>
 * 7 - GETTING_MOISTURE: The machine is measuring the moisture content.<br>
 * 8 - OPENING_GATE: The machine is opening the gate.<br>
 * 9 - CLOSING_GATE: The machine is closing the gate.<br>
 * 10 - MIXING: The machine is mixing the material.<br>
 * 11 - DROPPING: The machine is dropping the material.<br>
 * 12 - DROPPING_DONE: The dropping operation is complete.<br>
 * 13 - ERROR_STATE: The machine has encountered an error.<br>
 * 14 - GETTING_DATA: The machine is collecting sensor data.<br>
 * 15 - CHECKING_FOOD_LEVEL: The machine is checking the food hopper level against the preset.<br>
 * 16 - CHECKING_SOIL_LEVEL: The machine is checking the soil hopper level.
 * 14 - GETTING_DATA: The machine is collecting sensor data.<br>
 * 15 - CHECKING_FOOD_LEVEL: The machine is checking the food hopper level against the preset.<br>
 * 16 - CHECKING_SOIL_LEVEL: The machine is checking the soil hopper level.
 */
enum MachineState
{
  IDLE,
  GRINDING,
  GETTING_WEIGHT,
  ADDING_SOIL,
  LOWERING_SENSOR_PROBE,
  RAISING_SENSOR_PROBE,
  GETTING_TEMPERATURE,
  GETTING_MOISTURE,
  OPENING_GATE,
  CLOSING_GATE,
  MIXING,
  DROPPING,
  DROPPING_DONE,
  ERROR_STATE,
  GETTING_DATA,
  CHECKING_FOOD_LEVEL,
  CHECKING_SOIL_LEVEL
};

const char *machineStateName(MachineState state)
{
  switch (state)
  {
  case IDLE:
    return "IDLE";
  case GRINDING:
    return "GRINDING";
  case GETTING_WEIGHT:
    return "GETTING_WEIGHT";
  case ADDING_SOIL:
    return "ADDING_SOIL";
  case LOWERING_SENSOR_PROBE:
    return "LOWERING_SENSOR_PROBE";
  case RAISING_SENSOR_PROBE:
    return "RAISING_SENSOR_PROBE";
  case GETTING_TEMPERATURE:
    return "GETTING_TEMPERATURE";
  case GETTING_MOISTURE:
    return "GETTING_MOISTURE";
  case OPENING_GATE:
    return "OPENING_GATE";
  case CLOSING_GATE:
    return "CLOSING_GATE";
  case MIXING:
    return "MIXING";
  case DROPPING:
    return "DROPPING";
  case DROPPING_DONE:
    return "DROPPING_DONE";
  case ERROR_STATE:
    return "ERROR_STATE";
  case GETTING_DATA:
    return "GETTING_DATA";
  case CHECKING_FOOD_LEVEL:
    return "CHECKING_FOOD_LEVEL";
  case CHECKING_SOIL_LEVEL:
    return "CHECKING_SOIL_LEVEL";
  default:
    return "UNKNOWN";
  }
}

MachineState currentState = IDLE;
MachineState previousState = IDLE;

#define LCD_I2C_ADDRESS 0x27
#define LCD_COLUMNS 20
#define LCD_ROWS 4

#define MCP23017_INPUTS_ADDRESS 0x20
#define MCP23017_INPUTS_INT_PIN 25
#define MCP23017_RELAY_ADDRESS 0x21

#define SOIL_MOISTURE_0_PERCENT 4095
#define SOIL_MOISTURE_100_PERCENT 0

#define MCP23017_SENSOR_PROBE_HOME_PIN 1 // A1
#define MCP23017_FOOD_HOPPER_COVER_PIN 2 // A2
#define MCP23017_SOIL_HOPPER_COVER_PIN 3 // A3
#define MCP23017_GATE_HOME_PIN 4         // A4
#define MCP23017_MIXER_SAFE_PIN 5        // A5

#define RELAY_SOIL_DISPENSER_POSITIVE 13 // GPB5
#define RELAY_SOIL_DISPENSER_NEGATIVE 5  // GPA5
#define RELAY_BUZZER 9                   // GPB3 // DEV MODE
#define RELAY_MEAT_GRINDER 10            // GPB4 // DEV MODE
// #define RELAY_BUZZER 11               // GPB3 // REAL WIRING
// #define RELAY_MEAT_GRINDER 12 // GPB4 // REAL WIRING

#define SOIL_MOISTURE_SENSOR_ANALOG_PIN 35

#define SENSOR_STEPPER_MAX_SPEED 500
#define SENSOR_STEPPER_ACCELERATION 100
#define GATE_STEPPER_MAX_SPEED 1000
#define GATE_STEPPER_ACCELERATION 500
#define MIXER_STEPPER_MAX_SPEED 100 // DITO BABAGUHIN BILIS NG MIXER
#define MIXER_STEPPER_ACCELERATION 50
#define MIXER_STEPPER_HOMING_SPEED 50
#define MIXER_STEPPER_HOMING_ACCELERATION 10

#define SENSOR_STEPPER_MAX_STEPS 25000
#define GATE_STEPPER_MAX_STEPS 8000
#define MIXER_STEPPER_MIXING_STEPS 5000    // DITO MABABAGO GAANO KATAGAL MAGMIX YUNG MIXER
#define MIXER_STEPPER_DROPPING_STEPS 25000 // DITO MABABAGO GAANO KATAGAL IIKOT YUNG MIXER PARA MAG DROP NG LAMAN

#define SOIL_TOF_XSHUT_PIN 15
#define FOOD_TOF_XSHUT_PIN 2
#define FOOD_TOF_ADDRESS 0x30
#define SOIL_TOF_ADDRESS 0x31

#define PT100_CS_PIN 5
#define RREF 430.0     // Reference resistor value in ohms
#define RNOMINAL 100.0 // Nominal resistance of PT100 at 0°C in ohms

#define SOIL_EMPTY_THRESHOLD_MM 300.0f
#define FOOD_EMPTY_THRESHOLD_MM 200.0f
#define FOOD_LEVEL_PRESET_PERCENT 20.0f // Food hopper level (%) that must be exceeded
#define SOIL_LEVEL_PRESET_PERCENT 20.0f // Soil hopper level (%) that must be exceeded to start a batch

#define GRIND_DURATION_MS 3000      // How long the grinder runs each time
#define GRIND_INTERVAL_MS 5000      // Wait after the grinder stops before reading the scale
#define GRIND_TARGET_WEIGHT_G 50.0f // Ground food weight (g) to reach
#define GRIND_MAX_PULSES 20         // Safety limit on grinder pulses per batch

#define SOIL_DURATION_MS 3000       // How long the soil dispenser runs each time
#define SOIL_INTERVAL_MS 3000       // Wait after the dispenser stops before reading the scale
#define SOIL_TARGET_WEIGHT_G 950.0f // Soil weight (g) to add on top of the ground food
#define SOIL_MAX_PULSES 100         // Safety limit on soil dispenser pulses per batch

#define BOOT_GATE_HOME_MIN_WEIGHT_G 20.0f // At boot, the gate is homed only if the scale reads 20 g or less
#define BOOT_MIXING_MIN_WEIGHT_G 1000.0f  // At boot, if the scale reads more than this, skip the batch steps and go straight to mixing
#define SENSOR_SETTLE_MS 3000             // Wait after the sensor probe is lowered before reading temperature and moisture
#define DROP_SETTLE_MS 1000               // Wait between the drop steps

// ESP32 GPIO PINS USED TO CONTROL THE STEPPER MOTOR DRIVERS FOR SENSOR, MIXER, AND GATE STEPPERS MOTORS
#define SENSOR_STEPPER_PULSE_PIN 32
#define SENSOR_STEPPER_DIR_PIN 33
#define MIXER_STEPPER_PULSE_PIN 26
#define MIXER_STEPPER_DIR_PIN 27
#define GATE_STEPPER_PULSE_PIN 12
#define GATE_STEPPER_DIR_PIN 14

#define SCALE_DATA_PIN 16
#define SCALE_CLOCK_PIN 4
#define TO_CALIBRATE 0
#define SCALE_CALIBRATION_OFFSET 906540
#define SCALE_CALIBRATION_FACTOR 85.753746
#define SCALE_FILTER_TOLERANCE_G 5.0f // Samples further than this from the median are rejected (+/- 5 g accepted)
#define SCALE_READY_TIMEOUT_MS 250    // Max wait for a new HX711 sample
#define SCALE_MAX_SAMPLES 32

float soilRangeMm = 0.0f;
float foodRangeMm = 0.0f;
float soilPercentage = 0.0f;
float foodPercentage = 0.0f;
float currentNetWeight = 0.0f;
float targetNetWeight = 0.0f;
float targetSoilWeight = 0.0f;
bool bootProceedToMixing = false;                                                                   // True when the boot weight is above BOOT_MIXING_MIN_WEIGHT_G (batch already loaded)
float bootTareWeight = 0.0f;                                                                        // Weight (g) on the scale at boot, subtracted from every reading (calibration offset/factor stay untouched)
Adafruit_MCP23X17 mcpInputs, mcpRelay;                                                              // Create instances of Adafruit_MCP23X17 for inputs and relay
WiFiManager wifiManager;                                                                            // Create an instance of WiFiManager
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS);                                      // Create an instance of LiquidCrystal_I2C
AccelStepper sensorStepper(AccelStepper::DRIVER, SENSOR_STEPPER_PULSE_PIN, SENSOR_STEPPER_DIR_PIN); // Create an instance of AccelStepper for the sensor
AccelStepper mixerStepper(AccelStepper::DRIVER, MIXER_STEPPER_PULSE_PIN, MIXER_STEPPER_DIR_PIN);    // Create an instance of AccelStepper for the mixer
AccelStepper gateStepper(AccelStepper::DRIVER, GATE_STEPPER_PULSE_PIN, GATE_STEPPER_DIR_PIN);       // Create an instance of AccelStepper for the gate
HX711 scale;                                                                                        // Create an instance of HX711 for the scale
Adafruit_VL53L0X soilTof = Adafruit_VL53L0X();
Adafruit_VL53L0X foodTof = Adafruit_VL53L0X();
Adafruit_MAX31865 pt100 = Adafruit_MAX31865(PT100_CS_PIN);

VL53L0X_RangingMeasurementData_t foodRange;
VL53L0X_RangingMeasurementData_t soilRange;

bool isBuzzerOn = false;        // Flag to track the buzzer state
bool isSoilDispenserOn = false; // Flag to track the soil dispenser state
bool isMeatGrinderOn = false;   // Flag to track the meat grinder state

float grindBaselineWeight = 0.0f;        // Scale reading (g) at the start of the batch, used as tare
uint8_t grindPulseCount = 0;             // Number of grinder pulses done in this batch
unsigned long grindPulseStartTime = 0;   // millis() when the current grinder pulse started
unsigned long weightSettleStartTime = 0; // millis() when the grinder stopped (before weighing)

float soilBaselineWeight = 0.0f;       // Scale reading (g) at the start of soil dispensing, used as tare
float currentSoilWeight = 0.0f;        // Soil weight (g) added so far in this batch
uint8_t soilPulseCount = 0;            // Number of soil dispenser pulses done in this batch
unsigned long soilPulseStartTime = 0;  // millis() when the current soil pulse started
unsigned long soilSettleStartTime = 0; // millis() when the soil dispenser stopped (before weighing)
bool isSoilSettling = false;           // True while waiting for the scale to settle after a soil pulse

bool foodCoverState = false;              // Flag to track the food hopper cover state
bool soilCoverState = false;              // Flag to track the soil hopper cover state
bool isFoodCoverOpenedThenClosed = false; // Flag to track if the food hopper cover was opened and then closed
bool isSoilCoverOpenedThenClosed = false; // Flag to track if the soil hopper cover was opened and then closed
bool foodLevelSufficient = false;         // True once the food level check passed; lets a soil refill resume the batch start

void setupPt100()
{
  pt100.begin(MAX31865_3WIRE); // set to 2WIRE or 4WIRE as necessary
}

float getTemperature()
{

  return pt100.temperature(RNOMINAL, RREF);
}

void pt100Controller()
{

  // read sensor every second
  static unsigned long lastReadTime = 0;
  unsigned long currentTime = millis();

  if (currentTime - lastReadTime >= 1000)
  {
    lastReadTime = currentTime;

    float temperature = getTemperature();
    Serial.printf("Current Temperature: %.2f °C\n", temperature);
  }
}

void setupTofSensors()
{
  pinMode(SOIL_TOF_XSHUT_PIN, OUTPUT);
  pinMode(FOOD_TOF_XSHUT_PIN, OUTPUT);

  digitalWrite(SOIL_TOF_XSHUT_PIN, LOW);
  digitalWrite(FOOD_TOF_XSHUT_PIN, LOW);
}

void getFoodRange()
{
  digitalWrite(FOOD_TOF_XSHUT_PIN, HIGH);
  digitalWrite(SOIL_TOF_XSHUT_PIN, LOW);

  delay(100);

  if (!foodTof.begin())
  {
    Serial.println("Failed to boot food ToF sensor");
  }
  else
  {
    Serial.println("Food ToF sensor booted successfully");
    foodTof.rangingTest(&foodRange, false);
    foodRangeMm = static_cast<float>(foodRange.RangeMilliMeter);
    Serial.printf("Food ToF Range: %.2f mm\n", foodRangeMm);
  }
}

void getSoilRange()
{
  digitalWrite(SOIL_TOF_XSHUT_PIN, HIGH);
  digitalWrite(FOOD_TOF_XSHUT_PIN, LOW);
  delay(100);
  if (!soilTof.begin())
  {
    Serial.println("Failed to boot soil ToF sensor");
  }
  else
  {
    Serial.println("Soil ToF sensor booted successfully");
    soilTof.rangingTest(&soilRange, false);
    soilRangeMm = static_cast<float>(soilRange.RangeMilliMeter);
    Serial.printf("Soil ToF Range: %.2f mm\n", soilRangeMm);
  }
}

void setupScale()
{
  scale.begin(SCALE_DATA_PIN, SCALE_CLOCK_PIN);
  scale.set_offset(SCALE_CALIBRATION_OFFSET);
  scale.set_scale(SCALE_CALIBRATION_FACTOR);
}

void calibrateScale()
{
  if (!TO_CALIBRATE)
  {
    return;
  }

  Serial.println("\n\nCALIBRATION");
  Serial.println("===========");
  Serial.println("remove all weight from the loadcell");
  while (Serial.available())
  {
    Serial.read();
  }
  Serial.println("and press enter\n");

  while (Serial.available() == 0)
  {
  }

  Serial.println("Determine zero weight offset");
  scale.tare(20);
  int32_t offset = scale.get_offset();

  Serial.print("OFFSET: ");
  Serial.println(offset);
  Serial.println();

  Serial.println("place a weight on the loadcell");
  while (Serial.available())
  {
    Serial.read();
  }
  Serial.println("enter the weight in (whole) grams and press enter");

  uint32_t weight = 0;
  while (Serial.peek() != '\n')
  {
    if (Serial.available())
    {
      char ch = Serial.read();
      if (isdigit(ch))
      {
        weight *= 10;
        weight += static_cast<uint32_t>(ch - '0');
      }
    }
  }

  Serial.print("WEIGHT: ");
  Serial.println(weight);
  scale.calibrate_scale(weight, 20);
  float scaleFactor = scale.get_scale();

  // Serial.print("SCALE:  ");
  // Serial.println(scaleFactor, 6);
  // Serial.print("\nuse scale.set_offset(");
  // Serial.print(offset);
  // Serial.print("); and scale.set_scale(");
  // Serial.print(scaleFactor, 6);
  // Serial.print(");\n");
  // Serial.println("in the setup of your project");
  // Serial.println("\n\n");

  // PRINT THIS FORMAT
  // #define SCALE_CALIBRATION_OFFSET 934311
  // #define SCALE_CALIBRATION_FACTOR 54.350742

  Serial.printf("#define SCALE_CALIBRATION_OFFSET %ld\n", static_cast<long>(offset));
  Serial.printf("#define SCALE_CALIBRATION_FACTOR %.6f\n", scaleFactor);
  Serial.println();
}

/*
 *@brief Reads the scale and returns a filtered weight in grams.
 * Collects fresh HX711 samples, finds the median, rejects samples further than SCALE_FILTER_TOLERANCE_G (+/- 5 g)
 * from the median, and averages the rest. If no sample is obtained, the last good value is returned.
 */
float getWeightGrams(uint8_t sampleSize = 10)
{
  static float lastGoodWeight = 0.0f;

  if (sampleSize == 0)
  {
    return lastGoodWeight;
  }
  if (sampleSize > SCALE_MAX_SAMPLES)
  {
    sampleSize = SCALE_MAX_SAMPLES;
  }

  float samples[SCALE_MAX_SAMPLES];
  uint8_t count = 0;

  for (uint8_t i = 0; i < sampleSize; ++i)
  {
    unsigned long waitStart = millis();
    while (!scale.is_ready() && millis() - waitStart < SCALE_READY_TIMEOUT_MS)
    {
      delay(1);
    }
    if (!scale.is_ready())
    {
      continue;
    }

    float reading = scale.get_units(1);
    if (!isnan(reading) && !isinf(reading))
    {
      samples[count++] = reading;
    }
  }

  if (count == 0)
  {
    Serial.println("[SCALE] No readings. Using last good value.");
    return lastGoodWeight;
  }

  // Median of all samples (insertion sort on a copy)
  float sorted[SCALE_MAX_SAMPLES];
  for (uint8_t i = 0; i < count; ++i)
  {
    float key = samples[i];
    int8_t j = i - 1;
    while (j >= 0 && sorted[j] > key)
    {
      sorted[j + 1] = sorted[j];
      --j;
    }
    sorted[j + 1] = key;
  }
  float median = (count % 2 == 1) ? sorted[count / 2] : (sorted[count / 2 - 1] + sorted[count / 2]) / 2.0f;

  // Average only the samples within +/- tolerance of the median
  float total = 0.0f;
  uint8_t accepted = 0;
  for (uint8_t i = 0; i < count; ++i)
  {
    if (fabsf(samples[i] - median) <= SCALE_FILTER_TOLERANCE_G)
    {
      total += samples[i];
      ++accepted;
    }
  }

  float rawWeight = accepted > 0 ? (total / static_cast<float>(accepted)) : median;
  lastGoodWeight = rawWeight - bootTareWeight;
  return lastGoodWeight;
}

// Stores the current scale reading (boot weight) as a weight to subtract from later readings.
// The scale's calibration offset and factor are not changed. Call after the boot weight check and gate homing.
void tareScaleAtBoot()
{
  bootTareWeight = 0.0f; // Read the raw weight, not one already corrected by an earlier tare
  bootTareWeight = getWeightGrams(20);
  Serial.printf("[SCALE] Boot weight %.1f g stored as tare weight.\n", bootTareWeight);
}

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

void setupMoistureSensor()
{
  pinMode(SOIL_MOISTURE_SENSOR_ANALOG_PIN, INPUT);
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
  if (wifiManager.autoConnect("ECOCYCLE_AP", "password123"))
  {
    Serial.println("Connected to WiFi network successfully!");
  }
  else
  {
    Serial.println("Configportal running");
  }
}

SSL_CLIENT sslClient;
using AsyncClient = AsyncClientClass;
AsyncClient firebaseAsyncClient(sslClient);
UserAuth firebaseUserAuth(FIREBASE_WEB_API_KEY, FIREBASE_USER_EMAIL, FIREBASE_USER_PASSWORD, 3000);
FirebaseApp firebaseApp;
RealtimeDatabase firebaseDatabase;

unsigned long getEpochSeconds(unsigned long timeoutMs = 10000);

bool firebaseAuthFailed = false;

// Prints auth events and flags the first error so setupFirebase() can stop retrying.
void firebaseAuthCallback(AsyncResult &aResult)
{
  auth_debug_print(aResult);
  if (aResult.isError())
  {
    firebaseAuthFailed = true;
  }
}

// Initializes Firebase auth and blocks (up to timeoutMs) until the app is ready.
bool setupFirebase(unsigned long timeoutMs = 30000)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[FIREBASE] WiFi not connected. Skipping Firebase setup.");
    return false;
  }

  // Valid system time is required before authenticating.
  if (getEpochSeconds(20000) == 0)
  {
    Serial.println("[FIREBASE] NTP time sync failed. Skipping Firebase setup.");
    return false;
  }

  set_ssl_client_insecure_and_buffer(sslClient);
  initializeApp(firebaseAsyncClient, firebaseApp, getAuth(firebaseUserAuth), firebaseAuthCallback, "authTask");
  firebaseApp.getApp<RealtimeDatabase>(firebaseDatabase);
  firebaseDatabase.url(FIREBASE_DATABASE_URL);

  unsigned long start = millis();
  while (!firebaseApp.ready() && !firebaseAuthFailed && millis() - start < timeoutMs)
  {
    firebaseApp.loop();
    delay(10);
  }

  if (!firebaseApp.ready())
  {
    // firebaseApp.loop() is no longer called, so no further sign-in attempts are made.
    Serial.println(firebaseAuthFailed ? "[FIREBASE] Authentication failed. Not retrying." : "[FIREBASE] Authentication timed out.");
    return false;
  }

  Serial.println("[FIREBASE] Ready.");
  return true;
}

unsigned long getEpochSeconds(unsigned long timeoutMs)
{
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  unsigned long start = millis();
  time_t now = time(nullptr);
  while (now < 1700000000L && millis() - start < timeoutMs)
  {
    delay(200);
    now = time(nullptr);
  }

  return now < 1700000000L ? 0 : static_cast<unsigned long>(now);
}

// Record validation limits
#define RECORD_TIMESTAMP_START_EPOCH 1700000000UL
#define RECORD_TIMESTAMP_END_EPOCH 4102444800UL
#define RECORD_TEMPERATURE_MIN -50.0f
#define RECORD_TEMPERATURE_MAX 150.0f
#define RECORD_MOISTURE_MIN 0.0f
#define RECORD_MOISTURE_MAX 100.0f

#define FIREBASE_WRITE_MAX_ATTEMPTS 3

// Values stored in the batch record (record format: "EC-001,<epoch>,<temperature>,<moisture>")
float recordTemperature = 0.0f;
float recordMoisture = 0.0f;

bool firebaseWriteString(const String &path, const String &value)
{
  for (uint8_t attempt = 1; attempt <= FIREBASE_WRITE_MAX_ATTEMPTS; ++attempt)
  {
    if (firebaseDatabase.set<String>(firebaseAsyncClient, path, value))
    {
      return true;
    }
    Serial.printf("[FIREBASE] Write attempt %u/%u failed.\n", attempt, FIREBASE_WRITE_MAX_ATTEMPTS);
  }
  return false;
}

String makeBatchPath(const String &batchNumber)
{
  return "/records/" + batchNumber;
}

String nextBatchCode(const String &latestBatch)
{
  if (latestBatch == "EC-000" || latestBatch.length() == 0 || !latestBatch.startsWith("EC-") || latestBatch.length() <= 3)
    return "EC-001";

  int batchNumber = latestBatch.substring(3).toInt();
  if (batchNumber <= 0)
    return "EC-001";

  char padded[8];
  snprintf(padded, sizeof(padded), "EC-%03d", batchNumber + 1);
  return String(padded);
}

bool isValidBatchRecord(const String &batchKey, const String &recordValue)
{
  if (recordValue.length() == 0 || recordValue == "null" || recordValue == "{}" || recordValue == "[]")
    return false;

  int firstComma = recordValue.indexOf(',');
  int secondComma = firstComma >= 0 ? recordValue.indexOf(',', firstComma + 1) : -1;
  int thirdComma = secondComma >= 0 ? recordValue.indexOf(',', secondComma + 1) : -1;

  if (firstComma <= 0 || secondComma <= firstComma || thirdComma <= secondComma || recordValue.indexOf(',', thirdComma + 1) >= 0)
    return false;

  String storedBatch = recordValue.substring(0, firstComma);
  String timestampText = recordValue.substring(firstComma + 1, secondComma);
  String temperatureText = recordValue.substring(secondComma + 1, thirdComma);
  String moistureText = recordValue.substring(thirdComma + 1);

  if (storedBatch != batchKey)
    return false;

  unsigned long timestamp = timestampText.toInt();
  if (timestamp < RECORD_TIMESTAMP_START_EPOCH || timestamp > RECORD_TIMESTAMP_END_EPOCH)
    return false;

  float temperature = temperatureText.toFloat();
  float moisture = moistureText.toFloat();

  if (temperature < RECORD_TEMPERATURE_MIN || temperature > RECORD_TEMPERATURE_MAX)
    return false;

  if (moisture < RECORD_MOISTURE_MIN || moisture > RECORD_MOISTURE_MAX)
    return false;

  return true;
}

String getLatestBatchNodeName()
{
  String recordsJson = firebaseDatabase.get<String>(firebaseAsyncClient, "/records");

  if (recordsJson.length() == 0 || recordsJson == "null" || recordsJson == "{}" || recordsJson == "[]")
  {
    Serial.println("[FIREBASE] /records empty => EC-000");
    return "EC-000";
  }

  String latestBatch = "EC-000";
  int latestNumber = 0;

  int index = 0;
  while (index < recordsJson.length())
  {
    int quoteStart = recordsJson.indexOf('"', index);
    if (quoteStart < 0)
      break;

    int quoteEnd = recordsJson.indexOf('"', quoteStart + 1);
    if (quoteEnd < 0)
      break;

    String token = recordsJson.substring(quoteStart + 1, quoteEnd);

    int nextCharIndex = quoteEnd + 1;
    while (nextCharIndex < recordsJson.length() && isSpace(recordsJson[nextCharIndex]))
      ++nextCharIndex;

    bool isKey = nextCharIndex < recordsJson.length() && recordsJson[nextCharIndex] == ':';

    if (isKey && token.startsWith("EC-") && token.length() > 3)
    {
      String numericPart = token.substring(3);
      if (numericPart.length() > 0 && numericPart.indexOf(" ") < 0)
      {
        int batchNumber = numericPart.toInt();
        String batchValue = firebaseDatabase.get<String>(firebaseAsyncClient, "/records/" + token);

        if (isValidBatchRecord(token, batchValue))
        {
          if (batchNumber > latestNumber)
          {
            latestNumber = batchNumber;
            latestBatch = token;
          }
        }
        else
        {
          Serial.print("[FIREBASE] Skip invalid record: ");
          Serial.println(token);
        }
      }
    }

    index = quoteEnd + 1;
  }

  Serial.print("[FIREBASE] Latest valid batch: ");
  Serial.println(latestBatch);
  return latestBatch;
}

String makeRecordCsv(const String &batchCode, unsigned long timestamp, float temperature, float moisture)
{
  return batchCode + "," + String(timestamp) + "," + String(temperature, 1) + "," + String(moisture, 1);
}

String formatEpoch(unsigned long epochSeconds, const char *format)
{
  time_t t = (time_t)epochSeconds;
  struct tm timeinfo;
  localtime_r(&t, &timeinfo);

  char buffer[32];
  strftime(buffer, sizeof(buffer), format, &timeinfo);
  return String(buffer);
}

void prettyPrintRecord(const String &dataCsv)
{
  int firstComma = dataCsv.indexOf(',');
  int secondComma = firstComma >= 0 ? dataCsv.indexOf(',', firstComma + 1) : -1;
  int thirdComma = secondComma >= 0 ? dataCsv.indexOf(',', secondComma + 1) : -1;

  if (firstComma < 0 || secondComma < 0 || thirdComma < 0)
  {
    Serial.print("CSV record: ");
    Serial.println(dataCsv);
    return;
  }

  unsigned long epochSeconds = dataCsv.substring(firstComma + 1, secondComma).toInt();

  Serial.println("_________________________");
  Serial.println("Data");
  Serial.print("Batch Number: ");
  Serial.println(dataCsv.substring(0, firstComma));
  Serial.print("Date: ");
  Serial.println(formatEpoch(epochSeconds, "%Y-%m-%d"));
  Serial.print("Time: ");
  Serial.println(formatEpoch(epochSeconds, "%H:%M:%S"));
  Serial.print("Temperature: ");
  Serial.print(dataCsv.substring(secondComma + 1, thirdComma).toFloat(), 1);
  Serial.println(" C");
  Serial.print("Moisture: ");
  Serial.print(dataCsv.substring(thirdComma + 1).toFloat(), 1);
  Serial.println(" %");
  Serial.println("_________________________");
}

// Writes a new batch record (next EC-xxx) using the global recordTemperature and recordMoisture values,
// then reads it back to verify.
bool writeBatchRecord()
{
  if (!firebaseApp.ready())
  {
    Serial.println("[FIREBASE] Not ready. Batch record write skipped.");
    return false;
  }

  String batchCode = nextBatchCode(getLatestBatchNodeName());
  String recordPath = makeBatchPath(batchCode);
  String csvRecord = makeRecordCsv(batchCode, getEpochSeconds(), recordTemperature, recordMoisture);

  Serial.print("[FIREBASE] Writing ");
  Serial.print(recordPath);
  Serial.print(" = ");
  Serial.println(csvRecord);

  if (!firebaseWriteString(recordPath, csvRecord))
  {
    Serial.println("[FIREBASE] Batch record write FAILED.");
    return false;
  }

  Serial.println("[FIREBASE] Batch record write OK.");
  prettyPrintRecord(firebaseDatabase.get<String>(firebaseAsyncClient, recordPath));
  return true;
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

void lcdController()
{
  Serial.printf("[LCD][STATE] Refresh check at %lu ms: current=%s, previous=%s\n",
                millis(), machineStateName(currentState), machineStateName(previousState));

  if (currentState == previousState)
  {
    Serial.println("[LCD][STATE] Redraw skipped because current and previous states match.");
    return; // No state change, no need to update the LCD
  }

  Serial.printf("[LCD][STATE] Rendering %s.\n", machineStateName(currentState));

  switch (currentState)
  {
  case IDLE:
    lcdPrint("IDLE", 0);
    break;
  case GRINDING:
    lcdPrint("GRINDING", 0);
    break;
  case GETTING_WEIGHT:
    lcdPrint("GETTING WEIGHT", 0);
    break;
  case ADDING_SOIL:
    lcdPrint("ADDING SOIL", 0);
    break;
  case LOWERING_SENSOR_PROBE:
    lcdPrint("LOWERING SENSORS", 0);
    break;
  case RAISING_SENSOR_PROBE:
    lcdPrint("RAISING SENSORS", 0);
    break;
  case GETTING_TEMPERATURE:
    lcdPrint("GETTING TEMPERATURE", 0);
    break;
  case GETTING_MOISTURE:
    lcdPrint("GETTING MOISTURE", 0);
    break;
  case OPENING_GATE:
    lcdPrint("OPENING GATE", 0);
    break;
  case CLOSING_GATE:
    lcdPrint("CLOSING GATE", 0);
    break;
  case MIXING:
    lcdPrint("MIXING", 0);
    break;
  case DROPPING:
    lcdPrint("DROPPING", 0);
    break;
  case DROPPING_DONE:
    lcdPrint("DROPPING DONE", 0);
    break;
  case GETTING_DATA:
    lcdPrint("GETTING DATA", 0);
    break;
  case ERROR_STATE:
    lcdPrint("ERROR STATE", 0);
    break;
  case CHECKING_FOOD_LEVEL:
    lcdPrint("CHECKING FOOD LEVEL", 0);
    break;
  case CHECKING_SOIL_LEVEL:
    lcdPrint("CHECKING SOIL LEVEL", 0);
    break;
  default:
    lcdPrint("UNKNOWN STATE", 0);
    break;
  }
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

bool isMixerHome()
{
  return mcpInputs.digitalRead(MCP23017_MIXER_SAFE_PIN) == LOW;
}

#define COVER_DEBOUNCE_MS 50

/**
 * @brief Debounced open/close detection for a hopper cover.
 *
 * coverClosed (foodCoverState / soilCoverState): true = closed, false = open.<br>
 * openedThenClosed: set when the cover goes open -> closed, cleared when it is opened again.
 * The consumer should clear it after handling the event.
 */
void updateCoverState(bool isClosedNow, bool &coverClosed, bool &openedThenClosed, unsigned long &changeStartTime, const char *coverName)
{
  if (isClosedNow == coverClosed)
  {
    changeStartTime = 0; // Reading matches the stored state, cancel any pending change
    return;
  }

  unsigned long now = millis();
  if (changeStartTime == 0)
  {
    changeStartTime = (now == 0) ? 1 : now; // Start debounce timer
    return;
  }

  if (now - changeStartTime < COVER_DEBOUNCE_MS)
  {
    return; // Not stable long enough yet
  }

  coverClosed = isClosedNow;
  changeStartTime = 0;

  if (coverClosed)
  {
    openedThenClosed = true;
    Serial.printf("[COVER] %s cover closed (was opened).\n", coverName);
  }
  else
  {
    openedThenClosed = false;
    Serial.printf("[COVER] %s cover opened.\n", coverName);
  }
}

void setupCovers()
{
  foodCoverState = isFoodHopperCoverClosed();
  soilCoverState = isSoilHopperCoverClosed();
  isFoodCoverOpenedThenClosed = false;
  isSoilCoverOpenedThenClosed = false;
}

void coverController()
{
  static unsigned long foodChangeStart = 0;
  static unsigned long soilChangeStart = 0;

  updateCoverState(isFoodHopperCoverClosed(), foodCoverState, isFoodCoverOpenedThenClosed, foodChangeStart, "Food hopper");
  updateCoverState(isSoilHopperCoverClosed(), soilCoverState, isSoilCoverOpenedThenClosed, soilChangeStart, "Soil hopper");
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
  Serial.printf("Mixer Home: %d \t ", isMixerHome() ? 1 : 0);
  Serial.printf("Gate Home: %d\n", isGateHome() ? 1 : 0);
  delay(1000);
}

void testScale()
{
#if TO_CALIBRATE
  return; // Skip scale test if calibration is needed
#endif
  float weight = getWeightGrams(20);
  Serial.printf("Weight: %.2f grams\n", weight);
  delay(3000);
}

void initStepper(AccelStepper &stepper, int maxSpeed, int acceleration)
{
  stepper.setMaxSpeed(maxSpeed);
  stepper.setAcceleration(acceleration);
}

void homeStepper(AccelStepper &stepper, bool (*isHome)(), const char *stepperName = "Stepper", int maxSteps = 10000, int speed = 1000, int acceleration = 500)
{
  Serial.printf("[STEPPER][HOME] %s starting at %lu ms: position=%ld, home=%u, maxSteps=%d, speed=%d, acceleration=%d\n",
                stepperName, millis(), stepper.currentPosition(), isHome() ? 1 : 0, maxSteps, speed, acceleration);
  beep(100, 3, 100);

  // Move the stepper until the home switch is activated
  while (!isHome())
  {
    stepper.setMaxSpeed(speed);
    stepper.setAcceleration(acceleration);
    stepper.moveTo(-maxSteps); // Move in the negative direction
    stepper.run();
  }

  Serial.printf("[STEPPER][HOME] %s switch active at %lu ms; stopping at position=%ld.\n",
                stepperName, millis(), stepper.currentPosition());
  // Stop the stepper and set its current position to zero

  stepper.stop();
  stepper.setCurrentPosition(0);
  Serial.printf("[STEPPER][HOME] %s complete; position reset to 0.\n", stepperName);
}

void moveStepperToPosition(AccelStepper &stepper, long targetPosition, const char *stepperName = "Stepper")
{
  beep(100, 3, 100);

  stepper.moveTo(targetPosition);
  Serial.printf("[STEPPER][MOVE] %s starting at %lu ms: position=%ld, target=%ld, distanceToGo=%ld, maxSpeed=%.1f, acceleration=%.1f\n",
                stepperName, millis(), stepper.currentPosition(), targetPosition, stepper.distanceToGo(), stepper.maxSpeed(), stepper.acceleration());

  while (stepper.distanceToGo() != 0)
  {
    stepper.run();
  }
  Serial.printf("[STEPPER][MOVE] %s completed at %lu ms: position=%ld, target=%ld.\n",
                stepperName, millis(), stepper.currentPosition(), targetPosition);
}

void moveMixerToSafePosition()
{
  const long startingPosition = mixerStepper.currentPosition();
  const int homingSpeed = -MIXER_STEPPER_HOMING_SPEED;
  Serial.printf("[MIXER] Sensor-based homing started at %lu ms: position=%ld, sensor=%u, speed=%d steps/s.\n",
                millis(), startingPosition, isMixerHome() ? 1 : 0, homingSpeed);

  if (!isMixerHome())
  {
    mixerStepper.setMaxSpeed(MIXER_STEPPER_HOMING_SPEED);
    mixerStepper.setAcceleration(MIXER_STEPPER_HOMING_ACCELERATION);

    // Continue blocking moveTo/run segments in the setup homing direction until the sensor triggers.
    const long seekChunkSteps = 10000;
    mixerStepper.moveTo(mixerStepper.currentPosition() - seekChunkSteps);
    while (!isMixerHome())
    {
      mixerStepper.run();

      if (mixerStepper.distanceToGo() == 0)
      {
        mixerStepper.moveTo(mixerStepper.currentPosition() - seekChunkSteps);
      }
    }
  }

  mixerStepper.setCurrentPosition(0);
  Serial.printf("[MIXER] Safe-position sensor triggered at %lu ms; position reset to 0.\n", millis());
}

void setupSteppers()
{
  // Initialize the stepper motors with their respective maximum speed and acceleration
  initStepper(sensorStepper, SENSOR_STEPPER_MAX_SPEED, SENSOR_STEPPER_ACCELERATION);
  initStepper(gateStepper, GATE_STEPPER_MAX_SPEED, GATE_STEPPER_ACCELERATION);
  initStepper(mixerStepper, MIXER_STEPPER_MAX_SPEED, MIXER_STEPPER_ACCELERATION);

  // Home the steppers
  homeStepper(sensorStepper, isSensorProbeHome, "Sensor Stepper", SENSOR_STEPPER_MAX_STEPS);
  float bootWeight = getWeightGrams(10);
  bootProceedToMixing = bootWeight > BOOT_MIXING_MIN_WEIGHT_G;
  if (bootWeight <= BOOT_GATE_HOME_MIN_WEIGHT_G) // Not more than 20 g on the scale, safe to home the gate
  {
    Serial.printf("[SCALE] Boot weight %.1f g (<= %.1f g). Homing gate.\n", bootWeight, BOOT_GATE_HOME_MIN_WEIGHT_G);
    homeStepper(gateStepper, isGateHome, "Gate Stepper", GATE_STEPPER_MAX_STEPS);
  }
  else
  {
    Serial.printf("[SCALE] Boot weight %.1f g (> %.1f g). Gate homing skipped.\n", bootWeight, BOOT_GATE_HOME_MIN_WEIGHT_G);
  }
  // homeStepper(mixerStepper, isMixerHome, "Mixer Stepper", MIXER_STEPPER_MIXING_STEPS);

  moveMixerToSafePosition();
}

void openGate()
{
  currentState = OPENING_GATE;
  lcdController();
  homeStepper(gateStepper, isGateHome, "Gate Stepper", GATE_STEPPER_MAX_STEPS);
  Serial.println("[GATE] Gate opened.\n");
}

void closeGate()
{
  currentState = CLOSING_GATE;
  lcdController();
  moveStepperToPosition(gateStepper, GATE_STEPPER_MAX_STEPS, "Gate Stepper");
  Serial.println("[GATE] Gate closed.\n");
}

void lowerSensorProbe()
{
  currentState = LOWERING_SENSOR_PROBE;
  lcdController();
  moveStepperToPosition(sensorStepper, SENSOR_STEPPER_MAX_STEPS, "Sensor Stepper");
  Serial.println("[SENSOR PROBE] Sensor probe lowered.\n");
}

void raiseSensorProbe()
{
  currentState = RAISING_SENSOR_PROBE;
  lcdController();
  homeStepper(sensorStepper, isSensorProbeHome, "Sensor Stepper", SENSOR_STEPPER_MAX_STEPS);
  Serial.println("[SENSOR PROBE] Sensor probe raised.\n");
}
void mix()
{
  Serial.printf("[MIX] Routine entered at %lu ms: current=%s, previous=%s, movement=%d steps.\n",
                millis(), machineStateName(currentState), machineStateName(previousState), MIXER_STEPPER_MIXING_STEPS);
  currentState = MIXING;
  lcdController();
  initStepper(mixerStepper, MIXER_STEPPER_MAX_SPEED, MIXER_STEPPER_ACCELERATION);
  moveStepperToPosition(mixerStepper, -MIXER_STEPPER_MIXING_STEPS, "Mixer Stepper");
  Serial.printf("[MIX] Mixing movement complete at %lu ms; holding for 1000 ms.\n", millis());
  delay(1000);
  Serial.printf("[MIX] Returning mixer to safe position at %lu ms.\n", millis());
  moveMixerToSafePosition();
  Serial.printf("[MIX] Routine complete at %lu ms.\n", millis());
}
void mixDrop()
{
  currentState = DROPPING;
  lcdController();
  initStepper(mixerStepper, MIXER_STEPPER_MAX_SPEED, MIXER_STEPPER_ACCELERATION);
  moveStepperToPosition(mixerStepper, MIXER_STEPPER_DROPPING_STEPS, "Mixer Stepper");
  delay(1000);
  moveMixerToSafePosition();
}

/*
 *@brief Calculates the soil level percentage based on the soil range in millimeters.
 * The percentage is calculated as the difference between the empty threshold and the current soil range,
 *@return The soil calculated sol level percentage.
 */
float getSoilLevelPercent()
{
  return constrain(
      (SOIL_EMPTY_THRESHOLD_MM - soilRangeMm) * 100.0f / SOIL_EMPTY_THRESHOLD_MM,
      0.0f,
      100.0f);
}

/*
 *@brief Calculates the food level percentage based on the food range in millimeters.
 * The percentage is calculated as the difference between the empty threshold and the current food range.
 *@return The calculated food level percentage.
 */
float getFoodLevelPercent()
{
  return constrain(
      (FOOD_EMPTY_THRESHOLD_MM - foodRangeMm) * 100.0f / FOOD_EMPTY_THRESHOLD_MM,
      0.0f,
      100.0f);
}

/*
 *@brief Boots the food ToF sensor and averages several valid range readings (RangeStatus 4 = out of range is ignored).
 * Updates foodRangeMm and foodPercentage.
 *@param percent Receives the averaged food level percentage.
 *@return true if at least one valid reading was obtained, false otherwise.
 */
bool readFoodLevelPercent(float &percent, uint8_t samples = 5)
{
  digitalWrite(FOOD_TOF_XSHUT_PIN, HIGH);
  digitalWrite(SOIL_TOF_XSHUT_PIN, LOW);
  delay(100);

  if (!foodTof.begin())
  {
    Serial.println("[FOOD] Failed to boot food ToF sensor.");
    return false;
  }

  float totalMm = 0.0f;
  uint8_t validReadings = 0;

  for (uint8_t i = 0; i < samples; ++i)
  {
    foodTof.rangingTest(&foodRange, false);
    if (foodRange.RangeStatus != 4)
    {
      totalMm += static_cast<float>(foodRange.RangeMilliMeter);
      ++validReadings;
    }
    delay(50);
  }

  if (validReadings == 0)
  {
    Serial.println("[FOOD] No valid food ToF readings (out of range).");
    return false;
  }

  foodRangeMm = totalMm / static_cast<float>(validReadings);
  foodPercentage = getFoodLevelPercent();
  percent = foodPercentage;
  return true;
}

/*
 *@brief Boots the soil ToF sensor and averages several valid range readings (RangeStatus 4 = out of range is ignored).
 * Updates soilRangeMm and soilPercentage.
 *@param percent Receives the averaged soil level percentage.
 *@return true if at least one valid reading was obtained, false otherwise.
 */
bool readSoilLevelPercent(float &percent, uint8_t samples = 5)
{
  digitalWrite(SOIL_TOF_XSHUT_PIN, HIGH);
  digitalWrite(FOOD_TOF_XSHUT_PIN, LOW);
  delay(100);

  if (!soilTof.begin())
  {
    Serial.println("[SOIL] Failed to boot soil ToF sensor.");
    return false;
  }

  float totalMm = 0.0f;
  uint8_t validReadings = 0;

  for (uint8_t i = 0; i < samples; ++i)
  {
    soilTof.rangingTest(&soilRange, false);
    if (soilRange.RangeStatus != 4)
    {
      totalMm += static_cast<float>(soilRange.RangeMilliMeter);
      ++validReadings;
    }
    delay(50);
  }

  if (validReadings == 0)
  {
    Serial.println("[SOIL] No valid soil ToF readings (out of range).");
    return false;
  }

  soilRangeMm = totalMm / static_cast<float>(validReadings);
  soilPercentage = getSoilLevelPercent();
  percent = soilPercentage;
  return true;
}

/*
 *@brief Calculates the soil moisture percentage based on the analog sensor value.
 *@return The calculated soil moisture percentage.
 */
float getMoisturePercent()
{
  int soilMoistureValue = analogRead(SOIL_MOISTURE_SENSOR_ANALOG_PIN);
  return map(soilMoistureValue, SOIL_MOISTURE_0_PERCENT, SOIL_MOISTURE_100_PERCENT, 0, 100);
}

// read every second
void getTofRangeData()
{
  static unsigned long lastTofReadTime = 0;
  unsigned long currentTime = millis();
  if (currentTime - lastTofReadTime >= 1000) // read every second
  {
    lastTofReadTime = currentTime;

    getSoilRange();
    getFoodRange();

    // PRINT ON ROW 3 & 4
    lcdPrint("Soil: " + String(soilRangeMm, 2) + " mm", 2, false);
    lcdPrint("Food: " + String(foodRangeMm, 2) + " mm", 3, false);

    soilPercentage = getSoilLevelPercent();
    foodPercentage = getFoodLevelPercent();
  }
}

void drop()
{
  raiseSensorProbe();
  delay(3000);
  openGate();
  delay(3000);
  mix();
  closeGate();
  delay(3000);
}

void readSensorData()
{
  static float soilLevelPercent = 0.0f;
  static float foodLevelPercent = 0.0f;
  static float moisturePercent = 0.0f;
  static float temperature = 0.0f;

  getTofRangeData();
  // float soilMoistureValue = analogRead(SOIL_MOISTURE_SENSOR_ANALOG_PIN);
  // Serial.printf("Soil Moisture Sensor Value: %d\n", static_cast<int>(soilMoistureValue));
  // Serial.printf("Soil Moisture Percentage: %.2f %%\n", map(soilMoistureValue, SOIL_MOISTURE_0_PERCENT, SOIL_MOISTURE_100_PERCENT, 0, 100));

  // Get soil level percentage
  soilLevelPercent = getSoilLevelPercent();
  // Get food level percentage
  foodLevelPercent = getFoodLevelPercent();
  // Get soil moisture percentage
  moisturePercent = getMoisturePercent();
  // Get temperature in Celsius
  temperature = getTemperature();
}

void getDataRoutine()
{
  lowerSensorProbe();
  delay(3000);
  readSensorData();
  delay(3000);
  raiseSensorProbe();
  delay(3000);
}

void addSoilRoutine()
{
  closeGate();
  currentState = ADDING_SOIL;
  while (currentNetWeight < targetSoilWeight)
  {
    lcdPrint("ADDING SOIL", 0);
    lcdPrint("Target: " + String(targetSoilWeight, 2) + " g", 1);
    lcdPrint("Current: " + String(currentNetWeight, 2) + " g", 2);

    turnOnSoilDispenser();
    delay(1000); // Dispense soil for 5 seconds
    turnOffSoilDispenser();
    delay(1000); // Wait for soil to settle

    currentNetWeight = getWeightGrams(10);
  }
  lcdController();
  turnOnSoilDispenser();
  delay(1000); // Dispense soil for 5 seconds
  turnOffSoilDispenser();
  delay(1000); // Dispense soil for 5 seconds

  // GET WEIGHT
  float weight = getWeightGrams(10);

  Serial.println("[SOIL DISPENSER] Soil dispensing routine completed.\n");
}

void serialPrintController()
{
  static unsigned long lastSerialPrintTime = 0;
  unsigned long currentTime = millis();

  if (currentTime - lastSerialPrintTime >= 1000) // Print every second
  {
    lastSerialPrintTime = currentTime;
    Serial.printf("[TOF] SOIL: %.0f mm [%.2f %%], FOOD: %.0f mm [%.2f %%]\n", soilRangeMm, soilPercentage, foodRangeMm, foodPercentage);
  }
}

void mainController()
{
  static MachineState lastLoggedState = IDLE;
  if (currentState != lastLoggedState)
  {
    Serial.printf("[STATE] %s -> %s at %lu ms.\n",
                  machineStateName(lastLoggedState), machineStateName(currentState), millis());
    lastLoggedState = currentState;
  }

  // This function can be used to manage the main operations of the machine based on its current state.
  switch (currentState)
  {
  case IDLE:
    // Handle idle state
    if (previousState != IDLE)
    {
      Serial.println("[STATE] Transitioned to IDLE state.");
    }

    // Food grinder door: detect open -> closed event
    if (isFoodCoverOpenedThenClosed)
    {
      Serial.println("[STATE] Food grinder door closed after being opened (detected).");
      isFoodCoverOpenedThenClosed = false; // Clear the event after handling it

      lcdPrint("", 1); // Clear any previous message (e.g. insufficient soil)
      lcdPrint("", 2); // Clear any previous weight
      currentState = CHECKING_FOOD_LEVEL;
      lcdController(); // previousState is still IDLE here, so the new state is drawn
      break;
    }

    // Soil hopper door: detect open -> closed event (soil refilled) and re-check the soil level
    if (isSoilCoverOpenedThenClosed)
    {
      Serial.println("[STATE] Soil hopper door closed after being opened (detected).");
      isSoilCoverOpenedThenClosed = false; // Clear the event after handling it

      lcdPrint("", 1); // Clear any previous message (e.g. insufficient soil)
      lcdPrint("", 2);
      // If the food level already passed, only the soil needs re-checking; otherwise start from the food check
      currentState = foodLevelSufficient ? CHECKING_SOIL_LEVEL : CHECKING_FOOD_LEVEL;
      lcdController();
      break;
    }

    previousState = IDLE;
    break;
  case CHECKING_FOOD_LEVEL:
  {
    float foodLevel = 0.0f;
    bool levelRead = readFoodLevelPercent(foodLevel);

    bool foodAbovePreset = false;
    if (levelRead)
    {
      foodAbovePreset = foodLevel > FOOD_LEVEL_PRESET_PERCENT;
      Serial.printf("[FOOD] Level: %.1f %% (preset: %.1f %%) -> %s\n",
                    foodLevel,
                    FOOD_LEVEL_PRESET_PERCENT,
                    foodAbovePreset ? "ABOVE preset" : "NOT above preset");
    }
    else
    {
      Serial.println("[FOOD] Unable to read food level.");
    }

    previousState = CHECKING_FOOD_LEVEL;
    foodLevelSufficient = foodAbovePreset;
    // Food above preset -> check soil level next, otherwise return to IDLE
    currentState = foodAbovePreset ? CHECKING_SOIL_LEVEL : IDLE;
    lcdController();
    break;
  }
  case CHECKING_SOIL_LEVEL:
  {
    float soilLevel = 0.0f;
    MachineState nextState = IDLE;
    if (readSoilLevelPercent(soilLevel))
    {
      bool soilAbovePreset = soilLevel > SOIL_LEVEL_PRESET_PERCENT;
      Serial.printf("[SOIL] Level: %.1f %% (preset: %.1f %%) -> %s\n",
                    soilLevel,
                    SOIL_LEVEL_PRESET_PERCENT,
                    soilAbovePreset ? "ABOVE preset" : "NOT above preset");

      if (soilAbovePreset)
      {
        Serial.println("[BATCH] Soil sufficient. Batch process start.");
        foodLevelSufficient = false; // Food will be consumed by this batch; require a fresh food check next time
        grindBaselineWeight = getWeightGrams(20);
        grindPulseCount = 0;
        currentNetWeight = 0.0f;
        Serial.printf("[GRIND] Baseline weight: %.1f g, target: %.1f g\n", grindBaselineWeight, GRIND_TARGET_WEIGHT_G);
        nextState = GRINDING;
      }
      else
      {
        Serial.println("[SOIL] Insufficient soil. Please refill the soil hopper.");
        lcdPrint("INSUFFICIENT SOIL", 1);
      }
    }
    else
    {
      Serial.println("[SOIL] Unable to read soil level.");
    }

    // Soil sufficient -> start grinding, otherwise return to IDLE
    previousState = CHECKING_SOIL_LEVEL;
    currentState = nextState;
    lcdController();
    break;
  }
  case GRINDING:
  {
    // Safety: stop if the food grinder door is opened while grinding
    if (!foodCoverState)
    {
      turnOffMeatGrinder();
      Serial.println("[GRIND] Food grinder door opened. Grinding aborted.");
      lcdPrint("GRIND ABORTED", 1);
      previousState = GRINDING;
      currentState = IDLE;
      lcdController();
      break;
    }

    if (!isMeatGrinderOn)
    {
      turnOnMeatGrinder();
      grindPulseStartTime = millis();
      ++grindPulseCount;
      Serial.printf("[GRIND] Pulse %u/%u started (%d ms).\n", grindPulseCount, GRIND_MAX_PULSES, GRIND_DURATION_MS);
    }
    else if (millis() - grindPulseStartTime >= GRIND_DURATION_MS)
    {
      turnOffMeatGrinder();
      weightSettleStartTime = millis();
      previousState = GRINDING;
      currentState = GETTING_WEIGHT;
      lcdController();
      break;
    }

    previousState = GRINDING;
    break;
  }
  case GETTING_WEIGHT:
  {
    if (millis() - weightSettleStartTime < GRIND_INTERVAL_MS)
    {
      previousState = GETTING_WEIGHT;
      break; // Let the scale settle after the grinder stops
    }

    currentNetWeight = getWeightGrams(20) - grindBaselineWeight;
    Serial.printf("[WEIGHT] Ground food: %.1f g (target: %.1f g)\n", currentNetWeight, GRIND_TARGET_WEIGHT_G);
    lcdPrint("Weight: " + String(currentNetWeight, 1) + " g", 2);

    previousState = GETTING_WEIGHT;
    if (currentNetWeight >= GRIND_TARGET_WEIGHT_G)
    {
      Serial.println("[GRIND] Target weight reached.");
      lcdPrint("GRIND DONE", 1);

      // Start adding soil on top of the ground food
      soilBaselineWeight = getWeightGrams(20);
      currentSoilWeight = 0.0f;
      soilPulseCount = 0;
      isSoilSettling = false;
      Serial.printf("[SOIL] Baseline weight: %.1f g, target: %.1f g\n", soilBaselineWeight, SOIL_TARGET_WEIGHT_G);
      lcdPrint("", 2);
      currentState = ADDING_SOIL;
    }
    else if (grindPulseCount >= GRIND_MAX_PULSES)
    {
      Serial.println("[GRIND] Max pulses reached before target weight. Stopping.");
      lcdPrint("GRIND TIMEOUT", 1);
      currentState = IDLE;
    }
    else
    {
      currentState = GRINDING; // Target not reached, grind again
    }
    lcdController();
    break;
  }
  case ADDING_SOIL:
  {
    // Safety: stop if the soil hopper door is opened while dispensing
    if (!soilCoverState)
    {
      turnOffSoilDispenser();
      isSoilSettling = false;
      Serial.println("[SOIL] Soil hopper door opened. Soil dispensing aborted.");
      lcdPrint("SOIL ABORTED", 1);
      previousState = ADDING_SOIL;
      currentState = IDLE;
      lcdController();
      break;
    }

    if (isSoilSettling)
    {
      if (millis() - soilSettleStartTime < SOIL_INTERVAL_MS)
      {
        previousState = ADDING_SOIL;
        break; // Let the scale settle after the dispenser stops
      }

      isSoilSettling = false;
      currentSoilWeight = getWeightGrams(20) - soilBaselineWeight;
      Serial.printf("[SOIL] Soil added: %.1f g (target: %.1f g)\n", currentSoilWeight, SOIL_TARGET_WEIGHT_G);
      lcdPrint("Soil: " + String(currentSoilWeight, 1) + " g", 2);

      if (currentSoilWeight >= SOIL_TARGET_WEIGHT_G)
      {
        Serial.println("[SOIL] Target weight reached.");
        lcdPrint("SOIL DONE", 1);
        previousState = ADDING_SOIL;
        currentState = MIXING;
        lcdController();
        break;
      }
      if (soilPulseCount >= SOIL_MAX_PULSES)
      {
        Serial.println("[SOIL] Max pulses reached before target weight. Stopping.");
        lcdPrint("SOIL TIMEOUT", 1);
        previousState = ADDING_SOIL;
        currentState = IDLE;
        lcdController();
        break;
      }
      // Target not reached, fall through to start another pulse on the next loop
      previousState = ADDING_SOIL;
      break;
    }

    if (!isSoilDispenserOn)
    {
      turnOnSoilDispenser();
      soilPulseStartTime = millis();
      ++soilPulseCount;
      Serial.printf("[SOIL] Pulse %u/%u started (%d ms).\n", soilPulseCount, SOIL_MAX_PULSES, SOIL_DURATION_MS);
    }
    else if (millis() - soilPulseStartTime >= SOIL_DURATION_MS)
    {
      turnOffSoilDispenser();
      soilSettleStartTime = millis();
      isSoilSettling = true;
    }

    previousState = ADDING_SOIL;
    break;
  }
  case MIXING:
  {
    Serial.printf("[MIX] Mixing state entered at %lu ms.\n", millis());
    previousState = MIXING; // LCD already shows MIXING, avoid redrawing it inside mix()
    mix();                  // Blocking: runs the mixer then returns it to the safe position
    Serial.printf("[MIX] Mixing state work finished at %lu ms; advancing to GETTING_DATA.\n", millis());
    lcdPrint("MIX DONE", 1);
    currentState = GETTING_DATA;
    lcdController();
    break;
  }
  case GETTING_DATA:
  {
    Serial.println("[DATA] Getting sensor data.");
    previousState = GETTING_DATA;
    lowerSensorProbe(); // Blocking: moves the probe into the mixture
    delay(SENSOR_SETTLE_MS);
    recordTemperature = getTemperature();
    recordMoisture = constrain(getMoisturePercent(), RECORD_MOISTURE_MIN, RECORD_MOISTURE_MAX);
    Serial.printf("[DATA] Temperature: %.1f C, Moisture: %.1f %%\n", recordTemperature, recordMoisture);
    raiseSensorProbe(); // Raise before the (blocking) database write

    lcdPrint("Temp: " + String(recordTemperature, 1) + " C", 1);
    lcdPrint("Moisture: " + String(recordMoisture, 1) + " %", 2);
    lcdPrint("WRITING DATABASE", 3);
    bool written = writeBatchRecord();
    lcdPrint(written ? "DATABASE OK" : "DATABASE FAILED", 3);

    // The mixture is dropped even if the database write failed
    currentState = DROPPING;
    lcdController();
    break;
  }
  case DROPPING:
  {
    Serial.println("[DROP] Dropping the mixture.");
    previousState = DROPPING;
    openGate();
    delay(DROP_SETTLE_MS);
    mixDrop(); // Blocking: turns the mixer to push the mixture out, then returns it to the safe position
    delay(DROP_SETTLE_MS);
    closeGate();
    Serial.println("[DROP] Drop done.");
    lcdPrint("DROP DONE", 1);
    currentState = IDLE;
    lcdController();
    break;
  }
  default:
    // Handle unknown state
    break;
  }
}

void setup()
{
  Serial.begin(115200);

  setupWifi();
  setupLcd();
  setupMcp23017();
  setupCovers();
  delay(3000);
  setupScale(); // Must be ready before setupSteppers() reads the weight
  setupSteppers();
  if (bootProceedToMixing)
  {
    Serial.println("[SCALE] Boot weight above mixing threshold. Boot tare skipped.");
  }
  else
  {
    tareScaleAtBoot(); // Boot weight is stored and subtracted from later readings
  }
  setupTofSensors();
  setupPt100();

  if (setupFirebase())
  {
    // writeBatchRecord();
    Serial.println("[FIREBASE] Firebase setup completed successfully.");
  }

  if (bootProceedToMixing)
  {
    Serial.printf("[BOOT] Weight is above %.1f g. Proceeding to mixing.\n", BOOT_MIXING_MIN_WEIGHT_G);
    currentState = MIXING;
    lcdController(); // previousState is still IDLE, so MIXING is drawn
  }

  // beep(150, 3, 100);
  // beep(300);

  // turnOnMeatGrinder();
  // delay(10000);
  // turnOffMeatGrinder();
  // delay(3000);

  // turnOnSoilDispenser();
  // delay(10000);
  // turnOffSoilDispenser();

  // getDataRoutine();
  // drop();
  // addSoilRoutine();
  // drop();
}

void loop()
{
  firebaseApp.loop();
  wifiManager.process();
  coverController();
  mainController();
  calibrateScale();
  // testScale();

  // TEST SENSORS
  // Serial.printf("Soil Moisture Sensor Value: %d\n", analogRead(SOIL_MOISTURE_SENSOR_ANALOG_PIN));
  // Serial.printf("Temperature: %.2f °C\n", getTemperature());
  // delay(1000);
  // return;

  // mix();
  // testScale();

  // delay(5000);
  // return;

  // testLimitSwitches();
  // getTofRangeData();
  // lcdController();
  // serialPrintController();
}
