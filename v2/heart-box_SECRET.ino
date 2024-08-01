#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include <WiFiClientSecure.h>
#include <esp32-hal-ledc.h> // for servo
#include <SPIFFS.h>
#include <SPI.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include <Preferences.h>
#include <ArduinoJson.h>  // Bibliothèque pour traiter les données JSON
#include <HTTPClient.h>
#include <Arduino.h>
#include <qrcode.h>  // Assurez-vous d'inclure une bibliothèque QR Code
#include <pngle.h>

unsigned long startTime = 0;
bool isTiming = false;
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
bool firstBoot = true; // Variable pour suivre si c'est le premier démarrage
bool afficher_message = false;

#define LIGHT_SENSE_PIN 34
#define LED_RED 1
#define LED_GREEN 1
#define LED_BLUE 1
#define SERVO_PIN 21
#define BACKLIGHT_PIN 27
#define MID_POS 95 // The middle position for the servo that has the heart pointing down

#include <PNGdec.h>
#include <AnimatedGIF.h>
AnimatedGIF gif;
PNG png;
#define MAX_IMAGE_WIDTH 320 // Adjust for your images

uint8_t* image = nullptr; // Variable globale pour stocker l'image
int imageWidth, imageHeight;
int16_t xpos = 0;
int16_t ypos = 0;

#include <DNSServer.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <UniversalTelegramBot.h>
#define BOTtoken "6898173842:AAHH9U6j6sppDBDy0nxYJl84qyzgZMehO64"  // your Bot Token (Get from Botfather)
const char* serverName = "https://arcabox.onrender.com/api/show_image_status";

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

Servo myServo; // Déclarez un objet Servo


DNSServer dnsServer;
AsyncWebServer server(80);

Preferences preferences; // Preferences object for NVS

String ssid;
String password;
String chat_id;

String old_user_input = "";  
String user_input = "";             // Variable pour stocker l'ancienne entrée utilisateur
bool new_message_available = false;     // Indicateur pour vérifier si un nouveau message est disponible

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="fr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Configuration du Wi-Fi</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
      margin: 0;
      background-color: #ffe4e1;
      font-family: "Lucida Console", "Courier New", monospace;
    }
    .container {
      background: white;
      padding: 20px;
      border-radius: 8px;
      box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
      text-align: center;
      border: 2px solid #ff69b4;
    }
    h3 {
      margin-bottom: 20px;
      color: #ff1493;
    }
    form {
      display: flex;
      flex-direction: column;
      align-items: center;
    }
    label {
      width: 100%;
      text-align: left;
      margin: 5px 0;
      color: #ff1493;
    }
    input[type="text"], input[type="password"] {
      width: 100%;
      padding: 10px;
      margin: 10px 0;
      border: 1px solid #ff69b4;
      border-radius: 4px;
      box-sizing: border-box;
    }
    input[type="submit"] {
      background-color: #ff69b4;
      color: white;
      border: none;
      padding: 10px 20px;
      border-radius: 4px;
      cursor: pointer;
      transition: background-color 0.3s;
    }
    input[type="submit"]:hover {
      background-color: #ff1493;
    }
  </style>
</head>
<body>
  <div class="container">
    <h3>Configuration du Wi-Fi</h3>
    <form action="/post" method="POST">
      <label for="ssid">Nom du Wi-Fi:</label>
      <input type="text" id="ssid" name="ssid" required>
      <label for="password">Mot de passe:</label>
      <input type="password" id="password" name="password" required>
      <label for="chat_id">ID utilisateur Telegram:</label>
      <input type="text" id="chat_id" name="chat_id" required>
      <input type="submit" value="Entrer">
    </form>
  </div>
</body>
</html>
)rawliteral";

class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request) {
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  }
};

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  


  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) {
    delay(1000);
  }
  tft.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi!");
    tft.println("");
    tft.println("  Connexion reussie !");
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
  } else {
    Serial.println("Failed to connect to WiFi.");
    tft.println("  Impossible de se connecter au WiFi");
    tft.println("  La boite va redemarrer ...");
    delay(15000);
    preferences.clear();
    ESP.restart();  // Redémarrer l'ESP
  }
}

void setupServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
    Serial.println("Client Connected");
  });

  server.on("/post", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("ssid", true)) {
      ssid = request->getParam("ssid", true)->value();
      Serial.println("\nSSID: " + ssid);
    }

    if (request->hasParam("password", true)) {
      password = request->getParam("password", true)->value();
      Serial.println("Password: " + password);
    }

    if (request->hasParam("chat_id", true)) {
      String chat_id = request->getParam("chat_id", true)->value();
      Serial.println("Chat ID: " + chat_id);
      preferences.putString("chat_id", chat_id);
    }

    request->send(200, "text/html", R"rawliteral(
<!DOCTYPE HTML>
<html lang="fr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Succès de la configuration</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
      margin: 0;
      background-color: #ffe4e1;
      font-family: "Lucida Console", "Courier New", monospace;
    }
    .container {
      background: white;
      padding: 20px;
      border-radius: 8px;
      box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
      text-align: center;
      border: 2px solid #ff69b4;
    }
    h2 {
      color: #ff1493;
      margin-bottom: 20px;
    }
    h3 {
      color: #333;
    }
  </style>
</head>
<body>
  <div class="container">
    <h2>C'est tout bon !</h2>
    <h3>Tu peux te reconnecter à ton Wi-Fi et suivre les instructions sur l'écran</h3>
  </div>
</body>
</html>
)rawliteral");

    xTaskCreate([](void *parameter) {
      connectToWiFi();
      vTaskDelete(NULL);
    }, "WiFiTask", 4096, NULL, 1, NULL);
  });
}



const char *render_site = "https://arcabox.onrender.com///longPoll";

enum State {
  WAITING_FOR_IMAGE,
  WAITING_TO_DISPLAY_GIF,
  WAITING_TO_DISPLAY_PNG,
  DISPLAYING_GIF,
  DISPLAYING_PNG,
  GET_USER_INPUT  // Nouvel état pour obtenir l'entrée utilisateur
};

State currState = WAITING_FOR_IMAGE;

// holds the current upload
File fsUploadFile;
File gifFile; // Global File object for the GIF file

void setClock() {
  configTime(0, 0, "pool.ntp.org");

  Serial.print(F("Waiting for NTP time sync: "));
  time_t nowSecs = time(nullptr);
  while (nowSecs < 8 * 3600 * 2) {
    delay(500);
    Serial.print(F("."));
    yield();
    nowSecs = time(nullptr);
  }

  Serial.println();
  struct tm timeinfo;
  gmtime_r(&nowSecs, &timeinfo);
  Serial.print(F("Current time: "));
  Serial.print(asctime(&timeinfo));
}






void setup() {
  Serial.begin(115200);
  Serial.println();
  myServo.attach(SERVO_PIN); // Attachez le servo au pin défini
  clearWiFiCredentials();

  // Initialize NVS
  preferences.begin("wifi-creds", false);

  // Load WiFi credentials and chat ID
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  chat_id = preferences.getString("chat_id", chat_id); // Default to CHAT_ID if not found

  // Initialize TFT
  tft.begin();
  tft.setRotation(3); // Change orientation to horizontal

  if (firstBoot) {
    displayInitialMessage(); // Afficher le message initial
    delay(30000);
  }

  if (ssid.isEmpty() || password.isEmpty() || chat_id.isEmpty()) {
    Serial.println("No saved WiFi credentials or chat ID.");
    Serial.println("Setting up AP Mode");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Projet Marmotte");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    tft.init();
    
    // Affichage des instructions avec les cœurs rouges

    tft.setRotation(3);
    tft.fillScreen(TFT_WHITE);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(0, 0);
    tft.println("");
    tft.println("  Connecte toi au wifi Projet Marmotte");
    tft.println("");
    tft.println("  Va dans ton navigateur");
    tft.println("");
    tft.print("  Entre : ");
    tft.println(WiFi.softAPIP());

    Serial.println("Setting up Async WebServer");
    setupServer();
    Serial.println("Starting DNS Server");
    dnsServer.start(53, "*", WiFi.softAPIP());
    server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER); // only when requested from AP
    server.begin();
    Serial.println("All Done!");

    digitalWrite(LED_GREEN, LOW);  // Indicate AP mode with LED
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_BLUE, LOW);

  } else {
    connectToWiFi();
  }

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  SPIFFS.format();

  // Initialize PWM for servo
  ledcSetup(0, 50, 16);
  ledcAttachPin(SERVO_PIN, 0);
  ledcWrite(0, pulseWidth(MID_POS));

  setClock(); // Set the NTP time

  Serial.println("Setup complete.");

  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.println("");
  tft.print("  Tout est pret a etre utilise :)");
  delay(5000);
  tft.fillScreen(TFT_WHITE);
  tft.setRotation(2); // Change orientation to horizontaldownloadImage
  bot.sendMessage(chat_id, "Hello !\n\nJoyeux anniversaire !\n\nJe vais utiliser ce Bot pour t'envoyer une notif quand tu as reçu un message mais que tu ne l'as pas regardé apres une heure.\n\nA+ 😘");
  downloadImage("https://arcabox.onrender.com/coeur");
}

