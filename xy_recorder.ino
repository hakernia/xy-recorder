/*
  xy_recorder.ino

  Sends data to TDA1543 double DAC
  Added dial controller

*/

#define DEBUG_DAC
#undef DEBUG_DIAL
#define DEBUG_STORY

// Pin 13 has an LED connected on most Arduino boards.
int led = 13;

// plotter pins
int mode = 12;
int powerOff = 9;  // 0 - plotter ON, 1 - plotter OFF

// DAC pins
int ws = 2;
int bck = 3;
int data = 4;

// dial pins
int dialInputPin = 7;
int soundOutputPin = 8;
int tonePin = 10;

int dels[4] = {1600, 1100, 800};  // default tone for incorrect number
int dum;  // dummy global var to avoid compiler optimization


// DAC variables
short val1 = 0xFF;  // current value of X
short val2 = 0xFF;  // current value of Y
// warning: int is -32768..32767 on UNO!
long target_val1 = 0;
long target_val2 = 0;
double dval1 = (double)val1;  // current value of x in double and not int
double dval2 = (double)val2;  // current value of Y in double and not int
int caret_speed = 5;
double dstep1 = caret_speed;  // initial value of step X
double dstep2 = caret_speed;  // initial value of step Y

int story_length = 0;  // -1 - endless

char outdbg[300];




// DAC functions

void send_data(short value_l, short value_r) {
  int ff;
  int ch;
  short value;

  for(ch=0; ch < 2; ch++) {
    if(ch==0)
      value = value_l;
    else
      value = value_r;

    for (ff=0; ff < 16; ff++)
    {
      digitalWrite(bck, LOW);  // prepare to send data
      if(ff == 15)
        digitalWrite(ws, ch);  // select channel (0-left,1-right)
      digitalWrite(data, (value & 0x8000) > 0);  // set bit of data
      digitalWrite(bck, HIGH);  // send the bit of data
      value <<= 1;
    }
  }
}


void set_target(int trg1, int trg2) {
  
  target_val1 = trg1;
  target_val2 = trg2;
  
  // set step so that both arrive the target at the same time
  // if distance1 > distance2 then step2 must be < speed ( = dist2/dist1 )
  // and vice versa.
  if(abs((double)target_val1 - dval1) > abs((double)target_val2 - dval2)) {
    dstep1 = ((double)target_val1 - dval1) / abs((double)target_val1 - dval1) * (double)caret_speed;
    dstep2 = ((double)target_val2 - dval2) / abs((double)target_val1 - dval1) * (double)caret_speed;
  }
  else 
  if(abs((double)target_val2 - dval2) > 0) {
    dstep1 = ((double)target_val1 - dval1) / abs((double)target_val2 - dval2) * (double)caret_speed;
    dstep2 = ((double)target_val2 - dval2) / abs((double)target_val2 - dval2) * (double)caret_speed;
  }
  else {
    dstep1 = 0;
    dstep2 = 0;
  }

#ifdef DEBUG_DAC
  sprintf(outdbg, "set target: %lf %d   %lf %d", dval1, target_val1, dval2, target_val2);
  Serial.println(outdbg);
#endif
}

int step_ahead() {
  
    if(abs(dval1 - target_val1) > caret_speed + 1) { // pion
      dval1 += dstep1;
      val1 = (short) (dval1);
    }
    else {
      val1 = target_val1;
    }

    if(abs(dval2 - target_val2) > caret_speed + 1) { // poziom
      dval2 += dstep2;
      val2 = (short) (dval2);
    }
    else {
      val2 = target_val2;
    }
  
    return (val1 == target_val1 && val2 == target_val2);  // TRUE = target reached;
}


int sign(int x) {
  if(x > 0)  return 1;
  return -1;
}
int next_test_mode_target() {
    static int arrived = 0;  // 0 - cross, 1 - V or H

#ifdef DEBUG_DAC
    sprintf(outdbg, "next test target\n  in: %d %d   %d %d", 
                    val1, target_val1, val2, target_val2);
    Serial.println(outdbg);
#endif

    if(abs(target_val1) != 32767)
        target_val1 = -sign(target_val1) * 32767;
    if(abs(target_val2) != 32767)
        target_val2 = -sign(target_val2) * 32767;
        
    if(arrived) {
        // arrived V or H, next go across
        // now change both X and Y
        target_val1 = -target_val1;
        target_val2 = -target_val2;
        arrived = 0;
    }
    else
    {
        // arrived cross, next go V or H
        if(target_val1 == target_val2)
            // now change just Y
            target_val2 = -target_val2;
        else
            // now change just X
            target_val1 = -target_val1;
        arrived = 1;
    }
#ifdef DEBUG_DAC
    sprintf(outdbg, " out: %d %d   %d %d", val1, target_val1, val2, target_val2);
    Serial.println(outdbg);
#endif
}

