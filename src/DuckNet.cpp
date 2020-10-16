#include "include/DuckNet.h"

DuckNet* DuckNet::instance = NULL;

DuckNet::DuckNet() { duckRadio = DuckRadio::getInstance(); }
DuckNet* DuckNet::getInstance() {
  return (instance == NULL) ? new DuckNet : instance;
}

#ifndef CDPCFG_WIFI_NONE
IPAddress apIP(CDPCFG_AP_IP1, CDPCFG_AP_IP2, CDPCFG_AP_IP3, CDPCFG_AP_IP4);
AsyncWebServer webServer(CDPCFG_WEB_PORT);
DNSServer DuckNet::dnsServer;

const char* DuckNet::DNS = "duck";
const byte DuckNet::DNS_PORT = 53;

// Username and password for /update
const char* http_username = CDPCFG_UPDATE_USERNAME;
const char* http_password = CDPCFG_UPDATE_PASSWORD;

bool restartRequired = false;
size_t content_len;

void DuckNet::setDeviceId(std::vector<byte> deviceId) {
  this->deviceId.insert(this->deviceId.end(), deviceId.begin(), deviceId.end());
}

void DuckNet::setupWebServer(bool createCaptivePortal, String html) {

  if (txPacket == NULL) {
    txPacket = new DuckPacket(deviceId);
  }

  if (html == "") {
    Serial.println("[DuckNet] Setting up Web Server with default main page");
    portal = MAIN_page;
  } else {
    Serial.println("[DuckNet] Setting up Web Server with custom main page");
    portal = html;
  }
  webServer.onNotFound([&](AsyncWebServerRequest* request) {
    request->send(200, "text/html", portal);
  });

  webServer.on("/", HTTP_GET, [&](AsyncWebServerRequest* request) {
    request->send(200, "text/html", portal);
  });

  // Update Firmware OTA
  webServer.on("/update", HTTP_GET, [&](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password))
      return request->requestAuthentication();

    AsyncWebServerResponse* response =
        request->beginResponse(200, "text/html", update_page);

    request->send(response);
  });

  webServer.on(
      "/update", HTTP_POST,
      [&](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse(
            (Update.hasError()) ? 500 : 200, "text/plain",
            (Update.hasError()) ? "FAIL" : "OK");
        response->addHeader("Connection", "close");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
        restartRequired = true;
      },
      [&](AsyncWebServerRequest* request, String filename, size_t index,
          uint8_t* data, size_t len, bool final) {
        if (!index) {

          duckRadio->standBy();
          Serial.println("Pause Lora");
          Serial.println("startint OTA update");

          content_len = request->contentLength();

          int cmd = (filename.indexOf("spiffs") > -1) ? U_SPIFFS : U_FLASH;
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {

            Update.printError(Serial);
          }
        }

        if (Update.write(data, len) != len) {
          Update.printError(Serial);
          duckRadio->startReceive();
        }

        if (final) {
          if (Update.end(true)) {
            ESP.restart();
            esp_task_wdt_init(1, true);
            esp_task_wdt_add(NULL);
            while (true)
              ;
          }
        }
      });

  // Captive Portal form submission
  webServer.on("/formSubmit", HTTP_POST, [&](AsyncWebServerRequest* request) {
    Serial.println("Submitting Form");

    int err = DUCK_ERR_NONE;

    int paramsNumber = request->params();
    String val = "";

    for (int i = 0; i < paramsNumber; i++) {
      AsyncWebParameter* p = request->getParam(i);
      Serial.printf("%s: %s", p->name().c_str(), p->value().c_str());
      Serial.println();

      val = val + p->value().c_str() + "*";
    }

    std::vector<byte> data;
    data.insert(data.end(), val.begin(), val.end());
    txPacket->buildPacketBuffer(topics::status, data );
    err = duckRadio->sendData(txPacket->getCdpPacketBuffer());
    
    switch (err) {
      case DUCK_ERR_NONE:
        request->send(200, "text/html", portal);
        break;
      case DUCKLORA_ERR_MSG_TOO_LARGE:
        request->send(413, "text/html", "Message payload too big!");
        break;
      case DUCKLORA_ERR_HANDLE_PACKET:
        request->send(400, "text/html", "BadRequest");
        break;
      default:
        request->send(500, "text/html", "Oops! Unknown error.");
        break;
    }
  });

  webServer.on("/id", HTTP_GET, [&](AsyncWebServerRequest* request) {
    std::string id(deviceId.begin(), deviceId.end());
    request->send(200, "text/html", id.c_str());
  });

  webServer.on("/restart", HTTP_GET, [&](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", "Restarting...");
    delay(1000);
    duckesp::restartDuck();
  });

  webServer.on("/mac", HTTP_GET, [&](AsyncWebServerRequest* request) {
    String mac = duckesp::getDuckMacAddress(true);
    request->send(200, "text/html", mac);
  });

  webServer.on("/wifi", HTTP_GET, [&](AsyncWebServerRequest* request) {
    AsyncResponseStream* response = request->beginResponseStream("text/html");
    response->print("<!DOCTYPE html><html><head><title>Update Wifi "
                    "Credentials</title></head><body>");
    response->print("<p>Use this page to update your Wifi credentials</p>");

    response->print("<form action='/changeSSID' method='post'>");

    response->print("<label for='ssid'>SSID:</label><br>");
    response->print(
        "<input name='ssid' type='text' placeholder='SSID' /><br><br>");

    response->print("<label for='pass'>Password:</label><br>");
    response->print(
        "<input name='pass' type='text' placeholder='Password' /><br><br>");

    response->print("<input type='submit' value='Submit' />");

    response->print("</form>");

    response->print("</body></html>");
    request->send(response);
  });

  webServer.on("/changeSSID", HTTP_POST, [&](AsyncWebServerRequest* request) {
    int paramsNumber = request->params();
    String val = "";
    String ssid = "";
    String password = "";

    for (int i = 0; i < paramsNumber; i++) {
      AsyncWebParameter* p = request->getParam(i);

      String name = String(p->name());
      String value = String(p->value());

      if (name == "ssid") {
        ssid = String(p->value());
      } else if (name == "pass") {
        password = String(p->value());
      }
    }

    if (ssid != "" && password != "") {
      setupInternet(ssid, password);
      request->send(200, "text/plain", "Success");
    } else {
      request->send(500, "text/plain", "There was an error");
    }
  });

  webServer.begin();
}

