//##########################################################################################################################//
// Internet tool collection
//##########################################################################################################################//


void connectWIFI() {

  int ctr = 0;
  tft.fillScreen(TFT_BLACK);
  WiFi.begin(ssid.c_str(), password.c_str());
  tft.setCursor(5, 5);
  tft.print(F("Connecting to AP\n"));
  while (WiFi.status() != WL_CONNECTED) {
    ctr++;
    if (ctr == 20)
      ESP.restart();
    tft.print(F("."));
    delay(500);
  }
  tft.println("connected.");
}

//##########################################################################################################################//


const char *makeEibiFilename(struct tm *timeinfo) {  //changes every 6 months

  uint16_t month = timeinfo->tm_mon + 1;  //  add 1
  uint16_t year = timeinfo->tm_year + 1900;


  char letter;
  if (month == 11 || month == 12 || month <= 3) {
    letter = 'b';
  } else {
    letter = 'a';
  }

  // Last two digits of year
  int yy = year % 100;

  snprintf(urlbuffer, sizeof(urlbuffer), "https://www.eibispace.de/dx/sked-%c%02d.csv", letter, yy);

  tft.print("\n\n");
  tft.print(urlbuffer);
  return urlbuffer;
}

//##########################################################################################################################//



void downloadFile(bool fill) {  // downloads a jpg or png file

  if (fill)
    tft.fillRect(0, 0, tft.width(), tft.height(), TFT_BLACK);

  if (!LittleFS.begin()) {
    tft.printf("\n\nLittleFS Mount Failed");
    LittleFS.format();
    ESP.restart();
  } else
    tft.printf("\n\nLittleFS mounted\n\nDownloading...");


  File file;

  if (downloadSelector < 6) {
    file = LittleFS.open("/image.img", FILE_WRITE);
  }

  if (downloadSelector == 6) {

    file = LittleFS.open("/eibi.lst", FILE_WRITE);  // EiBi station list must not end with .csv (no automatic load)
  }

  if (!file) {
    tft.print(F("Failed to open file for writing\n"));
    return;
  }

  WiFiClientSecure client;
  HTTPClient http;
  client.setInsecure();  // Do not use HTTPS certificate

  switch (downloadSelector) {  // use the predefined URL's
    case 0:
      break;
    case 1:
      http.begin(client, host1);
      break;
    case 2:
      http.begin(client, host2);
      break;
    case 3:
      http.begin(client, host3);
      break;
    case 4:
      http.begin(client, host4);
      break;
    case 5:
      http.begin(client, host5);
      break;
    case 6:
      {
        const char *filename = makeEibiFilename(&timeinfo);  // changes 2x per year
        http.begin(client, filename);
      }
      break;
  }

  int16_t httpCode = http.GET();

  Serial.printf("HTTP code %d\n", httpCode);
  if (httpCode == HTTP_CODE_OK) {
    http.writeToStream(&file);
    tft.println("\nDownload successful");
  } else {
    tft.println("Download failed");
  }

  file.close();
  http.end();  // Close connection
  delay(1000);
}


//##########################################################################################################################//


void drawIBtns() {

  tft.fillRect(3, 119, 334, 58, TFT_BLACK);  // overwrite row 3 from mainscreen
  draw12Buttons(TFT_BLUE, TFT_NAVY);         // draw new buttons
  struct Button {
    const int x;
    const int y;
    const char *label;
  };

  const Button buttons[] PROGMEM = {

    { 20, 133, "Get" },
    { 20, 153, "UTC" },
    { 100, 133, "" },
    { 100, 153, "" },
    { 188, 133, "WiFi" },
    { 188, 153, "Interf." },
    { 270, 133, "Flight" },
    { 270, 153, "Rad." },
    { 20, 190, "Downl." },
    { 20, 210, "EiBi" },
    { 100, 190, "SW" },
    { 100, 210, "Fade" },
    { 190, 200, "Earth" },
    { 265, 248, "NOAA" },
    { 273, 200, "Sun" },
    { 270, 268, "Alerts" },
    { 190, 248, " " },
    { 190, 268, "" },
    { 100, 248, "Sat" },
    { 100, 268, "Image" },
    { 20, 248, "Fore" },
    { 20, 268, "cast" }
  };

  if (altStyle)
    etft.setTextColor(textColor);
  else
    etft.setTextColor(TFT_GREEN);

  etft.setTTFFont(Arial_14);


  for (int i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
    etft.setCursor(buttons[i].x, buttons[i].y);
    etft.print(buttons[i].label);
  }
}

//##########################################################################################################################//

