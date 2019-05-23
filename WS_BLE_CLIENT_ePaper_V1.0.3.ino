/*
Author: Dean Greenhough 2019
*/


/*Versions
   1.0.0  Intial
   1.0.1  Added WDT in callback function as freezing - set to 10secs
   1.0.2  Altered delays in callback, delay end of loop needs to be 3secs
   1.0.3  INIT_BLE ADDED & MOVED #include "BLEDevice.h" TO TOP AS FAILED TO COMPILE IN VS2019

*/

#include "BLEDevice.h"
//#include "BLEScan.h"
#include <GxEPD.h>
#include <GxGDEP015OC1/GxGDEP015OC1.h>        // 1.54" b/w
//#include <GxGDEW0154Z04/GxGDEW0154Z04.h>    // 1.54" b/w/r 200x200
//#include <GxGDEW0154Z17/GxGDEW0154Z17.h>    // 1.54" b/w/r 152x152
//#include <GxGDEW042T2/GxGDEW042T2.h>        // 4.2" b/w
//#include <GxGDEW042Z15/GxGDEW042Z15.h>      // 4.2" b/w/r

#include GxEPD_BitmapExamples
// FreeFonts from Adafruit_GFX
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
//#include <Fonts/FreeMonoBold18pt7b.h>
//#include <Fonts/FreeMonoBold24pt7b.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

// BUSY -> 4, RST -> 16, DC -> 17, CS -> SS(5), CLK -> SCK(18), DIN -> MOSI(23), GND -> GND, 3.3V -> 3.3V
GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16); // arbitrary selection of 17, 16
GxEPD_Class display(io, /*RST=*/ 16, /*BUSY=*/ 4); // arbitrary selection of (16), 4

//WDT
#include "esp_system.h"
const int wdtTimeout = 6000;  //WDT SET IN mS
hw_timer_t *timer = NULL;

//WDT FUNCTION
void IRAM_ATTR resetModule() {
  ets_printf("\n");
  ets_printf("***********************************\n");
  ets_printf("***********************************\n");
  ets_printf("****** WDT ACTIVATED @ 6 Sec ******\n");
  ets_printf("***********************************\n");
  ets_printf("***********************************\n");
  ets_printf("\n");
  delay(250);
  esp_restart();
  delay (10);

}

#define DEBUG 1

//static  BLEUUID serviceUUID(BLEUUID((uint16_t)0x180D));
//static BLEUUID    charUUID(BLEUUID((uint16_t)0x2A37));
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID    charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
static BLEAddress *pServerAddress;
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;

static BLERemoteCharacteristic* pRemoteCharacteristic;

unsigned int RX_LEFT  = 0;
unsigned int RX_RIGHT = 0;
int RX_VOLTAGE        = 0;
int RX_CURRENT        = 0;

//LoopTimer
unsigned long startMillis;         //completeLoopTime TIMER connectToServer
unsigned long currentMillis;       //completeLoopTime TIMER connectToServer


static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
{
  int completeLoopTime = 0;
  startMillis = millis();      //START LOOPTIME
  if (DEBUG)Serial.println("");
  if (DEBUG)Serial.println("Notify Callback");

  RX_LEFT = ((int)(pData[0]) << 8) + pData[1];  //DECODE CAST 2 BYTES INTO AN INT
  if (DEBUG)Serial.print ("RX_LEFT     ");
  if (DEBUG)Serial.println(RX_LEFT);
  delay(10);
  RX_RIGHT = ((int)(pData[2]) << 8) + pData[3]; //DECODE CAST 2 BYTES INTO AN INT
  if (DEBUG)Serial.print ("RX_RIGHT    ");
  if (DEBUG)Serial.println(RX_RIGHT);

  delay(10);

  RX_VOLTAGE = ((int)(pData[4]) << 8) + pData[5];  //DECODE CAST 2 BYTES INTO AN INT
  if (DEBUG)Serial.print ("RX_VOLTAGE  ");
  if (DEBUG)Serial.println(RX_VOLTAGE);
  delay(10);
  RX_CURRENT = ((int)(pData[6]) << 8) + pData[7];  //DECODE CAST 2 BYTES INTO AN INT
  if (DEBUG)Serial.print ("RX_CURRENT  ");
  if (DEBUG)Serial.println(RX_CURRENT);
  //Serial.println("");
  delay(10);
  SALT_BLOCK_READ();             //DISPLAY
  UPDATE_BLOCK_LEVELS();         //DISPLAY
  delay(10);
  currentMillis = millis();      //END LOOPTIME
  completeLoopTime = (currentMillis - startMillis) ;
  //Serial.println("");
  Serial.print ("Callback LoopTime ");
  Serial.println(completeLoopTime);
  Serial.println("");
  delay(10);

}

