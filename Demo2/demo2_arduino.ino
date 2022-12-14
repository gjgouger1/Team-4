//in this file we handle the arduino communcation with the motors for our robot. You can see the well commented code for our controllers that allows for us to complete this demo.

#include "Arduino.h"
#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>
#include <Wire.h>
#define SLAVE_ADDRESS 0X04

//Define constants for numerial and pin input/output operations
#define pi 3.1415
#define D2 4
#define M1DIR 7
#define M2DIR 8
#define M1PWM 9
#define M2PWM 10
#define SF 12
#define period 8

//Variables for MATLAB serial communication
String InputString = ""; // a string to hold incoming data
bool StringComplete = false;


//Variables for forward velocity PI controller 
float setRhoPrime = 0;
float finalRhoPrime = 0;
float kpRhoPrime = 5;  //I changed this.. could be tuned. Literally don't touch these two
float kiRhoPrime = 1; //tune
float rhoIntegrator = 0;
float eRhoPrime;

//Variables for angular velocity PI controller
float setPhiPrime;
float finalPhiPrime;
float kpPhiPrime = 10;

float kiPhiPrime = 0;
float phiIntegrator = 0;
float ePhiPrime;
float delta = 0;

//Variables to define theta and thetadot on motor 1
float theta1 = 0;
float thetaold1 = 0;
float thetadot1 = 0;

//Variables to define theta and thetadot on motor 2
float theta2 = 0;
float thetaold2 = 0;
float thetadot2 = 0;

//Variables to define the voltages applied to the two motor system
float va;
float deltaVa;

//Variables to define characteristsics of the motors width and wheel radius
float radius = 0.075;
float distance = 0.355;

//Variables to define the individual voltages applied to motors 1 and 2
float v1;
float v2;

//Variables to define the individual pulse-width modulations applied to motors 1 and 2
float PWM1;
float PWM2;

//Variables to define the true forward and angular velocities calculated from the encoder readings
float rhoPrime;
float phiPrime;

//Variables for use in conditional statements of either gradual velocity incrementation of time readings
int i = 1;
int j = 1;

//Variables for use to store time values in thetadot calculations
unsigned long t = 0;
unsigned long told = 0;

//Variables for use to store time values in P(I)(D) controllers
double Ts = 0;
double Tc = millis();

//Variable for Demo2 that faciliatates data transfer between Pi and Arduino 
/*float data = 0;*/

//Variables for angular position PID controller
float setPhi = 0.6;
float phi = 0;
float ePhi = 0;
//2.25
float phiKp = 6; //tune
//0.001
float phiKi = 1; //tune
float phiKd = 0.01; //tune
float positionIntegrator = 0;
float phiDerivative = 0;
float ePhiPast = 0;

//Variables to define parameters for straight motion
float setForwardDistance = 0;
float rho = 0;
float distanceMultiplier = 0;
signed char data = 0;
float last_data = 0;
int counter = 0;
bool marker_was_found = false;
bool moving_forward = false;
bool set_distance_once = false;
bool check_once = false;
int my_arr[] = {0,0,0,0,0};
bool stop_hardcoding = false;
bool garbage_value = false;
int next_state = 0;

//Defines encoders 1 and 2 and their respective pin connections
Encoder ENC1(3, 5);
Encoder ENC2(2, 6); 

signed char data_test[32] = {0};
int i_test = 0;
float dist = 0;
float angle_desired = 0;
bool started_receiving_data = false;
bool once = false;
bool aruco_off_frame = false;
void setup() {

  //Sets baud rate and prints serial montitor statement
  Serial.begin(115200);
  InputString.reserve(200);
  Serial.println("Ready!");

  //Initites pin channels as 'HIGH' or as 'inputs/outputs'
  digitalWrite(D2, HIGH);
  pinMode(M1DIR, OUTPUT);
  pinMode(M1DIR, OUTPUT);          
  pinMode(M1PWM, OUTPUT);
  pinMode(M2PWM, OUTPUT);
  pinMode(SF, INPUT);
  pinMode(D2, OUTPUT);

  //Defines forward/backward directions for the motors 
  digitalWrite(M1DIR, 1); // 1 = forward, 0 = backward
  digitalWrite(M2DIR, 0); // 0 = forward, 1 = backward

  //Demo2 data transfer code
  Wire.begin(SLAVE_ADDRESS);
  Wire.onReceive(receiveData);
}