void readIBtns() {

  tDoublePress();

  preferences.putBool("fB", true);  // Does not go back to main menu, uses fast restart instead. Image displays eats up too much memory and does not reliably free it
                                    //, so better reboot after leaving
  int buttonID = getButtonID();

  if (buttonID >= 31)
    pressSound = false;  // avoid squeaking sound while rebooting

  switch (buttonID) {
    case 21:
      tft.setCursor(10, 80);
      tft.print(F("Connecting to pool.ntp.org."));
      getTime();
      break;
    case 22:
      break;
    case 23:
      webIF();
      break;
    case 24:
      showFlights();
      break;
    case 31:
      downloadSelector = 6;  // EiBi station list
      connectWIFI();
      while (!getLocalTime(&timeinfo))
        ;  //need month and year to selecct Eibi file
      downloadFile(true);
      WiFi.disconnect();
      preferences.putBool("fB", true);  // set fastboot
      ESP.restart();
      break;
    case 32:  // SW fade map
      downloadSelector = 1;
      swappedJPEG = false;
      xShift = -40;
      JPEGWrapper(15);
      break;
    case 33:  // globe view
      downloadSelector = 2;
      swappedJPEG = true;
      xShift = +60;
      yShift = -10;
      JPEGWrapper(15);
      break;
    case 34:  // sun image
      downloadSelector = 3;
      swappedJPEG = true;
      yShift = -70;
      JPEGWrapper(15);
      break;
    case 41:  // propagation forecast
      reportSelector = 0;
      reportWrapper(30);
      break;
    case 42:
      downloadSelector = 4;  // Central America sat image
      swappedJPEG = true;
      xShift = -420;
      yShift = -620;
      JPEGWrapper(15);
      break;
    case 43:
      break;
    case 44:  // NOAA spaceweathr alerts
      reportSelector = 1;
      reportWrapper(30);
      break;
  }
  tx = ty = pressed = 0;
  return;
  tRel();
}

//##########################################################################################################################//

void displayJpeg() {
  tft.fillRect(0, 0, DISP_WIDTH, DISP_HEIGHT, TFT_BLACK);

  drawJpeg("/image.img", 0, 0);  // All image files are downloaded under filename image.img
}

//##########################################################################################################################//

void drawJpeg(const char *filename, int xpos, int ypos) {
  File jpegFile = LittleFS.open(filename, "r");  // Declare and openfile
  if (!jpegFile) {
    Serial_println("Failed to open jpeg file\n");
    return;
  }
  JpegDec.decodeFsFile(filename);
  renderJPEG(xpos, ypos);
  jpegFile.close();
}
//##########################################################################################################################//


void renderJPEG(int xpos, int ypos) {
  uint16_t *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;
  uint32_t min_w = min((uint32_t)mcu_w, max_x % mcu_w);
  uint32_t min_h = min((uint32_t)mcu_h, max_y % mcu_h);
  bool isSwapped = swappedJPEG;

  while (isSwapped ? JpegDec.readSwappedBytes() : JpegDec.read()) {
    pImg = JpegDec.pImage;
    uint16_t mcu_x = JpegDec.MCUx;
    uint16_t mcu_y = JpegDec.MCUy;
    uint16_t mcu_pix_x = mcu_x * mcu_w;
    uint16_t mcu_pix_y = mcu_y * mcu_h;
    uint16_t mcu_w_curr = (mcu_pix_x + mcu_w <= max_x) ? mcu_w : min_w;
    uint16_t mcu_h_curr = (mcu_pix_y + mcu_h <= max_y) ? mcu_h : min_h;
    {
      tft.pushImage(mcu_pix_x + xpos + xShift, mcu_pix_y + ypos + yShift, mcu_w_curr, mcu_h_curr, pImg);
    }
  }
}

//##########################################################################################################################//

void JPEGWrapper(int cycle) {  // JPG wrapper, cycle = reload time in minutes

  while (true) {
    connectWIFI();
    downloadFile(false);
    WiFi.disconnect();
    displayJpeg();
    for (long i = 0; i < cycle * 600; i++) {
      get_Touch();
      if (pressed)
        ESP.restart();
      delay(100);
    }
  }
}
//##########################################################################################################################//

void reportWrapper(int cycle) {

  encLockedtoSynth = false;
  static long offset = 0, oldOffset = 0;

  while (true) {
    getReport();
    displayReport(offset);
    for (long i = 0; i < cycle * 600; i++) {
      if (clw)  // change page
        offset += 400;
      if (cclw)
        offset -= 400;
      if (offset < 0)
        offset = 0;

      clw = false;
      cclw = false;

      if (oldOffset != offset) {
        displayReport(offset);
        oldOffset = offset;
      }
      get_Touch();
      if (pressed)
        ESP.restart();
      delay(100);
    }
  }
}

//##########################################################################################################################//


