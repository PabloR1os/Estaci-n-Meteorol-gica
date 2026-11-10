 #include <Adafruit_AHTX0.h>
Adafruit_AHTX0 aht;

void setup() {
  // put your setup code here, to run once:
Serial.begin(115200);
Serial.println("Adafruit AHT20 demo!");
if(! aht.begin()){
  Serial.println("No se pudo encontrar el sensor");
  while(1) delay(10);
}
Serial.println("Sensor AHT20 encontrado");
}

void loop() {
  // put your main code here, to run repeatedly:
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);
  Serial.print("Temperatura: ");
  Serial.print(temp.temperature);
  Serial.println("°C");
  Serial.print("Humedad: ");
  Serial.print(humidity.relative_humidity);
  Serial.println("%");

  delay(500);
}
