#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include "time.h"

// Pines y objetos
#define Buzzer 26
#define VibSens 12
#define MagnSens 32
#define myServo 14

Servo myServoMotor;
WiFiClientSecure wifiClient;
PubSubClient client(wifiClient);

// Estados
bool lastVibStatus;
uint8_t lastDoorStatus = 255;
uint16_t magnStatus;
bool accessState = true;
bool checkAccess = true;
uint16_t threshOpen = 2400, threshClosed = 2600;
bool accessCntrl = 0; // 0 (Manual) / 1 (LLM)

// WiFi/MQTT
const char* ssid = "Dian";
const char* password = "cursoiot";
const char* mqtt_server = "10.169.86.36";
const int mqtt_port = 8883;
const char* ID_disp = "ESP32_1";

const char* ca_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIEBTCCAu2gAwIBAgIUEmJ46CyjbB7s++/vEOYXiRCe+qkwDQYJKoZIhvcNAQEL\n" \
"BQAwgZExCzAJBgNVBAYTAkNPMRIwEAYDVQQIDAlBbnRpb3F1aWExETAPBgNVBAcM\n" \
"CE1lZGVsbGluMQ0wCwYDVQQKDARVZGVBMQwwCgYDVQQLDANJb1QxEDAOBgNVBAMM\n" \
"B0N1c29Jb1QxLDAqBgkqhkiG9w0BCQEWHWplZmZlcnNvbi5kdXJhbmdvQHVkZWEu\n" \
"ZWR1LmNvMB4XDTI1MDcxMTE0NTUzNloXDTI2MDcxMTE0NTUzNlowgZExCzAJBgNV\n" \
"BAYTAkNPMRIwEAYDVQQIDAlBbnRpb3F1aWExETAPBgNVBAcMCE1lZGVsbGluMQ0w\n" \
"CwYDVQQKDARVZGVBMQwwCgYDVQQLDANJb1QxEDAOBgNVBAMMB0N1c29Jb1QxLDAq\n" \
"BgkqhkiG9w0BCQEWHWplZmZlcnNvbi5kdXJhbmdvQHVkZWEuZWR1LmNvMIIBIjAN\n" \
"BgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAqxArmtmfapSZnoeqasVjIMJ0gR39\n" \
"CNsSCdPAvRsWomPSQgfe8OVevh19JrECCcz8StPDQYYH1OEwpiSf2MRk695UQrA2\n" \
"/FMJNRF8pFZnf487nqZYlDZ2OK/SAFif2cwS3pwtOGdDQOjQtfduQRiuOfPi5Ukc\n" \
"VF3ONPSB4zdURIilKJ6M7glwsM0BDFPqqqjPKEvYC4GIfzQx7KhwVAsaPSaWkGKv\n" \
"lgn4eeI9oPacowb3yAyEVqbjslX+QKc+2Xx0PdA6pstiwFkBeBzUPHDCvGojRe2d\n" \
"UG11jLK77jiFoC7f5QXNjaJ0CYRugmIdt3vtc/SG1pf72Kc/zzasRrw4SwIDAQAB\n" \
"o1MwUTAdBgNVHQ4EFgQUdG8J0zsAQn7uEgYI6eE6eQSVkB8wHwYDVR0jBBgwFoAU\n" \
"dG8J0zsAQn7uEgYI6eE6eQSVkB8wDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG9w0B\n" \
"AQsFAAOCAQEABrIDCfiR1teaUuZ13edJQhJ2rRSRKVz0xQUYG7K/HzcfuqGtetiW\n" \
"Gx6LHEj/Am2t9tO9uNU1YA/JlrrJOpsmtgEs1gNEdw67Y4xXFAa2uhBAelqOhdWV\n" \
"4XNtLobgGtnb+fkhaIvYiubaFIHa2RcvC93QFru8fCmcJfWIJi/sQHPSok9qn2/F\n" \
"wybi0WOIPSSDDx7mBwqDe+XryaE0YuLlj/tI1ew5+BNcKrMAQvMpAo61W+QXuy03\n" \
"wBPRczth3RTcvT6M4gdKRGHv+qpPNZcUGHEK88/u3Lc1aUa4qsqcVnfulq2VEFQf\n" \
"uJtnOYeWRTFFA7l3a5O9UqxPenJDt8i7LA==\n" \
"-----END CERTIFICATE-----\n";

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido en topic: ");
  Serial.println(topic);
  String topic_control = "door/control/" + String(ID_disp);

  if (strcmp(topic, topic_control.c_str()) == 0) {
    // Convertir el payload a string
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    
    Serial.print("Contenido del mensaje: ");
    Serial.println(message);
    
    // Parsear los valores (esperamos formato "checkAccess,accessState")
    char* token = strtok(message, ",");
    if (token != NULL) {
      checkAccess = (strcmp(token, "1") == 0);
      token = strtok(NULL, ",");
      if (token != NULL) {
        accessState = (strcmp(token, "1") == 0);
        Serial.print("Valores actualizados - checkAccess: ");
        Serial.print(checkAccess);
        Serial.print(", accessState: ");
        Serial.println(accessState);
      }
    }
  }
}

