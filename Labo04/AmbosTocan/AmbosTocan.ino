#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SH1106G display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

const int TOUCH_PIN1 = 32;
const int TOUCH_PIN2 = 33; 

const int UMBRAL_TOUCH = 200;

void setup() {

  Serial.begin(115200);

  Wire.begin(21, 22);

  if (!display.begin(OLED_ADDRESS, true)) {
    Serial.println("Error al iniciar la pantalla OLED");
    while (1);
  }

  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
}

void loop() {

  int valorTouch = touchRead(TOUCH_PIN1);
  int valorTouch2 = touchRead(TOUCH_PIN2);

  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(26, 5);
  display.println("Nada :v");

  if (valorTouch < UMBRAL_TOUCH) {

    display.setCursor(15, 28);
    display.println("Pin 1");

    display.fillCircle(105, 52, 5, SH110X_WHITE);
  }
  
  else if(valorTouch2 < UMBRAL_TOUCH){
    display.setCursor(32, 28);
    display.println("Pin 2");

    display.fillCircle(20, 52, 5, SH110X_WHITE);
  }

  else {

    display.setCursor(32, 28);
    display.println("No lo estas tocando manin");

    display.fillCircle(20, 52, 5, SH110X_WHITE);
  }

  display.display();

  delay(100);
}