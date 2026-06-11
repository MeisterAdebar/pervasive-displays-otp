/**
 * Pervasive Displays ESL 5.81" — OTP Memory Reader
 * Reads 128 bytes of OTP data via bidirectional 4-line software SPI (command 0xB9)
 *
 * Tested panel revision:
 *   Chip ID  (OTP[0x00]): 0x10
 *   COG Type (OTP[0x01]): 0x63
 *   Waveform (OTP[0x04]): "VRP023" (ASCII)
 *   Panel ID (OTP[0x68]): "S12581HS0B1023" (ASCII)
 *
 * Note: These IDs differ from the Pervasive Displays reference documentation
 * (Chip ID 0x16, COG Type 0x95). This appears to be a different COG revision,
 * possibly OEM-specific. The OTP read itself works correctly.
 */

// Pin assignment — adjust to match your wiring
#define PIN_DC    17
#define PIN_BUSY   4
#define PIN_MOSI  23  // SDIO / SDA line (bidirectional!)
#define PIN_CLK   18  // SCL / SCK
#define PIN_CS    15
#define PIN_RST   16

uint8_t otp_buffer[128];

// Send one byte via software SPI (output mode, MSB first)
void _writeSPIByte(uint8_t value) {
    for (int i = 0; i < 8; i++) {
        digitalWrite(PIN_MOSI, (value & 0x80) ? HIGH : LOW);
        value <<= 1;

        digitalWrite(PIN_CLK, HIGH);
        delayMicroseconds(2); // ~250 kHz, safe for all revisions
        digitalWrite(PIN_CLK, LOW);
        delayMicroseconds(2);
    }
}

// Read one byte via software SPI (input mode, MSB first)
uint8_t _readSPIByte() {
    uint8_t result = 0;
    for (int i = 0; i < 8; i++) {
        digitalWrite(PIN_CLK, HIGH);
        delayMicroseconds(2);

        result <<= 1;
        if (digitalRead(PIN_MOSI) == HIGH) {
            result |= 0x01;
        }

        digitalWrite(PIN_CLK, LOW);
        delayMicroseconds(2);
    }
    return result;
}

// Print a region of the OTP buffer as ASCII (non-printable bytes shown as '.')
void printASCIIRange(int start, int len) {
    for (int i = start; i < start + len; i++) {
        uint8_t b = otp_buffer[i];
        Serial.print((b >= 0x20 && b < 0x7F) ? (char)b : '.');
    }
}

void performOTPRead() {
    Serial.println("\n==================================================");
    Serial.println("[Step 2] COG driver reset...");

    // Reset sequence per datasheet timing
    digitalWrite(PIN_CS, HIGH);
    digitalWrite(PIN_DC, HIGH);
    digitalWrite(PIN_RST, HIGH); delay(200); // stabilise power
    digitalWrite(PIN_RST, LOW);  delay(4);   // assert reset
    digitalWrite(PIN_RST, HIGH); delay(20);  // wait for wake-up

    Serial.println("[Step 3] Triggering OTP read...");

    // 1. Enter command mode
    digitalWrite(PIN_DC, LOW);
    digitalWrite(PIN_CS, LOW);
    delayMicroseconds(2);

    // 2. Send command 0xB9
    pinMode(PIN_MOSI, OUTPUT);
    _writeSPIByte(0xB9);
    delayMicroseconds(2);

    // 3. Switch to data mode
    digitalWrite(PIN_DC, HIGH);
    delayMicroseconds(2);

    // 4. Switch SDIO to input — display will now drive the line
    pinMode(PIN_MOSI, INPUT);
    delayMicroseconds(2);

    // 5. Read and discard dummy byte (required per datasheet)
    uint8_t dummy = _readSPIByte();
    Serial.printf("  -> Dummy byte: 0x%02X (discarded)\n", dummy);

    // 6. Read 128 OTP bytes
    for (int i = 0; i < 128; i++) {
        otp_buffer[i] = _readSPIByte();
    }

    // 7. Release CS, restore SDIO to output
    digitalWrite(PIN_CS, HIGH);
    pinMode(PIN_MOSI, OUTPUT);

    // --- RAW DUMP ---
    Serial.println("\n==================================================");
    Serial.println("                   OTP MEMORY DUMP               ");
    Serial.println("==================================================");

    for (int i = 0; i < 128; i++) {
        Serial.printf("0x%02X ", otp_buffer[i]);
        if ((i + 1) % 16 == 0) {
            Serial.printf("   [Bytes %02d-%02d]\n", i - 15, i);
        }
    }
    Serial.println("==================================================");

    // --- INTERPRETATION ---
    Serial.println("\n--- Panel identification ---");
    Serial.printf("Chip ID  (OTP[0x00]): 0x%02X\n", otp_buffer[0x00]);
    Serial.printf("COG Type (OTP[0x01]): 0x%02X\n", otp_buffer[0x01]);

    Serial.print("Waveform (OTP[0x04]): \"");
    printASCIIRange(0x04, 6);
    Serial.println("\"  (likely Voltage Reference Profile)");

    Serial.print("Panel ID (OTP[0x68]): \"");
    printASCIIRange(0x68, 14);
    Serial.println("\"  (part number / lot code)");

    // --- RESULT ---
    // Only meaningful check: did we actually receive data?
    Serial.println("\n--- Result ---");
    bool hasData = (otp_buffer[0x00] != 0x00 && otp_buffer[0x00] != 0xFF);

    if (hasData) {
        Serial.println("[OK] OTP read successful. See Chip ID and COG Type above.");
    } else {
        Serial.println("[FAIL] No response — line is stuck LOW or HIGH.");
        Serial.println("       Check solder joints on the MOSI/SDA pin.");
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }

    Serial.println("\n=== Pervasive Displays ESL 5.81\" — OTP Reader ===");

    // All pins as plain GPIOs (no hardware SPI)
    pinMode(PIN_DC,   OUTPUT);
    pinMode(PIN_RST,  OUTPUT);
    pinMode(PIN_CLK,  OUTPUT);
    pinMode(PIN_CS,   OUTPUT);
    pinMode(PIN_MOSI, OUTPUT);
    pinMode(PIN_BUSY, INPUT);

    // Initial state: all lines low per datasheet
    digitalWrite(PIN_CS,   LOW);
    digitalWrite(PIN_DC,   LOW);
    digitalWrite(PIN_RST,  LOW);
    digitalWrite(PIN_CLK,  LOW);
    digitalWrite(PIN_MOSI, LOW);
    delay(100);

    performOTPRead();
}

void loop() {
    // Press the ESP32 reset button to repeat the read
    delay(1000);
}
