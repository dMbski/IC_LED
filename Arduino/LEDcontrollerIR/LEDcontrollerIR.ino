/*
---------------------------------------------------------------------------------
 LED WS2812b controller with IR remote (2014.12.08)
 by dMb (dbbrzozowski (at) tlen.pl)
 https://github.com/dMbski/IC_LED
 ---------------------------------------------------------------------------------                                          
 Written in a hurry for Arduino 1.0.6 + board with ATmega328  
 
 Needed and used libraries (BIG THX to its CREATORS):
 - FastLED version 3.000.002
 http://fastled.io
 - IRremote Version 0.1
 http://www.righto.com/2009/08/multi-protocol-infrared-remote-library.html
 - Arduino EEPROMex library 
 http://thijs.elenbaas.net/2012/07/extended-eeprom-library-for-arduino
 
 All copyrights belong to their respective owners.
 */


#include <IRremote.h>
#include <FastLED.h>
#include <EEPROMex.h>
#include <EEPROMVar.h>

//global definitions 
#define  BNONE  0
#define  BNEXT  1
#define  BPREV  2
#define  BMENU  4

//definitions for EEPROM
#define EEMARK  0xEE
struct ParamEE {
  byte EEmark;
  byte EEno;    //seting are saved in 5 succeeding locations
  byte EEBrightnes;
  byte EEEffect;
  int  EEPeriod;
};
ParamEE Param ={
  EEMARK, 0,0,0,0};

//definitions for IR receiver
#define RECV_PIN  11
IRrecv irrecv(RECV_PIN);
decode_results results;

#define IRCODE_BNEXT  0x40FF8A75
#define IRCODE_BMENU  0x40FF58A7
#define IRCODE_BPREV  0x40FF0AF5
#define IRCODE_FASTER  0x40FFAA55
#define IRCODE_SLOWER  0x40FF2AD5
#define IRCODE_LIGHTER  0x40FF28D7
#define IRCODE_DARKER   0x40FFE817
#define IRCODE_ENTSET
#define IRCODE_SAVBRIGHT
#define IRCODE_SAVEEFFECT
#define IRCODE_REPEAT  0xFFFFFFFF

byte IRButton= BNONE;

//definitions for LED strip 
#define LED_COUNT 20
#define LED_DATA_PIN  13
//add clock pin def for other IC LEDs

byte LEDEffect= 0;    //default effect at start
byte LEDEffectChange= true;
unsigned long RefreshLEDAt=0;
struct CRGB LEDData[LED_COUNT];
byte LEDBright= 160;  //default brightnes at start


//      program definitions
#define DEBUGMODEON    //uncomment this for Serial.print
#ifdef DEBUGMODEON        //serial print enable
#define PRINT_START Serial.begin(57600)
#define PRINTD(x)    Serial.print(F(x))    //to disable F() macro just delete it
#define PRINTVARH(x, y)    Serial.print(x); Serial.print(y, HEX)
#define PRINTVAR(x, v)  Serial.print(x); Serial.print(v)
#else                     //serial print disable
#define PRINTD(x)
#define PRINTH(x)
#define PRINTVAR(x, v)
#define PRINT_START
#endif

#define BUTTON_NEXT_PIN  5
#define BUTTON_MENU_PIN  4
#define BUTTON_PREV_PIN  3

byte ActiveButton = BNONE;   
unsigned long ReadButtonAt=0;
#define READING_PERIOD 150

#define  MODE_PLAY  0        //normal mode -play bnext/bprev= effect change
#define  MENUCONFIRMSAVE  5  //must be less then next-first speciall  mode
#define  MODE_SETBR  10      //enter setting brightness (then save for default)
#define  MODE_X      20      //test mode  

byte IsModeChange= false;

byte RunMode= MODE_PLAY;
byte BMenuCount= 0;     
#define  MODE_TIMEOUT  15000  //exit to modeplay after millis
unsigned long ModeChangeAt= 0;

//proto
void SetColorAll(byte r, byte g, byte b);
void BlinkShort();
void PlayEffect();
#define MAXEFFECT_PERIOD  1000
#define MINEFFECT_PERIOD  75
#define EFFECTS_COUNT  9            //how many effect is (last no at case in func playeffect)
int EffectPeriod= MAXEFFECT_PERIOD; // millis to next step in effect and refresh leds

