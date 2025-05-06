#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <time.h>  // Para funciones de fecha y hora

// Configuración de pines y sensores
#define Led 2
#define DHTPIN 4
#define DHTTYPE DHT11
#define MQ2_PIN 34
#define SERVO_PIN 13

DHT dht(DHTPIN, DHTTYPE);
Servo servoMotor;

// Configuración de red WiFi
const char* ssid = "xxxxxxxxxxx";
const char* password = "xxxxxxxxxxx";

// Configuración de MQTT
#define mqttUser "xxxxxxxxxxx"
#define mqttPass "xxxxxxxxxxx"
#define mqttPort 1883

char mqttBroker[] = "xxxxxxxxxxx";
char mqttClientId[] = "xxxxxxxxxxx";
char inTopic[] = "xxxxxxxxxxx";
char outTopic[] = "xxxxxxxxxxx";

// Configuración de NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -5 * 3600; // Colombia
const int daylightOffset_sec = 0;

WiFiClient BClient;
PubSubClient client(BClient);

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido [");
  Serial.print(topic);
  Serial.print("] ");

  String json;
  for (int i = 0; i < length; i++) {
    json += (char)payload[i];
  }
  Serial.println(json);

  StaticJsonDocument<300> doc;
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.println("❌ Error al deserializar JSON");
    return;
  }

  // Control del LED
  int estadoLed = doc["estado"];
  digitalWrite(Led, estadoLed == 1 ? HIGH : LOW);
  Serial.printf("LED %s\n", estadoLed == 1 ? "encendido" : "apagado");

  // Control del servomotor
  int comandoServo = doc["servo"];
  if (comandoServo == 1) {
    servoMotor.write(180);
    Serial.println("Servo girado a 180°");
  } else if (comandoServo == 0) {
    servoMotor.write(0);
    Serial.println("Servo girado a 0°");
  }
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi conectado");
  Serial.print("IP asignada: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conexión MQTT...");
    if (client.connect(mqttClientId, mqttUser, mqttPass)) {
      Serial.println("conectado ✅");
      client.subscribe(inTopic);
    } else {
      Serial.print("❌ fallo, rc=");
      Serial.print(client.state());
      Serial.println(" - reintentando en 5 segundos");
      delay(5000);
    }
  }
}

void setup() {
  pinMode(Led, OUTPUT);
  Serial.begin(115200);
  dht.begin();

  // Inicializar servomotor
  servoMotor.attach(SERVO_PIN);
  servoMotor.write(0);

  setup_wifi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  client.setServer(mqttBroker, mqttPort);
  client.setCallback(callback);

  Serial.println("✅ Setup completo");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Leer sensores
  float temperatura = dht.readTemperature();
  float humedad = dht.readHumidity();
  int gas = analogRead(MQ2_PIN);

  if (isnan(temperatura) || isnan(humedad)) {
    Serial.println("❌ Error al leer DHT11");
    return;
  }

  // Construir JSON
  StaticJsonDocument<256> doc;
  doc["temperatura"] = temperatura;
  doc["humedad"] = humedad;
  doc["gas"] = gas;

  // Obtener fecha y hora local
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeString[20];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M", &timeinfo);
    doc["fecha"] = timeString;
  } else {
    doc["fecha"] = "N/A";
  }

  // Publicar JSON
  char buffer[256];
  serializeJson(doc, buffer);
  client.publish(outTopic, buffer);

  Serial.println("📤 JSON publicado:");
  Serial.println(buffer);

  delay(10000); // Esperar 10 segundos entre publicaciones
}
