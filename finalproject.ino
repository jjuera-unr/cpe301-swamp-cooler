// CPE 301
// Final Project

#include <LiquidCrystal.h>
#include <DHT.h>
#include <Wire.h>
#include <RTClib.h>
#include <Stepper.h>

/** Register definitions **/

volatile unsigned char *PORT_B  = (unsigned char *) 0x25;
volatile unsigned char *DDR_B   = (unsigned char *) 0x24;
volatile unsigned char *PIN_B   = (unsigned char *) 0x23;

volatile unsigned char *PORT_C  = (unsigned char *) 0x28;
volatile unsigned char *DDR_C   = (unsigned char *) 0x27;
volatile unsigned char *PIN_C   = (unsigned char *) 0x26;

volatile unsigned char *myUCSR0A  = (unsigned char *) 0x00C0;
volatile unsigned char *myUCSR0B  = (unsigned char *) 0x00C1;
volatile unsigned char *myUCSR0C  = (unsigned char *) 0x00C2;
volatile unsigned int  *myUBRR0   = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0    = (unsigned char *) 0x00C6;

// ADC registers
volatile unsigned char *my_ADMUX = (unsigned char *) 0x7C;
volatile unsigned char *my_ADCSRB = (unsigned char *) 0x7B;
volatile unsigned char *my_ADCSRA = (unsigned char *) 0x7A;
volatile unsigned int *my_ADC_DATA = (unsigned int *) 0x78;

// Timer registers (for my_delay)
volatile unsigned char *myTCCR1A = (unsigned char *) 0x80;
volatile unsigned char *myTCCR1B = (unsigned char *) 0x81;
volatile unsigned char *myTCCR1C = (unsigned char *) 0x82;
volatile unsigned int *myTCNT1 = (unsigned int *) 0x84;
volatile unsigned char *myTIFR1 = (unsigned char *) 0x36;

volatile unsigned char *myPCICR   = (unsigned char *) 0x68;
volatile unsigned char *myPCMSK1  = (unsigned char *) 0x6C;

volatile bool startPressed = false;

/**
 * Helper structs for input and output pins
 */

struct Pin {
  volatile unsigned char *pin;
  volatile unsigned char *ddr;
  int number;

  Pin(volatile unsigned char *pin, volatile unsigned char *ddr, int number) {
    this->number = number;
    this->ddr = ddr;
    this->pin = pin;
  }

  void init(bool output) {
    if (output) {
      *ddr |= (1 << number);
    } else {
      *ddr &= ~(1 << number);
    }
  }

  void set(bool value) {
    if (value) {
      *pin |= (1 << number);
    } else {
      *pin &= ~(1 << number);
    }
  }

  bool get() {
    return (*pin & (1 << number)) != 0;
  }
};

DHT dht(49, DHT11);
LiquidCrystal lcd(28, 29, 26, 27, 24, 25);

const int STEPS_PER_REVOLUTION = 2048;
//Stepper stepper(STEPS_PER_REVOLUTION, 8, 9, 10, 11);
// change the pinout of 8 9 10 11 -> 8 10 9 11 because of this:
// https://www.reddit.com/r/arduino/comments/y95cfz/programming_a_28byj48_step_motor_it_seems_to/
// in addition, using 256 instead of 2048 makes the stepper rotate better for
// some reason, even though 2048 is the correct value
Stepper stepper(256, 8, 10, 9, 11);

// LEDs connected to pins 50-53
Pin redLed(PORT_B, DDR_B, 3);
Pin yellowLed(PORT_B, DDR_B, 2);
Pin greenLed(PORT_B, DDR_B, 1);
Pin blueLed(PORT_B, DDR_B, 0);

// buttons connected to pins 37, 35, 33
Pin startButton(PIN_C, DDR_C, 0);
Pin resetButton(PIN_C, DDR_C, 2);
Pin ventButton(PIN_C, DDR_C, 4);

float temperature;
float temperatureThreshold = 25;
int humidity;
int waterLevel;
int waterLevelThreshold = 120;
int ventStep = STEPS_PER_REVOLUTION / 8; // 45 degree vent movement

