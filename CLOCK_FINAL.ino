#include <Adafruit_GFX.h>   
#include <Adafruit_ST7735.h> 
#include <SPI.h>           
#include "DHT.h"
#include "image.h"
#include "gif_connect.h"
#include "time.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <RTClib.h>
#include <Wire.h>
#include <ctime>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <SimpleTimer.h>
#include <TimeLib.h>         // Thư viện cho thời gian now()

#include <vn_lunar.h>

#define DEFAULT_MQTT_HOST "mqtt1.eoh.io"
#define ERA_AUTH_TOKEN "0b5d8515-c9d5-4de9-90df-aa8a9944add2"
#define TIME_UPDATE 10000UL

#include <ERa.hpp>
#include <ERa/ERaTimer.hpp>

#define button 4
#define DHTPIN 15  
#define DHTTYPE DHT22  
DHT dht(DHTPIN, DHTTYPE);

int ledR = 12;
int ledG = 13; 

#define TFT_CS   5
#define TFT_RST  17
#define TFT_A0   19 //neu clock thi 19/16
#define TFT_MOSI 23 //SDA
#define TFT_SCLK 18 //SCK

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_A0, TFT_RST);

#define WHITE     0xFFFF
#define BLACK     0x0000      
#define BLUE      0x001F  
#define RED       0xF800
#define MAGENTA   0xF81F
#define GREEN     0x07E0
#define GREEN2    0xDFE0  
#define GREEN3    0x0c26  
#define CYAN      0x7FFF
#define YELLOW    0xFFE0
#define BROWN 		0XBC40 
#define GRAY      0X8430
#define PINK      0xD8D0
#define ORANGE    0xFA80

ERaTimer timer;

time_t lastUpdateTime = 0;

const char* ssid = "Connecting..";
const char* password = "nonotpass";
/*const char* ssid = "eoh.io";
const char* password = "Eoh@2020";*/

//String URL = "https://api.openweathermap.org/data/2.5/weather?lat=10.75&lon=106.6667&appid=82d54db1723be3afdd4077b426dc1b99";
String URL = "http://api.openweathermap.org/data/2.5/weather?";
String ApiKey = "82d54db1723be3afdd4077b426dc1b99";

// Replace with your location Credentials
String lat = "10.75";
String lon = "106.6667";

RTC_DS1307 rtc;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200, 60000);

uint32_t dd, mm, yy;

unsigned long lastNTPUpdate = 0;
const unsigned long NTP_UPDATE_INTERVAL = 15000; // Thời gian cập nhật NTP (5 giây)
bool rtcSynced = false;
bool isNetworkConnected = false; 

TaskHandle_t Era; 
TaskHandle_t LED;

//---------------------------------------------------------------------------------------------------------//
void setup() {
  Serial.begin(115200);
  Serial.println("DHTxx test!");
  dht.begin();
  SPI.begin();
  Wire.begin();
  rtc.begin();
  pinMode(button, INPUT);
  pinMode(ledR, OUTPUT);
  pinMode(ledG, OUTPUT);

  tft.initR(INITR_BLACKTAB);  
  tft.fillScreen(BLACK); 
  tft.setRotation(0); 

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    //delay(500);
    Serial.print(".");
    tft.drawRGBBitmap(2, 1, connect1, 126, 159);
    tft.drawRGBBitmap(2, 1, connect2, 126, 159);
    tft.drawRGBBitmap(2, 1, connect3, 126, 159);
    tft.drawRGBBitmap(2, 1, connect4, 126, 159); 
    tft.drawRGBBitmap(2, 1, connect5, 126, 159);
    tft.drawRGBBitmap(2, 1, connect6, 126, 159);
    tft.drawRGBBitmap(2, 1, connect7, 126, 159); 
    tft.drawRGBBitmap(2, 1, connect8, 126, 159);
    tft.drawRGBBitmap(2, 1, connect9, 126, 159);
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  isNetworkConnected = true;
  
  tft.fillRect(2, 1, 126, 159, BLACK);
  draw_clock(); 

  timeClient.begin();
  syncNTP();

  xTaskCreatePinnedToCore(Task1_Era,"Era",10000,NULL,1,&Era,1);  delay(500); 
  xTaskCreatePinnedToCore(Task1_LED,"LED",10000,NULL,1,&LED,1);  delay(100);
}

