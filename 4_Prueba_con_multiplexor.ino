#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <Adafruit_AHTX0.h>

// --- Configuración de Red WiFi ---
const char* ssid = "WALTERRIOS";
const char* password = "vivaBUENOSAIRESCHICO44";

// --- Dirección IP de la computadora ejecutando Node-RED ---
const char* serverUrl = "http://192.168.1.79:1880/clima";

Adafruit_AHTX0 aht;

// --- Pines del Multiplexor HCF4051BE ---
const int MUX_A = D5; // Bit 0 (LSB)
const int MUX_B = D7; // Bit 1
const int MUX_C = D8; // Bit 2 (MSB)
const int ANALOG_PIN = A0;

// --- Pin del Botón de Reset ---
const int RESET_PIN = D6;

// --- Temporizadores sin bloqueo ---
unsigned long ultimoEnvio = 0;
const long intervaloEnvio = 3000;

// --- Antirrebote Botón Reset ---
int estadoBoton;
int ultimoEstadoBoton = HIGH;
unsigned long ultimoTiempoRebote = 0;
const unsigned long retardoRebote = 50;

// Función para seleccionar el canal (0 a 7) en el HCF4051BE
void seleccionarCanalMux(byte canal) {
  digitalWrite(MUX_A, bitRead(canal, 0));
  digitalWrite(MUX_B, bitRead(canal, 1));
  digitalWrite(MUX_C, bitRead(canal, 2));
  delayMicroseconds(20); // Tiempo de estabilización de la conmutación
}

// Función para leer un canal analógico del MUX
int leerCanalMux(byte canal) {
  seleccionarCanalMux(canal);
  return analogRead(ANALOG_PIN);
}

void setup() {
  Serial.begin(115200);

  // Configuración de pines de control del MUX
  pinMode(MUX_A, OUTPUT);
  pinMode(MUX_B, OUTPUT);
  pinMode(MUX_C, OUTPUT);

  // Configuración del botón de reset
  pinMode(RESET_PIN, INPUT_PULLUP);

  Serial.println("\n--- Iniciando Estación Meteorológica con MUX HCF4051BE ---");

  // Inicialización AHT20
  if (!aht.begin()) {
    Serial.println("❌ ERROR: No se encontró el sensor AHT20");
    while (1) {
      if (digitalRead(RESET_PIN) == LOW) ESP.restart();
      delay(10);
    }
  }
  Serial.println("✅ Sensor AHT20 inicializado");

  // Conexión WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    if (digitalRead(RESET_PIN) == LOW) ESP.restart();
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Conectado exitosamente!");
}

void loop() {
  // 1. Antirrebote del Botón de Reset
  int lecturaActual = digitalRead(RESET_PIN);
  if (lecturaActual != ultimoEstadoBoton) {
    ultimoTiempoRebote = millis();
  }

  if ((millis() - ultimoTiempoRebote) > retardoRebote) {
    if (lecturaActual != estadoBoton) {
      estadoBoton = lecturaActual;
      if (estadoBoton == LOW) {
        Serial.println("Pulsador verificado: Reiniciando ESP8266...");
        delay(100);
        ESP.restart();
      }
    }
  }
  ultimoEstadoBoton = lecturaActual;

  // 2. Lectura y Envío cada 3 segundos
  if (millis() - ultimoEnvio >= intervaloEnvio) {
    ultimoEnvio = millis();

    if (WiFi.status() == WL_CONNECTED) {
      // Lectura del AHT20
      sensors_event_t humidity, temp;
      aht.getEvent(&humidity, &temp);

      // Lectura de los Canales del HCF4051BE
      int raw_ch0 = leerCanalMux(0); // Pruebas GND (debería dar ~0)
      int raw_ch1 = leerCanalMux(1); // Pruebas 3.3V (debería dar ~1023)
      int raw_ch2 = leerCanalMux(2); // Pruebas Divisor / Potenciómetro (debería dar ~512)

      // Construcción del JSON expandido
      String jsonPayload = "{";
      jsonPayload += "\"temperatura\":" + String(temp.temperature, 2) + ",";
      jsonPayload += "\"humedad\":" + String(humidity.relative_humidity, 2) + ",";
      jsonPayload += "\"mux_ch0\":" + String(raw_ch0) + ",";
      jsonPayload += "\"mux_ch1\":" + String(raw_ch1) + ",";
      jsonPayload += "\"mux_ch2\":" + String(raw_ch2);
      jsonPayload += "}";

      WiFiClient client;
      HTTPClient http;
      http.begin(client, serverUrl);
      http.addHeader("Content-Type", "application/json");

      int httpResponseCode = http.POST(jsonPayload);
      if (httpResponseCode > 0) {
        Serial.print("✅ Datos enviados. MUX CH0: ");
        Serial.print(raw_ch0);
        Serial.print(" | CH1: ");
        Serial.print(raw_ch1);
        Serial.print(" | CH2: ");
        Serial.println(raw_ch2);
      } else {
        Serial.print("❌ Error al enviar HTTP POST: ");
        Serial.println(httpResponseCode);
      }
      http.end();
    }
  }
}
