#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Wire.h>



//**********TELEGRAM & WIFI STUFF**********
#define WIFI_SSID " "
#define WIFI_PASSWORD " "
WiFiClientSecure secured_client;

#define BOT_TOKEN " "
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

const char* ALLOWED_CHAT_ID[] = { //CHAT ID degli utenti che devono ricevere le notifiche
  " ",
  " "
};
int numAllowedChatId = 2; //Numero degli utenti che devono ricevere le notifiche


//**********APDS9960**********
#define APDS9960_I2C_ADDR       0x39
#define APDS9960_ENABLE         0x80
#define APDS9960_ATIME          0x81
#define APDS9960_WTIME          0x83
#define APDS9960_PILT           0x89
#define APDS9960_PIHT           0x8B
#define APDS9960_PERS           0x8C
#define APDS9960_CONFIG1        0x8D
#define APDS9960_PPULSE         0x8E
#define APDS9960_CONTROL        0x8F
#define APDS9960_CONFIG2        0x90
#define APDS9960_ID             0x92
#define APDS9960_STATUS         0x93
#define APDS9960_PDATA          0x9C
#define APDS9960_POFFSET_UR     0x9D
#define APDS9960_POFFSET_DL     0x9E
#define APDS9960_CONFIG3        0x9F
#define APDS9960_PICLEAR        0xE5

#define APDS9960_INT_PIN GPIO_NUM_33
uint8_t throwaway;
uint8_t PROX_INT_HIGH;
uint8_t PROX_INT_LOW;


//**********Others**********
RTC_DATA_ATTR unsigned int bootCount = 0;



bool wireWriteDataByte(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(APDS9960_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  if(Wire.endTransmission() != 0)
    return false;
  return true;
}

bool wireReadDataByte(uint8_t reg, uint8_t &val) {
  Wire.beginTransmission(APDS9960_I2C_ADDR);
  Wire.write(reg);
  if(Wire.endTransmission() != 0)
    return false;
  Wire.requestFrom(APDS9960_I2C_ADDR, 1);
  while(Wire.available())
      val = Wire.read();
  return true;
}

void send_notify(void) {
  for(int i = 0; i < numAllowedChatId; i++) {
    if(PROX_INT_LOW == 0) {
      bot.sendMessage(ALLOWED_CHAT_ID[i], "La porta si è aperta", "");
    }
    else {
      bot.sendMessage(ALLOWED_CHAT_ID[i], "La porta si è chiusa", "");
    }
  }
}



void setup() {
  //Serial Initialization
  Serial.begin(115200);
  Serial.println();

  //Led Builtin Initialization
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); //For visualize interrupt receiving

  //Wifi Initialization
  Serial.print("Connecting to Wifi SSID " + (String)WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  //Time Configuration
  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org");
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);

  //BOOT Setup
  bootCount++;
  Serial.println("Boot number: " + String(bootCount));
  if(bootCount % 2) { // porta aperta
    PROX_INT_HIGH = 20;
    PROX_INT_LOW = 0;
  }
  else {
    PROX_INT_HIGH = 255;
    PROX_INT_LOW = 150;
  }

  //APDS9960 Initialization
  Wire.begin();
  wireWriteDataByte(APDS9960_ENABLE, 0);            //it's reccomended to turn off the device before config
  //wireWriteDataByte(APDS9960_ATIME, 0xDB);        //256-value cycles, 2.78ms per cycles (necessary only for ALS/Color engine)
  wireWriteDataByte(APDS9960_WTIME, 0x48);          //field value: 72 -> 500ms
  wireWriteDataByte(APDS9960_PPULSE ,0x47);         //PPLEN: 8us, PPULSE: 8 (0x47)
  wireWriteDataByte(APDS9960_CONTROL, 0xC0);        //LDRIVE: 12.5mA, PGAIN: 1x
  wireWriteDataByte(APDS9960_CONFIG2, 0x01);        //LEDBOOST disabled
  wireWriteDataByte(APDS9960_PERS, 0x10);           //Interrupt Persistency set
  wireWriteDataByte(APDS9960_PILT, PROX_INT_LOW);   //Set interrupt range
  wireWriteDataByte(APDS9960_PIHT, PROX_INT_HIGH);  //Set interrupt range
  wireWriteDataByte(APDS9960_ENABLE, 0x2D);         //Enabling pon (power enable), pen (proximity enable), wen (wait enable), PIEN (proximity interrupt enable)

  //Deep Sleep Wakeup Routine
  if(esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    wireReadDataByte(APDS9960_PICLEAR, throwaway);  //Clear Interrupt
    send_notify();
  }

  esp_sleep_enable_ext0_wakeup(APDS9960_INT_PIN, 0);//Turn on wakeup interrupt

  Serial.println("Going to sleep");
  esp_deep_sleep_start();
}

void loop() {

}