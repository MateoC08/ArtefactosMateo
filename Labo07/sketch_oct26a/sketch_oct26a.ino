
#include <SPI.h>
#include <LoRa.h>

// ======================================
// IDENTIFICACION
// ======================================

const int MI_ID = 1;
const int DESTINO_ID = 2;

String mensajeOriginal = "SENSOR ACTIVO";

// ======================================
// PINES LORA
// ======================================

#define LORA_SCK  18
#define LORA_MISO 19
#define LORA_MOSI 23
#define LORA_SS   5

// ======================================
// SENSOR TOUCH
// ======================================

const int TOUCH_PIN = 27;
const int UMBRAL_TOUCH = 500;

// ======================================
// TEMPORIZADOR
// ======================================

const unsigned long INTERVALO = 5000;
unsigned long ultimoEnvio = 0;

// ======================================
// CIFRADO ATBASH
// ======================================

char convertirAtbash(char letra) {
  if (letra >= 'A' && letra <= 'Z') {
    return 'Z' - (letra - 'A');
  }

  if (letra >= 'a' && letra <= 'z') {
    return 'z' - (letra - 'a');
  }

  return letra;
}

String cifrarAtbash(String mensaje) {
  String resultado = "";

  for (int i = 0; i < mensaje.length(); i++) {
    resultado += convertirAtbash(
      mensaje.charAt(i)
    );
  }

  return resultado;
}

// ======================================
// ENVIAR MENSAJE
// ======================================

void enviarMensaje() {
  String mensajeCifrado =
    cifrarAtbash(mensajeOriginal);

  String paquete =
    String(MI_ID) + "|" +
    String(DESTINO_ID) + "|" +
    mensajeCifrado;

  Serial.println();
  Serial.println("ENVIANDO MENSAJE LORA");

  Serial.print("Mensaje original: ");
  Serial.println(mensajeOriginal);

  Serial.print("Mensaje cifrado: ");
  Serial.println(mensajeCifrado);

  LoRa.beginPacket();
  LoRa.print(paquete);

  int resultado = LoRa.endPacket();

  if (resultado == 1) {
    Serial.println("Transmision finalizada");
  } else {
    Serial.println("Error al transmitir");
  }

  Serial.print("Paquete enviado: ");
  Serial.println(paquete);

  Serial.println("----------------------");
}

// ======================================
// SETUP
// ======================================

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("TRANSMISOR LORA - ID1");

  Serial.print("Mi ID: ");
  Serial.println(MI_ID);

  Serial.print("Destino: ");
  Serial.println(DESTINO_ID);

  Serial.print("Umbral Touch: ");
  Serial.println(UMBRAL_TOUCH);

  SPI.begin(
    LORA_SCK,
    LORA_MISO,
    LORA_MOSI,
    LORA_SS
  );

  LoRa.setPins(LORA_SS, -1, -1);

  if (!LoRa.begin(915E6)) {
    Serial.println("ERROR: LoRa no inicia");

    while (true) {
      delay(1000);
    }
  }

  Serial.println("LoRa iniciado correctamente");
  Serial.println("Enviando cada 5 segundos");
  
  // Primer envio inmediato
  enviarMensaje();
  ultimoEnvio = millis();
}

// ======================================
// LOOP
// ======================================

void loop() {
  unsigned long tiempoActual = millis();

  // Leer sensor Touch para diagnostico
  int valorTouch = touchRead(TOUCH_PIN);

// Enviar automaticamente cada 5 segundos
if (tiempoActual - ultimoEnvio >= INTERVALO) {
  ultimoEnvio = tiempoActual;

  Serial.print("Valor Touch: ");
  Serial.println(valorTouch);

  enviarMensaje();
}

  delay(100);
}
