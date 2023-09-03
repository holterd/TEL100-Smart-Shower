/*Bibliotek*/

#include <Servo.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <Stepper.h>
#include <SPI.h>
#include <MFRC522.h>

/*Pins*/

const int joyXPin = A0;                                 // Joystick X-axis pin
const int joyYPin = A1;                                 // Joystick Y-axis pin
const int but = A2;                                     // Joystick knapp pin
const int tempPin = A3;                                 // Temperatur sensor pin
const int servoPinTemp = 5;                             // Servo temperatur pin
const int servoPinOn = 6;                               // Servo vann på/avpin
const int avPin = 8;                                    // For å skru av dusj
//RFID er koblet fra 13 til 9

/*Globale variabler*/

//System
bool off = true;                                        // Dvale eller ikke med tanke på scanning
bool prev_edit = false;                                 // Redigeringsmodus 
bool edit = false;

//RFID                                
#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc522(SS_PIN, RST_PIN);                       // Definere RFID
byte on = LOW;

// ID bank
const int MAX_USERS = 10;                               // Max brukere i dusjen
const int parameters = 3;                               // Antall brukerfdefinerte variabler
int userVal[MAX_USERS][parameters] = {{2000, 45, 26}};  // høyde = [0]; degrees = [1]; temp = [2], liste med brukervariabler ideksert på samme måte som brukere i banken + admin bruker   
String userBank[MAX_USERS] = {"2c6ede37"};              // Brukere i banken + admin
int numUsers = 1;                                       // Antall brukere i babken nå
int degrees_user;                                       // Brukervariabler
int temp_user;
int height_user;

//Joystick
byte butPress;                                          // Knapp på joystick
byte butState;
byte prevState_but = LOW;        
int hor = 0;                                            // Joystick bevegelse for temperatur som starter på 0 grader
int vert = 0;                                           // Joystick bevegelse for stepper

//Servo
Servo servoMotorTemp;                                   // Servo for temperatur
int degrees = 0;                                        // Vinkel på servo
int prev_degrees;
float hastighet_servo = 5;                              // Max grader rotasjon
Servo servoMotorOn;                                     // Servo for vann på/av
int degrees_shower_on = 0;             
int prev_degrees_on;
int shower_def = 90;                                    // Grader som servo må rotere for å skru av/på spesefik dusj

//Stepper motor
const int stepsPerRevolution = 2038;                    // Antall steg på stepper
Stepper myStepper(stepsPerRevolution, 2, 3, 4, 7);
const int speed = 15;                                   // Hvor rask stepper skal bevege seg
int prev_steps = 0;                   
int steps;                                              // Stegvariabel til joystick
int height = 0;
int steps_on = 1000;
int hastighet_stepper = 1000;                           // Hvor rask stepper skal bevege seg i forhold til joystick

//LCD
LiquidCrystal_I2C lcd(0x27,16,2);

//Temperatur sensor
int temp;
#define DHTTYPE DHT22                                   // Temperatur sensor
DHT dht(tempPin, DHTTYPE);


void setup() {
  Serial.begin(9600);
  
  //RFID
  SPI.begin();
  mfrc522.PCD_Init();
 
  //Joystick
  pinMode(but, INPUT_PULLUP);
  pinMode(joyXPin, INPUT);
  pinMode(joyYPin, INPUT);

  //Servo
  servoMotorTemp.attach(servoPinTemp);
  servoMotorOn.attach(servoPinOn);

  //Stepper
  myStepper.setSpeed(speed);

  //LCD
  lcd.init();
  lcd.backlight();

  //Temp sensor
  dht.begin();
}

