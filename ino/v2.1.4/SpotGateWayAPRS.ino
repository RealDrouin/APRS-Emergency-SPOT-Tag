/*
   APRS BALISE Station and Spot Satellite Messenger GlobalStar GateWay Ver 2.1.4

   add variable Spot Latitude json
   change delay Spot request api to 2.5 min
   fix message when no message from api json broadcast to APRS server.
   add variable SpotTable and SpotSymbol


     Date 2024-Jun-21
     make by VE2CUZ
*/

const String ver = "Ver 2.1.4";

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>

byte broadcastCount = 0;

////////////////////////
// FINDMESPOT.COM API //
//////////////////////////////////////////////////////////////////////////////
String jsonBuffer;
String payload = "";
String line = "";
String SpotHost = "https://api.findmespot.com";
String SpotApi = "/spot-main-web/consumer/rest-api/2.0/public/feed/";
String SpotToken = "";     //your Spot API Token
String SpotComment = "Spot GlobalStar Satellite Messenger https://maps.findmespot.com/s/26TP";  //ex: your share map link from findmespot
// EX: https://api.findmespot.com/spot-main-web/consumer/rest-api/2.0/public/feed/apikey/latest.json

/////////////////////////////////////////
// FindMeSpot GPS CONVERTION DD to DDM //
/////////////////////////////////////////////////////////////////////////////
long previousUnixTime;
long responseUnixTime;
String blat, blon;             // GPS converted to dmm.mm ready for aprs.
String lat_SouthN, lon_EastW;  // ex: -74=W° or 74°=E, 45°=N or -45°=S
String SpotMessage = "";
String SpotBattery = "";
String SpotAltitude = "";
String SpotTable = "/";   //APRS icon table
String SpotSymbol = "[";  //APRS icon selection
bool FindMeSpotBroadcast = false;

////////////////////////
// APRS STATION SETUP //
//////////////////////////////////////////////////////////////////////////////
String SpotCallSign = "VE2CUZ-10";                 //your Callsign ex: VE2CUZ-10
String CallSign = "VE2CUZ";                        //your Callsign ex: VE2CUZ
String Passcode = "";                         //your APRS Passcode passcode
String Lat = "4545.99N";                           //your static GPS location ex: 4545.99N
String Lon = "07400.80W";                          //your static GPS location ex: 07400.80W
String Table = "/";                                //APRS icon table
String Symbol = "-";                               //APRS icon selection
String Comment = "ve2cuz@gmail.com https://github.com/RealDrouin ";  //your comment
String aprsemailto = "ve2cuz@gmail.com";           //your Email Address, Broadcast every 12h
bool AprsBroadcast = false;

////////////////
// WIFI SETUP //
//////////////////////////////////////////////////////////////////////////////
String ssid = "";              //your wifi ssid
String password = "";  //your wifi password

////////////
// Millis //
//////////////////////////////////////////////////////////////////////////////
const unsigned long Aprs = 600000;  // Broadcast to APRS Server every 10min
unsigned long previousAprs = 0;

const unsigned long Spot = 150000;  // Broadcast Spot Data every 2.5min
unsigned long previousSpot = 0;

const unsigned long Led = 2000;
unsigned long previousLed = 0;

void setup() {
  Serial.begin(9600);
  pinMode(16, OUTPUT);
  digitalWrite(16, HIGH);

  WifiConnect();
}

