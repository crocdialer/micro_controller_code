// 
// Arduinix Single Bulb 
// 
// This code runs a single bulb on both chip outputs.
//
// by Jeremy Howa
// www.robotpirate.com
// www.arduinix.com
// 2008 
//

// SN74141 : True Table
//D C B A #
//L,L,L,L 0
//L,L,L,H 1
//L,L,H,L 2
//L,L,H,H 3
//L,H,L,L 4
//L,H,L,H 5
//L,H,H,L 6
//L,H,H,H 7
//H,L,L,L 8
//H,L,L,H 9

// SN74141 (1)
int ledPin_0_a = 2;                
int ledPin_0_b = 3;
int ledPin_0_c = 4;
int ledPin_0_d = 5;
// SN74141 (2)
int ledPin_1_a = 6;                
int ledPin_1_b = 7;
int ledPin_1_c = 8;
int ledPin_1_d = 9;

// anod pins
int ledPin_a_1 = 10;
int ledPin_a_2 = 11;
int ledPin_a_3 = 12;

void setup() 
{
  pinMode(ledPin_0_a, OUTPUT);      
  pinMode(ledPin_0_b, OUTPUT);      
  pinMode(ledPin_0_c, OUTPUT);      
  pinMode(ledPin_0_d, OUTPUT);    
  
  pinMode(ledPin_1_a, OUTPUT);      
  pinMode(ledPin_1_b, OUTPUT);      
  pinMode(ledPin_1_c, OUTPUT);      
  pinMode(ledPin_1_d, OUTPUT);      
  
  pinMode(ledPin_a_1, OUTPUT);      
  pinMode(ledPin_a_2, OUTPUT);      
  pinMode(ledPin_a_3, OUTPUT);      
}

////////////////////////////////////////////////////////////////////////
//
// DisplayNumberSet
// Use: Passing anod number, and number for bulb 1 and bulb 2, this function
//      looks up the truth table and opens the correct outs from the arduino
//      to light the numbers given to this funciton (num1,num2).
//      On a 6 nixie bulb setup.
//
////////////////////////////////////////////////////////////////////////
void DisplayNumberSet( int anod, int num1, int num2 )
{
  int anodPin;
  int a,b,c,d;
  
  // set defaults.
  a=1;b=1;c=0;d=1; // will display a zero.
  anodPin =  ledPin_a_1;     // default on first anod.
  
  // Select what anod to fire.
  switch( anod )
  {
    case 0:    anodPin =  ledPin_a_1;    break;
    case 1:    anodPin =  ledPin_a_2;    break;
    case 2:    anodPin =  ledPin_a_3;    break;
  }  
  
  // Load the a,b,c,d.. to send to the SN74141 IC (1)
  switch( num1 )
  {
    case 0: a=0;b=0;c=0;d=0;break;
    case 1: a=1;b=0;c=0;d=0;break;
    case 2: a=0;b=1;c=0;d=0;break;
    case 3: a=1;b=1;c=0;d=0;break;
    case 4: a=0;b=0;c=1;d=0;break;
    case 5: a=1;b=0;c=1;d=0;break;
    case 6: a=0;b=1;c=1;d=0;break;
    case 7: a=1;b=1;c=1;d=0;break;
    case 8: a=0;b=0;c=0;d=1;break;
    case 9: a=1;b=0;c=0;d=1;break;
  }  
  
  // Write to output pins.
  digitalWrite(ledPin_0_d, d);
  digitalWrite(ledPin_0_c, c);
  digitalWrite(ledPin_0_b, b);
  digitalWrite(ledPin_0_a, a);

  // Load the a,b,c,d.. to send to the SN74141 IC (2)
  switch( num2 )
  {
    case 0: a=0;b=0;c=0;d=0;break;
    case 1: a=1;b=0;c=0;d=0;break;
    case 2: a=0;b=1;c=0;d=0;break;
    case 3: a=1;b=1;c=0;d=0;break;
    case 4: a=0;b=0;c=1;d=0;break;
    case 5: a=1;b=0;c=1;d=0;break;
    case 6: a=0;b=1;c=1;d=0;break;
    case 7: a=1;b=1;c=1;d=0;break;
    case 8: a=0;b=0;c=0;d=1;break;
    case 9: a=1;b=0;c=0;d=1;break;
  }
  
  // Write to output pins
  digitalWrite(ledPin_1_d, d);
  digitalWrite(ledPin_1_c, c);
  digitalWrite(ledPin_1_b, b);
  digitalWrite(ledPin_1_a, a);

  // Turn on this anod.
  digitalWrite(anodPin, HIGH);   

  // Delay
  // NOTE: With the differnce in Nixie bulbs you may have to change
  //       this delay to set the update speed of the bulbs. If you 
  //       dont wait long enough the bulb will be dim or not light at all
  //       you want to set this delay just right so that you have 
  //       nice bright output yet quick enough so that you can multiplex with
  //       more bulbs.
  
  // NOTE: delay here is set high because we are using it as a delay for the count up..
  delay(200);
  
  // Shut off this anod.
  digitalWrite(anodPin, LOW);
}

////////////////////////////////////////////////////////////////////////
//
//
////////////////////////////////////////////////////////////////////////
void loop()     
{
  // NOTE: We are just using anod 1 and bulb on each chip output
  //       you should see both bulbs count up to 9 and then reset to 0.
  
  int anod = 0;
  int chip1 = 0;
  int chip2 = 0;
  
  while( chip1!=10 )
  {
    // Fire up a number on each chip and trigger "anod" pin
    // this funtion has a large delay so you have time to see
    // the number display.. then we bump up the count and loop back
    // You should see each bulb count up from 0 to 9 and then back to zero.
    DisplayNumberSet( anod, chip1, chip2 );
    DisplayNumberSet( anod + 1, chip1, chip2 );
    DisplayNumberSet( anod + 2, chip1, chip2 );
    chip1++;
    chip2++;    
   }  
}