int DuckNet::setupWifiAp(const char* accessPoint) {

  bool success = true;

  success = WiFi.mode(WIFI_AP);
  if (!success) {
    return DUCKWIFI_ERR_AP_CONFIG;
  }

  success = WiFi.softAP(accessPoint);
  if (!success) {
    return DUCKWIFI_ERR_AP_CONFIG;
  }
  delay(200); // wait for 200ms for the access point to start before configuring

  success = WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  if (!success) {
    return DUCKWIFI_ERR_AP_CONFIG;
  }

  Serial.println("[DuckNet] Created Wifi Access Point");
  return DUCK_ERR_NONE;
}

int DuckNet::setupDns() {
  dnsServer.start(DNS_PORT, "*", apIP);

  if (!MDNS.begin(DNS)) {
    Serial.println("[DuckNet] Error setting up MDNS responder!");
    return DUCKDNS_ERR_STARTING;
  }

  Serial.println("[DuckNet] Created local DNS");
  MDNS.addService("http", "tcp", CDPCFG_WEB_PORT);

  return DUCK_ERR_NONE;
}

int DuckNet::setupInternet(String ssid, String password) {
  this->ssid = ssid;
  this->password = password;
  // Serial.println("[DuckNet] connecting to ssid: " + ssid);

  if (ssid == "" || password == "" || !ssidAvailable(ssid)) {
    return DUCK_ERR_SETUP;
  }
  // Connect to Access Point
  WiFi.begin(ssid.c_str(), password.c_str());

  // TODO: we should probably simply fail here and let the app decide what to do
  // Continuous retry could deplete the battery
  while (WiFi.status() != WL_CONNECTED) {
    duckutils::getTimer().tick(); // Advance timer to reboot after awhile
  }

  // Connected to Access Point
  Serial.println("[DuckNet] DUCK CONNECTED TO INTERNET");

  return DUCK_ERR_NONE;
}

bool DuckNet::ssidAvailable(String val) {

  int n = WiFi.scanNetworks();
  Serial.print("[DuckNet] scan done. ");
  if (n == 0 || ssid == "") {
    Serial.print("networks found: ");
    Serial.println(n);
  } else {
    Serial.print("networks found: ");
    Serial.println(n);
    if (val == "") {
      val = ssid;
    }
    for (int i = 0; i < n; ++i) {
      if (WiFi.SSID(i) == val) {
        Serial.println("[DuckNet] given ssid is available!");
        return true;
      }
      delay(AP_SCAN_INTERVAL_MS);
    }
  }
  Serial.println("[DuckNet] no ssid available");

  return false;
}

void DuckNet::setSsid(String val) { ssid = val; }

void DuckNet::setPassword(String val) { password = val; }

String DuckNet::getSsid() { return ssid; }

String DuckNet::getPassword() { return password; }

#endif