// End of DAC functions












// DIAL WHEEL ROUTINES

long debounceDelay = 10;

// debounces input pin
int readDialInputPin()
{
  static int buttonState;
  static int lastButtonState = LOW;
  static long lastDebounceTime = 0;
  int reading = digitalRead(dialInputPin);
  
  if (reading != lastButtonState) 
    lastDebounceTime = millis();
  if ((millis() - lastDebounceTime) > debounceDelay) 
    if (reading != buttonState) 
      buttonState = reading;
  lastButtonState = reading;
  return buttonState;
}


int lastDialPinState = readDialInputPin();
long lastLowStatePeriod;
long lastHighStatePeriod;

// detects and measures input pin state changes
int dialPinStateChanged()
{
  static long startLowStateTime;
  static long endLowStateTime;
  static long startHighStateTime;
  static long endHighStateTime;

  int change = false;
  
  if(readDialInputPin() == LOW && lastDialPinState == HIGH)
  {
    // Serial.println("H/L");
    // falling edge
    endHighStateTime = millis();
    startLowStateTime = endHighStateTime;
    lastHighStatePeriod = endHighStateTime - startHighStateTime;
    lastDialPinState = LOW;
    change = true;
  }
  else
  if(readDialInputPin() == HIGH && lastDialPinState == LOW)
  {
    // Serial.println("L/H");
    // rising edge
    endLowStateTime = millis();
    startHighStateTime = endLowStateTime;
    lastLowStatePeriod = endLowStateTime - startLowStateTime;
    lastDialPinState = HIGH;
    change = true;
  }
  return change;
}



#define MAX_DIAL_NUMBER_LEN   20
#define RESET_DELAY_DIAL     100
char dialDigits[MAX_DIAL_NUMBER_LEN+1];  // dialed digits buffer
int  dialDigitsNum = -2;    // dialed digits count

void resetDialDigits() {
  dialDigitsNum = -1;
  memset(dialDigits, 0, sizeof(dialDigits));
}

// Detects long unchanged state periods.
// Doeas not wait. Returns instantly. Must be run in endless loop.
// returns 
//      0 - picked up; no digits dialed yet
//    > 0 - number of digits dialed
//    < 0 - hanged up; negative number of dialed digits decreased by 1
// dialed number is stored in global dialDigits[]
int dialDigitDetected()
{
  static int  dialPhase;             // 0-idle (hanged up); 1-picked up; 2-pulsing
  //static long resetDelayDial = 100;  // pulse length threshold (ms)
  
  static long lastDialPinChangeTime = 0;
  static int  dialCounter; // keeps current digit value during pulsing phase
  
  // this part checks if dial eom condition hapened
  if(millis() - lastDialPinChangeTime > RESET_DELAY_DIAL)
  {
    switch(dialPhase)
    {
      case 0:  // idle
              if(lastDialPinState == LOW)
              {
                // Serial.println("idle + H/L => earphone picked up");
                // idle + H/L => earphone picked up
                // dialing did not start yet
                //dialDigitsNum = -1;  // clear digits counter
                resetDialDigits();
                dialPhase = 1;       // set "earphone up" phase
              }
              break;
      case 1: // earphone up
              if(lastDialPinState == HIGH) {
                // Serial.println("long H while earphone up - hanged up");
                dialPhase = 0;       // get idle
                dialDigitsNum = -dialDigitsNum - 3;
              }
              break;
      case 2: // pulsing
              if(lastDialPinState == HIGH) {
                // Serial.println("long H after pulsing - apparently hanged up while dialing");
                dialCounter = 0;     // reset erroneous digit
                dialPhase = 0;       // get idle
              }
              else {
                // Serial.println("long low after pulsing - finished dialing a digit");
                dialPhase = 1;       // set the earphone up phase
                dialDigitsNum++;     // save the next digit
                if(dialDigitsNum == MAX_DIAL_NUMBER_LEN)
                  dialDigitsNum = 0; // reset dialed number if length exceeds buffer
                dialDigits[dialDigitsNum] = '0' + (dialCounter < 10 ? dialCounter : 0);
                dialDigits[dialDigitsNum+1] = '\0';
              }
              break;
    }
    lastDialPinChangeTime = millis();
    return dialDigitsNum + 1;
  }
  
  if(dialPinStateChanged())
  {
    lastDialPinChangeTime = millis(); // for the long state detection
    switch(dialPhase)
    {
      case 1:  // earphone up phase
              if(lastDialPinState == HIGH) { 
                                     // L/H => first pulse arriving or hanging up
                dialCounter = 0;     // reset pulse counter
              }
              else {                 // earphone up + H/L => first pulse in series arrived
                dialPhase = 2;       // set pulsing phase
                dialCounter++;
              }
              break;
      case 2:  // pulsing phase
              if(lastDialPinState == LOW)
                dialCounter++;  // pulsing + H/L => next pulse in series arrived
              break;
    }
  }
  return dialDigitsNum + 1;
}





