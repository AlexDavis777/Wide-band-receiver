


bool sentMarker = false;
bool sentRxSetup = false;
int rxChan = -1;
int predictor = 0;
int stepIdx = 0;


uint16_t disconnects = 0;
uint64_t kiwiTimestamp;
bool wfOpened = false;
bool SNDOpened = false;
bool drawingWF = false;
uint16_t kiwiPort = 8073;
bool sentSNDParams = false;
bool wfLineReady = false;
bool audioblink = false;
bool clearMessage = false;
uint16_t wfY = 65;
uint16_t wfX = 0;
uint32_t blocked = 0;




//##########################################################################################################################//

//##########################################################################################################################//


void wsSndEvent(WStype_t type,
                uint8_t* payload,
                size_t length) {


  if (displayDebugInfo)
    Serial.printf("SND: payloadLen=%u\n", length);

  switch (type) {



    case WStype_CONNECTED:
      Serial.println("SND CONNECTED");
      if (!sentSNDParams) {
        if (showKiwiWF)
          openWaterfallSocket();


        kiwiSend("SET auth t=kiwi p=#");
        delay(10);
        kiwiSend("SET AR OK in=12000 out=44100");
        delay(10);
        kiwiSend("SERVER DE CLIENT openwebrx.js SND");
        delay(10);
        kiwiSend("SET dbug_v=0,0,0,0,0");
        delay(10);
        kiwiSend("SET agc=0 hang=0 thresh=0 slope=0 decay=0 manGain=0");
        delay(10);
        kiwiSend("SET browser=Mozilla/5.0 (X11; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/115.0");
        delay(10);
        kiwiSend("SET nb type=0 en=0");
        delay(10);
        kiwiSend("SET nb type=1 en=1");
        delay(10);
        kiwiSend("SET nb type=2 en=0");
        delay(10);
        kiwiSend("SET nb algo=0");
        delay(10);
        kiwiSetFreqMode(FREQ, modType);
        delay(10);
        kiwiSend("SET genattn=0");
        delay(10);
        kiwiSend("SET gen=0 mix=-1");
        delay(10);
        kiwiSend("SET de_emp=0 nfm=0");
        delay(10);
        kiwiSend("SET compression=1");
        delay(10);
        kiwiSend("SET squelch=0 param=0.0");
        delay(10);
        kiwiSend("SET agc=1 hang=0 thresh=-100 slope=6 decay=1000 manGain=50");
        delay(10);
        kiwiSend("SET GET_USERS");
        delay(10);
        kiwiSend("SET GET_CONFIG");
        sentSNDParams = true;
      }

      break;

    case WStype_BIN:

      if (length < 100)             // a small packet
        findbadp(payload, length);  // check for connection



      if (length == 1034) {


      if (payload[3] & 0x02) {
        Serial.println("!!! KIWI ADC OVERFLOW !!!");
}


        if (!clearMessage) {  // overwrite the Loading... mesafe
          tft.fillRect(105, 300, 200, 8, TFT_BLACK);
          clearMessage = true;
        }

        uint32_t t0 = micros();
        bool warmup = millis() < blocked + 500;
  
        for (int i = 10; i < 1034; i++) {
          uint8_t b = payload[i];

          if (warmup) {
            decodeNibbleToDAC(b & 0x0F);
            decodeNibbleToDAC((b >> 4) & 0x0F);  // sync the encoder
            continue;                            // no DAC output to drain buffer
          }


          // theoretically 82uS, but real value is lower (DAC and parser processing time)
          // dacDelay 70 - 78 produce good AM audio without dropuots, but cause audio lag when tuning/changing mode

          // dacDelay 69 and 70 produce good audio and low lag /fast tuning

          // dacDelay < 69 produce high pitch and dropouts, while fast tuning remains

          uint32_t t = micros();

          uint8_t s = decodeNibbleToDAC(b & 0x0F);
          dac2.outputVoltage(s);
        if (! kiwiUIParser()) // exit pressed
           return;  
          while (micros() < t + dacDelay)
            ;
           dac2.outputVoltage((uint8_t) 0);
         

          t = micros();
          s = decodeNibbleToDAC((b >> 4) & 0x0F);
         dac2.outputVoltage(s);
          
         if (! kiwiUIParser())
           return;  
          while (micros() < t + dacDelay)
            ;
        }


        uint32_t dt = micros() - t0;

       if (displayDebugInfo)
          Serial.printf("AUDIO packet processing: %lu us dacDelay%d\n", dt, dacDelay);
        

        static uint32_t told = 0; 
        uint32_t tnow = dt/1000;  
        if(tnow != told){ 

        tft.fillRect (450, 40, 20, 8, TFT_BLACK);
        tft.setCursor(450, 40);
        tft.printf("%ld", tnow);
        told = tnow;
        }


        audioblink = !audioblink;

        if (audioblink)
          tft.fillCircle(470, 20, 3, TFT_RED);
        else
          tft.fillCircle(470, 20, 3, TFT_BLACK);
        uint16_t sm = ((uint16_t)payload[8] << 8) | payload[9];
        drawVUBar(400, 20, sm / 15);
      }




/*
if (length == 1034) {

  uint32_t t0 = micros();
  bool warmup = millis() < blocked + 500;

  static uint64_t totalWork = 0;
  static uint64_t totalWait = 0;
  static uint32_t sampleCount = 0;

  for (int i = 10; i < 1034; i++) {

    uint8_t b = payload[i];

    if (warmup) {
      decodeNibbleToDAC(b & 0x0F);
      decodeNibbleToDAC((b >> 4) & 0x0F);
      continue;
    }

    // --------------------------------------------------
    // Low nibble
    // --------------------------------------------------

    uint32_t t = micros();

    uint8_t s = decodeNibbleToDAC(b & 0x0F);
    dac2.outputVoltage(s);
    kiwiUIParser();

    uint32_t afterWork = micros();

    uint32_t deadline = t + dacDelay;

    if ((int32_t)(deadline - afterWork) > 0) {
      while ((int32_t)(micros() - deadline) < 0);
    }

    uint32_t afterWait = micros();

    totalWork += (uint32_t)(afterWork - t);
    totalWait += (uint32_t)(afterWait - afterWork);
    sampleCount++;


    // --------------------------------------------------
    // High nibble
    // --------------------------------------------------

    t = micros();

    s = decodeNibbleToDAC((b >> 4) & 0x0F);
    dac2.outputVoltage(s);
    kiwiUIParser();

    afterWork = micros();

    deadline = t + dacDelay;

    if ((int32_t)(deadline - afterWork) > 0) {
      while ((int32_t)(micros() - deadline) < 0);
    }

    afterWait = micros();

    totalWork += (uint32_t)(afterWork - t);
    totalWait += (uint32_t)(afterWait - afterWork);
    sampleCount++;
  }

  uint32_t dt = micros() - t0;

  if (!warmup && sampleCount > 0) {

    uint32_t avgWork = totalWork / sampleCount;
    uint32_t avgWait = totalWait / sampleCount;
    uint32_t avgTotal = (totalWork + totalWait) / sampleCount;

    Serial.printf(
      "AUDIO: packet=%lu us  dacDelay=%d  work=%lu us  wait=%lu us  total=%lu us\n",
      dt,
      dacDelay,
      avgWork,
      avgWait,
      avgTotal
    );

    totalWork = 0;
    totalWait = 0;
    sampleCount = 0;
  }

  audioblink = !audioblink;

  if (audioblink)
    tft.fillCircle(475, 10, 3, TFT_RED);
  else
    tft.fillCircle(475, 10, 3, TFT_BLACK);

  uint16_t sm = ((uint16_t)payload[8] << 8) | payload[9];
  drawVUBar(400, 20, sm / 15);
}



*/


      //------------------
      if (length != 1034 && displayDebugInfo) {
        for (size_t i = 0; i < 128; i++) {
          Serial.write(payload[i]);
        }
        Serial.println();
      }

      break;


    case WStype_DISCONNECTED:

      disconnects++;
      Serial.printf("SND disconnects: %d\n", disconnects);

      if (disconnects > 10) {
        tft.setCursor(0, 70);
        tft.println("No sound frame!");
        delay(2000);
        kiwiExit();
        return;
      }

      break;

    default:
      break;
  }
}
//##########################################################################################################################//


