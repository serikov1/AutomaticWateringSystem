#define _LCD_TYPE 1
#include <LCD_1602_RUS_ALL.h>
#include <EEPROMex.h>
#include <EEPROMVar.h>
#include <EncButton.h>

#define PUPM_AMOUNT 2      // количество помп, подключенных через мосфеты
#define START_PIN 3        // стартовый пин первого мосфета
#define SWITCH_LEVEL 1     // мосфет - высокий уровень
#define PARALLEL 0         // 1 - параллельный полив, 0 - полив в порядке очереди

LCD_1602_RUS lcd(0x27, 16, 2);
EncButton<EB_TICK, 4> button;

static const wchar_t *relayNames[]  = {
  L"Куст 1",
  L"Куст 2",
};

unsigned long pump_timers[PUPM_AMOUNT];
unsigned int pumping_time[PUPM_AMOUNT];
unsigned int period_time[PUPM_AMOUNT];
unsigned int time_left[PUPM_AMOUNT];
boolean pump_state[PUPM_AMOUNT];
byte pump_pins[PUPM_AMOUNT];

byte current_set = 2;
byte current_pump;
boolean reDraw_flag, arrow_update;
boolean now_pumping;
unsigned long period_coef, pumping_coef;

void setup() {
// --------------------- КОНФИГУРИРУЕМ ПИНЫ ---------------------
  for (byte i = 0; i < PUPM_AMOUNT; i++)              // пробегаем по всем помпам
  {                                                    
    pump_pins[i] = START_PIN + i;                     // настраиваем массив пинов
    pinMode(START_PIN + i, OUTPUT);                   // настраиваем пины
    digitalWrite(START_PIN + i, !SWITCH_LEVEL);                   // выключаем пины
  }

// --------------------- НАСТРОЙКА ДИСПЛЕЯ ------------------------
  lcd.init();
  lcd.backlight();
  lcd.clear();

// --------------------- НАЧАЛЬНЫЕ НАСТРОЙКИ ----------------------
  period_coef = (long)1000 * 60;
  pumping_coef = 1000;

  // запись в энергонезависимую память
  if (EEPROM.read(100) != 1) {
    EEPROM.writeByte(100, 1);

    // для порядку сделаем 1 ячейки с 0 по 99
    for (byte i = 0; i < 100; i++) {
      EEPROM.writeByte(i, 1);
    }
  }

  for (byte i = 0; i < PUPM_AMOUNT; i++) {            // пробегаем по всем помпам
    period_time[i] = EEPROM.readByte(2 * i);          // читаем данные из памяти. На чётных - период (ч)
    pumping_time[i] = EEPROM.readByte(2 * i + 1);     // на нечётных - полив (с)
  }

// -------------------- ВЫВОД НА ДИСПЛЕЙ --------------------------
  lcd.setCursor(1, 0);
  lcd.print(relayNames[0]);
  lcd.setCursor(1, 1);
  lcd.print("Prd: ");
  lcd.setCursor(10, 1);
  lcd.print("t");
  lcd.print(": ");

  arrow_update = true;        // флаг на обновление стрелочки
}

void loop() {
  periodTick();
}

void periodTick() {
for (byte i = 0; i < PUPM_AMOUNT; i++) {            // по всем помпам
    if ( (millis() - pump_timers[i] > ( (long)period_time[i] * period_coef) )
         && (pump_state[i] != SWITCH_LEVEL)
         && !(now_pumping * !PARALLEL)) {
      pump_state[i] = SWITCH_LEVEL;
      digitalWrite(pump_pins[i], SWITCH_LEVEL);
      pump_timers[i] = millis();
      now_pumping = true;
    }
  }
}

