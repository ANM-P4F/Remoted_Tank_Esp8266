#include <SPI.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "FS.h"

#define LED_OUT    16
const uint8_t reset_pin = 5;

const char* ssid = "xxxxxxxxxxxxxxxxxx";
const char* password = "yyyyyyyyyyyyyyyyyyyy";
const char* ssid_ap = "TankMini";
const char* password_ap = "esp8266Testing";

#define PWM1   5   // PWM1 output
#define DIR1   0   // Dir 1
#define PWM2   4   // PWM2 output
#define DIR2   2   // DIr 2
#define CANON_0 15 
#define CANON_1 13 
#define FIRE   12  // Fire output

// variables declaration
boolean motor_dir = 0;
int8_t  quad = 0;
uint8_t previous_data;
int16_t motor_speed = 0;

int pre_dmotorL    = 0;
int pre_dmotorR    = 0;
int pre_dmotorTurn = 0;
int pre_dFire      = 0;

int dCanon         = 0; //INI_ANGLE/ANGLE_STEP;
int pre_dCanon     = dCanon;

const int MIN_MOTOR_PWM = 300;
const int MOTOR_TURN_SPEED = 1000;
const int MOTOR_STRAIGHT_SPEED = 800;
const int MOTOR_DELTA_SPEED = 200;

AsyncWebServer server(80);

String html_home;

const int NUM_CMD = 15;
String cmd[NUM_CMD] = {"/fwON", "/fwOFF", "/bwON", "/bwOFF", "/leftON", "/leftOFF", "/rightON", "/rightOFF", 
                        "/left_canonON", "/left_canonOFF", "/right_canonON", "/right_canonOFF", "/fireON", "/fireOFF"};


int ledState = LOW;

unsigned long previousMillis = 0;
unsigned long previousMillis2 = 0;
const long interval = 1000;
const long interval2 = 10;
float speed = 0;

int out_speedL = 0, out_speedL_pre = 0;
int out_speedR = 0, out_speedR_pre = 0;
int out_canon  = 0, out_canon_pre  = 0;
int out_fire   = 0, out_fire_pre   = 0;

IPAddress staticIP(192, 168, 1, 169); //ESP static ip
IPAddress gateway(192, 168, 1, 1);   //IP Address of your WiFi Router (Gateway)
IPAddress subnet(255, 255, 255, 0);  //Subnet mask
IPAddress dns(8, 8, 8, 8);  //DNS

// https://stackoverflow.com/questions/9072320/split-string-into-string-array
String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

/* Read html file from FFS */
void prepareFile() {

  Serial.println("Prepare file system");
  SPIFFS.begin();

  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    Serial.println("file open failed");
  } else {
    Serial.println("file open success");

    // html_home = "";
    // while (file.available()) {
    //   //Serial.write(file.read());
    //   String line = file.readStringUntil('\n');
    //   html_home += line + "\n";
    //   if(html_home.length() > 100){        
    //      Serial.print(html_home);
    //      html_home = "";
    //   }
    // }
    // Serial.print(html_home);
    // file.close();
    // if(html_home != ""){
    //    Serial.print(html_home);
    //    html_home = "";
    // }
  }
}

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.println();
  Serial.println();

  pinMode(LED_OUT, OUTPUT);
  digitalWrite(LED_OUT, 1);

  //motor setting
  pinMode(PWM1, OUTPUT);
  pinMode(DIR1, OUTPUT);
  analogWrite(PWM1, LOW);
  digitalWrite(DIR1, LOW);
  pinMode(PWM2, OUTPUT);
  pinMode(DIR2, OUTPUT);
  analogWrite(PWM2, LOW);
  digitalWrite(DIR2, LOW);

  //motor canon
  pinMode(CANON_0, OUTPUT);
  pinMode(CANON_1, OUTPUT);
  digitalWrite(CANON_0, LOW);
  digitalWrite(CANON_1, LOW);

  pinMode(FIRE, OUTPUT);
  digitalWrite(FIRE, LOW);

  setMotorSpeed(0,0);
  setMotorSpeed(1,0);

#if 1
  // WiFi.config(staticIP, subnet, gateway, dns);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
#else
  WiFi.softAP(ssid_ap, password_ap);
  Serial.println("");

  Serial.println();
  Serial.print("Server IP address: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("Server MAC address: ");
  Serial.println(WiFi.softAPmacAddress());
#endif

  // prepareFile();

  // Initialize SPIFFS
  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // handle index
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false);
  });


  for(int i = 0; i < NUM_CMD; i++){
    // forward
    server.on(cmd[i].c_str(), HTTP_GET, [i](AsyncWebServerRequest *request){
      processing(cmd[i]);
      // Serial.println("fwON");
      request->send(SPIFFS, "/index.html", String(), false);
    });
  }

  server.begin();

  Serial.printf("Server Start\n");

  digitalWrite(LED_OUT, 0);

}

