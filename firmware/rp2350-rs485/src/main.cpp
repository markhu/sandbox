/**
 * RP2350 RS485 Communication Example
 *
 * This example demonstrates basic RS485 communication using the RP2350 microcontroller.
 *
 * Hardware Connections:
 * - TX (UART0 GP0) -> RS485 Module DI (Driver Input)
 * - RX (UART0 GP1) -> RS485 Module RO (Receiver Output)
 * - RTS (GP2) -> RS485 Module DE and RE pins
 *
 * RS485 is a differential serial communication standard that allows multiple
 * devices to communicate over long distances with high noise immunity.
 */

#include <Arduino.h>
#include <pico/unique_id.h>

// For Pico W/2W boards, we need to include LED support
#ifdef ARDUINO_RASPBERRY_PI_PICO_W
#include <WiFi.h>  // This includes CYW43 support
#define USE_WIFI_LED 1
#endif

// Build timestamp - automatically set at compile time
#define BUILD_TIMESTAMP __DATE__ " " __TIME__

// Global variable for formatted build ID
char buildID[13]; // yyyyMMddhhmm + null terminator

// Helper function to parse build timestamp into yyyyMMddhhmm format
void initBuildID() {
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char monthStr[4] = {__DATE__[0], __DATE__[1], __DATE__[2], '\0'};
    int month = 1;
    for (int i = 0; i < 12; i++) {
        if (strcmp(monthStr, months[i]) == 0) {
            month = i + 1;
            break;
        }
    }
    
    int year = (__DATE__[7] - '0') * 1000 + (__DATE__[8] - '0') * 100 + 
               (__DATE__[9] - '0') * 10 + (__DATE__[10] - '0');
    int day = ((__DATE__[4] == ' ' ? 0 : __DATE__[4] - '0') * 10) + (__DATE__[5] - '0');
    int hour = (__TIME__[0] - '0') * 10 + (__TIME__[1] - '0');
    int minute = (__TIME__[3] - '0') * 10 + (__TIME__[4] - '0');
    
    snprintf(buildID, sizeof(buildID), "%04d%02d%02d%02d%02d", year, month, day, hour, minute);
}

// UART Configuration
#define RS485_SERIAL Serial1  // Using UART0
#define RS485_BAUD 230400

// RS485 Control Pins (User's actual wiring)
#define RS485_TX_PIN 0    // UART0 TX (GP0)
#define RS485_RX_PIN 1    // UART0 RX (GP1)
#define RS485_DE_PIN 2    // RTS/Driver Enable / Receiver Enable (GP2)

// Onboard LED for visual feedback
#define LED_PIN LED_BUILTIN

// RS485 Communication Modes
#define RS485_RECEIVE_MODE LOW
#define RS485_TRANSMIT_MODE HIGH

// Function prototypes
void setupRS485();
void rs485Transmit(const char* message);
void rs485Receive();
void setRS485Mode(bool transmitMode);
void printDeviceInfo();

void setup() {
    // Initialize build ID first
    initBuildID();
    
    // Initialize USB Serial for debugging first
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        ; // Wait for serial port to connect or timeout after 3 seconds
    }
    
    Serial.println();
    Serial.println("========================================");
    Serial.println("RP2350 RS485 Communication");
    Serial.println("========================================");
    
    // Print device identification
    printDeviceInfo();
    
    Serial.println("========================================");
    Serial.println();
    
    // Initialize onboard LED for visual feedback
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);  // Turn on LED to show firmware is running
    Serial.println("Onboard LED: ON");

    // Initialize RS485
    setupRS485();
    Serial.println("RS485: Initialized");

    Serial.println();
    Serial.println("=== Interactive Mode ===");
    Serial.println("Type any message and press Enter to send via RS485");
    Serial.println("AUTO: Auto-messages sent every 2 seconds");
    Serial.println(">>> YOU SENT: Your typed messages");
    Serial.println("RECEIVED: Messages from other RS485 devices");
    Serial.println("========================");
    Serial.println();
}