void getReport(void) {  //writes JSON or text page to file. Takes looooong....

  connectWIFI();
  tft.println("Preparing report...please be patient");
  WiFiClientSecure client;
  HTTPClient http;
  client.setInsecure();  //do not use https certificate

  if (reportSelector == 0) {  //NOAA forecast uses text file
    client.connect(fr, 443);
    http.begin(client, fr);
  }

  if (reportSelector == 1) {
    client.connect(al, 443);  // NOAA alert uses JSON format
    http.begin(client, al);
  }


  uint16_t httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
    tft.println("Connection error");
  if (!LittleFS.begin()) {
    tft.printf("\n\nLittleFS Mount Failed");
    LittleFS.format();
    ESP.restart();
  } else
    tft.printf("\n\nLittleFS mounted\n\nDownloading...");
  File file = LittleFS.open("/fc.txt", "w");
  http.writeToStream(&file);
  http.end();
  WiFi.disconnect();
}


//##########################################################################################################################//

void displayReport(size_t offset) {

  char txtbuf[1500] = { 0 };  // buffer for webpage text extractor

  int bufferSize = 1000;
  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    tft.printf("LittleFS Mount Failed\n");
    return;
  }

  // Open file
  File file = LittleFS.open("/fc.txt", FILE_READ);
  if (!file) {
    tft.printf("Failed to open file:fc.txt");
    return;
  }

  // Check if offset is inside file size
  size_t fileSize = file.size();
  if (offset >= fileSize) {
    tft.printf("End");
    offset = fileSize;
  }

  // goto offset position
  file.seek(offset, SeekSet);

  //read into the buffer
  size_t bytesRead = file.readBytes(txtbuf, bufferSize);
  txtbuf[bytesRead] = '\0';
  file.close();

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(2);
  tft.setCursor(0, 0);

  uint16_t ccounter = 0;
  uint16_t cursorX;
  uint8_t cb;

  while (txtbuf[ccounter]) {
    cb = txtbuf[ccounter];
    cursorX = tft.getCursorX();

    if (cursorX > 479) {
      tft.setCursor(0, tft.getCursorY() + 23);  // new line
    }

    if (cb == '{')
      tft.setCursor(0, tft.getCursorY() + 46);  // new JSON


    // Suppress control characters
    if (!(cb == 195 || cb == '{' || cb == '}' || cb == '\\' || cb == '[')) {
      tft.write(cb);
    }

    if (reportSelector == 1) {
      if (cb == '\\') {  // remove \r\n sequence in alerts
        ccounter += 3;
        tft.write(' ');
      }
    }
    ccounter++;
  }
}

//##########################################################################################################################//



void getTime() {

  uint8_t ctr = 0;

  WiFi.begin(ssid.c_str(), password.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    if (++ctr == 20) {
      WiFi.disconnect();
      WiFi.mode(WIFI_OFF);
      return;
    }
    delay(500);
  }


  configTime(0, 0, "pool.ntp.org");
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to get time");
    tft.setCursor(10, 100);
    tft.println("Failed to get time");
    delay(1000);
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    return;
  }

  else timeSet = true;
  tft.setCursor(10, 100);
  getLocalTime(&timeinfo);
  tft.print(&timeinfo, "Time: %H:%M UTC");
  delay(1000);
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}




//##########################################################################################################################//
void prepareWebIF() {

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN);
  tft.setCursor(0, 0);


  tft.println("Starting WiFi interface...\n");
  tft.println("WiFi interface needs to disable\nseveral other functions.\n\n");

  WiFi.begin(ssid, password);
  tft.print("Connecting to: ");
  tft.print(ssid.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
  }



  tft.println("\n\nWiFi connected!\n\n");
  tft.print("Touch to continue and open ");
  tft.setTextColor(TFT_WHITE);
  tft.print(WiFi.localIP());
  tft.setTextColor(TFT_GREEN);
  tft.print("in browser.\n\n");
  tft.setTextColor(textColor);
  tPress();
  tRel();
  delay(1000);
  tft.fillScreen(TFT_BLACK);
  rebuildMainScreen(false);
}




//##########################################################################################################################//


void notifyClients() {
  String msg = String(FREQ) + "," + String(signalStrength);
  ws.textAll(msg);  // send to all connected clients
}


//##########################################################################################################################//

