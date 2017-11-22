/*
  xy_recorder.ino
  
  Sends data to TDA1543 double DAC
  
 */
 
// DAC PINs
int ws = 2;
int bck = 3;
int data = 4;

int ii = 0;
short val = 0xFF;

// the setup routine runs once when you press reset:
void setup() {                
  // initialize the digital pin as an output.
  pinMode(ws, OUTPUT);     
  pinMode(bck, OUTPUT);     
  pinMode(data, OUTPUT);     
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

// the loop routine runs over and over again forever:
void loop() {
  //delay(1);
  
  send_data(val, val);
  if(val >=32600)
  {
    ii = 0;
    val = 0x8000;
  }
  else {
    ii++;
    val += 100;
  }
}
