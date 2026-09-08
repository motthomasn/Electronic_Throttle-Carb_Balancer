// #include <Arduino.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>

#include "Free_Fonts.h" // Include the header file attached to this sketch

// Rotary encoder pins
#define CLK_PIN 15
#define DT_PIN 2
#define SW_PIN 4
#define DEBOUNCE 500   // Button debounce time in ms
// #define LED_PIN 2    // Built-in LED for status

// general fixed values
#define DISP_WIDTH 480
#define DISP_HEIGHT 320
#define MAX_THROTTLES 6         // limited by number of sensor inputs
#define MIN_THROTTLES 2         // no point balancing less than 2 throttles
#define ZEROING_TIME 3000       // sampling time for sensors zeroing procedure in ms
#define GRAPH_HEADER_HEIGHT 20  // height of main graph header
#define GRAPH_FOOTER_HEIGHT 30  // height of main graph footer
#define NENG_X_POSN 120         // position in X to start the engine speed reading
#define MIN_CYCLE_BUFFER 2      // min number of cycles to buffer for display statistics
#define MAX_CYCLE_BUFFER 10     // min number of cycles to buffer for display statistics. This defines size of FIFO buffer array

TFT_eSPI tft = TFT_eSPI();                   // Invoke custom library with default width and height

// analog input pins
const uint8_t analog_pins[MAX_THROTTLES] = {36, 39, 32, 33, 34, 35}; // defined in an array to make looping easier

// rotary encoder variables
uint8_t encoderCnt = 0; // generic counter for tracking encoder steps
uint32_t previousEncoderTime = 0; // only used for testing to debounce a button in place of a pot. Remove for production
bool switchPressed = false; // global indicator for switch press
uint32_t previousSwitchPressTime = 0; // last time used for debounce

uint8_t numCyls = 4;
uint16_t graphColWidth;
uint16_t nEng = 0;
bool cycleComplete = false; // global flag for cycle completed used to trigger drawing the graph
uint16_t columnCentre[MAX_THROTTLES];
uint16_t columnLeftEdge[MAX_THROTTLES];
uint16_t columnRightEdge[MAX_THROTTLES];
uint16_t sensorOffset[MAX_THROTTLES];
uint16_t cycleMin[MAX_THROTTLES];


void ROT_ISR();
void SW_ISR();
uint8_t select_num_cyls();
void zero_sensors();
void configure_graph();
void read_sensors();
void menu();



void setup(void) {

  // Serial1.begin(115200); // debug only
  // pinMode(LED_PIN, OUTPUT);
  // set up display
  tft.begin();
  tft.setRotation(1);

  // set up rotary encoder
  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN, INPUT_PULLUP);
  pinMode(SW_PIN, INPUT_PULLUP);
  attachInterrupt(CLK_PIN, ROT_ISR, RISING);
  attachInterrupt(SW_PIN, SW_ISR, FALLING);

  // get number of cylinders
  numCyls = select_num_cyls();

  // zero sensors
  zero_sensors();

  // set up the main graph page dimensions
  configure_graph();

}

void loop() {
  // read sensors as fast as possible
  read_sensors();

  // draw the graph. This probably needs slowing down. Do we update at fixed rate 20Hz? or update every cycle 12.5Hz at 1500rpm or 21Hz at 2500rpm.
  // makes most sense to update every cycle.
  if (cycleComplete) {
    cycleComplete = false; // reset 
  }

  // check if the button has been pressed to enter the menu
  if (switchPressed) {
    switchPressed = false; // reset state
    menu();
    // Once menu is exited, draw the graph again
    configure_graph();
  }





}

void ROT_ISR() {
  // // for prototype, just increase count. 
  uint32_t now = millis();
  if ( now > ( previousEncoderTime+DEBOUNCE ) ) {
  //   encoderCnt++;
  //   previousEncoderTime = now;
  // }
    bool direction = digitalRead(DT_PIN);
    if (direction == HIGH) {
      encoderCnt--;
    }
    else {
      encoderCnt++;
    }
  }
}

