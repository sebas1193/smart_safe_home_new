**Secure Senior Home (SSH)**

> A modular IoT solution for monitoring and controlling home security for seniors living alone.

---

## Table of Contents

1. [Overview](#overview)
2. [Key Features](#key-features)
3. [Technologies & Components](#technologies--components)
4. [Project Structure](#project-structure)
5. [Prerequisites](#prerequisites)
6. [Installation & Configuration](#installation--configuration)

   * [.env File](#env-file)
   * [Credentials.h](#credentialsh)
   * [Docker MySQL Container](#docker-mysql-container)
7. [Usage](#usage)
8. [Modules & Libraries](#modules--libraries)
9. [Additional Documentation](#additional-documentation)
10. [Enhancements & Limitations](#enhancements--limitations)
11. [License](#license)

---

## Overview

**Secure Senior Home (SSH)** is an IoT platform designed to enhance safety in homes of seniors living alone. Through real-time sensors and notifications, SSH continuously monitors for anomalies (unauthorized movement, door openings, smoke detection, etc.) and automatically alerts family members or relevant authorities.

The modular architecture allows easy extension and customization by adding new sensors, actuators, or communication protocols.

---

## Key Features

* Presence detection (PIR HC-SR501)
* Access control via RFID RC522
* Audible alarm with buzzer and automatic door lock via SG90 servo
* Gas leak detection (MQ-2 sensor)
* Temperature and humidity measurement (DHT11)
* MQTT-based notifications
* MySQL database hosted in Docker container

---

## Technologies & Components

* **Platform**: ESP32 (Arduino Framework / PlatformIO)
* **Sensors & Actuators**:

  * RFID RC522
  * PIR HC-SR501
  * Buzzer
  * SG90 Servo
  * MQ-2 Gas Sensor
  * DHT11 Temperature & Humidity Sensor
  * 4-Node Button
* **Communication**:

  * WiFi
  * MQTT
* **Backend**:

  * MySQL (Docker)
* **Frontend**:

  * Node.js (Express + EJS)

---

## Project Structure

```bash
.
├── README.md
├── docker-compose.yaml         # MySQL container configuration
├── docs/                       # Additional documentation
├── include/                    # Global headers
│   └── ...
├── lib/                        # Hardware modules
│   ├── Buzzer/Buzzer.h
│   ├── Credentials/Credentials.h  # WiFi, MQTT & RFID credentials
│   ├── RFID_mod/RFID_mod.h
│   └── WiFiConnection/WiFiConnection.h
├── package.json                # Node.js dependencies
├── platformio.ini              # PlatformIO configuration
├── sql/init.sql                # Initial database schema
└── src/
    └── main.cpp                # Core logic in C++
```

---

## Prerequisites

* [PlatformIO](https://platformio.org/) installed
* Docker & Docker Compose
* Node.js & npm
* WiFi network access

---

## Installation & Configuration

### .env File

Place a `.env` file in the project root with the following content:

```dotenv
DB_HOST=localhost
DB_PORT=3306
DB_USER=your_db_user
DB_PASSWORD=your_db_password
DB_NAME=smart_home_db
```

### Credentials.h

Create `Credentials.h` under `lib/Credentials/`:

```cpp
#ifndef CREDENTIALS_H
#define CREDENTIALS_H

// WiFi credentials
#define WIFI_SSID       "Your_SSID"
#define WIFI_PASSWORD   "Your_Password"

// Expected RFID UID
const byte UID_EXPECTED[] = { /* 0xAA, 0xBB, 0xCC, 0xDD */ };

// MQTT credentials
#define MQTT_SERVER     "broker.local"
#define MQTT_PORT       1883
#define MQTT_CLIENT_ID  "SSH_Client"
#define MQTT_TOPIC      "ssh/alerts"
#define MQTT_USER       "mqtt_user"
#define MQTT_PASS       "mqtt_pass"

#endif
```

### Docker MySQL Container

Start the MySQL container:

```bash
docker-compose up -d
```

---

## Usage

1. Configure the `.env` file and `Credentials.h`.
2. Launch the MySQL container: `docker-compose up -d`.
3. Build and upload firmware to the ESP32 using PlatformIO:

   ```bash
   pio run --target upload
   ```

---

## Modules & Libraries

Each hardware module resides in `lib/` and provides a clear interface:

* **Buzzer**: `activate()`, `deactivate()`
* **RFID\_mod**: `readUID()`, `compareUID()`
* **WiFiConnection**: `connect()`, `disconnect()`

Refer to the header files for detailed API.

---

## Additional Documentation

Detailed analysis, conclusions, enhancements, and limitations are available in the `docs/` directory.

---

## License

This project is licensed under MIT License. See `LICENSE` for details.

---

