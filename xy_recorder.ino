/*
  xy_recorder.ino

  Sends data to TDA1543 double DAC

*/
 
// DAC PINs
int ws = 2;
int bck = 3;
int data = 4;

int mode = 12;

int ii = 0;
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

char outdbg[300];


// the setup routine runs once when you press reset:
void setup() {                
  // Pins for TDA1543 DAC
  pinMode(ws, OUTPUT);     
  pinMode(bck, OUTPUT);     
  pinMode(data, OUTPUT);     
  randomSeed(1809);


  pinMode(mode, INPUT);  // 0 - work, 1 - test


  Serial.begin(9600); 
}

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
  else {
    dstep1 = ((double)target_val1 - dval1) / abs((double)target_val2 - dval2) * (double)caret_speed;
    dstep2 = ((double)target_val2 - dval2) / abs((double)target_val2 - dval2) * (double)caret_speed;
  }
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

int next_test_mode_target() {
    static int arrived = 0;  // 0 - cross, 1 - V or H

    if(abs(target_val1) != 32767)
        target_val1 = -abs(target_val1)/target_val1 * 32767;
    if(abs(target_val2) != 32767)
        target_val1 = -abs(target_val2)/target_val2 * 32767;
        
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
}

// End of DAC functions







void loop() {
  //delay(1);

  // DAC controlling section
  // send data to DAC
  send_data(val1, val2);
  if(ii >= 5000) {
    ii = 0;
    if(digitalRead(mode) == HIGH) {
        next_test_mode_target();
    } else {
        target_val1 = random(-32768, 32767);
        target_val2 = random(-32768, 32767);
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
  // End of DAC controlling section
  
  
  
  

/*
    Serial.print((long)target_val1);
    Serial.print("  ");
    Serial.print((long)dval1);
    Serial.print("  ");
    Serial.print((long)dstep1);
    Serial.print("  ");
    Serial.print(ii);
    Serial.print("  ");
    Serial.println(val1);
*/
}