void webIF() {  // webIF needs >95K free heap

  prepareWebIF();

  webIFActive = true;
  spr2.deleteSprite();  // free heap from rolling RSSI trace
  sprite2Init = false;

  spr1.deleteSprite();  // free heap from miniwindow
  sprite1Init = false;


  // define routes, handlers
  aserver.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html", index_html);
  });

  aserver.on("/setfreq", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (req->hasParam("f")) {
      FREQ = req->getParam("f")->value().toInt();
      notifyClients();
      audioBufSize = 256;  // reduce audio buffer to reduce lag
    }

    req->send(200, "text/plain", "FREQ changed");
  });

  aserver.on("/setmode", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (req->hasParam("m")) {
      newMode = req->getParam("m")->value().toInt();
      modeChangeRequested = true;  // client changes modulation type
      audioBufSize = 256;
      if (streamAudio) {
        streamAudio = false;  // interrupt audio streaming
        wasStreaming = true;
      }
    }
    req->send(200, "text/plain", "123 ");
  });

  aserver.on("/stepfreq", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (req->hasParam("step")) {
      int step = req->getParam("step")->value().toInt();
      FREQ += step;
      notifyClients();
    }
    req->send(200, "text/plain", String(FREQ));
    audioBufSize = 256;  // reduce audio buffer to reduce lag
    frameIndex = 0;
  });

  aserver.on("/audioctl", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (req->hasParam("on")) {
      streamAudio = (req->getParam("on")->value() == "1");
    }
    req->send(200, "text/plain", streamAudio ? "AUDIO ON" : "AUDIO OFF");
  });




  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
      client->text("Connected");
    }
  });

  aserver.addHandler(&ws);
  aserver.begin();
}



//##########################################################################################################################//
// Flight radar
//##########################################################################################################################//

const uint16_t centerX = 160;
const uint16_t centerY = 110;
String trackedICAO = "";
bool trackingAircraft = false;
flight_info trackedFlight;


//##########################################################################################################################//
//  HTTP status
String http_status_reason(int code) {
  switch (code) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 500: return "Internal Server Error";
    default: return "Unknown";
  }
}

//##########################################################################################################################//

// fetch flight data, thank you for the inspiration: https://github.com/rzeldent/esp32-flightradar24-ttgo

bool get_flights(float latitude, float longitude,
                 float range_latitude, float range_longitude,
                 bool air, bool ground, bool gliders, bool vehicles,
                 std::list<flight_info> &flights, String &error_message) {
  const String flight_data_base_url = "http://data-cloud.flightradar24.com/zones/fcgi/feed.js";
  const String bounds = String(latitude + range_latitude / 2.0) + "," + String(latitude - range_latitude / 2.0) + "," + String(longitude - range_longitude / 2.0) + "," + String(longitude + range_longitude / 2.0);

  const String flight_data_url = flight_data_base_url + "?" + "bounds=" + bounds + "&faa=1&satellite=1&mlat=1&flarm=1&adsb=1" + "&gnd=" + String(ground) + "&air=" + String(air) + "&vehicles=" + String(vehicles) + "&estimated=1&maxage=14400" + "&gliders=" + String(gliders) + "&stats=0";

  flights.clear();
  error_message = "";

  HTTPClient client;
  log_i("Request states=%s", flight_data_url.c_str());
  if (!client.begin(flight_data_url)) {
    error_message = "Failed to start client. DNS/TCP error?";
    log_e("%s", error_message.c_str());
    return false;
  }

  const auto httpResultCode = client.GET();
  if (httpResultCode != HTTP_CODE_OK) {
    client.end();
    error_message = String(httpResultCode) + " " + (httpResultCode < 0 ? client.errorToString(httpResultCode) : http_status_reason(httpResultCode));
    log_e("HTTP error: %d (%s)", httpResultCode, error_message.c_str());
    return false;
  }

  auto response = client.getString();
  log_d("Body=%s", response.c_str());

  JsonDocument doc_flight_data;
  const auto error = deserializeJson(doc_flight_data, response);
  if (error != DeserializationError::Ok) {
    client.end();
    log_e("Deserialize. Error=%s", error.c_str());
    error_message = error.c_str();
    return false;
  }

  client.end();


  auto flight_data_root = doc_flight_data.as<JsonObject>();
  for (auto kvp : flight_data_root) {
    if (!kvp.value().is<JsonArray>()) continue;
    auto items = kvp.value().as<JsonArray>();
  if(displayDebugInfo)  
    log_i("KVP=%s", kvp.key().c_str());



    flight_info flight{
      items[0].as<const char *>(),   // ICAO address
      items[1].as<float>(),          // latitude
      items[2].as<float>(),          // longitude
      items[3].as<int>(),            // heading
      items[4].as<int>(),            // altitude
      items[5].as<int>(),            // ground speed
      items[6].as<const char *>(),   // squawk
      items[7].as<const char *>(),   // radar
      items[8].as<const char *>(),   // aircraft code
      items[9].as<const char *>(),   // registration
      items[10].as<time_t>(),        // timestamp
      items[11].as<const char *>(),  // origin airport
      items[12].as<const char *>(),  // destination airport
      items[13].as<const char *>(),  // flight number
      items[14].as<bool>(),          // on ground
      items[15].as<int>(),           // vertical speed
      items[16].as<const char *>(),  // call sign
      items[18].as<const char *>()   // airline ICAO
    };

    flights.push_back(flight);
  }

  return true;
}

