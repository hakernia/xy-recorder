/*

Recorder XY v4 + dial controller + ringer
Getting rid of the software tones. This shit stopped working after compiler upgrade
   (possibly the better optimization screwed delay loops) and morover 
   it causes strange troubles (uP gets spinning so that reset does not help).
   No point to investigate it.

v02 - dodane rozne telefony
 */

#include <Wtv020sd16p.h>

#undef DEBUG_DAC
#undef DEBUG_DIAL
#undef DEBUG_STORY


// wtv020 pin
int resetPin = 10; //5;  //2 
int clockPin = 11; //6; //3;  
int dataPin =  12; //9;  //4;  
int busyPin = 19;  //5;  // not used if async play only

// init wtv management object
Wtv020sd16p wtv020sd16p(resetPin,clockPin,dataPin,busyPin);

#define SPK_UNKNOWN_NUMBER  10
#define SPK_ZERO  0
#define SPK_PL  0
#define SPK_EN 20
#define SPK_SP 40


// Pin 13 has an LED connected on most Arduino boards.
int led = 13;

// plotter pins
int mode = 5;  //12;   // input: 0 - work, 1 - test
int powerOff = 6;  //9;  // 0 - plotter ON, 1 - plotter OFF

// DAC pins
int ws = 2;
int bck = 3;
int data = 4;

// dial pins
int dialInputPin = 8;  //7
int soundOutputPin = 7;  //8
int tonePin = 9; //10;
int ringerPin = A0;

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

#ifdef DEBUG_DAC
char outdbg[300];
#endif





#define EVT_01_BURPS_CALLING    1
#define EVT_02_DEVILS_CALLING   2
#define EVT_03                  3

// Time variables
unsigned long  secondsFromMidnight0;
unsigned long  currentEventStart = -1;
unsigned long  currentEventDuration = 20;
unsigned char  currentEventId;
unsigned char  incoming_call = 0;  // false - dialed call, true - incoming call


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
  static unsigned long lastDebounceTime = 0;
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

void makeUpDialDigits(char *buf) {
  dialDigitsNum = strlen(buf);
  strncpy(dialDigits, buf, dialDigitsNum);
}
// END OF DIAL WHEEL ROUTINES




// HW TONE GENERATOR MANAGEMENT ROUTINES

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

// END OF HW GENERATOR TONE ROUTINES




// SW SIGNAL ROUTINES
#define  MAX_INPLEN  50
unsigned int sound[MAX_INPLEN];  // buffer of digits: pitch, length
unsigned char snd;  // global index in sound[]
// 11 - sygnal wolne
// 12 - trzask
// 13 - sygnal nie ma takiego numeru
// 14 - busy
unsigned int sound_answer[]    = {11,1000,12,100,0,0};  // [1]:200-250, [4]-effect [5]-effect duration
unsigned int sound_busy[]      = {14,10000, 0,0};
unsigned int sound_unknownum[] = {13,500, 13,500, 13,500, 14,1000, 0,0};