void wsWfEvent(WStype_t type,
               uint8_t* payload,
               size_t length) {

  switch (type) {

    case WStype_CONNECTED:
      Serial.println("WF CONNECTED");
      kiwiSendWF("SET auth t=kiwi p=#");

      break;



    case WStype_BIN:


      if (length == 533) {  // compressed WF packet

        uint8_t wfTemp[1034];

        wfDecoderReset();

        int out = 0;

        // Decode compressed payload (517 bytes)
        for (int i = 16; i < 533; i++) {
          uint8_t b = payload[i];

          wfTemp[out++] = decodeWFNibble(b & 0x0F);
          wfTemp[out++] = decodeWFNibble((b >> 4) & 0x0F);
        }

        // Remove the ADPCM padding
        out -= 10;  // should now be 1024



        processWFpacket(wfTemp);
        waterfallTask();
      }





      if (!sentRxSetup) {

        int ch = findRxChan(payload, length);

        if (ch >= 0) {

          rxChan = ch;

          char cmd[64];

          sprintf(cmd, "SET STATS_UPD ch=%d", rxChan);
          kiwiSend(cmd);


          kiwiSetFreqMode(FREQ, modType);

          sentRxSetup = true;

          tft.setCursor(0, 130);
          tft.printf("Channel: %d", rxChan);


          if (displayDebugInfo)
            Serial.printf("\n*********************************RX CHAN=%d********************************************\n", rxChan);

          delay(10);
          kiwiSendWF("SERVER DE CLIENT openwebrx.js W/F");
          delay(10);
          kiwiSendWF("SET send_dB=1");
          delay(10);
          char ts[32];
          snprintf(ts, sizeof(ts), "SET zoom=5 cf=%ld", FREQ_TO_KHZ);  // center freq
          kiwiSendWF(ts);
          delay(10);
          kiwiSendWF("SET interp=13");
          delay(10);
          kiwiSendWF("SET window_func=2");
          delay(10);
          kiwiSendWF("SET wf_speed=4");
          delay(10);
          kiwiSendWF("SET wf_comp=1");
          delay(10);
          kiwiSendWF("SET maxdb=0 mindb=-110");
          delay(10);
          kiwiSendWF("SET wf_speed=1");
          delay(10);
          kiwiSendWF("SET antsw_init");
          delay(10);
          kiwiSendWF("SET antsw_GetAntenna");
          delay(10);
          kiwiSendWF("SET antsw_check_set_default");

          kiwiSendWF("SET aper=1 algo=3 param=0.00");

          kiwiSendWF("SET cmap=0");
        }
      }



      if (displayDebugInfo) {
        Serial.printf("WF: len=%u ", (unsigned)length);
        for (size_t i = 0; i < 128; i++) {
          if (isprint(payload[i])) Serial.write(payload[i]);
        }
        Serial.println();
      }

      break;

    case WStype_DISCONNECTED:
      Serial.println("WF DISCONNECTED");

      break;

    default:
      break;
  }
}