//---------------------------------------------------------------------------------------------------------//
void printText(char *text, uint16_t color, int x, int y,int textSize ){
  tft.setCursor(x, y);
  tft.setTextColor(color);
  tft.setTextSize(textSize);
  tft.print(text);
}

void draw_clock(){
  // Draw the bitmap image
  tft.drawRGBBitmap(96, 1, EoH, 32, 24);
  
  //hình vuông toàn màm hình
  tft.drawRect(2,1,126,159,WHITE); 

  //Cảm biến DHT11
  printText("ROOM", MAGENTA, 56, 133, 1);
  tft.drawLine(2, 136, 53, 136, WHITE);
  tft.drawLine(80, 136, 126, 136, WHITE);
  tft.drawLine(67, 141, 67, 159, WHITE);

  //API
  tft.drawLine(2, 47, 12, 47, WHITE);
  tft.drawLine(110, 47, 126, 47, WHITE);
}

//---------------------------------------------------------------------------------------------------------//
ERA_WRITE(V0){
  int value = param.getInt(); 
  digitalWrite(2, value); 
  ERa.virtualWrite(V0, digitalRead(2));
}

void timerEvent() {
    ERa.virtualWrite(V1, ERaMillis() / 1000L); //gửi thời gian hoạt động của eps lên v1
    float temperature = temperatureRead();
    ERa.virtualWrite(V2, temperature); //gửi giá trị nhiệt độ của esp lên
    ERA_LOG("Timer", "Uptime: %d", ERaMillis() / 1000L);
}

void Task1_Era(void * parameter)
{
  Serial.println(xPortGetCoreID()); 
  ERa.begin(ssid, password);
  timer.setInterval(1000L, timerEvent);
  for(;;){
    ERA_WRITE(V0);
    ERa.run();
    timer.run();
  }
}

void Task1_LED(void * parameter)
{
  Serial.println(xPortGetCoreID()); 
  for(;;){
    led();
  }
}

//---------------------------------------------------------------------------------------------------------//
void dht_22(){
  float humidity = dht.readHumidity(); // Đọc độ ẩm từ cảm biến DHT11
  float temperature = dht.readTemperature(); // Đọc nhiệt độ từ cảm biến DHT11 (độ C)

  // Lấy thời gian hiện tại
  time_t currentTime = now();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Kiểm tra nếu thời gian hiện tại khác thời gian gần nhất khi dữ liệu được cập nhật
  if (currentTime != lastUpdateTime) {
    updateDisplay(temperature, humidity);
    lastUpdateTime = currentTime;
  }

  tft.drawRGBBitmap(3, 138, nhietdo, 20, 20);
  tft.drawRGBBitmap(70, 143, doam, 15, 15);

  ERa.virtualWrite(V7, temperature);
  ERa.virtualWrite(V8, humidity);
}

// Hàm cập nhật dữ liệu lên màn hình TFT
void updateDisplay(float temperature, float humidity) {
  tft.setCursor(24, 145);                  //Hien thi nhiet do
  tft.setTextColor(YELLOW,BLACK);
  tft.setTextSize(1);
  tft.print(temperature);
  tft.print("C");

  tft.setCursor(86, 145);                  //Hien thi do am
  tft.setTextColor(YELLOW,BLACK);
  tft.setTextSize(1); //5x8
  tft.print(humidity);
  tft.print("%");
}

