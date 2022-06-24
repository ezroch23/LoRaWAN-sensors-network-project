/*
 * Project: Wireless network sensor for environment parameters monitoring at the Universidad Tecnologica de Panama
 * Description: LoRaWAN node for measure environment parameters and send to The Thing Network server.
 * Author: Ezequiel Robles Chong
 * Links: http://lora-panama.com/
 * Date: 30 / 07 / 2020
 */


#include "Particle.h"				//Header necesario para utilizar funciones de Arduino.
#include <lmic.h>					//MCCI Library 
#include <hal/hal.h>				//MCCI Library 
#include <Wire.h>					//I2C Library for communication with TSL2561
#include <Adafruit_Sensor.h>		//Adafruit dependet Library for their sensors
#include <Adafruit_TSL2561_U.h>		//Light sensor Library
#include "DHT.h"					//Temperature and Humidity sensor Library

//Disable wifi dependency of Photon
SYSTEM_MODE(SEMI_AUTOMATIC);

 //LoRa 
 #define Payload_size        6		//Number of bytes to send
 #define TX_INTERVAL_REAL    20  	//Interval (in minutes) between one packet send and other. This variable is use as the interval for the micro to sleep

 //DHT
 #define DHTPIN D5					//DHT Pin
 #define DHTTYPE DHT22 				//DHT type (this nodes works with a DHT22)

 /* -- SENSORS POWER PIN -- */
 int dht_v = D6;

 /* -- MEASUREMENT VARIABLES -- */
 float light_lux{0.0};				//Luminosity
 float hum{0.0};       	  			//Humedity
 float temp{0.0};      	  			//Temperature

 /* -- PAYLOAD VARIABLES -- */
 uint16_t L{0};						//Luminosity
 uint16_t H{0};						//Humedity
 uint16_t T{0};						//Temperature

 //Sensors classes instantiation
 Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);
 DHT dht(DHTPIN, DHTTYPE);

//Aplication Identifier
static const u1_t PROGMEM APPEUI[8]= { 0x00, 0x00, 0x00, 0x00, 0x0, 0xD5, 0xB3, 0x70 }; 	//fill with your identifier
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}

//Device Identifier
static const u1_t PROGMEM DEVEUI[8]= { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; 	//fill with your identifier
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}

//Encryptation key
static const u1_t PROGMEM APPKEY[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; //fill with your key
void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}

//Define Payload
byte payload [Payload_size]{0x00};
static osjob_t sendjob;

//Transmission interval
const unsigned TX_INTERVAL = 1; //HERE IS SET TO 1, BECAUSE THE TRUE TX_INTERVAL IS IN THE TIME IN WHICH THE MICRO SLEEPS, WHICH IS TX_INTERVAL_REAL

//LoRa Tranceiver pins
const lmic_pinmap lmic_pins = {
  .nss   = A2, //
  .rxtx  = LMIC_UNUSED_PIN,
  .rst   = A1,
  .dio   = {D3, D2, LMIC_UNUSED_PIN},
};

//a function to get data and stored into payload buffer
 void CapturarData(){

	/**** LUMINOSITY ***/
	delay(3000);
	sensors_event_t event;
	tsl.getEvent(&event);
	if (event.light)
	{
		light_lux = event.light;
		Serial.print(F("Luminosity is: "));
		Serial.print(light_lux);
		Serial.println(F(" lux"));
		L = light_lux;
		//Store Data into payload
		payload [0] = highByte(L);
		payload [1] = lowByte(L);
	}
	else
	{
	  //If sensor overload, set Data to 0
	  payload [0] = 0x00;
	  payload [1] = 0x00;
	  Serial.print("Sensor Overload");
	}

	//------------------------------------------------------------------------
	/**** Temperature y Humedity ***/
	delay(5000);
	hum = dht.readHumidity();
	temp = dht.readTemperature();
	Serial.println("Humedad: ");
	Serial.print(hum);
	Serial.println(" %");
	Serial.print("Temperatura: ");
	Serial.print(temp);
	Serial.println(" C");
	
	/* after measure the value, it is multiply by 100 to move the decimal part to the int part, then this can be store as a Integer number.
	the opposite operation should be apply on the server side */
	H = hum*100;
	T = temp*100;
	
	//The highByte extract the high-order (leftmost) byte of a word, lowByte extracts the low-order (rightmost) byte of a variable 
	payload [2] = highByte(H);   //Integer part
	payload [3] = lowByte(H);    //Decimal part
	payload [4] = highByte(T);   //Integer part
	payload [5] = lowByte(T);    //Decimal part
  }


void printHex2(unsigned v) {
	v &= 0xff;
	if (v < 16)
		Serial.print('0');
	Serial.print(v, HEX);
}