void setup_wifi() {

  Serial.print("Conectando a Wi-Fi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectado. IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando al broker MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("Conectado al broker");
      String topic_control = "door/control/" + String(ID_disp);
      client.subscribe(topic_control.c_str()); // Suscrito al topic para control de motor
    } else {
      Serial.print("Fallo, rc=");
      Serial.print(client.state());
      Serial.println(" Reintentando en 5 segundos");
      delay(5000);
    }
  }
}

void waitForNTP() {
  configTime(0, 0, "time.google.com", "time.windows.com", "co.pool.ntp.org");
  Serial.print("Esperando sincronización NTP");
  time_t now = time(nullptr);
  int retries = 0;
  const int maxRetries = 30; // 30 intentos = ~15 segundos
  while (now < 1000000000 && retries < maxRetries) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    retries++;
  }
  if (now >= 1000000000) {
    Serial.println("\nHora sincronizada correctamente.");
  } else {
    Serial.println("\nError: No se pudo sincronizar la hora.");
  }
}

void abrirPuerta(bool automaticDoor = true) {
  // Asegurar que el servo está adjunto (por si se desconectó)
  if (!myServoMotor.attached()) {
    myServoMotor.attach(myServo);
  }
  
  myServoMotor.write(180);
  delay(500); // Pequeña pausa para asegurar el movimiento

  if (automaticDoor) {
    delay(3000); // Espera 3 segundos (corrige el comentario)
    myServoMotor.write(0);
    delay(500); // Pequeña pausa para asegurar el movimiento
  }

  // Desactivar el servo para evitar consumo innecesario
  myServoMotor.detach();
}

void accesoPermitido() {
  tone(Buzzer, 880); delay(150);
  tone(Buzzer, 900); delay(150);
  tone(Buzzer, 944); delay(150);
  tone(Buzzer, 988); delay(150);
  tone(Buzzer, 1047); delay(200);
  noTone(Buzzer);
  abrirPuerta(true);
}

void accesoDenegado() {
  for (int i = 0; i < 5; i++) {
    tone(Buzzer, 400); delay(150);
    noTone(Buzzer); delay(100);
  }
  tone(Buzzer, 300); delay(300);
  noTone(Buzzer);
}

void alerta() {
  for (int i = 0; i < 10; i++) {
    tone(Buzzer, 200); delay(100);
    noTone(Buzzer); delay(100);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(Buzzer, OUTPUT);
  pinMode(VibSens, INPUT);
  pinMode(MagnSens, INPUT);
  myServoMotor.attach(myServo);

  setup_wifi();
  waitForNTP();
  wifiClient.setCACert(ca_cert);

  if (wifiClient.connect(mqtt_server, mqtt_port)) {
    Serial.println("Conexión TLS verificada");
    wifiClient.stop();
  }

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  delay(2000);
  myServoMotor.write(0);
  delay(2000);
  myServoMotor.detach();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  bool currentVibStatus = digitalRead(VibSens);
  magnStatus = analogRead(MagnSens);

  uint8_t newDoorStatus;
  const char* doorStat;

  if (magnStatus > threshClosed) {
    newDoorStatus = 0;
    doorStat = "Puerta cerrada";
  } else if (magnStatus >= threshOpen) {
    newDoorStatus = 1;
    doorStat = "Puerta mal cerrada";
  } else {
    newDoorStatus = 2;
    doorStat = "Puerta abierta";
  }

  // Solo publicar si el estado cambió
  if (newDoorStatus != lastDoorStatus) {
    lastDoorStatus = newDoorStatus;
    
    Serial.println(doorStat);
    
    // Publicar ambos valores
    char magnMsg[50];
    sprintf(magnMsg, "Campo Magn: %d", magnStatus);
    String topic_magnetic = "door/status/" + String(ID_disp);
    client.publish("sensors/magnetic", magnMsg);
    client.publish(topic_magnetic.c_str(), doorStat);
  }

  // Verificación de vibración (forcejeo)
  if (currentVibStatus == HIGH && lastVibStatus == LOW) {
    alerta();
    Serial.println("¡Intruso detectado!");
    client.publish("sensors/vibration", "Vib: 1");
    String topic_vibration = "door/intruder/" + String(ID_disp);
    client.publish(topic_vibration.c_str(), "Intruso detectado");
  } else if (currentVibStatus == LOW && lastVibStatus == HIGH) {
    delay(1000);
    Serial.println("No hay mas intrusos");
    client.publish("sensors/vibration", "Vib: 0");
    String topic_vibration = "door/intruder/" + String(ID_disp);
    client.publish(topic_vibration.c_str(), "No hay mas intrusos");
  }

  // Verificación de acceso
  const char* doorAccess;
  if (checkAccess) {
    if (accessState) {
      accesoPermitido();
      doorAccess = "Acceso Permitido";
    } else {
      accesoDenegado();
      doorAccess = "Acceso Denegado";
    }
    checkAccess = false;
    Serial.println(doorAccess);
    //client.publish("door/access", doorAccess);
    delay(100);
  }

  lastVibStatus = currentVibStatus;
  delay(750);
}
