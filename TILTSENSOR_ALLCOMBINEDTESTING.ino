#include <Wire.h>
#include "SparkFun_BNO080_Arduino_Library.h"
#include <SD.h>
#include <XInput.h>

BNO080 myIMU;
float conv = 180.0 / PI;

unsigned long lastA = 0;

const int START_PIN = 12;
const int A_PIN = 8;
const int B_PIN = 7;
uint16_t rumbleprevious = 0;
String nameBuffer = "teSt!#";
uint8_t score = 1;
uint8_t level = 1;
bool gettingScores = false;
bool gettingCertainScores = false;
uint16_t gettingScoreAttemptNumber = 1;
uint8_t packetNumber = 0;
uint16_t attemptNumber = 1;

File file;
File replayFile;
File saveHeader;
const int chipSelect = 4;

// how many writes before flushing to SD
const int flushInterval = 100;  
int writeCount = 0;

bool currentlyRecording = false;
bool currentlyReplaying = false;

//int cyclesOfNoData = 0;

uint8_t leftMotor = 0;
uint8_t rightMotor = 0;

float roll, pitch;
int32_t joyX, joyY;
int startBtn, aBtn, bBtn;

const int tilt_degree = 15;

void setup(){
  pinMode(START_PIN, INPUT_PULLUP);
  pinMode(A_PIN, INPUT_PULLUP);
  pinMode(B_PIN, INPUT_PULLUP);
  Wire.begin();
  Wire.setClock(400000);

  if (myIMU.begin(0x4A) == false)
  {
    while (1);
  }

  myIMU.enableRotationVector(1);  // the argument is the update rate in milliseconds
  //XInput.setAutoSend(false);  // disable automatic output (would then need to call XInput.send() in the loop)
  XInput.begin();                 // initialize XInput controller
  //XInput.setJoystickRange(0, 1023);

  if (!SD.begin(chipSelect)) {
    Serial.println("SD init failed!");
    while (1); // stop here
  } else {
    for (uint8_t i = 1; i < 5; i++){
      SD.mkdir("levels/" + String(i) + "/headers");
      SD.mkdir("levels/" + String(i) + "/replays");
    }
  }
}

