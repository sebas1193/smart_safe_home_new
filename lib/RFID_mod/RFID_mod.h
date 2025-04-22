//RFID _mod.h
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

public:
    RFID_mod(int ss_pin, int rst_pin, const byte* uid) 
        : SS_PIN(ss_pin), RST_PIN(rst_pin), mfrc522(ss_pin, rst_pin), uidEsperado(uid) {}

    void begin() {
        SPI.begin();
        mfrc522.PCD_Init();
        Serial.begin(115200);
        Serial.println("Escanea tu tarjeta RFID");
    }

    bool tarjetaDetectada() {
        if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
            return false;
        }
        return compararUID();
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

#endif