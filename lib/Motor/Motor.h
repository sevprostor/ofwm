/*
Библиотека работы с мотором
*/

#pragma once
#include <Arduino.h>

class Motor {
public:
    long SpinTest(int testspeed);
    int Spin(int tspeed, int fork = 0, bool updown = true);
    void InZero();
    int GetSpeed(bool filtered);
    int GetLoad(int c);
    void Reverse();
    static int actualSpeed;
    static int power;
    static int MinPower;
    static int mPower;
    static bool reversed;
    void controlAttach(void (*function)());
    void controlCall();
  private:
    void (*atatchedF)();
    //static int FSpeed;
    //bool startingUp = false;
    //bool paused = false;
    //uint32_t startup = millis();
    //int pwr;
    //int* pwr;
};

void Motor::controlAttach(void (*function)()) {
 atatchedF = *function;  
}

void Motor::controlCall() {
 (*atatchedF)();
}



int filter(int val){
    /*
  Простейший фильтр: запаздывающий, бегущее среднее, "цифровой фильтр", фильтр низких частот - это всё про него любимого
  Имеет две настройки: постоянную времени FILTER_STEP (миллисекунды), и коэффициент "плавности" FILTER_COEF
  Данный фильтр абсолютно универсален, подходит для сглаживания любого потока данных
  При маленьком значении FILTER_COEF фильтрованное значение будет меняться очень медленно вслед за реальным
  Чем больше FILTER_STEP, тем меньше частота опроса фильтра
  Сгладит любую "гребёнку", шум, ложные срабатывания, резкие вспышки и прочее говно. Пользуюсь им постоянно
*/
//#define FILTER_STEP 5
//#define FILTER_COEF 0.05
//int val;
static float val_f = 0.0;
static unsigned long filter_timer;
static int vvv;
  
  if (millis() - filter_timer > 10) { //FILTER_STEP +++++ было 5 ++++++
    filter_timer = millis();    // просто таймер
    // читаем значение (не обязательно с аналога, это может быть ЛЮБОЙ датчик)
    //val = analogRead(0);
    // основной алгоритм фильтрации. Внимательно прокрутите его в голове, чтобы понять, как он работает
    val_f = val * 0.05 + val_f * (1 - 0.05); //FILTER_COEF
    // для примера выведем в порт
    vvv = val_f * 100;
    vvv = vvv / 100; 
  }
    
    //if(vvv == 22) vvv = 0;
    return vvv;
}

void Motor::InZero(){ //диммирование
    //static int power;
    //int* pwr = Power;
    static byte count, last, lastVal;
    int val = ((uint16_t)++count * Motor::power) >> 8;
    if (lastVal != (val != last)) digitalWrite(10, val != last);
    lastVal = (val != last);
    last = val;
}

int Motor::GetSpeed(bool filtered = false){
    //static int FSpeed;
    static uint32_t speedCallTimer;

     if (millis() - speedCallTimer > 10){
        speedCallTimer = millis();
    
    //if(filtered) 
        Motor::actualSpeed = filter(analogRead(5));
    //else 
        //Motor::actualSpeed = aver(analogRead(5));
     }
    
    // = FSpeed;
    
    return Motor::actualSpeed;
}

int Motor::GetLoad(int c = 2){

    static int MinPower;
    Serial.println("Probing load");
    int i = 0;
  int startPower = 0;
  for(; i < c; i++){
 
    uint32_t t = millis();

    Motor::power = 25;
    t = millis();
    while(millis() - t < 3000){
        GetSpeed(true);
    }

      while(GetSpeed(true) < 40){
        if(millis() - t > 300) {
          Motor::power++;
          t = millis();
        }
      }
      startPower += Motor::power;
    

    Motor::power = 0;
    t = millis();
    while(millis() - t < 1000){
        GetSpeed(true);
    }
    
    Reverse();
    }
    
    Serial.println(startPower / i);
    MinPower = startPower / i; 
    return startPower / i;
  
}


