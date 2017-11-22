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
short val1 = 0xFF;
short val2 = 0xFF;

// warning: int is -32768..32767 on UNO!
long target_val1 = 0;
long target_val2 = 0;
double dval1 = (double)val1;
double dval2 = (double)val2;

int caret_speed = 5;
double dstep1 = caret_speed;
double dstep2 = caret_speed;

char outdbg[300];


// the setup routine runs once when you press reset:
void setup() {                
  // initialize the digital pin as an output.
  pinMode(ws, OUTPUT);     
  pinMode(bck, OUTPUT);     
  pinMode(data, OUTPUT);     

  pinMode(mode, INPUT);


  Serial.begin(9600); 

    target_val1 = random(65535);
    target_val2 = random(65535);
    
  randomSeed(1809);

}

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
  
  //trg1 = trg1 > 32767 ? 32767 : trg1;
  //trg2 = trg2 > 32767 ? 32767 : trg2;
/*  
    Serial.print("val1:");
    Serial.print(val1);
    Serial.print("  val2:");
    Serial.print(val2);

    Serial.print("  trg1:");
    Serial.print(trg1);
    Serial.print("  trg2:");
    Serial.print(trg2);
*/    
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

/*
    Serial.print("  dstep1:");
    Serial.print((long)dstep1);
    Serial.print("  dstep2:");
    Serial.print((long)dstep2);
    Serial.print("  ii:");
    Serial.println(ii);
*/
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

// the loop routine runs over and over again forever:
void loop() {
  //delay(1);
  
  send_data(val1, val2);
  
  if(ii >= 5000) {
    
    ii = 0;
    // caret_speed = random(5) + 5;

    if(digitalRead(mode) == HIGH) {
      if(ii == 0)
        target_val1 = (random(2)*2-1) * 32767;
        target_val2 = (random(2)*2-1) * 32767;

        //target_val1 = (random(3)-1) * 32767;
        //target_val2 = (random(3)-1) * 32767;
    } else
    {
      // Randomize hard
      //randomSeed(analogRead(1));
      target_val1 = random(-32768, 32767);
      target_val2 = random(-32768, 32767);
sprintf(outdbg, "seed:%d  val1:%ld  val2:%ld", 1809, target_val1, target_val2);
Serial.println(outdbg);
    }

    // set target and respective steps to have both channels
    // reach their targets at the same time
    set_target(target_val1, target_val2);
  }
  
  // if i == 0 do not do anything; just wait for reaching the target
  if (ii > 0) {
    ii++;
  }
 
 
  if( step_ahead() ) { // TRUE = target reached

/*
sprintf(outdbg, "ii:%d  val1:%d  trg1:%ld  dstep1:%ld    val2:%d  trg2:%ld  dstep2:%ld", 
                ii, 
                val1, target_val1, (long)(dstep1*100), 
                val2, target_val2, (long)(dstep2*100));
Serial.println(outdbg);
*/
    if(ii == 0)
      ii = 1; // bump it to make it counting
  }

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
