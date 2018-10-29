
/*  Pulse Sensor Amped 1.5    by Joel Murphy and Yury Gitman   http://www.pulsesensor.com

  ----------------------  Notes ----------------------  ----------------------
  This code:
  1) Blinks an LED to User's Live Heartbeat   PIN 13
  2) Fades an LED to User's Live HeartBeat    PIN 5
  3) Determines BPM
  4) Prints All of the Above to Serial

  Read Me:
  https://github.com/WorldFamousElectronics/PulseSensor_Amped_Arduino/blob/master/README.md
  ----------------------       ----------------------  ----------------------
*/

#include <Ticker.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <FirebaseArduino.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ESP8266HTTPClient.h>

#define PROCESSING_VISUALIZER 1
#define SERIAL_PLOTTER  2

//Temperature Sensor
#define MLX90614_TA 0x06
#define MLX90614_TOBJ1 0x07

// Firebase Data
#define FIREBASE_HOST "test-db-35cc0.firebaseio.com"
#define FIREBASE_AUTH "impYBWqUbLuX7i7CxtfN3Pi9j3zNn8954VqZZcVO"

Ticker flipper;

//  Variables
int pulsePin = 0;                 // Pulse Sensor purple wire connected to analog pin 0
int blinkPin = 13;                // pin to blink led at each beat

int fadeRate = 0;                 // used to fade LED on with PWM on fadePin

int counter = 0;
int ID = 0;

// Datos envio notificacion
String top = "387700";
String serve = "AIzaSyBbcZ9M-2nuCIsPq3h4ahse3fftMCgSizM";

// Volatile Variables, used in the interrupt service routine!
volatile int BPM;                   // int that holds raw Analog in 0. updated every 2mS
volatile int Signal;                // holds the incoming raw data
volatile int IBI = 600;             // int that holds the time interval between beats! Must be seeded!
volatile boolean Pulse = false;     // "True" when User's live heartbeat is detected. "False" when not a "live beat".
volatile boolean QS = false;        // becomes true when Arduoino finds a beat.

// SET THE SERIAL OUTPUT TYPE TO YOUR NEEDS
// PROCESSING_VISUALIZER works with Pulse Sensor Processing Visualizer
//      https://github.com/WorldFamousElectronics/PulseSensor_Amped_Processing_Visualizer
// SERIAL_PLOTTER outputs sensor data for viewing with the Arduino Serial Plotter
//      run the Serial Plotter at 115200 baud: Tools/Serial Plotter or Command+L
static int outputType = SERIAL_PLOTTER;

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}


void setup() {

  Serial.printf("The ESP8266 chip ID as a 32-bit integer:\t%08X\n", ESP.getChipId());
  ID = ESP.getChipId();
  String ID_str = String(ID);
  pinMode(blinkPin, OUTPUT);        // pin that will blink to your heartbeat!

  Serial.begin(115200);             // we agree to talk fast!

  Wire.begin(4, 5);
  Wire.setClock(100000);

  //wifi connection
  WiFiManager wifiManager;

    //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);
  

  if(!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  } 

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  
  // Begin connection to Firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  // Create the Firebase Realtime Database keys (if not yet) and set default values
  Firebase.set(ID_str + "/amb_temperature", 0);
  Firebase.set(ID_str + "/heartBeat", 0);
  Firebase.set(ID_str + "/id", "387700");
 // Firebase.set(ID_str + "/id_android", "");
  Firebase.set(ID_str + "/obj_temperature", 0);
  Firebase.set(ID_str + "/qs", false);
  //Firebase.set(ID_str + "/tipo_animal", 1);
  
  
  

  // Show the last error
  if (Firebase.failed()) {
    Serial.print("setting /message failed:");
    Serial.println(Firebase.error());
  }

  interruptSetup();                 // sets up to read Pulse Sensor signal every 2mS
  // IF YOU ARE POWERING The Pulse Sensor AT VOLTAGE LESS THAN THE BOARD VOLTAGE,
  // UN-COMMENT THE NEXT LINE AND APPLY THAT VOLTAGE TO THE A-REF PIN
  //   analogReference(EXTERNAL);
}

//Temperature sensor
uint16_t read16(uint8_t addr, uint8_t i2c_addr) {
  uint16_t ret;

  Wire.beginTransmission(i2c_addr); // start transmission to device
  Wire.write(addr); // sends register address to read from
  Wire.endTransmission(false); // end transmission

  Wire.requestFrom(i2c_addr, (uint8_t)3);// send data n-bytes read
  ret = Wire.read(); // receive DATA
  ret |= Wire.read() << 8; // receive DATA

  uint8_t pec = Wire.read();

  return ret;
}


