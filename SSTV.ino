


#define LINE_WIDTH 320  // Width of the image
#define FREQ_MIN 1500   // Minimum frequency for pixel decoding
#define FREQ_MAX 2300   // Maximum frequency for pixel decoding


int numLines = 240;
int sMode = 0;
bool autoMode = false;
//##########################################################################################################################//

void initAnalogDecoders() {

  timerEnd(timer);
}

//##########################################################################################################################//


void endAnalogDecoders() {
  startAudioSampler();
}

//##########################################################################################################################//


uint32_t getAnalogFrequency()
{
    static bool above = false;

    adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &raw36);

    // Hysteresis
    const int HYST = 3; // need chack what's best

    bool newAbove = above;

    if (raw36 > gpio36_Offset + HYST)
        newAbove = true;
    else if (raw36 < gpio36_Offset - HYST)
        newAbove = false;

    // Only process real crossings
    if (newAbove != above)
    {
        uint32_t now = micros();

        if (lastCrossUs)
        {
            uint32_t halfPeriod = now - lastCrossUs;

            if (prevHalfUs)
            {
                uint32_t fullPeriodUs = prevHalfUs + halfPeriod;

                
                    lastFreq = 1000000UL / fullPeriodUs;
                
            }

            prevHalfUs = halfPeriod;
        }

        lastCrossUs = now;
        above = newAbove;
    }

    return lastFreq;
}
//##########################################################################################################################//

void SSTVINIT() {

  sMode = autoMode = 0;

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN);
  fBuf = (uint8_t *)malloc(320 * 240);  // 1 byte per pixel
  if (!fBuf) {
    buffErr();
    return;
  }
  memset(fBuf, 0x00, 320 * 240);
  tft.setTextSize(1);

  si4735.setAudioMute(true);
  adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &raw36);  // recalibrate offset
  gpio36_Offset = raw36;
  si4735.setAudioMute(audioMuted);



  while (true) {

    if (!adjustFreq()) {
      spr1.deleteSprite();
      free(fBuf);
      return;
    }

    tft.fillScreen(TFT_BLACK);
    initAnalogDecoders();
    waitForVerticalSyncPulse();

    tft.setTextSize(1);
    tft.setCursor(130, 230);
    tft.print("Touch to leave");
    tft.setCursor(0, 230);
    tft.printf("F:%ld", FREQ);
    decodeLines();
    endAnalogDecoders();
    tRel();
    tft.fillScreen(TFT_BLACK);
    if (selectMedia()) {
      tRel();  // select where to save
      tx = 0;
      ty = 0;
      pressed = false;
    }
  }
}

//##########################################################################################################################//


#define RAW_THRESHOLD 80000   // 80 KB
#define BMP_THRESHOLD 500000  // 250 KB

int checkLittleFSSpaceAndCleanup(bool rawmode) {


  size_t totalBytes = LittleFS.totalBytes();
  size_t usedBytes = LittleFS.usedBytes();
  size_t freeBytes = totalBytes - usedBytes;

  size_t threshold = rawmode ? RAW_THRESHOLD : BMP_THRESHOLD;
  int deletedCount = 0;

  Serial.printf("LittleFS total=%u used=%u free=%u threshold=%u mode=%s\n",
                (unsigned)totalBytes, (unsigned)usedBytes, (unsigned)freeBytes,
                (unsigned)threshold, rawmode ? "RAW" : "BMP");

  while (freeBytes < threshold) {

    totalBytes = LittleFS.totalBytes();
    usedBytes = LittleFS.usedBytes();
    freeBytes = totalBytes - usedBytes;




    File root = LittleFS.open("/");
    int lowestNum = INT_MAX;
    String lowestFile = "";



    while (true) {
      File f = root.openNextFile();
      if (!f) break;

      String name = f.name();  // e.g. "/sstv12.raw"
      f.close();


      if ((rawmode && name.endsWith(".raw")) || (!rawmode && name.endsWith(".bmp"))) {
        int start = name.indexOf("sstv");
        if (start >= 0) {
          int end = name.lastIndexOf('.');
          String numStr = name.substring(start + 4, end);
          int num = numStr.toInt();
          if (num > 0 && num < lowestNum) {
            lowestNum = num;
            lowestFile = name;  // keep full path with leading slash
          }
        }
      }
    }

    if (lowestFile.length()) {

      String lf = "/" + lowestFile;

      if (LittleFS.remove(lf)) {

        deletedCount++;
      } else {
        Serial.printf("Delete FAILED for '%s'\n", lowestFile.c_str());
        break;
      }
    } else {
      Serial.println("Notn to delete");
      break;
    }

    usedBytes = LittleFS.usedBytes();
    freeBytes = totalBytes - usedBytes;
    Serial.printf("After deletion: used=%u free=%u\n",
                  (unsigned)usedBytes, (unsigned)freeBytes);
  }

  Serial.printf("Ready,deleted %d files\n", deletedCount);
  return deletedCount;
}

//##########################################################################################################################//

String createConsecutiveFilename(bool rawmode) {

  int fileNumber = preferences.getInt("sstv", 0);
  fileNumber++;


  String fileName = "sstv" + String(fileNumber);
  if (rawmode) {
    fileName += ".raw";
  } else {
    fileName += ".bmp";
  }
  preferences.putInt("sstv", fileNumber);

  return fileName;
}

//##########################################################################################################################//