RTC_DS1307 rtc;

// 1 minute delay for LCD update
unsigned long lastUpdate = 0;
unsigned long updateInterval = 1000 * 5;

/**
 * State machine states
 */
typedef enum _State {
  IDLE,
  RUNNING,
  DISABLED,
  ERROR
} State;

State currentState = IDLE;

void setup() {
  //Serial.begin(9600);
  U0init(9600);
  Wire.begin();
  adc_init();

  dht.begin();
  lcd.begin(16, 2);
  lcd.print("System Initializing");

  redLed.init(true);
  yellowLed.init(true);
  greenLed.init(true);
  blueLed.init(true);

  startButton.init(false);
  resetButton.init(false);
  ventButton.init(false);

  if (!rtc.begin()) {
    U0putstr("Couldn't find RTC\n");
    while (1);
  }

  DateTime dt = DateTime(F(__DATE__), F(__TIME__));
  rtc.adjust(dt);
  U0puttimestamp(dt);
  U0putstr("RTC time adjusted.\n");

  stepper.setSpeed(60);

  // interrupt
	PCICR  |=  (1 << PCIE1);
	PCMSK1 |=  (1 << PCINT10);
}

void loop() {
  my_delay(16384, 5);

  waterLevel = adc_read(0);
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  loopState();
}

void enterState(State newState) {
  // print state change
  U0puttimestamp(rtc.now());
  U0putstr(" STATE CHANGE: ");
  U0putstate(currentState);
  U0putstr(" -> ");
  U0putstate(newState);
  U0putchar('\n');

  currentState = newState;

  // update LEDs
  redLed.set(false);
  yellowLed.set(false);
  greenLed.set(false);
  blueLed.set(false);

  switch (currentState) {
    case IDLE:
      stopMotor();
      greenLed.set(true);
      break;
    case RUNNING:
      startMotor();
      blueLed.set(true);
      break;
    case DISABLED:
      stopMotor();
      lcd.clear();
      yellowLed.set(true);
      break;
    case ERROR:
      stopMotor();
      redLed.set(true);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ERR: water level");
      lcd.setCursor(0, 1);
      lcd.print("below threshold");
      break;
  }
}

void loopState() {
  // button presses to change states are instead handled in the ISR
  switch (currentState) {
    case IDLE:
      if (startButton.get()) {
        enterState(DISABLED);
        break;
      }

      // if water level is below threshold, go to ERROR
      if (waterLevel < waterLevelThreshold) {
        enterState(ERROR);
        break;
      }

      // if temperature exceeds threshold, start running the motor
      if (temperature >= temperatureThreshold) {
        enterState(RUNNING);
      }

      if (ventButton.get()) {
        toggleVent();
      }

      updateLCD();

      break;

    case RUNNING:
      if (startButton.get()) {
        enterState(DISABLED);
        break;
      }

      // if water level is below threshold, go to ERROR
      if (waterLevel < waterLevelThreshold) {
        enterState(ERROR);
        break;
      }

      // if temperature drops below threshold, stop running
      if (temperature < temperatureThreshold) {
        enterState(IDLE);
        break;
      }

      if (ventButton.get()) {
        toggleVent();
      }

      updateLCD();

      break;

    case DISABLED:
      // button press to re-enable is handled in ISR
      if (startButton.get()) {
        enterState(IDLE);
        break;
      }
      break;

    case ERROR:
      if (startButton.get()) {
        enterState(DISABLED);
        break;
      }

      // if reset button is pressed, go back to IDLE
      if (resetButton.get()) {
        lastUpdate = 0; // force LCD update
        enterState(IDLE);
        break;
      }

      if (ventButton.get()) {
        toggleVent();
      }

      break;
  }
}

// forward declare so we can use default value
void U0putint(int, int = 0);

void toggleVent() {
  stepper.step(ventStep);
  U0puttimestamp(rtc.now());
  U0putstr("Moving vent by ");
  U0putint(ventStep);
  U0putstr(" step(s)\n");
  ventStep = -ventStep; // toggle between opening and closing
}

