#include <Adafruit_AHTX0.h>

Adafruit_AHTX0 aht;

const int RESET_PIN = D6; // Pin asignado al pulsador

void setup() {
  Serial.begin(115200);
  
  // Configura el pin D6 con pull-up interno (en reposo lee HIGH, al presionar lee LOW)
  pinMode(RESET_PIN, INPUT_PULLUP);

  Serial.println("\nIniciando sistema...");
  Serial.println("Adafruit AHT20 demo!");

  if (!aht.begin()) {
    Serial.println("No se pudo encontrar el sensor AHT20");
    while (1) {
      // Configuración del boton de reset fisico en caso de algún tipo de error
      if (digitalRead(RESET_PIN) == LOW) {
        Serial.println("Reinicio manual detectado...");
        delay(200);
        ESP.restart();
      }
      delay(10);
    }
  }

  Serial.println("Sensor AHT20 encontrado correctamente");
}

void loop() {
  // 1. Verificación del pulsador de reseteo fisico
  if (digitalRead(RESET_PIN) == LOW) {
    Serial.println("Pulsador presionado: Reiniciando ESP8266...");
    delay(200); // Pequeño rebote/antirrebote visual antes de reiniciar
    ESP.restart(); // Reinicia el microcontrolador
  }

  // 2. Lectura del sensor
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  Serial.print("Temperatura: ");
  Serial.print(temp.temperature);
  Serial.println(" °C");

  Serial.print("Humedad: ");
  Serial.print(humidity.relative_humidity);
  Serial.println(" %");

  delay(500);
}