void downloadImage(const char* url) {
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        WiFiClient* stream = http.getStreamPtr();
        File file = SPIFFS.open("/coeur.png", FILE_WRITE);

        if (!file) {
            Serial.println("Failed to open file for writing");
            return;
        }

        uint8_t buffer[512];
        int len;

        while ((len = stream->read(buffer, sizeof(buffer))) > 0) {
            file.write(buffer, len);
        }

        file.close();
        Serial.println("Image downloaded successfully");
    } else {
        Serial.printf("Failed to download image, HTTP code: %d\n", httpCode);
    }

    http.end();
}


void showImageFromInternet(){
  // Scan LittleFS and load any *.png files¨
  tft.begin();
    tft.fillScreen(TFT_WHITE);
    tft.setRotation(3);
    tft.setCursor(0,0);
    tft.print("\n\n Tu as recu un message\n\n Scan le QRcode ou rends toi ici :\n\n https://arcabox.onrender.com/noa");
    tft.setRotation(2);
    tft.setCursor(0,240);
  Serial.println("Loading image");
  File file = SPIFFS.open("/coeur.png", "r");
  String strname = file.name();
  strname = "/" + strname;
  Serial.println(file.name());
  // If it is not a directory and filename ends in .png then load it
  if (!file.isDirectory() && strname.endsWith(".png")) {
    
    // Pass support callback function names to library
    int16_t rc = png.open(strname.c_str(), pngOpen, pngClose, pngRead, pngSeek, pngDraw);
    if (rc == PNG_SUCCESS) {
      tft.startWrite();
      Serial.printf("image specs: (%d x %d), %d bpp, pixel type: %d\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType());
      //480 x 320
      xpos = (320/2 - png.getWidth()/2) - 50;
      ypos = (480/2 - png.getHeight()/2);
      uint32_t dt = millis();
      if (png.getWidth() > MAX_IMAGE_WIDTH) {
        Serial.println("Image too wide for allocated line buffer size!");
      }
      else {
        rc = png.decode(NULL, 0);
        png.close();
        delay(5000);
      }
      tft.endWrite();
      // How long did rendering take...
      Serial.print(millis()-dt); Serial.println("ms");
    }else{
      Serial.println(rc);
      Serial.println("Failed to load image");
    }
  }
}




void displayInitialMessage() {
  tft.fillScreen(TFT_WHITE); // Fond noir
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(2); // Taille de police 2

  tft.println(" "); // Message à afficher
  tft.print(" Coucou ma petite Noa !");
  tft.print("             ");
  tft.print("\n\n Joyeux anniversaire");
  tft.print("\n\n Tu es enfin devenue une grande marmotte ");
  tft.println("\n\n Ca fait 2 ans que je te connais et tu   es la fille la plus incroyable que \n j'ai jamais vue.         \n\n Tu es extraordinaire et j'ai vraiment   de la chance de t'avoir et d'etre  amis avec toi :)"); 
  tft.print("\n\n T'es une fille super !");
  tft.print("\n\n Ne change rien :)");
}

void effacerTexte() {
  // Effacer le texte affiché à l'écran
  tft.fillScreen(TFT_BLACK); // Supprimer le texte en remplissant l'écran en noir
}


void ShowText() {
  old_user_input = user_input;
  HTTPClient http;
  String serverAddress = "https://arcabox.onrender.com/poll";  // Adresse de mon serveur Flask

  if (http.begin(serverAddress)) {  // Démarre la connexion HTTP
    int httpCode = http.GET();     // Envoie la requête GET
    if (httpCode > 0) {            // Vérifie le code de retour
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();  // Récupère le contenu de la réponse

        // Parse JSON
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
          const char* new_user_input = doc["user_input"];
          http.end();
          
          if (String(new_user_input) != "") {
            Serial.print("Got message : "); Serial.print(new_user_input); Serial.print("\n");
            if (check()) {
              Serial.print("L'user input est : ");
              Serial.print(new_user_input);
 
              // Effacer l'écran TFT et afficher le message
              tft.init();
              tft.setRotation(3);
              tft.fillScreen(TFT_WHITE);
              tft.setTextColor(TFT_BLACK);
              tft.setTextSize(2);
              tft.setCursor(10, 10);
              tft.println("  ");
              tft.print(new_user_input);
              tft.setRotation(2); // Réinitialiser la rotation à la valeur par défaut
              isTiming = false;  // Réinitialise le chronométrage
            } else {
              }
            }
          

          // Réinitialiser new_message_available après avoir traité le nouveau message
          new_message_available = false;
        } else {
          Serial.print("deserializeJson() failed: ");
          Serial.println(error.c_str());
        }
      }
    } else {
      Serial.printf("[HTTP] GET... failed, connection failed or timed out\n");
    }
    http.end();  // Libère les ressources
  } else {
    Serial.printf("[HTTP] Unable to connect\n");
  }
  currState = WAITING_FOR_IMAGE;  // Changer l'état après le traitement
}