//##########################################################################################################################//

void showFlights() {


  float lat = preferences.getFloat("lat", 0);
  float lon = preferences.getFloat("lon", 0);
  float displayRad = 25;         // km
  uint32_t refreshRate = 15000;  // ms
  static bool blink = false;
  connectWIFI();

  tft.fillScreen(TFT_BLACK);
  if (!lat && !lon) {
    tft.println("Flightradar needs coordinates. Set Lat/Lon in Config menu.");
    delay(2000);
    return;
  }


  loadATCFreqs();  // load frequencies from CSV
  tft.setTextSize(1);
  tft.setCursor(0, 0);
  tft.printf("Lat:%2.2f\nLon:%2.2f\n\nRad:%2.1f km", lat, lon, displayRad);

  drawRadar();
  printRadarLabels();

  std::list<flight_info> flights;
  uint32_t lastUpdate = 99999;




  while (true) {  // main loop

    uint32_t now = millis();

    if (now % 100 == 0)
      radarSquelch();

    if (now % 1000 == 0) {

      tft.setTextColor(textColor);
      tft.fillRect(0, 38, 40, 10, TFT_BLACK);
      tft.setCursor(0, 38);

      tft.printf(
        "T-%ld",
        refreshRate / 1000 - (now - lastUpdate) / 1000);

      blink = !blink;

      if (blink)
        tft.drawCircle(centerX, centerY, 1, TFT_WHITE);
      else
        tft.drawCircle(centerX, centerY, 1, TFT_RED);
    }



    tx = 0;
    ty = 0;
    pressed = get_Touch();




    //check touch is done BEFORE flights is refreshed, cause posX/posY
    // belong to what is currently on the screen.


    if (pressed) {

      Serial.printf("RADAR TOUCH x=%d y=%d\n", tx, ty);

      for (auto &f : flights) {

        if (f.posX < 0 || f.posY < 0)
          continue;

        if (abs(tx - f.posX) <= 8 && abs(ty - f.posY) <= 8) {

          trackedICAO = f.icao_address;
          trackingAircraft = true;
          trackedFlight = f;

          if (displayDebugInfo)
            Serial.printf("TRACK START: %s\n",
                          trackedICAO.c_str());

          displayFlightData(trackedFlight);
          sineTone(880, 200);

          break;
        }
      }
    }



    // Refresh radar
    
    if (now - lastUpdate > refreshRate) {

      lastUpdate = millis();

      String error;


      // Erase previous aircraft / redraw radar

      drawRadar();

      for (auto &f : flights) {

        RadarPoint pt = geoToRadarPixel(
          f.latitude, f.longitude,
          lat, lon,
          displayRad,
          110,
          centerX, centerY);

        if (pt.inside) {

          drawAircraft(  // convert to trail
            pt.x - centerX,
            pt.y - centerY,
            f.heading,
            f.call_sign.c_str(),
            f.altitude,
            true);
        }
      }


      // tracker nees a new vector so the old displayed



      std::list<flight_info> freshFlights;

      if (get_flights(
            lat, lon,
            0.5, 0.5,
            true, false, false, false,
            freshFlights,
            error)) {




        //best to using ICAO for tracker
        if (trackingAircraft) {

          bool found = false;

          for (auto &fresh : flights) {

            if (fresh.icao_address == trackedICAO) {

              trackedFlight = fresh;
              found = true;

              if (displayDebugInfo)
                Serial.printf(
                  "TRAK UPDATE: CALLSIGN=%s Alt=%d GS=%d HDG=%d\n",
                  trackedFlight.call_sign.c_str(),
                  trackedFlight.altitude,
                  trackedFlight.ground_speed,
                  trackedFlight.heading);

              break;
            }
          }

          if (found) {

            displayFlightData(trackedFlight);
          }

          else

            tft.fillRect(340, 40, 130, 140, TFT_BLACK);  // overwrite tracking window
        }

        // refressh
        flights = freshFlights;
        //draw new

        for (auto &f : flights) {

          RadarPoint pt = geoToRadarPixel(
            f.latitude, f.longitude,
            lat, lon,
            displayRad,
            110,
            centerX, centerY);

          if (pt.inside) {

            drawAircraft(
              pt.x - centerX,
              pt.y - centerY,
              f.heading,
              f.call_sign.c_str(),
              f.altitude,
              false);

            f.posX = (int)pt.x;
            f.posY = 220 - (int)pt.y;  // still a bug?

            if (displayDebugInfo)
              Serial.println(f.toString());
          }
        }
      } else {

        Serial.printf(
          "get_flights failed: %s\n",
          error.c_str());
      }
    }



    // ui and parser
    if (tft.getTouchRawZ() > 400) {

      if (!radarUIParser(
            centerX,
            centerY,
            &lastUpdate,
            &displayRad)) {

        rebuildMainScreen(false);
        return;
      }
    }
  }
}