void effect(int effectNumber) {
  int del = dels[0];
  int dur;
  int dum1 = 2;
      setTone(TONE_NONE);  // initially make it silent
      switch(effectNumber) {
        case 11:  // czekanie na podniesienie sluchawki
            setTone(TONE_FREE);
            break;
        case 12:  // losowy trzask
        /*
            for(int ff=0; ff<150; ff++) {
              digitalWrite(soundOutputPin, (random(4) < 2));
            }
            break;
        */
        case 13:  // nie ma takiego numeru
        /*
            dur = 50 * (10000 / del);
            for(int ff=0; ff<dur; ff++) {
              for(int t=0;t<del*1;t++) memcpy(dum, dum1, sizeof(dum)); dum = dum+dum1-1;
                  digitalWrite(soundOutputPin, LOW);
              for(int t=0;t<del*1;t++) memcpy(dum, dum1, sizeof(dum));dum1 = dum1+dum+1-1;
                  digitalWrite(soundOutputPin, HIGH);
            }
            dur = del - 1 + 1;
            del = dels[1];
            dur = 50 * (10000 / del);
            for(int ff=0; ff<dur; ff++) {
              for(int t=0;t<del*1;t++) memcpy(dum, dum1, sizeof(dum)); dum= dum+dum1-1;
                  digitalWrite(soundOutputPin, LOW);
              for(int t=0;t<del*1;t++) memcpy(dum, dum1, sizeof(dum));dum1 = dum1+dum+1-1;
                  digitalWrite(soundOutputPin, HIGH);
            }
            del = dels[2];
            dur = 50 * (10000 / del);
            for(int ff=0; ff<dur; ff++) {
              for(int t=0;t<del*1;t++) memcpy(dum, dum1, sizeof(dum));; dum = dum+dum1-1;
                  digitalWrite(soundOutputPin, LOW);
              for(int t=0;t<del*1;t++) memcpy(dum, dum1, sizeof(dum));dum1 = dum1+dum+1-1;
                  digitalWrite(soundOutputPin, HIGH);
            }
        */
            wtv020sd16p.asyncPlayVoice(SPK_UNKNOWN_NUMBER + SPK_PL);
            break;
        case 14:
            setTone(TONE_BUSY);
            break;
        default:
            if(effectNumber >= 1000)
                wtv020sd16p.asyncPlayVoice(effectNumber - 1000);
                
        dum1 = dum;  // anti optimizer
      }
}

