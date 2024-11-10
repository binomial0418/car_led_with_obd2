//由汽車的obd2讀取車門開啟狀態，門鎖狀態。當車門開啟或是解鎖時，點亮LED燈
//Hyundai Tucson L(NX4) Hybrid

#include "BluetoothSerial.h"
#include "ELMduino.h"

BluetoothSerial SerialBT;
#define ELM_PORT SerialBT
#define DEBUG_PORT Serial
#define R1_PIN    27 // led繼電器的3.3v電源

ELM327 myELM327;
// ELM327 Bluetooth device MAC address
// 66:1E:32:F9:C1:F2
uint8_t remoteAddress[] = { 0x13, 0xE0, 0x2F, 0x8D, 0x5A, 0xF1 };
//uint8_t remoteAddress[] = { 0x66, 0x1E, 0x32, 0xF9, 0xC1, 0xF2 };
// Pairing pin
const char* pinCode = "1234";
//定義讀取狀態，以door_open值開始讀
typedef enum { door_open,
               door_lock,
               SPEED} obd_pid_states;
obd_pid_states obd_state = door_open;
int nb_query_state = SEND_COMMAND;  // Set the inital query state ready to send a command

bool ledOn = false;
float mph = 0;
//控制LED點亮時間，避免解鎖狀態無限期點亮
int ledOnTimes = 0; 

