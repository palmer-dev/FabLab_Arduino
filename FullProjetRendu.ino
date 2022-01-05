// ====================== Import des librairies ======================
#include <MFRC522.h> // Librairie pour le lecteurs de carte
#include <SPI.h> // Librairie pour le lecteurs de carte
#include <Adafruit_NeoPixel.h> // Librairie pour ruban RGBW
#include <SoftwareSerial.h> // Librairie pour la gestion du ESP8266 WIFI
#include <PubSubClient.h> //Librairie pour la gestion Mqtt 
#include "WiFiEsp.h" // Librairie wifi


// ====================== Déclarations des pins utiles ======================
#define pinRST 9 //
#define pinSS 10
#define RingLED 3
#define PinIR 8
#define SoundSensorPin A0
#define PinRX_WIFI 5
#define PinTX_WIFI 6


// ====================== Déclaration des variables utiles ======================
SoftwareSerial ESPserial(PinRX_WIFI, PinTX_WIFI); // (RX | TX) Variable pour la communication avec le WIFI et MQTT en SerialPort

// ===== Lecteur de cartes =====
MFRC522 mfrc522(pinSS, pinRST); // Paramètre de la librairie du lecteur de badge  
String readCard; // Chaine de caractère lue lors du passage d'un badge
boolean automatique = false; // Variable du mode selectionné
boolean unlock_write = true; //variable qui permet la non répétition des animations lors du passage des boucles de vérifications badge
String BadgeAuthorized[] = {"ID_BADGE"}; // Liste des badges authorisé

// ===== Capteur infrarouge =====
boolean MouvementStat; // Stockage du status du mouvement
int calibrationTime = 14; // Durée de calibration
uint32_t color_mvmt; // Couleur pour lors d'un mouvement
int val_ir; // Status du capteur IR de mouvement

// ===== Ruban RGBW =====
int NUMPIXELS = 30; // Nombre de pixel sur le ruban led
int delayval = 50; // Interval pour les animations
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, RingLED, NEO_GRB + NEO_KHZ800);

// ===== Capteur Son =====
int SoundStatValue; // Etat de la lecture de son
int ValeursCaliSoundSensor[15]; // ([x] ; x = calibrationTime + 1) Liste des valeurs prises lors de la calibration du capteur son 
int SumCaliSensor; // Somme lors des valeurs prises pour la calibration
int valueCaliSensor; // Moyenne des valeures prise lors de la calibration du capteur sonor

// ===== WIFI =====
int status = WL_IDLE_STATUS;
const char* ssid = "NOM-WIFI"; // Nom du réseau wifi auquel se connecter
const char* password = "MDP-WIFI"; // Mot de passe pour se connecter au WIFI
WiFiEspClient espClient; // Activation librairie WIFI pour ESP

// ===== MQTT =====
const char* mqtt_server = "broker.hivemq.com";// Adresse IP du Broker Mqtt
const int mqttPort = 1883; // Port utilisé par le Broker 
const char* user_mqtt = "nom-utilisateur_mqtt"; // Nom utilisateur pour acces au broker mqtt
const char* pass_mqtt = "mdp_utilisateur_mqtt"; // Mot de passe pour acces au broker mqtt
PubSubClient client(espClient); // Paramétrage librairie MQTT
boolean mqtt_available = false; // Variable pour savoir si on a un acces à un MQTT
boolean mqtt_sended = false; // Variable pour éviter l'envoie a répétition des mêmes données au broker MQTT


// ====================== Initialisations de tous les composants ======================
void setup()
{
  //Initialisation de la communication série avec l'ordinateur
  Serial.begin(9600);

  //Initialisation du bandeau led
  pixels.begin();
  pixels.clear();

  //Initialisation du lecteur de badge
  SPI.begin();        
  mfrc522.PCD_Init(); 

  //Initialisation et calibration du capteur IR
  pinMode(PinIR, INPUT);
  for (int i = 0; i < calibrationTime + 1; i++)
  {
    init_calibre(i);
    ValeursCaliSoundSensor[i] = init_soundSensor(true, i);
  }
  SumCaliSensor = init_soundSensor(false, ValeursCaliSoundSensor);
  valueCaliSensor = SumCaliSensor/(calibrationTime + 1);

  // initialisation du wifi
  if (setupWifi() == true){
    // initialisation du MQTT
    mqtt_available = setup_mqtt();
    if (mqtt_available == true)
    {
      mqtt_publish("WebTech/FabLab/MQTT/FMACLM", "Connected WIFI & MQTT!");
      // Animation lorsque connecté au WIFI et MQTT
      lock();
    } 
    else {
      // Si pas de serveur MQTT on éteint le bandeau et passage en mode manuel
      black_led(0, NUMPIXELS);
    }
  }
  else{
    // Si pas de wifi on éteint le bandeau et passage en mode manuel
    black_led(0, NUMPIXELS);
  }
}