// int out_speedL = 0, out_speedL_pre = 0;
// int out_speedR = 0, out_speedR_pre = 0;
// int out_canon  = 0, out_canon_pre  = 0;
// int out_fire   = 0, out_fire       = 0;

void processing(const String& var){
  if(var == "/fwON"){
    out_speedL = MOTOR_STRAIGHT_SPEED;
    out_speedR = MOTOR_STRAIGHT_SPEED;
  }else if(var == "/fwOFF"){
    out_speedL = 0;
    out_speedR = 0;
  }else if(var == "/bwON"){
    out_speedL = (-1)*MOTOR_STRAIGHT_SPEED;
    out_speedR = (-1)*MOTOR_STRAIGHT_SPEED;
  }else if(var == "/bwOFF"){
    out_speedL = 0;
    out_speedR = 0;
  }else if(var == "/leftON"){
    if(out_speedL == 0){
      out_speedL = MOTOR_TURN_SPEED;
    }else{
      out_speedL += MOTOR_DELTA_SPEED;
    }
  }else if(var == "/leftOFF"){
    if(out_speedL == MOTOR_TURN_SPEED){
      out_speedL = 0;
    }else{
      out_speedL -= MOTOR_DELTA_SPEED;
    }
  }else if(var == "/rightON"){
    if(out_speedR == 0){
      out_speedR = MOTOR_TURN_SPEED;
    }else{
      out_speedR += MOTOR_DELTA_SPEED;
    }
  }else if(var == "/rightOFF"){
    if(out_speedR == MOTOR_TURN_SPEED){
      out_speedR = 0;
    }else{
      out_speedR -= MOTOR_DELTA_SPEED;
    }
  }else if(var == "/left_canonON"){
    out_canon = 1;
  }else if(var == "/left_canonOFF"){
    out_canon = 0;
  }else if(var == "/right_canonON"){
    out_canon = -1;
  }else if(var == "/right_canonOFF"){
    out_canon = 0;
  }else if(var == "/fireON"){
    out_fire = 1;
  }else if(var == "/fireOFF"){
    out_fire = 0;
  }
  Serial.print("out_speedL: ");
  Serial.println(out_speedL);
  Serial.print("out_speedR: ");
  Serial.println(out_speedR);
  Serial.print("out_canon");
  Serial.println(out_canon);
  Serial.print("out_fire");
  Serial.println(out_fire);
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    if (ledState == LOW)
      ledState = HIGH;  // Note that this switches the LED *off*
    else
      ledState = LOW;   // Note that this switches the LED *on*
    digitalWrite(LED_OUT, ledState);
  }
  if(currentMillis - previousMillis2 >= interval2){
    previousMillis2 = currentMillis;
    controller();
  }
}

void controller(){
  if( (out_speedL != out_speedL_pre) || (out_speedR != out_speedR_pre)){
    setSpeed(out_speedR, out_speedL);
    out_speedL_pre = out_speedL;
    out_speedR_pre = out_speedR;
  }
  // if( out_canon != out_canon_pre ){
    if(out_canon == 1){
      rotateCanon(1);
    }else if(out_canon == -1){
      rotateCanon(0);
    }
  //   out_canon = out_canon_pre;
  // }
  if(out_fire != out_fire_pre){
    fire(out_fire);
    out_fire_pre = out_fire;
  }
}

void rotateCanon(int val){
  if(val == 1){
   analogWrite(CANON_0, 450);
   digitalWrite(CANON_1, 0);
  }else{
   analogWrite(CANON_1, 450);
   digitalWrite(CANON_0, 0);
  }
  delay(2);
  digitalWrite(CANON_0, 0);
  digitalWrite(CANON_1, 0);
}

void fire(int val){
  if(val == 1){
    analogWrite(FIRE, 720);
    // delay(2000);
    // analogWrite(FIRE, 0);
  }else{
    analogWrite(FIRE, 0);
  }
}

void setSpeed(int left, int right){
  setMotorSpeed(0,left);
  setMotorSpeed(1,right);
}

/* Set motor speed */
void setMotorSpeed(int id, int speed){
  if(speed < 0){
    speed *= -1;
    if(id == 0){
      analogWrite(PWM1, speed);
      digitalWrite(DIR1, LOW);
    }else if (id==1){
      analogWrite(PWM2, speed);
      digitalWrite(DIR2, LOW);
    }
  }else{
    if(id == 0){
      analogWrite(PWM1, speed);
      digitalWrite(DIR1, HIGH);
    }else if (id==1){
      analogWrite(PWM2, speed);
      digitalWrite(DIR2, HIGH);
    }
  }
}