bool connectToServer(BLEAddress pAddress) {

  //WDT for hanging issues required set to 10Secs
  timerAlarmEnable(timer);                          //enable interrupt
  if (DEBUG)Serial.println("**WDT Alarm Enabled**");
  timerWrite(timer, 0);                             //reset timer (feed watchdog)


  if (DEBUG)Serial.print("Forming a connection to ");
  if (DEBUG)Serial.println(pAddress.toString().c_str());

  BLEClient*  pClient  = BLEDevice::createClient();
  if (DEBUG)Serial.println("Created client");

  pClient->connect(pAddress);
  if (DEBUG)Serial.println("Connected to server");


  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    if (DEBUG)Serial.print("Failed to find our service UUID: ");
    if (DEBUG)Serial.println(serviceUUID.toString().c_str());
    return false;
  }
  if (DEBUG)Serial.println("Found our service");

  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    if (DEBUG)Serial.print("Failed to find our characteristic UUID: ");
    if (DEBUG)Serial.println(charUUID.toString().c_str());
    return false;
  }
  if (DEBUG)Serial.println("Found our characteristic");

  pRemoteCharacteristic->registerForNotify(notifyCallback);
  if (DEBUG)Serial.println("**WDT Alarm Disabled**");
  Serial.println("");
  timerAlarmDisable(timer);

}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    /**
        Called for each advertising BLE server.
    */
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      if (DEBUG)Serial.print("BLE Advertised Device found: ");
      Serial.println("");
      if (DEBUG)Serial.println(advertisedDevice.toString().c_str());
      delay(10);
      // We have found a device, let us now see if it contains the service we are looking for.
      if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(serviceUUID)) {

        if (DEBUG)Serial.println("Found our device!  address: ");
        Serial.println("");
        delay(10);
        advertisedDevice.getScan()->stop();

        pServerAddress = new BLEAddress(advertisedDevice.getAddress());
        doConnect = true;
        doScan = true;

      } // Found our server
    } // onResult
}; // MyAdvertisedDeviceCallbacks


void INIT_BLE()
{
  BLEDevice::init("");
  if (DEBUG)Serial.println("Restarting BLE Client");
  Serial.println("");
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5);
}



void setup()
{ startMillis = millis();      //START LOOPTIME
  Serial.begin(115200);
  Serial.println(__FILE__);
  Serial.println(__DATE__);
  Serial.println(__TIME__);
  display.init(); 

  //WDT
  timer = timerBegin(0, 80, true);                    //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);    //attach callback
  timerAlarmWrite(timer, wdtTimeout * 1000, false);   //set time in us
 
  INIT_BLE();
  
}

