/**
  Main code
  Edited 5/8/14
  
  Reed switch + LCD work
  Relays - ?
*/

//bulb pins
const int numPins = 4;
int relayPins[] =   {10,11,12,13};
int thresholds[] = {50,150,250,400}; //maps to relayPins; this is rpm's to turn each on

int reedPin = 2;

//Constants. Change at will. I've told you what happens when you decrease something but you can increase it for the opposite effect.
const int buffer_size = 5; //how many rpm's to store in buffer. Reducing this causes more erratic changes in the bulbs but improves responsiveness (to sudden crank speed changes.)
const long update_time = 100; //update bulbs every this many ms. Reducing this increases bulb activity (power drain & expensive) but improves bulb responsiveness.
const long wait_for_input = long(3 * pow(10, 6)); //how long to wait for a click before declaring it a failure
const long delay_after_failure = 0; //ms to wait after an unsuccessful query. Reducing this increases power drain (more program activity) but increases accuracy.
const int buffer_tolerance = 100; //if the rpm is this much more/less than the historical average, we update. Reducing this increases sensitivity & num updates (which can be good or bad.)
const long failure_time = 10*1000; //if the reed switch tells us something besides this, we know nothing has happened (or there was a re-trigger cause the magnet hit twice)

//don't touch!
const int NOT_DEFINED = -1; //for the buffer; means that slot is empty. this is just a placeholder so keep it where it is (-1).

int buffer[buffer_size]; //contains the last few rpm's
long last_updated = 0; //ms since we last updated bulbs

unsigned long duration;

/* LCD stuff */
// Use the softwareSerial library to create a new "soft" Serial port
// for the display. This prevents display corruption when uploading code.
#include <SoftwareSerial.h>

// Attach the Serial display's RX line to digital pin TX
SoftwareSerial mySerial(0,1); // pin 1 = TX, pin 0 = RX (unused)


void setup() {      
  //Serial.begin(14400);
  for(int i=0; i<numPins; i++){
    pinMode(relayPins[i], OUTPUT);
  }  	
  pinMode(reedPin, INPUT_PULLUP);  
  
  mySerial.begin(9600);
  delay(500);
  /*
  mySerial.write(254); // move cursor to beginning of first line
  mySerial.write(128);

  mySerial.write("                "); // clear display
  mySerial.write("                ");

  mySerial.write(254); // move cursor to beginning of first line
  mySerial.write(128);
 
  mySerial.write("Hello, world!");
*/  
}

void loop() {
  char text[10];
  
  duration = pulseIn(reedPin, HIGH, wait_for_input);  
  //Serial.print(duration);
  
 
  if(duration < failure_time){
    //no interaction; the user has moved away from the bike!
    //Serial.println();
    flush_buffer(); //don't want to keep old data around
    turnAllOff();
    char errorMessage[] = "----";
  mySerial.write(254); // cursor to 7th position on first line
  mySerial.write(134);
  mySerial.write(errorMessage);   
    delay(delay_after_failure);
    return;
  }
  //now we know there WAS some interaction
  
  /**
    GAME PLAN:
    - Convert input to RPM (rotations of crank per minute)
    - Tack on to the buffer (that stores the last however-many RPM's)
    - If it's time to update bulbs, do that
  */
  
  //input told us how many microsec it took them to spin the crank once
  //convert to rotations per minute (usually in range 10-80)
  int rpm = int(60 * pow(10, 6)/duration); //60 sec in a minute, 1 million (10^6) microsec in a sec
  //Serial.print(" | Last = ");
  //Serial.print(rpm);
    
  //if buffer's empty, tack on reading but don't update yet (it may be a junk reading, plus you don't want to update w/ just one rpm under your belt)
  int average = average_buffer();
 
    //update bulbs immediately if user suddenly changes speed (they should see results immediately)
    //before we add to buffer, see if there's been a significant change (i.e. if the buffer has an average of 20rpm and they've now dialed it up to 40); if so, update immediately
    //average already done above
    int difference = abs(average - rpm);
    //code to handle this is just below
 
   //print RPM's to display
  sprintf(text,"%4d",rpm);
  mySerial.write(254); // cursor to 7th position on first line
  mySerial.write(134);
  mySerial.write(text);      
  
  if(average == NOT_DEFINED){ //buffer empty
    add_to_buffer(rpm);
    //let's do another reading before we potentially update
  }
  else if(difference >= buffer_tolerance){
      //large change in the value; the user really changed their spinning rate. update immediately (we don't care how long it's been, just do it!)
      flush_buffer();
      add_to_buffer(rpm); //make this the only thing there
      //update();   
  }
  else{
    //there is already stuff in buffer, let's add more!
    add_to_buffer(rpm);
      //is it time to update naturally?
      long time_since_update = millis() - last_updated;
      if(time_since_update >= update_time){
        update();
      }
  }

  //Serial.println();
} 

/**
Turns all the lights on or off en masse.
The first boolean is the first light (easiest to turn on); last boolean is last light.
Pass true to turn on, false to turn off.
The array should be as long as relayPins.
Example: boolean b[] = {true,true,false,false}; turn(b) turns on the first 2 lights and turns off the last 2.
*/
void turn(boolean data[]){
  for(int i=0; i<numPins; i++){
    if(data[i]) turnOn(relayPins[i]);
    else turnOff(relayPins[i]);
  }
}
 
void turnOn(int b){
  digitalWrite(b, HIGH);
}
 
void turnOff(int b){
  digitalWrite(b, LOW);
}

void turnAllOff(){
  boolean data[numPins];
  for(int i=0; i<numPins; i++){
    data[i] = false;
  }  
  turn(data);
}

/* BULBS */
/**
  Updates the bulbs based on the average RPM's from the buffer.
*/
void update(){
  int rpm = average_buffer(); //average rpm
  //Serial.print(" | Avg = ");
  //Serial.print(rpm);
  //update bulbs
  boolean turns[numPins];
  for(int i=0; i<numPins; i++){
    if(rpm >= thresholds[i]) turns[i] = true;
    else turns[i] = false;
  }
  turn(turns);
  last_updated = millis();
}

/* BUFFER LIBRARY */
/**
 Appends the given number to the front of the buffer, shifting everything else down.
*/
void add_to_buffer(int to_add){
  //shift everything else down
  for(int i=buffer_size-1; i>0; i--){
    buffer[i] = buffer[i-1];  
  }
  buffer[0] = to_add;
}

/**
 Returns the average of the buffer (in int form); NOT_DEFINED values are not counted.
 Returns NOT_DEFINED if the buffer is empty (i.e. filled with NOT_DEFINED.)
*/
int average_buffer(){
  int values = 0;
  int total = 0;
  for(int i=0; i<buffer_size; i++){
    if(buffer[i] != NOT_DEFINED){
      //good, add it
      values = values + 1;
      total = total + buffer[i];  
    }
  }  
  if(values == 0) return NOT_DEFINED;
  return total / values;
}

/**
  Effectively empties the buffer (i.e. sets all its values to NOT_DEFINED.)
*/
void flush_buffer(){
  for(int i=0; i<buffer_size; i++){
    buffer[i] = NOT_DEFINED;  
  }  
}