//##########################################################################################################################//

bool setupKiwi() {


  if (FREQ > 30000000)  // Kiwi limit
    return false;

  tft.setTextSize(2);
  connectWIFI();
  tft.setTextSize(1);
  tft.setTextColor(TFT_GREEN);
  if (!kiwiSessionInit())
    return false;

  si4735.setAudioMute(true);
  OLDSTEP = STEP;
  FREQ_OLD = FREQ;
  lastModType = modType;
  kiwiTimestamp = (1ULL << 62) + (esp_timer_get_time() / 1000ULL);  // generate s session identifier

  char ts[32];

  snprintf(ts, sizeof(ts), "%llu", (unsigned long long)kiwiTimestamp);

  String path = "/ws/kiwi/";
  path += ts;
  path += "/SND";

  if (displayDebugInfo) {
    Serial.print("SND path: ");
    Serial.println(path);
  }




  disconnects = 0;

  wsSND.onEvent(wsSndEvent);
  wsSND.begin(url, kiwiPort, path.c_str());
  SNDOpened = true;
  return true;
}



//##########################################################################################################################//

void openWaterfallSocket() {
  if (wfOpened) return;

  wfOpened = true;

  char ts[32];

  snprintf(ts, sizeof(ts), "%llu", (unsigned long long)kiwiTimestamp);

  String path = "/ws/kiwi/";
  path += ts;
  path += "/W/F";

  if (displayDebugInfo) {
    Serial.print("WF path: ");
    Serial.println(path);
  }
  wsWF.onEvent(wsWfEvent);
  wsWF.begin(url, kiwiPort, path.c_str());
}


