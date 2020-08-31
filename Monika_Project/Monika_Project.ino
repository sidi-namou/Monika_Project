

#include <SPI.h>
#include <SD.h>
#include <LiquidCrystal_PCF8574.h>
#include <Wire.h>
#include <ThreeWire.h>  
#include <RtcDS1302.h>
#include <SimpleDHT.h>
#include <SHT1x.h>


#define relay1 25
#define relay2 26
#define relay3 27
#define lumonisitySensorPin 0 //A0
#define seuilSoir   300 
#define levelSensorPin  1 // A1
#define dataPin  44 
#define clockPin 45
#define DateState 1
#define TempState 2
#define soir 1
#define jour 0
#define dixMinutes 600000
#define lampOn  1
#define lampOff 0

SHT1x sht1x(dataPin, clockPin);

ThreeWire myWire(4,5,2); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);


int pinDHT11 = 3;
SimpleDHT11 dht11(pinDHT11);

void SDCard_Init(void);
void LCDInit(void);
void RTCDS1302_Init(void);
void printDateTime(const RtcDateTime& dt);
int Read_TempHumDHT11(byte *temp, byte *humid);
void Init_Relay_And_Lumonisity(void);
void affichageDate(void);
void affichageTempAtmos(void);
void File_Name_Update(const RtcDateTime& now);
void WriteTemp(const RtcDateTime& now, byte temp);
void WriteHumid(const RtcDateTime& now, byte humid);
void WriteHumidSond(const RtcDateTime& now);
void WriteTempSond(const RtcDateTime& now);
void LumJardinAlum(const RtcDateTime& now);
void LumJardinEteint(const RtcDateTime& now);
int waterLevel(void);
void WriteWaterLevel(const RtcDateTime& now);
void alarmWtaerLevel(const RtcDateTime& now);
void writeHumidExt(const RtcDateTime& now, byte humid);
void writeArosage(const RtcDateTime& now, byte humid);
void writeCycleArosage(int i);

#define countof(a) (sizeof(a) / sizeof(a[0]))

LiquidCrystal_PCF8574 lcd(0x27); // set the LCD address to 0x27 for a 16 chars and 2 line display
const int chipSelect = 53;

char fileName[30];// à initialiser
char dataToWrite[200];
int temps,lastTime,lastTime1,temps1;
int etat = DateState;
int lampState = lampOff;
int heur = jour;
int lumonisity;

void setup() {

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  // Initialisation de la carte SD
  SDCard_Init();
  // Initialisation de l'écran LCD
  LCDInit();
  
  // Initialisation de l'horloge
  
  RTCDS1302_Init();

  // Initialisations des relais et du capteur de lumonisité
  Init_Relay_And_Lumonisity();
  
  temps = millis();
  lastTime = temps;
  temps1 = millis();
  lastTime1 = temps1;
}



void loop() {

  
    byte tempExt;// temperature de l'air
    byte humidExt;// l'humidité de l'air
   // Chaque jour à minuit sera crée un fichier
  // Lecture de l'heure et de la date
   RtcDateTime now = Rtc.GetDateTime();
   File_Name_Update(now);
  // Affichage par alternance sur l'écran maximum 50 jours
  temps = millis();
  if(temps - lastTime > 5000){
    
    lastTime = temps;
    if(etat == TempState) etat = DateState;
   else  etat = TempState;
   

  }
  switch(etat){
    case DateState:{
     affichageDate();
      break;
    }
    case TempState:{
     affichageTempAtmos();
     break;
    }
  }
 
   
  // Declanchement de la lumiére avec le relai 1
  
   if( now.Hour()==16 && now.Minute()==30)heur = soir;
   else if(now.Hour()==2 && now.Minute()==30) heur = jour;
   lumonisity = analogRead(lumonisitySensorPin);
   // si c'est le soir ainsi que le capteur capte moins que le seuil du soir alors on declanche la lumière
   
   if(lumonisity <= seuilSoir && heur == soir){
    digitalWrite(relay1,HIGH);  
    lampState = lampOn;
   LumJardinAlum(now);
   }
   else if(heur == jour && lampState == lampOn){
    digitalWrite(relay1,LOW);
    lampState = lampOff;
    LumJardinEteint(now);
     
   }

  // attendre 10 minutes.
  temps1 = millis();
   if(temps1 - lastTime1 >  dixMinutes ){
    lastTime1 = temps1;
    // Lecture de la temperature et l'humidité
    Read_TempHumDHT11(&tempExt, &humidExt);
    // ecriture de la temperature sur la carte SD
    WriteTemp(now,tempExt);
    // Ecriture de l'humidité sur la carte SD
    WriteHumid(now,humidExt);
    // Ecriture de l'humidité de la sonde
    WriteHumidSond(now);
    // Ecriture de la temperature de la sonde
    WriteTempSond(now);
    // Si le niveau d'eau est superieur à 10% alors :
    if(waterLevel() > 10){
    // Ecrire le niveau d'eau dans la carte SD 
    WriteWaterLevel(now);
    
    }
    else{
     // Ecrire Alarme manque d'eau et on active le relai 3
     alarmWtaerLevel(now);
    }
    
    if(sht1x.readHumidity() > 9){
      writeHumidExt(now,sht1x.readHumidity());// ecriture d'humidité du sol
    }
    else if(sht1x.readHumidity() < 9 && sht1x.readTemperatureC() > 3){
       writeArosage(now,sht1x.readHumidity());
       for(int i=0;i<5;i++){
        // actionner relai 2 pendant 30 secondes
         digitalWrite(relay2,HIGH);
         delay(30000);
         // arreter pendant 30secondes
         digitalWrite(relay2,LOW);
         delay(30000);
         // Ecriture du fin de cycle d'arosage sur la carte SD
         writeCycleArosage(i);
}
       
       
    }
   }
  


}



