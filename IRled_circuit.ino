void setup() {
  // put your setup code here, to run once:
  pinMode(2, OUTPUT); //set LED pin to output
  ledcAttach(2, 38000, 8); //set up PWM with correct properties
  ledcWrite(2, 128); //start the PWM on led, making the IR beam at the right frequency
}

void loop() {
  // put your main code here, to run repeatedly:

}
