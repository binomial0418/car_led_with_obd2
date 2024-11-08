#include "BluetoothSerial.h"
#include "ELMduino.h"

BluetoothSerial SerialBT;
#define ELM_PORT SerialBT
#define DEBUG_PORT Serial
#define R1_PIN          27 // GPIO 26 led的3.3v電源

ELM327 myELM327;

uint32_t rpm = 0;

// ELM327 Bluetooth device MAC address
uint8_t remoteAddress[] = { 0x13, 0xE0, 0x2F, 0x8D, 0x5A, 0xF1 };
// Pairing pin
const char* pinCode = "1234";

int nb_query_state = SEND_COMMAND;  // Set the inital query state ready to send a command

//定義讀取狀態，以door_open值開始讀
typedef enum { door_open,
               door_lock,
               tpms_p } obd_pid_states;
obd_pid_states obd_state = door_open;


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

  Serial.println("Start connect to OBD scanner - Phase 2");
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

void loop() {
  
   //checkTPMS();
  //使用obd_state搭配switch循環讀取OBD2資料，順序為door_open-->door_lock-->tmps_p
  switch (obd_state){
    case door_open:{
      checkOpen();
      break;
    }
    case door_lock:{
      checkLock();
      break;
    }
    case tpms_p:{
      checkTPMS();
      break;
    }
  }
  
  //checkLock();
  // checkOpen();
  //checkTPMS();
  // if (nb_query_state == SEND_COMMAND)         // We are ready to send a new command
  //   {
  //       myELM327.sendCommand("22bc04");         // Send the custom PID commnad
  //      // myELM327.sendCommand("2204FE");
  //       nb_query_state = WAITING_RESP;          // Set the query state so we are waiting for response
  //   }
  //   else if (nb_query_state == WAITING_RESP)    // Our query has been sent, check for a response
  //   {
  //       myELM327.get_response();                // Each time through the loop we will check again
  //   }

  //   if (myELM327.nb_rx_state == ELM_SUCCESS)    // Our response is fully received, let's get our data
  //   {
  //     for (int i = 0; i < 40; i++) {
  //      Serial.print(myELM327.payload[i], HEX);  // Print each byte in hexadecimal format
  //      Serial.print(" ");
  //    }
  //    Serial.println(" ");
  //      uint8_t E = myELM327.payload[22];
  //     // uint8_t E = myELM327.payload[7];

  //     //Apply decoding formula
  //     bool result = ((E & 32) > 5);
  //     Serial.print("Decoded Result for PID 0x22BC04(payload[7]): ");
  //     Serial.println(result ? "True" : "False");
  //     nb_query_state = SEND_COMMAND;          // Reset the query state for the next command
  //     delay(5000);                            // Wait 5 seconds until we query again
  //   }
  //   else if (myELM327.nb_rx_state != ELM_GETTING_MSG)
  //   {                                           // If state == ELM_GETTING_MSG, response is not yet complete. Restart the loop.
  //       nb_query_state = SEND_COMMAND;          // Reset the query state for the next command
  //       myELM327.printError();
  //       delay(5000);                            // Wait 5 seconds until we query again
  //   }
}

//取得門鎖狀態
void checkLock() {
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
    // Serial.print("door lock "); 
    // for (int i = 0; i < 40; i++) {
    //   // Serial.print(myELM327.payload[i], HEX);  // Print each byte in hexadecimal format
    //   Serial.print(i);
    //   Serial.print(":");
    //   Serial.print(myELM327.payload[i]);  // Print each byte in hexadecimal format
    //   Serial.print(" ");
    // }
    //Serial.println(" ");
    if (myELM327.payload[22] == '1'){
      Serial.println("Door Lock");  
    } else {
      Serial.println("Door UnLock");
    }
    nb_query_state = SEND_COMMAND;                       // Reset the query state for the next command
    obd_state = door_open;                               // 切換下一個讀取的狀態是door_open
    delay(200);                                         // Wait 5 seconds until we query again
  } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {  // If state == ELM_GETTING_MSG, response is not yet complete. Restart the loop.
    nb_query_state = SEND_COMMAND;                       // Reset the query state for the next command
    myELM327.printError();
    obd_state = door_open;                               // 切換下一個讀取的狀態是door_open
    delay(200);  // Wait 5 seconds until we query again
  }
}

