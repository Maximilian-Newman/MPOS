String CURRENT_VERSION = "2.4 BETA";


// default settings
bool blueFilter = false;
bool invertColor = false;
bool ramTracking = true;
byte screenOrientation = 0;
byte brightnessPercent = 100;
int GMTOffset = 2;
bool volume = true;
String deviceName = "My MaxPhone 2";
bool keyboardVisible = false;
bool darkMode = false;
bool bluetoothActive = false;



#include <UTFT.h>
#include <URTouch.h>

#include <Wire.h>
#include <ds3231.h>

#include <SD.h>

#include <MemoryFree.h>

#include "WatchDog.h"
#include <setjmp.h>


#include <MFRC522.h>
//#include <SPI.h>


#define blu_EN A12


/*
----------------------------------------------------------------------------------------------------------------------------------------------
WARNING !!!!!!
The following documentation is criminally out of date
----------------------------------------------------------------------------------------------------------------------------------------------





   MaxCloud file formats:

      .mci   (MaxCloud Image)

        description: image RGB format
                     RGB 1,1,1 means transparent

        structure: width,height,r,g,b,r,g,b,r,g,b,r,g,b, ...

        example: 3,3,0,0,0,255,255,255,0,0,0,255,255,255,0,0,0,255,255,255,0,0,0,255,255,255,0,0,0,       (3x3 white and black grid)


      .mrt   (Maxcloud RooT)

      description: root file used by the system to store OS-used data

      structure: depends on the file, most of them are basically csv's



      .bim    (Binary IMage)

        description: binary image icon format. 0=background, 1=icon. colors are not specified in the file, and are decided when displaying

        structure: width,height,color,color,color,color,color,color,color,color,...

        example: 3,3,0,1,0,1,0,1,0,1,0,       (3x3 grid)



  Other files may also be added for use as virtual memory within the 'SYS' folder

   File directory:

   /MPOS
      /F
        All user files are stored under this folder.
        
      /S

        SCREEN.MLI

          normal .mli file that keeps track of all the objects on the screen
          object names start with the name of the app they belong to and an underscore
          OS object names start with "SYS_"

          ex: SETTINGS_backButton: ...
              SYS_homeButton: ...

          screenshots could be made by duplicating this file.

        /RAM.MRT

          stores recent available RAM percentages.

          structure: numbers between 0 and 100 seperated by '\n', maximum of 30 samples stored



        /D

          /HOME.MRT
            contains the layout of apps on the home screen
            names are seperated by '\n'
            The order they appear in this file is the order they will appear on the home screen from left to right

            ex: Settings
                Files
                RFID
                Text

          /APPS.MRT
            contains a list of all the apps installed

            ex: Settings
                Files
                RFID
                Text

          /BACKGRD.MRT
            contains the list of apps allowed to perform background tasks

            ex: SYS
                Text

          /KEYBOARD.MRT
            contains the layout of the keyboard.

            example for french keyboard:
            1234567890AZERTYUIOPQSDFGHJKLMWXCVBN,.!?1234567890<>;:/=+-~_(){}[]#%&$"'\^*@,.!?

          /PASSWORD.MRT
            contains the password to access the device (encrypted)

          /A
            This folder contains app icons for the home screen in a .mci format
            Each filename is the name of the app, or the first 8 characters of the app's name
            
          /R
            /MCLOGO.MCI

              normal .mci image file with the MaxCloud logo

            /WIFION.BIM

              normal .bim file with the icon to show that wifi is connected

            /SWI-OFF.MLI

              normal .mli image file of the switch button used in settings app

            /SWI-ON.MLI

              normal .mli image file of the switch button used in settings app

            /FOLDER.BIM

              normal .bim image file of the folder icon used by the files app

            /MONTHS.MRT

              list of all the months

              structure:
              
              January
              February
              March
              April
              May
              June
              July
              August
              September
              October
              November
              December

            /UP.BIM

              normal .bim image of an arrow pointing up

            /DOWN.BIM

              normal .bim image of an arrow pointing down



            other icons and resource files for apps may be present in this folder.



        /SETTINGS

          /BRIGHT.MRT

            stores the brightness of the screen as a percentage

          /BLFILT.MRT

            stores if the bluelight filter is on or off

            structure: 'Y' for yes, 'N' for no

          /COLINVRT.MRT

            stores if invert color mode is on or off

            structure: 'Y' for yes, 'N' for no

          /DARK.MRT

            stores if dark mode is on or off

            structure: 'Y' for yes, 'N' for no

          /SOUND.MRT

            stores if the sound is activated or muted

            structure: 'Y' for sound on, 'N' for muted
          
          ...
            other settings files follow the same Y/N  structure as those above

*/



const byte PROGMEM powerPin = 8;
const byte PROGMEM speakerPin = 9;
UTFT screen(SSD1963_800ALT, 38, 39, 40, 41);
URTouch touch(6, 5, 4, 3, 2);



const unsigned int PROGMEM sampleIntervals = 10000;

byte currentRAM = 0;
byte lastRAM = 0;
unsigned int lastSampleTime = 0;
byte currentSampleLoopPasses = 0;
unsigned long SYS_RAMSampleTime = 0;
unsigned long SYS_loadSampleTime = 0;
unsigned long SYS_nextLoadSampleTime = 0;


String CURRENT_APP = "";
String CONTROLLING_APP = "SYS";

void sample_RAM(){
  unsigned long testRAM = 8192 - freeMemory();
  testRAM *= 100;
  testRAM /= 8192;
  if (testRAM > currentRAM){
    currentRAM = testRAM;
  }
}

//File *allFiles[5];
//bool allFilesUsedSpace[5] = {false};
//int *emergencyFreePointer;

// allows 'new' when clearing a String
void* operator new(size_t, void* ptr) noexcept {
  return ptr;
}

void clearString(String &s) {
  // Ensure no active use before calling this
  s.~String();            // Destroys the current string
  new (&s) String("");      // Reconstructs it in place
}

void printFullMemory(){
  byte *b;
  b = 0;
  for (unsigned int i=0; i<SP; i++){
    Serial.write(*b);
    b += 1;
  }
}

File* allFiles[5];
bool allFilesUsedSpace[5] = {false, false, false, false, false};

void addFileToList(File *fileptr){
  for (byte i=0; i<5; i++){
    if (allFilesUsedSpace[i] == false){
      allFilesUsedSpace[i] = true;
      allFiles[i] = fileptr;
      break;
    }
  }
}

void removeFileFromList(File *fileptr){
  for (byte i=0; i<5; i++){
    if (allFiles[i] == fileptr){
      allFilesUsedSpace[i] = false;
    }
  }
}

void closeAllFiles(){
  for (byte i=0; i<5; i++){
    if (allFilesUsedSpace[i] == true){
      allFilesUsedSpace[i] = false;
      allFiles[i]->close();
    }
  }
  SD.end();
  SD.begin(53);
}



bool openFile(File &file, String path, uint8_t mode){
  if (path.startsWith("/")) {path.remove(0, 1);}
  if (path.startsWith("MPOS/")) {path.remove(0, 5);}

  bool sysAccess = false;
  if (path.startsWith("S/")) {
    path.remove(0, 2);
    sysAccess = true;
  }
  else if (path.startsWith("F/")) {path.remove(0, 2);}

  String pathStart = "MPOS/";

  if (sysAccess){
    pathStart += "S/";
  }
  else{
    pathStart += "F/";
  }

  file = SD.open(pathStart + path, mode); // don't replace with openFile()
  if (file) {
    addFileToList(&file);
    return true;
  }
  return false;
}

void closeFile(File &file){
  removeFileFromList(&file);
  file.close();
}





void large_instant_notice(String text) {
  if (CONTROLLING_APP == "SYS"){
    setColor(40, 40, 150);
    setBackColor(40, 40, 150);
    screen.fillRoundRect(40, 70, screen.getDisplayXSize()-40, 300);
    setColor(0, 0, 0);
    screen.drawRoundRect(40, 70, screen.getDisplayXSize()-40, 300);
    setColor(255, 255, 255);
    setFont("xlarge");
    screen.print(text, CENTER, 150);
  }
}

void quit_all_apps(){
  if (CONTROLLING_APP == "SYS"){
    File file;
    openFile(file, "S/D/APPS.MRT", FILE_READ);
    if (file){
      while (file.available()){
        String appName = file.readStringUntil('\n');
        quit_app(&appName);
      }
      closeFile(file);
    }
  }
}

void shut_down() {

  if (CONTROLLING_APP == "SYS" or CONTROLLING_APP == "SETTINGS"){
    CONTROLLING_APP = "SYS";

    large_instant_notice("Goodbye!");
    quit_all_apps();

    bluetooth_stop_scan();
    bluetooth_exit_AT();
    bluetooth_power_off();

    closeAllFiles();

    WatchDog::stop();
    digitalWrite(powerPin, LOW);
    while (true);
  }
}

void playTone(int freq, int duration) {
  if (volume) {
    tone(speakerPin, freq, duration);
  }
}




void addSound(unsigned long Time, int freq, int duration) {
  Time += millis();
  SD.remove(String("/MPOS/S/") + "SOUNDT.MRT");
  File soundLogR;
  openFile(soundLogR, "S/SOUND.MRT", FILE_READ);
  File soundLogW;
  openFile(soundLogW, "S/SOUNDT.MRT", FILE_WRITE);
  bool entered = false;

  while (soundLogR.available()) {
    String logTimeStr = soundLogR.readStringUntil(',');
    unsigned long logTimeInt = logTimeStr.toInt();

    if (entered == false and logTimeInt > Time) {
      entered = true;
      soundLogW.print(Time);
      soundLogW.print(",");
      soundLogW.print(freq);
      soundLogW.print(",");
      soundLogW.print(duration + '\n');
    }

    soundLogW.print(logTimeInt);
    soundLogW.print(",");
    String line = soundLogR.readStringUntil('\n');
    soundLogW.print(line + '\n');
  }

  if (entered == false) {
    soundLogW.print(Time);
    soundLogW.print(",");
    soundLogW.print(freq);
    soundLogW.print(",");
    soundLogW.print(duration + '\n');
  }
  closeFile(soundLogW);
  closeFile(soundLogR);
  SD.remove(String("/MPOS/S/") + "SOUND.MRT");

  openFile(soundLogW, "S/SOUND.MRT", FILE_WRITE);
  openFile(soundLogR, "S/SOUNDT.MRT", FILE_READ);

  while (soundLogR.available()) {
    String line = soundLogR.readStringUntil('\n');
    soundLogW.print(line + '\n');
  }
  sample_RAM();
  closeFile(soundLogW);
  closeFile(soundLogR);
  SD.remove(String("/MPOS/S/") + "SOUNDT.MRT");
}


/*
byte NTPhours;
byte NTPminutes;
byte NTPseconds;


bool wifi_connected = false;
String lastWiFiCom = "";
bool wifi_problem = false;


bool wifi_on() { // esp chip connected to physical switch
  // so arduino doesn't know if it's on or not
  // unless it asks
  while (Serial2.available()) {
    Serial2.read();
  }
  Serial2.print("exist\n");
  delay(10);
  if (Serial2.readStringUntil('\n').indexOf("yes") >= 0) {
    return true;
  }
  else {
    wifi_connected = false;
    return false;
  }
}

bool NTPUpdate() { // only changes variables for NTPtime, doesn't return time or set the phone time
  // returns false if there is an error
  if (!wifi_on()) {
    return false;
  }
  if (!wifi_connected) {
    return false;
  }

  while (Serial2.available()) {
    Serial2.read();
  }

  Serial2.print("time\n");
  while (!Serial2.available());
  NTPhours = Serial2.readStringUntil(':').toInt() + GMTOffset;
  if (NTPhours > 24) {
    NTPhours -= 24;
  }
  if (NTPhours < 0) {
    NTPhours += 24;
  }
  NTPminutes = Serial2.readStringUntil(':').toInt();
  NTPseconds = Serial2.readStringUntil('\n').toInt();
  return true;
}
*/
/*
// modified from https://forum.arduino.cc/t/hex-string-to-byte-array/563827/4
byte nibble(char c){
  if (c >= '0' && c <= '9')
    return c - '0';

  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;

  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;

  return 0;  // Not a valid hexadecimal character
}

void convertMAC(String hexString, byte byteArray[]){
  byte currentByte = 0;
  byte byteIndex = 0;

  for (byte charIndex = 0; charIndex < 12; charIndex++){
    bool oddCharIndex = charIndex & 1;

    if (!oddCharIndex)
    {
      // Odd characters go into the high nibble
      currentByte = nibble(hexString[charIndex]) << 4;
    }
    else
    {
      // Odd characters go into low nibble
      currentByte |= nibble(hexString[charIndex]);
      byteArray[byteIndex] = currentByte;
      currentByte = 0;
      byteIndex += 1;
    }
  }
}
*/






struct ts Time;


MFRC522 mfrc522(A14, A15);
MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;
byte RFIDbuffer[18];
byte RFIDsize = 18;

void RFIDCardDataToFile() {
  SD.remove(String("/MPOS/S/") + "RFID.MRT");
  File cardData;
  openFile(cardData, "S/RFID.MRT", FILE_WRITE);

  for (byte sectorNum = 1; sectorNum <= 16; sectorNum++) {
    RFIDauthenticate(sectorNum * 4 + 3);
    for (byte blockAddr = 0; blockAddr < 3; blockAddr++) {
      String block = RFIDreadBlock(sectorNum * 4 + blockAddr);
      cardData.print(block);
      //Serial.print(block);
    }
  }
  sample_RAM();
  closeFile(cardData);
}

String RFIDreadBlock(byte blockAddr) {
  // Read data from the block
  status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(blockAddr, RFIDbuffer, &RFIDsize);
  if (status != MFRC522::STATUS_OK) {
    return "";
  }
  String out = "";
  for (byte i = 0; i < 16; i++) {
    if (char(RFIDbuffer[i]) != '\0') {
      out += char(RFIDbuffer[i]);
    }
  }
  return out;
}



bool RFIDwriteCardDataFromFile() {
  File cardData;
  openFile(cardData, "S/RFID.MRT", FILE_READ);
  //addFileToList(&cardData);
  String failedSectors = "";

  for (byte sectorNum = 1; sectorNum <= 16; sectorNum++) {
    bool success = RFIDauthenticate(sectorNum * 4 + 3);
    while (success == false){
      failedSectors += " " + String(sectorNum);
      sectorNum += 1;

      if (sectorNum > 16) {
        addNotification("RFID Card Problems", "Failed to authenticate sectors:" + failedSectors);
        if (cardData.available()){
          closeFile(cardData);
          addNotification("RFID Write Incomplete", "Insufficient space on card");
          return false;
        }
        closeFile(cardData);
        return true;
      }
      delay(25);
      success = RFIDauthenticate(sectorNum * 4 + 3);
    }
    for (byte blockAddr = 0; blockAddr < 3; blockAddr++) {
      String blockData = "";
      while (blockData.length() < 15) {
        if (cardData.available()){
          blockData += char(cardData.read());
        }
        else{
          blockData += char(255);
        }
      }

      //Serial.println(blockData);
      
      delay(25);
      success = RFIDwriteBlock(sectorNum * 4 + blockAddr, blockData);
      if (success == false) {
        closeFile(cardData);
        return false;
      }
    }
  }

  if (failedSectors != "") {addNotification("RFID Card Problems", "Failed to authenticate sectors:" + failedSectors);}

  if (cardData.available()) {
    closeFile(cardData);
    addNotification("RFID Write Incomplete", "Insufficient space on card");
    return false;
  }

  closeFile(cardData);
  return true;
}

