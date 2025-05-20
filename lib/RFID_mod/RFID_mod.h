#ifndef RFID_MOD_H
#define RFID_MOD_H

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

class RFID_mod {
private:
    int SS_PIN;
    int RST_PIN;
    const byte* uidEsperado;
    MFRC522 mfrc522;
    const unsigned long TIMEOUT_MS = 100;
    int readCounter = 0;  // Contador de lecturas

public:
    RFID_mod(int ss_pin, int rst_pin, const byte* uid)
      : SS_PIN(ss_pin), RST_PIN(rst_pin), mfrc522(ss_pin, rst_pin), uidEsperado(uid) {}

    void begin() {
        SPI.begin();
        mfrc522.PCD_Init();
        Serial.println("Escanea tu tarjeta RFID");
    }

    bool tarjetaDetectada() {
        // Incrementar contador de lecturas y reiniciar cada 10 lecturas
        if (++readCounter >= 10) {
            mfrc522.PCD_Init();
            Serial.println("⚙️ Reinicio periódico del módulo RFID tras 10 lecturas");
            readCounter = 0;
        }

        unsigned long start = millis();
        while (millis() - start < TIMEOUT_MS) {
            if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
                bool valid = compararUID();
                mfrc522.PICC_HaltA();
                mfrc522.PCD_StopCrypto1();
                return valid;
            }
        }
        // Timeout: reiniciar módulo y devolver false
        mfrc522.PCD_Init();
        Serial.println("⚠️ RFID reiniciado por timeout");
        return false;
    }

private:
    bool compararUID() {
        for (byte i = 0; i < mfrc522.uid.size; i++) {
            if (mfrc522.uid.uidByte[i] != uidEsperado[i]) {
                Serial.println("¡❌ Tarjeta Incorrecta!");
                return false;
            }
        }
        Serial.println("¡✅ Tarjeta correcta!");
        return true;
    }
};

#endif  // RFID_MOD_H