//---------------------------------------------------------------------------------------------------------//
void API(){
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(URL + "lat=" + lat + "&lon=" + lon + "&units=metric&appid=" + ApiKey);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String JSON_Data = http.getString();
      Serial.println(JSON_Data);

      DynamicJsonDocument doc(2048);
      deserializeJson(doc, JSON_Data);
      JsonObject obj = doc.as<JsonObject>();

      const char* cityName = obj["name"]; // Lấy tên thành phố
      const char* description = obj["weather"][0]["description"].as<const char*>();
      const char* main = obj["weather"][0]["main"].as<const char*>();
      const char* icon = obj["weather"] [0]["icon"].as<const char*>();
      const float temp = obj["main"]["temp"].as<float>();
      const float humidity = obj["main"]["humidity"].as<float>();
      const float windspeed = obj["wind"]["speed"].as<float>();
      const float winddeg = obj["wind"]["deg"].as<float>();
      const float feelslike = obj["main"]["feels_like"].as<float>();
      const float cloudiness = obj["clouds"]["all"].as<float>();
      const int pressure = obj["main"]["pressure"].as<int>();

      ERa.virtualWrite(V3, temp);
      ERa.virtualWrite(V4, humidity);
      ERa.virtualWrite(V5, windspeed);
      ERa.virtualWrite(V6, feelslike);
      ERa.virtualWrite(V9, description);

      printText("-Thanh Nguyen-", GREEN, 25, 120, 1);

      tft.setCursor(15, 44);                  //Hien thi thanh pho
      tft.setTextColor(MAGENTA,BLACK);
      tft.setTextSize(1);
      tft.print(cityName);

      tft.setCursor(10, 104);                  //Hien thi tinh trang thoi tiet
      tft.setTextColor(RED,BLACK);
      tft.setTextSize(1);
      tft.print(main);
      if (strcmp(icon, "01d") == 0) 
      {
        tft.drawRGBBitmap(5, 56, clear_sky_d, 54, 45);
      }
      else if (strcmp(icon, "01n") == 0) 
      {
        tft.drawRGBBitmap(5, 56, clear_sky_n, 54, 45);
      }
      else if (strcmp(icon, "02d") == 0) 
      {
        tft.drawRGBBitmap(5, 56, few_clouds_d, 54, 45);
      }
      else if (strcmp(icon, "02n") == 0) 
      {
        tft.drawRGBBitmap(5, 56, few_clouds_n, 54, 45);
      }
      else if ((strcmp(icon, "03d") == 0 || strcmp(icon, "03n") == 0) || (strcmp(icon, "04d") == 0 || strcmp(icon, "04n") == 0)) 
      {
        tft.drawRGBBitmap(5, 56, clouds, 54, 45);
      }
      else if ((strcmp(icon, "09d") == 0 || strcmp(icon, "09n") == 0) || (strcmp(icon, "10d") == 0 || strcmp(icon, "10n") == 0)) 
      {
        tft.drawRGBBitmap(5, 56, rain, 54, 45);
      }
      else if ((strcmp(icon, "11d") == 0 || strcmp(icon, "11n") == 0)) 
      {
        tft.drawRGBBitmap(5, 56, thunderstorm, 54, 45);
      }
    } else {
      Serial.println("Error!");
    }
    http.end();
  }
}

void API_2(){
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(URL + "lat=" + lat + "&lon=" + lon + "&units=metric&appid=" + ApiKey);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String JSON_Data = http.getString();
      Serial.println(JSON_Data);

      DynamicJsonDocument doc(2048);
      deserializeJson(doc, JSON_Data);
      JsonObject obj = doc.as<JsonObject>();

      const char* description = obj["weather"][0]["description"].as<const char*>();
      const char* main = obj["weather"][0]["main"].as<const char*>();
      const char* icon = obj["weather"] [0]["icon"].as<const char*>();
      const float temp = obj["main"]["temp"].as<float>();
      const float humidity = obj["main"]["humidity"].as<float>();
      const char* cityName = obj["name"]; // Lấy tên thành phố
      const float windspeed = obj["wind"]["speed"].as<float>();
      const float winddeg = obj["wind"]["deg"].as<float>();
      const float feelslike = obj["main"]["feels_like"].as<float>();
      const float cloudiness = obj["clouds"]["all"].as<float>();
      unsigned long sunrise = doc["sys"]["sunrise"]; // sunrise
      unsigned long sunset = doc["sys"]["sunset"]; // sunset
      int timezone = doc["timezone"];

      String sunriseTime = unixTimestampToLocalTime(sunrise, 7);
      String sunsetTime = unixTimestampToLocalTime(sunset, 7);

      tft.drawRGBBitmap(5, 52, weather, 20, 20);

      printText("Sunrise", GREEN, 5, 74, 1);
      printText("Sunset", GREEN, 5, 94, 1);
      printText("Temp", GREEN, 5, 114, 1);
      printText("Winds", GREEN, 70, 74, 1);
      printText("Feels", GREEN, 70, 94, 1);
      printText("Humi", GREEN, 70, 114, 1);

      tft.setCursor(5, 124);
      tft.setTextColor(RED,BLACK);
      tft.setTextSize(1);
      tft.print(temp);
      tft.print(" C");

      tft.setCursor(70, 124);
      tft.print(humidity);
      tft.print(" %");

      tft.setCursor(27, 57);
      tft.print(description);
          
      tft.setCursor(70, 84);
      tft.print(windspeed);
      tft.print(" m/s");
           
      tft.setCursor(70, 104);
      tft.print(feelslike);
      tft.print(" C");

      tft.setCursor(5, 84);
      tft.print(sunriseTime);

      tft.setCursor(5, 104);
      tft.print(sunsetTime);
    } 
    http.end();
  }else{
    tft.drawRGBBitmap(5, 52, disconnect, 120, 81);
  }
}

