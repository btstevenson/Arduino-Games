#include <SoftwareSerial.h>
#include "scheduler.h"

#define COMMAND_SIZE 1
#define MAX_INPUT_SIZE 4
#define rxPin 2
#define txPin 3

int latchPin = 5;
int clockPin = 6;
int dataPin = 4;

byte leds = 0;

unsigned char gameChoice = 1; // 1: simonsays, 2: TBD, 3:TBD
char simonArray[8] = {0};
char gameState = 1;
char simonState = 0; //0: setup, 1: playing, 2: results
char inputFlag = 0;
char userInput = 0;
char winLose = 0; //1: wins, 2: loses
char allLights = 0;


SoftwareSerial wifly(rxPin, txPin);

void updateShiftRegister()
{
   digitalWrite(latchPin, LOW);
   shiftOut(dataPin, clockPin, MSBFIRST, leds);
   digitalWrite(latchPin, HIGH);
}

//will need to make more flexible for different games,
//set max size for i from a parameter
char ParseInput(char *commandBuffer)
{
  char *input = NULL;
  char command = 0;
  char i = 0;

  commandBuffer = commandBuffer + 1; //removing initial b
  input = strtok(commandBuffer, ",");
  command = *input;
  return command;
}

char getInput()
{
  char commandBuffer[MAX_INPUT_SIZE + 1];
  char i = 0;
  char command = 0;

  if(wifly.peek() == 'b')
  {
    while(wifly.available() && i < MAX_INPUT_SIZE)
    {
      char c = wifly.read();
      commandBuffer[i] = c;
      ++i;
      delay(2);
    }
  }
  if(i >= 0 && commandBuffer[i-1] == 'e')
  {
    command = ParseInput(commandBuffer);
    Serial.println(command, DEC);
    i = 0;
    delay(20);
  }
  return command;
}

enum SimonState {Simon_Init, Simon_Random, Simon_Game};

void Simon_Says(int state)
{
  static int noInputCnt = 0;
  static char inputCnt = 0;
  static char timeTrack = 0;
  static char i = 0;
  Serial.print("says state is: ");
  Serial.println(state, DEC);
  //actions
  switch(state)
  {
    case Simon_Init:
      break;
    case Simon_Random:
      if(i < gameState)
      {
        int ranNum = random(1, 9);
        Serial.print("Random is: ");
        Serial.println(ranNum, DEC);
        simonArray[gameState - 1] = char(ranNum);
        ++i;
        timeTrack += 1;
      }
      break;
    case Simon_Game:
      if(simonState == 1 && !allLights)
      {
        userInput = getInput();
        Serial.println(userInput, DEC);
        if(userInput == 0)
        {
          ++noInputCnt;
          if(noInputCnt >= 6)
          {
            simonState = 2;
            winLose = 2;
            noInputCnt = 0;
          }
        }
        else
        {
          inputFlag = 1;
          if(userInput == simonArray[gameState - 1])
          {
            ++inputCnt;
            if(inputCnt >= gameState)
            {
              simonState = 2;
              winLose = 1;
            }
          }
          else
          {
            simonState = 2;
            winLose = 2;
          }
        }
      }
      break;
    default:
      break;
  }
  //transitions
  switch(state)
  {
    case Simon_Init:
      if(simonState != 2)
      {
        state = Simon_Random;
      }
      else
      {
        state = Simon_Init;
      }
      break;
    case Simon_Random:
      if(i < gameState)
      {
        state = Simon_Random;
      }
      else
      {
        state = Simon_Game;
        i = 0;
      }
      break;
    case Simon_Game:
      if(simonState != 2)
      {
        state = Simon_Game;
      }
      if(simonState == 2)
      {
        state = Simon_Init;
      }
      break;
    default:
      break; 
  }
  scheduleTimer1Task(&Simon_Says, state, 200);
}

enum SimonDisplayState {Disp_Init, Disp_Off, Disp_On};