//var to use in playeffect function
byte EffectSteep;
char EffectFase;
char EffectDir;    


void setup()
{
  PRINT_START;

  randomSeed(analogRead(0));

  irrecv.enableIRIn();
  pinMode(BUTTON_NEXT_PIN, INPUT);
  pinMode(BUTTON_MENU_PIN, INPUT);
  pinMode(BUTTON_PREV_PIN, INPUT);

  pinMode(LED_DATA_PIN, OUTPUT);

  LEDS.addLeds<WS2812B, LED_DATA_PIN, GRB>(LEDData, LED_COUNT); // here u can change other type of led ic see in FastLED
  SetColorAll(255, 255, 255);
  LEDS.setBrightness(LEDBright);
  LEDS.show();
  //Dumb EEprom settings
  Param.EEBrightnes= 55;
  PRINTVAR(F("\r\n EEno: "), Param.EEno);
  PRINTVARH(F("\tEEmark: "), Param.EEmark);
  PRINTVAR(F("\r\nEEBrightnes: "), Param.EEBrightnes);
  PRINTVAR(F("\tEEEffect: "), Param.EEEffect);
  PRINTVAR(F("\tEEPeriod: "), Param.EEPeriod);
  //Starting info printout
  PRINTD("\r\nLEDcontrollerIR Setup finish with:");
  PRINTVAR(F("\r\nLEDCount: "), LED_COUNT);
  PRINTVAR(F("\tLED Effects: "), EFFECTS_COUNT);
  PRINTVAR(F("\r\nLEDBright="), LEDBright);
  PRINTVAR(F("\r\nLEDEffect="), LEDEffect);  
}//end setup

