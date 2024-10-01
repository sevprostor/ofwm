#include <EEPROM.h>
#include <Arduino.h>
#include <Tachometer.h>
#include <NecDecoder.h>
#include <Motor.h>


// между 0 - 10000мкс или 10мс при 50гц. 

#define ZERO_PIN 2    // пин детектора нуля
#define INT_NUM 0     // соответствующий ему номер прерывания
#define DIMMER_PIN 4  // управляющий пин симистора
int dimmer;           // переменная диммера
int minPower = 0;

Motor motor;
int Motor::power = 0;
int Motor::actualSpeed = 0;
bool Motor::reversed = false;


Tachometer tacho;
Tachometer acFreq;
NecDecoder ir;


volatile unsigned long timer_1, timer_2, timer_3, ovh;
int speed = 0;
int state = 0;
bool working = false;
int emp = 810, sw = 830, lw = 870, mw = 890, hw = 915, water = 0;
int shakes[] = {80, 50, 20, 10}; //стирка и полоскания
int tryes = 11, trs = 0; //попытки отжима
bool pause = false, error = false, cancel = false, firstime = true, overheat = false, confirmed = false;

int enabledSpread = 65; //60 допустимый разброс скоростей для отжима


#define IR_1    0xA2
#define IR_2    0x62
#define IR_3    0xE2
#define IR_4    0x22
#define IR_5    0x2
#define IR_6    0xC2
#define IR_7    0xE0
#define IR_8    0xA8
#define IR_9    0x90
#define IR_STAR 0x68
#define IR_0    0x98
#define IR_HASH 0xB0
#define IR_UP   0x18
#define IR_LEFT 0x10
#define IR_OK   0x38
#define IR_RIGHT 0x5A
#define IR_DOWN 0x4A



void irIsr() {
  ir.tick();
}

bool confirmation(){
  uint32_t now = millis();
  Serial.println("Are you sure?");

  while(millis() - now < 5000 && !confirmed){
    if (ir.available() && ir.readCommand() == IR_OK) confirmed = true; 
    tone(13, 500, 100);
  }

  if(confirmed) return true;

  return false;
}


// прерывание детектора нуля
void isr() {
  if(pause) Motor::power = 0;
  motor.InZero();
}

void flooding(int wLevel){
  int psDuration = pulseIn(11, HIGH);
  Serial.println("Filling drum...");
  //Serial.println(psDuration);
  water = psDuration;
  delay(500);


  
    static int t = 0;
    static int vals[20];
    static int average = 0;
    
digitalWrite(5, HIGH); //кран
      delay(100);
      digitalWrite(8, HIGH); //реле питания кран/помпа
delay(100);

  if(wLevel < psDuration) wLevel = psDuration;


  Serial.println(wLevel);
  Serial.println(water);
  Serial.println(psDuration);

       

  while(psDuration < wLevel && wLevel < 920){
      //motor.controlCall(); // ***rControl(); перенести в отдельный класс; // ***rControl(); перенести в отдельный класс
      
      
      int sum = 0;                                  // локальная переменная sum
      for (int i = 0; i < 5; i++) {      // согласно количеству усреднений
        sum += pulseIn(11, HIGH);
        delay(100);                        // суммируем значения с любого датчика в переменную sum
      }
      psDuration = sum / 5;

      Serial.println(psDuration);
    }
  
  //delay(500);

    Serial.println("Drum filled");
    digitalWrite(8, LOW); //реле кран/помпа
    delay(100);
    digitalWrite(5, LOW); //кран
    delay(100);
    EEPROM.update(4, psDuration);

    //water = psDuration;
 
  
}

bool probeSpread(int s){
  int msum = 0;
  for (int i = 0; i < 5; i++)
  {
    int m = motor.SpinTest(s);
    msum = msum + m;
    
    //Serial.println(Motor::actualSpeed);
    //if(m > 50)
    //  return false;
  }

  Serial.println(msum);

  if(msum > enabledSpread)
    return false;
  else
    return true;         
}