// ====================== Fonction principale ======================
void loop()
{
  getID(); //recherche d'un passage de badge pour changement de mode, automatique ou manuel
  if (automatique == true){
    function_Auto(); // Si le mode automatique a été activé on passe par la fonction automatique
    mqtt_publish("WebTech/FabLab/MQTT/FMACLM", "Mode Manuel"); // Envoie au broker pour dire qu'on est en automatique
  }
  else{
    function_Manuelle(); // Si le mode manuel a été activé on passe par la fonction automatique
    mqtt_publish("WebTech/FabLab/MQTT/FMACLM", "Mode Automatique"); // Envoie au broker pour dire qu'on est en manuel
  }
}


// ====================== Fonctions utilitaires ======================

// ===== Verification pour savoir si le badge est dans la liste des badges authorisés =====
boolean In_Array(String authorisez[], String readed){
  for (int i = 0; i < calibrationTime + 1; i++)
  {
    if (authorisez[i] == readed)
    {
      return true;
    }
    
  }
  return false;
}


// ====================== Initialisation des capteurs ======================

// ===== Initialisation du capteur son =====
int init_soundSensor(boolean new_value, int list_values[]){
  if (new_value == true) // Ajout d'une nouvelle valeur à la liste des valeur de prise
  {
    return analogRead(SoundSensorPin);
  }
  else{
    int somme = 0; 
    for (int i = 0; i < calibrationTime + 1; i++)
    {
      somme += list_values[i];
    }
    return somme; // Retour de la somme des valeurs de la liste
  }
}

// ===== Initialisation du WIFI =====
boolean setupWifi(){
    ESPserial.begin(9600); // Initialisation de la communication série avec le ESP8266 WIFI
    WiFi.init(&ESPserial); // Initialisation de la communication série avec le ESP8266 WIFI
    int essais = 0; 
    while (essais < 3) // On tente 3 fois de se connecter au WIFI avant d'abandonner
    {
        status = WiFi.begin(ssid, password); // Connection au wifi
        if (status == WL_CONNECTED){ essais = 3;} // Si connecté alors on permet la suite du programme
        else{essais += 1;}
    }
    if (status == WL_CONNECTED){ WiFiEspServer server(80); return true;}
    else{return false;}  
}

// ===== Initialisation du serveur MQTT =====
boolean setup_mqtt(){
  client.setServer(mqtt_server, mqttPort);
  if (client.connect("Arduino_FabLab_WebTech")){ // On donne ce nom émetteur à l'arduino et on tente de se connecter
    client.disconnect();
    return true;
  }else{
    return false; // SI on arrive pas à jointre le serveur mqtt on désactive la fonction pour ne pas ralentir le programme
  }
}


// ====================== function de mode ======================
void function_Auto(){  
  if (capteurIR() != true){ // Si le capteur de mouvement ne capte rien on vérifie le capteur son 
    if (SoundSensor()){ // Si le capteur de son entend quelqu'un on envoie au serveur MQTT l'ordre de présence
      mqtt_publish("WebTech/FabLab/MQTT/FMACLM", "Presence Detected");
    }
    else{ // Si non capteur de son entend quelqu'un on envoie au serveur MQTT l'ordre de présence (si il n'a pas déjà été envoyé)
      mqtt_publish("WebTech/FabLab/MQTT/FMACLM", "No more presence detected"); 
    }
  }
  else{
    mqtt_publish("WebTech/FabLab/MQTT/FMACLM", "Presence Detected");
  }
}

void function_Manuelle(){
  // CODE POUR LE MODE MANUEL !
}


// ====================== Partie pour les fonction de capteurs ======================

// ===== Lecture du badge si il y en a un de passé =====
int getID() 
{
  readCard = ""; // Vidage de la variable du nom du badge lu
  if ( (! mfrc522.PICC_IsNewCardPresent()) || (! mfrc522.PICC_ReadCardSerial()) ) //vérification si carte
  {
     return 0;
  }

  // Enregistrement du nom du badge lu 
  for (int i = 0; i < mfrc522.uid.size; i++) {  
    readCard += mfrc522.uid.uidByte[i];    
  }

  // Verification si c'est le badge authorisé 
  if (In_Array(BadgeAuthorized, readCard)){
    if (unlock_write == false){
      unlock();    
    }else{
      lock();
    }
  } 

  mfrc522.PICC_HaltA();   
}

// ===== Fonction pour la detection de mouvement =====
boolean capteurIR(){
  val_ir = digitalRead(PinIR);  // lecture de la valeur du capteur IR
  delay(150);
  if (val_ir == HIGH) { // Si la valeur est 1 ou haute
    if (MouvementStat == false) { // Si le mouvement n'a pas déjà été vu
      MouvementStat = true;
      mouvement(MouvementStat);
      return true;
    }
  } else {
    if (MouvementStat == true){ 
      MouvementStat = false;
      mouvement(MouvementStat);
      return false;
    }
  }
}

