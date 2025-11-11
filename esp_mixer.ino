// 
// nodeId = 985208077
///////////////////////////////////////////////////////// внешні бібліотеки
#include "painlessMesh.h"
#include "CRC.h"
#include <U8g2lib.h>                           // драйвер дисплея
#include "DHT.h"                               // сенсор влажності і температури
#include <Wire.h>                              // І2С
#include <SPI.h>                               // SPI на якому висить дісплей
#include "Max44009.h"                          // люксометр
#include "MAX30105.h"                          // пульсометр
#include "heartRate.h"                         // чекер пульса
#include <iarduino_RTC.h>                      // часи
#include "MHZ19.h"                             // СО2 сенсор
#include "RevEng_PAJ7620.h"                    // сенсор жестів
#include "mash_parameter.h"
#include "IMG.h"
/////////////////////////////////////////////////////// всякі класи
Scheduler userScheduler;
painlessMesh mesh; 

MHZ19 myMHZ19;                                             

HardwareSerial mySerial(2);

U8G2_SSD1327_WS_128X128_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 5, /* dc=*/ 2 ); // піни підключеня дисплея но тут вони не всі бо spi апаратний

DHT dht(4, DHT22);                                         // пін і тип сенсора влажності і температури 

Max44009 myLux(Max44009::Boolean::False);                  

MAX30105 PARTICLE_SENSOR;                                  // обявляе обект класа пусльсометра

iarduino_RTC watch(RTC_DS3231);                            // Объявляем объект watch для модуля часу

RevEng_PAJ7620 sensor = RevEng_PAJ7620();
Gesture gesture;  


String garland = "999";

char16_t redled_mod = 999;
String redled_pow = "999";
char16_t redled_bri = 999;

unsigned long tempfh = 0; 
const unsigned long intempfh = 200000; // 5666 хвилин у мілісекундах

unsigned long previousMillis = 0;
const long intervaldelay = 20000; 
bool messageSent = false;  // Прапорець для відстеження відправки повідомлення

void temp_for_heat(){
  String temp = "05" + String(dht.readTemperature()); 
    sendB(temp);
}

void tfhtimi () {
  unsigned long x = millis(); 
  if (x - tempfh >= intempfh) {
    tempfh = x; 
    temp_for_heat(); 
  }
}
void ppm_fit(){
  String ppm = "04" + String(myMHZ19.getCO2()); 
    sendB(ppm);
}
void temp_fit(){
  String temp = "05" + String(dht.readTemperature()); 
    sendB(temp);
}
void humi_fit(){
  String humi = "06" + String(dht.readHumidity()); 
    sendB(humi);
}
void lux_fit(){
  String lux = "07" + String(myLux.getLux()); 
    sendB(lux);
}

unsigned long prevMf = 0;
const long fval = 10;

void sens_fit(){
  unsigned long cMillis = millis();

  if (cMillis - prevMf >= fval) {
    prevMf = cMillis; // Оновлення часу перед викликом кожної функції

    ppm_fit();
    prevMf = millis(); // Оновлення часу після кожної функції
    temp_fit();
    prevMf = millis();
    humi_fit();
    prevMf = millis();
    lux_fit();
    prevMf = millis();
  }
}

