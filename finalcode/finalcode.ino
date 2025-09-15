/*

    Reads the value of a QDY30A level sensor and sends the data to ThingSpeak.

    Mathias Van Nuland 2025
    Last updated on 15/09/2025

*/


#include <credentials.h>   // Credentials that are not uploaded to GitHub
#include <WiFi.h>          // Wifi library
#include <ThingSpeak.h>    // Communicating with ThingSpeak
#include <ModbusMaster.h>  // Communicate with the Modbus RS485 protocol https://github.com/4-20ma/ModbusMaster/tree/master
#include <TelnetStream.h>  // Use PuTTY to view the logs, last IP adress = 192.168.1.46

/* -------------------- SENSOR -------------------- */

#define FLOWCONTROLPIN 2  // RE and DE pins on MAX485

ModbusMaster node;  // Create Modbus object

// What should happen before transmission
void preTransmission() {
    digitalWrite(FLOWCONTROLPIN, HIGH);  // Enable writing
}

// What should happen after transmission
void postTransmission() {
    digitalWrite(FLOWCONTROLPIN, LOW);  // Enable reading
}

/* -------------------- WIFI AND THINGSPEAK -------------------- */

WiFiClient client;

// Replace with your network credentials (STATION)
const char* ssid = mySSID;
const char* password = myPASSWORD;

// Keep track when connected (or not) to the WiFi
const unsigned long CHECK_WIFI_INTERVAL = 15000;
unsigned long WiFiPreviousMillis = 0;

// ThingSpeak sends a code to say if the upload was successful or not.
int thingSpeakStatus;
unsigned long channelNumber = myChannelNumber;  // Channel number of my thingSpeak fields.
const char* writeAPIKey = myWriteAPIKey;        // ThingSpeak write API key

// Keep track of when last data sent to ThingSpeak
const unsigned long SEND_DATA_INTERVAL = 3 * 1000 * 60;  // 3 min
unsigned long dataPreviousMillis = -SEND_DATA_INTERVAL;  // Send distance on startup for quick testing

void initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print('.');
        delay(500);
    }
    Serial.print('\n');
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void reconnectToWiFi() {
    TelnetStream.println("Disconnecting WiFi...");
    WiFi.disconnect();
    TelnetStream.println("Reconnecting to WiFi...");
    WiFi.reconnect();
    TelnetStream.println("Reconnected to WiFi.");
}

// Gets the height from the sensor. -1.0 is error
float getHeight() {
    uint8_t result;  // Result error/success codes
    uint16_t data;   // Sensor data
    float height = -1.0;

    // Read one register, starting from 0x0004
    result = node.readHoldingRegisters(0x0004, 1);

    if (result == node.ku8MBSuccess) {  // Successfully read data
        data = node.getResponseBuffer(0x00);
        height = data / 10.0;  // convert to cm

        TelnetStream.print("Data at adress ");
        TelnetStream.print(0x0004);
        TelnetStream.print(": ");
        TelnetStream.println(data);  // Print the raw data

        TelnetStream.print("Converted height: ");
        TelnetStream.print(height, 1);  // Print the height in cm with one digit after the decimal point, as is defined in the datasheet
        TelnetStream.println("cm");
    } else {  // Some kind of error happened.
        if (result == node.ku8MBInvalidSlaveID) {
            TelnetStream.println("Invalid slave ID.");
        } else if (result == node.ku8MBInvalidFunction) {
            TelnetStream.println("Invalid Modbus function code.");
        } else if (result == node.ku8MBResponseTimedOut) {
            TelnetStream.println("Sensor response timed out.");
        } else if (result == node.ku8MBInvalidCRC) {
            TelnetStream.println("Invalid CRC.");
        } else {
            TelnetStream.println("Unknown sensor error.");
        }
    }

    return height;
}