void washing(){ 
  uint32_t now = millis();
  
  int h = 0;
  Motor::power = minPower;
  int shakeDur;
  int pauseDur;
  bool hh = false;

  if(minPower > 35){
    shakeDur = 6000;
    pauseDur = 12000;
  }else{
    shakeDur = 6000;
    pauseDur = 8000;

  }
  while(millis() - now < shakeDur && !pause){
    motor.controlCall(); // ***rControl(); перенести в отдельный класс; // ***// ***rControl(); перенести в отдельный класс перенести в отдельный класс
    motor.Spin(speed);
    //hh = h;
    if(Motor::actualSpeed == 0){
    
      //Нет ли перегрева? Крутится ли барабан?
      //Иногда выпадают нулевые скорости. Поэтому мы делаем много измерений. Если в течение 4c скорость 0,
      //то можно считать, что барабан точно стоит при поданной мощности. Определенно, сработала термозащита мотора.

      
      if(hh) h++;   
      
      if(h > 8) overheat = true;

      hh = true;
      //pause = true;
      //hguard = millis();
    }else{
      hh = false;
    }
    
    
  }
  //hguard = millis();
  h = 0;
  Motor::power = 0;
  motor.Reverse();
  now = millis();
  while(millis() - now < pauseDur && !pause){
    motor.controlCall(); // ***rControl(); перенести в отдельный класс; // ***rControl(); перенести в отдельный класс
  }
}



void machineResting(uint32_t timeout, uint32_t restTime = 600000){
  uint32_t now = millis();
  //Serial.println("Pause");

  while(overheat){
    Motor::power = 0;
    motor.controlCall(); // ***rControl(); перенести в отдельный класс; // ***rControl(); перенести в отдельный класс
    if(millis() - now > 5000){
      tone(13, 400, 100);
      now = millis();
      Serial.println("Too hard to work, machine resing for a while...");
    }
    if(millis() - timeout > restTime){
      overheat = false;
    }
  }
}

void onError(){
  uint32_t now = millis();
  Serial.println("Error");

  while(pause){
    motor.controlCall(); // ***rControl(); перенести в отдельный класс; // ***rControl(); перенести в отдельный класс
    if(millis() - now > 2000){
      tone(13, 300, 1000);
      now = millis();
    }

  }
}

void onPause(){
  uint32_t now = millis();
  Serial.println("Pause");

  while(pause){
    motor.controlCall(); // ***rControl(); перенести в отдельный класс; // ***rControl(); перенести в отдельный класс
    if(millis() - now > 2000){
      tone(13, 300, 300);
      now = millis();
    }

  }
}

void rControl(){
  if (ir.available()) {
    tone(13, 500, 100);
    switch (ir.readCommand()) {
      case IR_0:
      
        if(pause) {
          confirmed = false; //требует подтверждения
          if(confirmation()){
            state = 0;
            EEPROM.update(0, 0);
            EEPROM.update(2, 0);
            pause = !pause;
          }
        }

      break;

      case IR_1:
        if(state == 0){
          confirmed = false; //требует подтверждения
          if(confirmation()) state = 1;
        } 
      break;

      case IR_9:
        if(state == 0){ 
          confirmed = false; //требует подтверждения
          if(confirmation()) state = 9;
        }
      break;

      case IR_STAR:
        if(pause){
            flooding(water + 10);
          }
      break;

      case IR_HASH:
        if(pause){
          digitalWrite(5, LOW); //помпа
          delay(100);
          digitalWrite(8, HIGH); //реле питания кран/помпа
          delay(5000);
          digitalWrite(8, LOW); //реле кран/помпа 
      }
      break;

      case IR_UP:
        speed = speed + 2;
        //if(speed < 10) speed = 10;
        if(speed > 400) speed = 400;
          //minPower = motor.GetLoad();
          Serial.println("Target speed");
          Serial.println(speed);
          
          //state = 9;
      break;

      case IR_DOWN:
        speed = speed - 2;
        if(speed < 0) speed = 0;
        Serial.println("Target speed");
        Serial.println(speed);

      break;

      case IR_RIGHT:

      //пропуск шага

          if(error){
            error = !error;
            pause = false; 
          }
          if(pause && !overheat){
            confirmed = false; //требует подтверждения
            if(confirmation()){
              cancel = true;
              pause = !pause;
            }
            
          }
          
      break;

      case IR_LEFT:
      
      break;

      case IR_OK:
        
        if(!overheat) pause = !pause;
      
        
        //if(working) working = !working;
      break;
    }
  }
}