void handleBody(const String& msg){

  String compKey = "01";                         // "01_mode_2"
  if (msg.substring(0, 2) == compKey) {
    if (msg.endsWith(String("0"))) {             redled_mod = 0;
    } else if (msg.endsWith(String("1"))) {      redled_mod = 1;
    } else if (msg.endsWith(String("2"))) {      redled_mod = 2;
    } else if (msg.endsWith(String("3"))) {      redled_mod = 3;
    } else if (msg.endsWith(String("4"))) {      redled_mod = 4;
    } else if (msg.endsWith(String("5"))) {      redled_mod = 5;
    } else if (msg.endsWith(String("6"))) {      redled_mod = 6;
    } else if (msg.endsWith(String("7"))) {      redled_mod = 7;
    } else if (msg.endsWith(String("8"))) {      redled_mod = 8;
    }
  }

  String briKey = "02";                         // "02_bri_2"
  if (msg.substring(0, 2) == briKey) {
    if (msg.endsWith(String("0"))) {             redled_bri = 0;
    } else if (msg.endsWith(String("1"))) {      redled_bri = 10;
    } else if (msg.endsWith(String("2"))) {      redled_bri = 20;
    } else if (msg.endsWith(String("3"))) {      redled_bri = 30;
    } else if (msg.endsWith(String("4"))) {      redled_bri = 40;
    } else if (msg.endsWith(String("5"))) {      redled_bri = 50;
    } else if (msg.endsWith(String("6"))) {      redled_bri = 60;
    } else if (msg.endsWith(String("7"))) {      redled_bri = 70;
    } else if (msg.endsWith(String("8"))) {      redled_bri = 80;
    } else if (msg.endsWith(String("9"))) {      redled_bri = 90;
    } else if (msg.endsWith(String("M"))) {      redled_bri = 100;
    }
  }

  if (msg.equals("garland_on")) {           garland = "ON";
  } else if (msg.equals("garland_off")) {    garland = "OFF";

  } else if (msg.equals("redled_on")) {    redled_pow = "ON";
  } else if (msg.equals("redled_off")) {    redled_pow = "OFF";
  }

  if (msg.equals("ppm_echo")) { 
    ppm_fit();
  }
  if (msg.equals("temp_echo")) { 
    temp_fit();
  }
  if (msg.equals("humi_echo")) { 
    humi_fit();
  }
  if (msg.equals("lux_echo")) { 
    lux_fit();
  }
  if (msg.equals("sens_echo")) { 
    sens_fit();
  }
}
 
////////////////////////////////////////////////////// всякі переменні

struct Ir {
  byte ledBrightness  = 0;                         //  Задаём яркость работы светодиода, при этом потребление тока будет следующим: 0 - 0мА, 255 - 50 мА
  byte sampleAverage  = 8;                          //  Устанавливаем коэффициент усреднения. Возможные варианты значений: 1, 2, 4, 8, 16, 32
  byte ledMode        = 2;                          //  Устанавливаем режим работы светодиодов на сенсоре: 1 - только красный (Red only), 2 - красный и ИК (Red + IR), 3 - красный, ИК и зелёный (Red + IR + Green)
  byte sampleRate     = 200;                        //  Устанавливаем частоту дискретизации (сглаживания сигнала). Варианты: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int  pulseWidth     = 411;                        //  Устанавливаем ширину импульса. Варианты: 69, 118, 215, 411
  int  adcRange       = 16384;                       //  Устанавливаем диапазон значений с АЦП. Варианты: 2048(11 бит), 4096(12 бит), 8192(13 бит), 16384(14 бит)

  byte rateSpot = 0;          //  Переменная с порядковым номером значения в массиве
  int beatAvg;                //  Создаём переменную для хранения усреднённого значения ЧСС
  long lastBeat = 0;          //  Время последнего зафиксированного удара
  float beatsPerMinute;       //  Создаём переменную для хранения значения ЧСС
  byte rates[4];      //  Массив со значениями ЧСС //  Коэффициент усреднения в сердині. ЧЕм больше число, тем больше усреднение показаний.

  int irValue = 0;    // значенає динамічні
} ;
Ir IR;                // обект структури 

struct lang {
  const char* uap1[13] = {
    "Пізда сухо","Сухо","Нормась","Заєбок","Шикарно","За дуже","%","C","Lux",
    "Давленіє","mm","СО 2", "ppm"
  };
  const char* uap2[5] = {
    "IR=","BPM=","SBPM=","Палець","Притули"
  };

  const char* enp1[13] = {
    "Fuckin' dry", "Dry", "Okay", "Awesome", "Great", "Very much", "%", "C", "Lux", "Pressure", "mm", "CO 2", "ppm"
  };
  const char* enp2[5] = {
    "IR=", "BPM=", "SBPM=", "Finger", "Hug"
  };

  char ling = 0;

  const char* lang1[13];
  const char* lang2[5];
};
lang Lang;


int ppm;
unsigned long getDataTimer = 0;

float weep;
float temp;
unsigned long temp_Timer = 0;

uint32_t lostTime = 0;

float flux = 0;

char wind = 0;

char16_t redtime = 0;

uint32_t trimi;