float readTemp(uint8_t reg, uint8_t i2c_addr) {
  float temp;

  temp = read16(reg, i2c_addr);
  temp *= .02;
  temp -= 273.15;
  return temp;

}

double readObjectTempC(uint8_t i2c_addr) {
  return readTemp(MLX90614_TOBJ1, i2c_addr);
}


double readAmbientTempC(uint8_t i2c_addr) {
  return readTemp(MLX90614_TA, i2c_addr);
}

HTTPClient httpTOPIK;
void doitTOPIC(String paytitle, String pay, String top) {
  //more info @ https://firebase.google.com/docs/cloud-messaging/http-server-ref


  String data = "{";
  data = data + "\"to\": \"/topics/" + top + "\",";
  data = data + "\"notification\": {";
  data = data + "\"body\": \"" + pay + "\",";
  data = data + "\"title\" : \"" + paytitle + "\", ";
  data = data + "\"sound\" : \"default " "\" ";
  data = data + "} }";

  Serial.println (data);

  httpTOPIK.begin("http://fcm.googleapis.com/fcm/send");
  httpTOPIK.addHeader("Authorization", "key=" + serve);
  httpTOPIK.addHeader("Content-Type", "application/json");
  httpTOPIK.addHeader("Host", "fcm.googleapis.com");
  httpTOPIK.addHeader("Content-Length", String(pay.length()));
  httpTOPIK.POST(data);
  httpTOPIK.writeToStream(&Serial);
  httpTOPIK.end();
  Serial.println();

}


//  Where the Magic Happens
void loop() {
  serialOutput() ;

  float bpm = BPM;
  bool qs = QS;
  float obj_temp = readObjectTempC(0x5A);
  float amb_temp = readAmbientTempC(0x5A);
  ID = ESP.getChipId();

  String ID_str = String(ID);

  int id_animal = Firebase.getInt(ID_str + "/tipo_animal");

 
  if (QS == false) {
    counter = counter + 1;
    Serial.println ("contador ");
    Serial.println(counter);
  }

  
  if (counter == 120)
  {
    Firebase.setBool(ID_str + "/qs", false);
    Serial.println((ID_str  + "/qs", qs));
    Serial.println("Envio de notificacion");
    doitTOPIC("Alerta", "Verificar Equipo - Desconectado!!", top); //clear way with topic

    counter = 0;
    Serial.println("Delay - Equipo desconectado");
    delay (180000);
  }

  

  if (QS == true) {    // A Heartbeat Was Found

    Firebase.setFloat(ID_str  + "/heartBeat", bpm);
    Firebase.setBool(ID_str   + "/qs", QS);
    Firebase.setFloat(ID_str  + "/obj_temperature", readObjectTempC(0x5A));
    Firebase.setFloat(ID_str  + "/amb_temperature", readAmbientTempC(0x5A));

    // BPM and IBI have been Determined
    // Quantified Self "QS" true when arduino finds a heartbeat
    fadeRate = 255;         // Makes the LED Fade Effect Happen
    // Set 'fadeRate' Variable to 255 to fade LED with pulse
    serialOutputWhenBeatHappens();   // A Beat Happened, Output that to serial.

    counter = 0;

    Serial.print("BPM: ");Serial.println(bpm); 
    Serial.print("Ambient = "); Serial.print(readAmbientTempC(0x5A));
    Serial.print("*C\tObject = "); Serial.print(readObjectTempC(0x5A)); Serial.println("*C");
    Serial.print("QS: ");Serial.println(QS);

    if ((bpm > 180 || bpm < 60) && id_animal == 1)
    {
      doitTOPIC("Alerta - Canino!!!", "Valor de pulso cardiaco fuera de rango", top); //clear way with topic
      Serial.println ("delay perro");
      delay (180000);
    }
    else if ((bpm > 220  || bpm < 140) && id_animal == 2)
    {
      doitTOPIC("Alerta - Felino!!!", "Valor de pulso cardiaco fuera de rango", top); //clear way with topic
      Serial.println ("delay gato");
      delay (180000);
    }
    

    QS = false;                      // reset the Quantified Self flag for next time
  }

  delay(500);                             //  take a break
}