#define TONE_NONE    0
#define TONE_READY   1
#define TONE_FREE    2
#define TONE_BUSY    3

void setTone(int mode) {
  static int duty;
  static int full_period;
  static int phase;
  static int phase_step;
  
  phase += phase_step;
  phase %= full_period;
  
  switch(mode) {
    case TONE_NONE:
      duty = 1;
      full_period = 3;
      phase = duty;
      phase_step = 0;
      break;
    case TONE_READY:
      duty = 1;
      full_period = 3;
      phase = 0;
      phase_step = 0;
      break;
    case TONE_FREE:
      duty = 2000;
      full_period = 8000;
      phase = 0;
      phase_step = 1;
      break;
    case TONE_BUSY:
      duty = 800;
      full_period = 1600;
      phase = 0;
      phase_step = 1;
      break;
  }

  if(phase == 0)
      digitalWrite(tonePin, HIGH);  // turn tone on
  if(phase == duty)
      digitalWrite(tonePin, LOW);   // turn tone off
}

char sound[200];  // buffer of digits: pitch, length
void effect(int effectNumber) {
  int del = dels[0];
  int dur;
      setTone(TONE_NONE);  // initially make it silent
      switch(effectNumber) {
        case 11:  // czekanie na podniesienie sluchawki
            setTone(TONE_FREE);
            break;
        case 12:  // losowy trzask
            for(int ff=0; ff<150; ff++) {
              digitalWrite(soundOutputPin, random(4));
            }
            break;
        case 13:  // nie ma takiego numeru
            dur = 50 * (10000 / del);
            for(int ff=0; ff<dur; ff++) {
              for(int t=0;t<del*1;t++) dum = dum+1-1;
                  digitalWrite(soundOutputPin, LOW);
              for(int t=0;t<del*1;t++) dum = dum+1-1;
                  digitalWrite(soundOutputPin, HIGH);
            }
            dur = del - 1 + 1;
            del = dels[1];
            dur = 50 * (10000 / del);
            for(int ff=0; ff<dur; ff++) {
              for(int t=0;t<del*1;t++) dum = dum+1-1;
                  digitalWrite(soundOutputPin, LOW);
              for(int t=0;t<del*1;t++) dum = dum+1-1;
                  digitalWrite(soundOutputPin, HIGH);
            }
            del = dels[2];
            dur = 50 * (10000 / del);
            for(int ff=0; ff<dur; ff++) {
              for(int t=0;t<del*1;t++) dum = dum+1-1;
                  digitalWrite(soundOutputPin, LOW);
              for(int t=0;t<del*1;t++) dum = dum+1-1;
                  digitalWrite(soundOutputPin, HIGH);
            }
            break;
        case 14:
            setTone(TONE_BUSY);
            break;
      }
}

void beep(char *input) {
  static int curNoteIdx = 0;
  static int curNoteVal;
  static int phase = 0;
  static char freq[100];
  static int dur[100];
  static int curNoteDur;
  static int soundlen;
  static char outlevel;
  int inplen = strlen(input);

  if(*input == 'x') {
      soundlen = 0;
      return;
  }
  inplen = inplen > 200 ? 200/2 : inplen/2;
  // setup sound
  if(inplen > 0) {
    // read input tones 
    for(int ff=0; ff<inplen; ff++) {
      freq[ff] = input[ff*2];
      dur[ff] = input[ff*2+1] * 2;
#ifdef DEBUG_DIAL
    Serial.print((int)freq[ff]);
    Serial.print(",");
    Serial.print((int)dur[ff]);
    Serial.print(" ");
#endif
    }
    soundlen = inplen;
    Serial.print("soundlen: ");
    Serial.println(soundlen);
    curNoteIdx = 0;
    curNoteVal = freq[curNoteIdx];
    curNoteDur = dur[curNoteIdx] * 500;
    phase = 0;
    effect(curNoteVal);
  }
  
  // play sound
  if(curNoteIdx < soundlen) {
      phase++;
      if(curNoteVal < 11)
            if(phase % curNoteVal == 0) {
                outlevel = 1 - outlevel;
                digitalWrite(soundOutputPin, outlevel);
             }
      phase %= curNoteDur;
      if(phase == 0) {
        Serial.println("fire effect");
          curNoteIdx++;
          curNoteVal = freq[curNoteIdx];
          curNoteDur = dur[curNoteIdx] * 500;
       effect(curNoteVal);
      }
  }
  else
      digitalWrite(soundOutputPin, LOW);
}