void SDCard_Init(void){
  /*  
 ** MOSI - pin 51
 ** MISO - pin 50
 ** CLK - pin 52
 ** CS - pin 53 
*/
    Serial.print("Initialisation de SD card...");

  
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
  
  }
   else{
    Serial.println("card initialized.");
   }

}
void LCDInit(void){
  int error;
  Serial.println("LCD...");
  Wire.begin();
  Wire.beginTransmission(0x27);
  error = Wire.endTransmission();
  Serial.print("Error: ");
  Serial.print(error);

  if (error == 0) {
    Serial.println(": LCD found.");
    lcd.begin(16, 2); // initialize the lcd
    // Affiche Bonjour Monika sur l'écran
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Bonjour ");
  lcd.setCursor(0, 1);
  lcd.print(" Monika  ");
  // attendre 5s
  delay(5000);

  } else {
    Serial.println(": LCD not found.");
  } 

}

void RTCDS1302_Init(void){
  
// CONNECTIONS:
// DS1302 CLK/SCLK --> 5
// DS1302 DAT/IO --> 4
// DS1302 RST/CE --> 2
// DS1302 VCC --> 3.3v - 5v
// DS1302 GND --> GND

    Rtc.Begin();
    // Temps de compilation
    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    printDateTime(compiled);
    Serial.println();

    if (!Rtc.IsDateTimeValid()) 
    {
        // Common Causes:
        //    1) first time you ran and the device wasn't running yet
        //    2) the battery on the device is low or even missing

        Serial.println("RTC lost confidence in the DateTime!");
        Rtc.SetDateTime(compiled);
    }

    if (Rtc.GetIsWriteProtected())
    {
        Serial.println("RTC was write protected, enabling writing now");
        Rtc.SetIsWriteProtected(false);
    }

    if (!Rtc.GetIsRunning())
    {
        Serial.println("RTC was not actively running, starting now");
        Rtc.SetIsRunning(true);
    }
    // Temps de DS1302
    RtcDateTime now = Rtc.GetDateTime();
    if (now < compiled) 
    {
        Serial.println("RTC is older than compile time!  (Updating DateTime)");
        Rtc.SetDateTime(compiled);
    }
    else if (now > compiled) 
    {
        Serial.println("RTC is newer than compile time. (this is expected)");
    }
    else if (now == compiled) 
    {
        Serial.println("RTC is the same as compile time! (not expected but all is fine)");
    }

  
}
void printDateTime(const RtcDateTime& dt)
{
    char datestring[20];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
}
int Read_TempHumDHT11(byte *temp, byte *humid){
  int err = SimpleDHTErrSuccess;
  if ((err = dht11.read(temp, humid, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT11 failed, err="); Serial.println(err);delay(1000);
    return 0;
  }
  return 1;
}
void Init_Relay_And_Lumonisity(void){
  pinMode(relay1,OUTPUT);
  pinMode(relay2,OUTPUT);
  pinMode(relay3,OUTPUT);
  int lumonisity = analogRead(lumonisitySensorPin);
  Serial.println(" ");
  Serial.print("La lumonisitée est : ");
  Serial.println(lumonisity);
  
  float WaterLevel = analogRead(levelSensorPin)*100/1023;
  Serial.println(" ");
  Serial.print("Le niveau de l'eau est : ");
  Serial.println(WaterLevel,DEC);
}

void affichageDate(void){
   RtcDateTime now = Rtc.GetDateTime();
   char datestring[16];
   char heureString[16];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR(" %02u/%02u/%04u "),
            now.Day(),
            now.Month(),
            now.Year() );
     snprintf_P(heureString, 
            countof(heureString),
            PSTR(" %02u:%02u:%02u "),
            now.Hour(),
            now.Minute(),
            now.Second() );
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(datestring);
  lcd.setCursor(0,1);
  lcd.print(heureString);
}
void affichageTempAtmos(void){
  float temp_c;
  char Tempstring[16];
  RtcDateTime now = Rtc.GetDateTime();
  char heureString[16];
  temp_c = sht1x.readTemperatureC();
  sprintf(Tempstring,"Temp: %f",temp_c);
       snprintf_P(heureString, 
            countof(heureString),
            PSTR(" %02u:%02u:%02u "),
            now.Hour(),
            now.Minute(),
            now.Second() );
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(Tempstring);
  lcd.setCursor(0,1);
  lcd.print(heureString);
}
void File_Name_Update(const RtcDateTime& now){
  
  if(now.Hour() == 0 && now.Minute()==0 ){
     snprintf_P(fileName, 
            countof(fileName),
            PSTR("%04u-%02u-%02u - Monika.txt "),
            now.Year(),
            now.Month(),
            now.Day() );    
  }
  
}

void WriteTemp(const RtcDateTime& now, byte temp){
    File dataFile = SD.open(fileName,FILE_WRITE);
    if(dataFile){
      
      snprintf_P(dataToWrite, 
            countof(dataToWrite),
            PSTR("%02u-%02u-%02u - La temperature exterieur est de %02u°"),
            now.Hour(),
            now.Minute(),
            now.Second(),
            temp);
       dataFile.println(dataToWrite);
       dataFile.close();
    }
    else {
      Serial.print("error opening ");
      Serial.println(fileName);
    }
}
void WriteHumid(const RtcDateTime& now, byte humid){
    File dataFile = SD.open(fileName,FILE_WRITE);
    if(dataFile){
      
      snprintf_P(dataToWrite, 
            countof(dataToWrite),
            PSTR("%02u-%02u-%02u - Le taux humidité est de %02u%"),
            now.Hour(),
            now.Minute(),
            now.Second(),
            humid);
       dataFile.println(dataToWrite);
       dataFile.close();
    }
    else {
      Serial.print("error opening ");
      Serial.println(fileName);
    }
}

void WriteHumidSond(const RtcDateTime& now){
  File dataFile = SD.open(fileName,FILE_WRITE);
  float humidity;
    if(dataFile){
      humidity = sht1x.readHumidity();
      snprintf_P(dataToWrite, 
            countof(dataToWrite),
            PSTR("%02u-%02u-%02u - L'humidité est de %f"),
            now.Hour(),
            now.Minute(),
            now.Second(),
            humidity);
       dataFile.println(dataToWrite);
       dataFile.close();
    }
    else {
      Serial.print("error opening ");
      Serial.println(fileName);
    }
}

void WriteTempSond(const RtcDateTime& now){
   File dataFile = SD.open(fileName,FILE_WRITE);
  float temp_c;
    if(dataFile){
      temp_c = sht1x.readTemperatureC();
      snprintf_P(dataToWrite, 
            countof(dataToWrite),
            PSTR("%02u-%02u-%02u - Le temperature du sol est de %f°C"),
            now.Hour(),
            now.Minute(),
            now.Second(),
            temp_c);
       dataFile.println(dataToWrite);
       dataFile.close();
    }
    else {
      Serial.print("error opening ");
      Serial.println(fileName);
    }
}

int waterLevel(void){
  return (int )analogRead(levelSensorPin)*100/1023;
}

void WriteWaterLevel(const RtcDateTime& now){
  File dataFile = SD.open(fileName,FILE_WRITE);
  int waterLev;
    if(dataFile){
      waterLev = waterLevel();
      snprintf_P(dataToWrite, 
            countof(dataToWrite),
            PSTR("%02u-%02u-%02u - Le niveau de l'eau est à %d%"),
            now.Hour(),
            now.Minute(),
            now.Second(),
            waterLev);
       dataFile.println(dataToWrite);
       dataFile.close();
    }
    else {
      Serial.print("error opening ");
      Serial.println(fileName);
    }
    // desactiver le reali 3 au cas où c'est activé
    digitalWrite(relay3,LOW);
}
void alarmWtaerLevel(const RtcDateTime& now){
  File dataFile = SD.open(fileName,FILE_WRITE);
  int waterLev;
    if(dataFile){
      waterLev = waterLevel();
      snprintf_P(dataToWrite, 
            countof(dataToWrite),
            PSTR("%02u-%02u-%02u - Alarme manque d'eau %d% demarrage d'arrosage "),
            now.Hour(),
            now.Minute(),
            now.Second(),
            waterLev);
       dataFile.println(dataToWrite);
       dataFile.close();
    }
    else {
      Serial.print("error opening ");
      Serial.println(fileName);
    }
    // Activer le relai 3
    digitalWrite(relay3,HIGH);
}
void writeHumidExt(const RtcDateTime& now, byte humid){
   File dataFile = SD.open(fileName,FILE_WRITE);
    if(dataFile){
      
      snprintf_P(dataToWrite, 
            countof(dataToWrite),
            PSTR("%02u-%02u-%02u - Sol humide à valeur %02u% pas besoin d'arrosage"),
            now.Hour(),
            now.Minute(),
            now.Second(),
            humid);
       dataFile.println(dataToWrite);
       dataFile.close();
    }
    else {
      Serial.print("error opening ");
      Serial.println(fileName);
    }
  
}
void writeArosage(const RtcDateTime& now, byte humid){
     File dataFile = SD.open(fileName,FILE_WRITE);
    if(dataFile){
      
      snprintf_P(dataToWrite, 
            countof(dataToWrite),
            PSTR("%02u-%02u-%02u - Sol humide à %02u% manque d'eau! besoin d'arrosage"),
            now.Hour(),
            now.Minute(),
            now.Second(),
            humid);
       dataFile.println(dataToWrite);
       dataFile.close();
    }
    else {
      Serial.print("error opening ");
      Serial.println(fileName);
    }
}

void writeCycleArosage(int i){
      File dataFile = SD.open(fileName,FILE_WRITE);
      RtcDateTime now = Rtc.GetDateTime();
      if(i==4){
        if(dataFile){
      
      snprintf_P(dataToWrite, 
            countof(dataToWrite),
            PSTR("%02u-%02u-%02u - Attente de 10min avant verification de l'humidité du sol, de la temperature et du niveau d'eau"),
            now.Hour(),
            now.Minute(),
            now.Second());
       dataFile.println(dataToWrite);
       dataFile.close();
    }
    else {
      Serial.print("error opening ");
      Serial.println(fileName);
    }
      }
      else{
        if(dataFile){
      
      snprintf_P(dataToWrite, 
            countof(dataToWrite),
            PSTR("%02u-%02u-%02u - Cycle arrosage %d effectué"),
            now.Hour(),
            now.Minute(),
            now.Second(),
            i);
       dataFile.println(dataToWrite);
       dataFile.close();
    }
    else {
      Serial.print("error opening ");
      Serial.println(fileName);
    }
      }
    
  
}
void LumJardinAlum(const RtcDateTime& now){
    File dataFile = SD.open(fileName,FILE_WRITE);
    if(dataFile){
      
      snprintf_P(dataToWrite, 
            countof(dataToWrite),
            PSTR("%02u-%02u-%02u - La lumière du jardin s'allume "),
            now.Hour(),
            now.Minute(),
            now.Second() );
       dataFile.println(dataToWrite);
       dataFile.close();
    }
    else {
      Serial.print("error opening ");
      Serial.println(fileName);
    }
}
void LumJardinEteint(const RtcDateTime& now){
  File dataFile = SD.open(fileName,FILE_WRITE);
    if(dataFile){
      
      snprintf_P(dataToWrite, 
            countof(dataToWrite),
            PSTR("%02u-%02u-%02u - La lumière du jardin s'éteint "),
            now.Hour(),
            now.Minute(),
            now.Second() );
       dataFile.println(dataToWrite);
       dataFile.close();
    }
    else {
      Serial.print("error opening ");
      Serial.println(fileName);
    }
}