void loop() 
{

  //button reading
  if (millis()>ReadButtonAt)
  {
    byte hbutton= BNONE;
    if(digitalRead(BUTTON_NEXT_PIN)) hbutton=  BNEXT;
    if(digitalRead(BUTTON_MENU_PIN)) hbutton=  hbutton+BMENU;
    if(digitalRead(BUTTON_PREV_PIN)) hbutton=  hbutton+BPREV;
    ActiveButton= hbutton;   
    ReadButtonAt= millis()+READING_PERIOD;    
  }//end if readbuttonat

  //ir command reading
  if (irrecv.decode(&results))
  {
    byte hbutton= BNONE;
    //PRINTVARH("\nIRcode=", results.value);   //uncomment to get RAW IR code + DEBUGMODE
    switch(results.value)    //add here IR commands for enhanced settings during playmode
    {
    case  IRCODE_BNEXT:
      hbutton= BNEXT;
      break;
    case  IRCODE_BMENU:
      hbutton=  BMENU;
      break;
    case  IRCODE_BPREV:
      hbutton=  BPREV;
      break;
    case  IRCODE_FASTER:
      EffectPeriod= EffectPeriod-5;
      if (EffectPeriod<MINEFFECT_PERIOD) EffectPeriod= MINEFFECT_PERIOD;
      PRINTVAR("\r\nEffectPeriod=", EffectPeriod); 
      break;
    case  IRCODE_SLOWER:
      EffectPeriod= EffectPeriod+5;
      if (EffectPeriod>MAXEFFECT_PERIOD) EffectPeriod= MAXEFFECT_PERIOD;
      PRINTVAR("\r\nEffectPeriod=", EffectPeriod);
      break;
    case  IRCODE_LIGHTER:
      LEDBright++;
      LEDBright++;
      PRINTVAR("\r\nLEDBright=", LEDBright);
      break; 
    case  IRCODE_DARKER:
      LEDBright--;
      LEDBright--;
      PRINTVAR("\r\nLEDBright=", LEDBright);
      break;      
    }//end switch  
    irrecv.resume();
    IRButton= hbutton;
  }//end if IR results
  else
  {
    IRButton= BNONE;
  }

  //button processing
  if ((ActiveButton>BNONE) || (IRButton>BNONE))
  {
    if ((ActiveButton==BMENU) || (IRButton==BMENU))
    {
      BMenuCount++;
      ModeChangeAt= millis()+MODE_TIMEOUT; 
      switch (RunMode)
      {
      case  MODE_PLAY:        
        if (BMenuCount== MODE_SETBR)
        {
          RunMode= MODE_SETBR;
          IsModeChange= true;
          PRINTD("\r\nEnter mode MODE_SETBR...");
          BlinkShort();
        }
        break;
      case  MODE_SETBR:
        if (BMenuCount== MENUCONFIRMSAVE)
        {
          BMenuCount= 0;
          RunMode= MODE_PLAY;
          IsModeChange= true;
          PRINTD("\r\nSave to EEPROM from mode MODE_SETBR..."); 
          BlinkShort();          
        }
        else if (BMenuCount== MODE_X)
        {
          RunMode=MODE_X;
          IsModeChange= true;
        }
        break;
      case  MODE_X:
        PRINTD("Enter MODE_X");  //for example
        break;
      }  
    }//end if bmenu
    if ((ActiveButton==BNEXT) || (IRButton==BNEXT))
    {
      ModeChangeAt= millis()+MODE_TIMEOUT;
      BMenuCount= 0;
      switch (RunMode)
      {
      case  MODE_PLAY:
        LEDEffect++;
        LEDEffectChange= true;       
        break;
      case  MODE_SETBR:
        LEDBright++;
        LEDS.setBrightness(LEDBright);
        LEDS.show(); 
        PRINTVAR("\r\nLEDBright:", LEDBright);         
        break;
      }//end switch
    }//end if bnext 
    if ((ActiveButton==BPREV) || (IRButton==BPREV))
    {
      ModeChangeAt= millis()+MODE_TIMEOUT;
      BMenuCount= 0;
      switch (RunMode)
      {
      case  MODE_PLAY:
        LEDEffect--;
        LEDEffectChange= true;      
        break;
      case  MODE_SETBR:
        LEDBright--;
        LEDS.setBrightness(LEDBright);
        LEDS.show();
        PRINTVAR("\r\nLEDBright:", LEDBright);         
        break;
      }//end switch        
    }//end if bprev       
    ActiveButton= BNONE;  //buttons was processed
    IRButton= BNONE;
    if (LEDEffectChange)
    {  
      if (LEDEffect>EFFECTS_COUNT) LEDEffect= EFFECTS_COUNT;
      PRINTVAR("\r\nLEDEffect:", LEDEffect);       
    }
  }//enf if buttonprocessing
  else   
  {
    if (RunMode>MODE_PLAY)
    {
      if (millis()>ModeChangeAt)
      {
        RunMode= MODE_PLAY;
        IsModeChange= true;
        BMenuCount =0;
        PRINTD("\r\nExit to MODE_PLAY.");
        BlinkShort();
        BlinkShort();
        BlinkShort();
      }
    }//end return to playmode
  }//end else button

  // running loop for leds
  switch (RunMode)
  {
  case  MODE_PLAY: //just go to func for select effects to play
    if (IsModeChange)
    {
      IsModeChange= false;
      LEDEffectChange= true;
    }
    else
    {
      if (LEDEffectChange)    //goes here when effect is changing and setup leds data and vars for effect
      {//setups 4 new effect
        BlinkShort();
        RefreshLEDAt= millis();
        LEDEffectChange= false;
        EffectPeriod= 250;
        EffectSteep= 0;
        EffectFase= 0;
        EffectDir= 1;
        switch(LEDEffect)
        {
        case 0:
          SetColorAll(208, 160, 64);
          break;
        case 1:
          break;
        case 2:
          break;
        case 3:
          break;
        case 4:
          EffectFase= 0;
          EffectPeriod= 333;
          break;
        case 5:
          EffectSteep= 1;
          EffectPeriod= 125;
          break;
        case 6:
          EffectSteep= LED_COUNT-1;
          EffectPeriod= 125;
          break;
        case  7:
          EffectSteep= LED_COUNT-1;
          EffectPeriod= 125;
          break;  
        case  8:
          SetColorAll(16, 16, 16);
          break; 
        case  9:
          SetColorAll(random(127), random(127), random(127));
          break;           
        }
        PRINTD("\r\nLEDEffect begin.");
        PRINTVAR("\tEffectPeriod=", EffectPeriod);      
      }      
      PlayEffect();
    }
    break;
  case  MODE_SETBR://special mode to setup brightness
    if (IsModeChange)
    {
      SetColorAll(255, 255, 255);
      LEDS.show();
      IsModeChange= false;
    }  
    break;
  case  MODE_X:
    break;
  }
}// end main loop



