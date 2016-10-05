#include <LiquidCrystal_I2C.h>
#include <LibAPRS.h>
#include <TinyGPSplus.h>
#include <DRA818.h>
#include <SoftwareSerial.h>

#define VERSION "Beta-0.96g"

#define ADC_REFERENCE REF_5V

#define OPEN_SQUELCH false

#define OPTION_LCD  true

#define CALLSIGN "KK6DF"
#define SSID 2
#define PTT 3 
#define FREQ 144.39
#define UPDATE_BEACON 120000L
#define UPDATE_BEACON_INIT  10000L 

#define NOP __asm__ __volatile__("nop\n\t")

int beacon_init_count = 100 ;

unsigned long update_beacon = UPDATE_BEACON_INIT ;

unsigned long lastupdate = 0 ;

int sent_count=0 ;

#ifdef OPTION_LCD
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7 , 3, POSITIVE); 
#endif 

// GPS setup

TinyGPSPlus gps ;

// DRA818 setup
#define DRA_RXD A1
#define DRA_TXD A2
SoftwareSerial dra_serial(DRA_RXD,DRA_TXD);
DRA818 dra(&dra_serial, PTT);

boolean gotPacket = false ;
int recv_count = 0 ;
AX25Msg incomingPacket;
uint8_t *packetData;

void aprs_msg_callback(struct AX25Msg *msg)
{
   if(!gotPacket) {
      gotPacket = true ;
      memcpy(&incomingPacket, msg, sizeof(AX25Msg));
      if (freeMemory() > msg->len) {
         packetData = (uint8_t*) malloc(msg->len);
         memcpy(packetData,msg->info,msg->len);
         incomingPacket.info=packetData;
      } else {
         gotPacket = false ;
      }
   }
}


void processPacket()
{
   if(gotPacket) {
      gotPacket = false ;
      recv_count++ ;
      free(packetData) ;
   }
}


void locationUpdate(const char *lat, const char *lon,int height = 0 ,int power=1, int gain=0, int dir=0)
{
   APRS_setLat(lat);
   APRS_setLon(lon);
   APRS_setHeight(height);
   APRS_setPower(power);
   APRS_setGain(gain);
   APRS_setDirectivity(dir);

   char *comment = "LART-1 Tracker" ;
   APRS_sendLoc(comment, strlen(comment));
}


void mydelay(unsigned long msec) {
    unsigned long future = millis() + msec ;
    while ( (future - millis()) > 0 ) {
        NOP ;
    } ;
    return ;  
}


void setup()
{

   mydelay(1000UL) ;
#ifdef OPTION_LCD
   lcd.begin(16,2);
   lcd.clear();
   lcd.print(F("LART/1 APRS BEAC")); 
   lcd.setCursor(0,1);
   lcd.print(F("Call: ")) ;
   lcd.print(CALLSIGN) ;
   lcd.print("-") ;
   lcd.print(SSID) ;
#endif

   dra_serial.begin(9600);
   Serial.begin(4800) ;

   mydelay(2000UL) ;
#ifdef OPTION_LCD
   lcd.clear();
   lcd.print(F("Version:"));
   lcd.setCursor(0,1);
   lcd.print(VERSION);
#endif
   mydelay(1000UL);
   // DRA818 Setup
   // must set these, then call WriteFreq
   dra.setFreq(FREQ);
   dra.setTXCTCSS(0);
   dra.setSquelch(5);
   dra.setRXCTCSS(0);
   dra.setBW(0); // 0 = 12.5k, 1 = 25k
   dra.writeFreq();
   mydelay(3000UL);
#ifdef OPTION_LCD
   lcd.clear();
   lcd.print(F("Freq Set"));
#endif 
   dra.setVolume(1);
   mydelay(3000UL);
#ifdef OPTION_LCD
   lcd.clear();
   lcd.print(F("Vol Set"));
#endif
   dra.setFilters(false, true, true);
   mydelay(3000UL);
   lcd.clear();
   lcd.print(F("Filter Set"));
   dra.setPTT(LOW);
   mydelay(3000UL);
#ifdef OPTION_LCD
   lcd.clear();
   lcd.print(F("F:")) ;
   lcd.print(FREQ) ;
   lcd.setCursor(0,1) ;
   lcd.print(F(" TC: ")) ;
   lcd.print(0) ;
   lcd.print(F(" RC: ")) ;
   lcd.print(0) ;
#endif
   mydelay(2000UL) ;

   APRS_init(ADC_REFERENCE, OPEN_SQUELCH);
   APRS_setCallsign(CALLSIGN, SSID);
   APRS_setDestination("APZMDM",0);
   APRS_setPath1("WIDE1",1);
   APRS_setPath2("WIDE2",1);
   APRS_setPreamble(350L);
   APRS_setTail(50L);
   APRS_setSymbol('n');

#ifdef OPTION_LCD
   lcd.clear();
   lcd.print(F("APRS setup")) ;
   mydelay(2000UL) ;
   lcd.clear();
   lcd.print(F("Setup Complete")) ;
#endif
   mydelay(2000UL) ;

}

char  lat[] =  "0000.00N" ;
char  lon[] = "00000.00W" ;
int h = 0 ;

void loop()
{


   while(Serial.available() > 0 ) {
       if(gps.encode(Serial.read())) {
           if( gps.location.isValid()) {

               // latitude
               if ( gps.location.rawLat().negative ) {
                   dtostrf(-100* gps.location.lat(),7,2,lat) ;
                   lat[7] = 'S';
               } else {
                   dtostrf(100*gps.location.lat(),7,2,lat) ;
                   lat[7] = 'N';
               }
               // longitude
               if ( gps.location.rawLng().negative ) {
                   dtostrf(-100*gps.location.lng(),8,2,lon) ;
                   lon[8] = 'W';
               } else {
                   dtostrf(100*gps.location.lng(),8,2,lon) ;
                   lon[8] = 'E';
               }

           }

           // altitude 
           if ( gps.altitude.isValid() ) {
               h = (int) gps.altitude.meters() ;
           }

       }
   }

   if ( millis() - lastupdate > update_beacon ) {
     lastupdate = millis() ;
     if ( beacon_init_count-- <= -1 ) {
        update_beacon = UPDATE_BEACON ;
     }
     // const char * lat =  "3742.44N" ;
     // const char * lon = "12157.54W" ;
     // 

     if ( gps.location.isValid() )  {
#ifdef OPTION_LCD
         lcd.clear();
         lcd.print(F("sending packet"));
#endif
         locationUpdate(lat,lon,h,1,0,0) ;
         // mydelay(250UL);
         // dra.setPTT(LOW);
         sent_count++ ;
#ifdef OPTION_LCD
         lcd.clear();
         lcd.print(F("s: ")) ;
         lcd.print(sent_count) ;
         lcd.print(F(" r: ")) ;
         lcd.print(recv_count) ;
         lcd.setCursor(0,1);
         lcd.print(lat) ;
#endif
     } else {
         lcd.clear();
         lcd.print(F("gps not ready")) ;
     } 
   }

   processPacket() ;

}
