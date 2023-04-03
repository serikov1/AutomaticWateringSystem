#define PUPM_AMOUNT 2       // количество помп, подключенных через реле/мосфет
#define START_PIN 4         // подключены начиная с пина
#define SWITCH_LEVEL 1      // уровень для открытого мосфета
#define PARALLEL 0          // 1 - параллельный полив, 0 - полив в порядке очереди
#define TIMER_START 1       // 1 - отсчёт периода с момента ВЫКЛЮЧЕНИЯ помпы, 0 - с момента ВКЛЮЧЕНИЯ помпы
#define BACKL_TOUT 6       // тайм-аут работы экрана

static const char *relayNames[]  = {
  "Куст 1",
  "Куст 2",
};

#define CLK 17
#define DT 16
#define SW 15

#include "GyverEncoder.h"
Encoder enc1(CLK, DT, SW);

#include <EEPROMex.h>
#include <EEPROMVar.h>

#define _LCD_TYPE 1
#include "LCD_1602_RUS_ALL.h"
LCD_1602_RUS lcd(0x27, 16, 2);

uint32_t pump_timers[PUPM_AMOUNT];
uint32_t pumping_time[PUPM_AMOUNT];
uint32_t period_time[PUPM_AMOUNT];
bool pump_state[PUPM_AMOUNT];
byte pump_pins[PUPM_AMOUNT];

int8_t current_set;
int8_t current_pump;
bool now_pumping = false;

int8_t thisH, thisM, thisS;
long thisPeriod;
bool startFlag = true;
uint32_t backlTmr = 0;
bool backlFlag = true;

void setup() {
  // --------------------- КОНФИГУРИРУЕМ ПИНЫ ---------------------
  for (byte i = 0; i < PUPM_AMOUNT; i++) {            // по всем помпам
    pump_pins[i] = START_PIN + i;                     // настраиваем массив пинов
    pinMode(START_PIN + i, OUTPUT);                   // настраиваем пины
    digitalWrite(START_PIN + i, !SWITCH_LEVEL);       // выключаем
  }
  // --------------------- ИНИЦИАЛИЗИРУЕМ ЭКРАН И ЭНКОДЕР ---------------------

  lcd.init();
  lcd.backlight();
  lcd.clear();
  enc1.setType(1);

  // --------------------------- НАСТРОЙКИ EEPROM ---------------------------
  //в ячейке 1023 должен быть записан флаг, если его нет - делаем (ПЕРВЫЙ ЗАПУСК)
  if (EEPROM.read(1023) != 5) {
    EEPROM.writeByte(1023, 5);
    for (byte i = 0; i < 100; i += 4) EEPROM.writeLong(i, 0);
  }

  for (byte i = 0; i < PUPM_AMOUNT; i++) {            // по всем помпам
    period_time[i] = EEPROM.readLong(8 * i);          // читаем данные из памяти. На чётных - период (ПАУЗА)
    pumping_time[i] = EEPROM.readLong(8 * i + 4);     // на нечётных - полив (РАБОТА)
    pump_state[i] = 0;                                // выключить все помпы
  }

  // ---------------------- ВЫВОД НА ДИСПЛЕЙ ------------------------
  drawLabels();
  changeSet();
}

void loop() {
  periodTick();
  encoderTick();
  flowTick();
  backlTick();
}

void periodTick() {
  for (byte i = 0; i < PUPM_AMOUNT; i++) {
    if ( startFlag ||
        ( period_time[i] > 0
         && millis() - pump_timers[i] >= period_time[i] * 1000
         && (pump_state[i] != SWITCH_LEVEL)
         && !(now_pumping * !PARALLEL) ) ) {
      pump_state[i] = SWITCH_LEVEL;
      digitalWrite(pump_pins[i], SWITCH_LEVEL);
      pump_timers[i] = millis();
      now_pumping = true;
    }
  }
  startFlag = false;
}

void flowTick() {
  for (byte i = 0; i < PUPM_AMOUNT; i++) {
    if ( pumping_time[i] > 0
        && millis() - pump_timers[i] >= pumping_time[i] * 1000
        && (pump_state[i] == SWITCH_LEVEL) ) {
      pump_state[i] = !SWITCH_LEVEL;
      digitalWrite(pump_pins[i], !SWITCH_LEVEL);
      if (TIMER_START) pump_timers[i] = millis();
      now_pumping = false;
    }
  }
}