//functions
void SetColorAll(byte r, byte g, byte b)
{
  for (byte i=0; i< LED_COUNT; i++)
  {
    LEDData[i].r=r;
    LEDData[i].g=g;
    LEDData[i].b=b;
  }  
}

void BlinkShort()
{
  LEDS.setBrightness(220);
  LEDS.show();
  delay(50);
  LEDS.setBrightness(LEDBright/3);
  LEDS.show();
  delay(50);
  LEDS.setBrightness(LEDBright);
  LEDS.show();
}

byte GetNextLED(byte no)
{
  byte ret;
  ret = no+1;
  if (ret>(LED_COUNT-1)) ret=(LED_COUNT-1);
  return ret;
}
byte GetPrevLED(int no)
{
  no--;
  if (no < 1) return 0;
  else return (byte(no));
}
void PlayEffect()
{
  if (millis()< RefreshLEDAt)  return;
  static byte curr;
  static byte curg;
  static byte curb;
  switch(LEDEffect)
  {
  case 0:                                      //random led color change with a gleam of all leds
    byte i;
    i= random(LED_COUNT);
    LEDData[i].r= EffectSteep+(128/LED_COUNT)*i;
    LEDData[i].g= EffectSteep+LEDData[i].r-i;
    LEDData[i].b= EffectSteep+LEDData[i].r/4-i;
    EffectSteep++;
    //randomSeed(i+LEDData[i].r+EffectSteep);
    i= random(1, LED_COUNT);
    LEDData[i].b= LEDData[i-1].b;
    EffectPeriod= MINEFFECT_PERIOD+ i*10;
    int nbr;
    nbr= LEDBright;
    nbr= nbr -(3-(i/3))*10;
    LEDS.setBrightness(nbr);  
    break;
  case 1:                                      //reds with one led march
    SetColorAll(128, 0, 0);
    LEDData[EffectSteep].r= 255;
    if (EffectSteep==(LED_COUNT-1)) EffectSteep=0;
    else EffectSteep= GetNextLED(EffectSteep);  
    break;
  case 2:                                      //greens with one led march
    SetColorAll(0, 128, 0);
    LEDData[EffectSteep].g= 255;
    if (EffectSteep==(LED_COUNT-1)) EffectSteep=0;
    else EffectSteep= GetNextLED(EffectSteep);  
    break;
  case 3:                                      //blues with one led march
    SetColorAll(0, 0, 128);
    LEDData[EffectSteep].b= 255;
    if (EffectSteep==(LED_COUNT-1)) EffectSteep=0;
    else EffectSteep= GetNextLED(EffectSteep);
    break;  
  case 4:                                      //slooow all leds collors fade with random led flickering
    for (byte i=0; i<LED_COUNT; i++)
    {
      switch (EffectFase)
      {
      case 0:
        LEDData[i].r= EffectSteep;
        LEDData[i].g= 0;
        LEDData[i].b= 0;
        break;
      case 1:
        LEDData[i].r=254;
        LEDData[i].g= EffectSteep;
        LEDData[i].b= 0;
        break;
      case 2:
        LEDData[i].r=253;
        LEDData[i].g=253;
        LEDData[i].b= EffectSteep;
        break;
      case 3:
        LEDData[i].r= 255-EffectSteep;
        LEDData[i].g=253;
        LEDData[i].b=253;          
        break;
      case 4:
        LEDData[i].r= 0;
        LEDData[i].g= 255-EffectSteep;
        LEDData[i].b=253;
        break;
      case 5:
        LEDData[i].r= EffectSteep;
        LEDData[i].g= 0;
        LEDData[i].b=253;
        break;  
      case 6:
        LEDData[i].r= 255-EffectSteep;
        LEDData[i].g= EffectSteep;
        LEDData[i].b= 255-EffectSteep;
        break;
      case 7:
        LEDData[i].g= 255-EffectSteep;
        LEDData[i].r= EffectSteep;
        LEDData[i].b= 0;
        break;

      }//end switch
    }//end for

    EffectSteep++;
    //randomSeed(EffectSteep);
    i= random(LED_COUNT);
    LEDData[i].r= LEDData[i].r*2;
    LEDData[i].g= 127+random(128);
    LEDData[i].b= random(255);
    if (EffectSteep>254)
    {
      EffectFase++;
      EffectSteep=0;
    }
    if (EffectFase>7) EffectFase=1;
    break;   
  case 5:                                        //all random to one color from center+ bright fade
    byte ledst;
    ledst= EffectSteep/2;
    LEDData[(LED_COUNT/2)-ledst-1].r= curr;
    LEDData[(LED_COUNT/2)-ledst-1].g= curg;
    LEDData[(LED_COUNT/2)-ledst-1].b= curb;

    LEDData[(LED_COUNT/2)+ledst].r= curr;
    LEDData[(LED_COUNT/2)+ledst].g= curg;
    LEDData[(LED_COUNT/2)+ledst].b= curb;   
    byte brighst;
    brighst= LEDBright/(LED_COUNT/2);
    if (EffectDir) LEDS.setBrightness(2+brighst*EffectSteep);
    else LEDS.setBrightness((((brighst*LED_COUNT)-LEDBright)+LEDBright)-brighst*EffectSteep);
    EffectSteep++;
    if (EffectSteep > (LED_COUNT/2)) EffectDir= 0;    
    if (EffectSteep >= LED_COUNT)
    {
      EffectSteep= 1;
      EffectDir= 1;
      for (byte i=0; i<LED_COUNT; i++)
      {
        LEDData[i].r= 192+random(63);
        LEDData[i].g= random(255);
        LEDData[i].b= 16+i;
      }
      curr= random(255);
      curg= random(255);
      curb= random(255);
      //randomSeed(LEDData[(LED_COUNT/2)].b+EffectSteep);
    }
    break;//end effect 5 
  case 6:                                            //last led random collor to first led march
    LEDData[EffectSteep-1].r= curr;
    LEDData[EffectSteep-1].g= curg;
    LEDData[EffectSteep-1].b= curb;
    EffectSteep--;
    if (EffectSteep<1)
    {
      EffectSteep= LED_COUNT-1;
      //EffectFase++;
      //randomSeed(EffectFase*EffectFase);
      curr=random(255);
      curg=random(255);
      curb=random(255);
    }
    break;//end effect 6
  case 7:                                            //fade G +rand RB  from random position to last + var speed
    if (EffectSteep >= LED_COUNT)
    {
      EffectSteep= random(LED_COUNT);
      curr=random(255);
      curg=EffectFase++;
      curb=random(255);
      EffectPeriod= MINEFFECT_PERIOD+ EffectSteep*5;    
    }
    else
    {
      LEDData[EffectSteep].r= curr;
      LEDData[EffectSteep].g= curg;
      LEDData[EffectSteep].b= curb;
      LEDData[GetNextLED(EffectSteep)].r= curr*2;
      LEDData[GetNextLED(EffectSteep)].g= curg*2;
      LEDData[GetNextLED(EffectSteep)].b= curb*2;
      LEDData[GetPrevLED(EffectSteep)].r= curr/2;
      LEDData[GetPrevLED(EffectSteep)].g= curg/2;
      LEDData[GetPrevLED(EffectSteep)].b= curb/2;      
      EffectSteep++;
    }
    break;//end effect 7 
  case  8:                                            //fade white +random glem
    LEDData[EffectSteep].r= EffectFase;
    LEDData[EffectSteep].g= EffectFase;
    LEDData[EffectSteep].b= EffectFase++;
    EffectSteep= random(LED_COUNT-1);
    LEDData[EffectSteep].r= LEDData[EffectSteep].r*2;
    LEDData[EffectSteep].g= LEDData[EffectSteep].g*2;
    LEDData[EffectSteep].b= LEDData[EffectSteep].b*2;

    break;//end effect 8 
  case  9:                                            //random color, press nexteffect to change next +random glem
    LEDData[EffectSteep].r= LEDData[EffectSteep].r/2;
    LEDData[EffectSteep].g= LEDData[EffectSteep].g/2;
    LEDData[EffectSteep].b= LEDData[EffectSteep].b/2;
    EffectSteep= random(LED_COUNT-1);
    LEDData[EffectSteep].r= LEDData[EffectSteep].r*2;
    LEDData[EffectSteep].g= LEDData[EffectSteep].g*2;
    LEDData[EffectSteep].b= LEDData[EffectSteep].b*2;  
    break;//end effect 9 
  }//end switch

  LEDS.show();
  RefreshLEDAt= millis()+EffectPeriod;
}






















