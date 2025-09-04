#include <ModbusMaster.h>
#include "BluetoothSerial.h"  // Use the "Serial Bluetooth" app to view the serial output

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

String device_name = "LogWaterput"; // Connect to this Bluetooth device on your phone

// Check if Bluetooth is available
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

// Check Serial Port Profile
#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Port Profile for Bluetooth is not available or not enabled. It is only available for the ESP32 chip.
#endif

// Create BluetoothSerial object
BluetoothSerial SerialBT;

void setup() {
    pinMode(FLOWCONTROLPIN, OUTPUT);
    digitalWrite(FLOWCONTROLPIN, LOW); // Start in listening mode

    Serial.begin(9600);  // Communication with Modbus sensor

    node.begin(1, Serial);                    // Start Modbus sensor with adress 1
    node.preTransmission(preTransmission);    // Ensure the FLOWCONTROl pin is correct for transmission
    node.postTransmission(postTransmission);  // Ensure the FLOWCONTROl pin is correct for receiving

    SerialBT.begin(device_name);  //Bluetooth device name
                                  // SerialBT.deleteAllBondedDevices(); // Uncomment this to delete paired devices; Must be called after begin
}

void loop() {
    uint8_t result;  // Result error/success codes
    uint16_t data[1];

    // Read one register, starting from 0x0004
    result = node.readHoldingRegisters(0x0004, 1);

    if (result == node.ku8MBSuccess) {
        data[0] = node.getResponseBuffer(0x00);
        SerialBT.print("Data at adress ");
        SerialBT.print(0x0004, HEX);
        SerialBT.print(": ");
        SerialBT.print(data[0] / 10.0f, 1);  // Print the data in cm with one digit after the decimal point, as is defined in the datasheet
        SerialBT.println("cm");
    } else if (result == node.ku8MBInvalidSlaveID) {
        SerialBT.println("Invalid slave ID.");
    } else if (result == node.ku8MBInvalidFunction) {
        SerialBT.println("Invalid Modbus function code.");
    } else if (result == node.ku8MBResponseTimedOut) {
        SerialBT.println("Response timed out.");
    } else if (result == node.ku8MBInvalidCRC) {
        SerialBT.println("Invalid CRC.");
    } else {
        SerialBT.println("Unknown error.");
    }

    delay(2000);
}