void beep(unsigned int *input) {
  static unsigned int curNoteIdx = 0;
  static unsigned int curNoteVal;
  static unsigned long phase = 0;
  static unsigned int freq[MAX_INPLEN/2];
  static unsigned int dur[MAX_INPLEN/2];
  static unsigned long curNoteDur;
  static unsigned int soundlen;
  static unsigned char outlevel;
  unsigned int inplen = 0;
  
  if(*input > 0) {
      for(int ff=1; ff<MAX_INPLEN; ff++)
      if(input[ff] == 0) {
          inplen = ff;
          break;
      }
  }
  
  if(*input == 1) {
      soundlen = 0;
      return;
  }
  inplen = inplen > MAX_INPLEN ? MAX_INPLEN/2 : inplen/2;
  // setup sound
  if(inplen > 0) {
    memset(freq, 0, sizeof(freq));
    // read input tones 
    for(int ff=0; ff<inplen; ff++) {
      freq[ff] = input[ff*2];
      dur[ff] = input[ff*2+1];
#ifdef DEBUG_DIAL
    Serial.print((int)freq[ff]);
    Serial.print(",");
    Serial.print((int)dur[ff]);
    Serial.print(" ");
#endif
    }
    soundlen = inplen;
#ifdef DEBUG_DIAL
    Serial.print("soundlen: ");
    Serial.println(soundlen);
#endif
    curNoteIdx = 0;
    curNoteVal = freq[curNoteIdx];
    curNoteDur = dur[curNoteIdx];
    curNoteDur *= 10;
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
#ifdef DEBUG_DIAL
          Serial.println("fire effect");
#endif
          curNoteIdx++;
          curNoteVal = freq[curNoteIdx];
          curNoteDur = dur[curNoteIdx];
          curNoteDur *= 10;
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

void resetSound() {
    memset(sound, 0, sizeof(sound));
    snd = 0;
}
char addSound(unsigned int effect, unsigned int duration) {
    if(snd + 1 < MAX_INPLEN) {
        sound[snd++] = effect;
        sound[snd++] = duration;
        return true;
    }
    return false;
}
void addSoundArray(unsigned int arr[]) {
    int ff = 0;
    while(arr[ff] != 0) {
      addSound(arr[ff], arr[ff+1]);
      ff += 2;
    }
}

void addSoundAnswer() {
    addSound(11, 500 + random(1000));
}
void addSoundBusy() {
    addSound(14, 20000);
}

// END OF SW SIGNAL ROUTINES





// TIME MANAGEMENT FUNCTIONS

unsigned long lastMillis = 0;
long timeIn(char days, char hrs, char mins, char secs) {
  return days * 60*60*24 + hrs * 60*60 * mins * 60 + secs;
}

unsigned char cevts;  // temp counter of events being currently active 
unsigned char evt_num;  // temp number of event being processed
#define MAX_EVENTS  10
unsigned long eventStart[MAX_EVENTS];
unsigned int eventDuration[MAX_EVENTS];
unsigned char eventType[MAX_EVENTS];  // 0-execute once, 1-recurring daily
unsigned char eventId[MAX_EVENTS];
unsigned char eventCount;

// eventId == 0 - no event

#define EVT_TYPE_ONCE              0
#define EVT_TYPE_RECURRING_DAILY   1
#define EVT_TYPE_RECURRING_HOURLY  2

char scheduleEvent(unsigned long newEventStart,
                   unsigned long newEventDuration,
                   unsigned char newEventType,
                   unsigned char newEventId) {
    unsigned char evt;
    for(evt=0; evt<MAX_EVENTS; evt++) {
      if(eventStart[evt] == -1) {
        eventStart[evt] = newEventStart;
        eventDuration[evt] = newEventDuration;
        eventType[evt] = newEventType;
        eventId[evt] = newEventId;
        return true;
      }
    }
    return false;
}
void deleteEvt(unsigned char evt) {
    eventStart[evt] = -1;
    eventDuration[evt] = 0;
    eventType[evt] = 0;
    eventId[evt] = 0;
}
void finalizeEvt(unsigned char evt) {
   if(eventType[evt] == EVT_TYPE_ONCE) { 
       deleteEvt(evt);
   } else
   if(eventType[evt] == EVT_TYPE_RECURRING_DAILY) { 
       eventStart[evt] += 86400;  // move start 24 hrs
   } else
   if(eventType[evt] == EVT_TYPE_RECURRING_HOURLY) { 
       eventStart[evt] += 3600;  // move start 1 hr
   }
}
char deleteEvent(unsigned char deleteEventId) {
    unsigned char evt;
    for(evt=0; evt<MAX_EVENTS; evt++) {
      if(eventId[evt] == deleteEventId) {
          deleteEvt(evt);
          return true;
      }
    }
    return false;
}
char finalizeEvent(unsigned char finalizeEventId) {
    unsigned char evt;
    for(evt=0; evt<MAX_EVENTS; evt++) {
      if(eventId[evt] == finalizeEventId) {
           finalizeEvt(evt);
           return true;
      }
    }
    return false;
}
unsigned char countCurrentEvents() {
    unsigned char evt;
    unsigned char evt_num = 0;
    for(evt=0; evt<MAX_EVENTS; evt++) {
      if(eventStart[evt] < secondsFromMidnight0) {
         if(secondsFromMidnight0 < eventStart[evt] + eventDuration[evt]) {
           // one of the currently active
           evt_num++;
         }
         else {
           finalizeEvt(evt);
         }
      }
    }
    return evt_num;
}
char getCurrentEvent(unsigned char event_num,
                     unsigned long *foundEventStart,
                     unsigned long *foundEventDuration,
                     unsigned char *foundEventId) {
    unsigned char evt;
    unsigned char evt_num = 0;
    for(evt=0; evt<MAX_EVENTS; evt++) {
      if(eventStart[evt] < secondsFromMidnight0 &&
         secondsFromMidnight0 < eventStart[evt] + eventDuration[evt]) {
        if(evt_num++ == event_num) {
          *foundEventStart = eventStart[evt];
          *foundEventDuration = eventDuration[evt];
          *foundEventId = eventId[evt];
          return true;
        }
      }
    }
    return false;
}
// END OF TIME MANAGEMENT FUNCTIONS




#define RING_MSEC 500

unsigned short ring(unsigned short ringDur, unsigned short gapDur) {
  static unsigned long lastMillis = 0;
  static unsigned short ring_dur;
  static unsigned short gap_dur;
  int ring_msec = 0;

  if(ringDur > 0) {
    ring_dur = ringDur;
    gap_dur = gapDur;
  }
  
  if(millis() - lastMillis < ring_dur) {
      digitalWrite(ringerPin, HIGH);
      delay(ring_dur);
      digitalWrite(ringerPin, LOW);
  }
  else {
      if(millis() - lastMillis > ring_dur + gap_dur) {
          lastMillis = millis();
      }
  }
}




// the setup routine runs once when you press reset:
void setup() {
  pinMode(led, OUTPUT); 

  // dial pins
  pinMode(dialInputPin, INPUT);
  pinMode(soundOutputPin, OUTPUT);
  pinMode(tonePin, OUTPUT);
  pinMode(ringerPin, OUTPUT);
  digitalWrite(ringerPin, LOW);  // LOW = do not ring

  // Pins for TDA1543 DAC
  pinMode(ws, OUTPUT);     
  pinMode(bck, OUTPUT);     
  pinMode(data, OUTPUT);     
  randomSeed(1809);

  // plotter pin
  pinMode(mode, INPUT);  // 0 - work, 1 - test
  pinMode(powerOff, OUTPUT);  // 0 - plotter power ON
  set_target(0, 0);
  
  // Initializes the wtv sound module
  wtv020sd16p.reset();

  // Initialize time variables
  secondsFromMidnight0 = timeIn(0, 16, 00, 00);  // day 0, 16:00

  memset(eventStart, -1, sizeof(eventStart));
  memset(eventDuration, 0, sizeof(eventDuration));
  memset(eventId, 0, sizeof(eventId));
  
  Serial.begin(9600); 
}







  static char test_mode = 0;
  void setTestMode (int new_mode) {
    if(new_mode == 1)
      test_mode = 1; //digitalRead(mode);     // turn on
    else if(new_mode == 0)
      test_mode = 0; //1 - digitalRead(mode); // turn off
    else
      test_mode = 1 - test_mode;         // toggle
  };


void loop() {
  // dac variables
  static int ii;
  
  // dial wheel variables
  int dc = 0;
  static int last_dc = 0;
//  static char sound[MAX_INPLEN];  // input buffer for beep()
  static unsigned int zeroSound[2] = {0,0};
  static char pis_sequence = 0;  // used to unlock sections depending on pis talk
  
  
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
  



  dc = dialDigitDetected();




  // Time management
  if(millis() < lastMillis)
    lastMillis = millis();  // handle millis overflow
  if(millis() - lastMillis > 1000) {
    secondsFromMidnight0 += (millis() - lastMillis) / 1000;  // bump seconds counter
    lastMillis = millis();                                   // reset millis detector
  }

  if(dc < 0)  // do only if hanged up
  if((cevts = countCurrentEvents()) > 0) {
    for(evt_num=0; evt_num < cevts; evt_num++) {
      if(getCurrentEvent(evt_num, &currentEventStart, &currentEventDuration, &currentEventId)) {
        // one of the currently active events has been fetched
        switch(currentEventId) {
          case EVT_01_BURPS_CALLING: // dzwonia pobekiwania
               ring(1000, 4000);  // ring duration, gap duration
               break;
          /*
          case EVT_02_HOURLY_CALLING:
               ring(1000, 4000);
               break;
          */
        }
      };
    }
  }
  else {
      currentEventId = 0; // no current events!
  }
  /*
  if(dc < 0)  // do only if hanged up
  if(currentEventStart < secondsFromMidnight0 &&
     secondsFromMidnight0 < currentEventStart + currentEventDuration) {
     ring(1000, 4000);
     //if(ring(1000, 1000) == 0)
     //     currentEventStart = -1;
  }
  */
  // End of time management





  // Dial wheel controlling section
  
  digitalWrite(led, digitalRead(dialInputPin));   // turn the LED on (HIGH is the voltage level)
     
  beep(zeroSound);     // manage tones generated by uP
  setTone(-1);  // manage tones generated by external device
  
  if(dc != last_dc)  // enter only if new digit dialed or handset status changed
  {
    last_dc = dc;



    
    if(dc < 0) {
      #ifdef DEBUG_DIAL
      Serial.print(dc);
      Serial.println(": Hanged up.");      
      #endif
      setTone(TONE_NONE);
      memset(sound, 0, sizeof(sound));
      sound[0] = 1; // clear beeps
      beep(sound);
      wtv020sd16p.stopVoice();  //silent
      incoming_call = 0;  // reset to default
    }
    else
    {
      // Picked up. 
      // implement reactions to calling in events here
      // they usually do not depend on dialed numbers

    if (1)
    {

        
    if(dc == 0) {  // nothing dialed yet
      #ifdef DEBUG_DIAL
      Serial.print(dc);
      Serial.println(": Picked up.");
      #endif




      if(currentEventId == EVT_01_BURPS_CALLING) {
        /*
               sound[0] = 1056;  // ziewy
               sound[1] = 10000;  
               sound[2] = 14;   // busy
               sound[3] = 20000;
               beep(sound);
        */
        incoming_call = true;
        finalizeEvent(EVT_01_BURPS_CALLING); // delete or reschedule, depending on eventType
        currentEventId = 0;                // unmark current event, it has just been executed
        makeUpDialDigits("757173017");     // this is the number that is calling
                                           // the next loop will get to proper dc > 0
      } 
      else  // no event of incoming call; perform the ready tone
        setTone(TONE_READY);



        
    }
    if(dc > 0)
    {
      if(dc == 1) {
         setTone(TONE_NONE);
         memset(sound, 0, sizeof(sound));
         sound[0] = 1; // clear beeps
         beep(sound);  
      }
      // Put number driven commands here
      
      if(strncmp(dialDigits, "4", 1) == 0) {
        /*
        memset(sound, 0, sizeof(sound));
        sound[0] = 11;   // sygnal wolne
        sound[1] = 200 + random(50);
        sound[2] = 12;   // trzask
        sound[3] = 1;
        beep(sound);
        */
            //memset(sound, 0, sizeof(sound));
            resetSound();
            //memcpy(sound, sound_unknownum, sizeof(sound_unknownum));
            addSoundArray(sound_unknownum);
            /*
            sound[0] = 13;   // sygnal nie ma takiego numeru
            sound[1] = 5;
            sound[2] = 13;   // sygnal nie ma takiego numeru
            sound[3] = 5;
            sound[4] = 13;   // sygnal nie ma takiego numeru
            sound[5] = 5;
            sound[6] = 14;   // busy
            sound[7] = 50;
            */
            beep(sound);

            //scheduledEventStart = secondsFromMidnight0 + timeIn(0, 0, 0, 20);
            scheduleEvent(secondsFromMidnight0 + timeIn(0, 0, 0, 20), 20, 
                          EVT_TYPE_RECURRING_HOURLY, 
                          EVT_01_BURPS_CALLING);
      }
      else
      
      if(strncmp(dialDigits, "713215215", (dc < 9) ? dc : 9) == 0) {
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
            /*
            for(int ff=0; ff<dur; ff++) {
              for(int t=0;t<del*1;t++) dum = dum+1-1;
                  digitalWrite(soundOutputPin, LOW);
              for(int t=0;t<del*1;t++) dum = dum+1-1;
                  digitalWrite(soundOutputPin, HIGH);
            }
            */
        }
      }
      else
      if(strncmp(dialDigits, "757173017", 9) == 0) {
           // po sygnale ktos odbiera
               //memset(sound, 0, sizeof(sound));
               resetSound();
               if(!incoming_call) {
                   // add waiting signals etc.
                   //addSoundArray(sound_answer);  // sygnal wolne, trzask
                   addSoundAnswer();
               }
               addSound(1056, 10000);  // ziewy
               addSound(14, 20000);    // busy tone
               beep(sound);
      }
      else
      if(strncmp(dialDigits, "226215035", 9) == 0) {  // pis
           // po sygnale ktos odbiera
               resetSound();
               if(!incoming_call) {
                   addSoundAnswer();
               }
               pis_sequence = random(5);
               //sound[4] = 1050+pis_sequence;  // pis
               //sound[5] = 60000;  
               addSound(1050+pis_sequence, 6000);
               addSound(14, 2000);
               beep(sound);
      }
      else
      if(strncmp(dialDigits, "5665523", 7) == 0) {  // rm
           // po sygnale ktos odbiera
           if(dc == 9)  // wszystkie 9-cio cyfrowe nry zaczynajace sie od 5665523
           {
               resetSound();
               if(!incoming_call) {
                   addSoundAnswer();
               }               
               sound[4] = 1055;  // trzy slowa do ojca prowadzacego
               sound[5] = 20000;  
               addSound(1055, 2000);
               addSound(14, 20000);
               beep(sound);
           }
      }
      else
      if(strncmp(dialDigits, "0001", 4) == 0) {
          if(dc >= 13) {        
           // po sygnale ktos odbiera
               resetSound();
               if(!incoming_call) {
                   addSound(1011 + random(2), 1000 + random(500));  // 1011/1012 - ringing/busy US
               }
               beep(sound);
          }
      }
      else
      if(strncmp(dialDigits, "0044", 4) == 0) {
          if(dc >= 13) {
           // po sygnale ktos odbiera
               resetSound();
               if(!incoming_call) {
                   addSound(1013 + random(2), 1000 + random(500));  // 1011/1012 - ringing/busy UK
               }
               beep(sound);
          }
      }
      else
      if(pis_sequence == 3 && strncmp(dialDigits, "666", 3) == 0) {  // pis 3 = hymn
           // po sygnale ktos odbiera
               resetSound();
               if(!incoming_call) {
                addSound(1057, 1000); // POWITANIE SZATANA
               }
               addSound(14, 20000);
               beep(sound);
      }
      else
      if(strncmp(dialDigits, "922", 3) == 0) {  // wrozka
           // po sygnale ktos odbiera
               resetSound();
               if(!incoming_call) {
                addSound(1060 + random(3), 1000); // wrozby
               }
               addSound(14, 20000);
               beep(sound);
      }
      else
      if(strncmp(dialDigits, "926", 3) == 0) {
           // po sygnale ktos odbiera
               resetSound();
               if(!incoming_call) {
                addSound(1015, 800); // miedzymiastowa, tylko testowo w tym miejscu
               }
               addSound(14, 20000);
               beep(sound);
      }
      else
      if(strncmp(dialDigits, "928", 3) == 0) {
           // po sygnale ktos odbiera
               resetSound();
               if(!incoming_call) {
                addSound(1016, 8000); // mowi sie?, tylko testowo w tym miejscu
               }
               addSound(14, 20000);
               beep(sound);
      }
      else
      if(strncmp(dialDigits, "713216216", 9) == 0) {
        // toggles DAC test mode
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
           setTestMode(1);
           
           setSoundTxtSteady(dialDigits, 20);
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
           if(!incoming_call)
           if(random(2)) {
               resetSound();
               addSound(11, 500); // sygnal wolne
               addSound(17, 500); // prosze czekac
               addSound(17, 500); // prosze czekac
               addSound(14, 1500); // busy
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
           resetSound();
           if(!incoming_call) {
               addSoundArray(sound_unknownum);
               beep(sound);
           }
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

    } // else currentEventId ==
    
    } // else dc < 0
    
  } // dc != lastdc

  // End of dial wheel controlling section
  
}
