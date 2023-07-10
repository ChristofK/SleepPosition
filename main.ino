#include <Wire.h>
#include <WiFiManager.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
// #include <ESP32SMTP.h>
#include <ESPAsyncWebSrv.h>

const int MPU_addr = 0x68;
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;

int minVal = 265;
int maxVal = 402;

double x;
double y;
double z;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

unsigned long previousMillis = 0;
const unsigned long interval = 2000; // 2 Sekunden

const int buttonPin = 2; // Pin des Tasters

bool sendEmail = false; // Flag für E-Mail-Versand

// E-Mail-Konfiguration
const char* smtpServer = "smtp.example.com"; // SMTP-Server-Adresse
const int smtpPort = 587; // SMTP-Server-Port
const char* emailSender = "sender@example.com"; // Absender-E-Mail-Adresse
const char* emailRecipient = "recipient@example.com"; // Empfänger-E-Mail-Adresse
const char* emailSubject = "Data CSV"; // Betreff der E-Mail
const char* emailMessage = "Anbei befindet sich die data.csv-Datei."; // Nachricht der E-Mail
const char* emailUsername = "username"; // Benutzername für den SMTP-Server
const char* emailPassword = "password"; // Passwort für den SMTP-Server

AsyncWebServer server(80);

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);

  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
  Serial.begin(9600);

  // WiFiManager
  WiFiManager wm;
  bool res;
  res = wm.autoConnect("AutoConnectAP", "password"); // password protected ap
  if (!res) {
    Serial.println("Failed to connect");
    // ESP.restart();
  }
  else {
    //if you get here you have connected to the WiFi    
    Serial.println("connected...yeey :)");
  }
  // END WiFiManager

  // SPIFFS initialisieren
  if (!SPIFFS.begin(true)) {
    Serial.println("Fehler beim Initialisieren von SPIFFS!");
    return;
  }

  // Datei öffnen oder erstellen (Dateiname: data.csv)
  File file = SPIFFS.open("/data.csv", FILE_APPEND);

  if (!file) {
    Serial.println("Fehler beim Öffnen der Datei!");
    return;
  }

  // Header in die Datei schreiben
  file.println("Time,AngleX");

  // Datei schließen
  file.close();

  // NTP-Client konfigurieren
  timeClient.begin();
  timeClient.update();

  // HTTP-Server konfigurieren
  server.on("/", HTTP_GET, handleRoot);
  server.on("/download", HTTP_GET, handleDownload);

  server.begin();
}

