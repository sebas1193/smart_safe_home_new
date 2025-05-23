// main.cpp - Versión con control de luces completo
#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "Buzzer.h"
#include "RFID_mod.h"
#include "WiFiConnection.h"
#include "Credentials.h"

// Pines
#define SENSOR_IR_PIN 15
#define SENSOR_PIR_PIN 13
#define SS_PIN 21
#define RST_PIN 22
#define BUZZER_PIN 25
#define LED_PIN 32
#define BUTTON_LIGHTS_PIN 5

// Instancias
WiFiConnection wifi(WIFI_SSID, WIFI_PASSWORD);
WiFiClient espClient;
PubSubClient client(espClient);
Buzzer buzzer(BUZZER_PIN, false);
RFID_mod rfid(SS_PIN, RST_PIN, UID_ESPERADO);

// Configuración NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -5 * 3600;
const int daylightOffset_sec = 0;

// Variables de estado
const char* id_nodo = "NODE_A";
const char* id_nodo2 = "NODE_A1";
bool status_identification = false;
int status_IR = LOW;
int status_PIR = LOW;
bool status_lights = LOW;

// Estados anteriores para envío MQTT
bool last_identification = false;
bool last_IR = false;
bool last_buzzer = false;
bool last_PIR = false;
bool last_lights = false;

// Temporizadores
unsigned long tiempoInicioLuces = 0;

// Identificación y alarma
bool puertaAbierta = false;
unsigned long tiempoApertura = 0;
bool enVentana3s = false;
unsigned long inicioVentana3s = 0;

// Envío periódico en alarma
unsigned long lastSend = 0;

// Tiempo para resetear la identificación después de una lectura exitosa
unsigned long tiempoIdentificacion = 0;
const unsigned long TIMEOUT_IDENTIFICACION = 5000; // 5 segundos para resetear el estado

// Variable para controlar ventana de gracia para salida
bool enVentanaGracia = false;
unsigned long inicioVentanaGracia = 0;
const unsigned long TIEMPO_VENTANA_GRACIA = 7000; // 7 segundos de ventana de gracia

// Para controlar si una alarma fue cancelada con identificación
bool alarmaCancelada = false;

// Estados para el control de luces
#define MODO_PIR 0         // Modo normal: PIR enciende por 30 segundos
#define MODO_ENCENDIDO 1   // Forzado encendido permanente
#define MODO_APAGADO 2     // Forzado apagado permanente
int modoLuz = MODO_PIR;    // Inicialmente en modo PIR
bool forzadoPorMQTT = false; // Control para comandos MQTT
unsigned long ultimoPulsador = 0; // Para debounce del botón
bool estadoAnteriorPulsador = HIGH; // Estado previo del pulsador (con pull-up)

// Nuevas variables para reportar estado detallado de luces
bool manual_on = false;
bool manual_off = false;
bool auto_mode = true;
bool remote_on = false;
bool remote_off = false;
// Variables para auto-restauración del modo después de control remoto
bool reportedRemoteState = false;  // Para controlar si ya reportamos el estado remoto
unsigned long remoteControlTime = 0;  // Tiempo en que se activó control remoto
const unsigned long REMOTE_AUTO_RESET = 2000;  // Tiempo para auto-restaurar (2 segundos)
// Lecturas
int lecture_ir_sensor() { return digitalRead(SENSOR_IR_PIN); }
int lecture_pir_sensor() { return digitalRead(SENSOR_PIR_PIN); }
bool lecture_rfid_sensor() { return rfid.tarjetaDetectada(); }

// Declaraciones de funciones (forward declarations)
void sendMainMessage();
void sendSecondaryMessage();
void updateLightStates();

// Función para actualizar estados de luz basados en el modo actual
void updateLightStates() {
  // Actualizar variables de estado según el modo actual
  if (forzadoPorMQTT) {
    // Solo establecer remote_on/off como true si aún no hemos reportado el estado
    if (!reportedRemoteState) {
      remote_on = (modoLuz == MODO_ENCENDIDO);
      remote_off = (modoLuz == MODO_APAGADO);
      reportedRemoteState = true;  // Marcar que ya reportamos este estado
    } else {
      // Después del primer reporte, mostrar que estamos en modo auto
      remote_on = false;
      remote_off = false;
      auto_mode = true;
    }
    manual_on = false;
    manual_off = false;
  } else {
    remote_on = false;
    remote_off = false;
    
    if (modoLuz == MODO_PIR) {
      auto_mode = true;
      manual_on = false;
      manual_off = false;
    } else if (modoLuz == MODO_ENCENDIDO) {
      auto_mode = false;
      manual_on = true;
      manual_off = false;
    } else if (modoLuz == MODO_APAGADO) {
      auto_mode = false;
      manual_on = false;
      manual_off = true;
    }
  }
}