void loop(){
  unsigned long now = millis();
  uint16_t rumblecollect = XInput.getRumble();
  if (rumblecollect != rumbleprevious){
    uint8_t leftMotor  = (rumblecollect >> 8) & 0xFF;  // upper 8 bits
    uint8_t rightMotor = rumblecollect & 0xFF;         // lower 8 bits

    if (rightMotor == 253){
      if (leftMotor == 101){ // start recording
        currentlyRecording = true;
        if (SD.exists("testlog.txt")) SD.remove("testlog.txt");

        // Open file once and keep it open
        file = SD.open("testlog.txt", FILE_WRITE);
        if (!file) {
          while (1);
        }
      } else if (leftMotor == 102){ // stop recording
        currentlyRecording = false;
        file.close();
      }
    } else if (rightMotor == 123){
      if (leftMotor == 103) {  // start replay
        if (SD.exists("testlog.txt")) {
            replayFile = SD.open("testlog.txt", FILE_READ);
            currentlyReplaying = replayFile;  // true if file opened
        } else {
            currentlyReplaying = false;
        }
      } else if (leftMotor == 104) {  // stop replay
          currentlyReplaying = false;
          if (replayFile) replayFile.close();
      }
    } else if (rightMotor == 201){
      if (leftMotor == 105){   // clear name buffer
        nameBuffer = "";
      }
    } else if (rightMotor == 202){  // ODD ASCII TO NAME BUFFER
      if ((rumbleprevious & 0xFF) != 202){  // IF LAST INSTUCTION WASNT 202
        nameBuffer += (char)leftMotor;
      }
    } else if (rightMotor == 203){  // EVEN ASCII TO NAME BUFFER
      if ((rumbleprevious & 0xFF) != 203){  // IF LAST INSTUCTION WASNT 203
        nameBuffer += (char)leftMotor;
      }
    } else if (rightMotor == 204){  // SET SCORE BUFFER
      score = leftMotor;
    } else if (rightMotor == 205){  // SET LEVEL BUFFER
      level = leftMotor;
    } else if (rightMotor == 207){  // TRIAL REPLAY WRITING
      if (leftMotor == 107){      // START WRITING TRIAL REPLAY

        String replayDir = "levels/" + String(level) + "/replays";
        if (SD.exists(replayDir)){
          file = SD.open(replayDir);
          attemptNumber = 0;
          while (true) {
              attemptNumber++;
              File entry = file.openNextFile();
              if (!entry) break; // no more files
              entry.close();
          }
          file.close();

          file = SD.open(replayDir + "/" + String(attemptNumber) + ".txt", FILE_WRITE);
          currentlyRecording = true;
        }
      } else if (leftMotor == 108) {  // STOP WRITING TRIAL REPLAY
          currentlyRecording = false;
          file.close();
          if (SD.exists("levels/" + String(level) + "/replays")){
            file = SD.open("levels/" + String(level) + "/headers/" + String(attemptNumber) + ".txt", FILE_WRITE);
            file.println(nameBuffer);
            file.println(score);
            file.close();
          }
      }
    } else if (rightMotor == 206){  // GET ALL/CERTAIN SCORES
      gettingScores = true;
      if (leftMotor == 109) gettingCertainScores = false;
      else if (leftMotor == 110) gettingCertainScores = true;
      packetNumber = 0;
    }
    rumbleprevious = rumblecollect;
  }

  // IMU data-getting stuff
  if (myIMU.dataAvailable()){
    if (gettingScores){

      if (SD.exists("levels/" + String(level) + "/replays")){
          if (SD.exists("levels/" + String(level) + "/headers/" + String(gettingScoreAttemptNumber) + ".txt")){
            file = SD.open("levels/" + String(level) + "/headers/" + String(gettingScoreAttemptNumber) + ".txt", FILE_READ);
            
            if (file) {
                String line;
                int lineNumber = 0;
                int scoreValue = 0;

                bool correctName = true;

                while (file.available()) {
                    line = file.readStringUntil('\n');
                    line.trim();

                    if ((lineNumber == 0) && gettingCertainScores && (line != nameBuffer)) correctName = false;
                    if (lineNumber == 1) { // second line (index 1)
                        scoreValue = line.toInt();
                        break; // done reading early
                    }
                    lineNumber++;
                }

                file.close();

                if (correctName) XInput.setTrigger(TRIGGER_LEFT, scoreValue); // Set left trigger to the score we read

                // Increment for next file
                gettingScoreAttemptNumber++;

                if (correctName){
                  if (++packetNumber == 0) packetNumber = 1;
                  XInput.setTrigger(TRIGGER_RIGHT, packetNumber);
                }
            }

          } else {
            XInput.setTrigger(TRIGGER_LEFT, 0);
            XInput.setTrigger(TRIGGER_RIGHT, 0);
            gettingScores = false;
            gettingCertainScores = false;
            gettingScoreAttemptNumber = 1;
            packetNumber = 0;
          }
      } else {
        XInput.setTrigger(TRIGGER_LEFT, 0);
        XInput.setTrigger(TRIGGER_RIGHT, 0);
        gettingScores = false;
        gettingCertainScores = false;
        gettingScoreAttemptNumber = 1;
        packetNumber = 0;
      }
    }

    if (currentlyReplaying) {
            if (replayFile.available()) {
                String line = replayFile.readStringUntil('\n');
                line.trim();
                if (line.length() > 0) {
                    int firstSpace = line.indexOf(' ');
                    int secondSpace = line.indexOf(' ', firstSpace + 1);
                    int thirdSpace = line.indexOf(' ', secondSpace + 1);
                    int fourthSpace = line.indexOf(' ', thirdSpace + 1);

                    joyX = line.substring(0, firstSpace).toInt();
                    joyY = line.substring(firstSpace + 1, secondSpace).toInt();
                    startBtn = line.substring(secondSpace + 1, thirdSpace).toInt();
                    aBtn = line.substring(thirdSpace + 1, fourthSpace).toInt();
                    bBtn = line.substring(fourthSpace + 1).toInt();
                }
            } else {  // reached end of file
                replayFile.close();
                currentlyReplaying = false;
            }
    } else {
      roll  = myIMU.getRoll()  * conv;
      pitch = myIMU.getPitch() * conv;

      joyX = constrain(roll, -tilt_degree, tilt_degree)  * 32768 / tilt_degree;
      joyY = constrain(pitch, -tilt_degree, tilt_degree) * 32768 / tilt_degree;
      startBtn = !digitalRead(START_PIN);
      aBtn = !digitalRead(A_PIN);
      bBtn = !digitalRead(B_PIN);
    }
      XInput.setJoystick(JOY_LEFT, joyX, joyY); // left stick
      XInput.setButton(BUTTON_START, startBtn);
      XInput.setButton(BUTTON_A, aBtn);
      XInput.setButton(BUTTON_B, bBtn);

    if (currentlyRecording == true){
      // --- Print to SD file ---
      file.print(joyX);
      file.print(" ");
      file.print(joyY);
      file.print(" ");
      file.print(startBtn);
      file.print(" ");
      file.print(aBtn);
      file.print(" ");
      file.println(bBtn);

      // Flush occasionally
      if (++writeCount >= flushInterval) {
          file.flush();
          writeCount = 0;
      }
    }
  }
}