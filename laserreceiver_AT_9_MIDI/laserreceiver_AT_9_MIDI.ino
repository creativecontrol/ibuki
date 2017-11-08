/*
   Diavolo Laser Harp Receiver
   Teensy 3.0
   XBee

   Packet format:
   < platform number, sensor number, sensor value >

*/

 //#define DEBUG

int cyclePause = 50;
int8_t cutoff = 127;
const int channel = 1;

//data parsing

const byte numChars = 32;
char receivedChars[numChars];
char tempChars[numChars];        // temporary array for use when parsing

// variables to hold the parsed data
int platform = 0;
int sensor_num = 0;
int sensor_val = 0;

boolean newData = false;

int statusLed = 13;
int errorLed = 13;
int dataLed = 13;

uint8_t option = 0;
uint8_t data = 0;

void flashLed(int pin, int times, int wait) {

  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(wait);
    digitalWrite(pin, LOW);

    if (i + 1 < times) {
      delay(wait);
    }
  }
}

void setup() {
  pinMode(statusLed, OUTPUT);
  pinMode(errorLed, OUTPUT);
  pinMode(dataLed,  OUTPUT);

#if defined DEBUG
  Serial.begin(9600);

  delay(2000);

  Serial.print("waking up...");
#endif
  // XBee on Serial1.
  Serial1.begin(38400);

#if defined DEBUG
  Serial.print("Connected XBee");
#endif

  flashLed(statusLed, 3, 50);
}

// continuously reads packets, looking for RX16 or RX64
void loop() {
  recvWithStartEndMarkers();
  if (newData == true) {
    strcpy(tempChars, receivedChars);
    // this temporary copy is necessary to protect the original data
    //   because strtok() used in parseData() replaces the commas with \0
    parseData();
    sensor_val = constrain(sensor_val, 0, 127);
    usbMIDI.sendNoteOn(50 + (platform * 10) + sensor_num, sensor_val, channel );
    newData = false;
  }

#if defined DEBUG
  Serial.print(platform);
  Serial.print(" ");
  Serial.print(sensor_num);
  Serial.print(" ");
  Serial.print(sensor_val);
  Serial.println("");
#endif

  delay(cyclePause);
}

//====================

void recvWithStartEndMarkers() {
  static boolean recvInProgress = false;
  static byte ndx = 0;
  char startMarker = '<';
  char endMarker = '>';
  char rc;

  while (Serial1.available() > 0 && newData == false) {
    rc = Serial1.read();

    if (recvInProgress == true) {
      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      }
      else {
        receivedChars[ndx] = '\0'; // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    }

    else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
}

//====================

void parseData() {                          // split the data into its parts

  char * strtokIndx;                       // this is used by strtok() as an index

  strtokIndx = strtok(tempChars, ",");    // get the first part - the string
  platform = atoi(strtokIndx);           // copy it to messageFromPC
  //Serial.print(strtokIndx);
  //Serial.print(" ");
  

  strtokIndx = strtok(NULL, ",");         // this continues where the previous call left off
  sensor_num = atoi(strtokIndx);          // convert this part to an integer
  //Serial.print(strtokIndx);
  //Serial.print(" ");

  strtokIndx = strtok(NULL, ",");         // this continues where the previous call left off
  sensor_val = atoi(strtokIndx);          // convert this part to an integer
  //Serial.println(strtokIndx);

}