// Callback para MQTT
// Callback para MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("📩 Mensaje recibido [");
  Serial.print(topic);
  Serial.print("] ");
  
  // Convertir payload a string
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);
  
  // Parsear JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.print("❌ Error deserializando JSON: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Procesar control de luces - verificamos si es el tópico de control
  if (strcmp(topic, MQTT_TOPIC3) == 0) {
    if (doc.containsKey("turn_off")) {
      if (doc["turn_off"] == 1) {
        // Forzar apagado permanente
        forzadoPorMQTT = true;
        modoLuz = MODO_APAGADO;
        digitalWrite(LED_PIN, LOW);
        status_lights = false;
        reportedRemoteState = false;  // Reiniciar flag de reporte
        remoteControlTime = millis();  // Iniciar temporizador para auto-restauración
        Serial.println("💡 Luces forzadas a OFF por MQTT");
        updateLightStates();
        sendSecondaryMessage();
      } else if (doc["turn_off"] == 0) {
        // Forzar encendido permanente
        forzadoPorMQTT = true;
        modoLuz = MODO_ENCENDIDO;
        digitalWrite(LED_PIN, HIGH);
        status_lights = true;
        reportedRemoteState = false;  // Reiniciar flag de reporte
        remoteControlTime = millis();  // Iniciar temporizador para auto-restauración
        Serial.println("💡 Luces forzadas a ON por MQTT");
        updateLightStates();
        sendSecondaryMessage();
      }
    }
  }
}

// Reconexión MQTT
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Intentando conectar a MQTT...");
    if (client.connect("ESP32Client", MQTT_USER, MQTT_PASS)) {
      Serial.println("✅ Conectado a MQTT.");
      client.subscribe(MQTT_TOPIC); // Tópico principal
      client.subscribe(MQTT_TOPIC3); // Nuevo tópico para control de luces
      Serial.println("Suscrito a tópico principal: " + String(MQTT_TOPIC));
      Serial.println("Suscrito a tópico de control de luces: " + String(MQTT_TOPIC3));
    } else {
      Serial.print("❌ Fallo, rc=");
      Serial.print(client.state());
      Serial.println(" - Reintentando...");
      delay(3000);
    }
  }
}

// Formateo de fecha
String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "N/A";
  char buf[20];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

// Envío JSON genérico
void sendMainMessage() {
  JsonDocument doc;
  doc["id_node"] = id_nodo;
  doc["door"] = status_IR;
  doc["buzzer"] = buzzer.getStatus();
  doc["identification"] = status_identification;
  doc["sent_at"] = getTimestamp();
  char buffer[256];
  serializeJson(doc, buffer);
  client.publish(MQTT_TOPIC, buffer);
  Serial.println("📤 Main JSON enviado: " + String(buffer));
}

void sendSecondaryMessage() {
  JsonDocument doc;
  doc["id_node"] = id_nodo2;
  doc["presence"] = status_PIR;
  doc["lights"] = status_lights ? 1 : 0;
  doc["manual_on"] = manual_on;
  doc["manual_off"] = manual_off;
  doc["auto"] = auto_mode;
  doc["remote_on"] = remote_on;
  doc["remote_off"] = remote_off;
  doc["sent_at"] = getTimestamp();
  char buffer[256];
  serializeJson(doc, buffer);
  client.publish(MQTT_TOPIC2, buffer);
  Serial.println("📤 Secondary JSON enviado: " + String(buffer));
}