void SW_ISR() {
  uint32_t now = millis();
  if ( now > ( previousSwitchPressTime+DEBOUNCE ) ) {
    switchPressed = true;
    previousSwitchPressTime = now;
  }

}

uint8_t select_num_cyls() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(0, 40);    // Set cursor near top left corner of screen

  tft.setFreeFont(FSB12);       // Select Free Serif 12 point font
  // tft.println();                // Move cursor down a line
  tft.print("Select Number of Cylinders:");
  tft.println(); 
  tft.setFreeFont(FSB24);

  encoderCnt = 4; // init
  while (true) {
    if ( encoderCnt > MAX_THROTTLES ) {
      encoderCnt = MIN_THROTTLES;
    }
    else if ( encoderCnt < MIN_THROTTLES ) {
      encoderCnt = MAX_THROTTLES;
    }
    tft.fillRect(215, 110, 30, 50, TFT_BLACK); // erase the previous number
    tft.setCursor(220, 150);
    tft.print(encoderCnt);
    if ( switchPressed ) {
      switchPressed = false;
      // invert the colours to signify selection
      tft.fillRect(215, 110, 30, 50, TFT_WHITE);
      tft.setCursor(220, 150);
      tft.setTextColor(TFT_BLACK, TFT_WHITE);
      tft.print(encoderCnt);
      delay(200);
      break;
    }
    delay(100);

  }
  tft.fillScreen(TFT_BLACK); // clear display before returning
  return encoderCnt;
}

void zero_sensors() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(0, 40);    // Set cursor near top left corner of screen

  tft.setFreeFont(FSB12);       // Select Free Serif 12 point font
  // tft.println();                // Move cursor down a line
  tft.print("Press Button to Zero Sensors");
  tft.println(); 
  tft.setFreeFont(FSB9);
  tft.print("Ensure engine is not running");

  // while (true) {
  //   // wait for the button press
  //   if ( switchPressed ) {
  //     switchPressed = false; // reset
  //     break;
  //   }  
  //   delay(100);
  // }
  while (!switchPressed) { 
    delay(100);
  }
  switchPressed = false; // reset
  // draw an unfilled rectangle and then fill it within a loop (progress bar)
  uint16_t barHeight = 50;
  uint16_t barWidth = (DISP_WIDTH-40);
  uint16_t xPosn = 20;
  uint16_t yPosn = 120;
  uint8_t sensorReadCnt = 0;
  uint32_t sensorSums[numCyls] = {};

  tft.drawRect(xPosn, yPosn, barWidth, barHeight, TFT_WHITE);
  for (uint16_t i = 0; i <= barWidth; i++) {

    tft.drawFastVLine((i+xPosn), yPosn, barHeight, TFT_WHITE);

    // sample sensors but only every 10 pixels
    if ( (i%10)==0) {
      for (uint8_t i = 0; i < numCyls; i++) {
        uint16_t sensorValue = analogRead( analog_pins[i] );
        sensorSums[i] = sensorSums[i] + sensorValue; 
      }
      sensorReadCnt++;
    }

    delay( uint16_t ( ZEROING_TIME / barWidth ) );

  }
  // transfer averages
  for (uint8_t i = 0; i < numCyls; i++) {
    uint16_t sensorValue = analogRead( analog_pins[i] );
    sensorOffset[i] = uint16_t ( sensorSums[i] / sensorReadCnt ); 
  }

  delay(500);

  tft.fillScreen(TFT_BLACK); // clear display before returning
}