//##########################################################################################################################//


void drawRadar() {  // circles

  tft.drawCircle(centerX, centerY, 110, TFT_GREEN);
  tft.drawCircle(centerX, centerY, 82, TFT_DARKDARKGREY);
  tft.drawCircle(centerX, centerY, 55, TFT_DARKDARKGREY);
  tft.drawCircle(centerX, centerY, 27, TFT_DARKDARKGREY);
  tft.drawCircle(centerX, centerY, 1, TFT_RED);

  tft.setCursor(centerX, centerY - 108);
  tft.print("N");


  tft.setCursor(centerX, centerY + 101);
  tft.print("S");


  tft.setCursor(centerX + 105, centerY);
  tft.print("E");

  tft.setCursor(centerX - 108, centerY);
  tft.print("W");
}

//##########################################################################################################################//


void drawAircraft(float dx, float dy, int heading,
                  const char *label, uint32_t altitude, bool drawTrail) {

  int x = centerX + (int)dx;
  int y = centerY - (int)dy;

  const float size = 5.0f;
  const float wingAngle = 150.0f * DEG_TO_RAD;

  // Heading: 0 = north, 90 = east.
  // TFT coordinates: +X = east, +Y = south.
  float rad = heading * DEG_TO_RAD;

  // Nose
  int x1 = lroundf(x + sinf(rad) * size);
  int y1 = lroundf(y - cosf(rad) * size);

  // Left wing
  int x2 = lroundf(x + sinf(rad + wingAngle) * size);
  int y2 = lroundf(y - cosf(rad + wingAngle) * size);

  // Right wing
  int x3 = lroundf(x + sinf(rad - wingAngle) * size);
  int y3 = lroundf(y - cosf(rad - wingAngle) * size);

  uint16_t color = altitudeColor(altitude);

  if (drawTrail) {
    tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_BLACK);  // erase last triange
    tft.fillCircle(x, y, 1, color);
  }

  else
    tft.fillTriangle(x1, y1, x2, y2, x3, y3, color);


  // Label
  int tbw = tft.textWidth(label);


  int labelX = x + 8;
  int labelY = y;

  int dxLabel = labelX - centerX;
  int dyLabel = labelY - centerY;

  float distLabel =
    sqrtf(dxLabel * dxLabel + dyLabel * dyLabel);

  if (distLabel + tbw / 2 > 110) {
    float scale = (110.0f - tbw / 2.0f) / distLabel;

    labelX = centerX + dxLabel * scale;
    labelY = centerY + dyLabel * scale;
  }

  tft.setCursor(labelX, labelY);

  if (drawTrail)
    tft.setTextColor(TFT_BLACK);
  else
    tft.setTextColor(TFT_WHITE);

  tft.print(label);
}

//##########################################################################################################################//


uint16_t altitudeColor(uint32_t altitude) {
  // Clamp altitude
  if (altitude > 40000) altitude = 40000;

  uint8_t r = 0, g = 0, b = 0;

  if (altitude <= 10000) {
    float t = altitude / 10000.0f;
    r = (uint8_t)(255 * (1 - t));  // 255 → 0
    g = 255;                       // stays 255
    b = 0;                         // stays 0
  } else if (altitude <= 20000) {
    float t = (altitude - 10000) / 10000.0f;
    r = (uint8_t)(255 * t);        // 0 → 255
    g = (uint8_t)(255 * (1 - t));  // 255 → 0
    b = 0;
  } else if (altitude <= 30000) {
    float t = (altitude - 20000) / 10000.0f;
    r = (uint8_t)(255 * (1 - t) + 128 * t);  // 255 → 128
    g = 0;
    b = (uint8_t)(128 * t);  // 0 → 128
  } else {
    r = 128;
    g = 0;
    b = 128;
  }

  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}


//##########################################################################################################################//



RadarPoint geoToRadarPixel(float flightLat, float flightLon,
                           float userLat, float userLon,
                           float maxRadiusKm, int circleRadiusPx,
                           int centerX, int centerY) {
  // Normalize longitude difference to ±180°
  float dLon = flightLon - userLon;
  if (dLon > 180) dLon -= 360;
  if (dLon < -180) dLon += 360;

  // Convert degree diff to km
  float dxKm = dLon * cos(userLat * DEG_TO_RAD) * 111.0f;  // east-west
  float dyKm = (flightLat - userLat) * 111.0f;             // north-south

  // Distance from center
  float distKm = sqrt(dxKm * dxKm + dyKm * dyKm);

  RadarPoint pt;
  pt.inside = (distKm <= maxRadiusKm);

  if (!pt.inside) {
    // Clip to circle edge
    dxKm *= maxRadiusKm / distKm;
    dyKm *= maxRadiusKm / distKm;
  }

  // Scale km to pix
  float scaleKmPerPixel = maxRadiusKm / circleRadiusPx;
  int dxPix = dxKm / scaleKmPerPixel;
  int dyPix = dyKm / scaleKmPerPixel;

  // Convert to screen coords
  pt.x = centerX + dxPix;  // east = right
  pt.y = centerY + dyPix;  // north = up

  return pt;
}