void loop() {
  unsigned long currentMillis = millis();

  ////////////////////////////////
  // Broadcast Balise every 10m //
  ///////////////////////////////////////////////////////////////////////////////////////
  if (currentMillis - previousAprs > Aprs) {
    if (Passcode.length() > 0) {
      FindMeSpotBroadcast = false;
      AprsBroadcast = true;
      APRS();
    }
    FindMeSpotBroadcast = false;
    AprsBroadcast = false;
    previousAprs = currentMillis;
  }

  //////////////////////////
  // Send Spot every 2.5m //
  ///////////////////////////////////////////////////////////////////////////////////////
  if (currentMillis - previousSpot > Spot) {
    if (SpotToken.length() > 0) {
      FindMeSpot();
      if (responseUnixTime != previousUnixTime) {
        FindMeSpotBroadcast = true;
        AprsBroadcast = false;
        APRS();
      } else {
        Serial.println(F("No change on Spot API, BroadCast to APRS server cancelled!"));
        FindMeSpotBroadcast = false;
        AprsBroadcast = false;
      }
    }
    FindMeSpotBroadcast = false;
    AprsBroadcast = false;
    previousSpot = currentMillis;
  }

  /////////////////
  // Heart Beat //
  ///////////////////////////////////////////////////////////////////////////////////////
  if (currentMillis - previousLed > Led) {
    digitalWrite(16, LOW);
    delay(100);
    digitalWrite(16, HIGH);
    previousLed = currentMillis;
  }

  yield();
}

/////////////////////
// APRS Server API //
/////////////////////
void APRS() {
  digitalWrite(16, LOW);
  WiFiClient client;

  String readString;
  String response;

  client.setTimeout(3000);  // reduce delay executing get response

  ///////// Broadcast to APRS ////////////
  Serial.println(F("Connecting to APRS Server..."));

  if (client.connect("rotate.aprs2.net", 14580)) {

    int timeout = millis() + 5000;
    while (client.available() == 0) {
      if (timeout - millis() < 0) {
        delay(1);
        client.flush();
        client.stop();
        Serial.println(F("TimeOut to Connect to APRS Server!"));
        return;
      }
    }

    while (client.available()) {
      response = client.readStringUntil('\r');

      if (response.indexOf("aprs") > 0) {
        Serial.println(F("Connected! to APRS Server"));

        if (AprsBroadcast == true && FindMeSpotBroadcast == false) {
          client.println("user " + (CallSign) + " pass " + (Passcode.c_str()) + " vers ESP8266 1");
          Serial.println("user " + (CallSign) + " pass " + (Passcode.c_str()) + " vers ESP8266 1");
          delay(10);
        } else if (AprsBroadcast == false && FindMeSpotBroadcast == true) {
          client.println("user " + (SpotCallSign) + " pass " + (Passcode.c_str()) + " vers ESP8266 1");
          Serial.println("user " + (SpotCallSign) + " pass " + (Passcode.c_str()) + " vers ESP8266 1");
          delay(10);
        }
      }

      if (response.indexOf("verified") > 0) {
        if (AprsBroadcast == true && FindMeSpotBroadcast == false) {
          broadcastCount++;
          client.println((CallSign) + ">APRS,TCPIP:>Spot GlobalStar Satellite Messenger GateWay by VE2CUZ " + (ver.c_str()));
          Serial.println((CallSign) + ">APRS,TCPIP:>Spot GlobalStar Satellite Messenger GateWay by VE2CUZ " + (ver.c_str()));

          //  Email Notification
          if (aprsemailto.length() > 0 && broadcastCount >= 72) {
            long randNumber;
            randNumber = random(25);
            client.println((CallSign) + ">APRS::EMAIL-2  :" + (aprsemailto) + " Balise actived!{" + (randNumber));
            Serial.println((CallSign) + ">APRS::EMAIL-2  :" + (aprsemailto) + " Balise actived!{" + (randNumber));

            broadcastCount = 0;
          }
          delay(10);
          /////////////////////////////////////////////////////////////
          String aprsString = "";
          aprsString += (CallSign);
          aprsString += ">APRS,TCPIP:=";
          aprsString += (Lat.c_str());
          aprsString += (Table);
          aprsString += (Lon.c_str());
          aprsString += (Symbol);
          aprsString += (Comment);

          client.println(aprsString);
          Serial.println(aprsString);
          delay(10);
          Serial.println(F("APRS Server Updated! :-)"));
        }
        ////////////////////////////////////////////////////////////////
        else if (AprsBroadcast == false && FindMeSpotBroadcast == true) {
          if (responseUnixTime != previousUnixTime) {
            String findMeSpot = "";
            findMeSpot += (SpotCallSign);
            findMeSpot += ">APRS,TCPIP:=";
            findMeSpot += (blat.c_str());
            findMeSpot += (SpotTable);
            findMeSpot += (blon.c_str());
            findMeSpot += (SpotSymbol);
            findMeSpot += "/A=";
            findMeSpot += (SpotAltitude);

            if (SpotMessage.length() > 0) {
              findMeSpot += " Message: ";
              findMeSpot += (SpotMessage);
            }

            findMeSpot += " Battery: ";
            findMeSpot += (SpotBattery);

            client.println(findMeSpot);
            Serial.println(findMeSpot);
            delay(10);
            client.println((SpotCallSign) + ">APRS,TCPIP:>" + (SpotComment));
            Serial.println((SpotCallSign) + ">APRS,TCPIP:>" + (SpotComment));
            Serial.println(F("APRS Server Updated! :-)"));

            previousUnixTime = responseUnixTime;
          }
        }
      }
    }
  }
  Serial.println(F("Client STOP! 5sec"));
  delay(1);
  client.flush();
  client.stop();
  delay(5000);
}

