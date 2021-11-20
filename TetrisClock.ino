/*******************************************************************
    Tetris clock using a 64x32 RGB Matrix that fetches its
    time over WiFi using the EzTimeLibrary.
    For use with my I2S Matrix Shield.
    Parts Used:
    ESP32 D1 Mini * - https://s.click.aliexpress.com/e/_dSi824B
    ESP32 I2S Matrix Shield (From my Tindie) = https://www.tindie.com/products/brianlough/esp32-i2s-matrix-shield/
      = Affilate
    If you find what I do useful and would like to support me,
    please consider becoming a sponsor on Github
    https://github.com/sponsors/witnessmenow/
    Written by Brian Lough
    YouTube: https://www.youtube.com/brianlough
    Tindie: https://www.tindie.com/stores/brianlough/
    Twitter: https://twitter.com/witnessmenow
 *******************************************************************/
// ----------------------------
// Standard Libraries - Already Installed if you have ESP32 set up
// ----------------------------

#include <FS.h>
#include <SPIFFS.h>

// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
// Captive portal for configuring the WiFi

const int PIN_LED       = 2;
bool      initialConfig = false;


#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
// This is the library for interfacing with the display

// Can be installed from the library manager (Search for "ESP32 MATRIX DMA")
// https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA

#include <TetrisMatrixDraw.h>
// This library draws out characters using a tetris block
// amimation
// Can be installed from the library manager
// https://github.com/toblum/TetrisAnimation

#include <ezTime.h>
// Library used for getting the time and adjusting for DST
// Search for "ezTime" in the Arduino Library manager
// https://github.com/ropg/ezTime

#include <ArduinoJson.h>
// ArduinoJson is used for parsing and creating the config file.
// Search for "Arduino Json" in the Arduino Library manager
// https://github.com/bblanchon/ArduinoJson

// ----------------------------
// Dependancy Libraries - each one of these will need to be installed.
// ----------------------------

// Adafruit GFX library is a dependancy for the matrix Library
// Can be installed from the library manager
// https://github.com/adafruit/Adafruit-GFX-Library

// -------------------------------------
// -------   Matrix Config   ------
// -------------------------------------

#define PANEL_RES_X 64      // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 32     // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1      // Total number of panels chained one to another

// -------------------------------------
// -------   Clock Config   ------
// -------------------------------------

// Sets whether the clock should be 12 hour format or not.
//bool twelveHourFormat = true;

// If this is set to false, the number will only change if the value behind it changes
// e.g. the digit representing the least significant minute will be replaced every minute,
// but the most significant number will only be replaced every 10 minutes.
// When true, all digits will be replaced every minute.
//bool forceRefresh = false;
// -----------------------------

// Set a timezone using the following list
// https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
#define MY_TIME_ZONE "America/New_York"

#define TIME_ZONE_LEN 25

#define TIME_ZONE_LABEL "timeZone"
#define MATRIX_24H_LABEL "is24h"
#define MATRIX_FORCE_LABEL "isForce"
#define MATRIX_MADE_FOR_LABEL "madeFor"
#define MATRIX_MADE_BY_LABEL "madeBy"
#define MATRIX_HOSTNAME_LABEL "hostname"
#define MATRIX_MSG_LINE1 "msgLine1"
#define MATRIX_MSG_LINE2 "msgLine2"
#define MATRIX_MSG_LINE3 "msgLine3"
#define MATRIX_MSG_LINE4 "msgLine4"
#define TIME_ANIMATION_DELAY_LABEL   //crbtemp  set to ??

char matrixMsgLine1[10];
char matrixMsgLine2[10];
char matrixMsgLine3[10];
char matrixMsgLine4[10];

char timeZone[TIME_ZONE_LEN] = MY_TIME_ZONE;
bool matrixClk = false;
bool matrixDriver = false;
bool matrixIs64 = false;
bool matrixIs24H = false;
bool matrixForceRefresh = false;
char matrixMadeFor[10] = "MADEFOR";
String madefor = "MADE FOR";
String madeby = "MADE BY";
char matrixMadeBy[10] = "MADEBY";
char matrixHostname[40] = "Tetris-WiFi-Clock";   //What you want the clock to show up as on your network
bool twelveHourFormat = !matrixIs24H;  // this is set to opposite value of config var matrixIs24H

#define TETRIS_CONFIG_JSON "/tetris_config.json"

MatrixPanel_I2S_DMA *dma_display = nullptr;

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
hw_timer_t * animationTimer = NULL;

unsigned long animationDue = 0;
unsigned long animationDelay = 100; // 100 Smaller number == faster

//uint16_t myBLACK = dma_display->color565(0, 0, 0);

TetrisMatrixDraw tetris(*dma_display); // Main clock
TetrisMatrixDraw tetris2(*dma_display); // The "M" of AM/PM
TetrisMatrixDraw tetris3(*dma_display); // The "P" or "A" of AM/PM

char introDisplay[4][9] = {
  {' ', ' ', 'M', 'e', 'r', 'r', 'y', ' ', ' '},
  {'C', 'h', 'r', 'i', 's', 't', 'm', 'a', 's'},
  {'V', 'e', 'r', 's', 'i', 'o', 'n', ':', '9'},
  {'V', 'e', 'r', 's', 'i', 'o', 'n', ':', '7'}
};