void setSoundTxt(char *txt) {
    int len = strlen(txt) / 2;
    memset(sound, 0, sizeof(sound));
    for(int ff=0; ff<len; ff++) {
        sound[ff*2] = txt[ff*2] - '0' + 1;
        sound[ff*2+1] = txt[ff*2+1] - '0';
    }
    beep(sound);
}
void setSoundTxtSteady(char *txt, char notelen) {
    int txtlen = strlen(txt);
    memset(sound, 0, sizeof(sound));
    for(int ff=0; ff<txtlen; ff++) {
        sound[ff*2] = txt[ff] + 3 - '0';
        sound[ff*2+1] = notelen;
    }
    beep(sound);
}

// END OF DIAL WHEEL ROUTINES










// the setup routine runs once when you press reset:
void setup() {
  pinMode(led, OUTPUT); 

  // dial pins
  pinMode(dialInputPin, INPUT);
  pinMode(soundOutputPin, OUTPUT);
  pinMode(tonePin, OUTPUT);

  // Pins for TDA1543 DAC
  pinMode(ws, OUTPUT);     
  pinMode(bck, OUTPUT);     
  pinMode(data, OUTPUT);     
  randomSeed(1809);

  // plotter pin
  pinMode(mode, INPUT);  // 0 - work, 1 - test
  pinMode(powerOff, OUTPUT);  // 0 - plotter power ON
  set_target(0, 0);
  
  Serial.begin(9600); 
}







  static char test_mode = 0;
  void setTestMode (int mode) {
    if(mode == 1)
      test_mode = digitalRead(mode);     // turn on
    else if(mode == 0)
      test_mode = 1 - digitalRead(mode); // turn off
    else
      test_mode = 1 - test_mode;         // toggle
  };