bool RFIDwriteBlock(byte blockAddr, String dataStr) {

  byte dataBytes[16];
  dataStr.getBytes(dataBytes, 16);

  for (byte i = 0; i < 16; i++) {
    if (dataBytes[i] == 255) {
      dataBytes[i] = 0;
    }
  }

  status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(blockAddr, dataBytes, 16);
  if (status != MFRC522::STATUS_OK) {
    //Serial.print("RFID status: ");
    //Serial.print(status);
    //Serial.print(", ");
    //Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }
  return true;
}

bool RFIDauthenticate(byte trailerBlock) {
  status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  if (status == MFRC522::STATUS_OK) {
    return true;
  }
  return false;
}



const extern uint8_t SmallFont[];
const extern uint8_t BigFont[];
const extern uint8_t segment18_XXL[];
const extern uint8_t Grotesk16x32[];
const extern uint8_t Grotesk24x48[];



void setFont(String fontName) {

  if (fontName == "small" or fontName == "1") {
    screen.setFont(SmallFont);
  }
  else if (fontName == "medium" or fontName == "2") {
    screen.setFont(BigFont);
  }
  else if (fontName == "giant" or fontName == "3") {
    screen.setFont(segment18_XXL);
  }
  else if (fontName == "large" or fontName == "4") {
    screen.setFont(Grotesk16x32);
  }
  else if (fontName == "xlarge" or fontName == "5") {
    screen.setFont(Grotesk24x48);
  }

  else {
    screen.setFont(SmallFont);// font not found
    addNotification(F("FONT ERROR!"), "The font '" + fontName + "' requested by the app '" + CONTROLLING_APP + "' couldn't be found.");
  }
}

unsigned long lastTouchRead = 0;
int lastTouchX = 0;
int lastTouchY = 0;

int touchGetX() {
  if (millis() - lastTouchRead < 50) {
    touch.read();
    lastTouchX = touch.getX();
    lastTouchY = touch.getY();
    lastTouchRead = millis();
  }

  if (screenOrientation == PORTRAIT) {
    return lastTouchX;
  }
  else {
    return screen.getDisplayXSize() - lastTouchX;
  }
}

int touchGetY() {
  if (millis() - lastTouchRead > 50) {
    touch.read();
    lastTouchX = touch.getX();
    lastTouchY = touch.getY();
    lastTouchRead = millis();
  }

  if (screenOrientation == PORTRAIT) {
    return lastTouchY;
  }
  else {
    return screen.getDisplayYSize() - lastTouchY;
  }
}



byte getHour() {
  DS3231_get(&Time);
  return Time.hour;
}
byte getMinute() {
  DS3231_get(&Time);
  return Time.min;
}
byte getSecond() {
  DS3231_get(&Time);
  return Time.sec;
}
byte get_day() {
  DS3231_get(&Time);
  return Time.mday;
}
byte get_month() {
  DS3231_get(&Time);
  return Time.mon;
}
int get_year() {
  DS3231_get(&Time);
  return Time.year;
}


void changeSeconds(struct ts *t, int delta){
  delta += (*t).sec;
  while (delta < 0){
    delta += 60;
    changeMinutes(t, -1);
  }
  while (delta > 59){
    delta -= 60;
    changeMinutes(t, 1);
  }
  (*t).sec = delta;
}
void changeMinutes(struct ts *t, int delta){
  delta += (*t).min;
  while (delta < 0){
    delta += 60;
    changeHours(t, -1);
  }
  while (delta > 59){
    delta -= 60;
    changeHours(t, 1);
  }
  (*t).min = delta;
}
void changeHours(struct ts *t, int delta){
  delta += (*t).hour;
  while (delta < 0){
    delta += 24;
  }
  while (delta > 24){
    delta -= 24;
  }
  (*t).hour = delta;
}

void clockChangeSeconds(int delta){
  DS3231_get(&Time);
  changeSeconds(&Time, delta);
  DS3231_set(Time);
}
void clockChangeMinutes(int delta){
  DS3231_get(&Time);
  changeMinutes(&Time, delta);
  DS3231_set(Time);
}
void clockChangeHours(int delta){
  DS3231_get(&Time);
  changeHours(&Time, delta);
  DS3231_set(Time);
}


unsigned long lastTempRead = 0;
int lastTemp = 0;
float getInternalTemp() {
  if (lastTempRead < millis() - 10000) {
    lastTemp = DS3231_get_treg();
  }
  return lastTemp;
}

String getMonthStr() {
  File monthNames;
  openFile(monthNames, "S/D/R/MONTHS.MRT", FILE_READ);
  //addFileToList(&monthNames);
  byte mon = get_month() -1;
  for (byte i = 0; i < mon; i++){
    monthNames.readStringUntil('\n');
  }
  String monName = monthNames.readStringUntil('\n');
  sample_RAM();
  closeFile(monthNames);
  return monName;
}

String getDateString() {
  String date = String(get_day());
  if (date.endsWith("1")) {
    date += "st ";
  }
  else if (date.endsWith("2")) {
    date += "nd ";
  }
  else if (date.endsWith("3")) {
    date += "rd ";
  }
  else {
    date += "th ";
  }
  
  date += getMonthStr();
  date += " ";
  date += String(get_year());
  return date;
}
String getTimeString(bool showSeconds = false) {
  String Time = " ";
  Time += String(getHour());
  Time += ":";
  if (getMinute() < 10) {
    Time += "0";
  }
  Time += String(getMinute());

  if (showSeconds) {
    Time += ":";
    if (getSecond() < 10) {
      Time += "0";
    }
    Time += String(getSecond());
  }

  Time += " ";// space at start and end to erase previous number properly
  return Time;
}
/*
bool updateTimeFromNTP() {
  if (!NTPUpdate()) {
    return false;
  }
  else {
    DS3231_get(&Time);
    Time.hour = NTPhours;
    Time.min = NTPminutes;
    Time.sec = NTPseconds;

    DS3231_set(Time);
  }
}*/




int encrypt1(int input) {
  input = input * (input + 1);
  input = input * 2 / 3;
  return input;
}


bool removeFromFile(String path, String start, String end){
  SD.remove(String("/MPOS/S/") + "TEMP.MRT");
  File file;
  if (openFile(file, path, FILE_READ)){
    File tempFile;
    openFile(tempFile, "S/TEMP.MRT", FILE_WRITE);
    
    String search1 = "";
    String search2 = "";
    bool copying = true;
    while (file.available()){
      search1 = search2;
      search2 = "";
      for (byte i=0; i<20 and file.available(); i++){
        search2 += char(file.read());
      }
      
      if ((search1 + search2).indexOf(start) >= 0){
        sample_RAM();
        copying = false;
        tempFile.print((search1 + search2).substring(0, (search1 + search2).indexOf(start)));
        search2 = (search1 + search2).substring((search1 + search2).indexOf(start) + start.length());
        search1 = "";
      }
      
      if ((search1 + search2).indexOf(end) >= 0){
        sample_RAM();
        copying = true;
        search2 = (search1 + search2).substring((search1 + search2).indexOf(end) + end.length());
        search1 = "";
      }
    
      if (copying){
        tempFile.print(search1);
      }
    }
    if (copying){
      tempFile.print(search2);
    }
    
    closeFile(file);
    closeFile(tempFile);
    SD.remove(path);
    openFile(file, path, FILE_WRITE);
    openFile(tempFile, "S/TEMP.MRT", FILE_READ);
    
    while (tempFile.available()){
      String section = "";
      for (byte i=0; i<20; i++){
        if (tempFile.available()){
          section += char(tempFile.read());
        }
      }
      file.print(section);
    }
    sample_RAM();
    closeFile(file);
    closeFile(tempFile);

    return true;
  }
  return false;
}








void showMCI(String LABEL, String location, int startX, int startY, int scaleX, int scaleY, bool LOG = true) {
  // works with both .mci and .mi2
  // .mi2 loads much faster
  // .mci only for backwards compatibility
  location.toUpperCase();
  File graphFile;

  if (LOG) {
    openFile(graphFile, "S/SCREEN.MLI", FILE_WRITE);
    //addFileToList(&graphFile);
    graphFile.seek(graphFile.size());

    graphFile.print(CONTROLLING_APP + "_" + LABEL + String(":MCI,"));
    graphFile.print(location);
    graphFile.print(",");
    graphFile.print(startX);
    graphFile.print(",");
    graphFile.print(startY);
    graphFile.print(",");
    graphFile.print(scaleX);
    graphFile.print(",");
    graphFile.print(scaleY);
    graphFile.print(",\n");
    closeFile(graphFile);

    delay(10);
  }

  openFile(graphFile, location, FILE_READ);
  graphFile.seek(0);
  int imageWidth = graphFile.readStringUntil(',').toInt();
  int imageHeight = graphFile.readStringUntil(',').toInt();


  int currentWidth = 0;
  int currentHeight = 0;
  while (currentHeight < imageHeight) {
    while (currentWidth < imageWidth) {
      byte r;
      byte g;
      byte b;

      if (location.endsWith(".MCI")){
        String rS = graphFile.readStringUntil(',');
        String gS = graphFile.readStringUntil(',');
        String bS = graphFile.readStringUntil(',');
        rS.replace("\n", "");
        gS.replace("\n", "");
        bS.replace("\n", "");
        r = rS.toInt();
        g = gS.toInt();
        b = bS.toInt();
      }
      else if (location.endsWith(".MI2")){
        r = graphFile.read();
        g = graphFile.read();
        b = graphFile.read();
      }
      setColor(r, g, b);

      if (r != 1 or g != 1 or b != 1) { //  leave transparent if rgb = 1,1,1 (don't draw the pixel)
        screen.fillRect(currentWidth * scaleX + startX, currentHeight * scaleY + startY, currentWidth * scaleX + startX + scaleX, currentHeight * scaleY + startY + scaleY);
      }
      currentWidth += 1;
    }
    currentHeight += 1;
    currentWidth = 0;
  }
  sample_RAM();
  closeFile(graphFile);
}


void showBIM(String LABEL, String location, int startX, int startY, int scaleX, int scaleY, byte backR, byte backG, byte backB, byte frontR, byte frontG, byte frontB, bool LOG = true) {
  File graphFile;

  if (LOG) {
    openFile(graphFile, "S/SCREEN.MLI", FILE_WRITE);
    //addFileToList(&graphFile);
    graphFile.seek(graphFile.size());

    graphFile.print(CONTROLLING_APP + "_" + LABEL + ":BIM,");
    graphFile.print(location);
    graphFile.print(",");
    graphFile.print(startX);
    graphFile.print(",");
    graphFile.print(startY);
    graphFile.print(",");
    graphFile.print(scaleX);
    graphFile.print(",");
    graphFile.print(scaleY);
    graphFile.print(",");
    graphFile.print(backR);
    graphFile.print(",");
    graphFile.print(backG);
    graphFile.print(",");
    graphFile.print(backB);
    graphFile.print(",");
    graphFile.print(frontR);
    graphFile.print(",");
    graphFile.print(frontG);
    graphFile.print(",");
    graphFile.print(frontB);
    graphFile.print(",\n");
    closeFile(graphFile);

    delay(10);
  }


  openFile(graphFile, location, FILE_READ);
  graphFile.seek(0);
  int imageWidth = graphFile.readStringUntil(',').toInt();
  int imageHeight = graphFile.readStringUntil(',').toInt();


  int currentWidth = 0;
  int currentHeight = 0;
  while (currentHeight < imageHeight) {
    while (currentWidth < imageWidth) {
      String colorS = graphFile.readStringUntil(',');
      colorS.replace("\n", "");
      byte color = colorS.toInt();

      if (color == 0) {
        setColor(backR, backG, backB);
      }
      else {
        setColor(frontR, frontG, frontB);
      }


      screen.fillRect(currentWidth * scaleX + startX, currentHeight * scaleY + startY, currentWidth * scaleX + startX + scaleX, currentHeight * scaleY + startY + scaleY);
      currentWidth += 1;
    }
    currentHeight += 1;
    currentWidth = 0;
  }
  sample_RAM();
  closeFile(graphFile);
}


byte background_r = 0;
byte background_g = 0;
byte background_b = 0;

void fillScr(byte r, byte g, byte b) {
  keyboardVisible = false;
  background_r = r;
  background_g = g;
  background_b = b;

  if (invertColor) {
    r = 255 - r;
    g = 255 - g;
    b = 255 - b;
  }
  
  if (darkMode) {
    int offset = 2 * ((r + g + b) - 384) / 3;
    
    if (r - offset < 0) { r = 0; }
    else if (r - offset > 255) { r = 255; }
    else{ r -= offset; }
    
    if (g - offset < 0) { g = 0; }
    else if (g - offset > 255) { g = 255; }
    else{ g -= offset; }
    
    if (b - offset < 0) { b = 0; }
    else if (b - offset > 255) { b = 255; }
    else{ b -= offset; }
  }

  r = r * brightnessPercent / 100;
  g = g * brightnessPercent / 100;
  b = b * brightnessPercent / 100;

  if (blueFilter) {
    b = 0;
  }

  screen.fillScr(r, g, b);
}

void setColor(byte r, byte g, byte b) {

  if (invertColor) {
    r = 255 - r;
    g = 255 - g;
    b = 255 - b;
  }
  
  if (darkMode) {
    int offset = 2 * ((r + g + b) - 384) / 3;
    
    if (r - offset < 0) { r = 0; }
    else if (r - offset > 255) { r = 255; }
    else{ r -= offset; }
    
    if (g - offset < 0) { g = 0; }
    else if (g - offset > 255) { g = 255; }
    else{ g -= offset; }
    
    if (b - offset < 0) { b = 0; }
    else if (b - offset > 255) { b = 255; }
    else{ b -= offset; }
  }

  r = r * brightnessPercent / 100;
  g = g * brightnessPercent / 100;
  b = b * brightnessPercent / 100;

  if (blueFilter) {
    b = 0;
  }

  screen.setColor(r, g, b);
}

void setBackColor(byte r, byte g, byte b) {

  if (invertColor) {
    r = 255 - r;
    g = 255 - g;
    b = 255 - b;
  }
  
  if (darkMode) {
    int offset = 2 * ((r + g + b) - 384) / 3;
    
    if (r - offset < 0) { r = 0; }
    else if (r - offset > 255) { r = 255; }
    else{ r -= offset; }
    
    if (g - offset < 0) { g = 0; }
    else if (g - offset > 255) { g = 255; }
    else{ g -= offset; }
    
    if (b - offset < 0) { b = 0; }
    else if (b - offset > 255) { b = 255; }
    else{ b -= offset; }
  }

  r = r * brightnessPercent / 100;
  g = g * brightnessPercent / 100;
  b = b * brightnessPercent / 100;

  if (blueFilter) {
    b = 0;
  }

  screen.setBackColor(r, g, b);
}


void dumpScreen() {
  Serial.print("\n\n");
  Serial.print(F("Start of screen.mli\n"));
  File graphFile;
  openFile(graphFile, "S/SCREEN.MLI", FILE_READ);
  Serial.print(graphFile.readString());
  closeFile(graphFile);
  Serial.print(F("end of screen.mli\n"));
  Serial.print("\n\n");

  delay(10);
}





void addShape(String LABEL, String shape) {
  File graphFile;
  openFile(graphFile, "S/SCREEN.MLI", FILE_WRITE);
  graphFile.seek(graphFile.size());

  graphFile.print(CONTROLLING_APP + "_" + LABEL + ":" + shape + '\n');
  closeFile(graphFile);

  delay(10);
}

void drawPixel(String LABEL, byte r, byte g, byte b, int x, int y) {
  File graphFile;
  openFile(graphFile, "S/SCREEN.MLI", FILE_WRITE);
  graphFile.seek(graphFile.size());

  graphFile.print(CONTROLLING_APP + "_" + LABEL + ":PIXEL,");
  graphFile.print(r);
  graphFile.print(",");
  graphFile.print(g);
  graphFile.print(",");
  graphFile.print(b);
  graphFile.print(",");

  graphFile.print(x);
  graphFile.print(",");
  graphFile.print(y);
  graphFile.print(",\n");
  closeFile(graphFile);
  setColor(r, g, b);
  screen.drawPixel(x, y);

  delay(10);
}

void drawLine(String LABEL, byte r, byte g, byte b, int x1, int y1, int x2, int y2) {
  File graphFile;
  openFile(graphFile, "S/SCREEN.MLI", FILE_WRITE);
  graphFile.seek(graphFile.size());

  graphFile.print(CONTROLLING_APP + "_" + LABEL + ":LINE,");
  graphFile.print(r);
  graphFile.print(",");
  graphFile.print(g);
  graphFile.print(",");
  graphFile.print(b);
  graphFile.print(",");

  graphFile.print(x1);
  graphFile.print(",");
  graphFile.print(y1);
  graphFile.print(",");
  graphFile.print(x2);
  graphFile.print(",");
  graphFile.print(y2);
  graphFile.print(",\n");
  closeFile(graphFile);
  setColor(r, g, b);
  screen.drawLine(x1, y1, x2, y2);

  delay(10);
}

void drawRect(String LABEL, byte r, byte g, byte b, int x1, int y1, int x2, int y2) {
  File graphFile;
  openFile(graphFile, "S/SCREEN.MLI", FILE_WRITE);
  graphFile.seek(graphFile.size());

  graphFile.print(CONTROLLING_APP + "_" + LABEL + ":RECT,");
  graphFile.print(r);
  graphFile.print(",");
  graphFile.print(g);
  graphFile.print(",");
  graphFile.print(b);
  graphFile.print(",");

  graphFile.print(x1);
  graphFile.print(",");
  graphFile.print(y1);
  graphFile.print(",");
  graphFile.print(x2);
  graphFile.print(",");
  graphFile.print(y2);
  graphFile.print(",\n");
  closeFile(graphFile);
  setColor(r, g, b);
  screen.drawRect(x1, y1, x2, y2);

  delay(10);
}

void drawRoundRect(String LABEL, byte r, byte g, byte b, int x1, int y1, int x2, int y2) {
  File graphFile;
  openFile(graphFile, "S/SCREEN.MLI", FILE_WRITE);
  graphFile.seek(graphFile.size());

  graphFile.print(CONTROLLING_APP + "_" + LABEL + ":ROUNDRECT,");
  graphFile.print(r);
  graphFile.print(",");
  graphFile.print(g);
  graphFile.print(",");
  graphFile.print(b);
  graphFile.print(",");

  graphFile.print(x1);
  graphFile.print(",");
  graphFile.print(y1);
  graphFile.print(",");
  graphFile.print(x2);
  graphFile.print(",");
  graphFile.print(y2);
  graphFile.print(",\n");
  closeFile(graphFile);
  setColor(r, g, b);
  screen.drawRoundRect(x1, y1, x2, y2);

  delay(10);
}

void fillRect(String LABEL, byte r, byte g, byte b, int x1, int y1, int x2, int y2) {
  File graphFile;
  openFile(graphFile, "S/SCREEN.MLI", FILE_WRITE);
  graphFile.seek(graphFile.size());

  graphFile.print(CONTROLLING_APP + "_" + LABEL + ":FILLRECT,");
  graphFile.print(r);
  graphFile.print(",");
  graphFile.print(g);
  graphFile.print(",");
  graphFile.print(b);
  graphFile.print(",");

  graphFile.print(x1);
  graphFile.print(",");
  graphFile.print(y1);
  graphFile.print(",");
  graphFile.print(x2);
  graphFile.print(",");
  graphFile.print(y2);
  graphFile.print(",\n");
  closeFile(graphFile);
  setColor(r, g, b);
  screen.fillRect(x1, y1, x2, y2);

  delay(10);
}

void fillRoundRect(String LABEL, byte r, byte g, byte b, int x1, int y1, int x2, int y2) {
  File graphFile;
  openFile(graphFile, "S/SCREEN.MLI", FILE_WRITE);
  graphFile.seek(graphFile.size());

  graphFile.print(CONTROLLING_APP + "_" + LABEL + ":FILLROUNDRECT,");
  graphFile.print(r);
  graphFile.print(",");
  graphFile.print(g);
  graphFile.print(",");
  graphFile.print(b);
  graphFile.print(",");

  graphFile.print(x1);
  graphFile.print(",");
  graphFile.print(y1);
  graphFile.print(",");
  graphFile.print(x2);
  graphFile.print(",");
  graphFile.print(y2);
  graphFile.print(",\n");
  closeFile(graphFile);
  setColor(r, g, b);
  screen.fillRoundRect(x1, y1, x2, y2);

  delay(10);
}

void drawCircle(String LABEL, byte r, byte g, byte b, int x, int y, int radius) {
  File graphFile;
  openFile(graphFile, "S/SCREEN.MLI", FILE_WRITE);
  graphFile.seek(graphFile.size());

  graphFile.print(CONTROLLING_APP + "_" + LABEL + ":CIRCLE,");
  graphFile.print(r);
  graphFile.print(",");
  graphFile.print(g);
  graphFile.print(",");
  graphFile.print(b);
  graphFile.print(",");

  graphFile.print(x);
  graphFile.print(",");
  graphFile.print(y);
  graphFile.print(",");
  graphFile.print(radius);
  graphFile.print(",\n");
  closeFile(graphFile);
  setColor(r, g, b);
  screen.drawCircle(x, y, radius);

  delay(10);
}

void fillCircle(String LABEL, byte r, byte g, byte b, int x, int y, int radius) {
  File graphFile;
  openFile(graphFile, "S/SCREEN.MLI", FILE_WRITE);
  graphFile.seek(graphFile.size());

  graphFile.print(CONTROLLING_APP + "_" + LABEL + ":FILLCIRCLE,");
  graphFile.print(r);
  graphFile.print(",");
  graphFile.print(g);
  graphFile.print(",");
  graphFile.print(b);
  graphFile.print(",");

  graphFile.print(x);
  graphFile.print(",");
  graphFile.print(y);
  graphFile.print(",");
  graphFile.print(radius);
  graphFile.print(",\n");
  closeFile(graphFile);
  setColor(r, g, b);
  screen.fillCircle(x, y, radius);

  delay(10);
}

void print(String LABEL, byte r, byte g, byte b, byte b_r, byte b_g, byte b_b, int x, int y, int rotation, String fontName, String text) {
  String file_text = text;
  file_text.replace(",", "<fins-comma>");
  String shape = CONTROLLING_APP + "_" + LABEL + ":PRINT," + r + "," + g + "," + b + ",";
  shape += b_r;
  shape += ",";
  shape += b_g;
  shape += ",";
  shape += b_b;
  shape += ",";
  shape += x;
  shape += ",";
  shape += y;
  shape += ",";
  shape += rotation;

  shape += "," + fontName + "," + file_text + ",";


  File graphFile;
  openFile(graphFile, "S/SCREEN.MLI", FILE_WRITE);
  graphFile.seek(graphFile.size());
  graphFile.print(shape + '\n');
  closeFile(graphFile);

  setColor(r, g, b);
  setBackColor(b_r, b_g, b_b);
  setFont(fontName);
  screen.print(text, x, y, rotation);

  delay(10);
}

void drawGraph(String label, int startX, int startY, unsigned int endX, unsigned int endY, int numPoints, int startRangeY, int endRangeY, String path){
  fillRoundRect(label, 255, 255, 255, startX, startY, endX, endY);
  drawRoundRect(label, 70, 70, 70, startX, startY, endX, endY);
  File file;
  if (openFile(file, path, FILE_READ)){
    int lastPoint = 0;
    int nextPoint = 0;
    unsigned int count = 0;
    while (file.available() and count < numPoints){
      lastPoint = nextPoint;
      nextPoint = file.readStringUntil('\n').toInt();

      if (nextPoint > endRangeY){
        nextPoint = endRangeY;
      }
      if (nextPoint < startRangeY){
        nextPoint = startRangeY;
      }

      if (count > 0){
        drawLine(label, 0, 0, 150, map(numPoints-count, 1, numPoints, startX, endX), map(nextPoint, startRangeY, endRangeY, endY, startY), map(numPoints-count+1, 1, numPoints, startX, endX), map(lastPoint, startRangeY, endRangeY, endY, startY));
      }
      count += 1;
    }

    closeFile(file);
  }
  else{
    print(label, 0, 0, 0, 255, 255, 255, startX, startY, 0, "small", "no data");
  }
}

void scr_removeLayer(String LABEL) {
  LABEL = CONTROLLING_APP + "_" + LABEL;
  fileRemoveLineStartingWith(String("/MPOS/S/") + "screen.mli", LABEL);
}

void scr_removeApp(String app) {
  if (CONTROLLING_APP == "SYS") {
    fileRemoveLineStartingWith(String("/MPOS/S/") + "screen.mli", app + "_");
  }
}

void fileInsertStart(String path, String insert, unsigned int numLoops, byte linesPerLoop){
  File file;
  if (openFile(file, path, FILE_READ)){
    SD.remove(String("/MPOS/S/") + "TEMP.MRT");
    File tempFile;
    openFile(tempFile, "S/TEMP.MRT", FILE_WRITE);
    for (byte i=0; i<numLoops; i++){
      String dataFragment = "";
      for (byte sample=0; sample<linesPerLoop; sample++){
        if (file.available()){
          dataFragment += file.readStringUntil('\n') + '\n';
        }
      }

      if (dataFragment != ""){
        tempFile.print(dataFragment);
      }
    }
    sample_RAM();
    closeFile(file);
    closeFile(tempFile);
  
    SD.remove(path);
    openFile(tempFile, "S/TEMP.MRT", FILE_READ);
    openFile(file, path, FILE_WRITE);

    file.print(insert);
    for (byte i=0; i<numLoops; i++){
      String dataFragment = "";
      for (byte sample=0; sample<linesPerLoop; sample++){
        if (tempFile.available()){
          dataFragment += tempFile.readStringUntil('\n') + '\n';
        }
      }
  
      if (dataFragment != ""){
        file.print(dataFragment);
      }
    }
    closeFile(file);
    closeFile(tempFile);
  }
  else{
    openFile(file, path, FILE_WRITE);
    file.print(insert);
    closeFile(file);
  }
}

void fileRemoveLineStartingWith(String path, String startToRemove) {

  SD.remove(String("/MPOS/S/") + "T_DEL.MRT");

  File file;
  openFile(file, path, FILE_READ);
  
  if (!file) {
    closeFile(file);
    return;
  }
  if (!file.available()) {
    closeFile(file);
    return;
  }

  File temporaryFile;
  openFile(temporaryFile, "S/T_DEL.MRT", FILE_WRITE);

  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (!line.startsWith(startToRemove)) {
      temporaryFile.print(line + '\n');
    }
  }
  closeFile(file);
  closeFile(temporaryFile);

  // copy new version from temporary file to permanent file
  SD.remove(path);
  openFile(temporaryFile, "S/T_DEL.MRT", FILE_READ);
  openFile(file, path, FILE_WRITE);

  while (temporaryFile.available()) {
    String line = temporaryFile.readStringUntil('\n');
    file.print(line + '\n');
    sample_RAM();
  }

  closeFile(file);
  closeFile(temporaryFile);
  SD.remove(String("/MPOS/S/") + "T_DEL.MRT");
}



void showMLI(String LABEL, String path, int startX, int startY, int scaleX, int scaleY, bool record = true) {
  File graphFile;
  if (record) {
    openFile(graphFile, "S/SCREEN.MLI", FILE_WRITE);
    graphFile.seek(graphFile.size());
    graphFile.print(CONTROLLING_APP + "_" + LABEL + ":MLI,");
    graphFile.print(path);
    graphFile.print(",");
    graphFile.print(startX);
    graphFile.print(",");
    graphFile.print(startY);
    graphFile.print(",");
    graphFile.print(scaleX);
    graphFile.print(",");
    graphFile.print(scaleY);
    graphFile.print(",\n");
    closeFile(graphFile);
  }

  openFile(graphFile, path, FILE_READ);

  while (graphFile.available()) {
    graphFile.readStringUntil(':');
    String shape = graphFile.readStringUntil(',');

    if (shape == "FONT") {
      String font = graphFile.readStringUntil(',');
    }

    if (shape == "PIXEL") {
      byte r = graphFile.readStringUntil(',').toInt();
      byte g = graphFile.readStringUntil(',').toInt();
      byte b = graphFile.readStringUntil(',').toInt();
      int x = graphFile.readStringUntil(',').toInt();
      int y = graphFile.readStringUntil(',').toInt();
      setColor(r, g, b);
      screen.drawPixel(x + startX, y + startY);
      sample_RAM();
    }

    if (shape == "LINE") {
      byte r = graphFile.readStringUntil(',').toInt();
      byte g = graphFile.readStringUntil(',').toInt();
      byte b = graphFile.readStringUntil(',').toInt();
      int x1 = graphFile.readStringUntil(',').toInt();
      int y1 = graphFile.readStringUntil(',').toInt();
      int x2 = graphFile.readStringUntil(',').toInt();
      int y2 = graphFile.readStringUntil(',').toInt();
      setColor(r, g, b);
      screen.drawLine(x1 + startX, y1 + startY, x2 + startX, y2 + startY);
      sample_RAM();
    }

    if (shape == "RECT") {
      byte r = graphFile.readStringUntil(',').toInt();
      byte g = graphFile.readStringUntil(',').toInt();
      byte b = graphFile.readStringUntil(',').toInt();
      int x1 = graphFile.readStringUntil(',').toInt();
      int y1 = graphFile.readStringUntil(',').toInt();
      int x2 = graphFile.readStringUntil(',').toInt();
      int y2 = graphFile.readStringUntil(',').toInt();
      setColor(r, g, b);
      screen.drawRect(x1 + startX, y1 + startY, x2 + startX, y2 + startY);
      sample_RAM();
    }

    if (shape == "ROUNDRECT") {
      byte r = graphFile.readStringUntil(',').toInt();
      byte g = graphFile.readStringUntil(',').toInt();
      byte b = graphFile.readStringUntil(',').toInt();
      int x1 = graphFile.readStringUntil(',').toInt();
      int y1 = graphFile.readStringUntil(',').toInt();
      int x2 = graphFile.readStringUntil(',').toInt();
      int y2 = graphFile.readStringUntil(',').toInt();
      setColor(r, g, b);
      screen.drawRoundRect(x1 + startX, y1 + startY, x2 + startX, y2 + startY);
      sample_RAM();
    }

    if (shape == "FILLRECT") {
      byte r = graphFile.readStringUntil(',').toInt();
      byte g = graphFile.readStringUntil(',').toInt();
      byte b = graphFile.readStringUntil(',').toInt();
      int x1 = graphFile.readStringUntil(',').toInt();
      int y1 = graphFile.readStringUntil(',').toInt();
      int x2 = graphFile.readStringUntil(',').toInt();
      int y2 = graphFile.readStringUntil(',').toInt();
      setColor(r, g, b);
      screen.fillRect(x1 + startX, y1 + startY, x2 + startX, y2 + startY);
      sample_RAM();
    }

    if (shape == "FILLROUNDRECT") {
      byte r = graphFile.readStringUntil(',').toInt();
      byte g = graphFile.readStringUntil(',').toInt();
      byte b = graphFile.readStringUntil(',').toInt();
      int x1 = graphFile.readStringUntil(',').toInt();
      int y1 = graphFile.readStringUntil(',').toInt();
      int x2 = graphFile.readStringUntil(',').toInt();
      int y2 = graphFile.readStringUntil(',').toInt();
      setColor(r, g, b);
      screen.fillRoundRect(x1 + startX, y1 + startY, x2 + startX, y2 + startY);
      sample_RAM();
    }

    if (shape == "CIRCLE") {
      byte r = graphFile.readStringUntil(',').toInt();
      byte g = graphFile.readStringUntil(',').toInt();
      byte b = graphFile.readStringUntil(',').toInt();
      int x = graphFile.readStringUntil(',').toInt();
      int y = graphFile.readStringUntil(',').toInt();
      int rotation = graphFile.readStringUntil(',').toInt();
      setColor(r, g, b);
      screen.drawCircle(x + startX, y + startY, rotation);
      sample_RAM();
    }

    if (shape == "FILLCIRCLE") {
      byte r = graphFile.readStringUntil(',').toInt();
      byte g = graphFile.readStringUntil(',').toInt();
      byte b = graphFile.readStringUntil(',').toInt();
      int x = graphFile.readStringUntil(',').toInt();
      int y = graphFile.readStringUntil(',').toInt();
      int rotation = graphFile.readStringUntil(',').toInt();
      setColor(r, g, b);
      screen.fillCircle(x + startX, y + startY, rotation);
      sample_RAM();
    }

    if (shape == "PRINT") {
      byte r = graphFile.readStringUntil(',').toInt();
      byte g = graphFile.readStringUntil(',').toInt();
      byte b = graphFile.readStringUntil(',').toInt();
      byte b_r = graphFile.readStringUntil(',').toInt();
      byte b_g = graphFile.readStringUntil(',').toInt();
      byte b_b = graphFile.readStringUntil(',').toInt();
      int x = graphFile.readStringUntil(',').toInt();
      int y = graphFile.readStringUntil(',').toInt();
      int rotation = graphFile.readStringUntil(',').toInt();
      String font = graphFile.readStringUntil(',');
      String text = graphFile.readStringUntil(',');
      text.replace("<fins-comma>", ",");
      setFont(font);
      setColor(r, g, b);
      setBackColor(b_r, b_g, b_b);
      screen.print(text, x + startX, y + startY, rotation);
      sample_RAM();
    }

    if (shape == "MCI") {
      String path = graphFile.readStringUntil(',');
      int startX = graphFile.readStringUntil(',').toInt();
      int startY = graphFile.readStringUntil(',').toInt();
      byte scaleX = graphFile.readStringUntil(',').toInt();
      byte scaleY = graphFile.readStringUntil(',').toInt();
      showMCI("", path, startX, startY, scaleX, scaleY, false);
    }

    if (shape == "BIM") {
      String path = graphFile.readStringUntil(',');
      int startX = graphFile.readStringUntil(',').toInt();
      int startY = graphFile.readStringUntil(',').toInt();
      byte scaleX = graphFile.readStringUntil(',').toInt();
      byte scaleY = graphFile.readStringUntil(',').toInt();
      byte backR = graphFile.readStringUntil(',').toInt();
      byte backG = graphFile.readStringUntil(',').toInt();
      byte backB = graphFile.readStringUntil(',').toInt();
      byte frontR = graphFile.readStringUntil(',').toInt();
      byte frontG = graphFile.readStringUntil(',').toInt();
      byte frontB = graphFile.readStringUntil(',').toInt();
      showBIM("", path, startX, startY, scaleX, scaleY, backR, backG, backB, frontR, frontG, frontB, false);
    }

    if (shape == "MLI") {
      String path = graphFile.readStringUntil(',');
      int startX = graphFile.readStringUntil(',').toInt();
      int startY = graphFile.readStringUntil(',').toInt();
      byte scaleX = graphFile.readStringUntil(',').toInt();
      byte scaleY = graphFile.readStringUntil(',').toInt();
      showMLI("", path, startX, startY, scaleX, scaleY, false);
      sample_RAM();
    }
  }
}