void new_string() {

  // Vérifier la connexion Wi-Fi
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // Spécifier l'URL
    http.begin("https://arcabox.onrender.com//delete_user_input");
    
    // Effectuer la requête GET
    int httpResponseCode = http.GET();
    
    // Vérifier le code de réponse
    if (httpResponseCode > 0) {
      // Lire la réponse
      String payload = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(payload);
    } else {
      Serial.print("Erreur lors de la requête : ");
      Serial.println(httpResponseCode);
    }
    
    // Fermer la connexion
    http.end();
  } else {
    Serial.println("Erreur de connexion au Wi-Fi");
  }
  
  // Attendre avant de refaire une requête
  delay(10000);
}

bool check() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println(payload);

      // Parse JSON
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error) {
        bool show_image = doc["show_image"];
        Serial.print("show_image: ");
        Serial.println(show_image);
        
        // Retourne le booléen correspondant à show_image
        return show_image;
      } else {
        Serial.print("Erreur de parsing JSON: ");
        Serial.println(error.c_str());
      }
    } else {
      Serial.print("Erreur lors de la requête HTTP: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi non connecté");
  }

  delay(10000); // Attendre 10 secondes avant de faire une nouvelle requête
  return false; // Retourne false par défaut en cas d'erreur
}

void loop() {
  dnsServer.processNextRequest();

  HTTPClient http;

  handleSerialInput(); 

  if(afficher_message) {
    Serial.print("L'user input a envoyé sur telegram est : ");
    Serial.print(user_input);
    Serial.print("\n");
    bot.sendMessage(chat_id, "Tu as reçu un message, tu peux le voir ici : https://arcabox.onrender.com/messages");
    new_string(); 
    user_input = "";
    afficher_message = false; 
  }

  if(String(user_input) != ""){ 
    new_string();
    user_input = "";
  }

  if (Serial.available() > 0) {
    // Lire la commande entrée par l'utilisateur
    String command = Serial.readStringUntil('\n');
    command.trim(); // Supprimer les espaces inutiles autour de la commande

    // Vérifier si la commande est "reset firstboot"
    if (command.equalsIgnoreCase("reset firstboot")) {
      // Réinitialiser l'état firstboot à true (mettre votre logique ici)
      firstBoot = true; // Suppose que firstboot est une variable globale définie ailleurs

      // Confirmation dans le Serial Monitor
      Serial.println("firstboot reset to true.");
    } else {
      // Commande non reconnue
      Serial.println("Unknown command.");
    }
  }

  switch (currState) {
    case WAITING_FOR_IMAGE:
      pollForImage();
      pollForMessage();
      digitalWrite(BACKLIGHT_PIN, LOW);
      break;

    case WAITING_TO_DISPLAY_GIF:
      if (analogRead(LIGHT_SENSE_PIN) < 5) {
        digitalWrite(BACKLIGHT_PIN, HIGH);
        showGif(true);
        currState = DISPLAYING_GIF;
      } else {
        servoWiggle(); 
      }
      break;

    case WAITING_TO_DISPLAY_PNG:
      if (check()) {
        digitalWrite(BACKLIGHT_PIN, HIGH);
        showImage();
        delay(60000);
        tft.fillScreen(TFT_BLACK);
        // Éteindre l'écran
        digitalWrite(BACKLIGHT_PIN, LOW);  // Couper l'alimentation de l'écran
        currState = DISPLAYING_PNG;
        isTiming = false;  // Réinitialise le chronométrage
      } else {
        if (!isTiming) {
          startTime = millis();  // Commence le chronométrage
          isTiming = true;
        }
        
        if (millis() - startTime > 60 * 1000) {  // Vérifie si 1 minute s'est écoulée
          bot.sendMessage(chat_id, "Tu as reçu une image, tu peux la voir ici : https://arcabox.onrender.com/image");
          isTiming = false;  // Réinitialise le chronométrage
          showImage();
          currState = WAITING_FOR_IMAGE;
          ledcWrite(0, pulseWidth(MID_POS));
        } else {
          showImageFromInternet();
          tft.setRotation(3);
        }
      }
      break;

    case DISPLAYING_GIF:
      showGif(false);
      if (analogRead(LIGHT_SENSE_PIN) > 5) {
        currState = WAITING_FOR_IMAGE;
        tft.fillScreen(TFT_BLACK);
        ledcWrite(0, pulseWidth(MID_POS));
      }
      break;

    case DISPLAYING_PNG:
      if (analogRead(LIGHT_SENSE_PIN) > 5) {
        currState = WAITING_FOR_IMAGE;
        tft.fillScreen(TFT_BLACK);
        ledcWrite(0, pulseWidth(MID_POS));
      }
      break;









    case GET_USER_INPUT:  // Ajouter le nouvel état dans le switch

          if (check()) {
                  digitalWrite(BACKLIGHT_PIN, HIGH);
                  ShowText(); //Remplacer par le truc pour afficher le message
                  delay(60000);
                  tft.fillScreen(TFT_BLACK);
                  // Éteindre l'écran
                  digitalWrite(BACKLIGHT_PIN, LOW);  // Couper l'alimentation de l'écran
                  currState = DISPLAYING_PNG;
                  isTiming = false;  // Réinitialise le chronométrage
                } else {
                  if (!isTiming) {
                    startTime = millis();  // Commence le chronométrage
                    isTiming = true;
                  }
                  
                  if (millis() - startTime > 60 * 1000) {  // Vérifie si 1 minute s'est écoulée
                    bot.sendMessage(chat_id, "Tu as reçu un message, tu peux la voir ici : https://arcabox.onrender.com/messages");
                    isTiming = false;  // Réinitialise le chronométrage
                    ShowText(); //Remplacer par le truc pour afficher le message
                  } else {
                    showImageFromInternet();
                    tft.setRotation(3);
                  }
                }


                currState = WAITING_FOR_IMAGE;
                new_string();
                digitalWrite(BACKLIGHT_PIN, LOW);
                break;
  }
}