String connect;
bool connectF = 0;
///////////////////////////////////////////////////////////////////////// функції
void lang_flip() {
  if (Lang.ling == 0) {
    Lang.ling = 1;
    set_lang ();
    } else {
    Lang.ling = 0;
    set_lang ();
  }
}

void connecT() {                            // таймер обратного возврата з екрана конекта
  if(millis() - trimi >= 3000 && connectF == 1){
    trimi = millis();
    wind = 0;
  }
}
void newConnectionCallback(uint32_t nodeId) {
  connect = "New ";
  connect = connect + nodeId;
  wind = 4;
  connectF = 1;
}
void changedConnectionCallback() {
  connect = "Changed connections";
  wind = 4;
  connectF = 1;
}

void printCursor(int x,int y, const String& mas) {              // перегружана функція для удобства
  u8g2.setCursor(x,y);                                          //
  u8g2.print(mas);                                              //
}                                                               // зробляна просто для того шоб 
void printCursor(int x,int y, int cola) {                       //  було удобніше писати
  u8g2.setCursor(x,y);                                          //
  u8g2.print(cola);                                             //
}                                                               //
void printCursor(int x,int y, float coca) {                     //
  u8g2.setCursor(x,y);                                          //
  u8g2.print(coca);                                             //
}

void set_lang () {
  if (Lang.ling == 0) {
   for (int i = 0; i < 13; i++) {
     Lang.lang1[i] = Lang.uap1[i];
 }
 for (int i = 0; i < 5; i++) {
     Lang.lang2[i] = Lang.uap2[i];
 }
 } else {
   for (int i = 0; i < 13; i++) {
     Lang.lang1[i] = Lang.enp1[i];
 }
   for (int i = 0; i < 5; i++) {
     Lang.lang2[i] = Lang.enp2[i];
   }
 }
}

void redfix () {
  if (wind == 1 && redtime == 0) {             // фікс червоного светодіода пульсометра
    PARTICLE_SENSOR.setup(220, IR.sampleAverage, IR.ledMode, IR.sampleRate, IR.pulseWidth, IR.adcRange); // тут діод почті на макс (макс 255)
  } 
  if (wind == 0 && redtime > 0) {
    PARTICLE_SENSOR.setup(0, IR.sampleAverage, IR.ledMode, IR.sampleRate, IR.pulseWidth, IR.adcRange);   // логічно шо тут він виключаний
    redtime = 0;                    // і тут вспливає таймер повернення на головний дисплей
  }
}
void backTimer(){                            // таймер обратного возврата з екрана пульсометра
  if(millis() - trimi >= 3000 && IR.irValue < 50000){
    trimi = millis();
    wind = 0;
  }
}
float sbpm (float beatsPerMinute) {
  if (IR.beatsPerMinute < 255 && IR.beatsPerMinute > 20) {         //  Если количество ударов в минуту находится в промежутке между 20 и 255, то
        IR.rates[IR.rateSpot++] = (byte)IR.beatsPerMinute;         //  записываем это значение в массив значений ЧСС
        IR.rateSpot %= 4;                                          //  Задаём порядковый номер значения в массиве, возвращая остаток от деления и присваивая его переменной rateSpot
        IR.beatAvg = 0;                                            //  Обнуляем переменную и
        for (byte x = 0 ; x < 4 ; x++) {                           //  в цикле выполняем усреднение значений (чем больше RATE_SIZE, тем сильнее усреднение)
          IR.beatAvg += IR.rates[x];                               //  путём сложения всех элементов массива
        }
        IR.beatAvg /= 4;                                           //  а затем деления всей суммы на коэффициент усреднения (на общее количество элементов в массиве)
      }
  return IR.beatAvg;
}
int checkBeat (int irValue) {
  if (checkForBeat(IR.irValue) == true) {                     //  если пульс был зафиксирован, то
    long delta = millis() - IR.lastBeat;                   //  находим дельту по времени между ударами
    IR.lastBeat = millis();                                //  Обновляем счётчик
    IR.beatsPerMinute = 60 / (delta / 1000.0);             //  Вычисляем количество ударов в минуту
  }
  return IR.beatsPerMinute;
}
int redSens () {
  if (wind == 1) {

    redtime++;
    IR.irValue = PARTICLE_SENSOR.getIR();               //  Считываем значение отражённого ИК-светодиода (отвечающего за пульс) и

    checkBeat (IR.irValue);
    sbpm (IR.beatsPerMinute);
  }
  return IR.irValue;
}

