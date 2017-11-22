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

int caret_speed = 20;
double dstep1 = caret_speed;
double dstep2 = caret_speed;


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

// the loop routine runs over and over again forever:
void loop() {
  //delay(1);
  
  send_data(val1, val2);
  if(ii >= 500) {
    
    // Randomize it HARD!
    target_val2 = analogRead(0) * random(-32768, 32767);
    randomSeed(analogRead(1));
    target_val1 = random(-32768, 32767);
    ii = 0;
    Serial.print("Setting target_val1 = ");
    Serial.println(target_val1);

    // set target and respective steps to have both channels
    // reach their targets at the same time
    set_target(target_val1, target_val2);
  }
  // if i == 0 do not do anything
  if (ii > 0) {
    ii++;
  }
 
  if(digitalRead(mode) == HIGH) {
    if(ii == 0) {
      val1 = random(65535)-32767;
      val2 = random(65535)-32767;
    }
  }
  else {  

    /*    
    if(val1 >=32600)  // pion
      val1 = 0x8000;
    else
      val1 += 10;

    if(val2 >=32600)  // poziom
      val2 = 0x8000;
    else
      val2 += 10;
    */
    
    if(abs(dval1 - target_val1) > caret_speed + 1) { // pion
      dval1 += dstep1;
    }
    else
    if(ii == 0)
      ii = 1;  // bump ii to make it counting

    if(abs(dval2 - target_val2) > caret_speed + 1)  // poziom
      dval2 += dstep2;
    else
    if(ii == 0)
      ii = 1;  // bump ii to make it counting
    
    val1 = (short) (dval1);
    val2 = (short) (dval2);

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

   // val1 = (short) (target_val1 - 32768);
   // val2 = (short) (target_val2 - 32768);
    
   // val1 = (short) (0 - 32768);
   // val2 = (short) (random(65535) - 32768);
  }
}