void refreshScreen(bool sysOnly = false) {
  bool displayKeyboardThisRefresh = true;
  if (!sysOnly) {
    fillScr(background_r, background_g, background_b);
    displayKeyboardThisRefresh = false;
  }

  String prevControl = CONTROLLING_APP;
  CONTROLLING_APP = "SYS";
  File graphFile;
  openFile(graphFile, "S/SCREEN.MLI", FILE_READ);
  CONTROLLING_APP = prevControl;

  //addFileToList(&graphFile);
  graphFile.seek(0);

  while (graphFile.available()) {
    if (graphFile.readStringUntil('_') == "SYS" or sysOnly == false) {
      graphFile.readStringUntil(':');
      String shape = graphFile.readStringUntil(',');

      if (shape == "FONT") {
        String font = graphFile.readStringUntil(',');
        //setFont("", font, false);
      }

      if (shape == "PIXEL") {
        byte r = graphFile.readStringUntil(',').toInt();
        byte g = graphFile.readStringUntil(',').toInt();
        byte b = graphFile.readStringUntil(',').toInt();
        int x = graphFile.readStringUntil(',').toInt();
        int y = graphFile.readStringUntil(',').toInt();
        setColor(r, g, b);
        screen.drawPixel(x, y);
        sample_RAM();
      }

      if (shape == "LINE") {
        byte r = graphFile.readStringUntil(',').toInt();
        byte g = graphFile.readStringUntil(',').toInt();
        byte b = graphFile.readStringUntil(',').toInt();
        int x1 = graphFile.readStringUntil(',').toInt();
        int y1 = graphFile.readStringUntil(',').toInt();
        int x2 = graphFile.readStringUntil(',').toInt();
        int y2 = graphFile.readStringUntil(',').toInt();
        setColor(r, g, b);
        screen.drawLine(x1, y1, x2, y2);
        sample_RAM();
      }

      if (shape == "RECT") {
        byte r = graphFile.readStringUntil(',').toInt();
        byte g = graphFile.readStringUntil(',').toInt();
        byte b = graphFile.readStringUntil(',').toInt();
        int x1 = graphFile.readStringUntil(',').toInt();
        int y1 = graphFile.readStringUntil(',').toInt();
        int x2 = graphFile.readStringUntil(',').toInt();
        int y2 = graphFile.readStringUntil(',').toInt();
        setColor(r, g, b);
        screen.drawRect(x1, y1, x2, y2);
        sample_RAM();
      }

      if (shape == "ROUNDRECT") {
        byte r = graphFile.readStringUntil(',').toInt();
        byte g = graphFile.readStringUntil(',').toInt();
        byte b = graphFile.readStringUntil(',').toInt();
        int x1 = graphFile.readStringUntil(',').toInt();
        int y1 = graphFile.readStringUntil(',').toInt();
        int x2 = graphFile.readStringUntil(',').toInt();
        int y2 = graphFile.readStringUntil(',').toInt();
        setColor(r, g, b);
        screen.drawRoundRect(x1, y1, x2, y2);
        sample_RAM();
      }

      if (shape == "FILLRECT") {
        byte r = graphFile.readStringUntil(',').toInt();
        byte g = graphFile.readStringUntil(',').toInt();
        byte b = graphFile.readStringUntil(',').toInt();
        int x1 = graphFile.readStringUntil(',').toInt();
        int y1 = graphFile.readStringUntil(',').toInt();
        int x2 = graphFile.readStringUntil(',').toInt();
        int y2 = graphFile.readStringUntil(',').toInt();
        setColor(r, g, b);
        screen.fillRect(x1, y1, x2, y2);
        sample_RAM();
      }

      if (shape == "FILLROUNDRECT") {
        byte r = graphFile.readStringUntil(',').toInt();
        byte g = graphFile.readStringUntil(',').toInt();
        byte b = graphFile.readStringUntil(',').toInt();
        int x1 = graphFile.readStringUntil(',').toInt();
        int y1 = graphFile.readStringUntil(',').toInt();
        int x2 = graphFile.readStringUntil(',').toInt();
        int y2 = graphFile.readStringUntil(',').toInt();
        setColor(r, g, b);
        screen.fillRoundRect(x1, y1, x2, y2);
        sample_RAM();
      }

      if (shape == "CIRCLE") {
        byte r = graphFile.readStringUntil(',').toInt();
        byte g = graphFile.readStringUntil(',').toInt();
        byte b = graphFile.readStringUntil(',').toInt();
        int x = graphFile.readStringUntil(',').toInt();
        int y = graphFile.readStringUntil(',').toInt();
        int rotation = graphFile.readStringUntil(',').toInt();
        setColor(r, g, b);
        screen.drawCircle(x, y, rotation);
        sample_RAM();
      }

      if (shape == "FILLCIRCLE") {
        byte r = graphFile.readStringUntil(',').toInt();
        byte g = graphFile.readStringUntil(',').toInt();
        byte b = graphFile.readStringUntil(',').toInt();
        int x = graphFile.readStringUntil(',').toInt();
        int y = graphFile.readStringUntil(',').toInt();
        int rotation = graphFile.readStringUntil(',').toInt();
        setColor(r, g, b);
        screen.fillCircle(x, y, rotation);
        sample_RAM();
      }

      if (shape == "PRINT") {
        byte r = graphFile.readStringUntil(',').toInt();
        byte g = graphFile.readStringUntil(',').toInt();
        byte b = graphFile.readStringUntil(',').toInt();
        byte b_r = graphFile.readStringUntil(',').toInt();
        byte b_g = graphFile.readStringUntil(',').toInt();
        byte b_b = graphFile.readStringUntil(',').toInt();
        int x = graphFile.readStringUntil(',').toInt();
        int y = graphFile.readStringUntil(',').toInt();
        int rotation = graphFile.readStringUntil(',').toInt();
        String font = graphFile.readStringUntil(',');
        String text = graphFile.readStringUntil(',');
        text.replace("<fins-comma>", ",");
        setFont(font);
        setColor(r, g, b);
        setBackColor(b_r, b_g, b_b);
        screen.print(text, x, y, rotation);
        sample_RAM();
      }

      if (shape == "MCI") {
        String path = graphFile.readStringUntil(',');
        int startX = graphFile.readStringUntil(',').toInt();
        int startY = graphFile.readStringUntil(',').toInt();
        byte scaleX = graphFile.readStringUntil(',').toInt();
        byte scaleY = graphFile.readStringUntil(',').toInt();
        showMCI("", path, startX, startY, scaleX, scaleY, false);
      }

      if (shape == "BIM") {
        String path = graphFile.readStringUntil(',');
        int startX = graphFile.readStringUntil(',').toInt();
        int startY = graphFile.readStringUntil(',').toInt();
        byte scaleX = graphFile.readStringUntil(',').toInt();
        byte scaleY = graphFile.readStringUntil(',').toInt();
        byte backR = graphFile.readStringUntil(',').toInt();
        byte backG = graphFile.readStringUntil(',').toInt();
        byte backB = graphFile.readStringUntil(',').toInt();
        byte frontR = graphFile.readStringUntil(',').toInt();
        byte frontG = graphFile.readStringUntil(',').toInt();
        byte frontB = graphFile.readStringUntil(',').toInt();
        showBIM("", path, startX, startY, scaleX, scaleY, backR, backG, backB, frontR, frontG, frontB, false);
      }

      if (shape == "MLI") {
        String path = graphFile.readStringUntil(',');
        int startX = graphFile.readStringUntil(',').toInt();
        int startY = graphFile.readStringUntil(',').toInt();
        byte scaleX = graphFile.readStringUntil(',').toInt();
        byte scaleY = graphFile.readStringUntil(',').toInt();
        showMLI("", path, startX, startY, scaleX, scaleY, false);
      }
    }

    graphFile.readStringUntil('\n');

    if (!graphFile.available() and !sysOnly) { // make 2 passes through the file
      sysOnly = true;                         // to make sure 'SYS' objects are on top of others
      graphFile.seek(0);

      if (keyboardVisible) { // show keyboard over app graphics, but under other system graphics
        show_keyboard();
      }
    }

  }
  closeFile(graphFile);
}


void on_off_input(String LABEL, int x, int y, bool state) {
  String path = String("/MPOS/S/") + "D/R/SWI-OFF.MLI";
  if (state == true) {
    path = String("/MPOS/S/") + "D/R/SWI-ON.MLI";
  }

  showMLI(LABEL, path, x, y, 1, 1);
}




void initScreen(int orientation) {
  screenOrientation = orientation;
  screen.InitLCD(orientation);
  touch.InitTouch(orientation);
  //if (orientation == PORTRAIT) {
  //  touch.InitTouch(LANDSCAPE);
  //}
  //else {
  //  touch.InitTouch(PORTRAIT);
  //}
}





// bluetooth MAC ADRESS: 14:3:6050f

bool bluetoothInAT = false;
bool bluetoothIsMaster = false;
bool bluetoothScanning = false;
String BLUETOOTH_MAC_ADDRESS = "";

String format_MAC(String unformatted) {
  String mac = "";

  String tempR = unformatted.substring(0, unformatted.indexOf(":"));
  unformatted.remove(0, unformatted.indexOf(":")+1);
  mac = "";
  while (tempR.length() < 4){
    tempR = "0" + tempR;
  }
  mac += tempR;

  tempR = unformatted.substring(0, unformatted.indexOf(":"));
  unformatted.remove(0, unformatted.indexOf(":")+1);
  while (tempR.length() < 2){
    tempR = "0" + tempR;
  }
  mac += tempR;

  while (unformatted.length() < 6){
    unformatted = "0" + unformatted;
  }
  mac += unformatted;
  mac.toUpperCase();
  return mac;
}

void bluetoothPowerOn(bool preventRecursion=false){
  digitalWrite(A11, HIGH);
  digitalWrite(A10, HIGH);
  digitalWrite(A13, HIGH);
  bluetoothActive = true;
  delay(100);

  if (BLUETOOTH_MAC_ADDRESS == "" and preventRecursion == false){ // find MAC address
    bluetooth_enter_AT();
    while (Serial1.available()){
      Serial1.read();
    }
    Serial1.print("AT+ADDR?\r\n");

    byte delays = 0;
    while (delays < 200 and !Serial1.available()){
      delays += 1;
      delay(50);
    }

    String response = Serial1.readStringUntil('\n');
    Serial1.print(F("AT+RMAAD\r\n"));
    delay(100);
    Serial1.print(F("AT+ROLE=0\r\n"));
    delay(100);
    Serial1.print(F("AT+INQM=0,10,10\r\n"));
    delay(100);
    
    response.remove(0,6);
    response.replace("\r", "");

    
    BLUETOOTH_MAC_ADDRESS = format_MAC(response);

    bluetooth_exit_AT();
  }
}
void bluetooth_power_off(){
  digitalWrite(A10, LOW);
  digitalWrite(A11, LOW);
  digitalWrite(A13, LOW);
  bluetoothActive = false;
  bluetoothScanning = false;
}

void bluetooth_enter_AT(){
  if (bluetoothInAT == false and bluetoothActive){
    bluetooth_power_off();
    delay(50);
    digitalWrite(blu_EN, HIGH);
    delay(10);
    bluetoothPowerOn(true);
    delay(800);
    digitalWrite(blu_EN, LOW);

    while (Serial1.available()){
      Serial1.read();
    }

    Serial1.end();
    Serial1.begin(38400);
    bluetoothInAT = true;
    bluetoothScanning = false;
  }
}

void bluetooth_exit_AT(){
  if (bluetoothInAT == true and bluetoothActive){
    bluetooth_power_off();
    digitalWrite(blu_EN, LOW);
    delay(100);
    bluetoothPowerOn();

    while (Serial1.available()){
      Serial1.read();
    }

    Serial1.end();
    Serial1.begin(115200);
    bluetoothInAT = false;
    bluetoothScanning = false;
  }
}

void bluetooth_set_master(){
  if (!bluetoothIsMaster and bluetoothActive){
    bluetooth_enter_AT();
    Serial1.print("AT+ROLE=1\r\n");
    Serial1.flush();
    delay(100);
    bluetoothIsMaster = true;
  }
}

void bluetooth_set_slave(){
  if (bluetoothIsMaster and bluetoothActive){
    bluetooth_enter_AT();
    Serial1.print("AT+ROLE=0\r\n");
    delay(100);
    bluetoothIsMaster = false;
  }
}

void bluetooth_scan(){
  SD.remove(String("/MPOS/S/") + "BT/NEARBY.MRT");
  if (bluetoothActive) {
    bluetooth_set_master();
    bluetooth_exit_AT();
    delay(800);
    digitalWrite(blu_EN, HIGH); // enter secondary AT mode (main mode doesn't work for INQ)
    delay(100);
    Serial1.print("AT+INQ\r\n");
    delay(100);
    //Serial1.print("AT+STATE?\r\n");
    //digitalWrite(blu_EN, LOW);
    bluetoothScanning = true;
  }
}

void bluetooth_stop_scan() {
  if (bluetoothScanning and bluetoothActive) {
    Serial.println("Stopping Scan");
    digitalWrite(A10, LOW);
    digitalWrite(A11, LOW);
    digitalWrite(A13, LOW);
    digitalWrite(blu_EN, LOW);
    delay(500);

    digitalWrite(A10, HIGH);
    digitalWrite(A11, HIGH);
    digitalWrite(A13, HIGH);
    bluetoothScanning = false;
    delay(100);
  }
}

String BTConnectedMAC = "";
unsigned long BTConnectTime = 0;
unsigned long BTLastPing = 0;
unsigned long BTLastPingSent = 0;
String BTSendBuffer = "";

void bluetooth_transmit_packet(String data, String destinationMac = ""){
  if (bluetoothActive){
    bluetooth_stop_scan();
    bluetooth_exit_AT();
    if (BTLastPing > BTConnectTime and millis() - BTLastPing < 30000) {
      Serial1.print(data);
    }
    else if (millis() - BTConnectTime < 10000) { // wait for connection to take place before sending
      BTSendBuffer += data;
    }
  }
}


void bluetooth_connect(String targetMac){
  String prev_control_app = CONTROLLING_APP;
  CONTROLLING_APP = "SYS";
  large_instant_notice("Connecting...");
  bluetooth_enter_AT();
  bluetooth_set_master();
  delay(500);
  Serial1.print("AT+BIND=");
  Serial1.print(targetMac.substring(0,4));
  Serial1.print(",");
  Serial1.print(targetMac.substring(4,6));
  Serial1.print(",");
  Serial1.print(targetMac.substring(6,12));
  Serial1.print("\r\n");
  delay(200);
  bluetooth_exit_AT();
  delay(1000);
  bluetooth_transmit_packet("CONNECT\n" + BLUETOOTH_MAC_ADDRESS, targetMac);
  BTConnectedMAC = targetMac;
  BTConnectTime = millis();
  refreshScreen();
  CONTROLLING_APP = prev_control_app;
}









unsigned long lastNotification = 0;



bool addNotification(String title, String description) {
  description.replace("\n", "");
  File file;
  openFile(file, "S/NOTIF.MRT", FILE_WRITE);
  file.print(title);
  file.print('\n');
  file.print(description);
  file.print('\n');
  sample_RAM();
  closeFile(file);
}




bool showNotification(String title, String description) {
  String prev_control_app = CONTROLLING_APP;
  CONTROLLING_APP = "SYS";
  if (millis() > lastNotification + 5000) {
    scr_removeLayer("notification");
    lastNotification = millis();
    String description2 = "";
    String description3 = "";
    String description4 = "";
    String description5 = "";
    setColor(255, 255, 255);
    if (title.length() > 26) {
      title = title.substring(0, 24) + "...";
    }

    if (description.length() > 54) {
      description2 = description.substring(54, description.length());
      description = description.substring(0, 54);
    }
    if (description2.length() > 54) {
      description3 = description2.substring(54, description2.length());
      description2 = description2.substring(0, 54);
    }
    if (description3.length() > 54) {
      description4 = description3.substring(54, description3.length());
      description3 = description3.substring(0, 54);
    }
    if (description4.length() > 54) {
      description5 = description4.substring(54, description4.length());
      description4 = description4.substring(0, 54);
    }

    if (description5.length() > 53) {
      description5 = description5.substring(0, 51) + "...";
    }

    fillRoundRect("notification", 255, 255, 255, 20, 20, screen.getDisplayXSize() - 20, 100);
    drawRoundRect("notification", 0, 0, 0, 20, 20, screen.getDisplayXSize() - 20, 100);
    print("notification", 0, 0, 0, 255, 255, 255, 23, 23, 0, "medium", title);
    print("notification", 0, 0, 0, 255, 255, 255, 23, 45, 0, "small", description);
    print("notification", 0, 0, 0, 255, 255, 255, 23, 55, 0, "small", description2);
    print("notification", 0, 0, 0, 255, 255, 255, 23, 65, 0, "small", description3);
    print("notification", 0, 0, 0, 255, 255, 255, 23, 75, 0, "small", description4);
    print("notification", 0, 0, 0, 255, 255, 255, 23, 85, 0, "small", description5);
    sample_RAM();

    addSound(0, 300, 100);
    addSound(200, 300, 100);

    CONTROLLING_APP = prev_control_app;
    return true;
  }
  else {
    CONTROLLING_APP = prev_control_app;
    return false;// couldn't show notification
  }
}



bool confirmation_message(String msg1, String msg2, String msg3) {
  setColor(255, 20, 20);
  screen.fillRoundRect(50, 100, screen.getDisplayXSize() - 50, 370);
  setColor(255, 255, 255);
  screen.drawRoundRect(50, 100, screen.getDisplayXSize() - 50, 370);
  setBackColor(255, 20, 20);
  screen.setFont(Grotesk16x32);
  screen.print(msg1, 60, 110);
  screen.print(msg2, 60, 145);
  screen.print(msg3, 60, 180);

  setColor(255, 255, 255);
  screen.fillRoundRect(60, 300, screen.getDisplayXSize() / 2 - 10, 340);
  setColor(0, 0, 0);
  screen.drawRoundRect(60, 300, screen.getDisplayXSize() / 2 - 10, 340);

  setColor(255, 255, 255);
  screen.fillRoundRect(screen.getDisplayXSize() / 2 + 10, 300, screen.getDisplayXSize() - 60, 340);
  setColor(0, 0, 0);
  screen.drawRoundRect(screen.getDisplayXSize() / 2 + 10, 300, screen.getDisplayXSize() - 60, 340);

  setColor(0, 0, 0);
  setBackColor(255, 255, 255);
  screen.print("YES", 70, 305);
  screen.print("NO", screen.getDisplayXSize() / 2 + 20, 305);

  while (true) {
    if (touchGetY() > 300 and touchGetY() < 340) {
      if (touchGetX() > 60 and touchGetX() < screen.getDisplayXSize() / 2 - 10) { // clicked on YES
        return true;
      }
      if (touchGetX() > screen.getDisplayXSize() / 2 + 10 and touchGetX() < screen.getDisplayXSize() - 60) { // clicked on NO
        refreshScreen();
        return false;
      }
    }
  }
}


void keyboard_draw_key(String letter, unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, bool selected) {

  if (selected) {
    setColor(100, 100, 100);
    setBackColor(100, 100, 100);
  }
  else {
    setColor(200, 200, 200);
    setBackColor(200, 200, 200);
  }

  screen.fillRoundRect(x1, y1, x2, y2);

  if (selected) {
    setColor(200, 200, 200);
  }
  else {
    setColor(100, 100, 100);
  }

  screen.drawRoundRect(x1, y1, x2, y2);
  screen.print(letter, x1 + 7, y1 + 8, 0);
}


String currentKeyboard = "";
bool keyboardShift = false;
bool keyboardSymbol = false;
String keyboardUpcomingText = "";

void show_keyboard() {
  File layout;
  openFile(layout, "S/D/KEYBOARD.MRT", FILE_READ);
  currentKeyboard = layout.readString();
  sample_RAM();
  closeFile(layout);
  
  setColor(255, 255, 255); // black background if dark mode, else white background
  screen.fillRect(0, screen.getDisplayYSize() - 400, screen.getDisplayXSize(), screen.getDisplayYSize() - 1);
  setColor(0, 0, 0);
  screen.drawLine(0, screen.getDisplayYSize() - 400, screen.getDisplayXSize(), screen.getDisplayYSize() - 400);
  setFont("large");

  String keyboard;
  if (keyboardSymbol) {
    keyboard = currentKeyboard.substring(40, 80);
  }
  else {
    keyboard = currentKeyboard.substring(0, 40);
  }


  keyboard_draw_key("Shift", 30, screen.getDisplayYSize() - 50, screen.getDisplayXSize() / 2 - 50, screen.getDisplayYSize() - 5, keyboardShift);
  keyboard_draw_key("Enter", screen.getDisplayXSize() / 2 + 50, screen.getDisplayYSize() - 50, screen.getDisplayXSize() - 30, screen.getDisplayYSize() - 5, false);
  keyboard_draw_key("Space", 30, screen.getDisplayYSize() - 100, screen.getDisplayXSize() / 3 - 10, screen.getDisplayYSize() - 55, false);

  if (keyboardSymbol) {
    keyboard_draw_key("Alpha ", screen.getDisplayXSize() / 3 + 10, screen.getDisplayYSize() - 100, screen.getDisplayXSize() * 2 / 3 - 10, screen.getDisplayYSize() - 55, false);
  }
  else {
    keyboard_draw_key("Symbol ", screen.getDisplayXSize() / 3 + 10, screen.getDisplayYSize() - 100, screen.getDisplayXSize() * 2 / 3 - 10, screen.getDisplayYSize() - 55, false);
  }

  keyboard_draw_key("Delete", screen.getDisplayXSize() * 2 / 3 + 10, screen.getDisplayYSize() - 100, screen.getDisplayXSize() - 30, screen.getDisplayYSize() - 55, false);

  unsigned int y = screen.getDisplayYSize() - 360;
  unsigned int xModifyer = 0;

  for (byte key = 0; key < keyboard.length(); key ++) {

    if (key % 10 == 0 and key != 0) { // new line in keyboard
      y += 60;
      xModifyer += 400;
    }

    unsigned int x = key * 40 + 40 - xModifyer;

    keyboard_draw_key(String(keyboard[key]), x, y, x + 30, y + 45, false);
  }
  clearString(keyboardUpcomingText);
  keyboardVisible = true;
}


char keyboard_input() {
  /*
    returns '\0' if no key is pressed
    returns '\1' if delete is pressed
    returns a character if it is pressed
    displays the keyboard if it is not yet on the screen
    must be called in a loop while text is being entered

    hide_keyboard() and refreshScreen() must be used to remove the keyboard from the screen when done
  */

  String keyboard;
  if (keyboardSymbol) {
    keyboard = currentKeyboard.substring(40, 80);
  }
  else {
    keyboard = currentKeyboard.substring(0, 40);
  }


  setFont("large");

  if (!keyboardVisible) {
    show_keyboard();
  }

  // RFID card macros
  if (mfrc522.PICC_IsNewCardPresent()) {
    if ( mfrc522.PICC_ReadCardSerial()) {

      RFIDCardDataToFile();
      File cardFile;
      openFile(cardFile, "S/RFID.MRT", FILE_READ);
      //addFileToList(&cardFile);
      
      String search1 = "";
      String search2 = "";
      bool copying = false;
      
      while (cardFile.available()){
        search1 = search2;
        search2 = "";
        for (byte i=0; i<20 and cardFile.available(); i++){
          search2 += char(cardFile.read());
        }
      
        if ((search1 + search2).indexOf("keyMacro>") >= 0){
          copying = false;
          keyboardUpcomingText += (search1 + search2).substring(0, (search1 + search2).indexOf("keyMacro>"));
        }
        if ((search1 + search2).indexOf("<keyMacro:") >= 0){
          copying = true;
          String search = search1 + search2;
          search2 = search.substring(search.indexOf("<keyMacro:") + 10);
          search1 = "";
        }
      
        if (copying){
          keyboardUpcomingText += search1;
        }
      }
      if (copying){
        keyboardUpcomingText += search2;
      }
      sample_RAM();
      
      closeFile(cardFile);
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
    }
  }


  if (keyboardUpcomingText != "") {
    char nextLetter = keyboardUpcomingText[0];
    keyboardUpcomingText.remove(0, 1);
    return nextLetter;
  }

  unsigned int y = screen.getDisplayYSize() - 360;
  unsigned int xModifyer = 0;

  for (byte key = 0; key < keyboard.length(); key ++) {

    if (key % 10 == 0 and key != 0) { // new line in keyboard
      y += 60;
      xModifyer += 400;
    }

    unsigned int x = key * 40 + 40 - xModifyer;

    if (touchGetX() > x and touchGetX() < x + 30 and touchGetY() > y and touchGetY() <  y + 45) {

      keyboard_draw_key(String(keyboard[key]), x, y, x + 30, y + 45, true);
      while (touch.dataAvailable());
      keyboard_draw_key(String(keyboard[key]), x, y, x + 30, y + 45, false);

      if (keyboardShift) {
        return keyboard[key];
      }

      char letter = keyboard[key];
      if (letter >= 'A' and letter <= 'Z') { // make lowercase
        letter -= 'A' - 'a';
      }
      return letter;
    }
  }


  if (touchGetY() >= screen.getDisplayYSize() - 50 and touchGetY() <= screen.getDisplayYSize() - 5) {

    if (touchGetX() >= 30 and touchGetX() <= screen.getDisplayXSize() / 2 - 50) { // shift button pressed

      if (keyboardShift) {
        keyboardShift = false;
      }
      else {
        keyboardShift = true;
      }

      keyboard_draw_key("Shift", 30, screen.getDisplayYSize() - 50, screen.getDisplayXSize() / 2 - 50, screen.getDisplayYSize() - 5, keyboardShift);

      while (touch.dataAvailable());

      return '\0';
    }

    if (touchGetX() >= screen.getDisplayXSize() / 2 + 50 and touchGetX() <= screen.getDisplayYSize() - 5) { // newline button pressed

      keyboard_draw_key("Enter", screen.getDisplayXSize() / 2 + 50, screen.getDisplayYSize() - 50, screen.getDisplayXSize() - 30, screen.getDisplayYSize() - 5, true);
      while (touch.dataAvailable());
      keyboard_draw_key("Enter", screen.getDisplayXSize() / 2 + 50, screen.getDisplayYSize() - 50, screen.getDisplayXSize() - 30, screen.getDisplayYSize() - 5, false);

      return '\n';
    }
  }

  if (touchGetY() >= screen.getDisplayYSize() - 100 and touchGetY() <= screen.getDisplayYSize() - 55) {


    if (touchGetX() > 30 and touchGetX() < screen.getDisplayXSize() / 3 - 10) { //space pressed

      keyboard_draw_key("Space", 30, screen.getDisplayYSize() - 100, screen.getDisplayXSize() / 3 - 10, screen.getDisplayYSize() - 55, true);
      while (touch.dataAvailable());
      keyboard_draw_key("Space", 30, screen.getDisplayYSize() - 100, screen.getDisplayXSize() / 3 - 10, screen.getDisplayYSize() - 55, false);

      return ' ';
    }

    if (touchGetX() > screen.getDisplayXSize() / 3 + 10 and touchGetX() < screen.getDisplayXSize() * 2 / 3 - 10) { // Alpa/Symbol key pressed
      String btnText = "";
      if (keyboardSymbol) {
        keyboardSymbol = false;
        btnText = "Alpha ";
      }
      else {
        keyboardSymbol = true;
        btnText = "Symbol ";
      }

      keyboard_draw_key(btnText, screen.getDisplayXSize() / 3 + 10, screen.getDisplayYSize() - 100, screen.getDisplayXSize() * 2 / 3 - 10, screen.getDisplayYSize() - 55, true);
      while (touch.dataAvailable());
      show_keyboard();

      return '\0';
    }

    if (touchGetX() > screen.getDisplayXSize() * 2 / 3 + 10 and touchGetX() < screen.getDisplayXSize() - 30) { // Delete pressed

      keyboard_draw_key("Delete", screen.getDisplayXSize() * 2 / 3 + 10, screen.getDisplayYSize() - 100, screen.getDisplayXSize() - 30, screen.getDisplayYSize() - 55, true);
      while (touch.dataAvailable());
      keyboard_draw_key("Delete", screen.getDisplayXSize() * 2 / 3 + 10, screen.getDisplayYSize() - 100, screen.getDisplayXSize() - 30, screen.getDisplayYSize() - 55, false);

      return '\1';
    }
  }


  return '\0';
}



