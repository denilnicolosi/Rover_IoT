#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <DS3231.h>
#include <HCSR04.h>
#include <L298N.h>
#include <DHT.h>
#include <ArduinoJson.h>

// Definizione dei pin GPIO ESP32
#define HCSR04_TRIGGER_PIN 32
#define HCSR04_ECHO_PIN 33
#define DHT11_PIN 23
#define BATTERY_PIN 36
#define L_MOTOR_EN_PIN 25
#define L_MOTOR_IN1_PIN 26
#define L_MOTOR_IN2_PIN 27
#define R_MOTOR_EN_PIN 16
#define R_MOTOR_IN1_PIN 17
#define R_MOTOR_IN2_PIN 18

// Defininizione delle costanti utilizzate
#define DELAY_SEND_SENSOR_DATA_MS 2000
#define DELAY_FUNCTION_MODE_MS 50
#define DELAY_RECEIVE_DATA_MS 100
#define SAFETY_DISTANCE_CM 20

// Dichiarazione degli oggetti utilzzati
Adafruit_MPU6050 mpu;
RTClib myRTC;
DS3231 myClock;
DHT dht11(DHT11_PIN, DHT11);
L298N l_motor(L_MOTOR_EN_PIN, L_MOTOR_IN1_PIN, L_MOTOR_IN2_PIN);
L298N r_motor(R_MOTOR_EN_PIN, R_MOTOR_IN1_PIN, R_MOTOR_IN2_PIN );
UltraSonicDistanceSensor distanceSensor(HCSR04_TRIGGER_PIN, HCSR04_ECHO_PIN); 
StaticJsonDocument<250> docOutput; // Utilizzato il tool https://arduinojson.org/v6/assistant per calcolare la capacità del documento json.
StaticJsonDocument<250> docInput;

// Variabili ed enum utilizzati
sensors_event_t a, g, temp;

enum function_mode{
  AUTO = 0 ,
  MAN = 1
} function_mode_selected;

enum command{
  FORWARD = 0,
  BACKWARD = 1,
  RIGHT = 2,
  LEFT = 3,
  STOP = 4
} command_received;


void taskFunctionMode(void *pvParameters){
  while (1){

    if(function_mode_selected == AUTO){
            
      if(distanceSensor.measureDistanceCm()<=SAFETY_DISTANCE_CM){
        r_motor.stop();
        l_motor.stop();
        delay(500);        
        //random rotate
        r_motor.forward();
        l_motor.backward();
        delay(random(300,800));
        r_motor.stop();
        l_motor.stop();
        delay(500); 
      }else{
        r_motor.forward();
        l_motor.forward();    
      }      
    }
    else
    {  
      switch(command_received){
        case FORWARD:
           if(distanceSensor.measureDistanceCm()<=SAFETY_DISTANCE_CM){            
              r_motor.stop();
              l_motor.stop(); 
            }else{              
              r_motor.forward();
              l_motor.forward();
            }          
          break;
        case BACKWARD:
          r_motor.backward();
          l_motor.backward(); 
          break;
        case RIGHT:
          r_motor.backward();
          l_motor.forward(); 
          break;
        case LEFT:
          r_motor.forward();
          l_motor.backward(); 
          break;
        case STOP:
          r_motor.stop();
          l_motor.stop(); 
          break;  

        default:
          r_motor.stop();
          l_motor.stop(); 
          
      }      
    }
    vTaskDelay(DELAY_FUNCTION_MODE_MS / portTICK_PERIOD_MS);
  }
}


void taskRiceviDati(void *pvParameters){
  while (1){    
    if(Serial.available()){
      DeserializationError err = deserializeJson(docInput, Serial);
      if (err == DeserializationError::Ok) 
      {      
        if(!docInput["rtc_sync_unixtime"].isNull()){                   
          time_t epoch = (time_t)docInput["rtc_sync_unixtime"].as<long>();
          myClock.setEpoch(epoch);
        }
        if(!docInput["function_mode_auto"].isNull()){
          if(docInput["function_mode_auto"].as<String>()=="true"){
            function_mode_selected=AUTO;          
          }else{
            function_mode_selected=MAN;            
          }
        }
        if(!docInput["movement_command"].isNull()){
          command_received = (command) docInput["movement_command"].as<int>();         
        }
        if(!docInput["speed"].isNull()){
          int speed=docInput["speed"].as<int>();
          if(speed>0 && speed<=100){
            //mapping della velocità che viene ricevuta in 0-100% al range pwm in 0-255
            speed = map(speed, 0, 100, 0, 255);
            l_motor.setSpeed(speed);
            r_motor.setSpeed(speed);           
          }
        }
      } 
      else 
      {        
        // Flush all bytes in the "link" serial port buffer
        while (Serial.available() > 0)
          Serial.read();
      }
    }
    vTaskDelay(DELAY_RECEIVE_DATA_MS / portTICK_PERIOD_MS);    
  }
}

void taskInvioDati(void *pvParameters){
  while (1){    
    mpu.getEvent(&a, &g, &temp);   

    docOutput["distance"]=distanceSensor.measureDistanceCm();
    docOutput["acceleration"]["x"]=a.acceleration.x;
    docOutput["acceleration"]["y"]=a.acceleration.y;
    docOutput["acceleration"]["z"]=a.acceleration.z;
    docOutput["rotation"]["x"]=g.gyro.x;
    docOutput["rotation"]["y"]=g.gyro.y;
    docOutput["rotation"]["z"]=g.gyro.z;
    docOutput["unixtime"]=myRTC.now().unixtime();
    docOutput["temperature"]=dht11.readTemperature(); 
    docOutput["humidity"]=dht11.readHumidity();
    docOutput["battery"]["voltage"]=readVoltage();
    serializeJson(docOutput, Serial);
    Serial.println();

    vTaskDelay(DELAY_SEND_SENSOR_DATA_MS / portTICK_PERIOD_MS);
  }
}

float readVoltage(){
  return (float)analogRead(BATTERY_PIN) / 4096 * 17 ;
}

void setup(void) {
  Serial.begin(115200); 
  Wire.begin();  
  if (!mpu.begin(0x69)) {
    Serial.println("Failed to find MPU6050 chip");  
  }else{
    Serial.println("MPU6050 Found!"); 
  }
  dht11.begin();  

  function_mode_selected=MAN;
  command_received=STOP;
  l_motor.setSpeed(70);
  r_motor.setSpeed(70);
  
  xTaskCreate(taskRiceviDati, "Task ricezione dati", 5000, NULL, 2, NULL);  
  xTaskCreate(taskFunctionMode, "Task modalità di funzionamento", 5000, NULL, 3, NULL);
  xTaskCreate(taskInvioDati, "Task invio dati", 5000, NULL, 1, NULL);
}

void loop () {}