void pollForImage() {
  WiFiClientSecure *client = new WiFiClientSecure;
  if (client) {
    client->setInsecure();

    HTTPClient http;
    http.begin(*client, render_site);

    const char *headerKeys[] = {"imgname"};
    const size_t headerKeysCount = sizeof(headerKeys) / sizeof(headerKeys[0]);
    http.collectHeaders(headerKeys, headerKeysCount);
    int httpCode = http.GET();

    int numHeaders = http.headers();
    String fileName = http.header("imgname");
    fileName.toLowerCase();

    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        Serial.print("Got image: ");
        Serial.println(fileName);
        SPIFFS.remove("/image.gif");
        SPIFFS.remove("/image.png");
        if (fileName.endsWith(".gif")) {
          fsUploadFile = SPIFFS.open("/image.gif", "w");
        } else if (fileName.endsWith(".png")) {
          fsUploadFile = SPIFFS.open("/image.png", "w");
        } else {
          Serial.print("unknown file: ");
          Serial.println(fileName);
          http.end();
          return;
        }

        int len = http.getSize();
        uint8_t buff[2048] = {0};
        WiFiClient *stream = http.getStreamPtr();

        while (http.connected() && (len > 0 || len == -1)) {
          size_t size = stream->available();

          if (size) {
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
            fsUploadFile.write(buff, c);
            if (len > 0) {
              len -= c;
            }
          }
          delay(1);
        }

        Serial.println();
        Serial.print("[HTTP] connection closed or file end.\n");
        fsUploadFile.close();
        if (fileName.endsWith(".gif")) {
          currState = WAITING_TO_DISPLAY_GIF;
        } else if (fileName.endsWith(".png")) {
          currState = WAITING_TO_DISPLAY_PNG;
        }
      }
    } else {
    }
    http.end();
    delete client;
  } else {
    Serial.println("Unable to create client");
  }
}
void pollForMessage() {
   old_user_input = user_input;
  HTTPClient http;
  String serverAddress = "https://arcabox.onrender.com/poll";  // Adresse de mon serveur Flask

  if (http.begin(serverAddress)) {  // Démarre la connexion HTTP
    int httpCode = http.GET();     // Envoie la requête GET
    if (httpCode > 0) {            // Vérifie le code de retour
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();  // Récupère le contenu de la réponse

        // Parse JSON
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
          const char* new_user_input = doc["user_input"];
          http.end();
          
          if (String(new_user_input) != "") {
          
          currState = GET_USER_INPUT;

          }
        }
      }
    }
  }
}

