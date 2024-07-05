#include <Arduino.h>
#include <GyverHX711.h>
#include <Servo.h>
#include <GyverEncoder.h>
#include <TM1637Display.h>
#include "timer.h"

Timer debTmr(TMR_PERIOD, 500);

#define SERVO_PIN 12
Servo servo;

#define SCALE1_DT 9
#define SCALE1_GCK 8
GyverHX711 scale1(SCALE1_DT, SCALE1_GCK, HX_GAIN64_A);

#define SCALE2_DT 11
#define SCALE2_GCK 10
GyverHX711 scale2(SCALE2_DT, SCALE2_GCK, HX_GAIN64_A);

#define RELAY_PIN A2

#define DISPLAY_CLK 2
#define DISPLAY_DIO 3

TM1637Display display(DISPLAY_CLK, DISPLAY_DIO);

#define ENC_A 6
#define ENC_B 5
#define ENC_KEY 4

#define CALIB 7 // гр. Емкость на платформе должна имееть значение выше указанного

Encoder encoder(ENC_A, ENC_B, ENC_KEY, TYPE2);

bool ready1;
bool ready2;

uint16_t mass1;
uint16_t mass2;

uint16_t glassMass1;
uint16_t glassMass2;

uint16_t setMass = 100;


void restart()
{
  uint8_t data[4] = {0x33, 0x6D, 0x78};
  display.setSegments(data);
  Serial.println("Restart");
  delay(1000);
  asm volatile("jmp 0");
}

void calibMass()
{
  uint8_t data[4] = {0x40, 0x40, 0x40, 0x40};
  display.setSegments(data);
  delay(500);
  Serial.println("Calibration");
  scale1.tare();
  scale2.tare();
  delay(500);
}

void readScale(GyverHX711 &scale, uint16_t &mass)
{
#define COUNTER 100
  for (uint8_t i = 1; i <= COUNTER; i++)
  {
    mass = (mass + scale.read() * 2) / i;
    if (mass > 65000)
    {
      mass = 0;
    }
    delayMicroseconds(10);
  }
}

void setup()
{
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  display.setBrightness(0x0f);
  debTmr.start();
  Serial.begin(9400);
  Serial.println();
  servo.attach(SERVO_PIN);
  servo.write(90);
  calibMass();
}

void loop()
{
  encoder.tick();
  if (encoder.isRight())
  {
    if (setMass < 750)
    {
      setMass += 5;
    }
    else
    {
      setMass = 750;
    }
  }
  encoder.tick();
  if (encoder.isLeft())
  {
    if (setMass > 0)
    {
      setMass -= 5;
    }
    else
    {
      setMass = 0;
    }
  }
  display.showNumberDec(setMass, true);
  readScale(scale1, mass1);
  readScale(scale2, mass2);
  encoder.tick();

  if (debTmr.ready())
  {
    Serial.print("Current set mass ");
    Serial.println(setMass);
    Serial.print("Mass 1 ");
    Serial.println(mass1);
    Serial.print("Mass 2 ");
    Serial.println(mass2);
    Serial.print("Glass Mass 1 ");
    Serial.println(glassMass1);
    Serial.print("Glass Mass 2 ");
    Serial.println(glassMass2);
    Serial.println();
  }
  if (mass1 > 1000 || mass2 > 1000)
  {
    restart();
  }
  encoder.tick();

  if (encoder.isPress())
  { // если нажали на энкодер то выполняется заливка
    Serial.println("START NALIVATOR");

    if (mass1 > CALIB && !ready1 && mass1 + glassMass1 < setMass + glassMass1)
    {
      readScale(scale1, glassMass1);
      ready1 = true;
    }
    if (mass2 > CALIB && !ready2 && mass2 + glassMass2 < setMass + glassMass2)
    {
      readScale(scale2, glassMass2);
      ready2 = true;
    }

    if (mass1 < setMass + glassMass1 && ready1)
    {
      Serial.println("Position 1");
      servo.write(0); // повернуть серву на 1ую позицию
      readScale(scale1, glassMass1);
      delay(1000);
      for (uint16_t i = 0;            // указыаем стартовую массу с стаканом и платформой
           i <= setMass + glassMass1; // проверяем что вес меньше чем вес платформы и вес стакана и заданной массы
           i = mass1)                 // читаем значение при каждом проходе цикла
      {
        digitalWrite(RELAY_PIN, LOW); // включаем насос
        display.showNumberDec(i - glassMass1, true);
        readScale(scale1, mass1);
      }
      // если заполненние кончилось тогда выключаем насос и ставим серву в парковочную позицию

      digitalWrite(RELAY_PIN, HIGH);
      ready1 = false;
      delay(2000);
      servo.write(90);
    }

    // если на 2 месте не пусто и масса стакана меньше чем нужно тогда включить насос
    if (mass2 < setMass + glassMass2 && ready2)
    {
      Serial.println("Position 2");
      servo.write(180); // повернуть серву на 2ую позицию
      readScale(scale2, glassMass2);
      delay(1000);
      for (uint16_t i = 0;            // указыаем стартовую массу с стаканом и платформой
           i <= setMass + glassMass2; // проверяем что вес меньше чем вес платформы и вес стакана и заданной массы
           i = mass2)                 // читаем значение при каждом проходе цикла
      {
        digitalWrite(RELAY_PIN, LOW); // включаем насос
        display.showNumberDec(i - glassMass2, true);
        readScale(scale2, mass2);
      }
      // если заполненние кончилось тогда выключаем насос и ставим серву в парковочную позицию
      digitalWrite(RELAY_PIN, HIGH);
      ready2 = false;
      delay(2000);
      servo.write(90);
    }
  }
}
