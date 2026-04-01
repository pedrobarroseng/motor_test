#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <Servo.h>

//Variaveis para controlar o motor
Servo motor;
int potenciometro = A0;
int pinoEsc = 8;

// Configuração: CLK, DIN, DC, CE (CR), RST
Adafruit_PCD8544 display = Adafruit_PCD8544(13, 12, 11, 10, 9);

void setup() {

  Serial.begin(9600);

  //Parte da tela inicialização da tela
  display.begin();
  // Ajuste de contraste
  display.setContrast(55);
  display.clearDisplay(); 
  display.setTextSize(2); 
  display.setTextColor(BLACK);
  display.setCursor(0,24);

  display.println("Motor test");

  display.display(); 

  //Parte do controle dos motores com potenciometro
  pinMode(potenciometro, INPUT);

  motor.attach(pinoEsc);
  motor.writeMicroseconds(1000);

  delay(2000);
}

void loop() {

  int leitura_potenciometro;
  int sinalFinal;
  int porcentagem;

  leitura_potenciometro = analogRead(potenciometro);

  sinalFinal = map(leitura_potenciometro, 0, 1023, 1000, 2000);
  porcentagem = map(leitura_potenciometro, 0, 1023, 0, 100);

  Serial.println(sinalFinal);

  motor.writeMicroseconds(sinalFinal);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  //Parte que plota os sinais na tela
  
  display.print("Potencia: ");
  display.print(porcentagem);
  display.println("%");
  
  display.print("PWM (ms): ");
  display.println(sinalFinal);
  
  display.print("Analogico:");
  display.print(leitura_potenciometro);

  display.display();
}