//取得車門開啟狀態
void checkOpen() {
  if (nb_query_state == SEND_COMMAND)  // We are ready to send a new command
  {
    myELM327.sendCommand_Blocking("ATSH 770");
    delay(200);
    myELM327.sendCommand("22bc03");           // Send the custom PID commnad
                                              // myELM327.sendCommand("2204FE");
    nb_query_state = WAITING_RESP;            // Set the query state so we are waiting for response
  } else if (nb_query_state == WAITING_RESP)  // Our query has been sent, check for a response
  {
    myELM327.get_response();  // Each time through the loop we will check again
  }

  if (myELM327.nb_rx_state == ELM_SUCCESS)  // Our response is fully received, let's get our data
  {
    // Serial.print("door open ");
    // for (int i = 0; i < 40; i++) {
    //   if (i == 21 || i == 22){
    //     Serial.print(i);
    //     Serial.print(":");
    //     Serial.print(myELM327.payload[i]);  // Print each byte in hexadecimal format
    //     Serial.print(" ");
    //   }
    // }
    //Serial.println(" ");
    digitalWrite(R1_PIN, LOW);
    if (myELM327.payload[21] == '2' || myELM327.payload[21] == '3'){ //2 or 3 open ,0:close
      Serial.println("Front Door open");  
      digitalWrite(R1_PIN, HIGH);
    } 
    if (myELM327.payload[22] == 'B' || myELM327.payload[22] == 'E'){ //B or E OPEN ,A:close
      Serial.println("Rear Door Open");  
      digitalWrite(R1_PIN, HIGH);
    }
    nb_query_state = SEND_COMMAND;                       // Reset the query state for the next command
    obd_state = tpms_p;                                  // 切換下一個讀取的狀態是tpms_p
    delay(200);                                         // Wait 5 seconds until we query again
  } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {  // If state == ELM_GETTING_MSG, response is not yet complete. Restart the loop.
    nb_query_state = SEND_COMMAND;                       // Reset the query state for the next command
    myELM327.printError();
    obd_state = tpms_p;                                  // 切換下一個讀取的狀態是tpms_p
    delay(200);  // Wait 5 seconds until we query again
  }
}

//取得胎壓狀態（測試）
void checkTPMS() {
  if (nb_query_state == SEND_COMMAND)  // We are ready to send a new command
  {
    myELM327.sendCommand_Blocking("ATSH 7D4");
    delay(200);
    myELM327.sendCommand("220101");           // Send the custom PID commnad
                                              // myELM327.sendCommand("2204FE");
    nb_query_state = WAITING_RESP;            // Set the query state so we are waiting for response
  } else if (nb_query_state == WAITING_RESP)  // Our query has been sent, check for a response
  {
    myELM327.get_response();  // Each time through the loop we will check again
  }
  if (myELM327.nb_rx_state == ELM_SUCCESS)  // Our response is fully received, let's get our data
  {
    Serial.print("tpms ");
    for (int i = 0; i < 40; i++) {
      // Serial.print(i);
      // Serial.print(":");
      Serial.print(myELM327.payload[i]);  // Print each byte in hexadecimal format
      Serial.print(" ");
    }
    Serial.println(" ");
    nb_query_state = SEND_COMMAND;                       // Reset the query state for the next command
    obd_state = door_lock;                               // 切換下一個讀取的狀態是door_lock
    delay(200);                                         // Wait 5 seconds until we query again
  } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {  // If state == ELM_GETTING_MSG, response is not yet complete. Restart the loop.
    nb_query_state = SEND_COMMAND;                       // Reset the query state for the next command
    myELM327.printError();
    obd_state = door_lock;                               // 切換下一個讀取的狀態是door_lock
    delay(200);  // Wait 5 seconds until we query again
  }
}