void loop() {

  // Starter i dvale, og dersom den skal vekkes går den til vanlig innstillinger 
  servoMotorTemp.write(degrees);
  servoMotorOn.write(degrees_shower_on);
  off = true;           
  edit = false;
  
  //LCD
  lcd.setCursor(4,0);
  lcd.print("Welcome");
  lcd.setCursor(1,1);
  lcd.print("Scan to start!");

  Serial.println("Nå er den av og skanner etter kort");

  // Sjekker om RFID kort skannes
  mfrc522.PICC_IsNewCardPresent(); 

  // Skanner kort
  if (!mfrc522.PICC_ReadCardSerial()) {
    delay(100);
    return;
  }

  //Scanner kort og lager hexadesimal string
  Serial.println("Scanning card...");
  String cardUID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    cardUID += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    cardUID += String(mfrc522.uid.uidByte[i], HEX);
  }

  //Bekreftelse i seriell leser
  Serial.print("Card UID: ");
  Serial.println(cardUID);

  // Henter riktig indeks skann
  int userIndex = checkDatabase(cardUID);

  // Gjør klar skjerm for ny beskjed
  lcd.clear();

  // Dersom ny bruker skal en komme rett til edit
  if (userIndex == -1){
    edit = true;
    addUserToBank(cardUID, numUsers);
    Serial.print(userIndex);
    userIndex = numUsers;
    numUsers++;
    Serial.print(userIndex);
  }

  // While-løkke som kjører inntil off blir satt til false, altså at den er på og kan velges til å redigere eller ikke
  while (off) {

    //Joystick knapp som endrer edit/on
    butPress = digitalRead(but);
    butState = !butPress;
    degrees_shower_on = shower_def;
    /*
    Nå er den i på modus
    */

    if (!edit) {
      
      //Endrer brukerverdier fra banken
      height_user = userVal[userIndex][0];
      temp_user = userVal[userIndex][1];
      degrees_user = userVal[userIndex][2];

      // Hvis ikke riktig justering på grade bryteren
      if (degrees != degrees_user){
        //LCD melding
        lcd.setCursor(0,1);
        lcd.print("Adjusting temp");
        degrees = degrees_user;
        servoMotorTemp.write(degrees);
      }
      // Høyde stiller på stepper
      if (height_user != height){
        
        //LCD melding
        lcd.setCursor(0,0);
        lcd.print("Adjusting height");
        
        //Estimer hvor mange steps stepper må rotere
        int steps_on = height_user - height;
        myStepper.step(steps_on);
        height = height_user;
      }
        
      // Setter servo for på/av dusj i rett stilling hvis dusjen ikke allerede er det
      degrees_shower_on = shower_def;

      if (degrees_shower_on != prev_degrees_on){
        lcd.clear();
        lcd.setCursor(3,0);
        lcd.print("Shower on");
        prev_degrees_on = degrees_shower_on;
        servoMotorOn.write(degrees_shower_on);
        delay(1500);
      }

      prev_degrees_on = degrees_shower_on;
      
      // Temperatursensor leser
      int temp = dht.readTemperature();
      delay(1);        

   
      lcd.setCursor(0,0);
      lcd.print("Saved temp: ");
      lcd.print(temp_user);
      lcd.print(char(223));
      lcd.print("C");
      lcd.setCursor(0,1);
      lcd.print("Water temp: ");
      lcd.print(temp);
      lcd.print(char(223));
      lcd.print("C");

      // Skriver til seriel leser
      Serial.println("Nå er den på");
    }
    
    /* 
    Nå er den i edit modus
    */

    else {
      
      //Lesing av joystick rettninger
      int joyX = analogRead(joyXPin);
      int joyY = analogRead(joyYPin);
      
      // Map joystick verdier for servo 
      int hor = map(joyX, 0, 1023, -hastighet_servo, hastighet_servo);
              
      //Maks/min rotasjon samt faktisk rotasjon
      degrees += hor;
      if (degrees > 179){
        degrees = 179;
      }
      else if (degrees < 0){
        degrees = 0;
      }
      else {
        degrees += hor;
      }

      //Oppdater kun hvis det er en endring i joystick 
      if (prev_degrees != degrees){
        
        // Skriver endring på possisjon til servo
        servoMotorTemp.write(degrees);
        degrees_user = degrees;
      }

      // Vertikal bevegelse, stepper
      vert = analogRead(joyYPin);
      steps = map(vert, 0, 1023, -hastighet_stepper, hastighet_stepper);
      
      if (steps != prev_steps && abs(steps) > 100){
        
        //Lagrer høyden og roterer stepper
        myStepper.step(steps);
        height += steps;
        prev_steps = steps;
        height_user = height;
      }

      //Temperatursensor leser
      int temp = dht.readTemperature();
      delay(1);

      //LCD
      lcd.setCursor(3,0);
      lcd.print("Settings");
      lcd.setCursor(0,1); 
      lcd.print("Water temp: ");
      lcd.print(temp);
      lcd.print(char(223));
      lcd.print("C");

      userVal[userIndex][0] = height_user;
      userVal[userIndex][1] = temp;
      userVal[userIndex][2] = degrees_user;

      // Skriver til seriel leser
      Serial.println("Nå kan den redigeres");
    }
    
    // Sjekk om knappen ble trykket og reset edit
    if (butState > prevState_but) {
      reset();
      edit = !prev_edit;
    }
    
    // Sikre systemstatus fra forige iterasjon
    prevState_but = butState;
    prev_edit = edit;

    // Knapp for å avslutte dusj
    on = digitalRead(avPin);
    
    // Avslutt løkken
    if (on) {
      reset();

      lcd.setCursor(4,0);
      lcd.print("Welcome");
      lcd.setCursor(1,1);
      lcd.print("Scan to start!");
      
      degrees_shower_on = 0;
      prev_degrees_on = degrees_shower_on;
      myStepper.step(-height);
      height = 0;
      off = false;
    }
  }
}


// Funksjon som resetter parametere
void reset(){
  temp = 0;
  lcd.clear();
  return; 
}

// Sjekker om ID i hex er et element i listen userBank, og returnerer indexen til brukeren eller -1 
int checkDatabase(String cardUID) {
  for (int i = 0; i < numUsers; i++) {
    if (cardUID == userBank[i]) {
      return i;
    }
  }
  return -1;  // Not found in the bank
}

// Legger til bruker i banken liste
void addUserToBank(String cardUID, int index) {
  userBank[index] = cardUID;
  return;
}




