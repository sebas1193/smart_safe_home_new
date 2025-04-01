#include <Arduino.h>

// Definición de la clase Buzzer
class Buzzer {
    const int frequency1 = 1000;  // Frecuencia 1
    const int frequency2 = 1500;  // Frecuencia 2
    const int duration = 500;     // Duración de cada frecuencia (ms)
    int pin;
    bool status;
    unsigned long previousMillis = 0;
    bool toggle = false;  // Para alternar entre las dos frecuencias

public:
    Buzzer(int pin, bool initialStatus) : pin(pin), status(initialStatus) {
        pinMode(pin, OUTPUT);
        ledcSetup(0, 1000, 8);  // Configura el canal 0 con 1000 Hz, 8 bits de resolución
        ledcAttachPin(pin, 0);
    }

    bool getStatus() const {
        return status;
    }

    void setStatus(bool newStatus) {
        status = newStatus;
        if (!status) {
            turnOffSound();  // Apaga el sonido inmediatamente si se desactiva
        }
    }

    void turnOffSound() {
        ledcWriteTone(0, 0);  // Detiene el sonido
    }

    void sound() {
        if (!status) return;  // Si el buzzer está apagado, no hace nada

        unsigned long currentMillis = millis();

        if (currentMillis - previousMillis >= duration) {
            previousMillis = currentMillis;
            toggle = !toggle;  // Alternar entre true y false

            if (toggle) {
                ledcWriteTone(0, frequency1);  // Sonar en frecuencia 1
            } else {
                ledcWriteTone(0, frequency2);  // Sonar en frecuencia 2
            }
        }
    }

    void playTone() {
        sound();
    }
};
