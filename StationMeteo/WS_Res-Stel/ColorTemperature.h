/*Temperature by Led
Just a simple measuring temperature app, that shows the results by the color of the LED.
Cool is Blue, Hot is Red.
Can be used with more leds in array for a Led termometer, or any other aplication.
Made by @MrLndr at www.arduinoarts.com




int tempPin = 0;     // the thermistor and 4,7k resistor
int temp;     // the analog reading from the sensor divider
int LEDCool = 10;          // connect Blue LED to pin 10 
int LEDHot = 9;          // connect Red LED to pin 9 
int breakPoint = 575;

void setup(void) {
  Serial.begin(9600);
}

void loop(void) {
  temp = analogRead(tempPin);  

  Serial.print("Temp = ");
  Serial.println(temp);     // reading the values

 if (temp <= breakPoint){ //is cool or hot?
   digitalWrite (LEDCool, HIGH);
   digitalWrite (LEDHot, LOW);
 }
 else{
   digitalWrite (LEDHot, HIGH);
   digitalWrite (LEDCool, LOW);
  delay(10);
}
}
*/