void hide_keyboard() {
  if (keyboardVisible) {
    clearString(keyboardUpcomingText);
    keyboardShift = false;
    keyboardVisible = false;
    clearString(currentKeyboard);
  }
}




String encryptString(String text, byte iterations) {
  String cyphertext = "";
  unsigned int character = text.charAt(0) + text.charAt(text.length() - 1) + 1;
  while (character > 255) {
    character -= 256;
  }
  cyphertext += char(character);

  for (unsigned int i = 1; i < text.length(); i++) {
    character = text.charAt(i) + text.charAt(i - 1) + 3;
    while (character > 255) {
      character -= 256;
    }
    cyphertext += char(character);
  }

  if (iterations == 1) {
    return cyphertext;
  }
  return encryptString(cyphertext, iterations - 1);
}


bool askingPass = false;
String enteredPass = "";
String truePass = "";
byte incorrectPassInputs = 0;

void showPassInputScreen() {

  if (incorrectPassInputs >= 5) { // lock phone for some time if too many incorrect password attempts
    unsigned int waitTime = 10;

    if (incorrectPassInputs >= 8) {
      waitTime = 30;
    }
    if (incorrectPassInputs >= 10) {
      waitTime = 60;
    }
    if (incorrectPassInputs >= 11) {
      waitTime = 120;
    }
    if (incorrectPassInputs >= 15) {
      waitTime = 300;
    }
    if (incorrectPassInputs >= 17) {
      waitTime = 600;
    }
    if (incorrectPassInputs >= 20) {
      waitTime = 3600;
    }

    fillScr(255, 50, 50);
    setColor(255, 255, 255);
    setBackColor(255, 50, 50);
    setFont("large");
    screen.print(F("Your Device Has Been Locked"), CENTER, 200);
    setFont("medium");
    screen.print(F("Please wait to attempt"), CENTER, 260);
    screen.print(F("the password again."), CENTER, 280);
    screen.print(String(F("Password attempts: ")) + String(incorrectPassInputs), 10, 380);
    screen.print("Time Left:", CENTER, screen.getDisplayYSize() - 100);
    setFont("giant");

    while (waitTime > 0) {
      screen.print(" " + String(waitTime) + " ", CENTER, screen.getDisplayYSize() - 80);
      delay(1000);
      waitTime -= 1;
    }
  }

  File file;
  openFile(file, "S/D/PASSWORD.MRT", FILE_READ);

  truePass = file.readString();
  sample_RAM();
  closeFile(file);

  fillScr(0, 30, 60);

  setColor(255, 200, 20);
  setBackColor(255, 200, 20);
  screen.fillRoundRect(screen.getDisplayXSize() / 2 - 50, 250, screen.getDisplayXSize() / 2 + 50, 286);
  setColor(0, 0, 150);
  setFont("large");
  screen.print("Cancel", CENTER, 252);

  setBackColor(0, 30, 60);
  setColor(255, 255, 255);
  screen.print("Enter Password:", CENTER, 150);
  askingPass = true;
  clearString(enteredPass);
}

byte askForPass() {
  if (!askingPass) {
    showPassInputScreen();
  }

  char keyIn = keyboard_input();

  if (keyIn != '\0') { // key pressed
    if (keyIn == '\n') { // enter pressed
      enteredPass = encryptString(enteredPass, 10);
      askingPass = false;
      hide_keyboard();
      if (enteredPass == truePass) {
        incorrectPassInputs = 0;
        return 2; // password correct
      }
      else {
        incorrectPassInputs += 1;
        return 1; // password incorrect
      }
    }

    else if (keyIn == '\1') {
      if (enteredPass.length() > 0) {
        enteredPass.remove(enteredPass.length() - 1);
      }
    }

    else {
      enteredPass += keyIn;
    }

    setFont("medium");
    setBackColor(0, 30, 60);
    setColor(255, 255, 255);

    String asterixes = "";
    for (byte i = 0; i < enteredPass.length(); i++) {
      asterixes += "*";
    }

    while (asterixes.length() < 27) {
      asterixes += " ";
    }
    screen.print(asterixes, 10, 350);
  }
  else if (touch.dataAvailable()) {
    if (touchGetX() > screen.getDisplayXSize() / 2 - 50 and touchGetX() < screen.getDisplayXSize() / 2 + 50 and touchGetY() > 250 and touchGetY() < 286) {
      setColor(200, 100, 0);
      setBackColor(200, 100, 20);
      screen.fillRoundRect(screen.getDisplayXSize() / 2 - 50, 250, screen.getDisplayXSize() / 2 + 50, 286);
      setColor(255, 255, 255);
      setFont("large");
      screen.print("Cancel", CENTER, 252);

      while (touch.dataAvailable());

      askingPass = false;
      hide_keyboard();
      return 3; // canceled
    }
  }

  return 0; // still entering password
}

bool create_new_pass() {
  if (askForPass() != 0) {
    File passFile;
    openFile(passFile, "S/PASSWORD.MRT", FILE_WRITE);
    passFile.print(enteredPass);
    closeFile(passFile);
    return false; // process finished
  }
  else {
    return true; // still waiting for user to input password
  }
}








unsigned long lastTopBarUpdate = 0;
bool topBarShowsBluetooth = false;

void showTopBar() {
  String last_control = CONTROLLING_APP;
  CONTROLLING_APP = "SYS";
  lastTopBarUpdate = millis();
  scr_removeLayer("top-bar");
  /*if (wifi_on()) { // checks if wifi was disabled and updates 'wifi_connected' variable to false if it was
    Serial2.print("get_SSID\n");
    while (Serial2.available()) {
      Serial2.read();
    }
    while (!Serial2.available());
    String connected_SSID = Serial2.readStringUntil('\n');
    connected_SSID = connected_SSID.substring(0, connected_SSID.length() - 1);
    if (connected_SSID.length() > 0) {
      wifi_connected = true;
    }
    else {
      wifi_connected = false;
    }
  }*/

  byte color;
  byte b_color;

  color = 0;
  b_color = 255;

  fillRect("top-bar", b_color, b_color, b_color, 0, 0, screen.getDisplayXSize(), 13);
  drawLine("top-bar", color, color, color, 0, 14, screen.getDisplayXSize(), 14);
  print("top-bar", color, color, color, b_color, b_color, b_color, 1, 1, 0, "small", getTimeString());

  byte iconPosition = 0;

  //if (wifi_connected) {
  //  showBIM("wifi-icon", String("/MPOS/S/") + "D/R/WIFION.BIM", screen.getDisplayXSize() - 20 - iconPosition, 0, 1, 1, b_color, b_color, b_color, color, color, color);
  //  iconPosition += 20;
  //}

  topBarShowsBluetooth = false;
  if (millis() - BTLastPing < 10000) {
    showBIM("top-bar", String("/MPOS/S/") + "D/R/BTON.BIM", screen.getDisplayXSize() - 10 - iconPosition, 0, 1, 1, b_color, b_color, b_color, color, color, color);
    iconPosition += 10;
    topBarShowsBluetooth = true;
  }

  refreshScreen(true);
  CONTROLLING_APP = last_control;
}



void deleteDirectory(String dirName) {
  
  if (dirName.charAt(dirName.length()-1) == '/') {
    dirName = dirName.substring(0, dirName.length()-1);
  }
  
  File dir = SD.open(dirName); // openFile() doesn't support directories yet
  //addFileToList(&dir);
  while (true) {
    File entry =  dir.openNextFile();
    //addFileToList(&entry);
    if (! entry) {
      // no more files
      break;
    }
    sample_RAM();

    String entryName = dirName + "/" + String(entry.name());
    if (entry.isDirectory()) {
      deleteDirectory(entryName);
    } else {
      SD.remove(entryName);
    }
    closeFile(entry);
  }
  closeFile(dir);
  SD.rmdir(dirName);
}



void factoryReset() {

  if (confirmation_message("Are you sure you want", F("to Factory Reset your"), "device?") == false) {
    return;
  }

  SD.remove(String("/MPOS/S/") + "D/password.mrt");
  SD.remove(String("/MPOS/S/") + "screen.mli");
  SD.remove(String("/MPOS/S/") + "SOUNDT.MRT");
  SD.remove(String("/MPOS/S/") + "SOUND.MRT");
  SD.remove(String("/MPOS/S/") + "NOTIF.MRT");
  SD.remove(String("MPOS/S/SETTINGS/") + "blfilt.mrt");
  SD.remove(String("MPOS/S/SETTINGS/") + "colinvrt.mrt");
  SD.remove(String("MPOS/S/SETTINGS/") + "sound.mrt");
  SD.remove(String("MPOS/S/SETTINGS/") + "blfilt.mrt");
  SD.remove(String("MPOS/S/SETTINGS/") + "bright.mrt");
  SD.remove(String("MPOS/S/SETTINGS/") + "NAME.MRT");
  SD.remove(String("MPOS/S/SETTINGS/") + "DARK.MRT");
  SD.remove(String("MPOS/S/SETTINGS/") + "TRACK.MRT");
  SD.remove(String("MPOS/S/SETTINGS/") + "BLUA.MRT");
  SD.remove(String("/MPOS/S/") + "BT/SAVE.MRT");
  

  deleteDirectory("/MPOS/F/");
  SD.mkdir("/MPOS/F/");


  setColor(255, 255, 255);
  setBackColor(255, 0, 0);
  fillScr(255, 0, 0);
  screen.setFont(BigFont);
  screen.print(F("Your device has been factory-"), 0, 50);
  screen.print(F("reset. Restart it to continue"), 0, 100);
  delay(5000);
  digitalWrite(powerPin, LOW);
  while (true);
}





byte backgroundLoop = 0;
unsigned long lastBackgroundTime = 0;


void run_background(){
  File file;
  openFile(file, "S/SETTINGS/BACKGRD.MRT", FILE_READ);

  for (byte i=0; i<backgroundLoop and file.available();i++){
    file.readStringUntil('\n');
  }

  if (!file.available()){
    file.seek(0);
    backgroundLoop = 0;
  }
  String appName = file.readStringUntil('\n');
  closeFile(file);
  String prev_control = CONTROLLING_APP;
  
  if (appName == "Notifications"){
    CONTROLLING_APP = "SYS";
    BACKGROUND_NOTIFS();
  }
  else if (appName == "Temp Tracking"){
    CONTROLLING_APP = "SYS";
    BACKGROUND_TEMP_MONITOR();
  }
  else if (appName == "RAM Tracking"){
    CONTROLLING_APP = "SYS";
    BACKGROUND_RAM_MONITOR();
  }
  else if (appName == "TEXT app"){
    CONTROLLING_APP = "TEXT";
    TEXT_BACKGROUND();
  }
  
  lastBackgroundTime = millis();
  backgroundLoop += 1;
  CONTROLLING_APP = prev_control;
}



jmp_buf jmp_buffer;
bool stackFrameSaved = false;
const byte STACK_DUMP_SIZE = 20;
uint8_t stackBuffer[STACK_DUMP_SIZE];
uint8_t* recoverStackPointer;
byte errorLevel = 1;

/*
void copy_stack_to_file(){
  WatchDog::stop();
  uint8_t* stackpointer;
  asm volatile (
    "in %A0, __SP_L__ \n\t"
    "in %B0, __SP_H__"
    : "=r" (stackpointer)
  );

  File file = SD.open(String("/MPOS/S/") + "RECOV.MRT", FILE_WRITE);
  //file.print(stackpointer);
  //file.print("\n");
  for (uint8_t* i=RAMEND; i>=stackpointer; i++){
    file.print(String(*i));
    Serial.println(String(*i));
    file.print("\n");
  }
  closeFile(file);
  stackFrameSaved = true;
  lastBackgroundTime = millis();
  WatchDog::start();
}

void recover_stack_from_file(){
  WatchDog::stop();
  uint8_t* i = RAMEND;
  File file = SD.open(String("/MPOS/S/") + "RECOV.MRT", FILE_READ);
  while (file.available()){
    *i = file.readStringUntil('\n').toInt();
    Serial.println(*i);
    i -= 1;
  }
  closeFile(file);
  lastBackgroundTime = millis();
  WatchDog::start();
}
*/

void WDT_trigger() { // ensures no app has crashed the entire system
  if (millis() - lastBackgroundTime > 15000){
    longjmp(jmp_buffer, errorLevel);
  }
}

//watchdog recovery

void handle_jmp(){

  int result = setjmp(jmp_buffer);
  
  if (result != 0){
    lastBackgroundTime = millis();
    errorLevel += 1;
    Serial.print("Watchdog triggered. State: ");
    Serial.println(result);
    Serial.print("Free RAM: ");
    Serial.println(freeMemory());
    Serial.print("Current APP: ");
    Serial.println(CURRENT_APP);
    Serial.print("Control APP: ");
    Serial.println(CONTROLLING_APP);
    Serial.println();
  }

  //Serial.print("stack size: ");
  //Serial.println(RAMEND - SP);

  /* skipping recovery attempts because of stack return problem. just reboot. might get back to this later.

  if (result == 1){
    // hope the problem sorts itself out with the jump
    //printFullMemory();
  }
  else if (result == 2){
    quit_app(&CURRENT_APP);
    set_app("HOME");
  }
  else if (result == 3){
    //free(emergencyFreePointer);
    set_app("HOME");
  }

  */

  if (result == 1){

    //SP = RAMEND - 10; // decrease stack pointer to get extra memory. shutting down, therefore will never return or pull from stack
    //__brkval = &__heap_start + 10;


    //free(emergencyFreePointer);
    //__brkval -= 500;
    //fix28135_malloc_bug();
    fillScr(255, 0, 0);
    setColor(255, 255, 255);
    setBackColor(255, 0, 0);
    screen.setFont(Grotesk24x48);
    screen.print("ERROR", 100, 100);
    screen.setFont(BigFont);
    screen.print(F("Unrecoverable System Crash"), 0, 300);
    screen.print("Current APP:", 0, 350);
    screen.print(CURRENT_APP, 250, 350);
    screen.print("Control APP:", 0, 380);
    screen.print(CONTROLLING_APP, 250, 380);
    screen.print("Free RAM:", 0, 410);
    screen.print(String(freeMemory()), 250, 410);
    delay(5000);

    CONTROLLING_APP = "SYS";
    shut_down();
  }

  else if (result == 2){
    WatchDog::stop();
    digitalWrite(powerPin, LOW);
    while (true);
  }
}

String appPathArg = ""; // indicates which file an app should open
String allowedExt = ""; // indicates which file extensions are supported by the currrent app, mostly used by Files   -   "" means all file types, "*" means folders can be selected    -   ".*" means folders only
String previousApp = ""; // indicates which app a process should return to, mostly used by Files



void set_app(String appName) {
  String prev_control = CONTROLLING_APP;
  CONTROLLING_APP = "SYS";

  scr_removeLayer("HOME-BUTTON");
  if (appName != "HOME") {
    fillCircle("HOME-BUTTON", 255, 165, 0, screen.getDisplayXSize() / 2, screen.getDisplayYSize() - 25, 23);
    drawCircle("HOME-BUTTON", 255, 255, 255, screen.getDisplayXSize() / 2, screen.getDisplayYSize() - 25, 23);
    print("HOME-BUTTON", 255, 255, 255, 255, 165, 0, screen.getDisplayXSize() / 2 - 8, screen.getDisplayYSize() - 41, 0, "large", "H");
  }

  if (appName != CURRENT_APP) {
    hide_keyboard();
    scr_removeApp(CURRENT_APP);

    previousApp = CURRENT_APP;
    CURRENT_APP = appName;
    CONTROLLING_APP = appName;

    if (appName == "HOME") {
      HOMESCREEN_START();
    }

    else if (appName == "SETTINGS") {
      SETTINGS_START();
    }

    else if (appName == "FILES") {
      FILES_START();
    }

    else if (appName == "TEXT") {
      TEXT_START();
    }

    else if (appName == "RFID") {
      RFID_START();
    }





    else {
      set_app("HOME");
      addNotification("OS ERROR", String(F("Invalid app redirect to '")) + appName + "'.");
    }

    refreshScreen(true); // re-draw system graphics (notifications, home button, time ...)

  }
  CONTROLLING_APP = prev_control;
}






void quit_app(String *appName) {
  if (*appName == "SETTINGS") {
    SETTINGS_QUIT();
  }
  else if (*appName == "FILES") {
    FILES_QUIT();
  }
  else if (*appName == "TEXT") {
    TEXT_QUIT();
  }
  else if (*appName == "RFID") {
    RFID_QUIT();
  }

  else {
    addNotification(F("ERROR: Couldn't quit app"), "The app '" + *appName + "' couldn't be quit, because it isn't recognised.");
  }
}








/*

  applications
  all apps will have the following functions:

  _START()
  runs when the window is opened

  _()
  runs when the app is opened and its window is being displayed
  this will be run repeatedly and should execute quickly


  _QUIT()
  runs when the app is quit completely
  this should clear all the RAM used by the app


  more functions for each app may also be defined,
  but they won't be called by the O.S.




*/

















void HOMESCREEN_START() {
  clearString(appPathArg);
  clearString(allowedExt);

  fillScr(50, 50, 50);

  File layout;
  openFile(layout, "S/D/HOME.MRT", FILE_READ);
  unsigned int x = 50;
  unsigned int y = 100;

  while (layout.available()){
    if (x > 350){
      x = 50;
      y += 150;
    }

    String appName = layout.readStringUntil('\n');
    print("appNames", 255, 255, 255, 50, 50, 50, x, y+65, 0, "small", appName);
    if (appName.length() > 8){
      appName.remove(8);
    }

    if (SD.exists(String("/MPOS/S/") + "D/A/" + appName + ".MI2")) {
      showMCI("icon", String("/MPOS/S/") + "D/A/" + appName + ".MI2", x, y, 3, 3);
    }
    else{
      showMCI("icon", String("/MPOS/S/") + "D/A/" + appName + ".MCI", x, y, 3, 3);
    }

    x += 100;
  }
  closeFile(layout);
}

void HOMESCREEN() {

  if (!touch.dataAvailable()){
    return;
  }

  File layout;
  openFile(layout, "S/D/HOME.MRT", FILE_READ);
  unsigned int x = 50;
  unsigned int y = 100;

  while (layout.available()){
    if (x > 350){
      x = 50;
      y += 150;
    }

    if (touchGetX() > x and touchGetY() > y and touchGetX() < x+60 and touchGetY() < y+60){
      String appName = layout.readStringUntil('\n');
      appName.toUpperCase();
      closeFile(layout);
      set_app(appName);
      return;
    }
    else{
      layout.readStringUntil('\n');
    }

    x += 100;
  }
  closeFile(layout);
}








String SETTINGS_page = "";
bool SETTINGS_newRAMDataAvailable = false;
bool SETTINGS_newLoadDataAvailable = false;
bool SETTINGS_newBTFound = false;
byte SETTINGS_listSize = 0;
String SETTINGS_enteredMAC = "";