int giv_ppm () {                                      // вертає уровень СО2
  if (millis() - getDataTimer >= 2000) {
    ppm = myMHZ19.getCO2();                             
    getDataTimer = millis();
    return ppm;
  }
}

int givFlux () {                             // функція збору люксів 
    if (millis() - lostTime >= 1000) {       // шоб дані не збиралися дуже часто
    lostTime += 1000;                        // тут скидуєм таймер
    flux = myLux.getLux();                   // і получаєм люксіки
  }
    return flux;                             // вертаєм в глобалочку
}

void timeS (){
  u8g2.setFont(u8g2_font_courB18_tn );
  printCursor (0,60,watch.gettime("d-m"));

  u8g2.setFont(u8g2_font_maniac_tn);
  printCursor (0,100,watch.gettime("H:i:s"));
  u8g2.setFont(u8g2_font_cu12_t_cyrillic);
}


void guest() {
  switch (gesture)                  // собствено сам обработчик жестів
  {
    case GES_FORWARD:      {
        break;
      }

    case GES_BACKWARD:      {
        sendB("next_eff");
        break;
      }

    case GES_LEFT:      {
        if (wind == 0){
          wind = 1;
        }

        if (wind == 5){
          sendB("02_bri_5"); //red_led
        }
        break;
      }

    case GES_RIGHT:      {
        if (wind == 0){
          wind = 2;
        }

        if (wind == 5){
          sendB("garland");
        }
        break;
      }

    case GES_UP:      {
        if (wind != 0 && wind != 5) {
          wind = 0;
        }

        break;
      }

    case GES_DOWN:      {
        if (wind == 0) {
          wind = 5;
        }

        break;
      }

    case GES_CLOCKWISE:      {
        sendB("power"); //red_led
        break;
      }

    case GES_ANTICLOCKWISE:      {
        lang_flip();
        break;
      }

    case GES_WAVE:      {
        wind = 3;
        break;
      }

    case GES_NONE:      {
        break;
      }
  }
}

// void noodes(){
//   std::list<uint32_t> nodes = mesh.getNodeList();

//   Serial.println("Підключені вузли:");
//   for (auto nodeId : nodes) {
//     Serial.println(nodeId);
//   }
//}
//////////////////////////////////////////////////////////////////////////// ламповий сетапчик
void setup(void) {
  Serial.begin(9600);

  WiFi.setSleep(false);

  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT );
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);

  sensor.begin();

  mySerial.begin(9600);                                   // 
  myMHZ19.begin(mySerial);                                // тут вибираєця апаратний UART
  myMHZ19.autoCalibration();                              // переключатель авто калибровки ON (OFF autoCalibration(false))

  dht.begin();                                            // ініт сенсора температури і влажності
  
  u8g2.begin();                                           // ініт дисплея
  u8g2.drawBitmap(0, 0, 16, 128, Pic.logo);               // прінт лого
  u8g2.sendBuffer();                                      // висилка буфера)
  delay(1000);                                            // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  u8g2.enableUTF8Print();  
  u8g2.setFont(u8g2_font_cu12_t_cyrillic);                // підтримка шрифтом українського текста 

  Wire.begin(21, 22);                                     // ініт I2C

  myLux.configure(MAX44009_DEFAULT_ADDRESS, &Wire);

  pinMode(35, INPUT);    

  PARTICLE_SENSOR.begin();
  PARTICLE_SENSOR.setup(IR.ledBrightness, IR.sampleAverage, IR.ledMode, IR.sampleRate, IR.pulseWidth, IR.adcRange);

  watch.begin();

  set_lang ();
}
////////////////////////////////////////////////////////////////////// основна куча гавна
void loop(void) {

  if (!messageSent) { // Перевіряємо, чи повідомлення ще не було відправлено
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= intervaldelay) {
      sens_fit();
      // Встановлюємо прапорець, щоб більше не відправляти повідомлення
      messageSent = true;
    }
  }
  // --- deferred CRC queue processing ---
  for (uint8_t _i=0; _i<4; ++_i){ String _b; if (!qPop(_b)) break; handleBody(_b); }

  tfhtimi();

  connecT();

  mesh.update();

  giv_ppm();

  givFlux();

  redfix();

  redSens();


  if (millis() - temp_Timer >= 2002) {
    weep = dht.readHumidity();
    temp = dht.readTemperature();
    temp_Timer = millis();
  }

  gesture = sensor.readGesture();   // тут і зчитуюця жести

  guest();