uint16_t myBLACK = dma_display->color565(0, 0, 0);
uint16_t myWHITE = dma_display->color565(255, 255, 255);
uint16_t myRED = dma_display->color565(255, 0, 0);
uint16_t myORANGE = dma_display->color565(255, 165, 0);
uint16_t myYELLOW = dma_display->color565(255, 255, 0);
uint16_t myGREEN = dma_display->color565(0, 255, 0);
uint16_t myBLUE = dma_display->color565(0, 0, 255);
uint16_t myCYAN = dma_display->color565(0, 255, 255);
uint16_t myMAGENTA = dma_display->color565(255, 0, 255);
uint16_t myPURPLE = dma_display->color565(181, 5, 181);  //181 5 181  100,0,100
uint16_t scrollingColor = myBLUE;
uint16_t myUNC = dma_display->color565(75, 156, 211);  //Carolina Blue UNC RGB 75, 156, 211
uint16_t myGOLD = dma_display->color565(252, 186, 3);

//uint16_t arrColors[9] = {myWHITE, myRED, myORANGE, myYELLOW, myGREEN, myBLUE, myCYAN, myMAGENTA, myPURPLE};
uint16_t arrColors[8] = {myRED, myORANGE, myYELLOW, myGREEN, myBLUE, myWHITE, myPURPLE, myCYAN};

uint16_t introDisplayColor[4][9] = {};

uint16_t getRandomColor(int row, int col) {
      // we are passed in the x,y for the letter we are drawing
      bool uniqueColor = false;
      bool notUnique = true;
      
      uint16_t tempColor = 0;
      Serial.print("===== getRandomColr for [");
      Serial.print(row);
      Serial.print(col);
      Serial.print("]");

      while (!uniqueColor)
      {
        notUnique = false;

        // get random color from color array
        tempColor = arrColors[random(8)];
        Serial.print("TRYING THIS COLOR :");
        Serial.println(tempColor);
        
        //check if any other rows above have this same color
       for (int i = 0; i < row; i++)
        {
         if (introDisplayColor[i][col] == tempColor) 
          { 
              Serial.print("NOT UNIQUE-"); 
              Serial.print("COLOR ABOVE AT |");
              Serial.print(i);
              Serial.print(col);
              Serial.print("|");
              Serial.println(introDisplayColor[i][col]);
            notUnique = true;             
          }        
        }

          // only check the current row for before and after diff
           // check color to immediate left an immediate right
           if ( (col-1 >= 0) && (introDisplayColor[row][col-1] == tempColor) )
           {             
              Serial.print("NOT UNIQUE-"); 
              Serial.print("COLOR TO LEFT IS [");
              Serial.print(row);
              Serial.print(col);
              Serial.print("]");
              Serial.println(introDisplayColor[row][col-1]);
              notUnique = true; 
           }                
          // only check the current row for before and after diff
           // check color to immediate left an immediate right
           if ( (col-2 >= 0) && (introDisplayColor[row][col-2] == tempColor) )
           {             
              Serial.print("NOT UNIQUE-"); 
              Serial.print("COLOR TO LEFT 2 OVER IS [");
              Serial.print(row);
              Serial.print(col);
              Serial.print("]");
              Serial.println(introDisplayColor[row][col-2]);
              notUnique = true; 
           }                
          // only check the current row for before and after diff
           // check color to immediate left an immediate right
           if ( (col-3 >= 0) && (introDisplayColor[row][col-3] == tempColor) )
           {             
              Serial.print("NOT UNIQUE-"); 
              Serial.print("COLOR TO LEFT 3 OVER IS [");
              Serial.print(row);
              Serial.print(col);
              Serial.print("]");
              Serial.println(introDisplayColor[row][col-3]);
              notUnique = true; 
           }                
          // only check the current row for before and after diff
           // check color to immediate left an immediate right
           if ( (col-4 >= 0) && (introDisplayColor[row][col-4] == tempColor) )
           {             
              Serial.print("NOT UNIQUE-"); 
              Serial.print("COLOR TO LEFT 4 OVER IS [");
              Serial.print(row);
              Serial.print(col);
              Serial.print("]");
              Serial.println(introDisplayColor[row][col-4]);
              notUnique = true; 
           }                
           
        uniqueColor = !notUnique;
    }
      return tempColor;
  }
    
void drawIntro2() {

  dma_display->flipDMABuffer();
  dma_display->fillScreen(myBLACK);

  String strLetter="";
  int currentRow = -8;
  int currentCol = -6;  
  //runonce = true;  crbtemp
  int lenX = 0;
  String XX = "";
  
  // output each array element's value
  for (int i = 0; i < 4; i++)
  {
    currentRow = currentRow + 8;
    currentCol = -6;
    Serial.println(" ");
    Serial.print("========== LINE:");
    Serial.println(String(i));
    for (int j = 0; j < 9; j++)
    {
        currentCol = currentCol+ 6;

      //Serial.println(j);
//      if (!introDisplay[i][j] == ' ') {
        Serial.print("[");
        Serial.print(String(introDisplay[i][j]));
        Serial.print("]");
        introDisplayColor[i][j] = getRandomColor(i, j);
        //draw character on matrix display using color
        //void TetrisMatrixDraw::drawChar(String letter, uint8_t x, uint8_t y, uint16_t color)
        //{
        //this->display->setTextColor(color);
        //this->display->setCursor(x, y);
        //this->display->print(letter);
        //}
        strLetter = String(introDisplay[i][j]);
        tetris.drawChar(strLetter, currentCol, currentRow, introDisplayColor[i][j]);        
//      }
    }    
  }

    // print out the color array
 // output each array element's value
  for (int i = 0; i < 4; i++)
  {
    Serial.println(" ");
    Serial.print("========== LINE:");
    Serial.println(String(i));
    for (int j = 0; j < 9; j++)
    {
      //Serial.println(j);
        Serial.print("[");
        XX = String(introDisplayColor[i][j]);
        lenX = XX.length();
        for (int i = lenX; i < 5; i++)
        {
         Serial.print("0");          
        }

        Serial.print(String(introDisplayColor[i][j]));
        Serial.print("]");
    }
  }

  dma_display->showDMABuffer();
  delay(5000);

}
  

