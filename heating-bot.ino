#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Wire.h"

/* Wifi network station credentials */
#define WIFI_SSID       "OnlyStaff"
#define WIFI_PASSWORD   "jesuslove2"

/* Telegram BOT Token (Get from Botfather) */
#define BOT_TOKEN       "5521451449:AAFFOyVD0dxFqz2LxwsX99N4CxdmsFy7MO8"

String chat_ids[50] = {};
String chat_id_masters[5] = {"241097915" }; // Мастер чаты


OneWire oneWire(2);
DallasTemperature sensors(&oneWire);

const unsigned long BOT_MTBS = 1000; // Пауза между работой системы
const unsigned long BOT_ALR = 1000 * 60 * 60 * 1; // Пауза для предупрежденич

#define REL_1 4
bool ledStatus = true;
bool BuzzerStatus = false;

bool heating = true;
bool is_set_temp = false;
float set_temp = 20;
byte ds18b20_lost = 0;


// Пороговое значение:
unsigned long bot_lasttime; // last time messages' scan has been done
unsigned long bot_lasttime_alr; // last time messages' scan has been done

X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);


void handleNewMessages(int numNewMessages)
{
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = bot.messages[i].chat_id;

    if (check_chat_id(chat_id)) {
      send_messages_to_master("🛠 Добавлен новый чат " + chat_id + " Имя "+ bot.messages[i].from_name + " 🛠");
    }

    String text = bot.messages[i].text;
    if (is_set_temp) {
      if (text.toFloat() < 18 or text.toFloat() > 24) {
        bot.sendMessage(chat_id, "❗️❗️Введено неверное значение❗️❗️\n введите число от 18 до 24", "");
      } else {
        set_temp = text.toFloat();
        bot.sendMessage(chat_id, "Задано значение температруы: " + String(set_temp) + "°C", "");
        String message = "🛠Была изменена температура🛠\n";
        message += "пользователем: " + bot.messages[i].from_name;
        send_messages_to_master(message);
        is_set_temp = LOW;
      }
    }
    else if (text == "/heating_off")
    {
      heating = LOW;
      bot.sendMessage(chat_id, "Отопление ОТКЛЮЧЕНО❄️", "");
      String message = "🛠Отопление ОТКЛЮЧЕНО🛠\n";
      message += "пользователем: " + bot.messages[i].from_name;
      send_messages_to_master(message);
    }
    else if (text == "/heating_on")
    {
      heating = HIGH;
      bot.sendMessage(chat_id, "Отопление ВКЛЮЧЕНО🔥", "");
      String message = "🛠Отопление ВКЛЮЧЕНО🛠\n";
      message += "пользователем: " + bot.messages[i].from_name;
      send_messages_to_master(message);
    }
    else if (text == "/state")
    {
      sensors.requestTemperatures();
      float temp = sensors.getTempCByIndex(0);
      String message = "Температура в помещении: " + String(temp) + "°C\n";
      message += "Заданная температура: " + String(set_temp) + "°C\n";
      if (digitalRead(REL_1) == LOW) {
        message += "Отопление не работает\n";
      } else {
        message += "Отопление работает\n";
      }
      bot.sendMessage(chat_id, message, "");
    }
    else if (text == "/set_temp")
    {
      is_set_temp = HIGH;
      bot.sendMessage(chat_id, "Текущее значение: " + String(set_temp) + "°C\nВведите своё", "");
    }
    else if (text == "/help" or text == "/start")
    {
      String welcome = "Добро пожаловать!\nТут можно настроить отопление, для этого воспользуйтесь командами меню\n↙️";
      bot.sendMessage(chat_id, welcome, "");
    }
  }
}


void send_messages(String message) {
  int i = 0;
  while (chat_ids[i] != "" ) {
    bot.sendMessage(chat_ids[i], message, "");
    i = i + 1;
  }

}


void send_messages_to_master(String message) {
  int i = 0;
  while (chat_id_masters[i] != "" ) {
    bot.sendMessage(chat_id_masters[i], message, "");
    i = i + 1;
  }

}

bool check_chat_id(String chat_id) {
  int i = 0;
  while (not chat_ids[i].equals(chat_id) && chat_ids[i] != "") {
    i = i + 1;
    if (i == 50) return false ;
  }
  if (chat_ids[i] != "") return false;
  chat_ids[i] = chat_id;
  return true;
}


void setup() {
  Serial.begin(115200);
  Serial.println();

  pinMode(REL_1, OUTPUT);
  digitalWrite(REL_1, HIGH);

  configTime(0, 0, "pool.ntp.org");      // get UTC time via NTP
  secured_client.setTrustAnchors(&cert); // Add root certificate for api.telegram.org
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  send_messages_to_master("🛠Программа перезапускалась🛠");

  sensors.begin();
}


void loop() {
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    sensors.requestTemperatures();   // от датчика получаем значение температуры
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) // проверка есть ли в массиве сообщений от бота
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();

    if (heating == false) { // проверка на ручное отключение
      if (digitalRead(REL_1) == HIGH); {
        digitalWrite(REL_1, LOW);
      }
    }
    else {
      float temp = sensors.getTempCByIndex(0);

      if (temp == -127) { // проверка на потерю соединения с датчиком
        ds18b20_lost = ds18b20_lost + 1;
        if (ds18b20_lost > 30) {
          send_messages_to_master("🛠Потеря соединения с датчиком🛠");
          ds18b20_lost = 0;
        }
      }
      else {
        ds18b20_lost = 0;
        if (temp < set_temp) { //включение еслт температруа ниже заданной
          digitalWrite(REL_1, HIGH);
        } else if (temp > set_temp + 0.49) { // отключение ели температура выше заданно на пол градуса
          digitalWrite(REL_1, LOW);
        }
        if (temp < set_temp - 3) { // проверка экстремально низкой температруы
          if (millis() - bot_lasttime_alr > BOT_ALR) {
            bot_lasttime_alr = millis();
            send_messages("❗️❗️Внимание❗️❗️ \nтемператруа в помещении стала слишком низкой🥶🥶" + String(temp) + "°C");
          }
        }
      }
    }
  }
}
