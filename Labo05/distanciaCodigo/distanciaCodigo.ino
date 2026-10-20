//ahola
#include <WiFi.h>
#include "WiFiClientSecure.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <ESP32Servo.h>

// ---------------------- CONFIGURACIÓN WI-FI ----------------------
#define WLAN_SSID   "ARTEFACTOS"
#define WLAN_PASS   "87654321"

// ---------------------- CONFIGURACIÓN ADAFRUIT IO ----------------------
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "TU_USUARIO_ADAFRUIT"
#define AIO_KEY         "TU_KEY_ADAFRUIT"

// ---------------------- PINES ----------------------
#define TRIG_PIN   18
#define ECHO_PIN   19

#define PIN_R      25
#define PIN_G      26
#define PIN_B      27

#define SERVO_PIN  13

// ---------------------- AJUSTES ----------------------
#define RGB_ANODO_COMUN false
#define PWM_FREQ        5000
#define PWM_RES         8

#define INTERVALO_PUBLICAR_MS 5000

#define DIST_MIN_CM       0.0
#define DIST_MAX_CM      30.0
#define NUM_NIVELES       3

#define DIST_APERTURA_CM 10.0

#define ANGULO_CERRADO  0
#define ANGULO_ABIERTO 90

// ---------------------- SERVO ----------------------
Servo puerta;

// ---------------------- CLIENTE MQTT Y FEEDS ----------------------
WiFiClient client;

Adafruit_MQTT_Client mqtt(
  &client,
  AIO_SERVER,
  AIO_SERVERPORT,
  AIO_USERNAME,
  AIO_KEY
);

Adafruit_MQTT_Publish feedDistancia =
  Adafruit_MQTT_Publish(
    &mqtt, AIO_USERNAME "/feeds/distancia"
  );

Adafruit_MQTT_Publish feedAlarma =
  Adafruit_MQTT_Publish(
    &mqtt, AIO_USERNAME "/feeds/alarma"
  );

Adafruit_MQTT_Publish feedPuerta =
  Adafruit_MQTT_Publish(
    &mqtt, AIO_USERNAME "/feeds/puerta"
  );

// ---------------------- ESTADO ----------------------
float ultimaDistancia = -1;

unsigned long ultimoPublicar = 0;
unsigned long ultimoPing = 0;
unsigned long ultimoIntentoWiFi = 0;
unsigned long ultimoIntentoMQTT = 0;

int nivelAlarma = 0;
bool puertaAbierta = false;

// ---------------------- PROTOTIPOS ----------------------
void conectarWiFi();
void conectarMQTT();

float leerDistanciaCm();
float distanciaPromedio(int muestras);

int calcularAlarma(float valor, float minimo,
                   float maximo, int n);

void escribirRGB(uint8_t r, uint8_t g, uint8_t b);
void actualizarLED();
void actualizarPuerta();

// =====================================================================
// SETUP
// =====================================================================
void setup() {

  Serial.begin(115200);
  delay(10);

  // Configurar ultrasónico
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  // Configurar LED RGB
  ledcAttach(PIN_R, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_G, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_B, PWM_FREQ, PWM_RES);

  escribirRGB(0, 0, 0);

  // Configurar servomotor
  puerta.setPeriodHertz(50);
  puerta.attach(SERVO_PIN, 500, 2400);

  // Iniciar puerta cerrada
  puerta.write(ANGULO_CERRADO);

  conectarWiFi();

  Serial.println("Sistema de puerta iniciado");
}

// =====================================================================
// LOOP PRINCIPAL
// =====================================================================
void loop() {

  conectarWiFi();
  conectarMQTT();

  // Medir distancia cada cierto tiempo
  if (millis() - ultimoPublicar >= INTERVALO_PUBLICAR_MS) {

    ultimoPublicar = millis();

    float d = distanciaPromedio(5);

    Serial.println();
    Serial.println("========== MONITOREO ==========");

    if (d > 0) {

      ultimaDistancia = d;

      Serial.print("Distancia: ");
      Serial.print(d, 1);
      Serial.println(" cm");

      // Calcular alarma
      nivelAlarma = calcularAlarma(
        d,
        DIST_MIN_CM,
        DIST_MAX_CM,
        NUM_NIVELES
      );

      Serial.print("Nivel de alarma: ");
      Serial.println(nivelAlarma);

      // Abrir puerta a menos de 10 cm
      if (d < DIST_APERTURA_CM) {
        puertaAbierta = true;
      } else {
        puertaAbierta = false;
      }

    } else {

      ultimaDistancia = -1;
      nivelAlarma = 0;
      puertaAbierta = false;

      Serial.println("Error: no se detecta distancia");
    }

    // Actualizar componentes físicos
    actualizarLED();
    actualizarPuerta();

    // Publicar en Adafruit
    if (mqtt.connected()) {

      if (ultimaDistancia > 0) {

        if (!feedDistancia.publish(ultimaDistancia)) {
          Serial.println("Error publicando distancia");
        }
      }

      if (!feedAlarma.publish((int32_t)nivelAlarma)) {
        Serial.println("Error publicando alarma");
      }

      const char *estadoPuerta;

      if (ultimaDistancia <= 0) {
        estadoPuerta = "SIN LECTURA";
      } else if (puertaAbierta) {
        estadoPuerta = "ABIERTA";
      } else {
        estadoPuerta = "CERRADA";
      }

      if (!feedPuerta.publish(estadoPuerta)) {
        Serial.println("Error publicando puerta");
      }
    }
  }

  // Mantener MQTT conectado
  if (mqtt.connected() &&
      millis() - ultimoPing >= 30000) {

    ultimoPing = millis();

    if (!mqtt.ping()) {
      Serial.println("Conexion MQTT perdida");
      mqtt.disconnect();
    }
  }
}