String unixTimestampToLocalTime(unsigned long unixTimestamp, int utctimezone) {
    String timeString = "";

    unsigned long seconds = unixTimestamp % 60;
    unixTimestamp /= 60;
    unsigned long minutes = unixTimestamp % 60;
    unixTimestamp /= 60;
    unsigned long hours = unixTimestamp % 24;
    unixTimestamp /= 24;

    hours += utctimezone;
    if (hours >= 24) {
        hours -= 24;
    }

    bool isPM = false;
    if (hours >= 12) {
        isPM = true;
        if (hours > 12) {
            hours -= 12;
        }
    }

    if (!isPM && hours == 12) {
        hours = 0; // Midnight
    } else if (isPM && hours != 12) {
        hours += 12;
    }

    timeString += hours < 10 ? "0" : "";
    timeString += String(hours);
    timeString += ":";
    timeString += minutes < 10 ? "0" : "";
    timeString += String(minutes);
    timeString += ":";
    timeString += seconds < 10 ? "0" : "";
    timeString += String(seconds);

    return timeString;
}

//---------------------------------------------------------------------------------------------------------//
void realtime(){
  if (WiFi.status() == WL_CONNECTED && !isNetworkConnected) {
    Serial.println("Network connected.");
    isNetworkConnected = true;
    syncNTP(); // Kiểm tra đồng bộ RTC khi có kết nối mạng lại
  }
  if (WiFi.status() != WL_CONNECTED && isNetworkConnected) {
    Serial.println("Network disconnected.");
    isNetworkConnected = false;
  }
  unsigned long currentMillis = millis();

  if (currentMillis - lastNTPUpdate >= NTP_UPDATE_INTERVAL) {
    syncNTP();
  }

  if (rtcSynced) {
    DateTime rtcTime = rtc.now();
    unsigned long ntpEpochTime = timeClient.getEpochTime();
    
    // So sánh thời gian RTC với thời gian từ NTP
    if (ntpEpochTime != rtcTime.unixtime()) {
      rtc.adjust(DateTime(ntpEpochTime));
      rtcSynced = true;
      Serial.println("RTC time adjusted!");
    }
    // In ra ngày trong tuần
    Serial.print("Day of the week: ");
    switch (rtcTime.dayOfTheWeek()) {
      case 0:
        Serial.println("Sunday");
        break;
      case 1:
        Serial.println("Monday");
        break;
      case 2:
        Serial.println("Tuesday");
        break;
      case 3:
        Serial.println("Wednesday");
        break;
      case 4:
        Serial.println("Thursday");
        break;
      case 5:
        Serial.println("Friday");
        break;
      case 6:
        Serial.println("Saturday");
        break;
      default:
        Serial.println("Invalid day");
        break;
    }

    tft.setTextColor(ORANGE, BLACK);
    tft.setCursor(4, 3);
    tft.setTextSize(1);
    switch (rtcTime.dayOfTheWeek()) {
      case 0:
        tft.println("Sunday");
        break;
      case 1:
        tft.println("Monday");
        break;
      case 2:
        tft.println("Tuesday");
        break;
      case 3:
        tft.println("Wednesday");
        break;
      case 4:
        tft.println("Thursday");
        break;
      case 5:
        tft.println("Friday");
        break;
      case 6:
        tft.println("Saturday");
        break;
      default:
        tft.println("Invalid day");
        break;
    }

    // Print date
    tft.setCursor(4, 14);
    tft.setTextSize(1);
    tft.print(rtcTime.day());
    tft.print("/");
    tft.print(rtcTime.month());
    tft.print("/");
    tft.print(rtcTime.year());

    // Print time
    tft.setTextColor(GREEN2, BLACK);
    tft.setCursor(15, 27);
    tft.setTextSize(2);
    tft.print(rtcTime.hour());
    tft.print(":");
    if (rtcTime.minute() < 10) tft.print("0");
    tft.print(rtcTime.minute());
    tft.print(":");
    if (rtcTime.second() < 10) tft.print("0");
    tft.println(rtcTime.second());

  }
}