bool sendHeight(float h) {
    // Preparing to upload data to ThingSpeak
    TelnetStream.println("Uploading data to ThingSpeak...");
    ThingSpeak.setField(1, h);

    // Upload data to ThingSpeak
    thingSpeakStatus = ThingSpeak.writeFields(channelNumber, writeAPIKey);

    // Handle thingSpeak errors. For more information: visit https://github.com/mathworks/thingspeak-arduino
    if (thingSpeakStatus == 200) {
        TelnetStream.println("Successful upload to ThingSpeak.");
        return true;
    } else {
        TelnetStream.print("Error updating ThingSpeak channels. Error code: ");
        TelnetStream.println(thingSpeakStatus);
        if (thingSpeakStatus == -301) {
            TelnetStream.println("Failed to connect to ThingSpeak.");
        } else if (thingSpeakStatus == -302) {
            TelnetStream.println("Unexpected failure during write to ThingSpeak.");
        } else if (thingSpeakStatus == -303) {
            TelnetStream.println("Unable to parse response.");
        } else if (thingSpeakStatus == -304) {
            TelnetStream.println("Timeout waiting for server to respond.");
        } else if (thingSpeakStatus == -401) {
            TelnetStream.println("Point was not inserted (most probable cause is the rate limit of once every 15 seconds).");
        } else if (thingSpeakStatus == 400) {
            TelnetStream.println("Error: thingSpeakStatus == 400");
        } else {
            TelnetStream.println("Unknown error.");
        }
        return false;
    }
}

/* -------------------- SETUP -------------------- */

void setup() {
    pinMode(FLOWCONTROLPIN, OUTPUT);    // RE and DE pins on MAX485
    digitalWrite(FLOWCONTROLPIN, LOW);  // Start in listening mode so that setup Serial prints do not interfere with the sensor

    Serial.begin(9600);                       // Communication with Modbus sensor (baudrate from datasheet) and print ip adress of esp32 for logging with PuTTY later
    node.begin(1, Serial);                    // Start Modbus sensor with adress 1
    node.preTransmission(preTransmission);    // Ensure the FLOWCONTROL pin is correct for transmission
    node.postTransmission(postTransmission);  // Ensure the FLOWCONTROL pin is correct for receiving

    // Connect to WiFi
    initWiFi();

    // Start logging, connect with PuTTY to the ip adress
    TelnetStream.begin();

    // Start the thingSpeak object.
    ThingSpeak.begin(client);
    TelnetStream.println("Started ThingSpeak object.");

    TelnetStream.println("End setup.");
}

/* -------------------- LOOP -------------------- */

void loop() {
    // if WiFi is down, try reconnecting every CHECK_WIFI_INTERVAL milliseconds
    unsigned long currentMillis = millis();
    if ((WiFi.status() != WL_CONNECTED) && (currentMillis - WiFiPreviousMillis >= CHECK_WIFI_INTERVAL)) {
        WiFiPreviousMillis = currentMillis;
        TelnetStream.print("Connection lost. Current millis: ");
        TelnetStream.println(currentMillis);
        TelnetStream.println("Reconnecting to WiFi...");
        reconnectToWiFi();
    }

    // Only send data every SEND_DATA_INTERVAL milliseconds
    currentMillis = millis();
    if (WiFi.status() == WL_CONNECTED && (currentMillis - dataPreviousMillis >= SEND_DATA_INTERVAL)) {
        dataPreviousMillis = currentMillis;
        float height = getHeight();  // Get height from sensor
        TelnetStream.print("Height: ");
        TelnetStream.print(height, 1);
        TelnetStream.println("cm");

        if (height > 200.0) {  // Sensor is dry
            TelnetStream.println("Height > 200.0");
            TelnetStream.println("Dry sensor, changing height to 0...");
            height = 0;
        }

        if (height == -1.0) {  // Sensor error. Check wiring!
            TelnetStream.println("Height == -1.0");
            TelnetStream.println("Not attempting upload to ThingSpeak due to error with sensor. Check wiring!");
        } else {
            // Try to send distance to thingspeak, else reconnect to WiFi
            if (sendHeight(height)) {
                TelnetStream.print("Height: ");
                TelnetStream.print(height);
                TelnetStream.println(" uploaded to ThingSpeak server.");
            } else {  // Reconnect to WiFi
                TelnetStream.println("Sending data to ThingSpeak servers failed. Reconnecting to WiFi...");
                reconnectToWiFi();
            }
        }
    }
}