// Event Handler. Look the example of MCCI lib
void onEvent (ev_t ev) {
	Serial.print(os_getTime());
	Serial.print(": ");
	switch(ev) {
		case EV_SCAN_TIMEOUT:
			Serial.println(F("EV_SCAN_TIMEOUT"));
			break;
		case EV_BEACON_FOUND:
			Serial.println(F("EV_BEACON_FOUND"));
			break;
		case EV_BEACON_MISSED:
			Serial.println(F("EV_BEACON_MISSED"));
			break;
		case EV_BEACON_TRACKED:
			Serial.println(F("EV_BEACON_TRACKED"));
			break;
		case EV_JOINING:
			Serial.println(F("EV_JOINING"));
			break;
		case EV_JOINED:
			Serial.println(F("EV_JOINED"));
			{
			  u4_t netid = 0;
			  devaddr_t devaddr = 0;
			  u1_t nwkKey[16];
			  u1_t artKey[16];
			  LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
			  Serial.print("netid: ");
			  Serial.println(netid, DEC);
			  Serial.print("devaddr: ");
			  Serial.println(devaddr, HEX);
			  Serial.print("AppSKey: ");
			  for (size_t i=0; i<sizeof(artKey); ++i) {
				if (i != 0)
				  Serial.print("-");
				printHex2(artKey[i]);
			  }
			  Serial.println("");
			  Serial.print("NwkSKey: ");
			  for (size_t i=0; i<sizeof(nwkKey); ++i) {
					  if (i != 0)
							  Serial.print("-");
					  printHex2(nwkKey[i]);
			  }
			  Serial.println();
			}
			LMIC_setLinkCheckMode(0);
			break;
		case EV_JOIN_FAILED:
			Serial.println(F("EV_JOIN_FAILED"));
			break;
		case EV_REJOIN_FAILED:
			Serial.println(F("EV_REJOIN_FAILED"));
			break;
		case EV_TXCOMPLETE:
			Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
			if (LMIC.txrxFlags & TXRX_ACK)
			  Serial.println(F("Received ack"));
			if (LMIC.dataLen) {
			  Serial.print(F("Received "));
			  Serial.print(LMIC.dataLen);
			  Serial.println(F(" bytes of payload"));
			}
			//Here the micro is set to sleep for the time we set as TX_INTERVAL_REAL (20 mins to comply with TTN fair policy) 
			//so when it awakes, the next transmission will be scheduled.
			System.sleep(D4,RISING,TX_INTERVAL_REAL);
			//Scheduled next transmission, which will be send after the next "TX_INTERVAL" (1 sec, because the time the micro was sleeping complies the policy)
			os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
			break;
		case EV_LOST_TSYNC:
			Serial.println(F("EV_LOST_TSYNC"));
			break;
		case EV_RESET:
			Serial.println(F("EV_RESET"));
			break;
		case EV_RXCOMPLETE:
			// data received in ping slot
			Serial.println(F("EV_RXCOMPLETE"));
			break;
		case EV_LINK_DEAD:
			Serial.println(F("EV_LINK_DEAD"));
			break;
		case EV_LINK_ALIVE:
			Serial.println(F("EV_LINK_ALIVE"));
			break;
		case EV_TXSTART:
			Serial.println(F("EV_TXSTART"));
			break;
		default:
			Serial.print(F("Unknown event: "));
			Serial.println((unsigned) ev);
			break;
	}
}

void do_send(osjob_t* j){
	// Check if there is not a current TX/RX job running
	if (LMIC.opmode & OP_TXRXPEND) {
		Serial.println(F("OP_TXRXPEND, not sending"));
	} else {
		CapturarData();
		// Prepare upstream data transmission at the next possible time.
		LMIC_setTxData2(1, payload, sizeof(payload), 0);
		Serial.println(F("Packet queued"));
	}
	// Next TX is scheduled after TX_COMPLETE event.
}

void setup() {
	delay(5000);
	while (!Serial);
	Serial.begin(9600);
	Serial.println(F("Starting..."));
	//Initialize sensor power pin
	pinMode(dht_v, OUTPUT);
	digitalWrite(dht_v, HIGH);

	//Initialize sensors libreries
	tsl.begin();	//TSL2561
	dht.begin();	//DHT

	//TSL2561 config 
	tsl.enableAutoRange(true);            /* Auto-gain ... switches automatically between 1x and 16x */
	tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit data but slowest conversions */

	//Initialize LMIC lib
	os_init();
	
	// Reset the MAC state. Session and pending data transfers will be discarded.
	LMIC_reset();

	// in the US, with TTN, it saves join time if we start on subband 1 (channels 8-15). This will
	// get overridden after the join by parameters from the network. If working with other
	// networks or in other regions, this will need to be changed.
	LMIC_selectSubBand(1);

	// Disable link check validation
	LMIC_setLinkCheckMode(0);

	// TTN uses SF9 for its RX2 window.
	LMIC.dn2Dr = DR_SF9;

	// Set data rate and transmit power for uplink
	LMIC_setDrTxpow(DR_SF7,14);

	// Start job
	do_send(&sendjob);
}

void loop() {
  // we call the LMIC's runloop processor. This will cause things to happen based on events and time. One
  // of the things that will happen is callbacks for transmission complete or received messages. We also
  // use this loop to queue periodic data transmissions.  You can put other things here in the `loop()` routine,
  // but beware that LoRaWAN timing is pretty tight, so if you do more than a few milliseconds of work, you
  // will want to call `os_runloop_once()` every so often, to keep the radio running.
	os_runloop_once();
}