int Motor::Spin(int tspeed, int fork, bool updown){
    //static int spinPower;
    static uint32_t spintimer;

    int s = GetSpeed(true);
    int p = Motor::power;

    //if(s == 0) Motor::power = minPower;
    //Serial.println(Motor::actualSpeed);
    int farFromT[] = {10, 20, 40}; //пороги удаления от цели. Просто задам разную мощность для разного удаления

    if(millis() - spintimer > 500){
        spintimer = millis(); 

/*         Serial.println("Tspeed");
        Serial.println(tspeed); 

        Serial.println("Speed");
        Serial.println(Motor::actualSpeed);
        Serial.println("Power");
        Serial.println(p);
         */
        //if(s < tspeed - fork){

            if (tspeed - s - fork > farFromT[2])
                p = p + 10;
            if (tspeed - s - fork > farFromT[1] && tspeed - s  - fork < farFromT[3])
                p = p + 5;
            if (tspeed - s - fork > farFromT[0] && tspeed - s  - fork < farFromT[1])
                p = p + 3;
            if (tspeed - s - fork < farFromT[0] && tspeed - s  - fork > 0)
                p = p + 1;
            
            if(updown){
                if (tspeed - s > farFromT[0] - farFromT[0] - farFromT[0] && tspeed - s < 0)
                    p = p - 2;
                if (tspeed - s > farFromT[1] - farFromT[1] - farFromT[1] && tspeed - s < farFromT[0] - farFromT[0] - farFromT[0])
                    p = p - 15;
                if (tspeed - s < farFromT[2] - farFromT[2] - farFromT[2])
                    p = p - 50;
            }

            if(p > 255)
                p = 255;
            if(p < 10)
                p = 10;



            Motor::power = p;
        //}
    }
    return s;
}

long Motor::SpinTest(int testspeed){

    // Проводится 3 цикла по 10 измерений скорости. Скорость устанавливается небольшая, 40-60. 
    // Среднее значение биения возвращается в виде процента от средней скорости.
    // Опыты показали, что допустимо отжимать без контроля человека при разбросе меньшем чем 12.  

    int acnt = 0;
    long amin = 0, amax = 0;
    //int truespeed = 0;
    //int prb = 0;
    long sprd;
    int probes = 0;
    uint32_t t2 = millis();
    long speedSpr[10];
    while(probes < 3){
        controlCall();
        if(millis() - t2 > 50 && acnt < 10){ //50 было 20
            Spin(testspeed, 5, false);
            t2 = millis();
            speedSpr[acnt] = Motor::actualSpeed;
            
            acnt++;
        }

        //delay(10);

        if(acnt == 10){ //выудим максимальное и минимальное
            probes ++;
            long mn = 1000;  //start with a huge va
            long mx = 1;
            //Serial.println("---");
            for (int index = 0; index < 10; index++)
            {
                
            //Serial.println(speedSpr[index]);
            
                if (speedSpr[index] < mn)
                {
                  mn = speedSpr[index]; //save the new minimum
                   

                }
            }

            //Serial.println("---");

            for (int index = 0; index < 10; index++)
            {
                if (speedSpr[index] > mx)
                {

                  mx = (int)speedSpr[index]; //save the new maximum
                }
            }

            amin = amin + mn;
            amax = amax + mx;
            acnt = 0;
            
        }
    }

    amin = amin / 3;
    amax = amax / 3;
    sprd = map(amin, 0, (amax + amin) / 2, 100, 0);
    
          
          
    
    /* Serial.println("---");
    Serial.println(amin);
    Serial.println(amax);
    Serial.println(sprd);
    Serial.println("---"); */
    return sprd;
}

void Motor::Reverse(){
    Motor::power = 0;
    delay(100);
    digitalWrite(6, !Motor::reversed);
    delay(10);
    digitalWrite(7, !Motor::reversed);
    Motor::reversed = !Motor::reversed;
    delay(100);

}