void loop()
{
  INIT_BLE();
  
  if (doConnect == true) {
    if (connectToServer(*pServerAddress)) {
      if (DEBUG)Serial.println("Connected to BLE Server.");
      Serial.println("");
      connected = true;
    } else {
      if (DEBUG)Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    doConnect = false;
  }

  /*
     if (connected) {
      //String newValue = "Time since boot: " + String(millis() / 1000);
      //Serial.println("Setting new characteristic value to \"" + newValue + "\"");
      // Set the characteristic's value to be the array of bytes that is actually a string.
      //pRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());
    } else if (doScan) {
      BLEDevice::getScan()->start(0);  // this is just eample to start scan after disconnect, most likely there is better way to do it in arduino
    }
  */
  delay(3000); //REQUIRED 3SECS
}

void SALT_BLOCK_READ()
{
	const GFXfont* f = &FreeMonoBold9pt7b;
	display.fillScreen(GxEPD_WHITE);
	display.setTextColor(GxEPD_BLACK);
	display.setFont(f);
	//LEFT BLOCK
	display.setCursor(30, 30);
	display.print(RX_LEFT);
	//RIGHT BLOCK
	display.setCursor(130, 30);
	display.print(RX_RIGHT);

	//DRAW OUTLINE LEFT SALT BLOCK
	display.drawRect(5, 50, 90, 145, GxEPD_BLACK);
	//DRAW OUTLINE RIGHT SALT BLOCK
	display.drawRect(105, 50, 90, 145, GxEPD_BLACK);
	//TOP OUTLINE BOX
	display.drawRect(5, 5, 190, 40, GxEPD_BLACK);
}
void UPDATE_BLOCK_LEVELS()
{
	//LEFT SALT BLOCK DISPLAY LEVELS

	if (RX_LEFT <= 70)
	{ //100%
		display.drawRect(10, 84, 80,  24, GxEPD_BLACK);
		display.drawRect(10, 113, 80, 24, GxEPD_BLACK);
		display.drawRect(10, 142, 80, 24, GxEPD_BLACK);
		display.drawRect(10, 171, 80, 19, GxEPD_BLACK);

		display.fillRect(10, 55, 80,  24, GxEPD_BLACK);
		display.fillRect(10, 84, 80,  24, GxEPD_BLACK);
		display.fillRect(10, 113, 80, 24, GxEPD_BLACK);
		display.fillRect(10, 142, 80, 24, GxEPD_BLACK);
		display.fillRect(10, 171, 80, 19, GxEPD_BLACK);

	}

	else if (RX_LEFT <= 115)
	{ //80%
		display.drawRect(10, 55, 80,  24, GxEPD_BLACK);
		display.drawRect(10, 84, 80,  24, GxEPD_BLACK);
		display.drawRect(10, 113, 80, 24, GxEPD_BLACK);
		display.drawRect(10, 142, 80, 24, GxEPD_BLACK);
		display.drawRect(10, 171, 80, 19, GxEPD_BLACK);

		display.fillRect(10, 84, 80,  24, GxEPD_BLACK);
		display.fillRect(10, 113, 80, 24, GxEPD_BLACK);
		display.fillRect(10, 142, 80, 24, GxEPD_BLACK);
		display.fillRect(10, 171, 80, 19, GxEPD_BLACK);

	}

	else if (RX_LEFT <= 160)
	{ //60%
		display.drawRect(10, 55, 80,  24, GxEPD_BLACK);
		display.drawRect(10, 84, 80,  24, GxEPD_BLACK);
		display.drawRect(10, 113, 80, 24, GxEPD_BLACK);
		display.drawRect(10, 142, 80, 24, GxEPD_BLACK);
		display.drawRect(10, 171, 80, 19, GxEPD_BLACK);

		display.fillRect(10, 113, 80, 24, GxEPD_BLACK);
		display.fillRect(10, 142, 80, 24, GxEPD_BLACK);
		display.fillRect(10, 171, 80, 19, GxEPD_BLACK);

	}
	else if (RX_LEFT <= 205)
	{ //40%
		display.drawRect(10, 55, 80,  24, GxEPD_BLACK);
		display.drawRect(10, 84, 80,  24, GxEPD_BLACK);
		display.drawRect(10, 113, 80, 24, GxEPD_BLACK);
		display.drawRect(10, 142, 80, 24, GxEPD_BLACK);
		display.drawRect(10, 171, 80, 19, GxEPD_BLACK);

		display.fillRect(10, 142, 80, 24, GxEPD_BLACK);
		display.fillRect(10, 171, 80, 19, GxEPD_BLACK);

	}

	else if (RX_LEFT <= 245)
	{ //20%
		display.drawRect(10, 55, 80,  24, GxEPD_BLACK);
		display.drawRect(10, 84, 80,  24, GxEPD_BLACK);
		display.drawRect(10, 113, 80, 24, GxEPD_BLACK);
		display.drawRect(10, 142, 80, 24, GxEPD_BLACK);
		display.drawRect(10, 171, 80, 19, GxEPD_BLACK);

		display.fillRect(10, 171, 80, 19, GxEPD_BLACK);

	}

	else if (RX_LEFT <= 300)
	{
		//Serial.println ("LEFT EMPTY"); //FILL RECT WITH RED

		display.drawRect(10, 55, 80,  24, GxEPD_BLACK);
		display.drawRect(10, 84, 80,  24, GxEPD_BLACK);
		display.drawRect(10, 113, 80, 24, GxEPD_BLACK);
		display.drawRect(10, 142, 80, 24, GxEPD_BLACK);
		display.drawRect(10, 171, 80, 19, GxEPD_BLACK);
		display.fillRect(10, 171, 80, 19, GxEPD_BLACK);
	}

	else if (RX_LEFT > 300)
	{
		//Serial.println ("LEFT EMPTY_2");
		display.drawRect(10, 55, 80,  24, GxEPD_BLACK);
		display.drawRect(10, 84, 80,  24, GxEPD_BLACK);
		display.drawRect(10, 113, 80, 24, GxEPD_BLACK);
		display.drawRect(10, 142, 80, 24, GxEPD_BLACK);
		display.drawRect(10, 171, 80, 19, GxEPD_BLACK);

		//display.fillRect(5, 50, 90, 145, GxEPD_RED);

		/*
		  display.fillRect(10, 55,  80, 24, GxEPD_RED);
		  display.fillRect(10, 84,  80, 24, GxEPD_RED);
		  display.fillRect(10, 113, 80, 24, GxEPD_RED);
		  display.fillRect(10, 142, 80, 24, GxEPD_RED);
		  display.fillRect(10, 171, 80, 19, GxEPD_RED);
		*/
	}


	delay(500);                                        //REQUIRED OR FAILS TO GET VL53 MEASUREMENT

	//RIGHT SALT BLOCK DISPLAY LEVELS

	if (RX_RIGHT <= 70)
	{ //100%
	  // display.fillRect(110,  55, 80, 24, GxEPD_BLACK);
		display.drawRect(110, 84, 80,  24, GxEPD_BLACK);
		display.drawRect(110, 113, 80, 24, GxEPD_BLACK);
		display.drawRect(110, 142, 80, 24, GxEPD_BLACK);
		display.drawRect(110, 171, 80, 19, GxEPD_BLACK);

		display.fillRect(110, 55, 80,  24, GxEPD_BLACK);
		display.fillRect(110, 84, 80,  24, GxEPD_BLACK);
		display.fillRect(110, 113, 80, 24, GxEPD_BLACK);
		display.fillRect(110, 142, 80, 24, GxEPD_BLACK);
		display.fillRect(110, 171, 80, 19, GxEPD_BLACK);

	}

	else if (RX_RIGHT <= 115)
	{ //80%
		display.drawRect(110, 55, 80,  24, GxEPD_BLACK);
		display.drawRect(110, 84, 80,  24, GxEPD_BLACK);
		display.drawRect(110, 113, 80, 24, GxEPD_BLACK);
		display.drawRect(110, 142, 80, 24, GxEPD_BLACK);
		display.drawRect(110, 171, 80, 19, GxEPD_BLACK);

		display.fillRect(110, 84, 80,  24, GxEPD_BLACK);
		display.fillRect(110, 113, 80, 24, GxEPD_BLACK);
		display.fillRect(110, 142, 80, 24, GxEPD_BLACK);
		display.fillRect(110, 171, 80, 19, GxEPD_BLACK);

	}

	else if (RX_RIGHT <= 160)
	{ //60%
		display.drawRect(110, 55, 80,  24, GxEPD_BLACK);
		display.drawRect(110, 84, 80,  24, GxEPD_BLACK);
		display.drawRect(110, 113, 80, 24, GxEPD_BLACK);
		display.drawRect(110, 142, 80, 24, GxEPD_BLACK);
		display.drawRect(110, 171, 80, 19, GxEPD_BLACK);

		display.fillRect(110, 113, 80, 24, GxEPD_BLACK);
		display.fillRect(110, 142, 80, 24, GxEPD_BLACK);
		display.fillRect(110, 171, 80, 19, GxEPD_BLACK);

	}
	else if (RX_RIGHT <= 205)
	{ //40%
		display.drawRect(110, 55, 80,  24, GxEPD_BLACK);
		display.drawRect(110, 84, 80,  24, GxEPD_BLACK);
		display.drawRect(110, 113, 80, 24, GxEPD_BLACK);
		display.drawRect(110, 142, 80, 24, GxEPD_BLACK);
		display.drawRect(110, 171, 80, 19, GxEPD_BLACK);

		display.fillRect(110, 142, 80, 24, GxEPD_BLACK);
		display.fillRect(110, 171, 80, 19, GxEPD_BLACK);

	}

	else if (RX_RIGHT <= 245)
	{ //20%
		display.drawRect(110, 55, 80,  24, GxEPD_BLACK);
		display.drawRect(110, 84, 80,  24, GxEPD_BLACK);
		display.drawRect(110, 113, 80, 24, GxEPD_BLACK);
		display.drawRect(110, 142, 80, 24, GxEPD_BLACK);
		display.drawRect(110, 171, 80, 19, GxEPD_BLACK);

		display.fillRect(110, 171, 80, 19, GxEPD_BLACK);

	}

	else if (RX_RIGHT <= 300)
	{
		//Serial.println ("RIGHT EMPTY");                       //TODO FILL RECT WITH RED
		display.drawRect(110, 55, 80,  24, GxEPD_BLACK);
		display.drawRect(110, 84, 80,  24, GxEPD_BLACK);
		display.drawRect(110, 113, 80, 24, GxEPD_BLACK);
		display.drawRect(110, 142, 80, 24, GxEPD_BLACK);
		display.drawRect(110, 171, 80, 19, GxEPD_BLACK);
		display.fillRect(110, 171, 80, 19, GxEPD_BLACK);
	}

	else if (RX_RIGHT > 300)
	{
		Serial.println("RIGHT EMPTY_2");
		display.drawRect(110, 55, 80,  24, GxEPD_BLACK);     //TODO FILL RECT WITH RED
		display.drawRect(110, 84, 80,  24, GxEPD_BLACK);
		display.drawRect(110, 113, 80, 24, GxEPD_BLACK);
		display.drawRect(110, 142, 80, 24, GxEPD_BLACK);
		display.drawRect(110, 171, 80, 19, GxEPD_BLACK);

		//display.fillRect(105, 50, 90, 145, GxEPD_RED);


		/*
		  display.fillRect(110, 55,  80, 24, GxEPD_RED);
		  display.fillRect(110, 84,  80, 24, GxEPD_RED);
		  display.fillRect(110, 113, 80, 24, GxEPD_RED);
		  display.fillRect(110, 142, 80, 24, GxEPD_RED);
		  display.fillRect(110, 171, 80, 19, GxEPD_RED);
		*/
	}

	delay(500); //REQUIRED OR FAILS TO GET VL53 MEASUREMENT
	display.update();


}