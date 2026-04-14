#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <Servo.h>

Servo motor;
int potenciometro = A0;
int pinoEsc = 8;

unsigned long leituraInicial = 0;
bool sistemaTravado = false;
bool motorArmado = false;

// Configuração LCD: CLK, DIN, DC, CE, RST
Adafruit_PCD8544 display = Adafruit_PCD8544(13, 12, 11, 10, 9);

void setup() {
  Serial.begin(9600);
  
  display.begin();
  display.setContrast(50); // Baixei um pouco para estabilizar o consumo
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);
  
  display.setCursor(10,20);
  display.println("MODO TESTE");
  display.display();

  pinMode(potenciometro, INPUT);
  
  motor.attach(pinoEsc);
  motor.writeMicroseconds(1000); // Sinal de segurança inicial

  leituraInicial = analogRead(potenciometro);
  delay(2000);
}

void loop() {
  int leitura_potenciometro = analogRead(potenciometro);
  int sinalFinal;
  int porcentagem;

  // --- TRAVA 1: SEGURANÇA DE CONEXÃO (FAILSAFE) ---
  unsigned long leituraAtual = analogRead(potenciometro);
  if (abs(leituraAtual - leituraInicial) >= 1000) {
    sistemaTravado = true;
  }

  if (sistemaTravado) {
    motor.writeMicroseconds(1000);
    display.clearDisplay();
    display.setCursor(0, 20);
    display.print("ERRO CONEXAO");
    display.display();
  } 
  else {
    // --- TRAVA 2: ARMAÇÃO (ARMING) ---
    if (!motorArmado && leitura_potenciometro < 80) {
      motorArmado = true;
    }

    if (motorArmado) {
      // Mapeamento fixo para teste (Unidirecional)
      sinalFinal = map(leitura_potenciometro, 0, 1023, 1000, 1900);
      porcentagem = map(leitura_potenciometro, 0, 1023, 0, 100);

      
      Serial.print(sinalFinal);
      Serial.print("|");
      Serial.println(leitura_potenciometro);
      
      motor.writeMicroseconds(sinalFinal);

      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("MOTOR ATIVO");
      
      display.setCursor(0, 15);
      display.print("Pot: "); display.print(porcentagem); display.print("%");
      
      display.setCursor(0, 30);
      display.print("PWM: "); display.print(sinalFinal);
      
      display.setCursor(0, 40);
      display.print("ADC: "); display.print(leitura_potenciometro);
      
      display.display();
    } 
    else {
      // Enquanto não arma
      motor.writeMicroseconds(1000);
      display.clearDisplay();
      display.setCursor(0, 15);
      display.print("COLOQUE NO 0");
      display.setCursor(0, 25);
      display.print("PARA ARMAR");
      display.display();
    }
    leituraInicial = leituraAtual;

  }
  delay(50); // Pequeno delay para ajudar na estabilidade do LCD
}