//////////////////////////////////////////////////////////////////////////////   Графіка

  switch (wind) {

    case 0:                               /////////
      u8g2.firstPage();
      do {

        u8g2.setCursor(0, 12);
        if (weep < 30){
          u8g2.print(Lang.lang1[0]);
        } else if (weep < 40) {
          u8g2.print(Lang.lang1[1]);
        } else if (weep < 45) {
          u8g2.print(Lang.lang1[2]);
        } else if (weep < 50) {
          u8g2.print(Lang.lang1[3]);
        } else if (weep < 60) {
          u8g2.print(Lang.lang1[4]);
        } else {
          u8g2.print(Lang.lang1[5]);
        }
        printCursor (75,12,weep);
        printCursor (113,12,Lang.lang1[6]);

        printCursor (0,50,temp);
        printCursor (36,50,Lang.lang1[7]);

        printCursor (0,90,flux);
        if (flux < 1000) {
          if (flux < 10) {
            u8g2.setCursor(34, 90);
            u8g2.print(Lang.lang1[8]);
          } else {
            u8g2.setCursor(43, 90);
            u8g2.print(Lang.lang1[8]);
          }
        } else {
          u8g2.setCursor(48, 90);
          u8g2.print(Lang.lang1[8]);
        }


        printCursor (0,126,Lang.lang1[11]);
        printCursor (40,126,ppm);
        printCursor (75,124,Lang.lang1[12]);

        trimi = millis();

      } while ( u8g2.nextPage() );
      break;

    case 1:                                 //////////
      u8g2.firstPage();
      do {
        backTimer();

        if (IR.irValue > 50000) {

          printCursor (0,15,Lang.lang2[0]);
          printCursor (33,15,IR.irValue);

          printCursor (0,32,Lang.lang2[1]);
          printCursor (48,32,IR.beatsPerMinute);

          printCursor (0,47,Lang.lang2[2]);
          printCursor (55,47,IR.beatAvg);

          u8g2.drawBitmap(83, 5, 6, 45, Pic.ser);

          trimi = millis();
          
        } else {

          printCursor (0,32,Lang.lang2[3]);
          printCursor (0,50,Lang.lang2[4]);

          u8g2.drawBitmap(64, 0, 8, 64, Pic.clickpic);
        }
      } while ( u8g2.nextPage() );
    break;

    case 2:
      u8g2.firstPage();
      do{
        timeS();
      } while ( u8g2.nextPage() );
    break;
  
    case 3:
      u8g2.firstPage();
      do{
        timeS();
      } while ( u8g2.nextPage() );
        u8g2.firstPage();
      do{
        u8g2.drawBitmap(0, 0, 16, 128, Pic.egg);
      } while ( u8g2.nextPage() );
      break;

    case 4:
      u8g2.firstPage();
      do{
        printCursor (0,50,connect);
        u8g2.drawBitmap(0, 64, 16, 64, Pic.mesh_pic);
      } while ( u8g2.nextPage() );
    break;

    case 5:
      sendB("garland_echo");
      sendB("red_led_echo"); 

      u8g2.firstPage();
      do{
        printCursor (0,20,"Світло");
        printCursor (0,40, redled_pow );

        printCursor (0,60,"Єфект");
        printCursor (0,80, redled_mod );

        printCursor (0,100,"Яркость");
        printCursor (0,120, redled_bri );

        printCursor (64,30,"Гірлянда");
        printCursor (64,50, garland );

      } while ( u8g2.nextPage() );
    break;
    }
}
// spo2
// баг фікс середьньго серцевого ритму
// нормальне зміненя дисплеїв