//##########################################################################################################################//
void loopKiwi() {

  static uint32_t lastKeepalive = millis();

  while (true) {


    if (SNDOpened)
      wsSND.loop();
    else return;


    if (!drawingWF && showKiwiWF)
      wsWF.loop();


    if (millis() - lastKeepalive > 5000) {

      lastKeepalive = millis();
      kiwiSend("SET keepalive");


      if (wfOpened)
        kiwiSendWF("SET keepalive");
    }
  }
}
//##########################################################################################################################//


void kiwiSend(const char* cmd) {
  if (displayDebugInfo)
    Serial.printf("SNDSENT: %s\n", cmd);

  wsSND.sendBIN(
    (uint8_t*)cmd,
    strlen(cmd));
}


//##########################################################################################################################//


void kiwiSendWF(const char* cmd) {

  if (displayDebugInfo)
    Serial.printf("WFSENT: %s\n", cmd);

  wsWF.sendBIN(
    (uint8_t*)cmd,
    strlen(cmd));
}

//##########################################################################################################################//
bool payloadContains(uint8_t* payload, size_t length, const char* s) {
  size_t sl = strlen(s);

  for (size_t i = 0; i + sl <= length; i++) {
    if (memcmp(payload + i, s, sl) == 0)
      return true;
  }

  return false;
}

//##########################################################################################################################//
int findRxChan(uint8_t* payload, size_t length) {
  const char* needle = "rx_chan=";
  const size_t nlen = strlen(needle);

  for (size_t i = 0; i + nlen <= length; i++) {

    if (memcmp(payload + i, needle, nlen) == 0) {

      int rx_chan = payload[22] - 48;

      return rx_chan;
    }
  }

  return -1;
}

//##########################################################################################################################//



void findbadp(uint8_t* payload, size_t length) {
  const char* needle = "badp=";
  const size_t nlen = strlen(needle);
  int errcode = 0;


  for (size_t i = 0; i + nlen <= length; i++) {

    if (memcmp(payload + i, needle, nlen) == 0) {

      errcode = payload[9] - 48;

      tft.setCursor(0, 300);
      tft.print("Server status: ");

      switch (errcode) {

        case 0:
          tft.printf("OK   Loading config....");
          break;

        case 1:
          tft.printf("No slots available");
          break;

        case 2:
          tft.printf("Determining local IP");
          break;

        case 3:
          tft.printf("IP not allowed");
          break;

        case 4:
          tft.printf("No admin password");
          break;

        case 5:
          tft.printf("No multiple connections");
          break;

        case 6:
          tft.printf("Database update");
          break;

        case 7:
          tft.printf("Admin already open");
          break;

        case 8:
          tft.printf("Reset too late");
          break;

        case 9:
          tft.printf("Reset not serial");
          break;

        case 10:
          tft.printf("Reset not local");
          break;

        case 11:
          tft.printf("Reset already open");
          break;

        case 12:
          tft.printf("Reset OK");
          break;

        default:
          tft.printf("%d", errcode);
          break;
      }
    }
  }

  if (errcode) {
    delay(2000);
    kiwiExit();
  }
}


//##########################################################################################################################//