void setup() {
#if LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
#endif
  pinMode(R1_PIN, OUTPUT); // for key power
  digitalWrite(R1_PIN, LOW);  // 初始狀態下關閉電源輸出
  // digitalWrite(R1_PIN, HIGH);  // 初始狀態下關閉電源輸出
  // delay(1000);
  // digitalWrite(R1_PIN, LOW);  // 初始狀態下關閉電源輸出
  DEBUG_PORT.begin(115200);

  // Set Bluetooth name
  ELM_PORT.begin("ArduHUD", true);

  // Set pairing pin
  SerialBT.setPin(pinCode, strlen(pinCode));
  // Connect to specific MAC address
  for (int i = 0; i < 10; i++) {
    if (!ELM_PORT.connect(remoteAddress)) {
      DEBUG_PORT.println("Couldn't connect to OBD scanner - Phase 1");
      delay(200);
    } else {
      break;
    }
  }
  // Initialize ELM327
  for (int i = 0; i < 10; i++) {
    if (!myELM327.begin(ELM_PORT, false, 2000,'0',50)) {
      Serial.println("Couldn't connect to OBD scanner - Phase 2");
      delay(200);
    } else {
      break;
    }
  }
  
  Serial.println("Connected to ELM327");
  // myELM327.sendCommand_Blocking("ATSH 770");
  // myELM327.sendCommand_Blocking("ATSH 7A0");
}
/*********************************************
* loop setcion
**********************************************/
void loop() {
  ledOn = false;
  mph   = 0;
  
  //使用obd_state搭配switch循環讀取OBD2資料，順序為door_open-->door_lock-->speed
  //若ELM327斷線 重新連線
  // if (!myELM327.connected){
  //   Serial.println("reconnect elm327");
  //   ConnectToElm327();
  // }
  // if (myELM327.connected){
    switch (obd_state){
      case door_open:{
        checkDoorOpenStats();
        break;
      }
      case door_lock:{
        checkDoorLockStats();
        break;
      }
      case SPEED: //當車子速度到達X時關閉時LED
      {
        getCarSpeed();
        if (mph*1.65 > 10){
          ledOn = false;
        }
        break;
      }
    }
  // }
  //set led on
  //Led點亮時間由繼電器控制（目前設定一次20秒），這邊只要短暫觸發繼電器即可。
  if (ledOn == true){
    Serial.println("亮Led");
    digitalWrite(R1_PIN, HIGH);
    delay(200);
    digitalWrite(R1_PIN, LOW);
  }
  // if (ledOn == true){
  //   ledOnTimes++;
  //   if (ledOnTimes < 60){
  //     Serial.println("亮Led");
  //     digitalWrite(R1_PIN, HIGH);
  //     delay(200);
  //     digitalWrite(R1_PIN, LOW);
  //   }
  // } else {
  //   ledOnTimes = 0;
  // }
  delay(1000);
}
/************************************
* 從OBD2取得車門開啟狀態
************************************/
void checkDoorOpenStats() {
  if (nb_query_state == SEND_COMMAND)  // We are ready to send a new command
  {
    myELM327.sendCommand_Blocking("ATSH 770");
    delay(200);
    myELM327.sendCommand("22bc03");           
    nb_query_state = WAITING_RESP;            // Set the query state so we are waiting for response
  } else if (nb_query_state == WAITING_RESP)  // Our query has been sent, check for a response
  {
    myELM327.get_response();  // Each time through the loop we will check again
  }
  if (myELM327.nb_rx_state == ELM_SUCCESS)  // Our response is fully received, let's get our data
  {
    if (myELM327.payload[21] == '2' || myELM327.payload[21] == '3'){ //2 or 3 open ,0:close
      Serial.println("前門開啟");  
      ledOn = true;//點亮Led
    } 
    if (myELM327.payload[22] == 'B' || myELM327.payload[22] == 'E'){ //B or E OPEN ,A:close
      Serial.println("後門開啟");  
      ledOn = true;//點亮Led
    }
    nb_query_state = SEND_COMMAND;                       // Reset the query state for the next command
    obd_state = door_lock;                              // 切換下一個讀取的狀態
    delay(200);                                         // Wait 5 seconds until we query again
  } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {  // If state == ELM_GETTING_MSG, response is not yet complete. Restart the loop.
    nb_query_state = SEND_COMMAND;                       // Reset the query state for the next command
    myELM327.printError();
    obd_state = door_lock;                                  // 切換下一個讀取的狀態
    delay(200);  // Wait 5 seconds until we query again
  }
}
/************************************
* 從OBD2取得門鎖狀態
************************************/
void checkDoorLockStats() {
  if (nb_query_state == SEND_COMMAND)  // We are ready to send a new command
  {
    myELM327.sendCommand_Blocking("ATSH 770");
    delay(200);
    myELM327.sendCommand("22bc04");           // Send the custom PID commnad
    nb_query_state = WAITING_RESP;            // Set the query state so we are waiting for response
  } else if (nb_query_state == WAITING_RESP)  // Our query has been sent, check for a response
  {
    myELM327.get_response();  // Each time through the loop we will check again
  }
  if (myELM327.nb_rx_state == ELM_SUCCESS)  // Our response is fully received, let's get our data
  {
    if (myELM327.payload[22] == '1'){
      Serial.println("車門上鎖");  
    } else {
      Serial.println("車門解鎖");
      ledOn = true;//點亮Led
    }
    nb_query_state = SEND_COMMAND;                       // Reset the query state for the next command
    obd_state = SPEED;                                   // 切換下一個讀取的狀態
    delay(200);                                          // Wait 5 seconds until we query again
  } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {  // If state == ELM_GETTING_MSG, response is not yet complete. Restart the loop.
    nb_query_state = SEND_COMMAND;                       // Reset the query state for the next command
    myELM327.printError();
    obd_state = SPEED;                                   // 切換下一個讀取的狀態
    delay(200);  // Wait 5 seconds until we query again
  }
}
/************************************
* 從OBD2取得車速
************************************/
void getCarSpeed() {
  mph = myELM327.mph();
  if (myELM327.nb_rx_state == ELM_SUCCESS)
  {
    obd_state = door_open;
  }
  else if (myELM327.nb_rx_state != ELM_GETTING_MSG)
  {
    myELM327.printError();
    obd_state = door_open;
  }
}
/************************************
* 連線藍牙elm327
************************************/
void ConnectToElm327(){
  // Set pairing pin
  SerialBT.setPin(pinCode, strlen(pinCode));
  // Connect to specific MAC address
  if (!ELM_PORT.connect(remoteAddress)) {
    DEBUG_PORT.println("Couldn't connect to OBD scanner - Phase 1");
  }
  // Initialize ELM327
  
  if (!myELM327.begin(ELM_PORT, false, 2000,'0',50)) {
    Serial.println("Couldn't connect to OBD scanner - Phase 2");
  } else {
    Serial.println("Connected to ELM327");
  }
}