void loop() {


  switch(next_state){
    case 0:
      if (abs(ePhi) < 0.02){
          ENC1.write(0); //reset encoders
          ENC2.write(0);
//        ph/iKp = 12;
          setPhi =  0.600; //rotate our setPhi again
          setForwardDistance = 0;
          setRhoPrime = 0;
          finalRhoPrime = 0;
      }
      if (started_receiving_data == true){
        next_state = 1;
      }
      break;
  
    case 1:
      phiKp = 10;
      phiKi = 1.3;
      setPhi = angle_desired;

//      if (garbage_value == false){
//        next_state = 2;
//      }
      break;
    case 2:
      //some way to move to case 3
      if ((setPhi > 0.01 || setPhi < -0.01) && set_distance_once == false){
        next_state = 3;
      }
      break;
    case 3:
       ENC1.write(0); //resets encoders
       ENC2.write(0);
//       next_st/ate = 0;
       break;
    
  }

//  if (started_receiving_data == true){ //if we have data other than 0's
//    phiKp = 1.5;
//    phiKi = 0.0001;
////    if (ePhi > 0.01 || ePhi < -0.01){
////      setPhi = angle_desired;  //lets set our phi
////    }
////    i/f (once == false){
//      if (garbage_value == false){
//        Serial.println("setPhi");
//          setPhi = angle_desired;  //lets set our phi
//
//      }
////    }/
//
//    if ((setPhi > 0.01 || setPhi < -0.01) && set_distance_once == false){ //resets encoders if angle >0 and we have not moved forward
//      //could maybe delete set_distance_once?? not sure though. 
////      if (once == false){
//       ENC1.write(0); //resets encoders
//       ENC2.write(0);
////       
//////       phi = 0;
////       once = true; //this should be useless so can prob delete
//      }
//    
//      
//  } //end of started_receiving data if
  
  //MATLAB communication code
  if (StringComplete) {
       StringComplete = false;
  }

  //Reads time to check for duration since previous run
  told = t;
  while(millis()<told + period);  
  t = millis();

  //Calculation of thetadot for motor 1 by calculations for the encoder readings
  thetaold1 = theta1;
  theta1 = (float)ENC1.read() * -2 * pi / 3200;
  
  float deltat1 = t-told;           
  float deltatheta1 = theta1 - thetaold1;

  deltat1 = deltat1/1000;         
  thetadot1 = deltatheta1/deltat1;

  //Calculation of thetadot for motor 2 by calculations for the encoder readings
  thetaold2 = theta2;
  theta2 = (float)ENC2.read() * 2 * pi / 3200; 
  
  float deltat2 = t-told;           
  float deltatheta2 = theta2 - thetaold2;

  deltat2 = deltat2/1000;         
  thetadot2 = deltatheta2/deltat2;

  //Calculation of current Rho (forward motion) using the change in the wheel's circuimference
  rho = rho + radius*((deltatheta1+deltatheta2)/2);

  //Angular position PID controller that uses proportional, integral, and derivative error to reach a set phi (angle in radians) value.

  
  phi = radius*(theta1-theta2)/distance;

  
  ePhi = setPhi - phi; //phi error

  
  if (ePhi < 0.03 && ePhi > -0.03){ //if we are close to the marker, I think this could also be changed to 'setPhi' instead of ePhi's, worth a shot
//    Serial.println("in the small ephi if");
    
    if (started_receiving_data == true && set_distance_once == false){
//        ENC1.write(0);
//        ENC2.write(0); // maybe reset encoders? probably dont do this

//        setPhi = angle_desired; //set phi (again? might not be needed)
//        Serial.println(setPhi);


//        if (setPhi < 0.03 && setPhi > -0.03){ //as long as our set phi is rather small lets move forward, COULD BE TUNED/CHANGED to 0.05, 0.01, etc.
          
          
          //if we have not set the distance yet, and we have a dist value greater than 2, lets ride!
//          Serial.println(dist);
          if (set_distance_once == false && dist > 2 ){
            
//            setPhi = 0;
//            Serial.println("made it to set dist");
            setForwardDistance = 15; //setting dist
//            Serial.println(setForwardDistance);
            setRhoPrime = 1;
            finalRhoPrime = 1;
            stop_hardcoding = true;
            garbage_value = true;
//            Serial.prin/tln("setting garbage");
          }
          
          
//        } //end of set phi if
       
    } //end of started receive data and set distance if


     
//        Serial.println(setForwardDistance);
//        Serial.println(setRhoPrime);

 


    //THIS AINT BROKE SO PROB NO TOUCHIE.. except for setPHi value??? maybe make it bigger/smaller??
//    if (started_receiving_data == false && set_distance_once == false){ //if we havent started receiving data
//
//      if (marker_was_found == false){ //if a marker is not found yet
//        ENC1.write(0); //reset encoders
//        ENC2.write(0);
////        ph/iKp = 12;
//        setPhi =  0.600; //rotate our setPhi again
//        setForwardDistance = 0;
//        setRhoPrime = 0;
//        finalRhoPrime = 0;
//      }
//
//      
//    }




  } //This is the end of ephi if



if (dist < 2 && set_distance_once == false){ //if our distance is less than 2, let's stop and correct our angle to 0
//    Serial.println("hello");
    setForwardDistance = 0;
    setRhoPrime = 0;
    finalRhoPrime = 0;
    
  }
//  setPhi < 0.01 && setPhi > -0.01 && 

 if (set_distance_once == false && dist < 2 && dist != 0){ //if we have corrected to zero, then move forward the hardcoded distance
  
     set_distance_once = true; //no touchie
  
      
      setForwardDistance = 0.38; //no touchie
      setRhoPrime = 1; //I chose small values so it doesn't zoom (hopefully more accurate??) // possibly increase speed
      finalRhoPrime = 1;
      rho = 0;
      Serial.print(rho);
      Serial.println('\t');
      Serial.print("Hardcoded distance set");
      Serial.println('\t');
      
    }


 //begin all of the controller stuff
  
  positionIntegrator = positionIntegrator + ePhi*Ts/1000;

//  Serial.print(positionIntegrator);
//  Serial.print('\t');
  
  if (Ts > 0) {
    phiDerivative = (ePhi - ePhiPast)/Ts;
    ePhiPast = ePhi;
  }
  else {
    phiDerivative = 0;
  }

//Serial.print(phiDerivative);
//  Serial.print('\t');
  
 
  //Output of the PID controller is fed into the input variable for angular velocity controller
  setPhiPrime = phiKp*ePhi + phiKi*positionIntegrator + phiKd*phiDerivative;
  finalPhiPrime = setPhiPrime;

  if(abs(setPhiPrime) > 1) {
    setPhiPrime = 1*sgn(setPhiPrime);
    finalPhiPrime = 1*sgn(setPhiPrime);
    positionIntegrator = 0;
    
    
  }

  //Calculates the actual forward and angular velocitis
  rhoPrime = radius*(thetadot1 + thetadot2)/2;
  phiPrime = radius*(thetadot1 - thetadot2)/distance;

  //Conditional for stationary rotation with straight motion afterward 
  
//    if (ePhi <= 0) { 
//    setRhoPrime = 0.157;
//    finalRhoPrime = 0.157;
//  }

  //Condtional to cause incremental (by tenths) forward velocity increases (slow the step function)
  if(i <= 10) {
    setRhoPrime = setRhoPrime*0.1*i;
    i++;
  }

  else {
    setRhoPrime = finalRhoPrime;
  }

  //Condtional to cause incremental (by tenths) angular velocity increases (slow the step function)
  if(j <= 4) {
    setPhiPrime = setPhiPrime*0.25*j;
    j++;
  }

  else {
    setPhiPrime = finalPhiPrime;
  }

  
  //Conditional for travelling along an arc with straight motion afterward or just straight motion (if setPhi is set equal to zero)
//  if ((ePhi <= 0) && (setRhoPrime != 0)) {
//
//      //Converts meters to feet
////      setForwardDistance = 0;
//
//      //setForwardDistance = 1*0.305*2;
//
//      //Multipliers used are for tuning motion based on experiments with varying set meter distances
//      if (setForwardDistance <= 1) {
//
//        distanceMultiplier = 0.96;
//        
//      }
//
//      if ((setForwardDistance > 1) && (setForwardDistance <= 2)) {
//
//        distanceMultiplier = 0.98;
//      }
//
//      if ((setForwardDistance > 2)&& (setForwardDistance <= 3)) {
//
//        distanceMultiplier = 0.98;
//      }
//      
//  }

  distanceMultiplier = 0.98;

  //Prevents overshoot on the angle turn
  if(ePhi <= 0.005) {
    positionIntegrator = 0;
    phiDerivative = 0;
  }

//  Serial.println(setPhiPrime);
  //Start of PI controllers for both angular and forward velocity 
  eRhoPrime = setRhoPrime - rhoPrime; //Rhoprime error
  ePhiPrime = setPhiPrime - phiPrime; //Phiprime error


  rhoIntegrator = rhoIntegrator + Ts*eRhoPrime/1000;
  phiIntegrator = phiIntegrator + Ts*ePhiPrime/1000;

  
  //Halts straight motion if rover attains the desired traveled distance 
  if(rho >= distanceMultiplier*setForwardDistance) {
    //setForwardDistance = 0;
    eRhoPrime = 0;
    rhoIntegrator = 0;
   
  }

  //Defines the input voltages for the two motor system
  va = kpRhoPrime*eRhoPrime + kiRhoPrime*rhoIntegrator;
  deltaVa = kpPhiPrime*ePhiPrime + kiPhiPrime*phiIntegrator;

  //Defines the motor 1 and 2 voltages
  v1 = (va + deltaVa)/2;
  v2 = (va - deltaVa)/2;

  
//  if (started_receiving_data == true && set_distance_once == false && stop_hardcoding == false){
//    Serial.println("HARDCODING VOLT");
//      if ((setPhi) < 0.02 && (setPhi) > -0.02){
//        if (setPhi < 0){
//          v1 = 0;
//          v2 = .5;
//        }
//        if (setPhi > 0){
//          v1 = .5;
//          v2 = 0;
//        }
//      }
//    }
  

Serial.print(setRhoPrime);
Serial.println('\t');

  //Antiwindup and saturation for motor 1
  if (abs(va + deltaVa) > 7.6) {
      v1 = sgn(va+deltaVa)*7.6;
      rhoIntegrator = 0;
      phiIntegrator = 0;
  }

  //Antiwindup and saturation for motor 2
  if (abs(va - deltaVa) > 7.6) {
      v2 = sgn(va-deltaVa)*7.6;
      rhoIntegrator = 0;
      phiIntegrator = 0; 
  }


  //converts output voltages to pwm values
  PWM1 = (abs(v1)/7.6)*255;
  PWM2 = (abs(v2)/7.6)*255;

//  Serial.println(PWM1);
//  Serial.println(PWM2);

  //The four if statements ensure the motors spin the correct direction based off the output voltages
  if (v1 < 0) {
      digitalWrite(M1DIR,0);
  }

  if (v2 < 0) {
      digitalWrite(M2DIR,1);
  }

  if (v1 >= 0) {
      digitalWrite(M1DIR,1);
  }

  
  if (v2 >= 0) {
      digitalWrite(M2DIR,0);
  }

  //Writing the pwm values to the motors
  
  analogWrite(M1PWM, PWM1);
  analogWrite(M2PWM, PWM2);

//  Serial.println(PWM1);
//  Serial.println(PWM2);

  //Read in time values
  Ts = millis() - Tc;
  Tc = millis();
  
  //Serial.print(setRhoPrime);
  //Serial.print("\t");
  //Serial.println("");

}

