#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <DHT.h>
#include <FirebaseESP8266.h>
#include "EEPROM.h"  

#define RED_LED       D4
#define GREEN_LED     D3
#define YELLOW_LED    D7

#define RECONFIG_BUTTON   D6

#define DHTPIN        D1
#define DHTTYPE       DHT11

#define FIREBASE_HOST "https://lab-esp8266-station-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "IQWPuHJyPLOO37s8cssw3kIwitGMcEbtX5u4J1zV"

#define EEPROM_SIZE   100

int WIFI_CONFIG_STATUS = 0;
int NODE_CONFIG_STATUS = 0;

int addressCharArray;

FirebaseData firebaseData;

String symbol = "/";

String sub_path1 = "/Alarmes";
String sub_path2 = "/Sensores";

String sensor1 = "/Temperatura";
String sensor2 = "/Umidade";
String sensor3 = "/Bateria";
String sensor4 = "/Alimentacao";
String sensor5 = "/FumacaGas";

DHT dht(DHTPIN, DHTTYPE);

int umidade = 0;
int temperatura = 0;

int eeAddress_wifi_config = 0;
int eeAddress_node_config = 3;
int eeAddress_char = 20;

unsigned long _time_delay = 10000;
unsigned long tempo = 0;

struct MyObject{
  String ssid;
  String password;
  String path;
};

MyObject customVar;

enum mem_type{
  SSID_TYPE,
  PASSWORD_TYPE,
  NODE_TYPE,
  CONFIG_TYPE,
  UNKNOW_TYPE
};

String readCMD(enum mem_type type){
  String cmd;
  char caractere[20];
  int length = 0;

  tempo = millis();

  while(true){
    while(Serial.available() > 0) {
      length = Serial.readBytes(caractere, sizeof(caractere));  
    }
    if(length > 0){
      break;
    }
    if(type == CONFIG_TYPE){
      if (millis() - tempo >= _time_delay){
        Serial.println("Reconectando...");
        break;
      }
    }
  }

  for (int i = 0; i < length; i++){
    cmd.concat(caractere[i]);
    if(type == NODE_TYPE){
      EEPROM.write(eeAddress_char + i, caractere[i]);     
    }
  }

  Serial.print("CMD: ");
  Serial.println(cmd);

  return cmd;
}

void conectarWifi() {
  delay(10);
  Serial.println("Conectando ");

  WiFi.begin(customVar.ssid, customVar.password);

  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(RED_LED, HIGH);
    delay(500);
    Serial.print(".");
  }
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);
  Serial.println(" ");
}

void medirTemperaturaUmidade() {
  digitalWrite(YELLOW_LED, HIGH);
  umidade = dht.readHumidity();
  temperatura = dht.readTemperature(false);
  delay(1000);
  digitalWrite(YELLOW_LED, LOW);
}

void wifiConfig() {
  WIFI_CONFIG_STATUS = EEPROM.read(eeAddress_wifi_config);
  if(WIFI_CONFIG_STATUS == 0){
    Serial.println("Entre com o NOME da Rede Wifi: ");
    customVar.ssid = readCMD(UNKNOW_TYPE);
    Serial.println("Entre com a SENHA da Rede Wifi: ");
    customVar.password = readCMD(UNKNOW_TYPE);
    Serial.println("-------------------------------");
    EEPROM.write(eeAddress_wifi_config, 1);
    EEPROM.commit();
  }
}

void nodeConfig() {
  customVar.path = "";
  for (int i = eeAddress_char; i < eeAddress_char + 5; i++){
    customVar.path.concat((char)EEPROM.read(i));
  }

  Serial.print("Node: ");
  Serial.println(customVar.path);
  NODE_CONFIG_STATUS = EEPROM.read(eeAddress_node_config);
  if(NODE_CONFIG_STATUS == 0){
    Serial.println("Entre com o NOME do Node (EX: Node1): ");
    customVar.path = readCMD(NODE_TYPE);
    Serial.println("-------------------------------");
    EEPROM.write(eeAddress_node_config, 1);
    EEPROM.commit();
  }
}

void send_to_Firebase(String sub_path, String sensor, int data){
  if(Firebase.setInt(firebaseData, symbol + customVar.path + sub_path + sensor, data))
  {
    //Success
     Serial.println("Set int data success");
  }else{
    //Failed?, get the error reason from firebaseData
    Serial.print("Error in setInt, ");
    Serial.println(firebaseData.errorReason());
  }
}

void wifi_node_setup(){
  wifiConfig();
  conectarWifi();
  nodeConfig();  
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
}

void reConfig(){
  String resp;
  Serial.println("Deseja configurar/reconfigurar o dispositivo (S/N)?");
  resp = readCMD(CONFIG_TYPE);

  switch (resp[0]){
  case 'S':
    EEPROM.write(eeAddress_wifi_config, 0);
    EEPROM.write(eeAddress_node_config, 0);
    EEPROM.commit();
    Serial.println("Reconfigurando...");
    wifi_node_setup();
    Serial.println("Configurado!");
    break;
  case 'N':
    Serial.println("-------------------------------");
  default:
    break;
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  EEPROM.begin(EEPROM_SIZE);
  dht.begin();
  pinMode(RECONFIG_BUTTON, INPUT_PULLUP);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);

  reConfig();

  wifi_node_setup();

  tempo = millis(); // passa à variável o valor de millis()
}

void loop() {

  if (millis() - tempo >= _time_delay){ // após 10 segundos  //
    Serial.println("Atualizando...");
    medirTemperaturaUmidade();
    Serial.println("Temperatura: ");
    Serial.print(temperatura);
    Serial.print(" *C");
    Serial.println(" ");

    Serial.println("Umidade ");
    Serial.print(umidade);
    Serial.print(" %");
    Serial.println(" ");

    send_to_Firebase(sub_path2, sensor1, temperatura);
    send_to_Firebase(sub_path2, sensor2, umidade);

    send_to_Firebase(sub_path1, sensor3, 100);
    send_to_Firebase(sub_path1, sensor4, 1);
    send_to_Firebase(sub_path1, sensor5, 0);

    tempo = millis();
  }

  if((digitalRead(RECONFIG_BUTTON) == 0)){
    reConfig();
  }
  delay(100);
}