void decodeLines() {

  long syncInterval = 0;

  memset(fBuf, 0x00, 320 * 240);

  while (!sMode) {  // select decoder based on time between sync pulses, not VIS

    uint16_t z = tft.getTouchRawZ();
    if (z > 300) return;



    syncInterval = determineHorizontalSyncInterval();  // microsecs

    syncInterval /= 1000;  // convert to ms


    if (syncInterval < 1200 && syncInterval >= 220 && autoMode == true) {

      tft.fillRect(245, 80, 70, 16, TFT_BLACK);
      tft.setCursor(245, 80);
      tft.print(F("Autodetect"));
      tft.fillRect(245, 100, 130, 16, TFT_BLACK);
      tft.setCursor(245, 100);
      tft.printf("%ldms", syncInterval);


      if (syncInterval >= 418 && syncInterval <= 422)
        sMode = 1;  // Scottie 1 found

      else if (syncInterval >= 268 && syncInterval <= 272)
        sMode = 3;  // Scottie 2 found

      else if (syncInterval >= 440 && syncInterval <= 444)
        sMode = 2;  //Martin 1 found

      else if (syncInterval >= 220 && syncInterval <= 224)
        sMode = 4;  //Martin 2 found


      else if (syncInterval >= 290 && syncInterval <= 294)
        sMode = 5;  //Robot36

      else if (syncInterval >= 1039 && syncInterval <= 1044)
        sMode = 6;  //Scottie DX

      else sMode = 0;
    }
  }


  showMode();


  for (uint16_t line = 0; line < numLines; line++) {  // / Decode and draw individual lines

    if (sMode == 5)
      decodeLineRobot36(line);

    else
      decodeLineMS(line);  // Martin + Scottie


    uint16_t z = tft.getTouchRawZ();  // when image is finished
    if (z > 300) return;
  }

  // keep image unril presses

  while (true) {
    uint16_t z = tft.getTouchRawZ();  // when image is finished
    if (z > 300) return;
  }
}

//##########################################################################################################################/

void showMode() {

  tft.setCursor(245, 210);

  if (sMode == 1)
    tft.print(F("Scottie S1"));

  if (sMode == 3)
    tft.print(F("Scottie S2"));

  if (sMode == 2)
    tft.print(F("Martin 1"));

  if (sMode == 4)
    tft.print(F("Martin 2"));

  if (sMode == 5)
    tft.print(F("Robot 36"));
  if (sMode == 6)
    tft.print(F("Scottie DX"));

  tft.setCursor(0, 0);
}
//##########################################################################################################################/

uint32_t runG1200() {  // sample rate 8000,  1200Hz Goertzel,
  double q0, q1, q2;
  q0 = q1 = q2 = 0;

  for (int i = 0; i < 64; i++) {
    adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &raw36);
    double sample = (double)raw36;

    delayMicroseconds(88);

    q0 = 1.111140 * q1 - q2 + sample;
    q2 = q1;
    q1 = q0;
  }

  double magnitude = q1 * q1 + q2 * q2 - 1.111140 * q1 * q2;
  return (uint32_t)sqrt(magnitude);
}


//##########################################################################################################################/



bool adjustFreq() {
  // Helper function to fine tune the frequency and check sync pulse at 1200Hz

  const int startX = 10;
  const int startY = 0;
  const int width = 140;
  const int height = 25;
  const int spacing = 30;
  const int gradient = 2;

  const char *labels[] = {
    "Autodetect", "Martin M1", "Martin M2", "Scottie S1", "Scottie S2", "Scottie DX", "Back"
  };

  sMode = autoMode = 0;

  // Draw mode labels
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(2);
  for (int i = 0; i < 7; i++) {
    int y = startY + (i * spacing);
    tft.fillRect(startX, y, width, height, TFT_NAVY);
    tft.setCursor(startX + 5, y + 5);
    tft.print(labels[i]);
  }

  // Frequency display
  tft.setTextSize(1);
  tft.fillRect(210, 0, 110, 15, TFT_BLACK);
  tft.setCursor(210, 0);
  tft.printf("%ld", FREQ);
  tft.setTextColor(TFT_WHITE);

  tft.setCursor(192, 100);
  tft.print("1200Hz Sync:");

  tft.setCursor(192, 120);
  tft.print("1 = 1150 Hz");
  tft.setCursor(192, 130);
  tft.print("2 = 1250 Hz");
  tft.setCursor(192, 140);
  tft.print("3 = 1500 Hz");
  tft.setCursor(192, 150);
  tft.print("4 = 2300 Hz");

  tft.setCursor(175, 20);
  tft.print("Move enc. for fine tune.");

  spr1.createSprite(256, 30);


  uint32_t tmr = 0;

  while (true) {
    // Touch input
    uint16_t z = tft.getTouchRawZ();
    if (z > 300) {
      sMode = getSSTVMode();

      tRel();
    }

    if (sMode == -1) return false;  // touch out of area
    if ((autoMode == true) || sMode) break;



    uint32_t mag = runG1200();
    mag /= 600;

    drawVUBar(275, 100, mag);


    // FFT update every ~130 ms
    if (millis() - tmr > 130) {
      FFTSample();  // 256 samples = 4 kHz
      tmr = millis();



      spr1.scroll(0, 1);
      // Draw new top row (1k–3k range)
      for (int j = 64; j < 192; j++) {
        int value = (int)(RvReal[j]);
        spr1.drawPixel(j - 64, 0, valueToWaterfallColor(gradient * value));
      }
      spr1.pushSprite(192, 190);



      // Reference lines with numeric labels
      tft.setTextSize(1);

      // 1150 Hz → label "1"
      tft.drawFastVLine(192 + 10, 180, 50, TFT_RED);
      tft.setCursor(192 + 10 - 3, 170);
      tft.setTextColor(TFT_RED);
      tft.print("1");

      // 1250 Hz → label "2"
      tft.drawFastVLine(192 + 16, 180, 50, TFT_RED);
      tft.setCursor(192 + 16 - 3, 170);
      tft.setTextColor(TFT_RED);
      tft.print("2");

      // 1500 Hz → label "3"
      tft.drawFastVLine(192 + 32, 180, 50, TFT_YELLOW);
      tft.setCursor(192 + 32 - 3, 170);
      tft.setTextColor(TFT_YELLOW);
      tft.print("3");

      // 2300 Hz → label "4"
      tft.drawFastVLine(192 + 83, 180, 50, TFT_YELLOW);
      tft.setCursor(192 + 83 - 3, 170);
      tft.setTextColor(TFT_YELLOW);
      tft.print("4");

      tft.setTextColor(TFT_WHITE);
    }



    // Encoder input
    if (clw) {
      FREQ += 25;
    }
    if (cclw) {
      FREQ -= 25;
    }

    // Update frequency display if changed
    if (clw || cclw) {
      tft.fillRect(210, 0, 110, 15, TFT_BLACK);
      tft.setCursor(210, 0);
      tft.printf("%ld", FREQ);  // fine tune in 25 Hz steps
      setFreq();
      cclw = false;
      clw = false;
    }
  }

  return true;
}