// discern between game setup and user input
void Simon_Display(int state)
{
  static char cnt = 0;
  static char dispCnt = 0;
  static char displayOn = 0;
  static char i = 0;
  Serial.print("display state is: ");
  Serial.println(state, DEC);
  //actions
  switch(state)
  {
    case Disp_Init:
      break;
    case Disp_Off:
      //turn off lights
      leds = 0;
      updateShiftRegister();
      ++cnt;
      if(allLights)
      {
        allLights = 0;
      }
      break;
    case Disp_On:
      if(simonState == 0)
      {
        if(dispCnt < gameState + 1 && !displayOn)
        {
          //display value of simonArray[dispCnt]
          if(dispCnt < gameState)
          {
            int led = simonArray[gameState - 1];
            --led;
            Serial.print("LED to on: ");
            Serial.println(led, DEC);
            bitSet(leds, led);
            updateShiftRegister();
          }
          else
          {
            Serial.println("all lights on");
            for(i = 0; i < 8; i++)
            {
              bitSet(leds, i);
            }
            updateShiftRegister();
            allLights = 1;
          }
          displayOn = 1;
          ++dispCnt;
        }
        ++cnt;
      }
      if(simonState == 1)
      {
        if(inputFlag)
        {
          Serial.println(userInput, DEC);
          cnt++;
        }
      }
      if(simonState == 2)
      {
        if(winLose == 1)
        {
          Serial.println("happy face");
          for(i = 2; i < 7; i++)
          {
            bitSet(leds, i);
          }
          updateShiftRegister();
          ++cnt;
        }
        else
        {
          Serial.println("sad face");
          for(i = 0; i < 3; i++)
          {
            bitSet(leds, i);
          }
          for(i = 6; i < 8; i++)
          {
            bitSet(leds, i);
          }
          updateShiftRegister();
          ++cnt;
        }
      }
      break;
    default:
      break;
  }
  //transitions
  switch(state)
  {
    case Disp_Init:
      state = Disp_Off;
      break;
    case Disp_Off:
      if(simonState == 0)
      {
        if(cnt < 3)
        {
          state = Disp_Off;
        }
        else
        {
          cnt = 0;
          state = Disp_On;
        }
      }
      if(simonState == 1)
      {
        if(inputFlag)
        {
          cnt = 0;
          state = Disp_On;
        }
        else
        {
          state = Disp_Off;
        }
      }
      if(simonState == 2)
      {
        cnt = 0;
        state = Disp_On;
      }
      break;
    case Disp_On:
      if(simonState == 0)
      {
        if(cnt < 3)
        {
          state = Disp_On;
        }
        else
        {
          if(dispCnt > gameState)
          {
            simonState = 1;
            Serial.println("transition to player");
            dispCnt = 0;
          }
          state = Disp_Off;
          cnt = 0;
          displayOn = 0;
        }
      }
      if(simonState == 1 && !allLights)
      {
        if(cnt < 4)
        {
          state = Disp_On;
        }
        else
        {
          cnt = 0;
          state = Disp_Off;
        }
      }
      if(simonState == 2)
      {
        if(cnt < 5)
        {
          state = Disp_On;
        }
        else
        {
          winLose = 0;
          state = Disp_Off;
          simonState = 0;
          cnt = 0;
        }
      }
      break;
    default:
      break;
  }  
  scheduleTimer1Task(&Simon_Display, state, 200);
}

void setup() {
  // put your setup code here, to run once:
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);  
  pinMode(clockPin, OUTPUT);
  updateShiftRegister();
  Serial.begin(9600);
  while(!Serial)
  {
    ;
  }
  wifly.begin(9600);
  randomSeed(analogRead(0));

  setupTaskScheduler(2, 50);
  startSchedulerTicking();
  scheduleTimer1Task(&Simon_Says, Simon_Init, 200);
  scheduleTimer1Task(&Simon_Display, Disp_Init, 200);  
}

void loop() {

}