void setup() {
  Serial.begin(9600);

  
  pinMode(2, INPUT_PULLUP); //zero detector
  pinMode(4, OUTPUT); //реле пустое
  pinMode(5, OUTPUT); //кран или помпа
  pinMode(6, OUTPUT); //реверс
  pinMode(7, OUTPUT); //реверс

  pinMode(8, OUTPUT); //питание помпы и крана

  pinMode(10, OUTPUT); //диммер мотора
  digitalWrite(4, LOW);
  digitalWrite(5, LOW);
  digitalWrite(6, LOW);
  digitalWrite(7, LOW);
  digitalWrite(10, LOW);
  digitalWrite(8, LOW);
  
  attachInterrupt(0, isr, RISING);  // детектор нуля
  attachInterrupt(1, irIsr, FALLING); // пульт

  tone(13, 300, 1000);

  motor.controlAttach(rControl);
  //minPower = motor.GetLoad();
  //motor.Reverse();

  Serial.println("Ozadchanka starting");

  //грузимся после креша
  
  int shft;
  
  int oldstate = EEPROM.read(0);
  int oldshakes = EEPROM.read(2);
  //int oldmpower = EEPROM.read(4);
  

  if(oldstate > 2 && oldstate < 9){ 
    
    //если машина стирала
    speed = EEPROM.read(6);
    water = EEPROM.read(4);
    shft = oldstate - 3;
    state = oldstate;
    //minPower = oldmpower;
    shakes[shft] = shakes[shft] - oldshakes;
    delay(100);
    //water = pulseIn(11, HIGH);

    Serial.println("Recovering after crash:");
    Serial.println(state);
    Serial.println(water);
    Serial.println(shakes[shft]);
    Serial.println("***********************");
  
  }

}