// =====================================================================
// ULTRASÓNICO HC-SR04
// =====================================================================
float leerDistanciaCm() {

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duracion = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duracion == 0) return -1;

  float distancia = (duracion * 0.0343) / 2.0;

  if (distancia < 2 || distancia > 100) return -1;

  return distancia;
}

// =====================================================================
// PROMEDIO DE DISTANCIAS
// =====================================================================
float distanciaPromedio(int muestras) {

  float suma = 0;
  int validas = 0;

  for (int i = 0; i < muestras; i++) {

    float d = leerDistanciaCm();

    if (d > 0) {
      suma += d;
      validas++;
    }

    delay(40);
  }

  return (validas > 0) ? suma / validas : -1;
}

// =====================================================================
// FUNCIÓN DE ALARMA DE N NIVELES
// =====================================================================
int calcularAlarma(float valor, float minimo,
                   float maximo, int n) {

  if (n <= 0 || maximo <= minimo) {
    return 0;
  }

  if (valor < minimo || valor > maximo) {
    return 0;
  }

  float intervalo = (maximo - minimo) / n;

  // Mayor proximidad = mayor alarma
  if (valor == maximo) {
    return 1;
  }

  int posicion = int((valor - minimo) / intervalo);

  return n - posicion;
}

// =====================================================================
// LED RGB
// =====================================================================
void escribirRGB(uint8_t r, uint8_t g, uint8_t b) {

  if (RGB_ANODO_COMUN) {
    r = 255 - r;
    g = 255 - g;
    b = 255 - b;
  }

  ledcWrite(PIN_R, r);
  ledcWrite(PIN_G, g);
  ledcWrite(PIN_B, b);
}

// =====================================================================
// ACTUALIZAR LED SEGÚN ALARMA
// =====================================================================
void actualizarLED() {

  if (nivelAlarma == 3) {

    // Muy cerca
    escribirRGB(0, 255, 0);

  } else if (nivelAlarma == 2) {

    // Distancia media
    escribirRGB(255, 255, 0);

  } else if (nivelAlarma == 1) {

    // Lejos
    escribirRGB(255, 0, 0);

  } else {

    // Sin alarma o lectura inválida
    escribirRGB(0, 0, 0);
  }
}

// =====================================================================
// CONTROL DEL SERVOMOTOR
// =====================================================================
void actualizarPuerta() {

  if (puertaAbierta) {

    puerta.write(ANGULO_ABIERTO);
    Serial.println("PUERTA ABIERTA - Servo 90 grados");

  } else {

    puerta.write(ANGULO_CERRADO);

    if (ultimaDistancia <= 0) {
      Serial.println("SIN LECTURA - Servo a posicion cerrada");
    } else {
      Serial.println("PUERTA CERRADA - Servo 0 grados");
    }
  }
}

// =====================================================================
// CONEXIÓN WI-FI
// =====================================================================
void conectarWiFi() {

  if (WiFi.status() == WL_CONNECTED) return;

  if (ultimoIntentoWiFi != 0 &&
      millis() - ultimoIntentoWiFi < 10000) {
    return;
  }

  ultimoIntentoWiFi = millis();

  Serial.print("Conectando a ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);

  unsigned long inicio = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - inicio < 10000) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.print("WiFi conectado. IP: ");
    Serial.println(WiFi.localIP());

  } else {

    Serial.println("No se pudo conectar al WiFi");
  }
}

// =====================================================================
// CONEXIÓN MQTT
// =====================================================================
void conectarMQTT() {

  if (WiFi.status() != WL_CONNECTED) return;

  if (mqtt.connected()) return;

  if (ultimoIntentoMQTT != 0 &&
      millis() - ultimoIntentoMQTT < 5000) {
    return;
  }

  ultimoIntentoMQTT = millis();

  Serial.print("Conectando a Adafruit IO... ");

  int8_t ret = mqtt.connect();

  if (ret != 0) {

    Serial.println(mqtt.connectErrorString(ret));
    mqtt.disconnect();
    return;
  }

  Serial.println("Conectado a Adafruit IO!");

  ultimoPing = millis();
}