//##########################################################################################################################//
void printRadarLabels() {


  tft.setTextColor(TFT_GREENYELLOW);
  tft.setCursor(260, 0);
  tft.print("0-10000ft");
  tft.setTextColor(TFT_GREEN);
  tft.setCursor(260, 10);
  tft.print("10-20000ft");
  tft.setTextColor(TFT_RED);
  tft.setCursor(260, 20);
  tft.print("20-30000ft");
  tft.setTextColor(TFT_PURPLE);
  tft.setCursor(260, 30);
  tft.print("30000+ft");
  tft.setTextColor(textColor);

  tft.drawRect(0, 60, 40, 20, TFT_CYAN);  // radius
  tft.drawRect(0, 90, 40, 20, TFT_CYAN);

  tft.drawRect(0, 170, 50, 33, TFT_CYAN);  // clear
  tft.fillRect(1, 171, 48, 31, TFT_NAVY);

  tft.setCursor(10, 67);
  tft.print("Rad-");
  tft.setCursor(10, 97);
  tft.print("Rad+");

  tft.setCursor(8, 175);
  tft.print("CLEAR");
  tft.setCursor(8, 190);
  tft.print("TRAILS");

  tft.drawRect(270, 170, 50, 33, TFT_RED);
  tft.fillRect(271, 171, 48, 31, TFT_NAVY);
  tft.setCursor(282, 182);
  tft.print("EXIT");

  int rows[] = { 235, 280 };
  int cols[] = { 0, 100, 200, 300, 400 };
  int w = 60;
  int h = 40;

  for (int r = 0; r < 2; r++) {
    for (int c = 0; c < 5; c++) {
      tft.drawRoundRect(cols[c], rows[r], w, h, 3, TFT_SKYBLUE);  // draw frequency buttons
    }
  }


  tft.setTextColor(TFT_WHITE);


  // Labeled buttons

  int xPosL[] = { 8, 108, 208, 308, 408 };
  const char *keys[] = { "atcfreq1", "atcfreq2", "atcfreq3", "atcfreq4", "atcfreq5" };
  const char *labels[] = { "Ground", "Tower", "Approach", "Control", "Clearance" };

  for (int i = 0; i < 5; i++) {
    // Frequency line
    tft.setCursor(xPosL[i], 243);
    float freq = (float)preferences.getLong(keys[i], 0) / 1000.0f;
    tft.printf("%3.3f", freq);

    // Label line
    tft.setCursor(xPosL[i] - 3, 260);
    tft.print(labels[i]);
  }



  // unlabeled buttons
  int xPosU[] = { 8, 108, 208, 308, 408 };
  const char *keysU[] = { "atcfreq6", "atcfreq7", "atcfreq8", "atcfreq9", "atcfreq10" };

  for (int i = 0; i < 5; i++) {
    tft.setCursor(xPosU[i], 295);
    float freq = (float)preferences.getLong(keysU[i], 0) / 1000.0f;
    tft.printf("%3.3f", freq);
  }

  tft.setTextColor(textColor);
}

//##########################################################################################################################//