void loop() {

  int shft = 0;
  int ishak = 0;
  

  motor.controlCall(); // ***rControl(); перенести в отдельный класс; // ***rControl(); перенести в отдельный класс
  if(error) onError();
  if(pause) onPause();
  
    else{
    switch (state) {
      case 0:
        
        Motor::power = 0;
        working = false;
        pause = false;

      break;

      case 1: //залив воды
      //заливаем в 2 этапа. 1 - заливаем мало, крутим барабан. 
      //Если power < 35 то ок, 35...40 - средний, выше 45 - высокий
      //working = true;
      flooding(lw); //низкая вода
      washing();

      for(int i = 0; i < 2; i++){
        minPower = motor.GetLoad();
        //water = lw;
        if(minPower > 30 && minPower < 40){
          flooding(mw);
          //water = mw;
        }

        if(minPower > 40){
          flooding(hw);
          //water = hw;
        }
      }
      

      //Serial.println("Saving minPower");

      //EEPROM.update(4, minPower);
      
      
      //working = true;
      state = 3;
      break;

      case 3: //стирка
      case 4: //полоскание 1
      case 5: //полоскание 2
      case 6: //полоскание 3
        Serial.println("Saving state");
        EEPROM.update(0, state);

        working = true;
        //water =  EEPROM.read(6);

        flooding(water);

        shft = state - 3;
        //speed = EEPROM.read(6);
        if(speed == 0 || speed > 40) speed = 25;

        minPower = motor.GetLoad();

        cancel = false;       
        for (int i = 0; i < shakes[shft]; i++){
          ishak++;

          if(pause) onPause();
          if(overheat) machineResting(millis(), 1200000);

          if(state == 0 || cancel){
            i = shakes[shft];
            cancel = false;
          }

          if(ishak == 5){ // каждые 5 циклов сохраним
            Serial.println("Saving shakes");
            
            ishak = 0;
            int shs = i;
            EEPROM.update(2, shs);
            EEPROM.update(6, speed);
          }
          
          washing();

          if(i == 6){ //днократный долив
            flooding(water);
            minPower = motor.GetLoad();
          }
        }

        Serial.println("Saving shakes");
        EEPROM.update(2, 0);
          
        working = false;

        //state = state + 7;

         switch(state){
          case 3:
          state = 10;
          break;
          case 4:
          state = 11;
          break;
          case 5:
          state = 12;
          break;
          case 6:
          state = 13;
          break;

        } 
      break;

      case 7:
      case 9:
      case 10:
      case 11:
      case 12:
      case 13:


        
        // Перед отжимом измеряем биения скорости.
        // Сначала плавно, осторожно раскручиваем барабан до 60. 
        // Необходимо, чтобы в процессе тряпки распределились по барабану ровным слоем
        // При скорости 60 начинаем мерить.
        speed = 40;
        
        delay(1000);
        int psDuration = pulseIn(11, HIGH);
        Serial.println("level");
        Serial.println(psDuration);
        Serial.println("target level");
        Serial.println(sw);
        delay(100);
        error = false;

        



        if(psDuration > sw){
          Serial.println("Dumping water to low level");
          delay(100);
          digitalWrite(5, LOW); //помпа
          delay(100);
          digitalWrite(8, HIGH); //реле питания кран/помпа
          delay(500);
        }

        while(psDuration > sw){
          psDuration = pulseIn(11, HIGH);
          delay(500);

        }
        digitalWrite(8, LOW); //реле кран/помпа
        delay(200);

        if(firstime){
          minPower = motor.GetLoad();
          firstime = false;
          }

        if(!working){
          working = true;
          /*  */
          Serial.println("Starting up motor and dumping water...");
          
          Motor::power = minPower;
          timer_1 = millis();
          while(millis() - timer_1 < 5000 && !pause){ //Слив воды
            motor.controlCall(); // ***rControl(); перенести в отдельный класс; // ***rControl(); перенести в отдельный класс
            motor.Spin(speed, 0, false);          
          }
          
          
          /* delay(200);
          digitalWrite(8, LOW); //реле кран/помпа
          delay(200); */
        }

        //Начинаем измерять. Проводим 5 измерений, считаем сумму разбросов. Если она меньше 60, то прибавляем 10, и так до 100.
        //Если все успешно, то после 100 просто крутим на определенной скорости.

        bool success = true;
        speed = 60;
        int ms = 100;
        

        if(minPower > 45) ms = 150; //допустимые скорости для разных нагрузок
        else ms = 200;

        while(success && speed < 100 && !pause){
          motor.controlCall(); // ***rControl(); перенести в отдельный класс; // ***rControl(); перенести в отдельный класс
          timer_3 = millis();
          while(millis() - timer_3 < 4000){
            motor.Spin(speed, 0, false);
          }

          if(Motor::actualSpeed < speed) success = false; //мотор не раскрутился, барабан сильно бьет, отбой

          success = probeSpread(speed);
          if(success){
            
            speed = speed + 20;

            timer_3 = millis();
            while(millis() - timer_3 < 2000){
              motor.Spin(speed, 0, false);
            }

            Serial.println("Increasing speed");
          }
        }

        if(!success){ //пробуем еще раз
          //state = 9;
          trs++;
          error = true;
          working = false;
          Motor::power = 0;
          Serial.println("Spining failed");
          delay(5000);
          motor.Reverse();
          if(trs == tryes){
            //error = true;
            pause = true;
            trs = 0;
                       
          }
        }else{
          Serial.println("Spining in progress...");
          speed = ms;

          digitalWrite(5, LOW); //помпа
          delay(100);
          digitalWrite(8, HIGH); //реле питания кран/помпа

          timer_1 = millis();
          while(millis() - timer_1 < 120000 && !pause){
            motor.controlCall(); // ***rControl(); перенести в отдельный класс; // ***rControl(); перенести в отдельный класс
            motor.Spin(speed);
          }

          Motor::power = 0;
          firstime = true;
          trs = 0;
          delay(100);
          digitalWrite(8, LOW); //реле кран/помпа
        }
        working = false;
        if(!error){
          Serial.println("Spining complete! Going to next step.");
          overheat = true;
          machineResting(millis(), 30000);
          //delay(10000);
          motor.GetSpeed(true);
        }

        if(!error){
          switch(state){
          case 9:
          state = 0;
          break;
          case 10:
          state = 4;
          case 11:
          state = 5;
          break;
          case 12:
          state = 6;
          break;
          case 13:
          EEPROM.update(0, 0);
          EEPROM.update(2, 0);
          
          state = 0;
          break;
          }
        }

      break;

     
    }
  }
}
  

 