//Facilitates the commuincation between Arduino and MATLAB
void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    InputString += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      StringComplete = true;
    }
  }
}

//Function to check the sign of a value
int sgn(float uNumber) {
  int sign;
  if (uNumber >= 0) {
    sign = 1;
    return sign;
  }

  else {
    sign = -1;
    return sign;
  } 
}

void receiveData(int byteCount){
    i_test = 0;
   
    while(Wire.available()) {
      
      data_test[i_test % 2] = Wire.read();
      
      my_arr[i_test % 5] = data_test[i_test % 2]; //this populates the array
      
      
      if (dist != 0 && angle_desired != 0){ //if we receive something other than 0
        started_receiving_data = true;
        marker_was_found = true;

      }

        
      
      if (my_arr[0] == 0 && my_arr[1] == 0 && my_arr[2] == 0 && my_arr[3] == 0 && my_arr[4] == 0){ //I don't think this does jack, but it aint broke so idk
          started_receiving_data = false; //this statement is needed but the conditional is wack, I was tryna do the last 5 data points being zero means we are probably just not detecting anything
//          Serial.println("my_arr thing worked");
      }
      

      
  
        if ((i_test % 2) == 0){

          angle_desired = (-1 * (float)data_test[i_test % 2] * 0.003682)+phi; //big math boi

        }
      if ((i_test % 2) == 1){
        dist = data_test[i_test % 2] * 0.1;
      }
      i_test++;
      
  } //end of while loop
  
//      Serial.println(("ANGLE"));
//      Serial.println(angle_desired);
//      Serial.println(("DIST"));
//      Serial.println(dist);
}