bool radarUIParser(int centerX, int centerY, uint32_t *lastUpdate, float *displayRad) {


  get_Touch();


  if (ty > 165 && ty < 205 && tx > 270) { // return
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    return false;
  }

  else if (ty > 165 && ty < 205 && tx < 50) {  // clear trails
    tft.fillCircle(centerX, centerY, 118, TFT_BLACK);
    drawRadar();
    *lastUpdate = 999999;
    tRel();
    return true;
  }



  else if (ty > 56 && ty < 85 && tx < 50) {  // decrease radius
    *displayRad /= 2;
    if (*displayRad < 3.125f)  // min
      *displayRad = 3.125f;
    tft.fillRect(0, 23, 70, 8, TFT_BLACK);
    tft.setCursor(0, 23);
    tft.setTextColor(textColor);
    tft.printf("Rad:%2.1f km", *displayRad);  // print new radius

    tft.fillCircle(centerX, centerY, 118, TFT_BLACK);
    drawRadar();
    *lastUpdate = 999999;
    return true;
  }


  else if (ty > 85 && ty < 115 && tx < 50) {  // increase radius
    *displayRad *= 2;
    if (*displayRad > 50.0f)  // max
      *displayRad = 50.0f;

    tft.fillRect(0, 23, 70, 8, TFT_BLACK);
    tft.setCursor(0, 23);
    tft.setTextColor(textColor);
    tft.printf("Rad:%2.1f km", *displayRad);  // print new radius

    tft.fillCircle(centerX, centerY, 118, TFT_BLACK);  // overwrite disk
    drawRadar();
    *lastUpdate = 999999;
    return true;
  }




  // FREQ buttons
  if (ty > 235 && ty < 280) {  // labeled buttons

    if (tx > 0 && tx < 50) {
      FREQ = preferences.getLong("atcfreq1", 0);
    }

    else if (tx > 100 && tx < 150) {
      FREQ = preferences.getLong("atcfreq2", 0);
    } else if (tx > 200 && tx < 250) {  // Button 3
      FREQ = preferences.getLong("atcfreq3", 0);
    }

    else if (tx > 300 && tx < 350) {
      FREQ = preferences.getLong("atcfreq4", 0);
    }  // Button 4

    else if (tx > 400 && tx < 450) {  // Button 5
      FREQ = preferences.getLong("atcfreq5", 0);
    }

  }

  else if (ty > 280) {  // unlabeled buttons

    if (tx > 0 && tx < 50) {
      FREQ = preferences.getLong("atcfreq6", 0);
    }

    else if (tx > 100 && tx < 150) {
      FREQ = preferences.getLong("atcfreq7", 0);
    } else if (tx > 200 && tx < 250) {
      FREQ = preferences.getLong("atcfreq8", 0);
    }

    else if (tx > 300 && tx < 350) {
      FREQ = preferences.getLong("atcfreq9", 0);
    }  // Button 4

    else if (tx > 400 && tx < 480) {
      FREQ = preferences.getLong("atcfreq10", 0);
    }
  }

  if (ty > 235 && FREQ < 150000) {  // avoid in between button errors
    FREQ *= 1000;                   //  stored was in KHz
    setFreq();
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.fillRect(350, 10, 120, 15, TFT_BLACK);
    tft.setCursor(350, 10);
    tft.printf("%ld KHz", FREQ / 1000);
    tft.setTextSize(1);
    tft.setTextColor(textColor);
  }


  if (ty > 235)
    tRel();
  return true;
}

//##########################################################################################################################//

bool loadATCFreqs() {
  File file = LittleFS.open("/atcfreqs.lst", "r");

  if (!file) {
    tft.fillScreen(TFT_BLACK);
    tft.println("Cannot open /atcfreqs.lst!");
    tft.println("\nMake sure file is on LitleFS");
    delay(2000);
    return false;
  }

  String line = file.readStringUntil('\n');
  file.close();

  int start = 0;

  for (int i = 0; i < 10; i++) {

    int comma = line.indexOf(',', start);

    if (comma < 0) {
      return false;
    }

    String token = line.substring(start, comma);
    token.trim();

    uint32_t freq = strtoul(token.c_str(), nullptr, 10);

    char key[12];
    snprintf(key, sizeof(key), "atcfreq%d", i + 1);

    preferences.putLong(key, (long)freq);

    start = comma + 1;
  }


  return true;
}

//##########################################################################################################################//
void radarSquelch() {

  getRSSIAndSNR();
  readSquelchPot(false);
  setSquelch();
  setTunerAGC(false);
}


//##########################################################################################################################//
void displayFlightData(const flight_info &f) {

  const int startX = 350;
  const int startY = 40;

  tft.fillRect(startX - 10, startY, 130, 140, TFT_DARKDARKGREY);

  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(startX, startY + 10);
  tft.print(f.call_sign);
  tft.setTextSize(1);

  tft.setCursor(startX, startY + 35);
  tft.printf("ALT: %d ft", f.altitude);

  tft.setCursor(startX, startY + 50);
  tft.printf("GS: %d kts", f.ground_speed);

  tft.setCursor(startX, startY + 65);
  tft.printf("HEADING: %d deg", f.heading);

  tft.setCursor(startX, startY + 80);
  tft.printf("ORIGIN: %s", f.iata_origin_airport.c_str());

  tft.setCursor(startX, startY + 95);
  tft.printf("DDEST.: %s", f.iata_destination_airport.c_str());

  tft.setCursor(startX, startY + 110);
  tft.printf("TYPE: %s", f.aircraft_code.c_str());

  tft.setCursor(startX, startY + 125);
  tft.printf("AIRL.: %s", f.icao_airline.c_str());
}
/*

   String icao_address;
    float latitude;
    float longitude;
    int heading;
    int altitude;
    int ground_speed;
    String squawk;
    String radar;
    String aircraft_code;
    String registration;
    time_t timestamp;
    String iata_origin_airport;
    String iata_destination_airport;
    String flight;
    bool on_ground;
    int vertical_speed;
    String call_sign;
    String icao_airline;
*/
