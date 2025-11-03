# RFID Verification System

A comprehensive RFID-based access control and verification system built on the LPC1768 microcontroller platform. This system enables user registration and verification through RFID cards using the MFRC522 reader module.

## Overview

The RFID Verification System provides a secure authentication mechanism using contactless RFID technology. The system supports registration of up to 20 unique RFID cards and verifies them against the stored database. User interaction is facilitated through a 4x3 matrix keypad and 16x2 LCD display.

## Features

- **RFID Card Registration**: Register new RFID cards into the system database
- **RFID Card Verification**: Authenticate registered cards against the database
- **Visual Feedback**: 16x2 LCD display for system status and user prompts
- **LED Indication**: Visual confirmation when cards are detected
- **Keypad Interface**: Simple menu navigation using a matrix keypad
- **Database Management**: Store up to 20 unique RFID card UIDs
- **Duplicate Detection**: Prevents registration of already existing cards

## Hardware Components

- **Microcontroller**: NXP LPC1768 (ARM Cortex-M3)
- **RFID Reader**: MFRC522 (13.56 MHz)
- **Display**: 16x2 LCD Module
- **Input**: 4x3 Matrix Keypad
- **Indicator**: LED for card detection feedback
- **Interface**: SPI communication protocol

## Pin Configuration

### MFRC522 to LPC1768 Connections

![Hardware Connections](connections.jpg)

| MFRC522 Pin | Function | Connect to LPC1768 | Port | Connector |
|-------------|----------|-------------------|------|-----------|
| SDA | Slave Select (NSS) | P0.6 | P0.6 | CNA pin 3 |
| SCK | SPI Clock | P0.7 | P0.7 | CNA pin 4 |
| MOSI | Master Out Slave In | P0.9 | P0.9 | CNA pin 6 |
| MISO | Master In Slave Out | P0.8 | P0.8 | CNA pin 5 |
| IRQ | Interrupt (not used) | Leave unconnected | - | - |
| RST | Reset | Tie to 3.3 V (or GPIO) | - | - |
| GND | Ground | Common GND | - | - |
| 3.3V | Power | 3.3 V from LPC1768 | - | - |

### Other Peripheral Connections

- **LCD**: Connected to GPIO0 (Pins 23-26 for data, Pin 27 for RS, Pin 28 for EN)
- **Keypad**: Row pins on GPIO2 (Pins 10-13), Column pins on GPIO1 (Pins 23-25)
- **LED**: Connected to GPIO0 Pin 22

## System Architecture

### Communication Protocol

The system uses SPI (Serial Peripheral Interface) for communication between the LPC1768 and MFRC522 module:
- Clock Polarity: 0
- Clock Phase: 0
- Data Rate: Configurable (CPSR = 8)
- Mode: Master

### Operation Modes

#### 1. Registration Mode (Key 2)
- Scan new RFID cards
- Check for duplicates in the database
- Store unique card UIDs
- Provide confirmation feedback

#### 2. Verification Mode (Key 1)
- Scan RFID cards
- Compare against stored database
- Display verification result
- Provide visual and textual feedback

## Software Implementation

### Key Functions

- `RFID_Init()`: Initialize the MFRC522 module
- `MFRC522_Request()`: Request card detection
- `MFRC522_Anticoll()`: Read card UID
- `register_uid_rfid()`: Register new cards
- `verify_uid_rfid()`: Verify existing cards
- `find_uid_rfid()`: Search database for matching UID

### Database Structure

- **Capacity**: 20 RFID cards
- **UID Length**: 5 bytes per card
- **Storage**: 2D array in RAM

## Getting Started

### Prerequisites

- Keil uVision IDE or compatible ARM development environment
- LPC1768 development board
- MFRC522 RFID reader module
- Compatible 13.56 MHz RFID cards/tags
- 16x2 LCD display
- 4x3 Matrix keypad

### Building the Project

1. Clone the repository:
   ```bash
   git clone https://github.com/Addy-Da-Baddy/RFID-Verification-System.git
   ```

2. Open the project in Keil uVision

3. Configure the target device as LPC1768

4. Build the project (Project → Build Target)

5. Flash the binary to the LPC1768 board

### Hardware Setup

1. Connect the MFRC522 module to the LPC1768 as per the pin configuration table
2. Wire the LCD display to the appropriate GPIO pins
3. Connect the matrix keypad
4. Ensure proper power supply (3.3V) to all components
5. Verify all ground connections are common

## Usage

1. Power on the system
2. The LCD displays: "1:Verify 2:Register"
3. **To Register a Card**:
   - Press key '2' on the keypad
   - System displays "Scan Card..."
   - Place RFID card near the reader
   - LED blinks upon detection
   - System confirms registration
4. **To Verify a Card**:
   - Press key '1' on the keypad
   - System displays "Scan Card..."
   - Place RFID card near the reader
   - LED blinks upon detection
   - System displays verification result

## System Limitations

- Maximum 20 cards can be registered
- UIDs stored in volatile memory (lost on power cycle)
- No persistent storage implementation
- Single-user operation (no concurrent access)

## Future Enhancements

- Non-volatile storage for persistent UID database
- Serial communication for external database integration
- Enhanced security features (encryption)
- Real-time clock for access logging
- Support for additional RFID card types
- Web-based monitoring interface

## Technical Specifications

- **Operating Voltage**: 3.3V
- **RFID Frequency**: 13.56 MHz
- **Communication**: SPI (Master Mode)
- **Response Time**: < 100ms per card scan
- **Read Range**: Up to 5cm (depending on card type)

## License

This project is available for educational and non-commercial use.

## Author

Developed as part of an embedded systems design project.

## Acknowledgments

- NXP Semiconductors for LPC1768 documentation
- MFRC522 datasheet and protocol specifications
- ARM Cortex-M3 reference materials

## Repository

For more information, documentation, and updates, visit the [project repository](https://github.com/Addy-Da-Baddy/RFID-Verification-System).