Timezone myTZ;
unsigned long oneSecondLoopDue = 0;

bool showColon = true;
volatile bool finishedAnimating = false;
bool displayIntro = true;

String lastDisplayedTime = "";
String lastDisplayedAmPm = "";

//flag for saving data
bool shouldSaveConfig = false;

// wifimanager can run in a blocking mode or a non blocking mode
// Be sure to know how to process loops with no delay() if using non blocking
bool wm_nonblocking = false; // change to true to use non blocking

WiFiManager wm; // global wm instance
WiFiManagerParameter custom_field; // global param ( for non blocking w params )
WiFiManagerParameter p_custom_TZ;
WiFiManagerParameter custom_field2;

// This method is for controlling the tetris library draw calls
void animationHandler()
{
  // Not clearing the display and redrawing it when you
  // dont need to improves how the refresh rate appears
  if (!finishedAnimating) {
    dma_display->flipDMABuffer();
    dma_display->fillScreen(myBLACK);
    if (displayIntro) {
      finishedAnimating = tetris.drawText(1, 21);
    } else {
      if (twelveHourFormat) {
        // Place holders for checking are any of the tetris objects
        // currently still animating.
        bool tetris1Done = false;
        bool tetris2Done = false;
        bool tetris3Done = false;

        tetris1Done = tetris.drawNumbers(-6, 26, showColon);
        tetris2Done = tetris2.drawText(56, 25);

        // Only draw the top letter once the bottom letter is finished.
        if (tetris2Done) {
          tetris3Done = tetris3.drawText(56, 15);
        }

        finishedAnimating = tetris1Done && tetris2Done && tetris3Done;

      } else {
        finishedAnimating = tetris.drawNumbers(2, 26, showColon);
      }
    }
    dma_display->showDMABuffer();
  }

}

void drawIntro(int x = 0, int y = 0)
{
  dma_display->flipDMABuffer();
  dma_display->fillScreen(myBLACK);
  tetris.drawChar("T", x, y, tetris.tetrisCYAN);
  tetris.drawChar("e", x + 5, y, tetris.tetrisMAGENTA);
  tetris.drawChar("t", x + 11, y, tetris.tetrisYELLOW);
  tetris.drawChar("r", x + 17, y, tetris.tetrisGREEN);
  tetris.drawChar("i", x + 22, y, tetris.tetrisBLUE);
  tetris.drawChar("s", x + 27, y, tetris.tetrisRED);
  //tetris.drawChar("", x + 32, y, tetris.tetrisWHITE);
  //tetris.drawChar("", x + 37, y, tetris.tetrisCYAN);
  //tetris.drawChar("", x + 42, y, tetris.tetrisMAGENTA);
  //tetris.drawChar("", x + 47, y, tetris.tetrisYELLOW);
  //tetris.drawChar("", x + 52, y, tetris.tetrisGREEN);
  //tetris.drawChar("", x + 57, y, tetris.tetrisBLUE);
  dma_display->showDMABuffer();
  delay(1000);

  //dma_display->flipDMABuffer();
  //dma_display->fillScreen(myBLACK);
  tetris.drawChar("C", x, y + 10, tetris.tetrisRED);
  tetris.drawChar("l", x + 5, y + 10, tetris.tetrisWHITE);
  tetris.drawChar("o", x + 11, y + 10, tetris.tetrisBLUE);
  tetris.drawChar("c", x + 17, y + 10, tetris.tetrisYELLOW);
  tetris.drawChar("k", x + 22, y + 10, tetris.tetrisMAGENTA);
  //tetris.drawChar("", x + 27, y + 10, tetris.tetrisCYAN);
  //tetris.drawChar("", x + 32, y + 10, tetris.tetrisWHITE);
  //tetris.drawChar("", x + 37, y + 10, tetris.tetrisCYAN);
  //tetris.drawChar("", x + 42, y + 10, tetris.tetrisMAGENTA);
  //tetris.drawChar("", x + 47, y + 10, tetris.tetrisYELLOW);
  //tetris.drawChar("", x + 52, y + 10, tetris.tetrisGREEN);
  //tetris.drawChar("", x + 57, y + 10, tetris.tetrisBLUE);
  dma_display->showDMABuffer();
  delay(1000);

  //dma_display->flipDMABuffer();
  //dma_display->fillScreen(myBLACK);
  tetris.drawChar("V", x, y + 20, tetris.tetrisYELLOW);
  tetris.drawChar("e", x + 5, y + 20, tetris.tetrisRED);
  tetris.drawChar("r", x + 11, y + 20, tetris.tetrisGREEN);
  tetris.drawChar("s", x + 17, y + 20, tetris.tetrisBLUE);
  tetris.drawChar("i", x + 22, y + 20, tetris.tetrisCYAN);
  tetris.drawChar("o", x + 27, y + 20, tetris.tetrisORANGE);
  tetris.drawChar("n", x + 32, y + 20, tetris.tetrisWHITE);
  tetris.drawChar(":", x + 37, y + 20, tetris.tetrisCYAN);
  tetris.drawChar("1", x + 42, y + 20, tetris.tetrisMAGENTA);
  tetris.drawChar(".", x + 47, y + 20, tetris.tetrisYELLOW);
  tetris.drawChar("0", x + 52, y + 20, tetris.tetrisGREEN);
  //tetris.drawChar("", x + 57, y + 20, tetris.tetrisBLUE);
  //tetris.drawChar("", x + 57, y + 20, tetris.tetrisBLUE);
  //tetris.drawChar("", x + 47, y, tetris.tetrisYELLOW);
  dma_display->showDMABuffer();
  delay(3000);

  //dma_display->flipDMABuffer();
  //dma_display->fillScreen(myBLACK);
  tetris.drawChar("V", x, y + 20, tetris.tetrisYELLOW);
  tetris.drawChar("e", x + 5, y + 20, tetris.tetrisRED);
  tetris.drawChar("r", x + 11, y + 20, tetris.tetrisGREEN);
  tetris.drawChar("s", x + 17, y + 20, tetris.tetrisBLUE);
  tetris.drawChar("i", x + 22, y + 20, tetris.tetrisCYAN);
  tetris.drawChar("o", x + 27, y + 20, tetris.tetrisORANGE);
  tetris.drawChar("n", x + 32, y + 20, tetris.tetrisWHITE);
  tetris.drawChar(":", x + 37, y + 20, tetris.tetrisCYAN);
  tetris.drawChar("1", x + 42, y + 20, tetris.tetrisMAGENTA);
  tetris.drawChar(".", x + 47, y + 20, tetris.tetrisYELLOW);
  tetris.drawChar("0", x + 52, y + 20, tetris.tetrisGREEN);
  //tetris.drawChar("", x + 57, y + 20, tetris.tetrisBLUE);
  //tetris.drawChar("", x + 57, y + 20, tetris.tetrisBLUE);
  //tetris.drawChar("", x + 47, y, tetris.tetrisYELLOW);
  dma_display->showDMABuffer();
  delay(3000);

}

