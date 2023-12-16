#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <string.h>
#include "secrets.h"
#define TIME_ZONE -5

//로라 통신
#include "SNIPE.h"
#define ATSerial Serial

//16byte hex key
String lora_app_key = "11 22 33 44 55 66 77 88 99 aa bb cc dd ee ff 00";
SNIPE SNIPE(ATSerial);

unsigned long lastMillis = 0;
unsigned long previousMillis = 0;
const long interval = 5000;
 
#define AWS_IOT_PUBLISH_TOPIC "$aws/things/ArduinoB/shadow/update"
#define CHECK_RFID "checkRFID"
#define PUBLISH_A "Publish_To_A"
#define PUBLISH_B "Publish_To_B"

WiFiClientSecure net;
 
BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);
 
PubSubClient client(net);
 
time_t now;
time_t nowish = 1510592825;

String trash_name;
String ratio;
String uid;
String uid_id;

String value;
String isCheckedValue;
String infoValue; 
String isCheckedValueStr;



void NTPConnect(void)
{
  Serial.print("Setting time using SNTP");
  configTime(TIME_ZONE * 3600, 0 * 3600, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < nowish)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

void messageReceived(char *topic, byte *payload, unsigned int length) {
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // 변환할 JSON 문자열
  String jsonStr;
  for (unsigned int i = 0; i < length; i++) {
    jsonStr += (char)payload[i];
  }
  
  if (String(topic) == "Publish_To_B") {
    // 주제가 "Publish_To_A"인 경우, payload를 정수로 변환
    value = jsonStr;
    Serial.print("Value: ");
    Serial.println(value);
  } 
  
  else if (String(topic) == "checkRFID") {
    isCheckedValue = jsonStr;
    Serial.print("isChecked: ");
    Serial.println(isCheckedValue);
  }
   // 메시지 처리 후 jsonStr 초기화
  jsonStr = "";
}

void connectAWS()
{
  delay(3000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  Serial.println(String("Attempting to connect to SSID: ") + String(WIFI_SSID));
 
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
 
  NTPConnect();
 
  net.setTrustAnchors(&cert);
  net.setClientRSACert(&client_crt, &key);
 
  client.setServer(MQTT_HOST, 8883);
  client.setCallback(messageReceived);
 
 
  Serial.println("Connecting to AWS IOT");
 
  while (!client.connect(THINGNAME))
  {
    Serial.print(".");
    delay(1000);
  }
 
  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }
  // Subscribe to a topic
  client.subscribe(CHECK_RFID);
  client.subscribe(PUBLISH_A);
  client.subscribe(PUBLISH_B);

 
  Serial.println("AWS IoT Connected!");
}
 
 
void publishMessage(String trash_name, String ratio, String uid, String uid_id) {
  StaticJsonDocument<200> doc;
  char jsonBuffer[512];

  // Create the innermost reported object
  JsonObject reported = doc.createNestedObject("state").createNestedObject("reported");


  // Add the required fields to the reported object
  reported["name"] = trash_name;
  reported["ratio"] = ratio;
  reported["uid"] = uid;
  reported["uid_id"] = uid_id;

  serializeJson(doc, jsonBuffer); // Serialize the JSON to buffer

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer); // Publish the message
}
 


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  //connectAWS();
  while(!Serial); //시리얼 통신이 준비될 때까지 대기
  ATSerial.begin(115200);
  while (ATSerial.read() >= 0) {}
  while (!ATSerial);

  /* SNIPE LoRa Initialization */
  if (!SNIPE.lora_init())
  {
    Serial.println("SNIPE LoRa Initialization Fail!");
    while (1);
  }
  /* SNIPE LoRa Set AppKey */
  if (!SNIPE.lora_setAppKey(lora_app_key))
  {
    Serial.println("SNIPE LoRa app key value has not been changed");
  }
  /* SNIPE LoRa Set Frequency */
  if (!SNIPE.lora_setFreq(LORA_CH_1))
  {
    Serial.println("SNIPE LoRa Frequency value has not been changed");
  }
  /* SNIPE LoRa Set Spreading Factor */
  if (!SNIPE.lora_setSf(LORA_SF_7))
  {
    Serial.println("SNIPE LoRa Sf value has not been changed");
  }
  /* SNIPE LoRa Set Rx Timeout 
   * If you select LORA_SF_12, 
   * RX Timout use a value greater than 5000  
  */
  if (!SNIPE.lora_setRxtout(5000))
  {
    Serial.println("SNIPE LoRa Rx Timout value has not been changed");
  }
  Serial.println("SNIPE LoRa DHT22 Recv");
}

void loop() {
  // put your main code here, to run repeatedly:
  String ver = SNIPE.lora_recv(); //로라로 수신
  delay(300);

  //로라 통신
  //만약 데이터(여기서는 초음파 퍼센테이지 값)가 오류 없이 도착하면
   if (ver != "AT_RX_TIMEOUT" && ver != "AT_RX_ERROR")
   {
      char buffer[50];
      char buffer_send[50];
      String send_value;
      
      memset(buffer, 0x0, sizeof(buffer));
      ver.toCharArray(buffer, sizeof(buffer));
      int seperatorIndex1 = ver.indexOf(':');
      int seperatorIndex2 = ver.indexOf(':',seperatorIndex1+1);

      String Value1 = ver.substring(0, seperatorIndex1);
      String Value2 = ver.substring(seperatorIndex1+1,seperatorIndex2);
      String Value3 = ver.substring(seperatorIndex2 + 1);

      Serial.println("receive success");
      Serial.print("MESSAGE:");
      Serial.println(ver);
      Serial.println(SNIPE.lora_getRssi());
      Serial.println(SNIPE.lora_getSnr());
      Serial.print("Trash can:");
      Serial.println(Value1);
      Serial.print("ratio:");
      Value2 = Value2.toFloat();
      Serial.println(Value2);
      Serial.print("uid:");
      Serial.println(Value3);

      trash_name = Value1;
      ratio = Value2;
      uid = Value3;
      uid_id = Value1;

      sprintf(buffer, "%s:%s", isCheckedValue, value);
      send_value = buffer;
      //답신
      if(SNIPE.lora_send(send_value))
      {
        Serial.println("send success");
        Serial.print("send:");
        Serial.println(send_value);
      }
      else{
        Serial.println("send fail");
        delay(500);
      }
   }
   else{
      Serial.println("recv fail");
      delay(500);
   }
   
  //AssignName(trash_name, ratio, uid, uid_id);
 
  now = time(nullptr);
  if (!client.connected())
  {
    connectAWS();
  }
  else
  {
    client.loop();
    if (millis() - lastMillis > 5000)
    {
      lastMillis = millis();
      publishMessage(trash_name, ratio, uid, uid_id);
      Serial.println("Message");
      Serial.println(trash_name);
      Serial.println(ratio);
      Serial.println(uid);
      Serial.println(uid_id);
      Serial.println("succeed");
    }
    client.setCallback(messageReceived);
  }
  delay(3000);

}