void syncNTP() {
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.update();
    lastNTPUpdate = millis();
    rtc.adjust(DateTime(timeClient.getEpochTime()));
    rtcSynced = true;
    Serial.println("NTP đồng bộ RTC");
  } else {
    Serial.println("WiFi mất kết nối. Ngưng đồng bộ NTP.");
  }
}

//---------------------------------------------------------------------------------------------------------//
void lunar(){
  if (WiFi.status() == WL_CONNECTED && !isNetworkConnected) {
    Serial.println("Network connected.");
    isNetworkConnected = true;
    syncNTP(); // Kiểm tra đồng bộ RTC khi có kết nối mạng lại
  }
  if (WiFi.status() != WL_CONNECTED && isNetworkConnected) {
    Serial.println("Network disconnected.");
    isNetworkConnected = false;
  }
  unsigned long currentMillis = millis();

  if (currentMillis - lastNTPUpdate >= NTP_UPDATE_INTERVAL) {
    syncNTP();
  }

  // Kiểm tra xem có cần cập nhật thời gian RTC từ NTP không
  if (rtcSynced) {
    DateTime rtcTime = rtc.now();
    unsigned long ntpEpochTime = timeClient.getEpochTime();
    
    // So sánh thời gian RTC với thời gian từ NTP
    if (ntpEpochTime != rtcTime.unixtime()) {
      rtc.adjust(DateTime(ntpEpochTime));
      rtcSynced = true;
      Serial.println("RTC time adjusted!");
    }

    dd = rtcTime.day();
    mm = rtcTime.month();
    yy = rtcTime.year();

    vn_lunar solar;  //  You can change "solar" to "s2l" or something else.

    // convert from Solar (Gregorian calendar) to Lunar (Vietnamese lunar calendar).
    solar.convertSolar2Lunar(dd, mm, yy);

    int lunar_dd = solar.get_lunar_dd();
    int lunar_mm = solar.get_lunar_mm();
    int lunar_yy = solar.get_lunar_yy();

    tft.drawRGBBitmap(70, 77, date, 20, 20);
    printText("Lunar", GREEN, 80, 55, 1);
    printText("Calendar", GREEN, 70, 65, 1);

    tft.setTextColor(RED, BLACK);
    tft.setCursor(95, 80);
    tft.setTextSize(2);
    tft.print(lunar_dd);

    tft.setCursor(78, 103);
    tft.setTextSize(1);
    tft.print(lunar_mm);
    tft.print("/");
    tft.print(lunar_yy);
  }
}

//---------------------------------------------------------------------------------------------------------//
void led(){
  for(int i = 0; i < 2; i++) {
    digitalWrite(ledR, HIGH); 
    delay(100); 
    digitalWrite(ledR, LOW); 
    delay(100); 
  }

  for(int i = 0; i < 2; i++) {
    digitalWrite(ledG, HIGH); 
    delay(100);
    digitalWrite(ledG, LOW); 
    delay(100); 
  }
}

//---------------------------------------------------------------------------------------------------------//
void control_button(){
  int buttonState = digitalRead(button);
  if(buttonState == HIGH){
    tft.fillRect(5, 52, 120, 80, BLACK);
    API_2();
    delay(5000);
    tft.fillRect(5, 52, 120, 80, BLACK);
  }else{
    realtime();
    API();
    lunar();
    dht_22();
  }
}

//---------------------------------------------------------------------------------------------------------//
void loop() {
  control_button();
  if(WiFi.status() != WL_CONNECTED){
    tft.drawRGBBitmap(5, 56, noconnect, 54, 45);
  }
}

