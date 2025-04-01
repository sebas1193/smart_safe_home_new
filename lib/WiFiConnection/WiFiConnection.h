// WiFiConnection.h
#ifndef WIFI_CONNECTION_H
#define WIFI_CONNECTION_H

#include <Arduino.h>
#include <WiFi.h>

class WiFiConnection {
  private:
    const char* ssid;
    const char* password;
    const int ledPin;
    bool isConnected;
    unsigned long connectionTime;
    
    void handleConnecting() {
      digitalWrite(ledPin, !digitalRead(ledPin));
      delay(250);
      Serial.print(".");
      isConnected = false;
    }
    
    void handleConnected() {
      if (!isConnected) {
        connectionTime = millis();
        digitalWrite(ledPin, HIGH);
        Serial.println("\n¡WiFi Conectado!");
        isConnected = true;
      }
      
      if (millis() - connectionTime < 5000) {
        digitalWrite(ledPin, HIGH);
      } else {
        digitalWrite(ledPin, LOW);
      }
    }
    
  public:
    WiFiConnection(const char* ssid, const char* password, int ledPin = 2) 
      : ssid(ssid), password(password), ledPin(ledPin), isConnected(false), connectionTime(0) {
      pinMode(ledPin, OUTPUT);
    }
    
    void begin() {
      Serial.begin(115200);
      WiFi.begin(ssid, password);
      Serial.println("Iniciando conexión WiFi...");
    }
    
    void update() {
      if (WiFi.status() != WL_CONNECTED) {
        handleConnecting();
      } else {
        handleConnected();
      }
    }
    
    bool connected() const {
      return isConnected;
    }
};

#endif