// ===== Fonction pour la detection de son =====
boolean SoundSensor(){
    SoundStatValue = analogRead(SoundSensorPin);
    if (SoundStatValue > valueCaliSensor + 2){
      mouvement(true);
      return(true);
    }
    else {
      mouvement(false);
      return(false);
    }
}

// ===== Fonction pour publier un float sur un topic =====
boolean mqtt_publish(String topic, String toPub){
  if (mqtt_available != true)
  {
    return true;
  }
  if ( ((mqtt_sended == false) && (toPub == "No more presence detected")) || ( (mqtt_sended == true) && (toPub == "Presence Detected") ) ){ 
    char topCHR[topic.length()+1];
    char toPubCHR[toPub.length()+1];
    topic.toCharArray(topCHR,topic.length()+1);
    toPub.toCharArray(toPubCHR,toPub.length()+1);
    if (mqtt_sended == true){mqtt_sended = false;}
    else { mqtt_sended = true;}
    if (client.connect("Arduino_FabLab_WebTech")){
      client.publish(topCHR,toPubCHR);
      client.disconnect();
      return true;
    }
    else{
      return false;
    }
  }
  else{
    if ((toPub != "No more presence detected") || (toPub != "Presence Detected")){
      if (client.connect("Arduino_FabLab_WebTech")){
        client.publish(topCHR,toPubCHR);
        client.disconnect();
        return true;
      }
      else{
        return false;
      }
    }
    return true;
  }
}


// ====================== Partie pour les animations du ruban LED ======================

// ===== Animation au chargement lors des calibrations des capteurs =====
void init_calibre(int i){
  for (int j = 0; j < 2; j++)
  {
    for (int k = 0; k < ceil(i*NUMPIXELS / calibrationTime); k++)
    {
      pixels.setPixelColor(k, pixels.Color(0,0,255)); 
    }
    pixels.show();
    delay(500);
    black_led(ceil((i-1)*NUMPIXELS / calibrationTime), ceil(i*NUMPIXELS / calibrationTime));
    delay(500);
  }
}

// ===== Animation pour passage en mode manuel lors du passage du BON badge =====
void lock(){
  if (unlock_write == true){
    for(int i=0;i<NUMPIXELS;i++)
    {
      pixels.setPixelColor(i, pixels.Color(255,0,0)); 
      pixels.show();
      delay(delayval); 
    }
   
    for(int i=0;i<NUMPIXELS;i++)
    {
      pixels.setPixelColor(i+2, pixels.Color(255, 255, 0)); 
      pixels.show();
      delay(delayval);
      pixels.setPixelColor(i+1, pixels.Color(255,128,0));
      pixels.show(); 
      delay(delayval); 
      pixels.setPixelColor(i, pixels.Color(0, 0, 0));
      pixels.show();
    }
    unlock_write = false; 
    automatique = false; 
  }
}

// ===== Animation du ruban lors du passage en mode automatique après le passage du BON badge =====
// ===== Extinction des leds du bandeau =====
void black_led(int Start_Led, int Last_Led){
  for(int i=Start_Led;i<Last_Led;i++) // Bandeau mis en noir
  {
    pixels.setPixelColor(i, pixels.Color(0, 0, 0));
    pixels.show();
    delay(delayval); 
  }
}

// ===== Animation lors du mode Manuel =====
void unlock(){
  if (unlock_write == false){
    rainbowCycle(10);
    black_led(0,NUMPIXELS);
    unlock_write = true;
    automatique = true; 
  }
}

// ===== Animation lors de la détection de mouvement =====
void mouvement(boolean mvmt){
  if (mvmt == true){color_mvmt = pixels.Color(0, 255, 0);} // On set la couleur à afficher sur le ruban led au vert si il y a du mouvement
  else{color_mvmt = pixels.Color(0, 0, 0);} // On éteint le ruban si plus de mouvement
  if (color_mvmt == pixels.Color(0, 255, 0))
  {
    for(int i=0;i<NUMPIXELS;i++){pixels.setPixelColor(i, color_mvmt);}
    pixels.show();
  }
  else {black_led(0, NUMPIXELS);}
}

// ===== Animation ruban arc-en-ciel lors du mode automatiqe =====
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*2; j++) { 
    for(i=0; i< pixels.numPixels(); i++) {pixels.setPixelColor(i, Wheel(((i * 256 / pixels.numPixels()) + j) & 255));}
    pixels.show();
    delay(wait);
  }
}

// ===== Fonction complémentaire pour la fonction rainbowCycle =====
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
    return pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}