void SETTINGS_START() {
  fillScr(240, 240, 240);
  if (SETTINGS_page == "") {
    print("", 0, 0, 0, 240, 240, 240, 20, 30, 0, "large", "Settings");
    print("", 0, 0, 0, 240, 240, 240, 20, 100, 0, "medium", "About");
    print("", 0, 0, 0, 240, 240, 240, 20, 140, 0, "medium", "Device Name");
    print("", 0, 0, 0, 240, 240, 240, 20, 180, 0, "medium", "Network");
    print("", 0, 0, 0, 240, 240, 240, 20, 220, 0, "medium", "Processes");
    print("", 0, 0, 0, 240, 240, 240, 20, 260, 0, "medium", "Time");
    print("", 0, 0, 0, 240, 240, 240, 20, 300, 0, "medium", "Display Options");
    print("", 0, 0, 0, 240, 240, 240, 20, 340, 0, "medium", "Sound");
    print("", 0, 0, 0, 240, 240, 240, 20, 380, 0, "medium", "Shut Down");
    print("", 0, 0, 0, 240, 240, 240, 20, 420, 0, "medium", "Factory Reset");

    drawLine("", 100, 100, 100, 0, 90, screen.getDisplayXSize(), 90);
    drawLine("", 100, 100, 100, 0, 130, screen.getDisplayXSize(), 130);
    drawLine("", 100, 100, 100, 0, 170, screen.getDisplayXSize(), 170);
    drawLine("", 100, 100, 100, 0, 210, screen.getDisplayXSize(), 210);
    drawLine("", 100, 100, 100, 0, 250, screen.getDisplayXSize(), 250);
    drawLine("", 100, 100, 100, 0, 290, screen.getDisplayXSize(), 290);
    drawLine("", 100, 100, 100, 0, 330, screen.getDisplayXSize(), 330);
    drawLine("", 100, 100, 100, 0, 370, screen.getDisplayXSize(), 370);
    drawLine("", 100, 100, 100, 0, 410, screen.getDisplayXSize(), 410);
    drawLine("", 100, 100, 100, 0, 450, screen.getDisplayXSize(), 450);
  }

  if (SETTINGS_page == "About") {
    print("Title", 0, 0, 0, 240, 240, 240, 20, 30, 0, "large", F("<- Settings - About"));

    print("", 0, 0, 0, 240, 240, 240, 20, 90, 0, "medium", F("Device: MaxPhone 2S"));
    print("", 0, 0, 0, 240, 240, 240, 20, 120, 0, "medium", "Software: MPOS " + CURRENT_VERSION);
    print("", 0, 0, 0, 240, 240, 240, 20, 120, 0, "medium", "Compiled: " + String(__DATE__));

    drawLine("", 100, 100, 100, 0, 85, screen.getDisplayXSize(), 85);
    drawLine("", 100, 100, 100, 0, 115, screen.getDisplayXSize(), 115);
    drawLine("", 100, 100, 100, 0, 145, screen.getDisplayXSize(), 145);
  }

  if (SETTINGS_page == "Device-Name") {
    print("", 0, 0, 0, 240, 240, 240, 20, 30, 0, "large", F("<- Settings - Device Name"));
    print("name", 0, 0, 0, 240, 240, 240, 20, 365, 0, "medium", deviceName);
  }

  if (SETTINGS_page == "Network") {
    print("", 0, 0, 0, 240, 240, 240, 20, 30, 0, "large", F("<- Settings - Network"));
    drawLine("", 100, 100, 100, 0, 90, screen.getDisplayXSize(), 90);
    drawLine("", 100, 100, 100, 0, 130, screen.getDisplayXSize(), 130);
    drawLine("", 100, 100, 100, 0, 170, screen.getDisplayXSize(), 170);
    drawLine("", 100, 100, 100, 0, 210, screen.getDisplayXSize(), 210);
    print("", 0, 0, 0, 240, 240, 240, 20, 100, 0, "medium", F("Bluetooth Power:"));
    print("", 0, 0, 0, 240, 240, 240, 20, 140, 0, "medium", F("Manage Devices"));
    print("", 0, 0, 0, 240, 240, 240, 20, 180, 0, "medium", F("Pair New Device"));
    on_off_input("bt-switch", screen.getDisplayXSize() - 70, 110, bluetoothActive);
  }

  if (SETTINGS_page == "WiFi") {
    print("", 0, 0, 0, 240, 240, 240, 20, 30, 0, "large", F("<- Settings - WiFi"));
    print("", 0, 0, 0, 240, 240, 240, 20, 90, 0, "medium", "unavailable - coming soon!");
  }

  if (SETTINGS_page == "Bluetooth") {
    print("", 0, 0, 0, 240, 240, 240, 20, 30, 0, "large", F("<- Settings - Bluetooth"));
    print("", 0, 0, 0, 240, 240, 240, 20, 90, 0, "medium", "Saved Devices:");
    unsigned int yPos = 130;
    File file;
    if (openFile(file, "S/BT/SAVE.MRT", FILE_READ)){
      //addFileToList(&file);
      while (file.available() and yPos < screen.getDisplayYSize()){
        drawLine("", 100, 100, 100, 0, yPos, screen.getDisplayXSize(), yPos);
        file.readStringUntil('\t'); // don't print MAC
        print("list", 0, 0, 0, 240, 240, 240, 20, yPos + 15, 0, "medium", file.readStringUntil('\n'));
        yPos += 50;
      }
      closeFile(file);
    }
    if (yPos == 130) {
      print("list", 0, 0, 0, 240, 240, 240, 20, yPos + 15, 0, "medium", F("You have no saved Bluetooth devices"));
      yPos += 50;
    }
    drawLine("", 100, 100, 100, 0, yPos, screen.getDisplayXSize(), yPos);
  }

  if (SETTINGS_page == "Bluetooth-Manage") {
    print("", 0, 0, 0, 240, 240, 240, 20, 30, 0, "large", F("<- Settings - Bluetooth"));

    bool searching = true;
    File file;
    if (openFile(file, "S/BT/SAVE.MRT", FILE_READ)){
      while (file.available() and searching){
        String mac = file.readStringUntil('\t');
        String name = file.readStringUntil('\n');

        if (mac == SETTINGS_enteredMAC){
          searching = false;
          print("", 0, 0, 0, 240, 240, 240, 20, 90, 0, "medium", "Manage Device");
          drawLine("", 100, 100, 100, 0, 135, screen.getDisplayXSize(), 135);
          print("", 0, 0, 0, 240, 240, 240, 20, 150, 0, "medium", "Name: " + name);
          drawLine("", 100, 100, 100, 0, 185, screen.getDisplayXSize(), 185);
          print("", 0, 0, 0, 240, 240, 240, 20, 200, 0, "medium", "MAC: " + mac);
          drawLine("", 100, 100, 100, 0, 235, screen.getDisplayXSize(), 235);
          fillRoundRect("", 0, 0, 255, 20, screen.getDisplayYSize() - 200, screen.getDisplayXSize() / 2 - 10, screen.getDisplayYSize() - 100);
          fillRoundRect("", 0, 0, 255, screen.getDisplayXSize() / 2 + 10, screen.getDisplayYSize() - 200, screen.getDisplayXSize() - 10, screen.getDisplayYSize() - 100);
          print("", 255, 255, 255, 0, 0, 255, 60, screen.getDisplayYSize() - 160, 0, "medium", "Connect");
          print("", 255, 255, 255, 0, 0, 255, screen.getDisplayXSize() / 2 + 70, screen.getDisplayYSize() - 160, 0, "medium", "Forget");
        }
      }
      closeFile(file);
    }

    if (searching){ // MAC not found in save file
      clearString(SETTINGS_enteredMAC);
      scr_removeLayer("");
      SETTINGS_page = "Bluetooth";
      SETTINGS_START();
    }
  }

  if (SETTINGS_page == "Bluetooth-Pair") {
    print("", 0, 0, 0, 240, 240, 240, 20, 30, 0, "large", F("<- Settings - Pair Device"));

    fillRoundRect("refresh", 20, 20, 255, 20, 90, screen.getDisplayXSize() / 2 - 10, 150);
    drawRoundRect("refresh", 0, 0, 0, 20, 90, screen.getDisplayXSize() / 2 - 10, 150);
    fillRoundRect("other", 20, 20, 255, screen.getDisplayXSize() / 2 + 10, 90, screen.getDisplayXSize() - 20, 150);
    drawRoundRect("other", 0, 0, 0, screen.getDisplayXSize() / 2 + 10, 90, screen.getDisplayXSize() - 20, 150);
    print("refresh", 255, 255, 255, 20, 20, 255, 50, 110, 0, "medium", "Refresh");
    print("other", 255, 255, 255, 20, 20, 255, screen.getDisplayXSize() / 2 + 40, 110, 0, "medium", "Other...");
    drawLine("", 100, 100, 100, 0, 160, screen.getDisplayXSize(), 160);

    bluetooth_scan();
    SETTINGS_newBTFound = false;
    SETTINGS_listSize = 0;
  }

  if (SETTINGS_page == "Processes") {
    print("", 0, 0, 0, 240, 240, 240, 20, 30, 0, "large", F("<- Settings - Processes"));
    print("", 0, 0, 0, 240, 240, 240, 20, 90, 0, "medium", "RAM conumption");
    print("", 0, 0, 0, 240, 240, 240, 20, 290, 0, "medium", F("CPU load (loop time)"));

    print("", 0, 0, 0, 240, 240, 240, 20, 490, 0, "medium", "Track RAM and load:");
    on_off_input("track-switch", screen.getDisplayXSize() - 70, 500, ramTracking);

    print("", 0, 0, 0, 240, 240, 240, 20, 540, 0, "medium", "Background Tasks");
    print("", 0, 0, 0, 240, 240, 240, 20, 590, 0, "medium", "Quit Apps");
    drawLine("", 100, 100, 100, 0, 475, screen.getDisplayXSize(), 475);
    drawLine("", 100, 100, 100, 0, 525, screen.getDisplayXSize(), 525);
    drawLine("", 100, 100, 100, 0, 575, screen.getDisplayXSize(), 575);
    drawLine("", 100, 100, 100, 0, 625, screen.getDisplayXSize(), 625);
  }


  if (SETTINGS_page == "Quit") {
    print("Title", 0, 0, 0, 240, 240, 240, 20, 30, 0, "large", F("<- Settings - Quit Apps"));
    File file;
    if (openFile(file, "S/D/APPS.MRT", FILE_READ)){
      int yPos = 100;
      while (file.available()){
        String appName = file.readStringUntil('\n');
        print("apps", 0, 0, 0, 240, 240, 240, 20, yPos + 15, 0, "medium", appName);
        yPos += 50;
        drawLine("div", 100, 100, 100, 0, yPos, screen.getDisplayXSize(), yPos);
      }
      closeFile(file);
    }
    else{
      addNotification("Missing File", F("The root file '/MPOS/S/D/APPS.MRT' was not found."));
    }
  }

  if (SETTINGS_page == "Background") {
    print("", 0, 0, 0, 240, 240, 240, 20, 30, 0, "large", F("<- Settings - Background"));
    print("", 0, 0, 0, 240, 240, 240, CENTER, 90, 0, "medium", F("This page allows you to"));
    print("", 0, 0, 0, 240, 240, 240, CENTER, 110, 0, "medium", F("customize bakground tasks"));
    print("", 0, 0, 0, 240, 240, 240, CENTER, 130, 0, "medium", F("Don't touch anything unless"));
    print("", 0, 0, 0, 240, 240, 240, CENTER, 150, 0, "medium", F("you know what you are doing."));

    String backAllow = "";
    File file;
    if (openFile(file, "S/SETTINGS/BACKGRD.MRT", FILE_READ)){
      backAllow = file.readString();
      closeFile(file);
    }
    else{
      openFile(file, "S/SETTINGS/BACKGRD.MRT", FILE_WRITE);
      closeFile(file);
      addNotification("Missing File", F("The root file '/MPOS/S/SETTINGS/BACKGRD.MRT' was not found."));
    }

    Serial.println(backAllow);

    print("", 0, 0, 0, 240, 240, 240, 20, 195, 0, "medium", "SYSTEM");
    bool allow = false;
    //if (backAllow.indexOf("SYS\n") >= 0){
    //  allow = true;
    //}
    //on_off_input("switch-SYS", screen.getDisplayXSize() -70, 205, allow);
    drawLine("div", 100, 100, 100, 0, 180, screen.getDisplayXSize(), 180);
    //drawLine("div", 100, 100, 100, 0, 230, screen.getDisplayXSize(), 230);
    
    if (openFile(file, "S/D/ABTASKS.MRT", FILE_READ)){
      int yPos = 180;
      while (file.available()){
        String appName = file.readStringUntil('\n');
        print("", 0, 0, 0, 240, 240, 240, 20, yPos + 15, 0, "medium", appName);
        allow = false;
        //appName.toUpperCase();
        if (backAllow.indexOf(appName+'\n') >= 0){
          allow = true;
        }
        on_off_input("switch-"+appName, screen.getDisplayXSize() -70, yPos + 25, allow);
        yPos += 50;
        drawLine("div", 100, 100, 100, 0, yPos, screen.getDisplayXSize(), yPos);
      }
      closeFile(file);
    }
    else{
      addNotification("Missing File", F("The root file '/MPOS/S/D/ABTASKS.MRT' was not found."));
    }
  }

  if (SETTINGS_page == "Time") {
    print("", 0, 0, 0, 240, 240, 240, 20, 30, 0, "large", F("<- Settings - Time"));

    showBIM("buttons", String("/MPOS/S/") + "D/R/UP.BIM", 130, 100, 4, 4, 240, 240, 240, 0, 0, 0);
    showBIM("buttons", String("/MPOS/S/") + "D/R/UP.BIM", 220, 100, 4, 4, 240, 240, 240, 0, 0, 0);
    showBIM("buttons", String("/MPOS/S/") + "D/R/UP.BIM", 320, 100, 4, 4, 240, 240, 240, 0, 0, 0);
    showBIM("buttons", String("/MPOS/S/") + "D/R/DOWN.BIM", 130, 210, 4, 4, 240, 240, 240, 0, 0, 0);
    showBIM("buttons", String("/MPOS/S/") + "D/R/DOWN.BIM", 220, 210, 4, 4, 240, 240, 240, 0, 0, 0);
    showBIM("buttons", String("/MPOS/S/") + "D/R/DOWN.BIM", 320, 210, 4, 4, 240, 240, 240, 0, 0, 0);
  }

  if (SETTINGS_page == "Display") {
    print("", 0, 0, 0, 240, 240, 240, 20, 30, 0, "large", F("<- Settings - Display"));
    print("", 0, 0, 0, 240, 240, 240, 20, 100, 0, "medium", F("Brightness:"));
    fillRoundRect("brightness-scroll", 200, 200, 200, 20, 125, screen.getDisplayXSize() - 20, 135);

    unsigned int X_length = map(brightnessPercent, 10, 100, 20, screen.getDisplayXSize() - 20);

    fillRoundRect("brightness-scroll", 100, 50, 255, 20, 126, X_length, 134);
    fillCircle("brightness-scroll", 255, 255, 255, X_length, 130, 10);
    drawCircle("brightness-scroll", 0, 0, 0, X_length, 130, 11);


    print("", 0, 0, 0, 240, 240, 240, 20, 175, 0, "medium", F("Blue light filter:"));
    on_off_input("bluelight-filter-switch", screen.getDisplayXSize() - 70, 185, blueFilter);

    print("", 0, 0, 0, 240, 240, 240, 20, 225, 0, "medium", F("Invert Colors:"));
    on_off_input("invert-color-switch", screen.getDisplayXSize() - 70, 235, invertColor);

    print("", 0, 0, 0, 240, 240, 240, 20, 275, 0, "medium", "Dark Mode:");
    on_off_input("dark-mode-switch", screen.getDisplayXSize() - 70, 285, darkMode);

    drawLine("", 100, 100, 100, 0, 90, screen.getDisplayXSize(), 90);
    drawLine("", 100, 100, 100, 0, 160, screen.getDisplayXSize(), 160);
    drawLine("", 100, 100, 100, 0, 210, screen.getDisplayXSize(), 210);
    drawLine("", 100, 100, 100, 0, 260, screen.getDisplayXSize(), 260);
    drawLine("", 100, 100, 100, 0, 310, screen.getDisplayXSize(), 310);

  }

  if (SETTINGS_page == "Sound") {
    print("", 0, 0, 0, 240, 240, 240, 20, 30, 0, "large", F("<- Settings - Sound"));

    drawLine("", 100, 100, 100, 0, 90, screen.getDisplayXSize(), 90);
    drawLine("", 100, 100, 100, 0, 130, screen.getDisplayXSize(), 130);
    print("", 0, 0, 0, 240, 240, 240, 20, 100, 0, "medium", F("Mute Sound:"));
    on_off_input("mute-switch", screen.getDisplayXSize() - 70, 110, !volume);
  }

}


void SETTINGS() {
  if (SETTINGS_page == "") {

    if (touchGetY() > 90 and touchGetY() < 130) {
      scr_removeLayer("");
      SETTINGS_page = "About";
      SETTINGS_START();
    }
    if (touchGetY() > 130 and touchGetY() < 170) {
      scr_removeLayer("");
      SETTINGS_page = "Device-Name";
      SETTINGS_START();
    }
    if (touchGetY() > 170 and touchGetY() < 210) {
      scr_removeLayer("");
      SETTINGS_page = "Network";
      SETTINGS_START();
    }
    if (touchGetY() > 210 and touchGetY() < 250) {
      scr_removeLayer("");
      SETTINGS_page = "Processes";
      SETTINGS_START();
    }
    if (touchGetY() > 250 and touchGetY() < 290) {
      scr_removeLayer("");
      SETTINGS_page = "Time";
      SETTINGS_START();
    }
    if (touchGetY() > 290 and touchGetY() < 330) {
      scr_removeLayer("");
      SETTINGS_page = "Display";
      SETTINGS_START();
    }
    if (touchGetY() > 330 and touchGetY() < 370) {
      scr_removeLayer("");
      SETTINGS_page = "Sound";
      SETTINGS_START();
    }
    if (touchGetY() > 370 and touchGetY() < 410) {
      if (confirmation_message("Are you sure you want", F("to shut down your"), "device?")){
        shut_down();
      }
    }
    if (touchGetY() > 410 and touchGetY() < 450) {
      factoryReset();
    }
  }


  if (SETTINGS_page == "About") {

    if (touchGetX() < 52 and touchGetY() < 60 and touch.dataAvailable()) { // press back button

      clearString(SETTINGS_page);
      scr_removeLayer("");
      SETTINGS_START();
    }
  }


  if (SETTINGS_page == "Device-Name") {

    char key = keyboard_input();
    if (key != '\0' and key != '\n') {

      if (key == '\1') {
        if (deviceName.length() > 0) {
          deviceName.remove(deviceName.length() - 1);
        }
      }

      else {
        deviceName += key;
      }

      String printName = deviceName;
      while (printName.length() < 26) {
        printName += " ";
      }

      scr_removeLayer("name");
      print("name", 0, 0, 0, 240, 240, 240, 20, 365, 0, "medium", printName);
    }

    if ((touchGetX() < 52 and touchGetY() < 60 and touch.dataAvailable()) or key == '\n') { // press back button

      clearString(SETTINGS_page);
      scr_removeLayer("");
      hide_keyboard();
      SETTINGS_START();

      SD.remove(String("MPOS/S/SETTINGS/") + "NAME.MRT");
      File setFile;
      openFile(setFile, "S/SETTINGS/NAME.MRT", FILE_WRITE);
      setFile.print(deviceName);
      closeFile(setFile);
    }
  }


  if (SETTINGS_page == "Network") {
    if (touch.dataAvailable()){
        

      if (touchGetX() < 52 and touchGetY() < 60) { // press back button
        clearString(SETTINGS_page);
        scr_removeLayer("");
        SETTINGS_START();
      }


      if (touchGetX() > screen.getDisplayXSize() - 75 and touchGetY() > 95 and touchGetX() < screen.getDisplayXSize() - 15 and touchGetY() < 125) { // bluetooth switch
        SD.remove(String("MPOS/S/SETTINGS/") + "BLUA.MRT");
        File setFile;
        openFile(setFile, "S/SETTINGS/BLUA.MRT", FILE_WRITE);
        if (bluetoothActive == true) {
          setFile.print("N");
          bluetooth_power_off();
          bluetoothActive = false;
        }
        else {
          setFile.print("Y");
          bluetoothPowerOn();
          bluetoothActive = true;
        }
        closeFile(setFile);
        scr_removeLayer("bt-switch");
        on_off_input("bt-switch", screen.getDisplayXSize() - 70, 110, bluetoothActive);
      }

      else if (touchGetY() > 130 and touchGetY() < 170){
        SETTINGS_page = "Bluetooth";
        scr_removeLayer("");
        SETTINGS_START();
      }

      else if (touchGetY() > 170 and touchGetY() < 210){
        SETTINGS_page = "Bluetooth-Pair";
        scr_removeLayer("");
        SETTINGS_START();
      }
    }
  }


  if (SETTINGS_page == "WiFi") {

    if (touchGetX() < 52 and touchGetY() < 60 and touch.dataAvailable()) { // press back button

      SETTINGS_page = "Network";
      scr_removeLayer("");
      SETTINGS_START();
    }
  }


  if (SETTINGS_page == "Bluetooth") {

    if (touchGetX() < 52 and touchGetY() < 60 and touch.dataAvailable()) { // press back button

      SETTINGS_page = "Network";
      scr_removeLayer("");
      SETTINGS_START();
    }

    int touchY = touchGetY();
    if (touch.dataAvailable() and touchY > 130){
      int yPos = 130;
      File file;
      if (openFile(file, "S/BT/SAVE.MRT", FILE_READ)){
        while (file.available() and yPos < screen.getDisplayYSize()){
          SETTINGS_enteredMAC = file.readStringUntil('\t');
          file.readStringUntil('\n');

          if (touchY > yPos and touchY < yPos + 50){ // selected a device
            SETTINGS_page = "Bluetooth-Manage";
            scr_removeLayer("");
            SETTINGS_START();
          }

          yPos += 50;
        }
        closeFile(file);
      }
    }
  }


  if (SETTINGS_page == "Bluetooth-Manage" and touch.dataAvailable()) {

    if (touchGetX() < 52 and touchGetY() < 60 and touch.dataAvailable()) { // press back button
      SETTINGS_page = "Bluetooth";
      scr_removeLayer("");
      SETTINGS_START();
    }

    if (touchGetY() > screen.getDisplayYSize() - 200 and touchGetY() < screen.getDisplayYSize() - 100){
      if (touchGetX() < screen.getDisplayXSize() / 2){
        bluetooth_connect(SETTINGS_enteredMAC);
      }
      else{
        fileRemoveLineStartingWith(String("/MPOS/S/") + "BT/SAVE.MRT", SETTINGS_enteredMAC);
        SETTINGS_page = "Bluetooth";
        scr_removeLayer("");
        SETTINGS_START();
      }
    }
  }


  if (SETTINGS_page == "Bluetooth-Pair") {


    if (SETTINGS_newBTFound) {
      SETTINGS_newBTFound = false;
      File file;
      if (openFile(file, "S/BT/NEARBY.MRT", FILE_READ)){
        String mac;
        while (file.available()) {mac = file.readStringUntil('\n'); }
        print("btdevice", 0, 0, 0, 240, 240, 240, 20, 180 + 40 * SETTINGS_listSize, 0, "medium", mac);
        drawLine("btdevice", 100, 100, 100, 0, 210 + 40 * SETTINGS_listSize, screen.getDisplayXSize(), 210 + 40 * SETTINGS_listSize);
        SETTINGS_listSize += 1;
        closeFile(file);
      }
    }

    if (touch.dataAvailable()){

      File file;
      if (touchGetX() < 52 and touchGetY() < 60) { // press back button
        bluetooth_stop_scan();
        SETTINGS_page = "Network";
        scr_removeLayer("");
        SETTINGS_START();
      }

      else if (touchGetX() > 20 and touchGetY() > 90 and touchGetX() < screen.getDisplayXSize() / 2 - 10 and touchGetY() < 150) { // rescan
        bluetooth_scan();
        setColor(240, 240, 240);
        screen.fillRect(0, 180, screen.getDisplayXSize(), screen.getDisplayYSize());
        scr_removeLayer("btdevice");
        SETTINGS_listSize = 0;
      }

      else if (touchGetX() > screen.getDisplayXSize() / 2 + 10 and touchGetY() > 90 and touchGetX() < screen.getDisplayXSize() - 20 and touchGetY() < 150){ // pair other
        bluetooth_stop_scan();
        SETTINGS_page = "Bluetooth-Other";
        clearString(SETTINGS_enteredMAC);
        scr_removeLayer("");
        fillScr(240, 240, 240);
        print("", 0, 0, 0, 240, 240, 240, CENTER, 50, 0, "large", F("Enter MAC address"));
        print("", 0, 0, 0, 240, 240, 240, CENTER, 90, 0, "medium", F("Must be 12 HEX digits"));
      }

      else if (openFile(file, "S/BT/NEARBY.MRT", FILE_READ)) {
        unsigned int yPos = 170;
        bool found = false;
        while (file.available() and !found) {
          String mac = file.readStringUntil('\n');
          if (touchGetY() > yPos and touchGetY() < yPos + 40) {
            bluetooth_stop_scan();
            found = true;
            closeFile(file);
            bluetooth_connect(mac);

            bool isSaved = false;
            if (openFile(file, "S/BT/SAVE.MRT", FILE_READ)) {
              while (file.available() and !isSaved) {
                String savedMac = file.readStringUntil('\t');
                if (savedMac == mac) {
                  isSaved = true;
                }
                file.readStringUntil('\n');
              }
              closeFile(file);
            }

            if (!isSaved) {
              openFile(file, "S/BT/SAVE.MRT", FILE_WRITE);
              file.print(mac + "\tNo Name\n");
              closeFile(file);
              bluetooth_transmit_packet("NAME\n");
            }
          }

          yPos += 40;
        }
        closeFile(file);
      }
    }
  }

  if (SETTINGS_page == "Bluetooth-Other") {
    char key = keyboard_input();
    if ((key >= '0' and key <= '9') or (key >= 'A' and key <= 'F') or (key >= 'a' and key <= 'f') or key == '\1') {

      if (key == '\1') {
        if (SETTINGS_enteredMAC.length() > 0) {
          SETTINGS_enteredMAC.remove(SETTINGS_enteredMAC.length() - 1);
        }
      }

      else if (SETTINGS_enteredMAC.length() < 12) {
        SETTINGS_enteredMAC += key;
        SETTINGS_enteredMAC.toUpperCase();
      }

      String printName = SETTINGS_enteredMAC;
      while (printName.length() < 12) {
        printName += " ";
      }

      scr_removeLayer("name");
      print("name", 0, 0, 0, 240, 240, 240, 20, 365, 0, "medium", printName);
    }

    if (key == '\n' and SETTINGS_enteredMAC.length() == 12) {
      SETTINGS_page = "Bluetooth-Pair-Name";
      bluetooth_connect(SETTINGS_enteredMAC);
      bluetooth_transmit_packet("NAME\n");
      File file;
      openFile(file, "S/BT/SAVE.MRT", FILE_WRITE);
      file.print(SETTINGS_enteredMAC + "\tNo Name\n");
      closeFile(file);
      SETTINGS_page = "Network";
      scr_removeLayer("");
      SETTINGS_START();
    }
  }
  
  

  if (SETTINGS_page == "Processes") {

    if (SETTINGS_newRAMDataAvailable){
      SETTINGS_newRAMDataAvailable = false;
      scr_removeLayer("RAMChart");
      drawGraph("RAMChart", 20, 110, screen.getDisplayXSize()-20, 250, 30, 0, 100, String("/MPOS/S/") + "RAM.MRT");
      print("RAMChart", 0, 0, 0, 240, 240, 240, RIGHT, 90, 0, "medium", "  " + String(lastRAM) + "% ");
    }

    else if (SETTINGS_newLoadDataAvailable){
      SETTINGS_newLoadDataAvailable = false;
      scr_removeLayer("timeChart");
      drawGraph("timeChart", 20, 310, screen.getDisplayXSize()-20, 450, 30, 0, 1500, String("/MPOS/S/") + "LOAD.MRT");
      print("timeChart", 0, 0, 0, 240, 240, 240, RIGHT, 290, 0, "medium", "  " + String(lastSampleTime) + "ms ");
    }
    if (touch.dataAvailable()){
      
      if (touchGetX() < 52 and touchGetY() < 60) { // press back button
        clearString(SETTINGS_page);
        scr_removeLayer("");
        SETTINGS_START();
      }

      else if (touchGetY() > 475 and touchGetY() < 525 and touchGetX() > screen.getDisplayXSize()-75 and touchGetX() < screen.getDisplayXSize()-15){
        SD.remove(String("MPOS/S/SETTINGS/") + "TRACK.MRT");
        File setFile;
        openFile(setFile, "S/SETTINGS/TRACK.MRT", FILE_WRITE);
        if (ramTracking){
          ramTracking = false;
          setFile.print('N');
        }
        else{
          currentSampleLoopPasses = 0;
          SYS_RAMSampleTime = millis() + (sampleIntervals / 2);
          SYS_nextLoadSampleTime = millis() + sampleIntervals;
          SYS_loadSampleTime = millis();
          ramTracking = true;
          setFile.print('Y');
        }
        closeFile(setFile);
        scr_removeLayer("track-switch");
        on_off_input("track-switch", screen.getDisplayXSize() - 70, 500, ramTracking);
      }

      else if (touchGetY() > 525 and touchGetY() < 575){
        SETTINGS_page = "Background";
        scr_removeLayer("");
        SETTINGS_START();
      }

      else if (touchGetY() > 575 and touchGetY() < 625){
        SETTINGS_page = "Quit";
        scr_removeLayer("");
        SETTINGS_START();
      }
    }
  }

  if (SETTINGS_page == "Quit"){

    if (touchGetX() < 52 and touchGetY() < 60 and touch.dataAvailable()) { // press back button

      clearString(SETTINGS_page);
      scr_removeLayer("");
      SETTINGS_START();
    }

    else if (touch.dataAvailable()){
      unsigned int touchPos = touchGetY();
      File file;
      if (openFile(file, "S/D/APPS.MRT", FILE_READ)){
        int yPos = 100;
        while (file.available()){
          String appName = file.readStringUntil('\n');
          appName.toUpperCase();
          if (touchPos < yPos + 50 and touchPos > yPos){
            if (confirmation_message("Are you sure you", "want to quit", appName)){
              quit_app(&appName);
              refreshScreen();
            }
          }
          yPos += 50;
        }
        closeFile(file);
      }
    }
  }
  
  if (SETTINGS_page == "Background") {

    if (touch.dataAvailable()){
      if (touchGetX() < 52 and touchGetY() < 60 and touch.dataAvailable()) { // press back button
  
        SETTINGS_page = "Processes";
        scr_removeLayer("");
        SETTINGS_START();
      }
      else if (touchGetX() > screen.getDisplayXSize() - 75 ){
        File file;
        openFile(file, "S/SETTINGS/BACKGRD.MRT", FILE_READ);
        String backAllow = file.readString();
        closeFile(file);

        /*if (touchGetY() > 180 and touchGetY() < 230){
          scr_removeLayer("switch-SYS");
          if (backAllow.indexOf("SYS\n") >= 0){
            backAllow.remove(backAllow.indexOf("SYS\n"), 4);
            on_off_input("switch-SYS", screen.getDisplayXSize() -70, 205, false);
          }
          else{
            backAllow += "SYS\n";
            on_off_input("switch-SYS", screen.getDisplayXSize() -70, 205, true);
          }
        }*/
        if (openFile(file, "S/D/ABTASKS.MRT", FILE_READ)){
          unsigned int yPos = 180;
          while (file.available()){
            String appName = file.readStringUntil('\n');
            if (touchGetY() > yPos and touchGetY() < yPos + 50){
              //appName.toUpperCase();
              scr_removeLayer("switch-"+appName);
              if (backAllow.indexOf(appName+'\n') >= 0){
                backAllow.remove(backAllow.indexOf(appName + "\n"), appName.length()+1);
                on_off_input("switch-"+appName, screen.getDisplayXSize() -70, yPos + 25, false);
              }
              else{
                backAllow += appName + "\n";
                on_off_input("switch-"+appName, screen.getDisplayXSize() -70, yPos + 25, true);
              }
            }
            yPos += 50;
          }
          closeFile(file);
        }
        SD.remove(String("/MPOS/S/SETTINGS/") + "BACKGRD.MRT");
        openFile(file, "S/SETTINGS/BACKGRD.MRT", FILE_WRITE);
        file.print(backAllow);
        closeFile(file);
      }
    }
  }


  if (SETTINGS_page == "Time") {
    setColor(0, 0, 0);
    setBackColor(240, 240, 240);
    setFont("giant");
    screen.print(getTimeString(true), CENTER, 150);

    if (touch.dataAvailable()) {
      if (touchGetX() < 52 and touchGetY() < 60) { // press back button

        clearString(SETTINGS_page);
        scr_removeLayer("");
        SETTINGS_START();
      }

      bool changeDone = false;

      if (touchGetY() > 100 and touchGetY() < 140){
        if (touchGetX() > 130 and touchGetX() < 166){
          clockChangeHours(1);
          changeDone = true;
        }
        else if (touchGetX() > 220 and touchGetX() < 256){
          clockChangeMinutes(1);
          changeDone = true;
        }
        else if (touchGetX() > 320 and touchGetX() < 356){
          clockChangeSeconds(1);
          changeDone = true;
        }
      }

      else if (touchGetY() > 210 and touchGetY() < 250){
        if (touchGetX() > 130 and touchGetX() < 166){
          clockChangeHours(-1);
          changeDone = true;
        }
        else if (touchGetX() > 220 and touchGetX() < 256){
          clockChangeMinutes(-1);
          changeDone = true;
        }
        else if (touchGetX() > 320 and touchGetX() < 356){
          clockChangeSeconds(-1);
          changeDone = true;
        }
      }

      if (changeDone){ // pause between increments to prevent ridilulous over-corrected inputs
        byte counter = 200;
        while (counter > 0 and touch.dataAvailable()){
          delay(1);
          counter -= 1;
        }
      }

    }
    //clockChangeSeconds
  }


  if (SETTINGS_page == "Display") {

    if (touchGetX() < 52 and touchGetY() < 60 and touch.dataAvailable()) { // press back button

      clearString(SETTINGS_page);
      scr_removeLayer("");
      SETTINGS_START();
    }

    if (touchGetY() > 120 and touchGetY() < 140) { // brightness slider
      unsigned int newBright = touchGetX();
      if (newBright < 20) {
        newBright = 20;
      }
      if (newBright > screen.getDisplayXSize() - 20) {
        newBright = screen.getDisplayXSize() - 20;
      }


      brightnessPercent = map(newBright, 20, screen.getDisplayXSize() - 20, 10, 100);

      SD.remove(String("MPOS/S/SETTINGS/") + "bright.mrt");
      File setFile;
      openFile(setFile, "S/SETTINGS/BRIGHT.MRT", FILE_WRITE);
      setFile.print(brightnessPercent);
      closeFile(setFile);

      scr_removeLayer("brightness-scroll");
      fillRoundRect("brightness-scroll", 200, 200, 200, 20, 125, screen.getDisplayXSize() - 20, 135);

      unsigned int X_length = map(brightnessPercent, 10, 100, 20, screen.getDisplayXSize() - 20);

      fillRoundRect("brightness-scroll", 100, 50, 255, 20, 126, X_length, 134);
      fillCircle("brightness-scroll", 255, 255, 255, X_length, 130, 10);
      drawCircle("brightness-scroll", 0, 0, 0, X_length, 130, 11);

      refreshScreen();
    }

    if (touchGetX() > screen.getDisplayXSize() - 75 and touchGetY() > 170 and touchGetX() < screen.getDisplayXSize() - 15 and touchGetY() < 200) { // switch bluelight filter
      SD.remove(String("MPOS/S/SETTINGS/") + "blfilt.mrt");
      File setFile;
      openFile(setFile, "S/SETTINGS/BLFILT.MRT", FILE_WRITE);
      if (blueFilter == true) {
        setFile.print("N");
        blueFilter = false;
      }
      else {
        setFile.print("Y");
        blueFilter = true;
      }

      closeFile(setFile);

      scr_removeLayer("bluelight-filter-switch");
      on_off_input("bluelight-filter-switch", screen.getDisplayXSize() - 70, 185, blueFilter);
      refreshScreen();
    }

    if (touchGetX() > screen.getDisplayXSize() - 75 and touchGetY() > 220 and touchGetX() < screen.getDisplayXSize() - 15 and touchGetY() < 250) { // switch color invert
      SD.remove(String("MPOS/S/SETTINGS/") + "colinvrt.mrt");
      File setFile;
      openFile(setFile, "S/SETTINGS/COLINVRT.MRT", FILE_WRITE);
      if (invertColor == true) {
        setFile.print("N");
        invertColor = false;
      }
      else {
        setFile.print("Y");
        invertColor = true;
      }

      closeFile(setFile);

      scr_removeLayer("invert-color-switch");
      on_off_input("invert-color-switch", screen.getDisplayXSize() - 70, 235, invertColor);
      refreshScreen();
    }

    if (touchGetX() > screen.getDisplayXSize() - 75 and touchGetY() > 270 and touchGetX() < screen.getDisplayXSize() - 15 and touchGetY() < 300) { // switch dark mode
      SD.remove(String("MPOS/S/SETTINGS/") + "DARK.MRT");
      File setFile;
      openFile(setFile, "S/SETTINGS/DARK.MRT", FILE_WRITE);
      if (darkMode == true) {
        setFile.print("N");
        darkMode = false;
      }
      else {
        setFile.print("Y");
        darkMode = true;
      }

      closeFile(setFile);

      scr_removeLayer("dark-mode-switch");
      on_off_input("dark-mode-switch", screen.getDisplayXSize() - 70, 285, darkMode);
      refreshScreen();
    }


  }


  if (SETTINGS_page == "Sound") {

    if (touchGetX() < 52 and touchGetY() < 60 and touch.dataAvailable()) { // press back button
      clearString(SETTINGS_page);
      scr_removeLayer("");
      SETTINGS_START();
    }

    if (touchGetX() > screen.getDisplayXSize() - 75 and touchGetY() > 95 and touchGetX() < screen.getDisplayXSize() - 15 and touchGetY() < 125) { // mute switch
      SD.remove(String("MPOS/S/SETTINGS/") + "sound.mrt");
      File setFile;
      openFile(setFile, "S/SETTINGS/SOUND.MRT", FILE_WRITE);
      if (volume == true) {
        setFile.print("N");
        volume = false;
      }
      else {
        setFile.print("Y");
        volume = true;
      }
      closeFile(setFile);
      scr_removeLayer("mute-switch");
      on_off_input("mute-switch", screen.getDisplayXSize() - 70, 110, !volume);

      addNotification("test", F("volume change test"));
    }
  }
}