static const int16_t STEP_TABLE[89] = {
  7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
  34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
  157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
  724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
  3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
  15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static const int8_t INDEX_TABLE[16] = {
  -1, -1, -1, -1, 2, 4, 6, 8,
  -1, -1, -1, -1, 2, 4, 6, 8
};

//##########################################################################################################################//

static inline uint8_t decodeNibbleToDAC(uint8_t code)  // ADPCM decoder adapted from https://github.com/mcogoni/supersdr/blob/main/kiwi/client.py
{
  int step = STEP_TABLE[stepIdx];

  stepIdx += INDEX_TABLE[code];
  if (stepIdx < 0) stepIdx = 0;
  if (stepIdx > 88) stepIdx = 88;

  int diff = step >> 3;

  if (code & 1) diff += step >> 2;
  if (code & 2) diff += step >> 1;
  if (code & 4) diff += step;

  if (code & 8)
    diff = -diff;

  predictor += diff;

  if (predictor > 32767) predictor = 32767;
  if (predictor < -32768) predictor = -32768;
  return (predictor >> 8) + 128;
}




//##########################################################################################################################//

#define TUNE_DELAY 300  // ms

bool kiwiUIParser() {

  static int16_t pendingSteps = 0;
  static uint32_t lastStepTime = 0;
  static uint32_t lastTouchCheck = 0;

  // Touch only needs to be polled every few milliseconds.
  if (micros() - lastTouchCheck >= 5000) {   // 5 ms
    lastTouchCheck = micros();

    if (tft.getTouchRawZ() > 300) {          // touched
      
     if(!setKiwiParameters()) // exit pressed
      return false; 
    }
  }

  if (clw) {

    pendingSteps++;

    lastStepTime = millis();

    displayFREQ(FREQ + pendingSteps * STEP);

    clw = false;
  }

  if (cclw) {

    pendingSteps--;

    lastStepTime = millis();

    displayFREQ(FREQ + pendingSteps * STEP);

    cclw = false;
  }

  if (pendingSteps && (millis() - lastStepTime) >= TUNE_DELAY) {

    FREQ += pendingSteps * STEP;

    kiwiSetFreqMode(FREQ, modType);

    wfY = 65;
    wfX = 0;

    pendingSteps = 0;
  }

return true;

}
//##########################################################################################################################//


void kiwiSetFreqMode(uint32_t freqHz, uint8_t mode) {
  char cmd[96];


  displayFREQ(FREQ);

  tft.setTextSize(1);
  tft.setTextColor(TFT_GREEN);

  float freqKHz = freqHz / 1000.0f;

  switch (mode) {

    case AM:
      snprintf(cmd, sizeof(cmd), "SET mod=amn low_cut=-4900 high_cut=4900 freq=%.ld", FREQ_TO_KHZ);
      break;

    case USB:
      snprintf(cmd, sizeof(cmd), "SET mod=usb low_cut=300 high_cut=2700 freq=%.3f", freqKHz);
      break;

    case LSB:
      snprintf(cmd, sizeof(cmd), "SET mod=lsb low_cut=-2700 high_cut=-300 freq=%.3f", freqKHz);
      break;

    case SYNC:
      snprintf(cmd, sizeof(cmd), "SET mod=sam low_cut=-4900 high_cut=4900 freq=%.3f", freqKHz);
      break;

    case CW:
      snprintf(cmd, sizeof(cmd), "SET mod=cw low_cut=300 high_cut=700 freq=%.3f", freqKHz);
      break;

    case NBFM:
      snprintf(cmd, sizeof(cmd), "SET mod=nbfm low_cut=-6000 high_cut=6000 freq=%.3f",
               freqKHz);
      break;

    default:
      return;
  }

  if (displayDebugInfo)
    Serial.printf("Sent: %s", cmd);

  kiwiSend(cmd);


  if (wfOpened) {
    if (showKiwiWF)
      drawWFScale(FREQ);
    char ts[32];
    snprintf(ts, sizeof(ts), "SET zoom=5 cf=%ld", FREQ_TO_KHZ);  // center freq
    kiwiSendWF(ts);
  }



  blocked = millis();
}


//##########################################################################################################################//
bool kiwiSessionInit() {
  tft.fillScreen(TFT_BLACK);
  loadKiwiList();
  draw12Buttons(TFT_BTNCTR, TFT_BTNBDR);
  labelKiwiButtons();
  if (modType == WBFM)
    FREQ = lastAMFREQ;

  int result = getKiwiServer();
  if (!result)  // Back
    return false;

  else if (result == 1)  //  WF enabled
    getKiwiServer();

  tft.fillScreen(TFT_BLACK);
  draw16Buttons();
  labelKiwiParameters();
  sentSNDParams = false;
  return true;
}


//##########################################################################################################################//

void labelKiwiButtons() {

  struct Button {
    const int x;
    const int y;
    const char* label;
  };


  const Button buttons[] PROGMEM = {
    { 20, 145, kiwi[0].comment },  // 21

    { 105, 145, kiwi[1].comment },  // 22

    { 185, 145, kiwi[2].comment },  // 23

    { 265, 145, kiwi[3].comment },  // 24

    { 20, 202, kiwi[4].comment },  // 31

    { 105, 202, kiwi[5].comment },  // 32

    { 185, 202, kiwi[6].comment },  // 33

    { 265, 202, kiwi[7].comment },  // 34

    { 185, 260, kiwi[8].comment },  // 43

    { 265, 260, kiwi[9].comment },  // 44

  };

  tft.setTextSize(1);

  tft.setCursor(0, 0);
  tft.println("Select a KiwiSDR server.\n\nTo modify server list, edit the file kiwisdr.url.");


  for (int i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
    tft.setCursor(buttons[i].x, buttons[i].y);
    tft.print(buttons[i].label);
  }


  tft.setTextColor(TFT_RED);
  tft.setCursor(105, 250);
  tft.print("Enable");
  tft.setCursor(105, 270);
  tft.print("Waterfall");
  tft.setTextColor(TFT_GREEN);
  tft.setCursor(25, 260);
  tft.print("Back");
}

//##########################################################################################################################//

void labelKiwiParameters() {

  struct Button {
    const int x;
    const int y;
    const char* label;
  };


  const Button buttons[] PROGMEM = {

    { 185, 80, "SYNC" },  // 13

    { 265, 80, "NBFM" },  // 14

    { 20, 133, "AM" },  // 21

    { 100, 133, "LSB" },  // 22

    { 185, 133, "USB" },  // 235

    { 270, 133, "CW" },  // 24

    { 15, 197, "10Hz" },  // 31

    { 92, 197, "100Hz" },  // 32

    { 185, 197, "1K" },  // 33

    { 270, 197, "5K" },  // 34

    { 97, 250, "10K" },  // 42

    { 180, 250, "100K" },  // 43

    { 270, 250, "1M" },  // 44

  };


  tft.setTextColor(TFT_YELLOW);
  tft.fillRect(450, 5, 15, 10, TFT_BLACK);
  tft.setCursor(450, 5);
  tft.printf("%d", dacDelay);

  tft.setTextSize(2);

  tft.setCursor(20, 80);
  tft.print("DLY-");  // 11
  tft.setCursor(100, 80);
  tft.print("DLY+");





  tft.setTextColor(TFT_GREEN);

  for (int i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
    tft.setCursor(buttons[i].x, buttons[i].y);
    tft.print(buttons[i].label);
  }


  tft.setCursor(20, 250);
  tft.print("Back");

  tft.setTextSize(1);
}

//##########################################################################################################################//

bool loadKiwiList() {
  File f = LittleFS.open("/kiwisdr.url", "r");
  if (!f) return false;

  kiwiCount = 0;

  while (f.available() && kiwiCount < 10) {

    String line = f.readStringUntil('\n');
    line.trim();

    if (!line.length()) continue;

    int comma = line.indexOf(',');
    if (comma < 0) continue;

    String addr = line.substring(0, comma);
    String comment = line.substring(comma + 1);

    int colon = addr.indexOf(':');

    if (colon >= 0) {  // we have port in the URL
      strncpy(kiwi[kiwiCount].host,
              addr.substring(0, colon).c_str(),
              sizeof(kiwi[kiwiCount].host) - 1);

      kiwi[kiwiCount].port =
        addr.substring(colon + 1).toInt();
    } else {
      strncpy(kiwi[kiwiCount].host,
              addr.c_str(),
              sizeof(kiwi[kiwiCount].host) - 1);

      kiwi[kiwiCount].port = 8073;
    }


    comment.trim();

    if (comment.endsWith(","))
      comment.remove(comment.length() - 1);

    strncpy(kiwi[kiwiCount].comment,
            comment.c_str(),
            11);

    kiwi[kiwiCount].comment[11] = 0;


    strncpy(kiwi[kiwiCount].comment, comment.c_str(),
            sizeof(kiwi[kiwiCount].comment) - 1);

    kiwiCount++;
  }

  f.close();
  return true;
}

//##########################################################################################################################//

uint8_t getKiwiServer() {

  tDoublePress();

  get_Touch();

  int buttonID = getButtonID();

  if (!buttonID)
    return false;  // outside of area

  switch (buttonID) {

    case 21:
      url = kiwi[0].host;
      kiwiPort = kiwi[0].port;
      break;
    case 22:
      url = kiwi[1].host;
      kiwiPort = kiwi[1].port;
      break;
    case 23:
      url = kiwi[2].host;
      kiwiPort = kiwi[2].port;
      break;
    case 24:
      url = kiwi[3].host;
      kiwiPort = kiwi[3].port;
      break;
    case 31:
      url = kiwi[4].host;
      kiwiPort = kiwi[4].port;
      break;
    case 32:
      url = kiwi[5].host;
      kiwiPort = kiwi[5].port;
      break;
    case 33:
      url = kiwi[6].host;
      kiwiPort = kiwi[6].port;
      break;
    case 34:
      url = kiwi[7].host;
      kiwiPort = kiwi[7].port;
      break;
    case 41:
      return 0;
      break;
    case 42:
      showKiwiWF = true;
      tft.setCursor(0, 50);
      tft.print("Experimental waterfall enabled for this session.");
      return 1;
      break;
    case 43:
      url = kiwi[8].host;
      kiwiPort = kiwi[8].port;
      break;
    case 44:
      url = kiwi[9].host;
      kiwiPort = kiwi[9].port;
      break;
  }

  tRel();
  tx = 0;
  ty = 0;
  return 2;
}


//##########################################################################################################################//
void kiwiExit() {

  wsSND.disconnect();
  wsWF.disconnect();
  SNDOpened = false;
  wfOpened = false;
  clearMessage = false;
  sentMarker = false;
  sentRxSetup = false;
  rxChan = -1;
  disconnects = 0;
  drawingWF = false;
  sentSNDParams = false;
  wfY = 65;
  wfX = 0;
  tx = 0;
  ty = 0;
  pressed = false;
  STEP = OLDSTEP;
  FREQ = FREQ_OLD;
  FREQ_OLD -= 1;  // trigger update
  modType = lastModType;
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  loadSi4735parameters();
  tft.setTextSize(2);
  tft.setTextColor(textColor);
  si4735.setAudioMute(audioMuted);
  tRel();
  tft.fillScreen(TFT_BLACK);
  rebuildMainScreen(false);
  drawFrame();
  return;
}

//##########################################################################################################################//


// z    total   DDC     CICF    zm1 decim   bw kHz  ~bw/bin
//      decim   decim   decim   r   1<<r
// ---------------------------------------------------------
// 0    0       0       bypass  0   0       30000   29.3 kHz
// 1    0       0       bypass  0   0       15000   14.6
// 2    4       2       2       1   2       7500    7.3
// 3    8       4       2       2   4       3750    3.7
// 4    16      8       2       3   8       1875    1.8 kHz
// 5    32      16      2       4   16      937.5   916 Hz
// 6    64      32      2       5   32      468.8   458
// 7    128     64      2       6   64      234.4   229
// 8    256     128     2       7   128     117.2   114
// 9    512     256     2       8   256     58.6    57
// 10   1024    512     2       9   512     29.3    29
// 11   2048    1024    2       10  1024    14.6    14 Hz



//##########################################################################################################################//
void processWFpacket(uint8_t* payload) {

  if (wfLineReady == true)
    return;
  // skip 16-byte header
  for (int x = 0; x < 320; x++) {

    int bin = 16 + (x * 1024 / 320);

    transferBuffer[x] = payload[bin];
  }

  wfLineReady = true;
}


//##########################################################################################################################//



void waterfallTask() {
  if (!wfLineReady) return;

  drawingWF = true;



  static uint8_t mn = 255;
  static uint8_t mx = 0;

  for (int i = 0; i < 320; i++) {
    if (transferBuffer[i] < mn) mn = transferBuffer[i];
    if (transferBuffer[i] > mx) mx = transferBuffer[i];
  }





  uint32_t t = micros() + 5000;

  while ((int32_t)(micros() - t) < 0) {

    int v = transferBuffer[wfX];


    // We map 0..-200 dBm to (u1_t) 255..55
    // If we map it the reverse way, (u1_t) 0..255 => 0..-255 dBm (which is more natural), then the
    // noise in the bottom bits due to the ADPCM compression will effect the high-order dBm bits
    // which is bad.

    // need -120 to - roughly 40dBm  = 135 - 215


    v -= 128;
    v *= 3;

    v = constrain(v, 0, 255);


    uint16_t color = waterfall565(v);


    tft.drawPixel(wfX, wfY, color);

    wfX++;

    if (wfX >= 320) {

      wfX = 0;
      wfY++;

      wfLineReady = false;

      if (wfY >= 135)
        wfY = 65;

      break;
    }
  }

  drawingWF = false;
}



//##########################################################################################################################//
uint16_t waterfall565(uint8_t v) {
  uint8_t r, g, b;

  if (v < 64) {
    // black -> blue
    r = 0;
    g = 0;
    b = v * 4;
  } else if (v < 128) {
    // blue -> green
    r = 0;
    g = (v - 64) * 4;
    b = 255 - (v - 64) * 4;
  } else if (v < 192) {
    // green -> yellow
    r = (v - 128) * 4;
    g = 255;
    b = 0;
  } else {
    // yellow -> red
    r = 255;
    g = 255 - (v - 192) * 4;
    b = 0;
  }

  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}


//##########################################################################################################################//

struct WFState {
  int previousValue;
  int index;
};

static WFState wfState;

void wfDecoderReset() {
  wfState.previousValue = 0;
  wfState.index = 0;
}

uint8_t decodeWFNibble(uint8_t nibble) {
  int step = STEP_TABLE[wfState.index];

  int diff = step >> 3;
  if (nibble & 1) diff += step >> 2;
  if (nibble & 2) diff += step >> 1;
  if (nibble & 4) diff += step;
  if (nibble & 8) diff = -diff;

  wfState.previousValue += diff;

  if (wfState.previousValue < 0)
    wfState.previousValue = 0;
  else if (wfState.previousValue > 255)
    wfState.previousValue = 255;

  wfState.index += INDEX_TABLE[nibble];

  if (wfState.index < 0)
    wfState.index = 0;
  else if (wfState.index > 88)
    wfState.index = 88;

  return (uint8_t)wfState.previousValue;
}


//##########################################################################################################################//
void drawWFScale(uint32_t centerFreq)  // centerFreq in Hz
{
  const int yTick = 60;
  const int yText = 50;


  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // clear area
  tft.fillRect(0, 40, 360, 125, TFT_BLACK);

  float leftFreq = centerFreq - 468750.0f;
  float rightFreq = centerFreq + 468750.0f;

  // first 100 kHz marker
  uint32_t f = ((uint32_t)(leftFreq + 99999) / 100000) * 100000;

  while (f <= rightFreq) {

    int x = (int)((f - leftFreq) * 320.0f / 937500.0f);

    if (x >= 0 && x < 320) {

      // major tick
      tft.drawFastVLine(x, yTick, 5, TFT_WHITE);

      char s[8];
      snprintf(s, sizeof(s), "%lu", f / 1000);  // kHz

      int w = strlen(s) * 6;
      tft.setCursor(x - w / 2, yText);
      if (x < 300)
        tft.print(s);
    }

    f += 100000;
  }

  // center marker
  tft.drawFastVLine(159, yTick, 6, TFT_RED);
}


//##########################################################################################################################//

bool setKiwiParameters() {

  get_Touch();

  column = 1 + (tx / HorSpacing);
  row = 1 + ((ty - 35) / vTouchSpacing);
  int buttonID = row * 10 + column;  // calculate separate since getButtonID does not cover 11-14



  if (buttonID < 11)
    return false;  // outside of area

  switch (buttonID) {


    case 11:
      dacDelay--;
      preferences.putChar("kiwi", dacDelay);
      tft.setTextColor(TFT_YELLOW);
      tft.fillRect(450, 5, 15, 10, TFT_BLACK);
      tft.setCursor(450, 5);
      tft.printf("%d", dacDelay);
      tft.setTextColor(TFT_GREEN);
      break;
    case 12:
      dacDelay++;
      preferences.putChar("kiwi", dacDelay);
      tft.setTextColor(TFT_YELLOW);
      tft.fillRect(450, 5, 15, 10, TFT_BLACK);
      tft.setCursor(450, 5);
      tft.printf("%d", dacDelay);
      tft.setTextColor(TFT_GREEN);
      break;

    case 13:
      modType = SYNC;
      STEP = 1000;
      kiwiSetFreqMode(FREQ, modType);
      break;
    case 14:
      modType = NBFM;
      STEP = 5000;
      kiwiSetFreqMode(FREQ, modType);
      break;

    case 21:
      modType = AM;
      STEP = 5000;
      kiwiSetFreqMode(FREQ, modType);
      break;
    case 22:
      modType = LSB;
      STEP = 1000;
      kiwiSetFreqMode(FREQ, modType);
      break;
    case 23:
      modType = USB;
      STEP = 1000;
      kiwiSetFreqMode(FREQ, modType);
      break;
    case 24:
      modType = CW;
      STEP = 100;
      kiwiSetFreqMode(FREQ, modType);
      break;
    case 31:
      STEP = 10;
      break;
    case 32:
      STEP = 100;
      break;
    case 33:
      STEP = 1000;
      break;
    case 34:
      STEP = 5000;
      break;
    case 41:
      kiwiExit();
      return false;
      break;
    case 42:
      STEP = 10000;
      break;
    case 43:
      STEP = 100000;
      break;
    case 44:
      STEP = 1000000;
      break;
  }

  tRel();
  return true;
}