void WifiConnect() {
  digitalWrite(16, LOW);
  WiFi.mode(WIFI_STA);
  delay(10);
  WiFi.hostname("Balise-APRS");
  delay(10);
  WiFi.begin(ssid.c_str(), password.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }

  Serial.println(F(""));
  Serial.print(F("Connected to "));
  Serial.println(ssid);
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());
  delay(1000);
}

void FindMeSpot() {
  digitalWrite(16, LOW);
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  HTTPClient https;

  String url = SpotHost + SpotApi + SpotToken + "/latest.json";

  Serial.print("[HTTPS] Connecting to Spot API Server...\n");
  if (https.begin(*client, url)) {  // HTTPS

    Serial.print("[HTTPS] GET...\n");
    // start connection and send HTTP header
    int httpCode = https.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {

        String payload = https.getString();
        Serial.println(payload);

        if (payload.indexOf("error") > 0) {
          return;
        }

        // Stream& input;

        DynamicJsonDocument doc(1024);

        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }

        JsonObject response_feedMessageResponse = doc["response"]["feedMessageResponse"];
        //int response_feedMessageResponse_count = response_feedMessageResponse["count"];  // 1

        JsonObject response_feedMessageResponse_feed = response_feedMessageResponse["feed"];
        //const char* response_feedMessageResponse_feed_id = response_feedMessageResponse_feed["id"];
        //const char* response_feedMessageResponse_feed_name = response_feedMessageResponse_feed["name"];
        //const char* response_feedMessageResponse_feed_description = response_feedMessageResponse_feed["description"];
        //const char* response_feedMessageResponse_feed_status = response_feedMessageResponse_feed["status"];
        //int response_feedMessageResponse_feed_usage = response_feedMessageResponse_feed["usage"];          // 0
        //int response_feedMessageResponse_feed_daysRange = response_feedMessageResponse_feed["daysRange"];  // 7
        //bool response_feedMessageResponse_feed_detailedMessageShown = response_feedMessageResponse_feed["detailedMessageShown"];
        //const char* response_feedMessageResponse_feed_type = response_feedMessageResponse_feed["type"];

        //int response_feedMessageResponse_totalCount = response_feedMessageResponse["totalCount"];        // 1
        //int response_feedMessageResponse_activityCount = response_feedMessageResponse["activityCount"];  // 0

        JsonObject response_feedMessageResponse_messages_message = response_feedMessageResponse["messages"]["message"];
        //const char* response_feedMessageResponse_messages_message_clientUnixTime = response_feedMessageResponse_messages_message["@clientUnixTime"];
        //long long response_feedMessageResponse_messages_message_id = response_feedMessageResponse_messages_message["id"];
        //const char* response_feedMessageResponse_messages_message_messengerId = response_feedMessageResponse_messages_message["messengerId"];
        //const char* response_feedMessageResponse_messages_message_messengerName = response_feedMessageResponse_messages_message["messengerName"];
        long unixTime = response_feedMessageResponse_messages_message["unixTime"];
        //const char* response_feedMessageResponse_messages_message_messageType = response_feedMessageResponse_messages_message["messageType"];
        float gpslat = response_feedMessageResponse_messages_message["latitude"];
        float gpslon = response_feedMessageResponse_messages_message["longitude"];
        //const char* response_feedMessageResponse_messages_message_modelId = response_feedMessageResponse_messages_message["modelId"];
        //const char* response_feedMessageResponse_messages_message_showCustomMsg = response_feedMessageResponse_messages_message["showCustomMsg"];
        //const char* response_feedMessageResponse_messages_message_dateTime = response_feedMessageResponse_messages_message["dateTime"];
        const char* batteryState = response_feedMessageResponse_messages_message["batteryState"];
        //int response_feedMessageResponse_messages_message_hidden = response_feedMessageResponse_messages_message["hidden"];
        const char* messageContent = response_feedMessageResponse_messages_message["messageContent"];
        int altitude = response_feedMessageResponse_messages_message["altitude"];

        Serial.println(F(""));
        Serial.print(F("UnixTime: "));
        Serial.println(unixTime);
        Serial.print(F("Battery: "));
        Serial.println(batteryState);
        Serial.print(F("Message: "));
        Serial.println(messageContent);
        Serial.println(F("GPS Coordinate DD: "));
        Serial.println(gpslat, 5);
        Serial.println(gpslon, 5);

        char Altitude[6];
        sprintf(Altitude, "%06d", altitude);
        Serial.print(F("Altitude = "));
        Serial.println(Altitude);

        SpotBattery = (batteryState);
        SpotMessage = (messageContent);
        SpotAltitude = (Altitude);
        responseUnixTime = (unixTime);

        ////////////////////////////////////////////////////////////////////////
        // Lat Converting DD to DDM
        if (gpslat < 0) {
          lat_SouthN = "S";
          gpslat = (gpslat * -1);
        } else {
          lat_SouthN = "N";
        }

        int lat_d = gpslat;
        gpslat -= lat_d;
        gpslat = gpslat * 60;
        int lat_mm = gpslat;
        gpslat -= lat_mm;
        int lat_m = gpslat * 100;

        char ddmLat_d[3];
        sprintf(ddmLat_d, "%02d", lat_d);
        char ddmLat_mm[3];
        sprintf(ddmLat_mm, "%02d", lat_mm);
        char ddmLat_m[3];
        sprintf(ddmLat_m, "%02d", lat_m);

        blat = "";
        blat += (ddmLat_d);
        blat += (ddmLat_mm);
        blat += (".");
        blat += (ddmLat_m);
        blat += String(lat_SouthN);

        // Lon Converting DD to DDM

        if (gpslon < 0) {
          lon_EastW = "W";
          gpslon = (gpslon * -1);
        } else {
          lon_EastW = "E";
        }

        int lon_d = gpslon;
        gpslon -= lon_d;
        gpslon = gpslon * 60;
        int lon_mm = gpslon;
        gpslon -= lon_mm;
        int lon_m = gpslon * 100;

        char ddmLon_d[3];
        sprintf(ddmLon_d, "%03d", lon_d);
        char ddmLon_mm[3];
        sprintf(ddmLon_mm, "%02d", lon_mm);
        char ddmLon_m[3];
        sprintf(ddmLon_m, "%02d", lon_m);

        blon = "";
        blon += (ddmLon_d);
        blon += (ddmLon_mm);
        blon += (".");
        blon += (ddmLon_m);
        blon += String(lon_EastW);

        Serial.println(F("Converte GPS coordinate DD to DDM"));
        Serial.println(blat);
        Serial.println(blon);
      }
    } else {
      Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }

    https.end();
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
  delay(1);
  client->stop();
  Serial.println(F("Client STOP! 5sec"));
  delay(5000);
}

