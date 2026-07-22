/* RTTY decoder runs two Goertzel algorithms and compares their output. Upon that decides whether Mark or Space, feeds the state machine and then the bit to char decoder */

#define BASELINE_Y 120

float markFreq = 2125.0;
float spaceFreq = 2295.0;
float baud = 45.45;


float coeffMark, coeffSpace;
float cosineMark, cosineSpace;
float sineMark, sineSpace;
int markVal, spaceVal;
const float Fs = 8000;     // sample rate
const int oversample = 4;  // oversampling of the state machine
int samplesPerSymbol;
int16_t dly = 75;  // tested against weak signal

void RTTYMenu() {


  tft.fillScreen(TFT_BLACK);
  setFreeFont();
  tft.setCursor(0, 95);
  tft.print("Move encoder to align Mark/Space with bars.");
  draw4Buttons();
  displayFREQ(FREQ);
  setFreeFont();


  uint32_t tmr = 0;
  spr1.createSprite(256, 30);

  tft.setCursor(25, 265);
  tft.print("Back");

  tft.setCursor(95, 258);
  tft.print("45.45Bd");
  tft.setCursor(100, 274);
  tft.print("170Hz");

  tft.setCursor(185, 258);
  tft.print("50Bd");
  tft.setCursor(185, 274);
  tft.print("450Hz");

  tft.setCursor(270, 258);
  tft.print("75Bd");
  tft.setCursor(265, 274);
  tft.print("170Hz");

  setTextFont();
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(1);
  tft.setCursor(180 + 13, 120);
  tft.print("M");
  tft.setTextColor(TFT_RED);
  tft.setCursor(180 + 35, 120);
  tft.print("S170");
  tft.setCursor(180 + 71, 120);
  tft.print("S450");

  tft.drawFastVLine(180 + 16, 130, 50, TFT_GREEN);  // Mark
  tft.drawFastVLine(180 + 38, 130, 50, TFT_RED);    // Space
  tft.drawFastVLine(180 + 74, 130, 50, TFT_RED);    // Space 450

  tRel();
  delay(200);


  int buttonID = 0;
  while (true) {


    tx = 0;
    ty = 0;

    get_Touch();
    if (pressed)
      buttonID = getButtonID();

    if (buttonID >= 41) {

      switch (buttonID) {

        break;
        case 42:
          markFreq = 2125.0;
          spaceFreq = 2295.0;
          baud = 45.45;
          samplesPerSymbol = 42;
          initRTTY();
          break;
        case 43:
          markFreq = 2125.0;
          spaceFreq = 2575.0;
          baud = 50.0;
          samplesPerSymbol = 40;
          initRTTY();
          break;
        case 44:
          markFreq = 2125.0;
          spaceFreq = 2295.0;
          baud = 75;
          samplesPerSymbol = 25;
          initRTTY();
          break;
        default:
          break;
      }
      tRel();
      spr1.deleteSprite();
      setTextFont();
      return;
    }


    if (clw) {
      FREQ += 10;
    }
    if (cclw) {
      FREQ -= 10;
    }

    // Update frequency display if changed
    if (clw || cclw) {
      displayFREQ(FREQ);
      setFreq();
      cclw = false;
      clw = false;
    }



    if (millis() - tmr > 130) {
      FFTSample();  // 256 samples = 4 kHz
      tmr = millis();

      spr1.scroll(0, 1);
      // Draw new top row (2–3k range) // bin 256 = 4KHz
      for (int j = 128; j < 192; j++) {
        int value = (int)(RvReal[j]);

        spr1.drawPixel((j - 128) * 2, 0, valueToWaterfallColor(3 * value));
        spr1.drawPixel((j - 128) * 2 + 1, 0, valueToWaterfallColor(3 * value));
        spr1.drawFastVLine(16, 0, spr1.height(), TFT_GREEN);  // Mark
        spr1.drawFastVLine(38, 0, spr1.height(), TFT_RED);    // Space
        spr1.drawFastVLine(74, 0, spr1.height(), TFT_RED);    // Space2
      }
      spr1.pushSprite(180, 140);
    }
  }
}
//##########################################################################################################################//
// Baudot lookup tables (Letters/Figures shift)
// Letters shift
char baudotLetters[32] = {
  '\0', 'E', '\n', 'A', ' ', 'S', 'I', 'U',
  '\r', 'D', 'R', 'J', 'N', 'F', 'C', 'K',
  'T', 'Z', 'L', 'W', 'H', 'Y', 'P', 'Q',
  'O', 'B', 'G', '\0', 'M', 'X', 'V', '\0'
};