//##########################################################################################################################/


int getSSTVMode() {
  int decoMode = 0;

  get_Touch();

  if (!pressed)
    return 0;

  pressed = false;

  if (tx <= 130) {

    tRel();

    if (ty >= 0 && ty <= 25)
      autoMode = true;

    if (ty >= 30 && ty <= 55)
      decoMode = 2;  // Martin 1

    if (ty >= 60 && ty <= 85)
      decoMode = 4;  // Martin 2

    if (ty >= 90 && ty <= 115)
      decoMode = 1;  // Scottie 1

    if (ty >= 120 && ty <= 145)
      decoMode = 3;  // Scottie 2

    if (ty >= 150 && ty <= 175) {
      decoMode = 6;  // Scottie DX
    }


    if (ty >= 180) {  // Back
      return -1;
    }

    return decoMode;
  }

  return -1;
}


//##########################################################################################################################/



void decodeLineMS(uint16_t line) {  // Martin and Scottie line decoder


  uint8_t bufG[330] = { 0 };
  uint8_t bufB[330] = { 0 };
  uint8_t bufR[330] = { 0 };

  int frq = 0;
  uint8_t *currentBuf = nullptr;

  uint16_t pixelDuration = 0;
  uint16_t syncDelay = 0;


  switch (sMode) {

    case 1:
      pixelDuration = 435;  // 437Scottie S1
      syncDelay = 6200;
      break;
    case 2:
      pixelDuration = 452;  // 453 Martin M1
      syncDelay = 2200;
      break;
    case 3:
      pixelDuration = 279;  // 281 Scottie S2
      syncDelay = 4500;
      break;
    case 4:
      pixelDuration = 223;  // Martin M2
      syncDelay = 2200;
      break;
    case 5:
      break;
    case 6:
      pixelDuration = 1079;  // 1082 Scottie DX
      syncDelay = 6200;
      break;

    default:
      break;
  }


  tft.fillRect(280, 230, 40, 10, TFT_BLACK);
  tft.setCursor(280, 230);
  tft.printf("L:%d", line + 1);

  waitForHorizontalSyncPulse();
  delayMicroseconds(syncDelay);


  for (uint16_t pass = 0; pass < 3; ++pass) {

  encoderModifyFreq();  // For dfine tune



    if (sMode == 1 || sMode == 3 || sMode == 6) {
      currentBuf = (pass == 0) ? bufR : (pass == 1) ? bufG  // Scottie
                                                    : bufB;
    }

    if (sMode == 2 || sMode == 4) {
      currentBuf = (pass == 0) ? bufG : (pass == 1) ? bufB  // Martin
                                                    : bufR;
    }


    unsigned long pixelStartTime = micros();




    for (uint16_t x = 0; x < 320; ++x) {

      uint32_t pixelEndTime = pixelStartTime + (uint32_t)(x + 1) * pixelDuration;

      uint16_t ctr = 0;
      uint32_t accum = 0;
      while (micros() < pixelEndTime) {

        frq = getAnalogFrequency();
        accum += frq;
        ctr++;
      }

      if (ctr)
        frq = accum / ctr;

      pixelDuration = encoderModifyDelay(pixelDuration, 1);


      frq = constrain(frq, 1500, 2300);
      *(currentBuf + x) = map(frq, 1500, 2300, 0, 255);
    }


    if (sMode == 2 || sMode == 4)
      delayMicroseconds(1800);  // Martin Sync porch
  }


  medianFilter(bufR);
  medianFilter(bufG);
  medianFilter(bufB);

  temporalFilter(bufR, helper1);
  temporalFilter(bufG, helper2);
  temporalFilter(bufB, helper3);



  for (int x = 10; x < 320; x++) {

    uint16_t red = (bufR[x] >> 3) << 11;

    uint16_t green = (bufG[x] >> 2) << 5;

    uint16_t blue = (bufB[x] >> 3);
    lineBuffer[x - 10] = red | green | blue;
    storePixel(x - 10, line, lineBuffer[x - 10]);
  }


  // Push the buffer to the display
  tft.startWrite();
  tft.setAddrWindow(0, line, 310, 1);  // x, y, width, height
  tft.pushColors(lineBuffer, 310, true);
  tft.endWrite();
}


//##########################################################################################################################/
void temporalFilter(uint8_t *cur, uint8_t *prev) {
  for (int x = 0; x < 320; x++) {
    if (abs((int)cur[x] - (int)prev[x]) < 30)
      cur[x] = (3 * cur[x] + prev[x]) >> 2;

    prev[x] = cur[x];
  }
}
//##########################################################################################################################/


uint8_t median3(uint8_t a, uint8_t b, uint8_t c) {
  if (a > b) {
    uint8_t t = a;
    a = b;
    b = t;
  }
  if (b > c) {
    uint8_t t = b;
    b = c;
    c = t;
  }
  if (a > b) {
    uint8_t t = a;
    a = b;
    b = t;
  }
  return b;
}

void medianFilter(uint8_t *buf) {
  uint8_t tmp[320];

  tmp[0] = buf[0];
  tmp[319] = buf[319];

  for (int x = 1; x < 319; x++) {
    tmp[x] = median3(
      buf[x - 1],
      buf[x],
      buf[x + 1]);
  }

  memcpy(buf, tmp, 320);
}


//##########################################################################################################################/