void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_IR_PIN, INPUT);
  pinMode(SENSOR_PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_LIGHTS_PIN, INPUT_PULLUP);

  // Inicializar estados de luces
  auto_mode = true;
  manual_on = false;
  manual_off = false;
  remote_on = false;
  remote_off = false;

  wifi.begin();
  rfid.begin();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Esperar NTP
  Serial.println("⌛ Sincronizando hora NTP...");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    delay(500);
  }
  Serial.println("✅ Hora sincronizada: " + getTimestamp());
  
  // Imprimir información de tópicos
  Serial.println("Tópico principal (publicación): " + String(MQTT_TOPIC));
  Serial.println("Tópico de control de luces (suscripción): " + String(MQTT_TOPIC3));
  Serial.println("Tópico secundario (publicación): " + String(MQTT_TOPIC2));
}

void loop() {
  // Variables para rastrear el estado anterior
  static int last_PIR_state = -1;  // Inicializar con un valor imposible
  static int last_lights_state = -1;
  static bool last_manual_on_state = false;
  static bool last_manual_off_state = false;
  static bool last_auto_mode_state = true;
  static bool last_remote_on_state = false;
  static bool last_remote_off_state = false;
  static int last_IR_state = -1;
  static bool last_identification_state = false;
  static bool last_buzzer_state = false;
  
  // Flag para forzar envío después de un cambio de estado
  static bool stateJustChanged = false;
  static unsigned long lastChangeTime = 0;
  
  // Resto del código actual...
  wifi.update();
  if (!wifi.connected()) return;

  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  // Actualizar sensores
  int ir = lecture_ir_sensor();
  int pir = lecture_pir_sensor();
  bool rfid_ok = lecture_rfid_sensor();
  unsigned long now = millis();
  
  // Inicializar estados si es la primera ejecución
  if (last_PIR_state == -1) {
    last_PIR_state = pir;
    last_lights_state = status_lights;
    last_IR_state = ir;
  }
  
  // === Control PIR/luces con tres estados ===
  bool btnActual = !digitalRead(BUTTON_LIGHTS_PIN); // Invertimos porque es INPUT_PULLUP

  // Control de botón con debounce
  if (btnActual != estadoAnteriorPulsador && (now - ultimoPulsador > 50)) { // 50ms debounce
    if (btnActual == true) { // Solo en flanco de bajada (presionar botón)
      stateJustChanged = true;  // Marcar que hubo un cambio de estado
      lastChangeTime = now;
      
      if (forzadoPorMQTT) {
        // Si está forzado por MQTT, liberamos el control y volvemos a modo PIR
        forzadoPorMQTT = false;
        modoLuz = MODO_PIR;
        Serial.println("💡 Control liberado de MQTT, volviendo a modo PIR");
        
        // Verificar si hay movimiento actual para determinar estado inicial
        if (pir == HIGH) {
          digitalWrite(LED_PIN, HIGH);
          status_lights = true;
          tiempoInicioLuces = now;
        } else {
          digitalWrite(LED_PIN, LOW);
          status_lights = false;
        }
      } else {
        // Comportamiento normal - ciclar entre los tres modos
        modoLuz = (modoLuz + 1) % 3;
        
        Serial.print("💡 Cambio de modo a: ");
        switch (modoLuz) {
          case MODO_ENCENDIDO:
            Serial.println("Encendido permanente");
            digitalWrite(LED_PIN, HIGH);
            status_lights = true;
            break;
          case MODO_PIR:
            Serial.println("Control por PIR");
            // Verificar si hay movimiento actual
            if (pir == HIGH) {
              digitalWrite(LED_PIN, HIGH);
              status_lights = true;
              tiempoInicioLuces = now;
            } else {
              digitalWrite(LED_PIN, LOW);
              status_lights = false;
            }
            break;
          case MODO_APAGADO:
            Serial.println("Apagado permanente");
            digitalWrite(LED_PIN, LOW);
            status_lights = false;
            break;
        }
      }
      
      // Actualizar estados antes de enviar
      updateLightStates();
      
      // Guardar nuevo estado antes de enviar
      last_lights_state = status_lights;
      last_PIR_state = pir;
      last_manual_on_state = manual_on;
      last_manual_off_state = manual_off;
      last_auto_mode_state = auto_mode;
      last_remote_on_state = remote_on;
      last_remote_off_state = remote_off;
      
      // Enviar mensaje SOLO UNA VEZ
      sendSecondaryMessage();
    }
    ultimoPulsador = now;
  }
  estadoAnteriorPulsador = btnActual;

  // Lógica de PIR según el modo actual
  if (modoLuz == MODO_PIR && !forzadoPorMQTT) {
    if (pir == HIGH && status_lights == false) {
      // Encender luz al detectar movimiento
      digitalWrite(LED_PIN, HIGH);
      status_lights = true;
      tiempoInicioLuces = now;
      
      stateJustChanged = true;  // Marcar cambio de estado
      lastChangeTime = now;
      
      // Actualizar antes de enviar
      updateLightStates();
      
      // Guardar nuevo estado antes de enviar
      last_lights_state = status_lights;
      last_PIR_state = pir;
      last_manual_on_state = manual_on;
      last_manual_off_state = manual_off;
      last_auto_mode_state = auto_mode;
      last_remote_on_state = remote_on;
      last_remote_off_state = remote_off;
      
      // Enviar mensaje SOLO UNA VEZ
      sendSecondaryMessage();
    } else if (status_lights && (now - tiempoInicioLuces >= 30000)) {
      // Apagar después de 30 segundos
      digitalWrite(LED_PIN, LOW);
      status_lights = false;
      
      stateJustChanged = true;  // Marcar cambio de estado
      lastChangeTime = now;
      
      // Actualizar antes de enviar
      updateLightStates();
      
      // Guardar nuevo estado antes de enviar
      last_lights_state = status_lights;
      last_PIR_state = pir;
      last_manual_on_state = manual_on;
      last_manual_off_state = manual_off;
      last_auto_mode_state = auto_mode;
      last_remote_on_state = remote_on;
      last_remote_off_state = remote_off;
      
      // Enviar mensaje SOLO UNA VEZ
      sendSecondaryMessage();
    }
  }

  // Auto-restauración después de control remoto
  static bool autoRestoreMessageSent = false;
  
  if (forzadoPorMQTT && (now - remoteControlTime >= REMOTE_AUTO_RESET)) {
    // Verificar si ya hemos enviado al menos un mensaje con estado remoto
    if (reportedRemoteState && !autoRestoreMessageSent) {
      // Cambiar a modo PIR realmente, no solo el reporte
      forzadoPorMQTT = false;
      modoLuz = MODO_PIR;
      
      // Verificar si hay movimiento para determinar el estado de las luces
      if (pir == HIGH) {
        // Mantener la luz encendida por 30s desde ahora
        digitalWrite(LED_PIN, HIGH);
        status_lights = true;
        tiempoInicioLuces = now;
      } else {
        // Apagar la luz si no hay movimiento
        digitalWrite(LED_PIN, LOW);
        status_lights = false;
      }
      
      stateJustChanged = true;  // Marcar cambio de estado
      lastChangeTime = now;
      
      // Actualizar estados
      updateLightStates();
      
      // Guardar nuevo estado antes de enviar
      last_lights_state = status_lights;
      last_PIR_state = pir;
      last_manual_on_state = manual_on;
      last_manual_off_state = manual_off;
      last_auto_mode_state = auto_mode;
      last_remote_on_state = remote_on;
      last_remote_off_state = remote_off;
      
      // Enviar mensaje SOLO UNA VEZ
      sendSecondaryMessage();
      
      // Marcar que ya restauramos el sistema
      autoRestoreMessageSent = true;
      
      Serial.println("💡 Restaurado a modo automático PIR después de control remoto");
    }
  } else if (!forzadoPorMQTT) {
    // Reiniciar el flag si salimos del estado forzado por MQTT
    autoRestoreMessageSent = false;
  }

  // === Detección de puerta y UID ===
  status_IR = ir;
  status_PIR = pir;

  // Detectar cambios en el estado del PIR (presencia)
  if (status_PIR != last_PIR_state) {
    // Solo marcar el cambio si no acabamos de enviar un mensaje
    if (!stateJustChanged || (now - lastChangeTime > 500)) {
      stateJustChanged = true;
      lastChangeTime = now;
      
      // Actualizar antes de enviar
      updateLightStates();
      
      // Guardar nuevo estado antes de enviar
      last_PIR_state = status_PIR;
      last_lights_state = status_lights;
      last_manual_on_state = manual_on;
      last_manual_off_state = manual_off;
      last_auto_mode_state = auto_mode;
      last_remote_on_state = remote_on;
      last_remote_off_state = remote_off;
      
      // Enviar mensaje SOLO UNA VEZ
      sendSecondaryMessage();
    } else {
      // Solo actualizar el estado sin enviar mensaje
      last_PIR_state = status_PIR;
    }
  }

  // === IMPORTANTE: Silenciar buzzer siempre que se detecte un UID válido ===
  if (rfid_ok) {
    status_identification = true;
    tiempoIdentificacion = now;
    buzzer.setStatus(false);
    buzzer.turnOffSound();
    alarmaCancelada = true;  // Marcar que una alarma fue cancelada con identificación
  }

  // === NUEVO: Lectura previa de UID con puerta cerrada ===
  if (ir == LOW && !puertaAbierta && !enVentanaGracia && rfid_ok) {
    // Se detectó UID válido con puerta cerrada, iniciar ventana de gracia
    enVentanaGracia = true;
    inicioVentanaGracia = now;
    Serial.println("Salida autorizada");
    
    // Actualizar estado antes de enviar
    last_IR_state = status_IR;
    last_identification_state = status_identification;
    last_buzzer_state = buzzer.getStatus();
    
    sendMainMessage(); // Notificar cambio de estado
  }

  // === Control de ventana de gracia para salida ===
  if (enVentanaGracia) {
    // Si la puerta se abre durante la ventana de gracia
    if (ir == HIGH && !puertaAbierta) {
      puertaAbierta = true;
      tiempoApertura = now;
    }
    
    // Si la puerta se cierra durante la ventana de gracia, todo correcto
    if (puertaAbierta && ir == LOW) {
      puertaAbierta = false;
      enVentanaGracia = false; // Fin de ventana de gracia, todo correcto
      Serial.println("Salida completada correctamente");
      
      // Actualizar estado antes de enviar
      last_IR_state = status_IR;
      last_identification_state = status_identification;
      last_buzzer_state = buzzer.getStatus();
      
      sendMainMessage(); // Notificar salida correcta
    }
    
    // Si pasan 7s y no se completó el ciclo abrir/cerrar
    if (now - inicioVentanaGracia >= TIEMPO_VENTANA_GRACIA) {
      // Si la puerta sigue abierta, mantener la ventana de gracia
      if (puertaAbierta) {
        // No hacemos nada, se esperará a que se cierre la puerta
      } else {
        // Si la puerta nunca se abrió y está cerrada
        enVentanaGracia = false;
        // NO activamos alarma aquí, solo cancelamos la ventana de gracia
        Serial.println("⚠️ Ventana de gracia finalizada sin apertura de puerta");
        
        // Actualizar estado antes de enviar
        last_IR_state = status_IR;
        last_identification_state = status_identification;
        last_buzzer_state = buzzer.getStatus();
        
        sendMainMessage();
      }
    }
  }

  // === CORRECCIÓN: Control de reset de identificación ===
  // Resetear identificación después de un tiempo si la puerta está cerrada, no estamos en ventana de gracia
  // y no hubo una cancelación de alarma reciente
  if (status_identification && ir == LOW && !enVentanaGracia && 
      (now - tiempoIdentificacion >= TIMEOUT_IDENTIFICACION) && !alarmaCancelada) {
    status_identification = false;
    Serial.println("🔄 Identificación reseteada por timeout");
    
    // Actualizar estado antes de enviar
    last_IR_state = status_IR;
    last_identification_state = status_identification;
    last_buzzer_state = buzzer.getStatus();
    
    sendMainMessage(); // Notificar cambio de estado
  }

  // Resetear flag de alarma cancelada después del timeout de identificación
  if (alarmaCancelada && (now - tiempoIdentificacion >= TIMEOUT_IDENTIFICACION)) {
    alarmaCancelada = false;
  }

  // Apertura inicial (cuando no hay identificación previa o ventana de gracia)
  if (ir == HIGH && !puertaAbierta && !enVentanaGracia) {
    puertaAbierta = true;
    tiempoApertura = now;
    enVentana3s = false;
    
    // No resetear identificación aquí si estamos en proceso de salida
    if (!status_identification) {
      // Si no hay identificación previa, preparar para posible alarma
      buzzer.setStatus(false); // Asegurar que el buzzer está apagado al inicio
    }
    
    // Detectar cambio en el estado de la puerta
    if (status_IR != last_IR_state) {
      // Actualizar estado antes de enviar
      last_IR_state = status_IR;
      last_identification_state = status_identification;
      last_buzzer_state = buzzer.getStatus();
      
      sendMainMessage();
    }
  }

  // Si permanece abierta >7s sin identificación y no estamos en ventana de gracia, activa alarma
  if (puertaAbierta && !status_identification && !enVentanaGracia && (now - tiempoApertura >= 7000)) {
    // Solo activar el buzzer si no se ha identificado y no hubo una cancelación reciente
    if (!rfid_ok && !alarmaCancelada && !buzzer.getStatus()) {
      buzzer.setStatus(true);
      buzzer.playTone();
      
      // Detectar cambio en el estado del buzzer
      if (buzzer.getStatus() != last_buzzer_state) {
        // Actualizar estado antes de enviar
        last_IR_state = status_IR;
        last_identification_state = status_identification;
        last_buzzer_state = buzzer.getStatus();
        
        sendMainMessage();
        lastSend = now;
      }
    } else if (buzzer.getStatus() && (now - lastSend >= 2000)) {
      // Enviar actualización periódica del estado de alarma, pero menos frecuente
      lastSend = now;
      
      // Actualizar estado antes de enviar
      last_IR_state = status_IR;
      last_identification_state = status_identification;
      last_buzzer_state = buzzer.getStatus();
      
      sendMainMessage();
    }
  }

  // Cierre de puerta (cuando no estamos en ventana de gracia)
  if (puertaAbierta && ir == LOW && !enVentanaGracia) {
    // Si aún no estuvo en ventana de 3s y no pasó 7s, iniciar ventana 3s
    if (!enVentana3s && (now - tiempoApertura < 7000) && !status_identification) {
      enVentana3s = true;
      inicioVentana3s = now;
    }
    
    // La detección de UID ya se maneja al principio del loop
    if (rfid_ok) {
      Serial.println("Ingreso autorizado");
      
      // Actualizar estado antes de enviar
      last_IR_state = status_IR;
      last_identification_state = status_identification;
      last_buzzer_state = buzzer.getStatus();
      
      sendMainMessage();
    }
    puertaAbierta = false;
    
    // Detectar cambio en el estado de la puerta
    if (status_IR != last_IR_state) {
      // Actualizar estado antes de enviar
      last_IR_state = status_IR;
      last_identification_state = status_identification;
      last_buzzer_state = buzzer.getStatus();
      
      sendMainMessage();
    }
  }

  // Ventana de 3s para UID (ingreso regular)
  if (enVentana3s && !status_identification) {
    if (rfid_ok) {
      // La identificación y buzzer ya se manejan al principio
      Serial.println("Ingreso autorizado");
      
      // Actualizar estado antes de enviar
      last_IR_state = status_IR;
      last_identification_state = status_identification;
      last_buzzer_state = buzzer.getStatus();
      
      sendMainMessage();
      enVentana3s = false;
    } else if (now - inicioVentana3s >= 3000) {
      // Solo activar buzzer si no hay UID y no hubo cancelación reciente
      if (!rfid_ok && !alarmaCancelada && !buzzer.getStatus()) {
        buzzer.setStatus(true);
        buzzer.playTone();
        
        // Detectar cambio en el estado del buzzer
        if (buzzer.getStatus() != last_buzzer_state) {
          // Actualizar estado antes de enviar
          last_IR_state = status_IR;
          last_identification_state = status_identification;
          last_buzzer_state = buzzer.getStatus();
          
          sendMainMessage();
          lastSend = now;
        }
      }
      enVentana3s = false;
    }
  }

  // === Envío por cambios de estado ===
  // Buzzer alterna tono solo si está activo
  if (buzzer.getStatus()) buzzer.sound();

  // Envío por cambios de estado principal - SOLO si hay un cambio real
  if ((status_identification != last_identification_state) ||
      (status_IR != last_IR_state) ||
      (buzzer.getStatus() != last_buzzer_state)) {
    // Actualizar estado antes de enviar
    last_identification_state = status_identification;
    last_IR_state = status_IR;
    last_buzzer_state = buzzer.getStatus();
    
    sendMainMessage();
  }

  // El control de envío secundario ya está manejado cuando ocurren los cambios
  
  // Resetear flag de cambio reciente después de cierto tiempo
  if (stateJustChanged && (now - lastChangeTime > 500)) {
    stateJustChanged = false;
  }
}