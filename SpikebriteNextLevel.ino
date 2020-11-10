#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_LIS3DH.h>

// Squared bit values to eliminate square root calculation
#define GRAVITY_SQUARED_BITS_HIGH 324806901
#define GRAVITY_SQUARED_BITS_LOW  217432719
#define TWO_TENTH_G_OFFSET        2684354

#define DETECT_RIM_HITS           1
#define IGNORE_RIM_HITS           0

#define RIM_DETECT_PIN 13
#define LED_PIN        5
#define LED_COUNT      20
#define LED_BRIGHTNESS 50 // 0-255

// Sensor and LED Initialization
Adafruit_LIS3DH acc = Adafruit_LIS3DH();
Adafruit_NeoPixel led_strip(LED_COUNT, LED_PIN, NEO_GRB /*+ NEO_KHZ800*/);


uint16_t count_clk = 0;
uint16_t last_hit_clk = 0;
uint16_t mode_switch_clk = 0;

uint8_t movement_settled = 0;
uint8_t sensor_calibrated = 0;
uint8_t calibration_count = 0;
uint8_t rim_detect_mode = DETECT_RIM_HITS;
int32_t magnitude_squared = 0;

uint32_t red;
uint32_t blue;
uint32_t green;
uint32_t current_color;

typedef struct acc_data
{
  int32_t static_magnitude_squared;
  int32_t normal_hit_magnitude_squared;
} acc_data;
acc_data stored_data; 

// LED Animation ENUM
typedef enum{
  Circle_Fill,         // 0
  Fade_Switch,         // 1
  Half_Circle_Switch,  // 2
  LED_length,          // 4
} LED_Transitions;  
LED_Transitions LED_Effects = LED_Transitions(0);


void Poll_ACC_Movement();
void Calibrate_Normal_Hit();
void Switch_Colors_On_Hit(int32_t hit_magnitude);
void Circle_Color_Switch(uint32_t color, uint8_t wait);
void Half_Circle_Fill(uint32_t color, uint8_t wait);
void Back_and_Forth_Fill(uint32_t color, uint8_t wait);
void Check_Rim_Detection();

void setup() {
  // initialize pin to toggle rim detection
  pinMode(RIM_DETECT_PIN, INPUT);
  
  // initialize LED Strip
  led_strip.begin();
  led_strip.show();
  led_strip.setBrightness(LED_BRIGHTNESS);

  // Define colors that will be used
  green = led_strip.Color(0, 64, 0);
  red = led_strip.Color(64, 0, 0);
  blue = led_strip.Color(0, 0, 64);

  current_color = green;
  
  Serial.begin(9600);
  while (!Serial) delay(10);

   Serial.println("LIS3DH test!");

  if (! acc.begin(0x18)) {   
    Serial.println("Couldnt start");
    while (1) yield();
  }

  // initialize accelerometer register
  acc.setRange(LIS3DH_RANGE_2_G);
  acc.setDataRate(LIS3DH_DATARATE_25_HZ);

  // Set LED Strip to White
  for(uint8_t i = 0; i < 20; i++ ){
    led_strip.fill(led_strip.Color(i,i,i));
    led_strip.show();
    delay(10);
  }

}

void loop() {
  // put your main code here, to run repeatedly:
  Check_Rim_Detection();
  Poll_ACC_Movement();


  count_clk++;
  delay(50);
}


/*
 *  Reads accelerometer and decides whether to calibrate rest value or change LED color if a ball hit is detected
 */
void Poll_ACC_Movement(){
  acc.read();
  magnitude_squared = (int32_t(acc.x) * int32_t(acc.x)) + (int32_t(acc.y) * int32_t(acc.y)) + (int32_t(acc.z) * int32_t(acc.z));

  if ( (magnitude_squared  <  GRAVITY_SQUARED_BITS_HIGH) &&
            (magnitude_squared  >  GRAVITY_SQUARED_BITS_LOW)){
    movement_settled = 1;
  }
 else{
  // The accelerometer should not be read again immediately after a hit because the rim could still be moving and register multiple hits
  if(!movement_settled || ((count_clk - last_hit_clk) < 10)  ){ 
     return;   
  }
    
    if(!sensor_calibrated){
      Calibrate_Normal_Hit(magnitude_squared);
    }
    else{
      Switch_Colors_On_Hit(magnitude_squared);
    }

    movement_settled = 0;
 }  
}