void loop() {
  // dac variables
  static int ii;
  
  // dial wheel variables
  int dc = 0;
  static int last_dc = 0;
  static char sound[200];  // input buffer for beep()
  
  
  // DAC controlling section
  // send data to DAC
  send_data(val1, val2);
  
  if(story_length != 0) {
  if(ii >= 5000) {
    ii = 0;
    if(digitalRead(mode) == test_mode) {
        next_test_mode_target();
    } else {
        target_val1 = random(-32768, 32767);
        target_val2 = random(-32768, 32767);
        
        if(story_length > 0)
            story_length--;
    }
    set_target(target_val1, target_val2);
  }
  // if i == 0 do not do anything; just wait for reaching the target
  if (ii > 0) {
    ii++;
  }
  if( step_ahead() ) { // TRUE = target reached
    if(ii == 0)
      ii = 1; // bump it to make it counting
  }
    digitalWrite(powerOff, 0);  // Turn plotter ON
  } // if story_length
  else
    digitalWrite(powerOff, 1);  // turn plotter OFF

  // End of DAC controlling section
  






  // Dial wheel controlling section
  
  digitalWrite(led, digitalRead(dialInputPin));   // turn the LED on (HIGH is the voltage level)
     
  dc = dialDigitDetected();
  beep("");     // manage tones generated by uP
  setTone(-1);  // manage tones generated by external device
  
  if(dc != last_dc)
  {
    last_dc = dc;
    if(dc == 0) {
      #ifdef DEBUG_DIAL
      Serial.print(dc);
      Serial.println(": Picked up.");
      #endif
      setTone(TONE_READY);
    }
    if(dc < 0) {
      #ifdef DEBUG_DIAL
      Serial.print(dc);
      Serial.println(": Hanged up.");      
      #endif
      setTone(TONE_NONE);
      memset(sound, 0, sizeof(sound));
      beep("x");  // clear beeps
    }
    if(dc > 0)
    {
      if(dc == 1) {
         setTone(TONE_NONE);
         memset(sound, 0, sizeof(sound));
         beep("x");
      }
      // Put number driven commands here
      
      if(strncmp(dialDigits, "5", 1) == 0) {
        /*
        memset(sound, 0, sizeof(sound));
        sound[0] = 11;   // sygnal wolne
        sound[1] = 200 + random(50);
        sound[2] = 12;   // trzask
        sound[3] = 1;
        beep(sound);
        */
            memset(sound, 0, sizeof(sound));
            sound[0] = 13;   // sygnal nie ma takiego numeru
            sound[1] = 5;
            sound[2] = 13;   // sygnal nie ma takiego numeru
            sound[3] = 5;
            sound[4] = 13;   // sygnal nie ma takiego numeru
            sound[5] = 5;
            sound[6] = 14;   // busy
            sound[7] = 50;
            beep(sound);

      }
      else
      
      if(strncmp(dialDigits, "713216216", (dc < 9) ? dc : 9) == 0) {
        // catch prefix 713216216 + n + xxxx  -> n=1,2,3  xxxx=delay
        // set one of the three tones in "number not available"
        if(dc == 14)
        {
            int del, dur, nn;
            sscanf(&dialDigits[10], "%d", &del);
            nn = dialDigits[9]-'1';
            if(nn > 2) nn = 2;
            dels[nn] = del;
            dur = 50 * (10000 / del);
            Serial.print(del);
            Serial.print("/");
            Serial.println(dur);
            for(int ff=0; ff<dur; ff++) {
              for(int t=0;t<del*1;t++) dum = dum+1-1;
                  digitalWrite(soundOutputPin, LOW);
              for(int t=0;t<del*1;t++) dum = dum+1-1;
                  digitalWrite(soundOutputPin, HIGH);
            }
        }
      }
      else
      if(strncmp(dialDigits, "7173017", 7) == 0) {
        // 7173017 toggles DAC test mode
          setTestMode(-1);  // toggle test mode (DAC)
          #ifdef DEBUG_STORY
          Serial.print("Test Mode ");
          Serial.println(digitalRead(mode) == test_mode);
          #endif
          // resetDialDigits(); 
          story_length = -1;
          setSoundTxt("01110141");
      }
      else
      if(dialDigits[0] == '9' && dc <= 7) {
        // 7 digit number starting with 9 sets seed for DAC story
        if(dc == 7) {
           // set new story (seed)
           int seed;
           sscanf(&dialDigits[1], "%d", &seed);
           randomSeed(seed);
           story_length = random(20) + 3;
           setTestMode(0);
           
           setSoundTxtSteady(dialDigits, 1);
           // resetDialDigits(); 
          #ifdef DEBUG_STORY
          sprintf(outdbg, "seed %d, length %d", seed, story_length);
          Serial.println(outdbg);
          #endif
        }
      }
      else
      if(strncmp(dialDigits, "71", 2) == 0) {
        // all 9 digit numbers starting 71 can be free or busy
        if(dc == 9) {
           // po sygnale ktos odbiera
           if(random(2)) {
               memset(sound, 0, sizeof(sound));
               sound[0] = 11;   // sygnal wolne
               sound[1] = 200 + random(50);
               sound[2] = 12;   // trzask
               sound[3] = 1;
               beep(sound);
           }
           else {
               setTone(TONE_BUSY); // zajete
           }
           //resetDialDigits(); 
        }
      }
      else
      if(dc > 12) {
        // all numbers longer than 12 are unknown
            memset(sound, 0, sizeof(sound));
            sound[0] = 13;   // sygnal nie ma takiego numeru
            sound[1] = 10;
            sound[0] = 12;   // sygnal nie ma takiego numeru
            sound[1] = 100;
            sound[0] = 13;   // sygnal nie ma takiego numeru
            sound[1] = 100;
            beep(sound);
      }
      else
      if(dc > 10)
          setTone(TONE_FREE);
      else
          setTone(TONE_NONE);


        /*
        memset(sound, 0, sizeof(sound));
        for(int ff=0; ff<dc; ff++) {
            sound[ff*2] = dialDigits[ff] + 3 - '0';
            sound[ff*2+1] = 2;
        }
        beep(sound);
        */
#ifdef DEBUG_DIAL
        Serial.print("Dialed Number[");
        Serial.print(dc);
        Serial.print("]: ");
        Serial.print(dialDigits);
        Serial.println("");
#endif
      } // dc > 0
  } // dc != lastdc

  // End of dial wheel controlling section
  
}