void decodeLineRobot36(uint16_t line) {  //
  uint16_t pixelDurationY = 240;         // ~88 ms / 320 pixels
  uint16_t pixelDurationC = 120;         // ~44 ms / 320 pixels



  uint8_t bufY[320] = { 0 };
  uint8_t bufC[320] = { 0 };

  // --- Sync pulse (1200 Hz, 9 ms) ---
  waitForHorizontalSyncPulse();
  delayMicroseconds(10000);

  // --- Sync porch (1500 Hz, 3 ms) ---
  //delayMicroseconds(3000);

  // --- Luminance scan (Y channel, 88 ms) ---
  for (int x = 0; x < 320; x++) {
    unsigned long pixelStart = micros();
    int ctr = 0;
    uint32_t accum = 0;
    while (micros() - pixelStart < pixelDurationY) {
      int frq = getAnalogFrequency();
      accum += frq;
      ctr++;
    }
    if (ctr) {
      int frq = constrain(accum / ctr, 1500, 2300);
      bufY[x] = map(frq, 1500, 2300, 0, 255);
    }
  }

  delayMicroseconds(4500);

  // --- Separator + porch ---

  //delayMicroseconds(4500); // 1500 Hz separator


  // --- Chrominance scan (R-Y or B-Y, 44 ms) ---
  for (int x = 0; x < 320; x++) {
    unsigned long pixelStart = micros();
    int ctr = 0;
    uint32_t accum = 0;
    while (micros() - pixelStart < pixelDurationC) {
      int frq = getAnalogFrequency();
      accum += frq;
      ctr++;
    }
    if (ctr) {
      int frq = constrain(accum / ctr, 1500, 2300);
      bufC[x] = map(frq, 1500, 2300, 0, 255);
    }
  }

  // --- Store line data ---
  if (line % 2 == 0) {
    memcpy(helper3, bufY, 320);
    memcpy(helper4, bufC, 320);
  } else {
    memcpy(helper1, bufY, 320);
    memcpy(helper2, bufC, 320);

    // Now we have both chromas → combine
    uint16_t lineBuffer[320];
    for (int x = 0; x < 320; x++) {
      float Y = (helper1[x] + helper3[x]) / 2.0f;
      float Ry = helper2[x] - 128.0f;
      float By = helper4[x] - 128.0f;

      float Rf = Y + 1.402f * Ry;
      float Bf = Y + 1.772f * By;
      float Gf = Y - 0.344f * Ry - 0.714f * By;

      int R = constrain((int)Rf, 0, 255);
      int G = constrain((int)Gf, 0, 255);
      int B = constrain((int)Bf, 0, 255);

      lineBuffer[x] = ((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3);
    }

    tft.startWrite();                       // double two horizontal lines into one
    tft.setAddrWindow(0, line, 320, 2);     // two display lines
    tft.pushColors(lineBuffer, 320, true);  // first line
    tft.pushColors(lineBuffer, 320, true);  // duplicate for second line
    tft.endWrite();
  }
}




//##################################################################################################################/


void waitForHorizontalSyncPulse() {

  int16_t ctr = 0;
  uint16_t f = 0;
  uint32_t now = micros();


  while (true) {

    if (micros() > now + 100000)  // emergency exit
      return;


    f = getAnalogFrequency();

    if (f <= 1250 && f >= 1150)
      ctr++;

    else ctr--;

    if (ctr < 0)
      ctr = 0;
    if (ctr == 3)
      break;
  }
}
//###
//##########################################################################################################################/


void waitForVerticalSyncPulse() {  // Wait for transmission start

  int16_t ctr = 0;


  tft.setCursor(10, 100);
  tft.setTextSize(1);
  tft.print(F("Waiting for vertical sync..."));
  tft.setCursor(10, 120);
  tft.print("Touch to start immediately.");


  while (true) {

    uint16_t z = tft.getTouchRawZ();
    if (z > 300) {
      tRel();
      tft.fillRect(10, 100, 310, 30, TFT_BLACK);
      return;
    }


    uint32_t now = micros();
    uint16_t c = 0;
    uint32_t accum = 0;
    while (micros() < now + 1000) {
      frq = getAnalogFrequency();
      accum += frq;
      c++;
    }

    if (c)
      frq = accum / c;



    //Serial.printf("ctr%d frq %d\n", ctr,frq);

    if (frq > 1870 && frq < 1930) {  // vertical start freq detected
      ctr++;


    }

    else if (frq < 1870 || frq > 1930)  // decrease counter over time to eliminate false positives
      ctr--;


    if (ctr < 0)
      ctr = 0;

    if ((ctr > 10)) {  // 10 * consecutive 1900Hz freq detected, assume this is a SSTV transmission
      break;
    }
  }  // stage one finished


  delay(500);  // wait until end of 2nd halp of the horizintal sync pulse

  while (true) {

    uint16_t z = tft.getTouchRawZ();
    if (z > 300) {
      tRel();
      tft.fillRect(10, 100, 310, 30, TFT_BLACK);
      return;
    }


    uint32_t now = micros();
    uint16_t c = 0;
    uint32_t accum = 0;
    while (micros() < now + 1000) {
      frq = getAnalogFrequency();
      accum += frq;
      c++;
    }

    if (c)
      frq = accum / c;



    ////Serial.printf("ctr%d frq %d\n", ctr,frq);

    if (frq > 1870 && frq < 1930) {  // vertical start freq detected
      ctr++;

    }

    else if (frq < 1870 || frq > 1930)  // decrease counter over time to eliminate false positives
      ctr--;


    if (ctr < 0)
      ctr = 0;

    if ((ctr > 10)) {  // 10 * consecutive 1900Hz freq detected, assume this is a SSTV transmission
      tft.fillRect(10, 100, 310, 30, TFT_BLACK);
      break;
    }
  }  // stage two finished

  waitForHorizontalSyncPulse();
}
//##########################################################################################################################/



void encoderModifyFreq() {  // fine tune


  if (clw || cclw) {

    if (clw) {
      FREQ += 25;
      clw = false;
    }

    if (cclw) {
      FREQ -= 25;
      cclw = false;
    }

    tft.fillRect(0, 230, 130, 16, TFT_BLACK);
    tft.setCursor(0, 230);
    tft.printf("F:%ld", FREQ);
    setFreq();
  }
}

//##########################################################################################################################/



uint32_t encoderModifyDelay(uint32_t val, int step) {  // debug function


  static uint32_t dely = val;


  if (clw || cclw) {

    if (clw) {
      dely -= step;
      clw = false;
    }

    if (cclw) {
      dely += step;
      cclw = false;
    }


    tft.fillRect(100, 220, 200, 16, TFT_BLACK);
    tft.setCursor(100, 220);
    tft.printf("Val:%ld", dely);
    setFreq();
  }

  return dely;
}
//##########################################################################################################################/


uint32_t determineHorizontalSyncInterval() {

  uint32_t start = 0;
  uint32_t end = 0;
  int32_t ctr = 0;


  while (true) {


    if (clw || cclw)
      return 0;


    frq = getAnalogFrequency();  // about 30uS
    delayMicroseconds(60);

    if (frq >= 1150 && frq <= 1250) {
      ctr++;
    }

    else ctr--;

    if (ctr < 0)
      ctr = 0;

    // Sync pulse length  Martin: about 6 periods
    // Scottie: about 11 periods

    if (ctr > 4) {  // 5 periods
      start = micros();
      break;
    }
  }

  if (sMode == 6)
    delay(800);  // Scottie DX arounf 1040ms

  delay(210);  // min for all other modes


  ctr = 0;

  while (true) {


    if (clw || cclw)
      return 0;


    frq = getAnalogFrequency();
    delayMicroseconds(60);

    if (frq >= 1150 && frq <= 1250) {
      ctr++;
    }

    else ctr--;

    if (ctr < 0)
      ctr = 0;

    if (ctr > 4) {
      end = micros();
      break;
    }
  }


  uint32_t diff = (end - start);

  diff -= 3000;  // time needed to measure the sync pulses


  return diff;
}


//##########################################################################################################################/

void writeScreenbuffertoFile(const char *filename, int width, int height, bool target, bool rawMode) {  // target 0 = sd card, 1= LittleFS,

  uint8_t rowBuf[width * 3 + 4];  // +4 for padding

  uint32_t rowSize = (width * 3 + 3) & ~3;
  uint32_t dataSize = rowSize * height;
  uint32_t fileSize = 54 + dataSize;

  uint8_t header[54] = { 0 };
  header[0] = 'B';
  header[1] = 'M';
  *(uint32_t *)&header[2] = fileSize;
  *(uint32_t *)&header[10] = 54;
  *(uint32_t *)&header[14] = 40;
  *(int32_t *)&header[18] = width;
  *(int32_t *)&header[22] = height;
  *(uint16_t *)&header[26] = 1;
  *(uint16_t *)&header[28] = 24;
  *(uint32_t *)&header[34] = dataSize;

  if (!target) {

    FsFile fs;

    mountSDCard();
    fs = sd.open(filename, O_WRITE | O_CREAT | O_TRUNC);

    if (!fs) {
      Serial.println("Failed to open file on SDCard");
      tft.println("Failed to open file on SDCard");
    }

    fs.write(header, 54);

    // Write rows bottom-to-top

    for (int y = height - 1; y >= 0; y--) {
      int idx = 0;
      for (int x = 0; x < width; x++) {
        uint8_t packed = fBuf[y * width + x];

        // Unpack symmetrically
        uint8_t r3 = (packed >> 5) & 0x07;
        uint8_t g3 = (packed >> 2) & 0x07;
        uint8_t b2 = packed & 0x03;

        uint8_t r = (r3 * 255) / 7;
        uint8_t g = (g3 * 255) / 7;
        uint8_t b = (b2 * 255) / 3;

        // BMP expects BGR order
        rowBuf[idx++] = b;
        rowBuf[idx++] = g;
        rowBuf[idx++] = r;
      }
      while (idx % 4) rowBuf[idx++] = 0;  // pad
      fs.write(rowBuf, idx);
    }

    fs.close();

  }

  else {

    File fss;

    if (!LittleFS.begin()) {
      Serial.println("LittleFS init failed");
      tft.println("LittleFS init failed");
      return;
    }

    String filePath = "/" + String(filename);

    fss = LittleFS.open(filePath, FILE_WRITE);

    if (!fss) {
      Serial.println("Failed to open file on LittleFS");
      tft.println("Failed to open file on LittleFS");
    }


    if (!rawMode) {
      fss.write(header, 54);


      // Write rows bottom-to-top
      for (int y = height - 1; y >= 0; y--) {
        int idx = 0;
        for (int x = 0; x < width; x++) {
          uint8_t packed = fBuf[y * width + x];
          uint8_t r3 = (packed >> 5) & 0x07;
          uint8_t g3 = (packed >> 2) & 0x07;
          uint8_t b2 = packed & 0x03;

          uint8_t r = (r3 * 255) / 7;
          uint8_t g = (g3 * 255) / 7;
          uint8_t b = (b2 * 255) / 3;

          // bgr for bmp
          rowBuf[idx++] = b;
          rowBuf[idx++] = g;
          rowBuf[idx++] = r;
        }
        while (idx % 4) rowBuf[idx++] = 0;  // pad
        fss.write(rowBuf, idx);
      }
    }

    else

      fss.write(fBuf, width * height);

    fss.close();
  }
}
//##########################################################################################################################/


void storePixel(int x, int y, uint16_t rgb565) {

  uint8_t r5 = (rgb565 >> 11) & 0x1F;  // 0–31
  uint8_t g6 = (rgb565 >> 5) & 0x3F;   // 0–63
  uint8_t b5 = rgb565 & 0x1F;          // 0–31

  // Symmetric quantization with rounding
  uint8_t r3 = (r5 * 7 + 15) / 31;  // map 0–31 → 0–7
  uint8_t g3 = (g6 * 7 + 31) / 63;  // map 0–63 → 0–7
  uint8_t b2 = (b5 * 3 + 15) / 31;  // map 0–31 → 0–3

  // Pack into one byte
  uint8_t packed = (r3 << 5) | (g3 << 2) | b2;
  fBuf[y * 320 + x] = packed;
}
//##################################################################################################################/

bool selectMedia() {


  int selectedMedia = 0;

  tft.fillScreen(TFT_BLACK);
  setFreeFont();
  tft.setCursor(0, 20);
  tft.setTextSize(1);
  tft.setTextColor(textColor);
  tft.print("Save image to either internal flash,\nor SD card. ");
  tft.print("Internal flash can hold\n\n approx. 4 bmp or 12 raw images.\n");
  tft.print("Use WiFi Sync to download images.\nUse SSTV viewer to view images");


  draw4Buttons();

  tft.setCursor(25, 260);
  tft.print("Don't");
  tft.setCursor(25, 276);
  tft.print("Save");
  tft.setCursor(190, 265);
  tft.print("Flash");
  tft.setCursor(262, 265);
  tft.print("SDCard");
  setTextFont();
  tft.setTextSize(2);
  tDoublePress();
  get_Touch();
  int buttonID = getButtonID();
  tft.fillScreen(TFT_BLACK);

  switch (buttonID) {

    case 41:
      tRel();
      tx = 0;
      ty = 0;
      pressed = false;
      return false;
    case 42:
      break;
    case 43:
      tft.setCursor(0, 0);
      tft.print("Saving image to flash.");
      selectedMedia = 1;
      break;
    case 44:
      tft.setCursor(0, 0);
      tft.print("Saving image to SD card.");
      selectedMedia = 2;
      break;
    default:
      break;
  }


  String fn;

  if (selectedMedia == 1) {  // flash

    if (rawMode)
      checkLittleFSSpaceAndCleanup(true);
    else
      checkLittleFSSpaceAndCleanup(false);


    fn = createConsecutiveFilename(rawMode);                    // create a new file number
    writeScreenbuffertoFile(fn.c_str(), 320, 240, 1, rawMode);  // flash
    tft.fillScreen(TFT_BLACK);
  }

  else if (selectedMedia == 2) {
    fn = createConsecutiveFilename(false);                    // always bmp
    writeScreenbuffertoFile(fn.c_str(), 320, 240, 0, false);  // SD
    tft.fillScreen(TFT_BLACK);
  }

  setTextFont();
  tft.setTextSize(2);
  return true;
}

//##################################################################################################################/
void showSSTVFiles() {


  tft.fillScreen(TFT_BLACK);
  setFreeFont();
  tft.setCursor(0, 20);
  tft.setTextSize(1);
  tft.setTextColor(textColor);
  tft.print("Displays SSTV images from either\ninternal flash, or, (if installed) SD card.\n\n");
  tft.print("Move encoder to cycle between images.\n\nPress encoder to leave.");


  draw4Buttons();

  tft.setCursor(25, 265);
  tft.print("Back");
  tft.setCursor(190, 265);
  tft.print("Flash");
  tft.setCursor(268, 265);
  tft.print("SDCard");
  setTextFont();
  tft.setTextSize(2);
  tDoublePress();
  get_Touch();
   int buttonID = getButtonID();
  tft.fillScreen(TFT_BLACK);

  switch (buttonID) {

    case 41:
      return;
    case 42:
      break;
    case 43:
      viewFile(true);  // LittleFS
      break;
    case 44:
      viewFile(false);  //SD
      break;
    default:
      break;
  }

  setTextFont();
  tft.setTextSize(2);
}


//##################################################################################################################/

void displayBMP(File &f) {
  tft.fillScreen(TFT_BLACK);

  uint8_t header[54];
  f.read(header, 54);
  int32_t width  = *(int32_t *)&header[18];
  int32_t height = *(int32_t *)&header[22];
  uint16_t bpp   = *(uint16_t *)&header[28];

  if (width != 320 || height != 240 || bpp != 24) {
    Serial.println("wrong format");
    return;
  }

  uint8_t rowBuf[320 * 3];      

  // read upside wown
  for (int y = 0 - 1; y <= height; y++) {
    int bytesRead = f.read(rowBuf, sizeof(rowBuf));
    if (bytesRead < sizeof(rowBuf)) break;

    for (int x = 0; x < 480; x++) {
      int srcX = x * width / 480;   
      uint8_t b = rowBuf[srcX * 3 + 0];
      uint8_t g = rowBuf[srcX * 3 + 1];
      uint8_t r = rowBuf[srcX * 3 + 2];
      lineBuffer[x] = tft.color565(r, g, b);
    }


    int destYStart = (height - 1 - y) * 320 / 240; // scale 
    int destYEnd   = (height - y) * 320 / 240;

 
    for (int destY = destYStart; destY < destYEnd; destY++) {
      tft.startWrite();
      tft.setAddrWindow(0, destY, 480, 1);
      tft.pushColors(lineBuffer, 480, true);
      tft.endWrite();
    }
  }
}


//##################################################################################################################/

void displayRAW(File &f) {
  uint8_t rowBuf[320];       


  for (int y = 0; y < 240; y++) {
    int bytesRead = f.read(rowBuf, 320);
    if (bytesRead < 320) break;

    // Scale horizontally: 320 → 480
    for (int x = 0; x < 480; x++) {
      int srcX = x * 320 / 480;
      uint8_t packed = rowBuf[srcX];

      uint8_t r3 = (packed >> 5) & 0x07;
      uint8_t g3 = (packed >> 2) & 0x07;
      uint8_t b2 = packed & 0x03;

      uint8_t r = (r3 * 255) / 7;
      uint8_t g = (g3 * 255) / 7;
      uint8_t b = (b2 * 255) / 3;

      lineBuffer[x] = tft.color565(r, g, b);
    }

   // 240 to 320
    int destYStart = y * 320 / 240;
    int destYEnd   = (y + 1) * 320 / 240;

    // Draw multiple to close
    for (int destY = destYStart; destY < destYEnd; destY++) {
      tft.startWrite();
      tft.setAddrWindow(0, destY, 480, 1);
      tft.pushColors(lineBuffer, 480, true);
      tft.endWrite();
    }

    if (clw || cclw) return;
  }
}



//##################################################################################################################/

void viewFile(bool useLittleFS) {
  
  
int currentFileIndex = -1;  // will be set to highest available
  int lowest = INT_MAX;
  int highest = -1;

  if (!useLittleFS)
    readSDCard(true);  // init SD

  tft.setTextSize(1);
  // Scan filesystem for sstv files
  File root = useLittleFS ? LittleFS.open("/") : SD.open("/");


  while (true) {
    File f = root.openNextFile();
    if (!f) break;
    String name = f.name();
    f.close();

    if (name.indexOf("sstv") >= 0 && (name.endsWith(".raw") || name.endsWith(".bmp"))) {
      int start = name.indexOf("sstv");
      int end = name.lastIndexOf('.');
      int num = name.substring(start + 4, end).toInt();
      if (num > 0) {
        if (num < lowest) lowest = num;
        if (num > highest) highest = num;
      }
    }
  }



  if (lowest == INT_MAX) {
    tft.setCursor(0, 0);
    tft.println("No sstv files found");
    delay(1000);
    return;
  }



  // Initialize to highest if first time
  if (currentFileIndex < lowest || currentFileIndex > highest) {
    currentFileIndex = highest;
  }

File f;

  while (true) {


    if (clw) {
      currentFileIndex++;
    }
    if (cclw) {
      currentFileIndex--;
    }



    if (currentFileIndex < lowest) {
      currentFileIndex = highest;
    }

    if (currentFileIndex > highest) {
      currentFileIndex = lowest;
    }

    if (useLittleFS) {
      String fnameRaw = "sstv" + String(currentFileIndex) + ".raw";
      f = LittleFS.open("/" + fnameRaw, FILE_READ);
      
      
      if (!f){ 
      String fnameBmp = "sstv" + String(currentFileIndex) + ".bmp";
      f = LittleFS.open("/" + fnameBmp, FILE_READ);
      }
    
    }

    else {
      String fnameBmp = "sstv" + String(currentFileIndex) + ".bmp";
      f = SD.open("/" + fnameBmp, FILE_READ);
    }

    

    if (!f) {
      Serial.printf("file index %d not found\n", currentFileIndex);
      if (cclw) {  // gap in numbering? try until opening next file.
        continue;
      }

      else if (clw) {
        continue;
      }

    
    }

  else {
    clw = false;
    cclw = false;
  } 
   

    if (String(f.name()).endsWith(".raw")) {
      displayRAW(f);
    } else {
      displayBMP(f);
    }

   

    String fname = f.name();
    f.close();

    tft.fillRect(400, 310, 80, 10, TFT_BLACK);
    tft.setCursor(400, 310);
    tft.print(fname);



    while (!(clw + cclw)) {
     uint16_t z = tft.getTouchRawZ(); 
     if(z > 300) {   
      int dir =  dragScroll(); 
      if(dir == 1) cclw = true;
      else if (dir == -1) clw = true;


     }

  
        if (digitalRead(ENCODER_BUTTON) == LOW){
        setScroll(0);
        SD.end();
        currentFileIndex = -1;
        tft.setTextSize(2);
        return;
      }
    }
  }
}

//##########################################################################################################################//

// Weather fax

//##########################################################################################################################//
int32_t lineDuration = 499790;  // 499650


uint32_t encoderModifyLineDuration(uint32_t lineDuration) {  // debug function


  if (clw || cclw) {

    if (clw) {
      lineDuration -= 5;
      clw = false;
    }

    if (cclw) {
      lineDuration += 5;
      cclw = false;
    }
    tft.fillRect(0, 220, 320, 16, TFT_BLACK);
    tft.setCursor(0, 220);
    tft.printf("Move encoder to align vertically. Linedurtion:%ld", lineDuration);
  }

  return lineDuration;
}


//##########################################################################################################################/



void wefaxInit() {
  tRel();

  fBuf = (uint8_t *)malloc(320* 240);  // 1 byte per pixel
  if (!fBuf) {
    buffErr();
    return;
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(0, 200);
  tft.printf("Waiting for phasing completion.");

  tft.setCursor(0, 210);
  tft.print("Touch screen to start without phasing.");
  tft.setCursor(0, 0);
  wefaxStartWait();
  tft.fillScreen(TFT_BLACK);

  tft.fillRect(0, 220, 320, 16, TFT_BLACK);
  tft.setCursor(0, 210);
  tft.printf("Touch screen to leave.\n");
  tft.setCursor(0, 220);
  tft.printf("Move encoder to align vertically. Linedurtion:%ld", lineDuration);

  delay(250);  //shift white shoulder
  decodewefaxFrame();

  while (true) {
    uint16_t z = tft.getTouchRawZ();  // hold image
    if (z > 300) break;
  }

  selectMedia();  // select where to save
  free(fBuf);
}
//##########################################################################################################################//
void wefaxStartWait() {

  int16_t ctr = 0;
  uint32_t frameStart, frameEnd, frameDuration;
  uint16_t c = 0;
  uint32_t accum = 0;
  int16_t phasingLines = 0;

  frq = 0;

  while (true) {


    while (true) {  // stage one wait for 1500Hz

      uint16_t z = tft.getTouchRawZ();  // touched
      if (z > 300) return;

      uint32_t now = micros();
      // stage 1
      while (micros() < now + 2000) {  // 2ms sample time
        frq = getAnalogFrequency();
        accum += frq;
        c++;
      }



      if (c)
        frq = accum / c;

      accum = 0;
      c = 0;

      if (frq > 1400 && frq < 1600)  // black has started
        ctr++;
      else ctr--;
      if (ctr < 0) ctr = 0;
      else if (ctr > 4) break;
    }

    ctr = 0;
    frq = 0;
    frameStart = micros();



    while (true) {  // stage 2

      uint16_t z = tft.getTouchRawZ();  // touched
      if (z > 300) return;

      uint32_t now = micros();

      uint16_t c = 0;
      uint32_t accum = 0;
      while (micros() < now + 2000) {  // 1ms
        frq = getAnalogFrequency();
        accum += frq;
        c++;
      }

      if (c)
        frq = accum / c;

      //Serial.printf("stage 2 frq%d\n", frq);

      if (frq > 2150 && frq < 2450)  // white
        ctr++;
      else ctr--;

      if (ctr < 0) ctr = 0;
      else if (ctr > 2) break;

    }  // black has ended




    frameEnd = micros();
    frameDuration = frameEnd - frameStart;

    if (frameDuration > 470000 && frameDuration < 480000) {
    }

    else {
      delay(50);
    }

    tft.setCursor(0, 0);
    tft.fillRect(0, 0, 320, 8, TFT_BLACK);
    
    
    phasingLines++;
    
    
    tft.printf("Phasing Lines: %d\n", phasingLines);
    if (phasingLines > 40)
      return;
  }
}


//##########################################################################################################################//

bool decodewefaxLine(int line) {  // Scan line frequency: 2 lines per second (120 lines per minute)

 //static float pixelDuration = 824.675;  //theoretical

   lineDuration = 499540;

  static float pixelDuration = 823.0;  // shorter to allow time for compensator

  uint32_t lineStartTime = micros();
  uint32_t lineEndTime = lineStartTime + lineDuration;  // 500 ms per line
  uint32_t pixelStartTime = lineStartTime + 25000;
  uint16_t blackCounts = 0;
  delayMicroseconds(25000);  // white shoulder

  int phase = line % 4; // write 1/4 lines to display


 if (phase == 3) 
   pixelDuration = 820.5; // shorter, phase 3 needs time to write to screen
  else
   pixelDuration = 823.0;


  for (int x = 0; x < 576; ++x) {

    uint32_t pixelEndTime =
      pixelStartTime + (uint32_t)((x + 1) * pixelDuration);

    uint16_t ctr = 0;
    uint32_t accum = 0;

    while (micros() < pixelEndTime) {
      frq = getAnalogFrequency();
      accum += frq;
      ctr++;
    }

    if (ctr)
      frq = accum / ctr;

    lineDuration = encoderModifyLineDuration(lineDuration);

    int pos = (x * 480) / 576;

    // start of a new 5-line block
    if (phase == 0)
      transferBuffer[pos] = 0;

    if (frq < 1800) {
      transferBuffer[pos]++;
       blackCounts++;
    }
  
  }
     

  // draw aftr the 4th accumulated line
  if (phase == 3) {

    for (int pos = 0; pos < 480; pos++) {


      switch (transferBuffer[pos]) {

        case 0:
          lineBuffer[pos] = TFT_WHITE;
          break;

        case 1:
          lineBuffer[pos] = TFT_SILVERBLUE;  
          break;

        case 2:
          lineBuffer[pos] = TFT_BLUE;
          break;

        case 3:
          lineBuffer[pos] = TFT_BLACK;
          break;
      }

      storePixel((pos * 320) / 480, line / 5, lineBuffer[pos]);
    }

     if(blackCounts > 2) { // do not write empty lines, 2 is a good treshold
   
  

    tft.startWrite();
    tft.setAddrWindow(0, line / 4, 480, 1);
    tft.pushColors(lineBuffer, 480, true);
    tft.endWrite();
    
     }  
  }

  int32_t timeDiff = micros() - lineEndTime;


  Serial.printf("timeError %ld  blackCounts%d  line %d\n", timeDiff, blackCounts, line);

  while (micros() < lineEndTime)
    ;

 if(blackCounts > 2) return true; 
 else return false; // empty line


}

//##########################################################################################################################//

void decodewefaxFrame() {
  int line = 0;

  memset(fBuf, 0x00, 320 * 240);

  for (line = 0; line < 1200; line++) {
    

    if (!decodewefaxLine(line) && line)
        line -=1;


    Serial.printf("Line %d\n", line);
    uint16_t z = tft.getTouchRawZ();  // while building image
    if (z > 300) return;
  }

  line = 0;
}
//##########################################################################################################################//


void initScroll()
{   tft.startWrite(); 
    tft.writecommand(0x33);
    tft.writedata(0);
    tft.writedata(0);
    tft.writedata(0x01);
    tft.writedata(0x40); // 320

    tft.writedata(0);
    tft.writedata(0);
    tft.endWrite();

}
//##################################################################################################################/
void setScroll(uint16_t pos)
{
    pos %= 480;
    tft.startWrite();
    tft.writecommand(0x37);
    tft.writedata(pos >> 8);
    tft.writedata(pos & 0xFF);
    tft.endWrite();
}


//##################################################################################################################/


int dragScroll()
{
    static bool dragging = false;
    static uint16_t lastTx = 0;
    static int32_t scrollPos = 0;

    if (tft.getTouch(&tx, &ty))
    {
        
      if (tx < 10)
          return - 1;
      else if (tx > 470)
          return 1;   
        
        if (!dragging)
        {
            dragging = true;
            lastTx = tx;
        }
        else
        {
            int16_t dx = tx - lastTx;

            scrollPos -= dx;

            while (scrollPos < 0)
                scrollPos += 480;

            while (scrollPos >= 480)
                scrollPos -= 480;

            setScroll(scrollPos * -1);

            lastTx = tx;
        }
    }
    else
    {
        dragging = false;
    }

return 0;

}

//##########################################################################################################################//

void drawVUBar(int x, int y, int vuValue) {
  const int width = 60;
  const int height = 5;

  if (vuValue < 0) vuValue = 0;
  if (vuValue > 100) vuValue = 100;


  int filled = (vuValue * width) / 100;


  tft.fillRect(x, y, width, height, TFT_BLACK);

  for (int i = 0; i < filled; i++) {
    float ratio = (float)i / width;
    uint16_t color;

    if (ratio < 0.5f) {
      //r to y
      color = blendBarColors(TFT_GREEN, TFT_YELLOW, ratio * 2.0f);
    } else {
      // y to g
      color = blendBarColors(TFT_YELLOW, TFT_RED, (ratio - 0.5f) * 2.0f);
    }

    tft.drawFastVLine(x + i, y, height, color);
  }
}

//##########################################################################################################################//

uint16_t blendBarColors(uint16_t c1, uint16_t c2, float ratio) {
  // extract RGB565
  uint8_t r1 = ((c1 >> 11) & 0x1F) << 3;
  uint8_t g1 = ((c1 >> 5) & 0x3F) << 2;
  uint8_t b1 = (c1 & 0x1F) << 3;

  uint8_t r2 = ((c2 >> 11) & 0x1F) << 3;
  uint8_t g2 = ((c2 >> 5) & 0x3F) << 2;
  uint8_t b2 = (c2 & 0x1F) << 3;

  // interpolate
  uint8_t r = r1 + (r2 - r1) * ratio;
  uint8_t g = g1 + (g2 - g1) * ratio;
  uint8_t b = b1 + (b2 - b1) * ratio;

  // back to uint16
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

//##########################################################################################################################//