/*
 * Function is used as a calibration step. The player will hit the ball at the net in a typical way to set an upper limit for a normal hit.
 * If hits are detected below this upper limit then a legal hit is recorded, but if the hit is above the limit then it is likely an illegal rim hit 
 * magnitude -  the squared magnitude of an accelerometer reading
 */
void Calibrate_Normal_Hit(int32_t magnitude){
    if(magnitude > stored_data.normal_hit_magnitude_squared){
       stored_data.normal_hit_magnitude_squared = magnitude;
    }

    for(uint8_t i = calibration_count * (LED_COUNT / 3) ; i < (calibration_count * (LED_COUNT / 3)) + (LED_COUNT / 3); i++){
      led_strip.setPixelColor(i, green);
      led_strip.show();
      delay(30);
    }
    
    calibration_count++;
    last_hit_clk = count_clk;
    
    if(calibration_count == 3){
        sensor_calibrated = 1;    
    }
}

void Switch_Colors_On_Hit(int32_t hit_magnitude){
  if( (hit_magnitude > (stored_data.normal_hit_magnitude_squared + TWO_TENTH_G_OFFSET)) && rim_detect_mode  ) {
    last_hit_clk = count_clk;
    
    for(uint8_t i = 0; i < LED_COUNT; i++ ){
       led_strip.setPixelColor(i, red);
    }
    led_strip.show();
    delay(50);    
  }
  else{
    last_hit_clk = count_clk;

    if(current_color == blue ){
        current_color = green;  
    }
    else{
        current_color = blue;
    }

    
    switch(LED_Effects){
      case Circle_Fill:
        Circle_Color_Switch(current_color, 25);       
        break;
      case Fade_Switch:
        Fade_Color_Switch(current_color, 10);
        break;
      case Half_Circle_Switch:
        Half_Circle_Fill(current_color, 50);
        break;  
    }
      
    LED_Effects = (LED_Transitions)(LED_Effects + 1);
    
    if(LED_Effects == LED_length){
      LED_Effects = (LED_Transitions)(0);
    }

  }
}


void Circle_Color_Switch(uint32_t color, uint8_t wait){
  for(uint8_t i = 0; i < LED_COUNT; i++ ){
      led_strip.setPixelColor(i, color);
      led_strip.show();
      delay(wait);
  }
}

void Fade_Color_Switch(uint32_t color, uint8_t wait){
  if(color == blue){
    for(uint8_t i = 64; i > 0; i-= 2){
      led_strip.fill(led_strip.Color(0, i, 0));
      led_strip.show();
      delay(wait);
    }
    for(uint8_t i = 0; i < 64; i+=2){
      led_strip.fill(led_strip.Color(0, 0, i));
      led_strip.show();
      delay(wait);
    }    
  }
  else{
    for(uint8_t i = 64; i > 0; i-=2)                                                                                                                                                          {
      led_strip.fill(led_strip.Color(0, 0, i));
      led_strip.show();
      delay(wait);
    }
    for(uint8_t i = 0; i < 64; i+=2){
      led_strip.fill(led_strip.Color(0, i, 0));
      led_strip.show();
      delay(wait);
    }     
  }
}

void Half_Circle_Fill(uint32_t color, uint8_t wait){
  for(uint8_t i = 0; i <= 15; i++){
    led_strip.setPixelColor(15 - i, color);
    led_strip.setPixelColor(30 - i, color);
    led_strip.show();
    delay(wait);
  }
}

void Back_and_Forth_Fill(uint32_t color, uint8_t wait){
  for(uint8_t i = 3; i < 30; i += 3){
    led_strip.setPixelColor(i, color);
    led_strip.show();
    delay(wait);
    i -= 2;
    led_strip.setPixelColor(i, color);
    led_strip.show();
    delay(wait);
    
  }
}

void Check_Rim_Detection(){
   if(digitalRead(RIM_DETECT_PIN) && ((count_clk - mode_switch_clk) > 20)){
      if(rim_detect_mode){
        rim_detect_mode = IGNORE_RIM_HITS;  
      }
      else{
        rim_detect_mode = DETECT_RIM_HITS;
      }
   mode_switch_clk = count_clk;   
  }
}
