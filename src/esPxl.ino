#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <NeoPixelBus.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED Display
#define OLED_CS     0  // Pin 19, CS - Chip select
#define OLED_DC     15   // Pin 20 - DC digital signal
#define OLED_RESET  16  // Pin 15 -RESET digital signal
Adafruit_SSD1306 display(OLED_DC, OLED_RESET, OLED_CS);
int updateDsp = false;

//WIFI
const char *ssid = "FLITZBox";
const char *password = "7567484214953532";

//UDP instance
WiFiUDP Udp;
const IPAddress outIP;
const unsigned int outPort = 9000;
const unsigned int localPort = 8000;

//LED strip
#define NUMPIXELS 50
#define PIXELPIN 2
//NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart800KbpsMethod> pixels(NUMPIXELS, PIXELPIN);
NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod>  pixels(NUMPIXELS, PIXELPIN);

bool updatePxl = false;
int apixl;
RgbColor rgbC;

//Rotary Encoder
#define ENCODERPIN1 12
#define ENCODERPIN2 5
#define ENCODERPINB 4
int lastMSB = 0;
int lastLSB = 0;
volatile int lastEncoded = 0;
volatile long encoderValue = 0;
int encoderVal = 0;
int encoderTmp = 0;

//menu
#define MMODES 6
int mMode = 0;
bool mChange = false;
int modeVals[MMODES];
int modeValsMax[MMODES];
int modeValsMin[MMODES];
int mValTmp;
const char* modeNames[MMODES];

void wipePxlRGB(int R, int G, int B){
  for(int i = 0; i < NUMPIXELS; ++i){
    pixels.SetPixelColor(i, RgbColor(R,G,B));
  }
  pixels.Show();
}

void wipePxlHSL(float H, float S, float L){
  for(int i = 0; i < NUMPIXELS; ++i){
    pixels.SetPixelColor(i, HsbColor(H,S,L));
  }
  //rgbC = RgbColor(HsbColor(H,S,L));
  pixels.Show();
}

void wipePxlHC(float H, float Cyc, float L){
  float ci = Cyc/NUMPIXELS;
  for(int i = 0; i < NUMPIXELS; ++i){
    pixels.SetPixelColor(i, HsbColor(H+ci*i,1.0f,L));
  }
  pixels.Show();
}

void updateEncoder(){
  int MSB = digitalRead(ENCODERPIN1); //MSB = most significant bit
  int LSB = digitalRead(ENCODERPIN2); //LSB = least significant bit
  int encoded = (MSB << 1) |LSB; //converting the 2 pin value to single number
  int sum  = (lastEncoded << 2) | encoded; //adding it to the previous encoded value
  if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue ++;
  if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue --;

  if (encoded == 3) {
    modeVals[mMode] += encoderValue>>2;
    if(modeVals[mMode] > modeValsMax[mMode])
      modeVals[mMode] = modeValsMax[mMode];
    if(modeVals[mMode] < modeValsMin[mMode])
      modeVals[mMode] = modeValsMin[mMode];
    encoderValue = 0;
    updatePxl = true;
    updateDsp = true;
  }
  lastEncoded = encoded; //store this value for next time
}

void setup() {

  // OLED Display Init
	display.begin(SSD1306_SWITCHCAPVCC);
	display.clearDisplay();
	display.setTextSize(1);
	display.setTextColor(WHITE);
	display.setCursor(0,0);
	
	//Pixel Init + Tests
	pixels.Begin();
	display.print("pew  ");
	display.display();
	wipePxlRGB(1.0f,0.0f,0.0f);
	delay(1000);
	wipePxlRGB(0.0f,1.0f,0.0f);
	delay(1000);
	wipePxlRGB(0.0f,0.0f,1.0f);
	delay(1000);
	display.print("pew  ");
	display.display();
	for(int i = 0; i<360; ++i){
		wipePxlHSL(i/360.0f,1.0f,1.0f);
		delay(10);
	}
	display.println("pew");
	display.display();

  //Wifi
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("wifi: ");
	display.println(ssid);
	display.println("----------------");
  display.display();
  WiFi.begin(ssid, password);
	int tcount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    display.print(">");
		display.display();
		if (tcount > 16) break;
  }
  if((WiFi.status() = WL_CONNECTED){
  IPAddress myIP = WiFi.localIP();
  display.print("IP: ");
  display.println(myIP);
  display.display();
	}
	else{
		 display.println("fail!");
	}
  
  //Encoder
  pinMode(ENCODERPIN1, INPUT_PULLUP);
  pinMode(ENCODERPIN2, INPUT_PULLUP);
  pinMode(ENCODERPINB, INPUT_PULLUP);
  attachInterrupt(ENCODERPIN1, updateEncoder, CHANGE);
  attachInterrupt(ENCODERPIN2, updateEncoder, CHANGE);

  //Modes
  modeVals[0] = 0;
  modeVals[1] = 100;
  modeVals[2] = 0;
  modeVals[3] = 0;
  modeVals[4] = 0;
  modeVals[5] = 0;

  modeValsMax[0] = 36;
  modeValsMax[1] = 100;
  modeValsMax[2] = 100;
  modeValsMax[3] = 255;
  modeValsMax[4] = 255;
  modeValsMax[5] = 255;

  modeValsMin[0] = -36;
  modeValsMin[1] = 0;
  modeValsMin[2] = 0;
  modeValsMin[3] = 0;
  modeValsMin[4] = 0;
  modeValsMin[5] = 0;

  modeNames[0] = "color";
  modeNames[1] = "lumin";
  modeNames[2] = "cycle";
  modeNames[3] = "modus";
  modeNames[4] = "speed";
  modeNames[5] = "width";
  

  updateDsp = true;
  updatePxl = false;
}

void loop() {

  //menu
  if(!digitalRead(ENCODERPINB) && !mChange){
    mChange = true;
    encoderValue=0;
    lastEncoded = 0;
    ++ mMode;
    if(mMode > 3) mMode = 0;
    updateDsp = true;
  }
  if(digitalRead(ENCODERPINB)) mChange = false;

  if(updatePxl){
    wipePxlHC(modeVals[0]/36.0f,modeVals[2]*0.01f, modeVals[1]*0.01f);
    updatePxl = false;
  }
  
  if(updateDsp){
    display.clearDisplay();
    display.setCursor(0,0);
    //display.setTextColor(BLACK,WHITE);
    //display.print("manual pixels");
    display.setCursor(0,9);
    display.setTextColor(WHITE);
    for(int i=0; i < 6;++i){
      if (i > 2) display.setCursor(64,(i-2)*8+1);
      if(mMode == i) display.print(">");
      else display.print(" ");
      display.print(modeNames[i]);
      display.print(" ");
      display.println(modeVals[i]);

    }
    rgbC = pixels.GetPixelColor(0);
    display.setCursor(0,0);
    display.print(rgbC.R);
    display.print(":");
    display.print(rgbC.G);
    display.print(":");
    display.print(rgbC.B);    
    display.display();
    updateDsp = false;
  }

  //delay(10);

}