void loop() {
  // Zeit vom NTP-Server aktualisieren
  timeClient.update();

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    Wire.beginTransmission(MPU_addr);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_addr, 14, true);
    AcX = Wire.read() << 8 | Wire.read();
    AcY = Wire.read() << 8 | Wire.read();
    AcZ = Wire.read() << 8 | Wire.read();
    int xAng = map(AcX, minVal, maxVal, -90, 90);
    int yAng = map(AcY, minVal, maxVal, -90, 90);
    int zAng = map(AcZ, minVal, maxVal, -90, 90);

    x = RAD_TO_DEG * (atan2(-yAng, -zAng) + PI);
    y = RAD_TO_DEG * (atan2(-xAng, -zAng) + PI);
    z = RAD_TO_DEG * (atan2(-yAng, -xAng) + PI);
    int roundedValueX = round(x);

    // Datei öffnen (Dateiname: data.csv)
    File file = SPIFFS.open("/data.csv", FILE_APPEND);

    if (!file) {
      Serial.println("Fehler beim Öffnen der Datei!");
      return;
    }

    // Zeit und Daten in die Datei schreiben (als CSV formatiert)
    file.print(getFormattedTime());
    file.print(",");
    file.println(x);

    // Datei schließen
    file.close();

    Serial.print("AngleX= ");
    Serial.print(roundedValueX);
    Serial.print(" | ");
    if (x <= 45 || x >= 315) {
      Serial.println ("Supine");
    }
    else if (x < 315 && x >= 225) {
      Serial.println ("left side");
    }
    else if (x < 225 && x >= 135) {
      Serial.println ("Prone");
    }
    else if (x < 135 && x > 45) {
      Serial.println ("right side");
    }
    else {
      Serial.println ("ERROR");
    }
  }

  // Taster abfragen
  if (digitalRead(buttonPin) == LOW) {
    delay(50); // Entprellung

    if (digitalRead(buttonPin) == LOW) {
      sendEmail = true; // Flag für E-Mail-Versand setzen
    }
  }

  // E-Mail versenden, wenn Flag gesetzt ist
  if (sendEmail) {
    sendEmail = false; // Flag zurücksetzen

    // Datei öffnen (Dateiname: data.csv)
    File file = SPIFFS.open("/data.csv", FILE_READ);

    if (!file) {
      Serial.println("Fehler beim Öffnen der Datei!");
      return;
    }

    // Dateigröße ermitteln
    size_t fileSize = file.size();

    // Buffer für Dateiinhalt erstellen
    char* fileContent = new char[fileSize + 1];

    if (!fileContent) {
      Serial.println("Fehler beim Erstellen des Dateiinhalts-Puffers!");
      file.close();
      return;
    }

    // Dateiinhalt lesen
    size_t bytesRead = file.readBytes(fileContent, fileSize);
    fileContent[bytesRead] = '\0'; // Null-Terminator hinzufügen

    // Datei schließen
    file.close();

    // E-Mail-Versand konfigurieren
    // ESP32SMTP::QuickSetup(smtpServer, smtpPort, emailSender, emailUsername, emailPassword);

    // E-Mail versenden
    // bool success = ESP32SMTP::SendEmail(emailRecipient, emailSubject, emailMessage, fileContent, "data.csv");

    // Überprüfen, ob der E-Mail-Versand erfolgreich war
    // if (success) {
    //   Serial.println("E-Mail erfolgreich versendet!");
    // } else {
    //   Serial.println("Fehler beim Versenden der E-Mail!");
    // }

    // Speicher freigeben
    delete[] fileContent;
  }

 //server.handleClient();
}

String getFormattedTime() {
  String formattedTime = "";
  int hours = timeClient.getHours();
  int minutes = timeClient.getMinutes();
  int seconds = timeClient.getSeconds();

  if (hours < 10) {
    formattedTime += "0";
  }
  formattedTime += String(hours) + ":";

  if (minutes < 10) {
    formattedTime += "0";
  }
  formattedTime += String(minutes) + ":";

  if (seconds < 10) {
    formattedTime += "0";
  }
  formattedTime += String(seconds);

  return formattedTime;
}

void handleRoot(AsyncWebServerRequest* request) {
  // Aktuellen Wert aus data.csv auslesen
  File file = SPIFFS.open("/data.csv", FILE_READ);

  if (!file) {
    request->send(500, "text/plain", "Fehler beim Öffnen der Datei!");
    return;
  }

  // Dateiinhalt lesen
  size_t fileSize = file.size();
  char* fileContent = new char[fileSize + 1];

  if (!fileContent) {
    request->send(500, "text/plain", "Fehler beim Erstellen des Dateiinhalts-Puffers!");
    file.close();
    return;
  }

  size_t bytesRead = file.readBytes(fileContent, fileSize);
  fileContent[bytesRead] = '\0'; // Null-Terminator hinzufügen

  // Datei schließen
  file.close();

  // HTTP-Antwort mit dem aktuellen Wert und einem Link zum Herunterladen der data.csv senden
  String htmlResponse = "<html><body>";
  htmlResponse += "<h1>Aktueller Wert:</h1>";
  htmlResponse += "<p>" + String(x) + "</p>";
  htmlResponse += "<h2><a href=\"/download\">data.csv herunterladen</a></h2>";
  htmlResponse += "</body></html>";

  request->send(200, "text/html", htmlResponse);

  // Speicher freigeben
  delete[] fileContent;
}

void handleDownload(AsyncWebServerRequest* request) {
  // data.csv zum Herunterladen bereitstellen
  File file = SPIFFS.open("/data.csv", FILE_READ);

  if (!file) {
    request->send(500, "text/plain", "Fehler beim Öffnen der Datei!");
    return;
  }

  size_t fileSize = file.size();
  request->send(SPIFFS, "/data.csv", "text/csv", false);

  // Datei schließen
  file.close();
}