void stopMotor() {
  // implement stop motor logic
}

void startMotor() {
  // start motor
}

/** ISR for handling button presses **/
ISR(PCINT1_vect) {
  if (startButton.get()) {
    if (currentState == DISABLED) {
      enterState(IDLE);
    }
  }
}

/** UART functions **/

void U0init(int U0baud) {
  unsigned long FCPU = 16000000;
  unsigned int tbaud;
  tbaud = (FCPU / 16 / U0baud - 1);
  // Same as (FCPU / (16 * U0baud)) - 1;
  *myUCSR0A = 0x20;
  *myUCSR0B = 0x18;
  *myUCSR0C = 0x06;
  *myUBRR0  = tbaud;
}

unsigned char U0getchar() {
  return *myUDR0;
}

void U0putchar(unsigned char data) {
  while (!(*myUCSR0A & 0x20));
  *myUDR0 = data;
}

void U0putstr(char *s) {
  while (*s != '\0') {
    U0putchar(*s++);
  }
}

void U0putint(int value, int leadingZeros) {
  // render negative numbers correctly by prepending negative sign and printing
  // as positive
  if (value < 0) {
    U0putchar('-');
    value = -value;
  }

  const int digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
  int temp = value;
  int count = 0;

  // count number of digits for leading zeros
  if (temp == 0) {
      count = 1;
  } else {
      while (temp > 0) {
          temp /= 10;
          count++;
      }
  }

  while (leadingZeros > count) {
      U0putchar('0');
      leadingZeros--;
  }

  if (value >= 10) {
      U0putint(value / 10, 0);
  }

  U0putchar(digits[value % 10]);
}

void U0putstate(State state) {
  switch (state) {
    case IDLE:
      U0putstr("IDLE");
      break;
    case RUNNING:
      U0putstr("RUNNING");
      break;
    case DISABLED:
      U0putstr("DISABLED");
      break;
    case ERROR:
      U0putstr("ERROR");
      break;
  }
}

void U0puttimestamp(DateTime dt) {
  U0putchar('[');
  U0putint(dt.hour(), 2);
  U0putchar(':');
  U0putint(dt.minute(), 2);
  U0putchar(':');
  U0putint(dt.second(), 2);
  U0putchar(']');
  U0putchar(' ');
}

void adc_init() {
  *my_ADCSRA |=  0b10000000; 
  *my_ADCSRA &=  0b11011111; 
  
  *my_ADCSRB &=  0b11110111; 
  *my_ADCSRB &=  0b11111000; 
  
  *my_ADMUX  &=  0b01111111; 
  *my_ADMUX  |=  0b01000000; 
  *my_ADMUX  &=  0b11011111; 
  *my_ADMUX  &=  0b11100000;
}

unsigned int adc_read(unsigned char adc_channel_num) {
  *my_ADMUX  &= 0b11100000;
  *my_ADCSRB &= 0b11110111;

  if (adc_channel_num > 7) {
    adc_channel_num -= 8;
    *my_ADCSRB |= 0b00001000;
  }
  *my_ADMUX += adc_channel_num;
  *my_ADCSRA |= 0x40;

  while ((*my_ADCSRA & 0x40) != 0);

  return *my_ADC_DATA;
}

void my_delay(unsigned short ticks, unsigned char prescaler) {
  *myTCCR1B &= 0xF8;
  *myTCNT1 = (unsigned int) (65536 - ticks); 

  *myTCCR1A = 0x0;
  *myTCCR1B |= (prescaler & 0x07);
  while ((*myTIFR1 & 0x01) == 0);
  *myTCCR1B &= 0xF8;
  *myTIFR1 |= 0x01;
}

void updateLCD() {
  unsigned long currentTime = millis();

  if (currentTime < lastUpdate + updateInterval) {
    return;
  }

  lastUpdate = currentTime;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(temperature);
  lcd.print(" C");
  lcd.setCursor(0, 1);
  lcd.print("Humidty: ");
  lcd.print(humidity);
  lcd.print("%");
}