// Figures shift
char baudotFigures[32] = {
  '\0', '3', '\n', '-', ' ', '\'', '8', '7',
  '\r', '$', '4', '\'', ' ', '!', '&', ';',
  '5', '+', ' ', '2', ' ', '6', '0', '1',
  '9', '?', ' ', '\0', '.', '/', ' ', '\0'
};


enum Shift { LETTERS,
             FIGURES };
Shift currentShift = LETTERS;


//##########################################################################################################################//
void drawScreenElements() {

  tft.fillRect(0, 50, 320, 270, TFT_BLACK);
  displayFREQ(FREQ);
  tft.setTextSize(2);
  tft.setCursor(360, 5);
  tft.setTextColor(TFT_GREEN);
  tft.printf("M:%d", (int)markFreq);
  tft.setCursor(360, 25);
  tft.setTextColor(TFT_RED);
  tft.printf("S:%d", (int)spaceFreq);
  tft.setTextColor(textColor);
  tft.setTextSize(1);
  tft.setCursor(100, 97);
  tft.print("Tune for strongest bars.");
  tft.setTextSize(2);

  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(0, BASELINE_Y);

  //tft.drawRect(0, 110, 320, 100, TFT_CYAN);
}

//##########################################################################################################################//



void drawMarkSpace(int rawMark, int rawSpace) {
  const int N = 3;
  static int markBuf[N], spaceBuf[N];
  static int idx = 0;

  // store new values (clamped)
  markBuf[idx] = constrain(rawMark, 0, 30);
  spaceBuf[idx] = constrain(rawSpace, 0, 30);
  idx = (idx + 1) % N;

  // compute averages
  int sumMark = 0, sumSpace = 0;
  for (int i = 0; i < N; i++) {
    sumMark += markBuf[i];
    sumSpace += spaceBuf[i];
  }
  int markVal = sumMark / N;
  int spaceVal = sumSpace / N;


  tft.fillRect(175, 60, 2, 30, TFT_BLACK);
  tft.fillRect(180, 60, 2, 30, TFT_BLACK);


  tft.fillRect(175, 90 - markVal, 2, markVal, TFT_GREEN);
  tft.fillRect(180, 90 - spaceVal, 2, spaceVal, TFT_RED);
}

//##########################################################################################################################//



void initRTTY() {


  Serial.printf("baud=%.2f samplesPerSymbol=%d\n", baud, samplesPerSymbol);

  tft.fillScreen(TFT_BLACK);
  drawScreenElements();
  //tft.setTextSize(1);
  initGoertzel();
  currentShift = LETTERS;
  RTTYMainLoop();
}

//##########################################################################################################################//



void initGoertzel() {
  float omegaMark = 2.0f * M_PI * markFreq / Fs;
  float omegaSpace = 2.0f * M_PI * spaceFreq / Fs;

  Serial.printf("Mark  = %.1f Hz\n", markFreq);
  Serial.printf("Space = %.1f Hz\n", spaceFreq);

  coeffMark = 2.0f * cosf(omegaMark);
  coeffSpace = 2.0f * cosf(omegaSpace);

  cosineMark = cosf(omegaMark);
  cosineSpace = cosf(omegaSpace);

  sineMark = sinf(omegaMark);
  sineSpace = sinf(omegaSpace);
}

//##########################################################################################################################//