void SETTINGS_QUIT(){
  clearString(SETTINGS_page);
  clearString(SETTINGS_enteredMAC);
}











String FILES_selected = "";
unsigned int FILES_lastYPos = 0;
#define FILES_ROOT "/MPOS/F/"

void FILES_START() {
  scr_removeLayer("");
  
  if (appPathArg.startsWith("!")){
    appPathArg.remove(0, 1);
  }
  if ( ! (FILES_ROOT + appPathArg).endsWith("/")){
    appPathArg += "/";
  }
  if (!SD.exists(FILES_ROOT + appPathArg)){ // reset to root if directory doesn't exist
    addNotification("Files Error!", "The directory: '" + appPathArg + "' doesn't exist. As a consequence, you have been redirected to the root directory.");
    clearString(appPathArg);
    FILES_START();
    return;
  }
  
  fillScr(255, 255, 255);
  clearString(FILES_selected);
  FILES_lastYPos = 0;

  File dir = SD.open(FILES_ROOT + appPathArg, FILE_READ); // openFile() doesn't support directories yet
  //addFileToList(&dir);

  print("head", 0, 0, 0, 255, 255, 255, CENTER, 20, 0, "large", "Files");
  print("head", 0, 0, 0, 255, 255, 255, 10, 55, 0, "small", "/" + appPathArg);

  if (appPathArg != "") {
    fillRoundRect("head", 20, 30, 255, 20, 70, screen.getDisplayXSize()/2 - 10, 135);
    drawRoundRect("head", 0, 0, 0, 20, 70, screen.getDisplayXSize()/2 - 10, 135);
    print("head", 255, 255, 255, 20, 30, 255, 30, 95, 0, "medium", "Up Directory");
  }
  
  fillRoundRect("head", 20, 30, 255, screen.getDisplayXSize()/2 + 10, 70, screen.getDisplayXSize() - 20, 135);
  drawRoundRect("head", 0, 0, 0, screen.getDisplayXSize()/2 + 10, 70, screen.getDisplayXSize() - 20, 135);
  print("head", 255, 255, 255, 20, 30, 255, screen.getDisplayXSize()/2 + 20, 95, 0, "medium", "New Folder");

  int yPos = 130;

  while (true) {
    yPos += 50;
    drawLine("divider", 10, 10, 10, 0, yPos - 20, screen.getDisplayXSize(), yPos - 20);

    File entry =  dir.openNextFile();
    //addFileToList(&entry);
    
    if (! entry) {
      break;
    }

    String fName = entry.name();
    print("fileName", 0, 0, 0, 255, 255, 255, 50, yPos, 0, "medium", fName);

    if (entry.isDirectory()) { // draw icon next to name
      showBIM("fileName", String("/MPOS/S/") + "D/R/FOLDER.BIM", 10, yPos-5, 2, 2, 255, 255, 255, 50, 50, 100);
    } else {
      unsigned long fSize = entry.size();
      if (fSize > pow(2024, 9)){
        print("fileName", 0, 0, 0, 255, 255, 255, RIGHT, yPos, 0, "medium", String(fSize / pow(2024, 9), 2) + " GiB");
      }
      else if (fSize > pow(2024, 6)){
        print("fileName", 0, 0, 0, 255, 255, 255, RIGHT, yPos, 0, "medium", String(fSize / pow(2024, 6), 2) + " MiB");
      }
      else if (fSize > pow(2024, 3)){
        print("fileName", 0, 0, 0, 255, 255, 255, RIGHT, yPos, 0, "medium", String(fSize / pow(2024, 3), 2) + " KiB");
      }
      else{
        print("fileName", 0, 0, 0, 255, 255, 255, RIGHT, yPos, 0, "medium", String(fSize) + " bytes");
      }
    }
    closeFile(entry);
  }
  

  closeFile(dir);
}










void FILES() {
  
  if (!touch.dataAvailable()){
    return;
  }

  if (touchGetY() > 70 and touchGetY() < 135) {
    
    if (appPathArg != "" and touchGetX() > 20 and touchGetX() < screen.getDisplayXSize()/2 - 10) { // rewind directory
      appPathArg.remove(appPathArg.length()-1);
      if (appPathArg.lastIndexOf("/") >= 0){
        appPathArg.remove(appPathArg.lastIndexOf("/"));
      }
      else{
        clearString(appPathArg);
      }
      FILES_START();
      return;
    }
    
    if (touchGetX() > screen.getDisplayXSize()/2 + 10 and touchGetX() < screen.getDisplayXSize() - 20){ // new folder
      String newFolderName = "New";
      fillScr(255, 255, 255);
      print("newFolder", 0, 0, 0, 255, 255, 255, CENTER, 30, 0, "large", "New Folder");
      print("newFolder", 0, 0, 0, 255, 255, 255, CENTER, 70, 0, "medium", "Enter the name:");
      print("newFolder", 0, 0, 0, 255, 255, 255, CENTER, 87, 0, "small", "Maximum 8 characters");
      
      fillRoundRect("newFolder", 255, 200, 20, screen.getDisplayXSize() / 2 - 50, 250, screen.getDisplayXSize() / 2 + 50, 286);
      print("newFolder", 0, 0, 150, 255, 200, 20, CENTER, 252, 0, "large", "cancel");
  
      char keyIn = '\0';
      
      while (keyIn != '\n'){
        setColor(0, 0, 0);
        setBackColor(255, 255, 255);
        setFont("medium");
        screen.print(newFolderName + "        ", 30, 365, 0);

        
        if (touchGetX() > screen.getDisplayXSize() / 2 - 50 and touchGetX() < screen.getDisplayXSize() / 2 + 50 and touchGetY() > 250 and touchGetY() < 286) {
          // pressed cancel button
          setColor(200, 100, 0);
          setBackColor(200, 100, 20);
          screen.fillRoundRect(screen.getDisplayXSize() / 2 - 50, 250, screen.getDisplayXSize() / 2 + 50, 286);
          setColor(255, 255, 255);
          setFont("large");
          screen.print("Cancel", CENTER, 252);
    
          while (touch.dataAvailable());
          scr_removeLayer("newFolder");
          hide_keyboard();
          FILES_START();
          return;
        }
  
        
        if (keyIn == '\1'){
          if (newFolderName != ""){
            newFolderName.remove(newFolderName.length()-1);
          }
        }
        else if (keyIn != '\0' and newFolderName.length() < 8){
          newFolderName += keyIn;
        }
        
        keyIn = keyboard_input();
      }
  
      hide_keyboard();
      
      if (!SD.mkdir(FILES_ROOT + appPathArg + newFolderName)){
        addNotification("Files Error!", F("Failed to create new folder"));
      }
      FILES_START();
      return;
    }
  }
  
  if (touchGetY() > screen.getDisplayYSize() - 45 and touchGetY() < screen.getDisplayYSize() - 5){

    if (touchGetX() > 20 and touchGetX() < screen.getDisplayXSize()/2 - 30){
      
      if (FILES_selected.endsWith("/")){ // open folder
        appPathArg = FILES_selected;
        FILES_START();
        return;
      }

      else{ // return file to app
        if (allowedExt.indexOf(FILES_selected.substring(FILES_selected.length()-3, FILES_selected.length())) >= 0 or allowedExt == "" or allowedExt == "*"){
          appPathArg = FILES_ROOT + FILES_selected;
          if (previousApp != "HOME"){
            set_app(previousApp);
          }
          else{
            // search for default app (will be added later)
            addNotification("ERROR", F("You can't open files directly from Files app yet. Please open the app you want to edit the file with first"));
            while (touch.dataAvailable());
          }
        }
        return;
      }
    }

    if (touchGetX() > screen.getDisplayXSize()/2 + 30 and touchGetX() < screen.getDisplayXSize() - 20){ // delete
      if (confirmation_message("Are you sure you", "want to delete", "this file?")){
        if (FILES_selected.endsWith("/")) { // if folder
          if (!SD.rmdir(FILES_ROOT + FILES_selected)){
            addNotification("Files Error!", F("Couldn't delete the folder."));
          }
        }
        else{
          if (!SD.remove(FILES_ROOT + FILES_selected)){
            addNotification("Files Error!", F("Couldn't delete the file."));
          }
        }
        clearString(FILES_selected);
        FILES_START();
        return;
      }
    }
  }

  
  if (touchGetY() > screen.getDisplayYSize() - 90 and touchGetY() < screen.getDisplayYSize() - 50){

    if (touchGetX() > 20 and touchGetX() < screen.getDisplayXSize()/2 - 30 and allowedExt.indexOf("*") >= 0 and FILES_selected.endsWith("/")){ // return folder to app
      appPathArg = FILES_ROOT + FILES_selected;
      set_app(previousApp);
      return;
    }
  }
  
  
  unsigned int yPos = 130;
  
  File dir = SD.open(FILES_ROOT + appPathArg, FILE_READ); // openFile() doesn't support directories yet
  //addFileToList(&dir);
  
  while (true) { // loop for each file
    yPos += 50;

    File entry =  dir.openNextFile();
    if (! entry) {
      break;
    }
    //addFileToList(&entry);

    String fName = entry.name();

    if (touchGetY() > yPos - 20 and touchGetY() < yPos + 30){ // file selected
      FILES_selected = appPathArg + entry.name();

      if (FILES_lastYPos != 0) { // remove previous label
        scr_removeLayer("selected");
        setColor(255, 255, 255);
        screen.drawRoundRect(3, FILES_lastYPos - 18, screen.getDisplayXSize() - 3, FILES_lastYPos + 28);
        screen.drawRoundRect(4, FILES_lastYPos - 17, screen.getDisplayXSize() - 4, FILES_lastYPos + 27);
        screen.drawRoundRect(5, FILES_lastYPos - 16, screen.getDisplayXSize() - 5, FILES_lastYPos + 26);
      }

      FILES_lastYPos = yPos;
      drawRoundRect("selected", 0, 255, 30, 3, yPos - 18, screen.getDisplayXSize() - 3, yPos + 28);
      drawRoundRect("selected", 0, 255, 30, 4, yPos - 17, screen.getDisplayXSize() - 4, yPos + 27);
      drawRoundRect("selected", 0, 255, 30, 5, yPos - 16, screen.getDisplayXSize() - 5, yPos + 26);

      fillRoundRect("selected", 20, 30, 255, screen.getDisplayXSize()/2 + 30, screen.getDisplayYSize() - 45, screen.getDisplayXSize() - 20, screen.getDisplayYSize() - 5);
      drawRoundRect("selected", 0, 0, 0, screen.getDisplayXSize()/2 + 30, screen.getDisplayYSize() - 45, screen.getDisplayXSize() - 20, screen.getDisplayYSize() - 5);
      print("selected", 255, 255, 255, 20, 30, 255, screen.getDisplayXSize()/2 + 40, screen.getDisplayYSize() - 35, 0, "medium", "Delete");
      
      if (entry.isDirectory()) {
        FILES_selected += "/";
        
        fillRoundRect("selected", 20, 30, 255, 20, screen.getDisplayYSize() - 45, screen.getDisplayXSize()/2 - 30, screen.getDisplayYSize() - 5);
        drawRoundRect("selected", 0, 0, 0, 20, screen.getDisplayYSize() - 45, screen.getDisplayXSize()/2 - 30, screen.getDisplayYSize() - 5);
        print("selected", 255, 255, 255, 20, 30, 255, 30, screen.getDisplayYSize() - 35, 0, "medium", "Open Folder");

        if (allowedExt.indexOf("*") >= 0){
          fillRoundRect("selected", 20, 30, 255, 20, screen.getDisplayYSize() - 90, screen.getDisplayXSize()/2 - 30, screen.getDisplayYSize() - 50);
          drawRoundRect("selected", 0, 0, 0, 20, screen.getDisplayYSize() - 90, screen.getDisplayXSize()/2 - 30, screen.getDisplayYSize() - 50);
          print("selected", 255, 255, 255, 20, 30, 255, 30, screen.getDisplayYSize() - 75, 0, "medium", "Choose");
        }
      }
      
      else if (allowedExt.indexOf(fName.substring(fName.length()-4)) >= 0 or allowedExt == "" or allowedExt == "*"){ // normal files
        fillRoundRect("selected", 20, 30, 255, 20, screen.getDisplayYSize() - 45, screen.getDisplayXSize()/2 - 30, screen.getDisplayYSize() - 5);
        drawRoundRect("selected", 0, 0, 0, 20, screen.getDisplayYSize() - 45, screen.getDisplayXSize()/2 - 30, screen.getDisplayYSize() - 5);
        print("selected", 255, 255, 255, 20, 30, 255, 30, screen.getDisplayYSize() - 35, 0, "medium", "Open File");
      }
    }
    closeFile(entry);
  }
  closeFile(dir);
}

void FILES_QUIT(){
  clearString(FILES_selected);
}







String TEXT_newName = "NEW";
String TEXT_fileContent = "";
String TEXT_filePath = "";
int TEXT_cursorPosition = 0; // keep signed. may underflow when scrolling if unsigned
bool TEXT_editSinceLastSaved = false;
bool TEXT_editSinceLastFullSave = false;
unsigned int TEXT_blockNumber = 0; // if changing type, must also change underflow check in loadBlock()

const byte TEXT_BLOCK_SIZE = 80;

void TEXT_print(String file, int cursorPos, bool clearPrev = false);

void TEXT_START(){
  if (TEXT_filePath != ""){ // file already selected, initialize editor interface
    fillScr(255, 255, 255);
    print("head", 0, 0, 0, 255, 255, 255, CENTER, 50, 0, "xlarge", "Text Editor");
    print("head", 0, 0, 0, 255, 255, 255, CENTER, 100, 0, "small", "Editing: " + TEXT_filePath);
    fillRoundRect("buttons", 20, 60, 255, screen.getDisplayXSize()-60, 150, screen.getDisplayXSize()-2, 250);
    drawRoundRect("buttons", 0, 0, 0, screen.getDisplayXSize()-60, 150, screen.getDisplayXSize()-2, 250);
    fillRoundRect("buttons", 20, 60, 255, screen.getDisplayXSize()-60, 260, screen.getDisplayXSize()-2, 360);
    drawRoundRect("buttons", 0, 0, 0, screen.getDisplayXSize()-60, 260, screen.getDisplayXSize()-2, 360);
    fillRoundRect("buttons", 20, 60, 255, 20, 20, 90, 90);
    drawRoundRect("buttons", 0, 0, 0, 20, 20, 90, 90);
    print("buttons", 255, 255, 255, 20, 60, 255, 45, 35, 0, "large", "X");
    print("buttons", 255, 255, 255, 20, 60, 255, 35, 65, 0, "small", "close");

    showBIM("buttons", String("/MPOS/S/") + "D/R/UP.BIM", screen.getDisplayXSize()-50, 170, 4, 6, 20, 60, 255, 255, 255, 255);
    showBIM("buttons", String("/MPOS/S/") + "D/R/DOWN.BIM", screen.getDisplayXSize()-50, 280, 4, 6, 20, 60, 255, 255, 255, 255);
    
    TEXT_cursorPosition = 0;
    TEXT_print(TEXT_fileContent, TEXT_cursorPosition);
  }
  else if (appPathArg.endsWith("/")){ // folder selected for new file, user must create name
    fillScr(255, 255, 255);
    print("newFile", 0, 0, 0, 255, 255, 255, CENTER, 65, 0, "medium", "Enter a name:");
    print("newFile", 0, 0, 0, 255, 255, 255, CENTER, 87, 0, "small", "Maximum 8 characters");
    TEXT_newName = "NEW";
  }
  
  else if (!appPathArg.endsWith(".TXT")){
    fillScr(200, 200, 255);
    print("menu", 0, 0, 0, 200, 200, 255, CENTER, 50, 0, "xlarge", "Text Editor");
    print("menu", 0, 0, 0, 200, 200, 255, CENTER, 105, 0, "large", F("Please choose a file to edit"));
    fillRoundRect("menu", 255, 0, 0, 50, 200, screen.getDisplayXSize() - 50, 250);
    drawRoundRect("menu", 0, 0, 0, 50, 200, screen.getDisplayXSize() - 50, 250);
    fillRoundRect("menu", 255, 0, 0, 50, 300, screen.getDisplayXSize() - 50, 350);
    drawRoundRect("menu", 0, 0, 0, 50, 300, screen.getDisplayXSize() - 50, 350);
    print("menu", 0, 0, 0, 255, 0, 0, CENTER, 220, 0, "medium", "Choose existing file");
    print("menu", 0, 0, 0, 255, 0, 0, CENTER, 320, 0, "medium", "Create new file");
  }

  else if (!SD.exists(appPathArg)){
    addNotification("Text Editor ERROR!", "The file '" + appPathArg + "' doesn't exist.");
    clearString(appPathArg);
    TEXT_START();
  }

  else { // file just selected, get file ready
    TEXT_filePath = appPathArg;
    
/*
    TEXT_clearCache();
    File file = SD.open(TEXT_filePath, FILE_READ);
    //addFileToList(&file);

    unsigned int tfilenum = 0;

    // copy file into split temporary buffer files

    while (file.available()) {
      String tFileName = String("/MPOS/S/") + "TEXT/" + String(tfilenum) + ".txt";
      String buf = "";
      buf.reserve(TEXT_BLOCK_SIZE + 1);
      for (byte i=0; i<TEXT_BLOCK_SIZE; i++){
        if (file.available()){
          buf += char(file.read());
        }
      }

      File tempFile = SD.open(tFileName, FILE_WRITE);
      //addFileToList(&tempFile);
      tempFile.print(buf);
      closeFile(tempFile);
      tfilenum += 1;
    }

    //TEXT_fileContent = file.readString();
    //clearString(TEXT_fileContent);
    //while (file.available()){
    //  TEXT_fileContent += char(file.read());
    //}

    closeFile(file);
    TEXT_blockNumber = 0;
    TEXT_loadBlock();*/

    TEXT_blockNumber = 0;
    TEXT_loadFullFile();
    TEXT_START();
  }
}