void configure_graph() {
  // Use the number 
  graphColWidth = uint16_t ( DISP_WIDTH / numCyls );
  // draw header & footer lines
  tft.drawFastHLine(0, GRAPH_HEADER_HEIGHT, DISP_WIDTH, TFT_WHITE);
  tft.drawFastHLine(0, uint16_t (DISP_HEIGHT-GRAPH_FOOTER_HEIGHT), DISP_WIDTH, TFT_WHITE);
  // update centres & extremes arrays and draw column dividers and place numbers in header
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setFreeFont(FSB12);
  for (uint16_t i = 1; i <= numCyls; i++) {
    // first populate arrays
    columnCentre[i-1] = uint16_t ( ( graphColWidth * i ) - ( graphColWidth / 2 ) );
    columnRightEdge[i-1] = graphColWidth*i;
    columnLeftEdge[i-1] = columnRightEdge[i-1] - graphColWidth;

    // draw column seperators
    tft.drawFastVLine(columnRightEdge[i-1], 0, (DISP_HEIGHT-GRAPH_FOOTER_HEIGHT), TFT_WHITE);
    // draw cylinder numbers
    tft.setCursor( columnCentre[i-1], (GRAPH_HEADER_HEIGHT-2) );
    tft.print(i);
  }
  // draw the footer text
  tft.setFreeFont(FSB18);
  tft.setCursor( 10, (DISP_HEIGHT-2) );
  tft.print("RPM:");
  tft.setCursor( NENG_X_POSN, (DISP_HEIGHT-2) );
  tft.print(nEng);
  
}

void read_sensors() {
  // function for reading analog pins and updating arrays
  for (uint8_t i = 0; i <= numCyls-1; i++) {
    uint16_t sensorValue = analogRead( analog_pins[i] );
  }
}

void menu() {
  // function to manage the menu options
  encoderCnt = 1; // init locally
  uint8_t prevEncoderCnt = 0; // track last state to avoid redrawing menu constantly. Deliberately init different to draw the initial menu
  uint8_t numMenuItems = 2;
  
  while (!switchPressed) {
    // Serial1.println(prevEncoderCnt);
    if (encoderCnt != prevEncoderCnt) {
    // if (true) {
      // first check for overflows
      if ( encoderCnt > numMenuItems ) {
        encoderCnt = 1;
      }
      else if ( encoderCnt <= 0 ) {
        encoderCnt = numMenuItems;
      }
      prevEncoderCnt = encoderCnt;

      // Draw
      // how do we set the colours in the best way? 
      uint16_t bgColour[2] = {TFT_BLACK, TFT_WHITE}; // index corresponds with whether selected or not 
      uint16_t txtColour[2] = {TFT_WHITE, TFT_BLACK}; //  
      tft.fillScreen(TFT_BLACK); // graph will still be drawn
      tft.setFreeFont(FSB12);
      for (uint8_t i = 1; i <= numMenuItems; i++) {
        tft.setCursor( 10, ((i*30)+20) );
        tft.setTextColor(txtColour[(i==encoderCnt)], bgColour[(i==encoderCnt)]);
        tft.fillRect(0, (i*30), DISP_WIDTH, 30, bgColour[(i==encoderCnt)]);
        switch (i) {
          case 1:
            tft.print("Set Cycle Buffer Size");
            // tft.print(encoderCnt);
            break;
          case 2:
            tft.print("Set Cycle Detection Threshold");
            break;
        }
        // this part is running through once but then getting stuck. Why? It works if the check for inequality of encoderCnt & previous is replaced with if true.
        // Why though?? Everything works as intended then. The encoderCnt changes, the highlighting works
        // if (i==1) {
        //   tft.print("Set Cycle Buffer Size");
        // }
        // else if (i==2) {
        //   tft.print("Set Cycle Buffer Size");
        // }
      }
      
      // tft.setCursor( 10, 20 );
      // tft.fillRect(0, 0, DISP_WIDTH, 30, TFT_WHITE);
      // tft.print("Set Cycle Buffer Size");
      // // tft.drawFastHLine(0, 30, DISP_WIDTH, TFT_WHITE);
      // tft.setCursor( 10, 50 );
      // tft.fillRect(0, 30, DISP_WIDTH, 30, TFT_BLACK);
      // tft.print("Set Cycle Detection Threshold");


    }
    delay(100);
  }
  // digitalWrite(LED_PIN, HIGH);
  // Serial1.println("Switch pressed");

  switchPressed = false; // reset
  switch (encoderCnt) {
    case 1:
      tft.fillScreen(TFT_BLACK);
      // call menu item 1 function
      break;
    case 2:
      tft.fillScreen(TFT_BLACK);
      // call menu item 2 function
      break;
  }
}