int detectMarkSpace() {
  float Q1m = 0, Q2m = 0;
  float Q1s = 0, Q2s = 0;



  for (int i = 0; i < samplesPerSymbol; i++) {


    adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &raw36);
    delayMicroseconds(dly);


    float sample = (float)(raw36 - gpio36_Offset);  // raw36 gets provided by ISR center at zero

    // mark
    float Q0m = coeffMark * Q1m - Q2m + sample;
    Q2m = Q1m;
    Q1m = Q0m;

    // space
    float Q0s = coeffSpace * Q1s - Q2s + sample;
    Q2s = Q1s;
    Q1s = Q0s;
  }

  //magnitudes
  float realMark = Q1m - Q2m * cosineMark;
  float imagMark = Q2m * sineMark;
  float magMark = sqrt(realMark * realMark + imagMark * imagMark);

  float realSpace = Q1s - Q2s * cosineSpace;
  float imagSpace = Q2s * sineSpace;
  float magSpace = sqrt(realSpace * realSpace + imagSpace * imagSpace);

  markVal = (int)(magMark) / 1000;
  spaceVal = (int)(magSpace) / 1000;

  return (magMark > magSpace) ? 1 : 0;  // 1=mark, 0=space
}

//##########################################################################################################################//


void RTTYMainLoop() {


  while (true) {

    // symbol detector

    int bit = detectMarkSpace();


    processStep(bit);  // bit to character logic


    drawMarkSpace(markVal, spaceVal);


    //delayMicroseconds(300);




    if (tft.getTouchRawZ() > 400) {
      return;
    }

    if (clw) {
      FREQ += 10;
      setFreq();
      clw = false;
      currentShift = LETTERS;
      displayFREQ(FREQ);
      tft.fillRect(0, BASELINE_Y, 480, 200, TFT_BLACK);
      tft.setCursor(0, BASELINE_Y);
      drawScreenElements();
    }

    else if (cclw) {
      FREQ -= 10;
      setFreq();
      cclw = false;
      currentShift = LETTERS;
      displayFREQ(FREQ);
      tft.fillRect(0, BASELINE_Y, 480, 200, TFT_BLACK);
      tft.setCursor(0, BASELINE_Y);
      //drawScreenElements();
    }
  }
}


//##########################################################################################################################//


enum State {
  WAIT_IDLE,
  VERIFY_START,
  DATA_BITS,
  STOP_BIT
};

State state = WAIT_IDLE;

int lastBit = 1;  // idle = mark
int phase = 0;    // oversample counter
int bitCount = 0;
uint8_t currentChar = 0;


//##########################################################################################################################//

void processStep(int bit) {  // state machine
  switch (state) {

    case WAIT_IDLE:

      // mark to sace
      if (lastBit == 1 && bit == 0) {
        phase = 0;
        state = VERIFY_START;
      }
      break;


    case VERIFY_START:

      phase++;


      if (phase >= oversample) {
        phase = 0;
        currentChar = 0;
        bitCount = 0;

        // sample first data bit
        currentChar |= (bit << bitCount);
        bitCount++;

        state = DATA_BITS;
      }
      break;


    case DATA_BITS:

      phase++;

      if (phase >= oversample) {
        phase = 0;

        currentChar |= (bit << bitCount);
        bitCount++;

        if (bitCount >= 5) {
          state = STOP_BIT;
        }
      }
      break;


    case STOP_BIT:

      phase++;

      if (phase >= oversample) {
        phase = 0;

        // valid stop bit must be Mark
        if (bit == 1) {
          decodeBaudot(currentChar);
        }

        state = WAIT_IDLE;
      }
      break;
  }

  lastBit = bit;
}


//##########################################################################################################################//




void printChar(char out) {


    if(isprint(out))
    tft.print(out);
   


  int ty = tft.getCursorY();
  int tx = tft.getCursorX();

  if (tx > 465 || (tx > 420 && out == 32)) 
    tft.setCursor(0, ty + 22);


  if (ty > 300) {

    tft.fillRect(0, BASELINE_Y, 480, 200, TFT_BLACK);
    tft.setCursor(0, BASELINE_Y);
  }
}
//##########################################################################################################################//
void decodeBaudot(uint8_t currentChar) {

  if (currentChar == 31) {
    currentShift = LETTERS;
  } else if (currentChar == 27) {
    currentShift = FIGURES;
  }

  else {
    char out = (currentShift == LETTERS) ? baudotLetters[currentChar] : baudotFigures[currentChar];
        printChar(out);
    
  }
}



//##########################################################################################################################//