void TEXT(){

  //while (true); // simulate crash


  if (TEXT_filePath == ""){
    if (appPathArg.endsWith("/")){ // folder selected for new file, user must create name

      setColor(0, 0, 0);
      setBackColor(255, 255, 255);
      setFont("medium");
      screen.print(TEXT_newName + "        ", 30, 365, 0);
      
      char keyIn = keyboard_input();
      
      if (keyIn == '\n'){
        hide_keyboard();
        appPathArg += TEXT_newName + ".TXT";
    
        File file;
        openFile(file, appPathArg, FILE_WRITE);
        closeFile(file);
  
        scr_removeLayer("");
        TEXT_START();
      }
      else if (keyIn == '\1'){
        if (TEXT_newName != ""){
          TEXT_newName.remove(TEXT_newName.length()-1);
        }
      }
      else if (keyIn != '\0' and TEXT_newName.length() < 8){
        TEXT_newName += keyIn;
      }
    }
    
    else if (!appPathArg.endsWith(".TXT")){
      if (touchGetX() > 50 and touchGetX() < screen.getDisplayXSize() - 50){
        if (touchGetY() > 200 and touchGetY() < 250){
          allowedExt = ".TXT";
          set_app("FILES");
        }
        if (touchGetY() > 300 and touchGetY() < 350){
          allowedExt = ".*";
          set_app("FILES");
        }
      }
    }
  }

  else{
    
    if (touch.dataAvailable()){
      
      if (touchGetX() > screen.getDisplayXSize()-60){
        if (touchGetY() > 150 and touchGetY() < 250){ // up button
          showBIM("buttons", String("/MPOS/S/") + "D/R/UP.BIM", screen.getDisplayXSize()-50, 170, 4, 6, 20, 60, 255, 0, 200, 0);
          unsigned int remainingSize = TEXT_saveBlock();
          TEXT_blockNumber -= 1;
          TEXT_loadBlock();
          TEXT_cursorPosition += TEXT_fileContent.length() - remainingSize;
          if (TEXT_cursorPosition > TEXT_fileContent.length()) {TEXT_cursorPosition = TEXT_fileContent.length();}
          TEXT_print(TEXT_fileContent, TEXT_cursorPosition, true);
          showBIM("buttons", String("/MPOS/S/") + "D/R/UP.BIM", screen.getDisplayXSize()-50, 170, 4, 6, 20, 60, 255, 255, 255, 255);
        }
        else if (touchGetY() > 260 and touchGetY() < 360){ // down button

          showBIM("buttons", String("/MPOS/S/") + "D/R/DOWN.BIM", screen.getDisplayXSize()-50, 280, 4, 6, 20, 60, 255, 0, 200, 0);

          TEXT_cursorPosition -= TEXT_saveBlock();
          TEXT_blockNumber += 1;
          TEXT_loadBlock();
          if (TEXT_cursorPosition < 0) {TEXT_cursorPosition = 0;}
          TEXT_print(TEXT_fileContent, TEXT_cursorPosition, true);

          showBIM("buttons", String("/MPOS/S/") + "D/R/DOWN.BIM", screen.getDisplayXSize()-50, 280, 4, 6, 20, 60, 255, 255, 255, 255);
        }
      }

      else if (touchGetX() > 20 and touchGetX() < 90 and touchGetY() > 20 and touchGetY() < 90){
        TEXT_saveFullFile();
        clearString(TEXT_fileContent);
        clearString(TEXT_filePath);
        clearString(appPathArg);
        TEXT_START();
        return;
      }

      else if (touchGetY() > 120 and touchGetY() < screen.getDisplayYSize()-400 and touchGetX() >= 10){
        TEXT_cursorPosition = TEXT_getPos(TEXT_fileContent, touchGetX(), touchGetY());
        TEXT_print(TEXT_fileContent, TEXT_cursorPosition);
      }
    }
    
    
    char keyIn = keyboard_input();

    if (keyIn == '\1'){
      if (TEXT_cursorPosition != 0){
        TEXT_fileContent.remove(TEXT_cursorPosition -1, 1);
        TEXT_cursorPosition -= 1;
        TEXT_editSinceLastSaved = true;
        TEXT_editSinceLastFullSave = true;
        TEXT_print(TEXT_fileContent, TEXT_cursorPosition);
      }
    }
    else if (keyIn != '\0'){
      TEXT_fileContent = TEXT_fileContent.substring(0, TEXT_cursorPosition) + keyIn + TEXT_fileContent.substring(TEXT_cursorPosition, TEXT_fileContent.length());
      TEXT_cursorPosition += 1;
      TEXT_editSinceLastSaved = true;
      TEXT_editSinceLastFullSave = true;
      TEXT_print(TEXT_fileContent, TEXT_cursorPosition);
    }

    if (TEXT_fileContent.length() > 3 * TEXT_BLOCK_SIZE){
      showBIM("buttons", String("/MPOS/S/") + "D/R/DOWN.BIM", screen.getDisplayXSize()-50, 280, 4, 6, 20, 60, 255, 0, 200, 0);
      TEXT_saveFullFile();
      TEXT_blockNumber += 1;
      TEXT_loadFullFile();
      TEXT_print(TEXT_fileContent, TEXT_cursorPosition, true);
      showBIM("buttons", String("/MPOS/S/") + "D/R/DOWN.BIM", screen.getDisplayXSize()-50, 280, 4, 6, 20, 60, 255, 255, 200, 255);
    }
    else if (TEXT_fileContent.length() == 0 and TEXT_blockNumber > 0){
      showBIM("buttons", String("/MPOS/S/") + "D/R/UP.BIM", screen.getDisplayXSize()-50, 170, 4, 6, 20, 60, 255, 0, 200, 0);
      TEXT_saveFullFile();
      TEXT_blockNumber -= 1;
      TEXT_loadFullFile();
      TEXT_print(TEXT_fileContent, TEXT_cursorPosition, true);
      showBIM("buttons", String("/MPOS/S/") + "D/R/UP.BIM", screen.getDisplayXSize()-50, 170, 4, 6, 20, 60, 255, 255, 255, 255);
    }
  }
}



void TEXT_BACKGROUND(){
  if (TEXT_filePath == ""){
    return;
  }
  if (TEXT_editSinceLastSaved){
    TEXT_saveBlock();
  }
}


void TEXT_print(String file, int cursorPos, bool clearPrev = false){
  unsigned int yOffset = 103;
  if (clearPrev){
    setColor(255, 255, 255);
    screen.fillRect(0, 120, screen.getDisplayXSize() -61, screen.getDisplayYSize() - 401);
  }
  file += "\n ";
  cursorPos += 1;
  setFont("medium");
  
  byte charsPerLine = (screen.getDisplayXSize() - 80) / screen.getFontXsize();

  setColor(255, 255, 255);
  screen.drawLine(screen.getFontXsize() * charsPerLine + 10, 120, screen.getFontXsize() * charsPerLine + 10, screen.getDisplayYSize() - 401);
  screen.drawLine(screen.getFontXsize() * charsPerLine + 11, 120, screen.getFontXsize() * charsPerLine + 11, screen.getDisplayYSize() - 401);
  
  setColor(0, 0, 0);
  setBackColor(255, 255, 255);
  
  while (file.length() > 0){
    yOffset += 17;
    String line = "";
    bool fullLine = false;
    
    if (file.indexOf('\n') >= 0 and file.indexOf('\n') < charsPerLine){
      line = file.substring(0, file.indexOf('\n'));
      file = file.substring(file.indexOf('\n') + 1, file.length());
      if (cursorPos > 0){
        cursorPos -= 1;
      }
    }

    else if (file.length() <= charsPerLine){
      line = file;
      clearString(file);
    }
    
    else{
      line = file.substring(0, charsPerLine);
      file = file.substring(charsPerLine, file.length());
      fullLine = true;
    }

    bool drawCursor = false;
    if (line.length() < cursorPos){
      cursorPos -= line.length();
    }
    else if (cursorPos >= 0) {
      drawCursor = true;
    }
    
    if (yOffset >= 120 and yOffset < screen.getDisplayYSize() - 416){
      while (line.length() < charsPerLine){
        line += " ";
      }
      screen.print(line, 10, yOffset, 0);

      if (drawCursor){
        if (fullLine){
          cursorPos -= 1;
        }
        setColor(30, 30, 180);
        screen.drawLine(screen.getFontXsize() * cursorPos + 10, yOffset, screen.getFontXsize() * cursorPos + 10, yOffset + 16);
        screen.drawLine(screen.getFontXsize() * cursorPos + 11, yOffset, screen.getFontXsize() * cursorPos + 11, yOffset + 16);
        setColor(0, 0, 0);
      }
    }
    
    if (drawCursor){
      cursorPos = -1;
    }
  }
}


void TEXT_clearCache(){
  bool searching = true;
  byte tfilenum = 0;
  while (searching){
    String tFileName = String("/MPOS/S/") + "TEXT/" + String(tfilenum) + ".txt";
    if (SD.exists(tFileName)){
      SD.remove(tFileName);
    }
    else{
      searching = false;
    }
    tfilenum += 1;
  }
}

unsigned int TEXT_getPos(String file, unsigned int x, unsigned int y){
  unsigned int yOffset = 103;
  x -= 10;
  yOffset += 17;
  x = x / screen.getFontXsize();
  setFont("medium");
  
  byte charsPerLine = (screen.getDisplayXSize() - 80) / screen.getFontXsize();
  unsigned int cursorPosition = 0;

  
  while (file.length() > 0){
    yOffset += 17;
    unsigned int lineLen = 0;
    
    if (file.indexOf('\n') >= 0 and file.indexOf('\n') < charsPerLine){
      lineLen = file.indexOf('\n') + 1;
      file = file.substring(file.indexOf('\n') + 1, file.length());
    }

    else if (file.length() <= charsPerLine){
      lineLen = file.length();
      clearString(file);
    }
    
    else{
      lineLen = charsPerLine;
      file = file.substring(charsPerLine, file.length());
    }
    
    if (yOffset >= y and yOffset <= y + 16){
      if (x > lineLen){
        return cursorPosition + lineLen;
      }
      else{
        return cursorPosition + x;
      }
    }

    cursorPosition += lineLen;
  }

  return cursorPosition;
}


unsigned int TEXT_saveBlock(){ // returns size of first file buffer size (out of the 2 used)

  if (!TEXT_editSinceLastSaved) {
    return TEXT_fileContent.length() / 2; // crude approx. but if no edits were made, user probably doesn't care about cursor moving when scrolling
  }

  String fileName1 = "S/TEXT/" + String(TEXT_blockNumber) + ".txt";
  String fileName2 = "S/TEXT/" + String(TEXT_blockNumber + 1) + ".txt";

  SD.remove(fileName1);

  if (TEXT_fileContent.length() < TEXT_BLOCK_SIZE){
    File file;
    openFile(file, fileName1, FILE_WRITE);
    file.print(TEXT_fileContent);
    closeFile(file);
    
    if (SD.exists(fileName2)){
      SD.remove(fileName2);
      openFile(file, fileName2, FILE_WRITE);
      file.close();
    }

    TEXT_editSinceLastSaved = false;
    return TEXT_fileContent.length();
  }

  else{
    SD.remove(fileName2);

    unsigned int splitIndex = TEXT_fileContent.length() / 2;
    File file;
    openFile(file, fileName1, FILE_WRITE);
    for (unsigned int i=0; i<splitIndex; i++){
      file.write(TEXT_fileContent[i]);
    }
    closeFile(file);

    openFile(file, fileName2, FILE_WRITE);
    for (unsigned int i=splitIndex; i<TEXT_fileContent.length(); i++){
      file.write(TEXT_fileContent[i]);
    }
    closeFile(file);
    TEXT_editSinceLastSaved = false;
    return splitIndex;
  }
}

void TEXT_saveFullFile(){
  if (!TEXT_editSinceLastFullSave){return;}

  TEXT_saveBlock();
  SD.remove(TEXT_filePath);

  File file;
  openFile(file, TEXT_filePath, FILE_WRITE);
  unsigned int i = 0;
  String buf = "";
  buf.reserve(TEXT_BLOCK_SIZE + 1);
  File tempFile;
  while (openFile(tempFile, "S/TEXT/" + String(i) + ".txt", FILE_READ)){
    buf = tempFile.readString();
    closeFile(tempFile);
    file.print(buf);
    i += 1;
  }
  closeFile(file);
  TEXT_editSinceLastFullSave = false;
}


void TEXT_loadFullFile(){
  TEXT_clearCache();
  File file;
  openFile(file, TEXT_filePath, FILE_READ);

  unsigned int tfilenum = 0;

  // copy file into split temporary buffer files

  while (file.available()) {
    String tFileName = String("/MPOS/S/") + "TEXT/" + String(tfilenum) + ".txt";
    String buf = "";
    buf.reserve(TEXT_BLOCK_SIZE + 1);
    for (byte i=0; i<TEXT_BLOCK_SIZE; i++){
      if (file.available()){
        buf += char(file.read());
      }
    }

    File tempFile;
    openFile(tempFile, tFileName, FILE_WRITE);
    tempFile.print(buf);
    closeFile(tempFile);
    tfilenum += 1;
  }

  closeFile(file);
  TEXT_loadBlock();
}


void TEXT_loadBlock(){ // load text from 2 cached sections into TEXT_fileContent String
  clearString(TEXT_fileContent);

  if (!SD.exists(String("/MPOS/S/") + "TEXT/" + String(TEXT_blockNumber+1) + ".txt")) {
    TEXT_blockNumber -= 1;
  }
  if (TEXT_blockNumber > 65500){ // if underflowed (because unsigned int)
    TEXT_blockNumber = 0;
  }

  for (byte i=0; i<2; i++){
    File file;
    if (openFile(file, "S/TEXT/" + String(TEXT_blockNumber + i) + ".txt", FILE_READ)){
      TEXT_fileContent += file.readString();
      closeFile(file);
    }
  }
}


void TEXT_QUIT(){
  if (TEXT_editSinceLastFullSave){
    TEXT_saveFullFile();
  }

  clearString(TEXT_fileContent);
  clearString(TEXT_filePath);
  clearString(TEXT_newName);
  TEXT_newName = "NEW";
  TEXT_cursorPosition = 0;
  TEXT_editSinceLastSaved = false;
  TEXT_editSinceLastFullSave = false;
  TEXT_blockNumber = 0;
  TEXT_clearCache();
}



// */

String RFID_mode = "";
String RFID_path = "";

void RFID_START(){
  scr_removeLayer("");
  fillScr(255, 255, 255);
  print("title", 0, 0, 0, 255, 255, 255, CENTER, 20, 0, "xlarge", "RFID Editor");

  if (RFID_mode == ""){
    fillRoundRect("options", 30, 60, 255, 30, 100, screen.getDisplayXSize()-30, 150);
    fillRoundRect("options", 30, 60, 255, 30, 170, screen.getDisplayXSize()-30, 220);
    fillRoundRect("options", 30, 60, 255, 30, 240, screen.getDisplayXSize()-30, 290);
    fillRoundRect("options", 30, 60, 255, 30, 310, screen.getDisplayXSize()-30, 360);
    
    drawRoundRect("options", 0, 0, 0, 30, 100, screen.getDisplayXSize()-30, 150);
    drawRoundRect("options", 0, 0, 0, 30, 170, screen.getDisplayXSize()-30, 220);
    drawRoundRect("options", 0, 0, 0, 30, 240, screen.getDisplayXSize()-30, 290);
    drawRoundRect("options", 0, 0, 0, 30, 310, screen.getDisplayXSize()-30, 360);
    
    print("options", 255, 255, 255, 30, 60, 255, CENTER, 115, 0, "medium", F("Download files from card"));
    print("options", 255, 255, 255, 30, 60, 255, CENTER, 185, 0, "medium", F("Upload file to card"));
    print("options", 255, 255, 255, 30, 60, 255, CENTER, 255, 0, "medium", F("Create keyboard shortcut"));
    print("options", 255, 255, 255, 30, 60, 255, CENTER, 325, 0, "medium", F("Erase card"));

    clearString(RFID_path);
  }

  else if (RFID_mode == "download"){
    if (RFID_path == ""){
      RFID_path = appPathArg;
      clearString(appPathArg);
    }
    
    print("header", 0, 0, 0, 255, 255, 255, 30, 100, 0, "large", F("Downloading files from card."));
    fillRoundRect("header", 20, 20, 255, 40, 140, 200, 180);
    print("header", 255, 255, 255, 20, 20, 255, 65, 145, 0, "large", "Cancel");
    print("header", 0, 0, 0, 255, 255, 255, 15, 185, 0, "small", "Path: " + RFID_path);
    print("instruct", 0, 0, 0, 255, 255, 255, CENTER, 600, 0, "medium", "Place the card behind the");
    print("instruct", 0, 0, 0, 255, 255, 255, CENTER, 625, 0, "medium", "device to continue.");
  }

  else if (RFID_mode == "upload"){
    if (RFID_path == ""){
      RFID_path = appPathArg;
      clearString(appPathArg);
    }
    
    print("header", 0, 0, 0, 255, 255, 255, 30, 100, 0, "large", F("Uploading file to card."));
    fillRoundRect("header", 20, 20, 255, 40, 140, 200, 180);
    print("header", 255, 255, 255, 20, 20, 255, 65, 145, 0, "large", "Cancel");
    print("header", 0, 0, 0, 255, 255, 255, 15, 185, 0, "small", "Path: " + RFID_path);
    print("instruct", 0, 0, 0, 255, 255, 255, CENTER, 600, 0, "medium", "Place the card behind the");
    print("instruct", 0, 0, 0, 255, 255, 255, CENTER, 625, 0, "medium", "device to continue.");
  }

  else if (RFID_mode == "keyboard"){
    if (RFID_path == ""){
      RFID_path = appPathArg;
      clearString(appPathArg);
    }
    
    print("header", 0, 0, 0, 255, 255, 255, 30, 100, 0, "large", F("Replacing keyboard macro."));
    fillRoundRect("header", 20, 20, 255, 40, 140, 200, 180);
    print("header", 255, 255, 255, 20, 20, 255, 65, 145, 0, "large", "Cancel");
    print("header", 0, 0, 0, 255, 255, 255, 15, 185, 0, "small", "Path: " + RFID_path);
    print("instruct", 0, 0, 0, 255, 255, 255, CENTER, 600, 0, "medium", "Place the card behind the");
    print("instruct", 0, 0, 0, 255, 255, 255, CENTER, 625, 0, "medium", "device to continue.");
  }

  else if (RFID_mode == "erase"){
    print("header", 0, 0, 0, 255, 255, 255, 30, 100, 0, "large", F("Erasing card data"));
    fillRoundRect("header", 20, 20, 255, 40, 140, 200, 180);
    print("header", 255, 255, 255, 20, 20, 255, 65, 145, 0, "large", "Cancel");
    print("instruct", 0, 0, 0, 255, 255, 255, CENTER, 600, 0, "medium", "Place the card behind the");
    print("instruct", 0, 0, 0, 255, 255, 255, CENTER, 625, 0, "medium", "device to continue.");
  }

  else{
    clearString(RFID_mode);
    RFID_START();
  }
}