void showGif(bool clearScreen) {
  if (clearScreen) {
    tft.fillScreen(TFT_BLACK);
  }
  Serial.println("Displaying GIF");
  tft.fillScreen(TFT_WHITE);
  if (gif.open("/image.gif", fileOpen, fileClose, fileRead, fileSeek, GIFDraw)) {
    tft.startWrite();
    int frameRes;
    do {
      frameRes = gif.playFrame(true, NULL);
    } while (frameRes);
    gif.close();
    tft.endWrite();
  } else {
    Serial.println("/image.gif did not work");
  }
}

void showImage(){
  // Scan LittleFS and load any *.png files
    tft.fillScreen(TFT_WHITE);
    tft.setRotation(2);
  Serial.println("Loading image");
  File file = SPIFFS.open("/image.png", "r");
  String strname = file.name();
  strname = "/" + strname;
  Serial.println(file.name());
  // If it is not a directory and filename ends in .png then load it
  if (!file.isDirectory() && strname.endsWith(".png")) {
    
    // Pass support callback function names to library
    int16_t rc = png.open(strname.c_str(), pngOpen, pngClose, pngRead, pngSeek, pngDraw);
    if (rc == PNG_SUCCESS) {
      tft.startWrite();
      Serial.printf("image specs: (%d x %d), %d bpp, pixel type: %d\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType());
      //480 x 320
      xpos = 320/2 - png.getWidth()/2;
      ypos = 480/2 - png.getHeight()/2;
      uint32_t dt = millis();
      if (png.getWidth() > MAX_IMAGE_WIDTH) {
        Serial.println("Image too wide for allocated line buffer size!");
      }
      else {
        rc = png.decode(NULL, 0);
        png.close();
      }
      tft.endWrite();
      // How long did rendering take...
      Serial.print(millis()-dt); Serial.println("ms");
    }else{
      Serial.println(rc);
      Serial.println("Failed to load image");
    }
  }
}


#include <qrcode.h>  // Make sure this is included at the top

void displayQR() {
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(2);
    tft.print("Bonjour Xena\n\nJ'aime tes gros sboobs");
    delay(30000);
}




void clearWiFiCredentials() {
  preferences.clear();
  Serial.println("WiFi credentials cleared.");
}

void handleSerialInput() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.equalsIgnoreCase("CLEAR_WIFI")) {
      clearWiFiCredentials();
      Serial.println("Command executed: WiFi credentials cleared.");
      ESP.restart();

    } else {
      Serial.println("Unknown command.");
    }
  }
}

void servoWiggle() {
 Serial.print("Wesh faut bosser maintenant \n");
}

int pulseWidth(int angle) {
  int pulse_wide = map(angle, 0, 180, 1638, 8191);
  return pulse_wide;
}

void pngDraw(PNGDRAW *pDraw) {
  uint16_t lineBuffer[MAX_IMAGE_WIDTH];
  png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
  tft.pushImage(xpos, ypos + pDraw->y, pDraw->iWidth, 1, lineBuffer);
}

void *fileOpen(const char *filename, int32_t *pFileSize) {
  gifFile = SPIFFS.open(filename, FILE_READ);
  *pFileSize = gifFile.size();
  if (!gifFile) {
    Serial.println("Failed to open GIF file from SPIFFS!");
  }
  return &gifFile;
}

void fileClose(void *pHandle) {
  gifFile.close();
}

int32_t fileRead(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  int32_t iBytesRead;
  iBytesRead = iLen;
  if ((pFile->iSize - pFile->iPos) < iLen)
    iBytesRead = pFile->iSize - pFile->iPos;
  if (iBytesRead <= 0)
    return 0;

  gifFile.seek(pFile->iPos);
  int32_t bytesRead = gifFile.read(pBuf, iLen);
  pFile->iPos += iBytesRead;

  return bytesRead;
}

int32_t fileSeek(GIFFILE *pFile, int32_t iPosition) {
  if (iPosition < 0)
    iPosition = 0;
  else if (iPosition >= pFile->iSize)
    iPosition = pFile->iSize - 1;
  pFile->iPos = iPosition;
  gifFile.seek(pFile->iPos);
  return iPosition;
}
