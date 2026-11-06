/**
 * Simple Firebase write test using the working await-style API from FirebaseClient.
 *
 * IMPORTANT:
 * This uses the exact app initialization pattern from the example and a
 * minimal retry loop. The writes are sent to fixed locations (/test/int,
 * /test/float, /test/bool), so the value is updated in place and not pushed as
 * a new child node.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <Wire.h>
#include <MCP23017.h>

#define ENABLE_USER_AUTH
#define ENABLE_DATABASE
#include <FirebaseClient.h>
#include "ExampleFunctions.h"
#include "Adafruit_MAX31865.h"
#include "Adafruit_VL53L0X.h"

#include "BuzzerHelper.h"
#include "Config.h"
#include "Defines.h"
#include "Constants.h"
#include "SoilMoistureSensor.h"

BuzzerHelper buzzer(BUZZER_PIN);
Adafruit_MAX31865 temperatureSensor(MAX31865_CS_PIN);
Adafruit_VL53L0X tofSensor = Adafruit_VL53L0X();
SoilMoistureSensor soilMoisture(MOISTURE_SENSOR_ANALOG_PIN, 800, 300);
MCP23017 mcpInputs(MCP_INPUTS_ADDRESS);
MCP23017 mcpOutputs(MCP_OUTPUTS_ADDRESS);

bool initializeMcp23017()
{
  Wire.begin();

  if (!mcpInputs.begin(false))
  {
    Serial.println("Failed to initialize MCP23017 inputs");
    return false;
  }
  else
  {
    Serial.println("MCP23017 for inputs initialized");
  }

  // if (!mcpOutputs.begin(false))
  // {
  //   Serial.println("Failed to initialize MCP23017 outputs");
  //   return false;
  // }

  for (uint8_t pin = 0; pin < 16; ++pin)
  {
    mcpInputs.pinMode1(pin, INPUT);
    mcpOutputs.pinMode1(pin, OUTPUT);
    mcpOutputs.write1(pin, LOW);
  }

  Serial.println("MCP23017 initialized");
  return true;
}

void setMcpOutput(uint8_t expanderPin, bool high)
{
  if (expanderPin >= 16)
    return;

  mcpOutputs.write1(expanderPin, high ? HIGH : LOW);
}

bool readMcpInput(uint8_t expanderPin)
{
  if (expanderPin >= 16)
    return false;

  return mcpInputs.read1(expanderPin) == HIGH;
}

SSL_CLIENT ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);

UserAuth user_auth(FIREBASE_WEB_API_KEY, FIREBASE_USER_EMAIL, FIREBASE_USER_PASSWORD, 3000);
FirebaseApp app;
RealtimeDatabase Database;

class FirebaseWriter
{
public:
  typedef void (*FailedAttemptCallback)();
  typedef void (*FailedWriteCallback)();

  FirebaseWriter(uint8_t maxAttempts = 3)
      : maxAttempts_(maxAttempts == 0 ? 1 : maxAttempts),
        failedAttemptCallback_(nullptr),
        failedWriteCallback_(nullptr)
  {
  }

  void begin()
  {
    set_ssl_client_insecure_and_buffer(ssl_client);
    initializeApp(aClient, app, getAuth(user_auth), nullptr, "🔐 authTask");
    app.getApp<RealtimeDatabase>(Database);
    Database.url(FIREBASE_DATABASE_URL);
  }

  void loop()
  {
    app.loop();
  }

  void setFailedAttemptCallback(FailedAttemptCallback callback)
  {
    failedAttemptCallback_ = callback;
  }

  void setFailedWriteCallback(FailedWriteCallback callback)
  {
    failedWriteCallback_ = callback;
  }

  bool writeInt(const String &path, int value)
  {
    return writeWithRetry(path, value, false, 0.0f, false);
  }

  bool writeFloat(const String &path, float value)
  {
    return writeWithRetry(path, 0, true, value, false);
  }

  bool writeBool(const String &path, bool value)
  {
    return writeWithRetry(path, 0, false, 0.0f, value);
  }

  bool writeString(const String &path, const String &value)
  {
    return writeStringWithRetry(path, value);
  }

private:
  bool writeWithRetry(const String &path, int intValue, bool isFloat, float floatValue, bool boolValue)
  {
    for (uint8_t attempt = 1; attempt <= maxAttempts_; ++attempt)
    {
      bool ok = false;

      if (isFloat)
      {
        ok = Database.set<number_t>(aClient, path, number_t(floatValue, 2));
      }
      else if (boolValue)
      {
        ok = Database.set<bool>(aClient, path, boolValue);
      }
      else
      {
        ok = Database.set<int>(aClient, path, intValue);
      }

      if (ok)
        return true;

      if (failedAttemptCallback_ != nullptr && attempt < maxAttempts_)
        failedAttemptCallback_();
    }

    if (failedWriteCallback_ != nullptr)
      failedWriteCallback_();

    return false;
  }

  bool writeStringWithRetry(const String &path, const String &value)
  {
    for (uint8_t attempt = 1; attempt <= maxAttempts_; ++attempt)
    {
      bool ok = Database.set<String>(aClient, path, value);

      if (ok)
        return true;

      if (failedAttemptCallback_ != nullptr && attempt < maxAttempts_)
        failedAttemptCallback_();
    }

    if (failedWriteCallback_ != nullptr)
      failedWriteCallback_();

    return false;
  }

  uint8_t maxAttempts_;
  FailedAttemptCallback failedAttemptCallback_;
  FailedWriteCallback failedWriteCallback_;
};

void onFailedAttempt()
{
  buzzer.failedAttempt();
}

void onFailedWrite()
{
  buzzer.failedWrite();
}

FirebaseWriter firebaseWriter(3);
bool firebaseWriteTriggered = false;

float getTofDistanceCm()
{
  if (!tofSensor.isRangeComplete())
  {
    return -1.0f;
  }

  uint16_t distanceMm = tofSensor.readRange();
  if (distanceMm == 0 || distanceMm == 65535)
  {
    return -1.0f;
  }

  return distanceMm / 10.0f;
}

int getMoisturePercent()
{
  return soilMoisture.readMoisture();
}

float getTemperatureCelsius()
{
  return temperatureSensor.temperature(RNOMINAL, RREF);
}

String makeBatchPath(const String &batchNumber)
{
  return "/records/" + batchNumber;
}

String nextBatchCode(const String &latestBatch)
{
  if (latestBatch == "EC-000" || latestBatch.length() == 0 || !latestBatch.startsWith("EC-") || latestBatch.length() <= 3)
    return "EC-001";

  String numberText = latestBatch.substring(3);
  int batchNumber = numberText.toInt();

  if (batchNumber <= 0)
    return "EC-001";

  int nextNumber = batchNumber + 1;
  char padded[8];
  snprintf(padded, sizeof(padded), "EC-%03d", nextNumber);
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
  String recordsJson = Database.get<String>(aClient, "/records");
  // Serial.print("DEBUG /records RAW: ");
  // Serial.println(recordsJson);

  if (recordsJson.length() == 0 || recordsJson == "null" || recordsJson == "{}" || recordsJson == "[]")
  {
    Serial.println("DEBUG /records empty => EC-000");
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
        String batchPath = "/records/" + token;
        String batchValue = Database.get<String>(aClient, batchPath);

        // Serial.print("DEBUG candidate key: ");
        // Serial.print(token);
        // Serial.print(" -> number=");
        // Serial.print(batchNumber);
        // Serial.print(" ; value=");
        // Serial.println(batchValue);

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
          Serial.print("DEBUG skip invalid record: ");
          Serial.println(token);
        }
      }
    }

    index = quoteEnd + 1;
  }

  Serial.print("DEBUG latest valid batch: ");
  Serial.println(latestBatch);
  return latestBatch;
}

String getFirebaseTimestampSeconds()
{
  time_t now = time(nullptr);
  if (now <= 0)
  {
    Serial.println("DEBUG timestamp fallback => using millis()/1000");
    return String((unsigned long)(millis() / 1000UL));
  }

  return String((unsigned long)now);
}

String makeRecordCsv(const String &batchCode, unsigned long timestamp, float temperature, float humidity)
{
  return batchCode + "," + String(timestamp) + "," + String(temperature, 1) + "," + String(humidity, 1);
}

String formatEpochDate(unsigned long epochSeconds)
{
  time_t t = (time_t)epochSeconds;
  struct tm timeinfo;
  localtime_r(&t, &timeinfo);

  char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeinfo);
  return String(buffer);
}

String formatEpochTime(unsigned long epochSeconds)
{
  time_t t = (time_t)epochSeconds;
  struct tm timeinfo;
  localtime_r(&t, &timeinfo);

  char buffer[32];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
  return String(buffer);
}

void prettyPrintRecord(const String &dataCsv)
{
  if (dataCsv.length() == 0)
  {
    Serial.println("No CSV record available.");
    return;
  }

  int firstComma = dataCsv.indexOf(',');
  int secondComma = firstComma >= 0 ? dataCsv.indexOf(',', firstComma + 1) : -1;
  int thirdComma = secondComma >= 0 ? dataCsv.indexOf(',', secondComma + 1) : -1;

  if (firstComma < 0 || secondComma < 0 || thirdComma < 0)
  {
    Serial.print("CSV record: ");
    Serial.println(dataCsv);
    return;
  }

  String batchNumber = dataCsv.substring(0, firstComma);
  String timestampText = dataCsv.substring(firstComma + 1, secondComma);
  String temperatureText = dataCsv.substring(secondComma + 1, thirdComma);
  String moistureText = dataCsv.substring(thirdComma + 1);

  unsigned long epochSeconds = timestampText.toInt();
  float temperature = temperatureText.toFloat();
  float moisture = moistureText.toFloat();

  Serial.println("_________________________");
  Serial.println("Data");
  Serial.print("Batch Number: ");
  Serial.println(batchNumber);
  Serial.print("Date: ");
  Serial.println(formatEpochDate(epochSeconds));
  Serial.print("Time: ");
  Serial.println(formatEpochTime(epochSeconds));
  Serial.print("Temperature: ");
  Serial.print(temperature, 1);
  Serial.println(" C");
  Serial.print("Moisture: ");
  Serial.print(moisture, 1);
  Serial.println(" %");
  Serial.println("_________________________");
}

void writeTestBatchRecord()
{
  String latestBatch = getLatestBatchNodeName();
  String batchCode = nextBatchCode(latestBatch);
  String recordPath = makeBatchPath(batchCode);

  unsigned long epochTime = getFirebaseTimestampSeconds().toInt();
  float temperature = 29.4f;
  float humidity = 58.0f;

  String csvRecord = makeRecordCsv(batchCode, epochTime, temperature, humidity);

  Serial.print("Writing CSV batch record: ");
  Serial.println(csvRecord);
  prettyPrintRecord(csvRecord);
  Serial.print("Record path: ");
  Serial.println(recordPath);

  firebaseWriter.writeString(recordPath, csvRecord);

  Serial.println("Batch record string write complete.");
}

void setup()
{
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Waiting for NTP time sync...");
  time_t now = time(nullptr);
  while (now < 1700000000L)
  {
    delay(500);
    now = time(nullptr);
  }

  Serial.print("NTP synced: ");
  Serial.print(formatEpochDate((unsigned long)now));
  Serial.print(" ");
  Serial.println(formatEpochTime((unsigned long)now));

  buzzer.begin();
  initializeMcp23017();

  firebaseWriter.begin();
  firebaseWriter.setFailedAttemptCallback(onFailedAttempt);
  firebaseWriter.setFailedWriteCallback(onFailedWrite);

  Serial.println("------------------------------");
  Serial.println("Firebase write test");
  Serial.println("------------------------------");

  // temperatureSensor.begin(MAX31865_3WIRE);

  // if (!tofSensor.begin())
  // {
  //   Serial.println(F("Failed to boot VL53L0X"));
  // }
  // else
  // {
  //   Serial.println(F("VL53L0X initialized"));
  //   tofSensor.startRangeContinuous();
  // }

  // pinMode(MOISTURE_SENSOR_ANALOG_PIN, INPUT);
  // Serial.println("Soil moisture sensor initialized");
}

void loop()
{

  return;

  buzzer.update();
  firebaseWriter.loop();

  if (app.ready() && !firebaseWriteTriggered)
  {
    firebaseWriteTriggered = true;
    writeTestBatchRecord();

    String latestBatch = getLatestBatchNodeName();
    Serial.print("Latest batch node: ");
    Serial.println(latestBatch);

    String recordPath = makeBatchPath(latestBatch);
    String recordCsv = Database.get<String>(aClient, recordPath);

    Serial.print("Batch path: ");
    Serial.println(recordPath);
    Serial.print("CSV record: ");
    Serial.println(recordCsv);
    prettyPrintRecord(recordCsv);
  }
}