void RFID(){
  
  if (RFID_mode == ""){
    
    if (!touch.dataAvailable()){
      return;
    }
    
    if (touchGetX() > 30 and touchGetX() < screen.getDisplayXSize()-30){

      if (touchGetY() > 100 and touchGetY() < 150){ // download from card
        RFID_mode = "download";
        fillRoundRect("popup", 255, 20, 20, 30, 200, screen.getDisplayXSize()-30, 310);
        drawRoundRect("popup", 255, 255, 255, 29, 199, screen.getDisplayXSize()-29, 311);
        drawRoundRect("popup", 0, 0, 0, 28, 198, screen.getDisplayXSize()-28, 312);
        print("popup", 0, 0, 255, 255, 20, 20, CENTER, 220, 0, "large", F("Please select a folder to"));
        print("popup", 0, 0, 255, 255, 20, 20, CENTER, 260, 0, "large", F("save the files to."));
        delay(2000);
        allowedExt = ".*";
        set_app("FILES");
      }
      if (touchGetY() > 170 and touchGetY() < 220){ // upload to card
        RFID_mode = "upload";
        fillRoundRect("popup", 255, 20, 20, 30, 200, screen.getDisplayXSize()-30, 310);
        drawRoundRect("popup", 255, 255, 255, 29, 199, screen.getDisplayXSize()-29, 311);
        drawRoundRect("popup", 0, 0, 0, 28, 198, screen.getDisplayXSize()-28, 312);
        print("popup", 0, 0, 255, 255, 20, 20, CENTER, 220, 0, "large", F("Please select a file to"));
        print("popup", 0, 0, 255, 255, 20, 20, CENTER, 260, 0, "large", F("upload to the card."));
        delay(2000);
        clearString(allowedExt);
        set_app("FILES");
      }
      if (touchGetY() > 240 and touchGetY() < 290){ // keyboard shortcut
        RFID_mode = "keyboard";
        allowedExt = ".TXT";
        set_app("FILES");
      }
      if (touchGetY() > 310 and touchGetY() < 360){ // erase card
        RFID_mode = "erase";
        RFID_START();
      }
    }
  }

  else{
    if (touch.dataAvailable()){
      if (touchGetX() > 40 and touchGetX() < 200 and touchGetY() > 140 and touchGetY() < 180){ // cancel button pressed
        clearString(RFID_mode);
        RFID_START();
        return;
      }
    }
    
    if (RFID_mode == "download"){
      if (mfrc522.PICC_IsNewCardPresent()) {
        if ( mfrc522.PICC_ReadCardSerial()) {
          setColor(10, 255, 20);
          screen.fillRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
          setColor(0, 0, 0);
          screen.drawRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
          setBackColor(10, 255, 20);
          setFont("medium");
          screen.print("Reading...", CENTER, 610);
          
          RFIDCardDataToFile();
          File cardFile;
          openFile(cardFile, "S/RFID.MRT", FILE_READ);
          File file;

          byte fileNum = 0;
          bool copying = false;
          String search1 = "";
          String search2 = "";
          while (cardFile.available()){
            search1 = search2;
            search2 = "";
            for (byte i=0; i<20; i++){
              if (cardFile.available()){
                search2 += char(cardFile.read());
              }
            }
            
            if ( (search1+search2).indexOf("<file " + String(fileNum) + "<name: ") >= 0){
              String search = search1 + search2;
              for (byte i=0; i<40; i++){
                if (cardFile.available()){
                  search += char(cardFile.read());
                }
              }
              
              String fName = search.substring(search.indexOf("<file " + String(fileNum) + "<name: ") + 14, search.indexOf("<end fileName " + String(fileNum) + "><content: "));
              if (fileNum >= 10){
                fName.remove(0, 1);
              }
              SD.remove(RFID_path + fName);
              if (openFile(file, RFID_path + fName, FILE_WRITE)){
                copying = true;
                search.remove(0, search.indexOf("<end fileName " + String(fileNum) + "><content: ") + 26);
                if (fileNum >= 10){
                  search.remove(0, 1);
                }
                search1 = "";
                search2 = search;
              }
              else{
                addNotification(F("RFID download error!"), "The file '" + fName + "' could not be saved.");
              }
            }
            
            if ( (search1+search2).indexOf("<endContent " + String(fileNum) + ">>") >= 0){
              file.print((search1+search2).substring(0, (search1+search2).indexOf("<endContent " + String(fileNum) + ">>")));
              closeFile(file);
              copying = false;
              fileNum += 1;
            }

            if (copying){
              file.print(search1);
            }
          }
          

          if (file){
            closeFile(file);
          }
          closeFile(cardFile);
          mfrc522.PICC_HaltA();
          mfrc522.PCD_StopCrypto1();
          
          addNotification(F("RFID download complete"), String(fileNum) + " files have been downloaded from the card.");

          clearString(RFID_mode);
          clearString(RFID_path);
          RFID_START();
        }
      }
    }
  
    else if (RFID_mode == "upload"){
      if (mfrc522.PICC_IsNewCardPresent()) {
        if ( mfrc522.PICC_ReadCardSerial()) {
          File file;
          if (openFile(file, RFID_path, FILE_READ)){
            setColor(10, 255, 20);
            screen.fillRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
            setColor(0, 0, 0);
            screen.drawRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
            setBackColor(10, 255, 20);
            setFont("medium");
            screen.print("Writing...", CENTER, 610);
            
            RFIDCardDataToFile();
            File cardFile;
            openFile(cardFile, "S/RFID.MRT", FILE_READ);
            
            byte fileNum = 0;

            String check1 = "";
            String check2 = "";
            while (cardFile.available()){
              check1 = check2;
              for (byte i=0; i<20; i++){
                if (cardFile.available()){
                  check2 += char(cardFile.read());
                }
              }

              if ( (check1+check2).indexOf("<file " + String(fileNum) + "<name: ") >= 0){
                fileNum += 1;
              }
            }

            closeFile(cardFile);

            openFile(cardFile, "S/RFID.MRT", FILE_WRITE);
            
            cardFile.print("<file ");
            cardFile.print(fileNum);
            cardFile.print("<name: ");
            cardFile.print(file.name());
            cardFile.print("<end fileName ");
            cardFile.print(fileNum);
            cardFile.print("><content: ");

            while (file.available()){
              String section = "";
              
              for (byte i=0; i<20; i++){
                if (file.available()){
                  section += char(file.read());
                }
              }
              cardFile.print(section);
            }
            
            closeFile(file);
            cardFile.print("<endContent ");
            cardFile.print(fileNum);
            cardFile.print(">>");
            closeFile(cardFile);
            
            if (RFIDwriteCardDataFromFile()){
              setColor(10, 255, 20);
              screen.fillRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
              setColor(0, 0, 0);
              screen.drawRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
              setBackColor(10, 255, 20);
              screen.print("Done!", CENTER, 610);
              delay(500);
              
              clearString(RFID_mode);
              clearString(RFID_path);
              RFID_START();
            }
            else{
              setColor(255, 10, 10);
              screen.fillRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
              setColor(0, 0, 0);
              screen.drawRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
              setBackColor(255, 10, 10);
              setColor(255, 255, 255);
              screen.print("Failed!", CENTER, 610);
              delay(500);
            }
            
            mfrc522.PICC_HaltA();
            mfrc522.PCD_StopCrypto1();
          }
          else {
            addNotification("Upload ERROR!", "ERROR: The file '" + RFID_path + "' doesn't exist.");
          }
        }
      }
    }
  
    else if (RFID_mode == "keyboard"){
      if (mfrc522.PICC_IsNewCardPresent()) {
        if ( mfrc522.PICC_ReadCardSerial()) {
          setColor(10, 255, 20);
          screen.fillRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
          setColor(0, 0, 0);
          screen.drawRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
          setBackColor(10, 255, 20);
          setFont("medium");
          screen.print("Writing...", CENTER, 610);
          
          RFIDCardDataToFile();
          removeFromFile(String("/MPOS/S/") + "RFID.MRT", "<keyMacro:", "keyMacro>");
          File cardFile;
          openFile(cardFile, "S/RFID.MRT", FILE_WRITE);
          
          File file;
          if (openFile(file, RFID_path, FILE_READ)){
            cardFile.print("<keyMacro:");
            
            while (file.available()){
              String section = "";
              for (byte i=0; i<16 and file.available(); i++){
                section += char(file.read());
              }
              cardFile.print(section);
            }
            
            cardFile.print("keyMacro>");
            closeFile(file);
            closeFile(cardFile);
            
            if (RFIDwriteCardDataFromFile()){
              setColor(10, 255, 20);
              screen.fillRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
              setColor(0, 0, 0);
              screen.drawRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
              setBackColor(10, 255, 20);
              screen.print("Done!", CENTER, 610);
              delay(500);
  
              clearString(RFID_mode);
              clearString(RFID_path);
              RFID_START();
            }
            else{
              setColor(255, 10, 10);
              screen.fillRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
              setColor(0, 0, 0);
              screen.drawRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
              setBackColor(255, 10, 10);
              setColor(255, 255, 255);
              screen.print("Failed!", CENTER, 610);
              delay(500);
            }
            
            mfrc522.PICC_HaltA();
            mfrc522.PCD_StopCrypto1();
          }
          else{
            closeFile(cardFile);
          }
        }
      }
    }
  
    else if (RFID_mode == "erase"){
      if (mfrc522.PICC_IsNewCardPresent()) {
        if ( mfrc522.PICC_ReadCardSerial()) {
          setColor(10, 255, 20);
          screen.fillRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
          setColor(0, 0, 0);
          screen.drawRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
          setBackColor(10, 255, 20);
          setFont("medium");
          screen.print("Writing...", CENTER, 610);
          
          SD.remove(String("/MPOS/S/") + "RFID.MRT"); // create blank file
          File cardFile;
          openFile(cardFile, "S/RFID.MRT", FILE_WRITE);
          closeFile(cardFile);
          
          if (RFIDwriteCardDataFromFile()){
            setColor(10, 255, 20);
            screen.fillRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
            setColor(0, 0, 0);
            screen.drawRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
            setBackColor(10, 255, 20);
            screen.print("Done!", CENTER, 610);
            delay(500);
              
            clearString(RFID_mode);
            RFID_START();
          }
          else{
            setColor(255, 10, 10);
            screen.fillRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
            setColor(0, 0, 0);
            screen.drawRoundRect(40, 590, screen.getDisplayXSize()-40, 645);
            setBackColor(255, 10, 10);
            setColor(255, 255, 255);
            screen.print("Failed!", CENTER, 610);
            delay(500);
          }
          
          mfrc522.PICC_HaltA();
          mfrc522.PCD_StopCrypto1();
        }
      }
    }
  
    else{
      clearString(RFID_mode);
      RFID_START();
    }
  }
}


void RFID_QUIT(){
  clearString(RFID_mode);
  clearString(RFID_path);
}












void BACKGROUND_NOTIFS(){
  
  File notifFile;
  openFile(notifFile, "S/NOTIF.MRT", FILE_READ);
  if (notifFile.available()) {
    String notifTitle = notifFile.readStringUntil('\n');
    String notifDescription = notifFile.readStringUntil('\n');

    if (showNotification(notifTitle, notifDescription)) {

      // remove displayed notification from file
      SD.remove(String("/MPOS/S/") + "NOTIFT.MRT");
      File tempFile;
      openFile(tempFile, "S/NOTIFT.MRT", FILE_WRITE);
      while (notifFile.available()) {
        String line = notifFile.readStringUntil('\n');
        tempFile.print(line);
        tempFile.print('\n');
      }
      closeFile(notifFile);
      closeFile(tempFile);
      SD.remove(String("/MPOS/S/") + "NOTIF.MRT");

      openFile(tempFile, "S/NOTIFT.MRT", FILE_READ);
      openFile(notifFile, "S/NOTIF.MRT", FILE_WRITE);

      while (tempFile.available()) {
        String line = tempFile.readStringUntil('\n');
        notifFile.print(line);
        notifFile.print('\n');
      }
      closeFile(tempFile);
    }
  }
  closeFile(notifFile);


  
  if (lastNotification < millis() - 6000 and lastNotification > 0) {
    scr_removeLayer("notification");
    refreshScreen();
    lastNotification = 0;
  }
}

void BACKGROUND_TEMP_MONITOR(){

  if (getInternalTemp() > 40) {
    fillScr(200, 0, 0);
    setColor(255, 255, 255);
    setBackColor(200, 0, 0);
    setFont("xlarge");
    screen.print(F("Danger: Overheating!"), CENTER, 100);
    setFont("large");
    screen.print(F("Your device is overheating"), CENTER, 300);
    screen.print(F("and is going to shut down"), CENTER, 330);
    screen.print(F("to avoid damage."), CENTER, 360);
    
    delay(5000);
    shut_down();
  }
}

void BACKGROUND_RAM_MONITOR(){

  if (ramTracking){
    if (SYS_RAMSampleTime < millis()){
      SYS_RAMSampleTime += sampleIntervals;
      fileInsertStart(String("/MPOS/S/") + "RAM.MRT", String(currentRAM) + '\n', 10, 3);
      lastRAM = currentRAM;
      currentRAM = 0;
      sample_RAM();
      SETTINGS_newRAMDataAvailable = true;
    }


    if (SYS_nextLoadSampleTime < millis()){
      SYS_nextLoadSampleTime += sampleIntervals;
      unsigned long currentSampleTime = (millis()-SYS_loadSampleTime) / currentSampleLoopPasses;
      currentSampleLoopPasses = 0;
      fileInsertStart(String("/MPOS/S/") + "LOAD.MRT", String(currentSampleTime) + '\n', 10, 3);
      lastSampleTime = currentSampleTime;
      SYS_loadSampleTime = millis();
      sample_RAM();
      SETTINGS_newLoadDataAvailable = true;
    }
  }
}


void process_sound(){ // needs to run much more frequently than normal background task scheduling allows
  if (volume){
  
    File soundLogR;
    openFile(soundLogR, "S/SOUND.MRT", FILE_READ);

    if (soundLogR.available()) {
      unsigned long soundTime = soundLogR.readStringUntil(',').toInt();
      if (soundTime <= millis()) {
        unsigned int freq = soundLogR.readStringUntil(',').toInt();
        unsigned int duration = soundLogR.readStringUntil('\n').toInt();
        playTone(freq, duration);

        SD.remove(String("/MPOS/S/") + "SOUNDT.MRT");
        File soundLogW;
        openFile(soundLogW, "S/SOUNDT.MRT", FILE_WRITE);
        while (soundLogR.available()) {
          String line = soundLogR.readStringUntil('\n');
          soundLogW.print(line + '\n');
        }

        closeFile(soundLogR);
        closeFile(soundLogW);
        SD.remove(String("/MPOS/S/") + "SOUND.MRT");

        openFile(soundLogW, "S/SOUND.MRT", FILE_WRITE);
        openFile(soundLogR, "S/SOUNDT.MRT", FILE_READ);

        while (soundLogR.available()) {
          String line = soundLogR.readStringUntil('\n');
          soundLogW.print(line + '\n');
        }

        closeFile(soundLogR);
        closeFile(soundLogW);
        SD.remove(String("/MPOS/S/") + "SOUNDT.MRT");
      }
    }
    closeFile(soundLogR);
  }
}













void setup() {
  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, HIGH);
  pinMode(A13, OUTPUT); // bluetooth power
  pinMode(A11, OUTPUT); // bluetooth power
  pinMode(A10, OUTPUT); // bluetooth power
  bluetooth_power_off();
  pinMode(blu_EN, OUTPUT);
  digitalWrite(blu_EN, LOW);
  Wire.begin();
  Serial.begin(115200);  // Debug port
  Serial1.begin(115200); // bluetooth
  Serial2.begin(115200); // WiFi
  SPI.begin();






  initScreen(PORTRAIT);

  if (!SD.begin(53)) {
    fillScr(255, 0, 0);
    setColor(255, 255, 255);
    setBackColor(255, 0, 0);
    screen.setFont(Grotesk24x48);
    screen.print("ERROR", 100, 100);
    screen.setFont(BigFont);
    screen.print("SD card not found", 100, 300);
    delay(5000);
    digitalWrite(powerPin, LOW);
    while (true);
  }


  if (!SD.exists("/MPOS")) {
    fillScr(255, 0, 0);
    setColor(255, 255, 255);
    setBackColor(255, 0, 0);
    setFont("xlarge");
    screen.print("ERROR", CENTER, 100);
    setFont("medium");
    screen.print("SD card not set up", 100, 300);
    delay(5000);
    digitalWrite(powerPin, LOW);
    while (true);
  }



  File setFile;
  if (openFile(setFile, "S/SETTINGS/BRIGHT.MRT", FILE_READ)) {
    brightnessPercent = setFile.readString().toInt();
    closeFile(setFile);
  }

  if (openFile(setFile, "S/SETTINGS/BLFILT.MRT", FILE_READ)) {
    if (setFile.read() == 'Y') {
      blueFilter = true;
    }
    closeFile(setFile);
  }

  if (openFile(setFile, "S/SETTINGS/COLINVRT.MRT", FILE_READ)) {
    if (setFile.read() == 'Y') {
      invertColor = true;
    }
    closeFile(setFile);
  }
  

  //SPI.setClockDivider(SPI_CLOCK_DIV4);



  fillScr(255, 255, 255);

  showMCI("", String("/MPOS/S/") + "D/R/MCLOGO.MI2", 25, 350, 3, 3, false);


  if (openFile(setFile, "S/SETTINGS/SOUND.MRT", FILE_READ)) {
    if (setFile.read() == 'N') {
      volume = false;
    }
    closeFile(setFile);
  }

  if (openFile(setFile, "S/SETTINGS/NAME.MRT", FILE_READ)) {
    deviceName = setFile.readString();
    closeFile(setFile);
  }

  if (openFile(setFile, "S/SETTINGS/DARK.MRT", FILE_READ)) {
    if (setFile.read() == 'Y') {
      darkMode = true;
    }
    closeFile(setFile);
  }

  if (openFile(setFile, "S/SETTINGS/TRACK.MRT", FILE_READ)) {
    if (setFile.read() == 'N') {
      ramTracking = false;
    }
    closeFile(setFile);
  }

  if (openFile(setFile, "S/SETTINGS/BLUA.MRT", FILE_READ)) {
    if (setFile.read() == 'Y') {
      bluetoothActive = true;
      bluetoothPowerOn();
    }
    closeFile(setFile);
  }
  



  SD.remove(String("/MPOS/S/") + "SCREEN.MLI");
  SD.remove(String("/MPOS/S/") + "SOUNDT.MRT");
  SD.remove(String("/MPOS/S/") + "SOUND.MRT");
  SD.remove(String("/MPOS/S/") + "NOTIF.MRT");
  SD.remove(String("/MPOS/S/") + "RAM.MRT");
  SD.remove(String("/MPOS/S/") + "LOAD.MRT");
  SD.remove(String("/MPOS/S/") + "BT/NEARBY.MRT");
  SD.remove(String("/MPOS/S/") + "RECOV.MRT");

  //SD.remove(String("/MPOS/S/") + "BT/SAVE.MRT"); // temporary line

  openFile(setFile, "S/SCREEN.MLI", FILE_WRITE);
  closeFile(setFile);
  openFile(setFile, "S/SOUND.MRT", FILE_WRITE);
  closeFile(setFile);
  openFile(setFile, "S/NOTIF.MRT", FILE_WRITE);
  closeFile(setFile);


  setColor(0, 20, 30);
  setBackColor(255, 255, 255);
  screen.setFont(BigFont);







  mfrc522.PCD_Init();
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }




  //screen.print("                               ", CENTER, 500);
  touch.setPrecision(PREC_MEDIUM);

  /*playTone(200, 500);
  delay(500);
  delay(100);

  playTone(350, 550);
  delay(550);
  delay(100);

  playTone(550, 1000);
  delay(1000);
  */


  if (SD.exists(String("/MPOS/S/") + "D/password.mrt")) { // is set up, phone used before


    bool authenticated = false;
    bool askingPass = false;
    fillScr(0, 0, 0);

    setColor(200, 200, 200);
    screen.fillRoundRect(20, 580, screen.getDisplayXSize() - 20, 620);
    screen.fillRoundRect(20, screen.getDisplayYSize() - 100, screen.getDisplayXSize() - 20, screen.getDisplayYSize() - 50);
    //screen.fillRoundRect(20, 680, screen.getDisplayXSize() - 20, 720);
    //setColor(255, 255, 255);
    //screen.drawRoundRect(20, 580, screen.getDisplayXSize() - 20, 620);
    //screen.drawRoundRect(20, 680, screen.getDisplayXSize() - 20, 720);

    lastTouchRead = millis() - 100; // start shutdown timer at 0 without affecting touchGetX() outputs

    while (!authenticated) {

      if (askingPass) {
        int passResult = askForPass();
        if (passResult == 2) {
          playTone(300, 60);
          delay(100);
          playTone(400, 50);
          delay(50);
          playTone(500, 200);
          authenticated = true;
          hide_keyboard();
        }
        if (passResult == 1) {
          playTone(300, 60);
          delay(100);
          playTone(400, 50);
          delay(50);
          playTone(300, 200);
          showPassInputScreen();
        }
        if (passResult == 3) {
          fillScr(0, 0, 0);
          setColor(200, 200, 200);
          screen.fillRoundRect(20, 580, screen.getDisplayXSize() - 20, 620);
          screen.fillRoundRect(20, screen.getDisplayYSize() - 100, screen.getDisplayXSize() - 20, screen.getDisplayYSize() - 50);
          //screen.fillRoundRect(20, 680, screen.getDisplayXSize() - 20, 720);
          //setColor(255, 255, 255);
          //screen.drawRoundRect(20, 580, screen.getDisplayXSize() - 20, 620);
          //screen.drawRoundRect(20, 680, screen.getDisplayXSize() - 20, 720);
          askingPass = false;
        }
      }
      else {
        setColor(255, 255, 255);
        setBackColor(0, 0, 0);
        screen.setFont(BigFont);
        screen.print(getDateString(), CENTER, 180);
        screen.setFont(segment18_XXL);
        screen.print(getTimeString(), CENTER, 100);

        unsigned int loadBarPos = map(millis() - lastTouchRead, 0, 20000, 30, screen.getDisplayXSize() - 30);
        setColor(200, 200, 200);
        screen.fillRect(loadBarPos - 3, screen.getDisplayYSize() - 90, screen.getDisplayXSize() - 30, screen.getDisplayYSize() - 60);

        screen.setFont(BigFont);
        setBackColor(200, 200, 200);
        setColor(random(0, 255), random(0, 255), random(0, 255));
        screen.print(F("Click here to unlock"), CENTER, 595);
        //screen.print(F("Click here to shut down"), CENTER, 695);
        screen.fillRoundRect(30, screen.getDisplayYSize() - 90, loadBarPos, screen.getDisplayYSize() - 60);

        if (touch.dataAvailable()) {
          if (touchGetY() > 580 and touchGetY() < 620) {
            askingPass = true;
          }
        }

        if (millis() - lastTouchRead > 20000) { // shut down if not touched in 20 seconds
          digitalWrite(powerPin, LOW);
          while (true);
        }
      }
    }
  }






  else { // has been reset to factory settings or phone is new
    fillScr(255, 255, 255);
    setColor(0, 0, 0);
    setBackColor(255, 255, 255);



    screen.setFont(Grotesk24x48);
    screen.print("HELLO", CENTER, 50);
    setFont("large");
    screen.print(F("This is your new MaxPhone 2"), CENTER, 200);
    setFont("medium");
    screen.print(F("You will be guided through the"), CENTER, 350);
    screen.print(F("steps to set up your phone"), CENTER, 366);

    while (!touch.dataAvailable()) {
      setFont("medium");
      setColor(random(0, 255), random(0, 255), random(0, 255));
      screen.print(F("Touch anywhere to begin"), CENTER, screen.getDisplayYSize() - 30);
    }

    fillScr(255, 255, 255);
    setColor(0, 0, 50);
    setBackColor(255, 255, 255);
    setFont("medium");
    screen.print(F("Please create a password"), CENTER, CENTER);
    delay(3000);


    while (create_new_pass());

  }

  unsigned long t = millis();
  SYS_RAMSampleTime = t + (sampleIntervals / 2);
  SYS_nextLoadSampleTime = t + sampleIntervals;
  SYS_loadSampleTime = t;

  set_app("HOME");
  //addNotification("Test Notifiation", "test");
  lastBackgroundTime = millis(); // prevent immediate watchdog trigger
  WatchDog::init(WDT_trigger, 500);


  //int* dummy = (int*) malloc(freeMemory() - 150);
  //emergencyFreePointer = dummy;
  //Serial.println(freeMemory());
  //free(dummy);
  //fix28135_malloc_bug();

  //emergencyFreePointer = (int*) malloc(800);
  Serial.print("Initial Free Memory: ");
  Serial.println(freeMemory());
  //printFullMemory();
  //for (byte i=0; i<20; i++){
  //  Serial.println();
  //}
}













void loop() {

  handle_jmp(); // must be kept in seperate function, otherwise creates massive RAM buffer for whole loop

  //interrupts();

  //Serial.println("test");



  // execute the open app
  
  if (CURRENT_APP == "HOME") {
    CONTROLLING_APP = "HOME";
    HOMESCREEN();
    CONTROLLING_APP = "SYS";
  }
  else if (CURRENT_APP == "SETTINGS") {
    CONTROLLING_APP = "SETTINGS";
    SETTINGS();
    CONTROLLING_APP = "SYS";
  }
  else if (CURRENT_APP == "FILES") {
    CONTROLLING_APP = "FILES";
    FILES();
    CONTROLLING_APP = "SYS";
  }
  else if (CURRENT_APP == "TEXT") {
    CONTROLLING_APP = "TEXT";
    TEXT();
    CONTROLLING_APP = "SYS";
  }
  else if (CURRENT_APP == "RFID") {
    CONTROLLING_APP = "RFID";
    RFID();
    CONTROLLING_APP = "SYS";
  }



  // do one background task, and alternate which one gets executed
  // space with no tasks in between tasks are to improve speed of foreground app
  
  if (millis() - lastBackgroundTime > 200){
    run_background();
  }


  process_sound();



  if (lastTopBarUpdate + 10000 < millis()) {
    showTopBar();
  }





  if (CURRENT_APP != "HOME") {
    if (touchGetX() > screen.getDisplayXSize() / 2 - 23 and touchGetX() < screen.getDisplayXSize() / 2 + 23 and touchGetY() > screen.getDisplayYSize() - 48 and touchGetY() < screen.getDisplayYSize() + 48) {
      set_app("HOME"); // home button pressed
    }
  }





  if (bluetoothActive and BTConnectedMAC != "" and millis() - BTLastPingSent > 8000) {
    Serial1.print("PING\n");
    BTLastPingSent = millis();
  }



  //testing touchscreen
  if (touch.dataAvailable()) {
    screen.fillCircle(touchGetX(), touchGetY(), 3);
  }



  if (Serial.available()) {
    String SerialReadData = Serial.readStringUntil('\n');
    if (SerialReadData == "screenDump") {
      dumpScreen();
    }

    else if (SerialReadData == "Shut Down") {
      shut_down();
    }

    else if (SerialReadData == "ATCONNECT") {
      bluetooth_enter_AT();
    }
    else if (SerialReadData == "ATDISCONNECT") {
      bluetooth_exit_AT();
    }
    else if (SerialReadData == "CONNECT"){
      Serial.println("Enter the mac address");
      while (!Serial.available());
      Serial.println("received");
      bluetooth_connect(Serial.readStringUntil('\n'));
    }

    else {
      if (SerialReadData.startsWith("AT+")) {SerialReadData += "\r";}
      Serial1.print(SerialReadData + "\n");
    }
  }



  if (Serial1.available()){
    String BTRecieved = Serial1.readStringUntil('\n');
    Serial.println(BTRecieved);


    if (BTRecieved.startsWith("+INQ:")) {
      BTRecieved.remove(0, 5);
      String mac = format_MAC(BTRecieved.substring(0, BTRecieved.indexOf(',')));
      File file;
      openFile(file, "S/BT/NEARBY.MRT", FILE_WRITE);
      file.print(mac + "\n");
      closeFile(file);
      Serial.println("found MAC: " + mac);
      SETTINGS_newBTFound = true;
    }

    else if (BTRecieved == "OK\r") {
      bluetooth_stop_scan();
    }

    else if (BTRecieved == "PING") {
      BTLastPing = millis();
      Serial1.print(BTSendBuffer);
      Serial.print(BTSendBuffer);
      clearString(BTSendBuffer);
      if (!topBarShowsBluetooth) {showTopBar();}
    }

    else if (BTRecieved.startsWith("NAME:") and BTConnectedMAC != "") {
      BTRecieved.remove(0, 5);
      BTRecieved.replace('\t', ' ');
      fileRemoveLineStartingWith(String("/MPOS/S/") + "BT/SAVE.MRT", BTConnectedMAC);
      File file;
      openFile(file, "S/BT/SAVE.MRT", FILE_WRITE);
      file.print(BTConnectedMAC + "\t" + BTRecieved + "\n");
      closeFile(file);
      Serial.println("bluetooth name set: " + BTRecieved);
    }
  }




  if (topBarShowsBluetooth and millis() - BTLastPing >= 10000) { // hide bluetooth icon when disconnected
    showTopBar();
  }


  // measurement of CPU load based off how much time passed in one loop
  if (ramTracking){
    currentSampleLoopPasses += 1;
  }
}
