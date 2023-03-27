#define _LCD_TYPE 1
#include <LCD_1602_RUS_ALL.h>
#include <EEPROMex.h>
#include <EEPROMVar.h>
#include <EncButton.h>
#include <Encoder.h>

#define PUPM_AMOUNT 2      // количество помп, подключенных через мосфеты
#define START_PIN 7        // стартовый пин первого мосфета
#define SWITCH_LEVEL 1     // мосфет - высокий уровень
#define PARALLEL 0         // 1 - параллельный полив, 0 - полив в порядке очереди
#define CLK 17             // пин CLOCK энкодера
#define DT 16              // пин DATA энкодера
#define SW 15              // пин кнопки энкодера

LCD_1602_RUS lcd(0x27, 16, 2);
EncButton<EB_TICK, 9> buttonON;
Encoder encoder(CLK, DT, SW);

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
boolean arrow_update;
boolean now_pumping;
unsigned long period_coef, pumping_coef;

void setup() {
// --------------------- КОНФИГУРИРУЕМ ПИНЫ ---------------------
  for (byte i = 0; i < PUPM_AMOUNT; i++)              // пробегаем по всем помпам
  {                                                    
    pump_pins[i] = START_PIN + i;                     // настраиваем массив пинов
    pinMode(START_PIN + i, OUTPUT);                   // настраиваем пины
    digitalWrite(START_PIN + i, !SWITCH_LEVEL);       // выключаем пины
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
  lcd.print("t: ");

  arrow_update = true;        // флаг на обновление стрелочки

  reDraw();                   // вывести на дисплей
  reDraw();                   // вывести на дисплей (флаг стрелок сбросился, выведем числа)
}

void loop() {
  buttonTick();
  periodTick();
  encoderTick();
  flowTick();
}

void buttonTick() {
  buttonON.tick();

  if(buttonON.hasClicks(1) && !arrow_update) {
    lcd.setBacklight(0);
  }
  if(buttonON.hasClicks(2) && !arrow_update) {
    lcd.backlight();
  }
}

void reDraw() {
  if (arrow_update) {                                     // если изменился режим выбора
    if (++current_set > 2)                                // менять current_set в пределах 0.. 2
      current_set = 0;
    if (current_set == 0) update_EEPROM();                // если переключилиссь на выбор помпы, обновить данные
    switch (current_set) {                                // смотрим, какая опция сейчас выбрана
      case 0:                                             // если номер помпы
        encoder.setCounterNorm(current_pump);             // говорим энкодеру работать с номером помпы
        encoder.setLimitsNorm(0, PUPM_AMOUNT - 1);        // ограничиваем
        // стереть предыдущую стрелочку и нарисовать новую
        lcd.setCursor(0, 0); lcd.write(126); lcd.setCursor(9, 1); lcd.print(" ");
        break;
      case 1:
        encoder.setCounterNorm(period_time[current_pump]);
        encoder.setLimitsNorm(1, 99);
        lcd.setCursor(0, 1); lcd.write(126); lcd.setCursor(0, 0); lcd.print(" ");
        break;
      case 2:
        encoder.setCounterNorm(pumping_time[current_pump]);
        encoder.setLimitsNorm(1, 99);
        lcd.setCursor(9, 1); lcd.write(126); lcd.setCursor(0, 1); lcd.print(" ");
        break;
    }
    arrow_update = false;
  } else {
    // вывести все цифровые значения
    if (current_set == 0) {
      lcd.setCursor(1, 0);
      lcd.print("              ");
      lcd.setCursor(1, 0);
      lcd.print(relayNames[current_pump]);
    }
    lcd.setCursor(5, 1);
    lcd.print(period_time[current_pump], DEC);
    lcd.print("h ");
    lcd.setCursor(12, 1);
    lcd.print(pumping_time[current_pump], DEC);
    lcd.print("s ");
  }
}

void periodTick() {
for (byte i = 0; i < PUPM_AMOUNT; i++) {                                        // по всем помпам
    if ( (millis() - pump_timers[i] > ( (long)period_time[i] * period_coef) )   // проверяем пришло ли вермя включения
         && (pump_state[i] != SWITCH_LEVEL)                                     // если помпа выключена
         && !(now_pumping * !PARALLEL)) {                                       // и другая помпа не работает (для непараллельного режима работы) 
      pump_state[i] = SWITCH_LEVEL;                                             
      digitalWrite(pump_pins[i], SWITCH_LEVEL);                                 // включаем помпу
      pump_timers[i] = millis();
      now_pumping = true;
    }
  }
}

void flowTick() {
  for (byte i = 0; i < PUPM_AMOUNT; i++) {                                      // по всем помпам
    if ( (millis() - pump_timers[i] > ( (long)pumping_time[i] * pumping_coef) ) // проверяем пришло ли время выключения
         && (pump_state[i] == SWITCH_LEVEL) ) {                                 // если помпа включена
      pump_state[i] = !SWITCH_LEVEL; 
      digitalWrite(pump_pins[i], !SWITCH_LEVEL);                                // выключаем помпу                              
      now_pumping = false;
    }
  }
}

// обновляем данные в энергонезависимой памяти
void update_EEPROM() {
  EEPROM.updateByte(2 * current_pump, period_time[current_pump]);
  EEPROM.updateByte(2 * current_pump + 1, pumping_time[current_pump]);
}

void encoderTick() {
  encoder.tick();                  // отработка энкодера

  if (encoder.isRelease()) {       // если был нажат
    arrow_update = true;           // флаг на обновление стрелочки
    reDraw();                      // обновить дисплей
  }

  if (encoder.isTurn()) {                                   // если был совершён поворот
    switch (current_set) {                               // смотрим, какая опция сейчас меняется
      case 0:                                            // если номер помпы
        current_pump = encoder.normCount;                // получить значение с энкодера
        break;
      case 1:                                            // если период работы помпы
        period_time[current_pump] = encoder.normCount;   // получить значение с энкодера
        break;
      case 2:                                            // если время работы помпы
        pumping_time[current_pump] = encoder.normCount;     // получить значение с энкодера
        break;
    }
    reDraw();                                            // обновить дисплей
  }
}

