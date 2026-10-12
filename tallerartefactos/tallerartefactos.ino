#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h> // Librería para el sensor DHT11/DHT22

// ======================================================
// CONFIGURACIÓN WIFI
// ======================================================

const char* WIFI_SSID = "ARTEFACTOS";         // <-- Cambiar por el nombre de tu Wi-Fi
const char* WIFI_PASSWORD = "87654321"; // <-- Cambiar por la contraseña de tu Wi-Fi


// ======================================================
// CONFIGURACIÓN MQTT
// ======================================================

const char* MQTT_SERVER = "10.40.3.29"; // Tu dirección IP local obtenida de ipconfig[cite: 5]
const int MQTT_PORT = 1883;

// Topics utilizados por el dashboard React.
const char* TOPIC_TEMPERATURA = "clima/esp32/temperatura";
const char* TOPIC_HUMEDAD = "clima/esp32/humedad";
const char* TOPIC_LUZ = "clima/esp32/luz";


// ======================================================
// CONFIGURACIÓN DE SENSORES
// ======================================================

// (Estudiante)
// Pines asignados según tu circuito físico:
#define DHTPIN 4          // Cable verde conectado al pin G4 (GPIO 4)[cite: 10]
#define DHTTYPE DHT11     // Tipo de sensor (DHT11)
#define LDRPIN 34         // Cable naranja conectado al pin G34 (GPIO 34)[cite: 9]

DHT dht(DHTPIN, DHTTYPE);

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Enviar una lectura cada 2 segundos.
const unsigned long INTERVALO_ENVIO = 2000;

unsigned long ultimoEnvio = 0;

void conectarWiFi() {

  Serial.print("Conectando a WiFi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi conectado");

  Serial.print("IP del ESP32: ");
  Serial.println(WiFi.localIP());
}

void conectarMQTT() {

  while (!mqttClient.connected()) {

    Serial.print("Conectando a Mosquitto...");

    // Crear un Client ID único para este ESP32.
    String clientId = "ESP32-";
    clientId += String(
      (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF),
      HEX
    );

    if (mqttClient.connect(clientId.c_str())) {

      Serial.println(" conectado");

    } else {

      Serial.print(" error, rc=");
      Serial.print(mqttClient.state());
      Serial.println(". Reintentando en 2 segundos...");

      delay(2000);
    }
  }
}


// ======================================================
// LEER Y PUBLICAR SENSORES
// ======================================================

void publicarSensores() {

  // 1 y 2. Lecturas del sensor DHT11
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  // 3. Lectura analógica de la fotorresistencia (LDR)
  int luz = analogRead(LDRPIN);

  // Validación por si el sensor no responde
  if (isnan(temp) || isnan(hum)) {
    Serial.println("Error al leer el sensor DHT11");
    return;
  }

  // 4 y 5. Publicar las mediciones en sus respectivos topics MQTT
  mqttClient.publish(TOPIC_TEMPERATURA, String(temp).c_str());
  mqttClient.publish(TOPIC_HUMEDAD, String(hum).c_str());
  mqttClient.publish(TOPIC_LUZ, String(luz).c_str());

  // 6. Mostrar lecturas en el Monitor Serial
  Serial.print("Enviando -> Temp: ");
  Serial.print(temp);
  Serial.print(" °C | Humedad: ");
  Serial.print(hum);
  Serial.print(" % | Luz: ");
  Serial.println(luz);
}

void setup() {

  Serial.begin(115200);

  // (Estudiante) Inicializar el sensor DHT
  dht.begin();

  conectarWiFi();

  mqttClient.setServer(
    MQTT_SERVER,
    MQTT_PORT
  );
}

void loop() {

  // Si se pierde la conexión WiFi, reintentar
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  // Si se pierde la conexión MQTT, reintentar
  if (!mqttClient.connected()) {
    conectarMQTT();
  }

  mqttClient.loop();

  // Publicar sensores cada 2 segundos
  if (millis() - ultimoEnvio >= INTERVALO_ENVIO) {

    ultimoEnvio = millis();

    publicarSensores();
  }
}