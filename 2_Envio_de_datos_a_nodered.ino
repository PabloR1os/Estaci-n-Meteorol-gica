#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <Adafruit_AHTX0.h>

// --- Configuración de Red WiFi ---
const char* ssid = "NOMBRE_DE_LA_RED";
const char* password = "CONTRASEÑA";

// --- Dirección IP de la computadora ejecutando Node-RED ---
const char* serverUrl = "http://192.168.1.XX:1880/clima";

Adafruit_AHTX0 aht;
const int RESET_PIN = D6;

// Variables para el temporizador sin bloqueo (millis)
unsigned long ultimoEnvio = 0;
const long intervaloEnvio = 3000; // Enviar datos cada 3000 ms (3 seg)

// Variables para el Antirrebote (Debounce)
int estadoBoton;
int ultimoEstadoBoton = HIGH;
unsigned long ultimoTiempoRebote = 0;
const unsigned long retardoRebote = 50; // 50 ms de tiempo de estabilización

void setup() {
  Serial.begin(115200);
  pinMode(RESET_PIN, INPUT_PULLUP);

  Serial.println("\n--- Iniciando Emisor AHT20 para Node-RED ---");

  if (!aht.begin()) {
    Serial.println("❌ ERROR: No se encontró el sensor AHT20");
    while (1) {
      if (digitalRead(RESET_PIN) == LOW) ESP.restart();
      delay(10);
    }
  }
  Serial.println("✅ Sensor AHT20 inicializado");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    // Incluso mientras conecta al WiFi, leemos el botón por si querés reiniciar
    if (digitalRead(RESET_PIN) == LOW) ESP.restart();
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Conectado exitosamente!");
}

void loop() {
  // 1. ANTIRREBOTE PARA EL BOTÓN DE RESET
  int lecturaActual = digitalRead(RESET_PIN);

  // Si hubo un cambio por ruido o pulsación, reiniciamos el reloj de rebote
  if (lecturaActual != ultimoEstadoBoton) {
    ultimoTiempoRebote = millis();
  }

  // Si el estado se mantuvo estable más de 50 ms, lo consideramos válido
  if ((millis() - ultimoTiempoRebote) > retardoRebote) {
    if (lecturaActual != estadoBoton) {
      estadoBoton = lecturaActual;

      // Si el estado confirmado es LOW, el usuario presionó el botón
      if (estadoBoton == LOW) {
        Serial.println(" Pulsador verificado: Reiniciando ESP8266...");
        delay(100); // Pequeña pausa de seguridad antes de reiniciar
        ESP.restart();
      }
    }
  }
  ultimoEstadoBoton = lecturaActual;

  // 2. TEMPORIZADOR DE ENVÍO SENSORIAL (MILLIS)

  if (millis() - ultimoEnvio >= intervaloEnvio) {
    ultimoEnvio = millis();

    if (WiFi.status() == WL_CONNECTED) {
      sensors_event_t humidity, temp;
      aht.getEvent(&humidity, &temp);

      String jsonPayload = "{";
      jsonPayload += "\"temperatura\":" + String(temp.temperature, 2) + ",";
      jsonPayload += "\"humedad\":" + String(humidity.relative_humidity, 2);
      jsonPayload += "}";

      WiFiClient client;
      HTTPClient http;
      http.begin(client, serverUrl);
      http.addHeader("Content-Type", "application/json");

      int httpResponseCode = http.POST(jsonPayload);
      if (httpResponseCode > 0) {
        Serial.print("✅ Datos enviados. Respuesta Node-RED: ");
        Serial.println(httpResponseCode);
      } else {
        Serial.print("❌ Error al enviar HTTP POST: ");
        Serial.println(httpResponseCode);
      }
      http.end();
    }
  }
}