void loop() {
    static unsigned long lastTransmitTime = 0;
    const unsigned long transmitInterval = 2000; // Transmit every 2 seconds
    static char deviceName[16] = {0};
    
    // Get device name on first run
    if (deviceName[0] == 0) {
        pico_unique_board_id_t board_id;
        pico_get_unique_board_id(&board_id);
        snprintf(deviceName, sizeof(deviceName), "RP2350-%02X%02X", 
                 board_id.id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES-2],
                 board_id.id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES-1]);
    }

    // Check for commands from USB Serial
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command.length() > 0) {
            // Blink LED to show activity
            digitalWrite(LED_PIN, LOW);
            delay(100);
            digitalWrite(LED_PIN, HIGH);
            
            // Transmit the user's message via RS485
            char message[128];
            snprintf(message, sizeof(message), "[%s|%s] %s", deviceName, buildID, command.c_str());
            rs485Transmit(message);
            Serial.print(">>> YOU SENT: ");
            Serial.println(command);
        }
    }

    // Periodic transmission example
    if (millis() - lastTransmitTime >= transmitInterval) {
        lastTransmitTime = millis();

        // Blink LED to show activity
        digitalWrite(LED_PIN, LOW);   // Turn LED off
        delay(100);                   // Short blink
        digitalWrite(LED_PIN, HIGH);  // Turn LED back on

        // Prepare message with device ID and build timestamp
        char message[128];
        snprintf(message, sizeof(message), "[%s|%s] Uptime: %lu ms", deviceName, buildID, millis());

        // Transmit via RS485
        rs485Transmit(message);
        Serial.print("AUTO: ");
        Serial.println(message);
    }

    // Check for incoming RS485 data
    rs485Receive();

    delay(10); // Small delay to prevent tight loop
}

/**
 * Initialize RS485 hardware and pins
 */
void setupRS485() {
    // Configure DE/RE control pin
    pinMode(RS485_DE_PIN, OUTPUT);
    setRS485Mode(false); // Start in receive mode

    // Initialize UART1 for RS485 communication
    // Note: With Arduino Mbed framework, UART1 uses fixed pins GP0 (TX) and GP1 (RX)
    // Pin configuration is handled automatically by the framework
    RS485_SERIAL.begin(RS485_BAUD);

    // Clear any pending data
    while (RS485_SERIAL.available()) {
        RS485_SERIAL.read();
    }
}

/**
 * Transmit data via RS485
 * @param message - Null-terminated string to transmit
 */
void rs485Transmit(const char* message) {
    // Switch to transmit mode
    setRS485Mode(true);
    delayMicroseconds(10); // Small delay for transceiver to switch modes

    // Send the message
    RS485_SERIAL.println(message);

    // Wait for transmission to complete
    RS485_SERIAL.flush();
    delayMicroseconds(10);

    // Switch back to receive mode
    setRS485Mode(false);
}

/**
 * Receive and process RS485 data
 */
void rs485Receive() {
    if (RS485_SERIAL.available() > 0) {
        String receivedData = RS485_SERIAL.readStringUntil('\n');
        receivedData.trim();

        if (receivedData.length() > 0) {
            Serial.print("RECEIVED: ");
            Serial.println(receivedData);
        }
    }
}

/**
 * Set RS485 transceiver mode
 * @param transmitMode - true for transmit mode, false for receive mode
 */
void setRS485Mode(bool transmitMode) {
    if (transmitMode) {
        digitalWrite(RS485_DE_PIN, RS485_TRANSMIT_MODE);
    } else {
        digitalWrite(RS485_DE_PIN, RS485_RECEIVE_MODE);
    }
}

/**
 * Print device identification information
 */
void printDeviceInfo() {
    // Get the unique device ID (8 bytes for RP2350)
    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);
    
    // Print build timestamp
    Serial.print("Build Time: ");
    Serial.print(BUILD_TIMESTAMP);
    Serial.print(" (");
    Serial.print(buildID);
    Serial.println(")");
    
    // Print unique board ID
    Serial.print("Device ID:  ");
    for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
        if (board_id.id[i] < 0x10) Serial.print("0");
        Serial.print(board_id.id[i], HEX);
        if (i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES - 1) Serial.print(":");
    }
    Serial.println();
    
    // Create a short device name from last 2 bytes of ID
    char deviceName[16];
    snprintf(deviceName, sizeof(deviceName), "RP2350-%02X%02X", 
             board_id.id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES-2],
             board_id.id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES-1]);
    Serial.print("Device Name: ");
    Serial.println(deviceName);
}