void drawConnecting(int x = 0, int y = 0)
{
  dma_display->flipDMABuffer();
  dma_display->fillScreen(myBLACK);
  tetris.drawChar("C", x, y, tetris.tetrisCYAN);
  tetris.drawChar("o", x + 5, y, tetris.tetrisMAGENTA);
  tetris.drawChar("n", x + 11, y, tetris.tetrisYELLOW);
  tetris.drawChar("n", x + 17, y, tetris.tetrisGREEN);
  tetris.drawChar("e", x + 22, y, tetris.tetrisBLUE);
  tetris.drawChar("c", x + 27, y, tetris.tetrisRED);
  tetris.drawChar("t", x + 32, y, tetris.tetrisWHITE);
  tetris.drawChar("i", x + 37, y, tetris.tetrisCYAN);
  tetris.drawChar("n", x + 42, y, tetris.tetrisMAGENTA);
  tetris.drawChar("g", x + 47, y, tetris.tetrisYELLOW);
  tetris.drawChar(".", x + 51, y, tetris.tetrisGREEN);
  tetris.drawChar(".", x + 54, y, tetris.tetrisBLUE);
  tetris.drawChar(".", x + 57, y, tetris.tetrisRED);
  dma_display->showDMABuffer();
  delay(5000);
}

void saveConfigFile() {

  Serial.println(F("Saving config"));
  DynamicJsonDocument  json(1024);
  json[TIME_ZONE_LABEL] = timeZone;
  JsonObject matrixJson = json.createNestedObject("matrix");
  //matrixJson[MATRIX_CLK_LABEL] = matrixClk;
  //matrixJson[MATRIX_DRIVER_LABEL] = matrixDriver;
  //matrixJson[MATRIX_64_LABEL] = matrixIs64;
  matrixJson[MATRIX_24H_LABEL] = matrixIs24H;
  twelveHourFormat = !matrixIs24H;  // this is set to opposite value of config var matrixIs24H
  matrixJson[MATRIX_FORCE_LABEL] = matrixForceRefresh;
  matrixJson[MATRIX_MADE_FOR_LABEL] = matrixMadeFor;
  matrixJson[MATRIX_MADE_BY_LABEL] = matrixMadeBy;
  matrixJson[MATRIX_HOSTNAME_LABEL] = matrixHostname;
  matrixJson[MATRIX_MSG_LINE1] = matrixMsgLine1;
  matrixJson[MATRIX_MSG_LINE2] = matrixMsgLine2;
  matrixJson[MATRIX_MSG_LINE3] = matrixMsgLine3;
  matrixJson[MATRIX_MSG_LINE4] = matrixMsgLine4;


  File configFile = SPIFFS.open(TETRIS_CONFIG_JSON, "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }

  //serializeJsonPretty(json, Serial);  this prints out the pretty json to serial
  if (serializeJson(json, configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  }
  configFile.close();
}


bool setupSpiffs() {

  //read configuration from FS json
  Serial.println("mounting FS...");

  // May need to make it begin(true) first time you are using SPIFFS
  if (SPIFFS.begin(false) || SPIFFS.begin(true)) {
    Serial.println("mounted file system");
    if (SPIFFS.exists(TETRIS_CONFIG_JSON)) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open(TETRIS_CONFIG_JSON, "r");
      if (configFile) {
        Serial.println("opened config file");
        StaticJsonDocument<1024> json;
        DeserializationError error = deserializeJson(json, configFile);
        //serializeJsonPretty(json, Serial);
        if (!error) {
          Serial.println("\nparsed json");
          // Retrieve configuration and store in global vars
          //Serial.println(json[TIME_ZONE_LABEL].as<String>());
          strcpy(timeZone, json[TIME_ZONE_LABEL]);
          //Serial.println(timeZone);
          matrixIs24H = json["matrix"][MATRIX_24H_LABEL].as<bool>();
          twelveHourFormat = !matrixIs24H;  // this is set to opposite value of config var matrixIs24H
          matrixForceRefresh = json["matrix"][MATRIX_FORCE_LABEL].as<bool>();
          strcpy(matrixMadeFor, json["matrix"][MATRIX_MADE_FOR_LABEL]);
          strcpy(matrixMadeBy, json["matrix"][MATRIX_MADE_BY_LABEL]);
          strcpy(matrixHostname, json["matrix"][MATRIX_HOSTNAME_LABEL]);
          strcpy(matrixMsgLine1, json["matrix"][MATRIX_MSG_LINE1]);
          strcpy(matrixMsgLine2, json["matrix"][MATRIX_MSG_LINE2]);
          strcpy(matrixMsgLine3, json["matrix"][MATRIX_MSG_LINE3]);
          strcpy(matrixMsgLine4, json["matrix"][MATRIX_MSG_LINE4]);

          //Serial.println("-------VALUES RETRIEVED FROM CONFIG FILE-------------");
          //Serial.println("---------- millitary time-------------");
          //Serial.println(matrixIs24H);
          //Serial.println("---------- force refresh-------------");
          //Serial.println(matrixForceRefresh);
          //Serial.println("---------- MADE FOR -------------");
          //Serial.println(matrixMadeFor);
          //Serial.println("---------- MADE BY -------------");
          //Serial.println(matrixMadeBy);
          //Serial.println("---------- HOST NAME -------------");
          //Serial.println(matrixHostname);

          return true;

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
  return false;
}

void configDisplay() {

  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X,   // module width
    PANEL_RES_Y,   // module height
    PANEL_CHAIN    // Chain length
  );

  if (matrixIs64) {
    mxconfig.mx_height = 64;
    //mxconfig.gpio.e = 18; // Needed for 64x64 -1 or comment out if 64x32
  }

  // Uncomment these 4 lines if Blue and Green are swapped
  mxconfig.gpio.b1 = 26;
  mxconfig.gpio.b2 = 12;
  mxconfig.gpio.g1 = 27;
  mxconfig.gpio.g2 = 13;

  //mxconfig.double_buff = true;
  // May or may not be needed depending on your matrix
  // Example of what needing it looks like:
  // https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/134#issuecomment-866367216
  mxconfig.clkphase = false;

  mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  // Display Setup
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->fillScreen(myBLACK);
}

void setMatrixTime() {
  String timeString = "";
  String AmPmString = "";
  if (twelveHourFormat) {
    // Get the time in format "1:15" or 11:15 (12 hour, no leading 0)
    // Check the EZTime Github page for info on
    // time formatting
    timeString = myTZ.dateTime("g:i");

    //If the length is only 4, pad it with
    // a space at the beginning
    if (timeString.length() == 4) {
      timeString = " " + timeString;
    }

    //Get if its "AM" or "PM"
    AmPmString = myTZ.dateTime("A");
    if (lastDisplayedAmPm != AmPmString) {
      Serial.println(AmPmString);
      lastDisplayedAmPm = AmPmString;
      // Second character is always "M"
      // so need to parse it out
      tetris2.setText("M", matrixForceRefresh);

      // Parse out first letter of String
      tetris3.setText(AmPmString.substring(0, 1), matrixForceRefresh);
    }
  } else {
    // Get time in format "01:15" or "22:15"(24 hour with leading 0)
    timeString = myTZ.dateTime("H:i");
  }

  // Only update Time if its different
  if (lastDisplayedTime != timeString) {
    Serial.println(timeString);
    lastDisplayedTime = timeString;
    tetris.setTime(timeString, matrixForceRefresh);

    // Must set this to false so animation knows
    // to start again
    finishedAnimating = false;
  }
}

void handleColonAfterAnimation() {

  // It will draw the colon every time, but when the colour is black it
  // should look like its clearing it.
  uint16_t colour =  showColon ? tetris.tetrisWHITE : tetris.tetrisBLACK;
  // The x position that you draw the tetris animation object
  int x = twelveHourFormat ? -6 : 2;
  // The y position adjusted for where the blocks will fall from
  // (this could be better!)
  int y = 26 - (TETRIS_Y_DROP_DEFAULT * tetris.scale);
  tetris.drawColon(x, y, colour);
  dma_display->showDMABuffer();
}

void setupClock() {
  configDisplay();

  tetris.display = dma_display; // Main clock
  tetris2.display = dma_display; // The "M" of AM/PM - Always an M
  tetris3.display = dma_display; // The "P" or "A" of AM/PM

  // "connecting"
  drawConnecting(1, 12); //5, 10

  // Setup EZ Time
  setDebug(INFO);
  waitForSync();

  Serial.println();
  Serial.println("UTC:             " + UTC.dateTime());

  //myTZ.setLocation(F(MY_TIME_ZONE));
  myTZ.setLocation(F(timeZone));

  //myTZ.setLocation(timeZone);
  Serial.print(F("Time in your set timezone:         "));
  Serial.println(myTZ.dateTime());
  
  dma_display->fillScreen(myBLACK); // to make sure "connecting..." doesn't show anymore
  
  // "Intro Screen"
  drawIntro(1, 1); //6, 12
  delay(3000);

  // "Intro2222222222222  Screen"

  for ( int i = 0; i<2; i++)
  {
    drawIntro2(); 
  }

  // Start the Animation Timer
  tetris.setText(madefor, true);  //MUST BE IN CAPS
  // Wait for the animation to finish
  while (!finishedAnimating)
  {
    delay(animationDelay);
    animationHandler();
  }
  delay(3000);

  //-------------------------------------------
  finishedAnimating = false;
  tetris.setText(matrixMadeFor, true);  //MUST BE IN CAPS
  // Wait for the animation to finish
  while (!finishedAnimating)
  {
    delay(animationDelay);
    animationHandler();
  }
  delay(3000);

  finishedAnimating = false;
  tetris.setText(madeby, true);  //MUST BE IN CAPS
  // Wait for the animation to finish
  while (!finishedAnimating)
  {
    delay(animationDelay);
    animationHandler();
  }
  delay(3000);

  finishedAnimating = false;
  tetris.setText(matrixMadeBy, true);  //MUST BE IN CAPS
  // Wait for the animation to finish
  while (!finishedAnimating)
  {
    delay(animationDelay);
    animationHandler();
  }
  delay(3000);

  finishedAnimating = false;
  tetris.setText("WITH LOVE", true);  //MUST BE IN CAPS
  // Wait for the animation to finish
  while (!finishedAnimating)
  {
    delay(animationDelay);
    animationHandler();
  }
  delay(3000);

  finishedAnimating = false;
  tetris.setText("XMAS 2021", true);  //MUST BE IN CAPS
  // Wait for the animation to finish
  while (!finishedAnimating)
  {
    delay(animationDelay);
    animationHandler();
  }
  delay(3000);

  finishedAnimating = false;
  displayIntro = false;
  //-----------------------------------------
  tetris.scale = 2;
  //tetris.setTime("00:00", true);


}

void setMatrixConfigParameters() {

  WiFiManagerParameter p_time_zone(TIME_ZONE_LABEL, "Time Zone label", timeZone, TIME_ZONE_LEN, "readonly");
  WiFiManagerParameter p_is24h(MATRIX_24H_LABEL, " Is Millitary Time", "T", 2, "type=\"checkbox\"");
  WiFiManagerParameter p_ForceRefresh(MATRIX_FORCE_LABEL, " Drop All Digits", "T", 2, "type=\"checkbox\"");
  WiFiManagerParameter p_madeFor(MATRIX_MADE_FOR_LABEL, "Made For", matrixMadeFor, 10 );
  // you can make this readonly
  //WiFiManagerParameter p_madeBy(MATRIX_MADE_BY_LABEL, "Made By", matrixMadeBy, 10, "readonly");
  WiFiManagerParameter p_madeBy(MATRIX_MADE_BY_LABEL, "Made By", matrixMadeBy, 10);
  WiFiManagerParameter p_hostname(MATRIX_HOSTNAME_LABEL, "Host Name", matrixHostname, 50);
  WiFiManagerParameter custom_text("<p>Drop All Digits or Changed Only</p>");

  // test customTZ html(radio)
  const char* cRadio = "<br/><hr/><label for='cTZid'>Time Zone Selection</label><input type='radio' name='cTZid' value='1' checked> EST(New York)<br><input type='radio' name='cTZid' value='2'> CST(Chicago)<br><input type='radio' name='cTZid' value='3'> MST(Denver)<br><input type='radio' name='cTZid' value='4'> CST(Los Angeles)<hr/>";
  new (&p_custom_TZ) WiFiManagerParameter(cRadio); // custom html input

  WiFiManagerParameter p_msg_line1(MATRIX_MSG_LINE1, "Message Line 1", matrixMsgLine1, 10);
  WiFiManagerParameter p_msg_line2(MATRIX_MSG_LINE1, "Message Line 2", matrixMsgLine2, 10);
  WiFiManagerParameter p_msg_line3(MATRIX_MSG_LINE1, "Message Line 3", matrixMsgLine3, 10);
  WiFiManagerParameter p_msg_line4(MATRIX_MSG_LINE1, "Message Line 4", matrixMsgLine4, 10);

  wm.addParameter(&p_time_zone);
  wm.addParameter(&p_custom_TZ);
  wm.addParameter(&p_is24h);
  wm.addParameter(&custom_text);
  wm.addParameter(&p_ForceRefresh);
  wm.addParameter(&p_madeFor);
  wm.addParameter(&p_madeBy);
  wm.addParameter(&p_hostname);
  wm.addParameter(&p_msg_line1);
  wm.addParameter(&p_msg_line2);
  wm.addParameter(&p_msg_line3);
  wm.addParameter(&p_msg_line4);


  //wm.addParameter(&custom_field2);
  //wm.addParameter(&custom_field);
  wm.setSaveParamsCallback(saveParamCallback);

  //returns the list of Parameters
  //    WiFiManagerParameter** getParameters();
  // returns the Parameters Count
  //    int           getParametersCount();
  //   Serial.println("==============  GET PARAM COUNT====================");
  //   Serial.println(wm.getParametersCount());
  //   Serial.println("==============  GET PARAM LIST====================");
  //   Serial.println(wm.getParameters());


}



void setup() {

  Serial.begin(115200);

  bool spiffsSetup = setupSpiffs();
  //SPIFFS.format();
  //wm.resetSettings(); // wipe settings

  pinMode(PIN_LED, OUTPUT);
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  //Serial.setDebugOutput(true);
  delay(3000);
  Serial.println("\n Starting");
  Serial.println(ARDUINO_BOARD);

  //pinMode(TRIGGER_PIN, INPUT);
  //CRB: the following 2 settings were in the Super ex/ of wifi mgr 
  // This is sometimes necessary, it is still unknown when and why this is needed but it may solve some race condition or bug in esp SDK/lib
  wm.setCleanConnect(true); // disconnect before connect, clean connect
//CRBTEMP??  WiFi.setSleepMode(WIFI_NONE_SLEEP); // disable sleep, can improve ap stability


  if (wm_nonblocking) wm.setConfigPortalBlocking(false);

  // add a custom input field
  int customFieldLength = 20;

  //  setMatrixConfigParameters();
  WiFiManagerParameter p_time_zone(TIME_ZONE_LABEL, "Time Zone label", timeZone, TIME_ZONE_LEN, "readonly");
  WiFiManagerParameter p_is24h(MATRIX_24H_LABEL, " Is Millitary Time", "T", 2, "type=\"checkbox\"");
  WiFiManagerParameter p_ForceRefresh(MATRIX_FORCE_LABEL, " Drop All Digits", "T", 2, "type=\"checkbox\"");
  WiFiManagerParameter p_madeFor(MATRIX_MADE_FOR_LABEL, "Made For", matrixMadeFor, 10 );
  // you can make this readonly
  //WiFiManagerParameter p_madeBy(MATRIX_MADE_BY_LABEL, "Made By", matrixMadeBy, 10, "readonly");
  WiFiManagerParameter p_madeBy(MATRIX_MADE_BY_LABEL, "Made By", matrixMadeBy, 10);
  WiFiManagerParameter p_hostname(MATRIX_HOSTNAME_LABEL, "Host Name", matrixHostname, 50);
  WiFiManagerParameter custom_text("<p>Drop All Digits or Changed Only</p>");

  // test customTZ html(radio)
  const char* cRadio = "<br/><hr/><label for='cTZid'>Time Zone Selection</label><input type='radio' name='cTZid' value='1' checked> EST(New York)<br><input type='radio' name='cTZid' value='2'> CST(Chicago)<br><input type='radio' name='cTZid' value='3'> MST(Denver)<br><input type='radio' name='cTZid' value='4'> CST(Los Angeles)<hr/>";
  new (&p_custom_TZ) WiFiManagerParameter(cRadio); // custom html input

  WiFiManagerParameter p_msg_line1(MATRIX_MSG_LINE1, "Message Line 1", matrixMsgLine1, 10);
  WiFiManagerParameter p_msg_line2(MATRIX_MSG_LINE2, "Message Line 2", matrixMsgLine2, 10);
  WiFiManagerParameter p_msg_line3(MATRIX_MSG_LINE3, "Message Line 3", matrixMsgLine3, 10);
  WiFiManagerParameter p_msg_line4(MATRIX_MSG_LINE4, "Message Line 4", matrixMsgLine4, 10);
  
  wm.addParameter(&p_time_zone);
  wm.addParameter(&p_custom_TZ);
  wm.addParameter(&p_is24h);
  wm.addParameter(&custom_text);
  wm.addParameter(&p_ForceRefresh);
  wm.addParameter(&p_madeFor);
  wm.addParameter(&p_madeBy);
  wm.addParameter(&p_hostname);
  wm.addParameter(&p_msg_line1);
  wm.addParameter(&p_msg_line2);
  wm.addParameter(&p_msg_line3);
  wm.addParameter(&p_msg_line4);


  //wm.addParameter(&custom_field2);
  //wm.addParameter(&custom_field);
  wm.setSaveParamsCallback(saveParamCallback);

  //returns the list of Parameters
  //    WiFiManagerParameter** getParameters();
  // returns the Parameters Count
  //    int           getParametersCount();
  Serial.println("==============  GET PARAM COUNT====================");
  Serial.println(wm.getParametersCount());
  //   Serial.println("==============  GET PARAM LIST====================");
  //   Serial.println(wm.getParameters());

  // custom menu via array or vector
  //
  // menu tokens, "wifi","wifinoscan","info","param","close","sep","erase","restart","exit" (sep is seperator) (if param is in menu, params will not show up in wifi page!)
  //const char* menu[] = {"wifi","info","param","sep","restart","exit"};
  //wm.setMenu(menu,6);
//  std::vector<const char *> menu = {"wifi", "info", "param", "sep", "restart", "exit"};
  std::vector<const char *> menu = {"wifi", "info", "sep", "restart", "exit"};
  wm.setMenu(menu);

  // set dark theme
  wm.setClass("invert");

  //set static ip
  // wm.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0)); // set static ip,gw,sn
  // wm.setShowStaticFields(true); // force show static ip fields
  // wm.setShowDnsFields(true);    // force show dns field always
  wm.setCountry("US");  // someone said this prevents delays

  //wm.setConnectTimeout(60); // how long to try to connect for before continuing
  // wm.setCaptivePortalEnable(false); // disable captive portal redirection
  wm.setAPClientCheck(true); // avoid timeout if client connected to softap   crbtemp-------------------------------------

  // wifi scan settings
  // wm.setRemoveDuplicateAPs(false); // do not remove duplicate ap names (true)
  wm.setMinimumSignalQuality(32);  // set min RSSI (percentage) to show in scans, null = 8%
  // wm.setShowInfoErase(false);      // do not show erase button on info page
  wm.setScanDispPerc(true);       // show RSSI as percentage not graph icons

  // wm.setBreakAfterConfig(true);   // always exit configportal even if wifi save fails

  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
  // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
 // res = wm.autoConnect("TetrisClock","kevin"); // password protected ap
  res = wm.startConfigPortal("TetrisClock","kevin123");
  if (!res) {
    Serial.println("Failed to connect or hit timeout");
    // ESP.restart();  //crbtemp we might want this-----------------------------------
  }
  else {
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  }

  setupClock();
}

void checkButton() {
  // check for button press
  if (initialConfig) {
    //Serial.println("Button Held");
    //Serial.println("Erasing Config, restarting");
    wm.resetSettings();
    ESP.restart();
  }
}


String getParam(String name) {
  //read parameter from server, for customhmtl input
  String value;
  if (wm.server->hasArg(name)) {
    value = wm.server->arg(name);
    Serial.println(value);
  }
  return value;
}

void saveParamCallback() {
  Serial.println("[CALLBACK] saveParamCallback fired");
  //  Serial.println("PARAM customfieldid = " + getParam("customfieldid"));
  //  Serial.println("PARAM cTZid = " + getParam("cTZid"));
  //  Serial.println("PARAM customChkBox = " + getParam("customChkBox"));

  Serial.println("PARAM p_custom_TZ = " + getParam("cTZid"));
  //put select case here fo TZ
  //  int p_TimeZone = 1;
  int p_TimeZone = getParam("cTZid").toInt();
  switch (p_TimeZone)
  {
    case 1:
      strncpy(timeZone, "America/New_York" , sizeof(timeZone));
      break;
    case 2:
      strncpy(timeZone, "America/Chicago" , sizeof(timeZone));
      break;
    case 3:
      strncpy(timeZone, "America/Denver" , sizeof(timeZone));
      break;
    case 4:
      strncpy(timeZone, "America/Los_Angeles" , sizeof(timeZone));
      break;
  }
  Serial.println("====== YOUR TIME ZONE HAS BEEN CHANGED TO ===========");
  Serial.println(timeZone);

  //Serial.println("PARAM p_is24h = " + getParam(MATRIX_24H_LABEL));
  matrixIs24H = (strncmp(getParam(MATRIX_24H_LABEL).c_str(), "T", 1) == 0);
  twelveHourFormat = !matrixIs24H;  // this is set to opposite value of config var matrixIs24H

  //Serial.println("PARAM p_ForceRefresh = " + getParam(MATRIX_FORCE_LABEL));
  matrixForceRefresh = (strncmp(getParam(MATRIX_FORCE_LABEL).c_str(), "T", 1) == 0);
  //Serial.println("PARAM p_madeFor = " + getParam(MATRIX_MADE_FOR_LABEL));
  strncpy(matrixMadeFor, getParam(MATRIX_MADE_FOR_LABEL).c_str(), 10);

  String temp = "";
  temp = String(matrixMadeFor);
  temp.toUpperCase();
  strncpy(matrixMadeFor, temp.c_str(), 10);
  //Serial.println(matrixMadeFor);

  //Serial.println("PARAM p_madeFor = " + getParam(MATRIX_MADE_FOR_LABEL));
  strncpy(matrixMadeBy, getParam(MATRIX_MADE_BY_LABEL).c_str(), 10);
  temp = String(matrixMadeBy);
  temp.toUpperCase();
  strncpy(matrixMadeBy, temp.c_str(), 10);
  //Serial.println(matrixMadeBy);
  //Serial.println("PARAM p_hostname = " + getParam(MATRIX_HOSTNAME_LABEL));
  strncpy(matrixHostname, getParam(MATRIX_HOSTNAME_LABEL).c_str(), 40);

  strncpy(matrixMsgLine1, getParam(MATRIX_MSG_LINE1).c_str(), 10);
  strncpy(matrixMsgLine2, getParam(MATRIX_MSG_LINE2).c_str(), 10);
  strncpy(matrixMsgLine3, getParam(MATRIX_MSG_LINE3).c_str(), 10);
  strncpy(matrixMsgLine4, getParam(MATRIX_MSG_LINE4).c_str(), 10);

  saveConfigFile();

}

void loop() {
  if (wm_nonblocking) wm.process(); // avoid delays() in loop when non-blocking and other long running code

  // put your main code here, to run repeatedly:
  unsigned long now = millis();
  if (now > oneSecondLoopDue) {
    // We can call this often, but it will only
    // update when it needs to
    setMatrixTime();
    showColon = !showColon;

    // To reduce flicker on the screen we stop clearing the screen
    // when the animation is finished, but we still need the colon to
    // to blink
    if (finishedAnimating) {
      handleColonAfterAnimation();
    }
    oneSecondLoopDue = now + 1000;
  }
  now = millis();
  if (now > animationDue) {
    animationHandler();
    animationDue = now + animationDelay;
  }

}
