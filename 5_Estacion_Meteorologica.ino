#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <BH1750.h>
#include <Adafruit_BMP085.h>

// --- Configuración de Red WiFi ---
const char* ssid = "Claro2667796";
const char* password = "30DF8D2333D0";
const char* serverUrl = "http://192.168.1.72:1880/clima"; // Ajusta con la IP de tu PC

// --- Instancias de Sensores ---
Adafruit_AHTX0 aht;
BH1750 luxometro;
Adafruit_BMP085 bmp;

// --- Definición de Pines ---
const int PIN_SUELO = A0;             // Salida analógica AO del HW-103
const int PIN_PLUVIOMETRO = D5;       // GPIO14 conectado al KY-003
const int RESET_PIN = D6;             // GPIO12 conectado al pulsador de reinicio

// --- Parámetros del Pluviómetro ---
const float RESOLUCION_LLUVIA = 0.3979;         // mm de precipitación por pulso
const unsigned long INTERVALO_LLUVIA = 60000;   // 1 minuto (60000 ms)

volatile unsigned int contadorPulsos = 0;
volatile unsigned long ultimoTiempoInterrupcion = 0;

unsigned long tiempoAnteriorLluvia = 0;
float lluviaUltimoMinuto = 0.0;
float lluviaAcumuladaTotal = 0.0;

// --- Temporizador de Envío HTTP ---
unsigned long ultimoEnvio = 0;
const long intervaloEnvio = 3000; // Enviar datos a Node-RED cada 3 segundos

// --- Antirrebote Botón Reset ---
int estadoBoton;
int ultimoEstadoBoton = HIGH;
unsigned long ultimoTiempoRebote = 0;
const unsigned long retardoRebote = 50;

// --- Variables de Estado de Sensores I2C ---
bool bmpDisponible = false;

// --- Rutina de Interrupción (ISR) del Pluviómetro ---
void IRAM_ATTR contarLluvia() {
  unsigned long tiempoActual = millis();
  if (tiempoActual - ultimoTiempoInterrupcion > 200) {
    contadorPulsos++;
    ultimoTiempoInterrupcion = tiempoActual;
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(); // Inicia bus I2C en D2 (SDA) y D1 (SCL)

  pinMode(RESET_PIN, INPUT_PULLUP);
  pinMode(PIN_PLUVIOMETRO, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_PLUVIOMETRO), contarLluvia, FALLING);

  Serial.println("\n--- Iniciando Estación Meteorológica Completa ---");

  // 1. Inicializar AHT20
  if (!aht.begin()) {
    Serial.println("❌ ERROR: No se encontró el sensor AHT20");
    while (1) {
      if (digitalRead(RESET_PIN) == LOW) ESP.restart();
      delay(10);
    }
  }
  Serial.println("✅ Sensor AHT20 inicializado");

  // 2. Inicializar BH1750
  if (luxometro.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("✅ Sensor BH1750 (Luxómetro) inicializado");
  } else {
    Serial.println("⚠️ Advertencia: No se detectó el BH1750");
  }

  // 3. Inicializar BMP180
  if (bmp.begin()) {
    bmpDisponible = true;
    Serial.println("✅ Sensor BMP180 (Presión/Altitud) inicializado");
  } else {
    Serial.println("⚠️ Advertencia: No se detectó el BMP180");
  }

  // 4. Conexión WiFi
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
  unsigned long tiempoActual = millis();

  // ==========================================
  // 1. ANTIRREBOTE DEL BOTÓN DE RESET (D6)
  // ==========================================
  int lecturaActual = digitalRead(RESET_PIN);
  if (lecturaActual != ultimoEstadoBoton) {
    ultimoTiempoRebote = tiempoActual;
  }

  if ((tiempoActual - ultimoTiempoRebote) > retardoRebote) {
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

  // ==========================================
  // 2. CÁLCULO DEL PLUVIÓMETRO CADA 1 MINUTO
  // ==========================================
  if (tiempoActual - tiempoAnteriorLluvia >= INTERVALO_LLUVIA) {
    noInterrupts();
    unsigned int pulsosEsteMinuto = contadorPulsos;
    contadorPulsos = 0;
    interrupts();

    lluviaUltimoMinuto = pulsosEsteMinuto * RESOLUCION_LLUVIA;
    lluviaAcumuladaTotal += lluviaUltimoMinuto;

    Serial.print("🌧️ Reporte 1 min -> Pulsos: ");
    Serial.print(pulsosEsteMinuto);
    Serial.print(" | Intensidad: ");
    Serial.print(lluviaUltimoMinuto, 2);
    Serial.print(" mm/min | Total: ");
    Serial.print(lluviaAcumuladaTotal, 2);
    Serial.println(" mm");

    tiempoAnteriorLluvia = tiempoActual;
  }

  // ==========================================
  // 3. LECTURA SENSORIAL Y ENVÍO A NODE-RED
  // ==========================================
  if (tiempoActual - ultimoEnvio >= intervaloEnvio) {
    ultimoEnvio = tiempoActual;

    if (WiFi.status() == WL_CONNECTED) {
      // AHT20
      sensors_event_t humidity, temp;
      aht.getEvent(&humidity, &temp);

      // BH1750
      float lux = luxometro.readLightLevel();
      if (lux < 0) lux = 0;

      // BMP180
      float presionHpa = 0.0;
      float altitudMetros = 0.0;
      if (bmpDisponible) {
        presionHpa = bmp.readPressure() / 100.0; // Conversión de Pa a hPa
        altitudMetros = bmp.readAltitude();
      }

      // HW-103
      int rawSuelo = analogRead(PIN_SUELO);
      int porcentajeSuelo = map(rawSuelo, 1023, 350, 0, 100);
      porcentajeSuelo = constrain(porcentajeSuelo, 0, 100);

      // JSON Unificado
      String jsonPayload = "{";
      jsonPayload += "\"temperatura\":" + String(temp.temperature, 2) + ",";
      jsonPayload += "\"humedad\":" + String(humidity.relative_humidity, 2) + ",";
      jsonPayload += "\"luz\":" + String(lux, 1) + ",";
      jsonPayload += "\"suelo\":" + String(porcentajeSuelo) + ",";
      jsonPayload += "\"lluvia_min\":" + String(lluviaUltimoMinuto, 2) + ",";
      jsonPayload += "\"lluvia_total\":" + String(lluviaAcumuladaTotal, 2) + ",";
      jsonPayload += "\"presion\":" + String(presionHpa, 2) + ",";
      jsonPayload += "\"altitud\":" + String(altitudMetros, 1);
      jsonPayload += "}";

      WiFiClient client;
      HTTPClient http;
      http.begin(client, serverUrl);
      http.addHeader("Content-Type", "application/json");

      int httpResponseCode = http.POST(jsonPayload);
      if (httpResponseCode > 0) {
        Serial.print("✅ JSON enviado: ");
        Serial.println(jsonPayload);
      } else {
        Serial.print("❌ Error HTTP POST: ");
        Serial.println(httpResponseCode);
      }
      http.end();
    }
  }
}