void encoderTick() {
  enc1.tick();

  if (enc1.isTurn()) {
    if (backlFlag) {
      backlTmr = millis();
      if (enc1.isRight()) {                         // поворот без нажатия на кнопку энкодера
        if (++current_set >= 7) current_set = 6;
      } else if (enc1.isLeft()) {
        if (--current_set < 0) current_set = 0;
      }

      if (enc1.isRightH())                          // поворот с нажатием на кнопку энкодера
        changeSettings(1);
      else if (enc1.isLeftH())
        changeSettings(-1);

      changeSet();
    } else {
      backlOn();
    }
  }
}


// меняем номер помпы и настройки
void changeSettings(int increment) {
  if (current_set == 0) {
    current_pump += increment;
    if (current_pump > PUPM_AMOUNT - 1) current_pump = PUPM_AMOUNT - 1;
    if (current_pump < 0) current_pump = 0;
    s_to_hms(period_time[current_pump]);
    drawLabels();
  } else {
    if (current_set == 1 || current_set == 4) {
      thisH += increment;
    } else if (current_set == 2 || current_set == 5) {
      thisM += increment;
    } else if (current_set == 3 || current_set == 6) {
      thisS += increment;
    }
    if (thisS > 59) {
      thisS = 0;
      thisM++;
    }
    if (thisM > 59) {
      thisM = 0;
      thisH++;
    }
    if (thisS < 0) {
      if (thisM > 0) {
        thisS = 59;
        thisM--;
      } else thisS = 0;
    }
    if (thisM < 0) {
      if (thisH > 0) {
        thisM = 59;
        thisH--;
      } else thisM = 0;
    }
    if (thisH < 0) thisH = 0;
    if (current_set < 4) period_time[current_pump] = hms_to_s();
    else pumping_time[current_pump] = hms_to_s();
  }
}

// вывести название помп
void drawLabels() {
  lcd.setCursor(1, 0);
  lcd.print("                ");
  lcd.setCursor(1, 0);
  lcd.print(relayNames[current_pump]);
}

// изменение позиции стрелки и вывод данных
void changeSet() {
  switch (current_set) {
    case 0: drawArrow(0, 0); update_EEPROM();
      break;
    case 1: drawArrow(7, 1);
      break;
    case 2: drawArrow(10, 1);
      break;
    case 3: drawArrow(13, 1);
      break;
    case 4: drawArrow(7, 1);
      break;
    case 5: drawArrow(10, 1);
      break;
    case 6: drawArrow(13, 1);
      break;
  }
  lcd.setCursor(0, 1);
  if (current_set < 4) {
    lcd.print("Пауза ");
    s_to_hms(period_time[current_pump]);
  }
  else {
    lcd.print("Работа");
    s_to_hms(pumping_time[current_pump]);
  }
  lcd.setCursor(8, 1);
  if (thisH < 10) lcd.print(0, DEC);
  lcd.print(thisH, DEC);
  lcd.setCursor(11, 1);
  if (thisM < 10) lcd.print(0, DEC);
  lcd.print(thisM, DEC);
  lcd.setCursor(14, 1);
  if (thisS < 10) lcd.print(0, DEC);
  lcd.print(thisS, DEC);
}

// перевод секунд в ЧЧ:ММ:СС
void s_to_hms(uint32_t period) {
  thisH = floor((long)period / 3600);                 // секунды в часы (округляем вниз с помощью floor)
  thisM = floor((period - (long)thisH * 3600) / 60);
  thisS = period - (long)thisH * 3600 - thisM * 60;
}

// перевод ЧЧ:ММ:СС в секунды
uint32_t hms_to_s() {
  return ((long)thisH * 3600 + thisM * 60 + thisS);
}

// отрисовка стрелки и двоеточий
void drawArrow(byte col, byte row) {
  lcd.setCursor(0, 0); lcd.print(" ");
  lcd.setCursor(7, 1); lcd.print(" ");
  lcd.setCursor(10, 1); lcd.print(":");
  lcd.setCursor(13, 1); lcd.print(":");
  lcd.setCursor(col, row); lcd.write(126);
}

// обновляем данные в памяти
void update_EEPROM() {
  EEPROM.updateLong(8 * current_pump, period_time[current_pump]);
  EEPROM.updateLong(8 * current_pump + 4, pumping_time[current_pump]);
}

void backlTick() {
  if(backlFlag && millis() - backlTmr >= BACKL_TOUT * 1000) {
    lcd.setBacklight(0);
    backlFlag = false;
  }
}

void backlOn() {
  backlFlag = true;
  backlTmr = millis();
  lcd.backlight();
}

