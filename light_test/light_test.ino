void setup() {
  Serial.begin(115200);
  analogSetAttenuation(ADC_0db);   // makes the light sensor more sensitive
}

void loop() {
  int light = analogRead(34);
  Serial.println(light);
  delay(300);
}