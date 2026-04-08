#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <Servo.h>

//Variaveis para controlar o motor
Servo motor;
int potenciometro = A0;
int pinoEsc = 8;

//Variaveis para a camada de segurança
unsigned long leituraInicial = 0;
bool sistemaTravado = false;
bool motorArmado = false;

//Variavel para a escolha dos ESC (Unidirecional ou bidirecional)
int ESC = 22;

// Configuração: CLK, DIN, DC, CE (CR), RST
Adafruit_PCD8544 display = Adafruit_PCD8544(13, 12, 11, 10, 9);

void setup() {

  Serial.begin(9600);

  //Parte da tela inicialização da tela
  display.begin();
  // Ajuste de contraste
  display.setContrast(55);
  display.clearDisplay(); 
  display.setTextSize(1); 
  display.setTextColor(BLACK);
  display.setCursor(10,20);

  display.println("MOTOR TEST");

  display.display(); 

  //Parte do controle dos motores com potenciometro
  pinMode(potenciometro, INPUT);

  //Parte que decidimos via chave gangorra
  pinMode(ESC, INPUT_PULLUP);
  
  //Envio do sinal para o motor
  motor.attach(pinoEsc);
  motor.writeMicroseconds(1000);

  //Variavel que faz a leitura incial do potenciometro para os calculos da camada de segurança
  leituraInicial = analogRead(potenciometro);
  delay(2000);
}

void loop() {

  int leitura_potenciometro;
  int sinalFinal;
  int porcentagem;  

  leitura_potenciometro = analogRead(potenciometro);

  //leitura do potenciometro para o calculo da camada de segurança
  unsigned long leituraAtual = analogRead(potenciometro);
  if (abs(leituraAtual - leituraInicial) >= 512){
    sistemaTravado = true;  
  }
  if (sistemaTravado == true){
    motor.writeMicroseconds(1000);

    display.setContrast(55);
    display.clearDisplay(); 
    display.setTextSize(1); 
    display.setTextColor(BLACK);
    display.setCursor(0,20);
    display.print("connection error");
    display.display();
  }

  else{

    if (leitura_potenciometro < 50){
      motorArmado = true;
    }

    if (motorArmado == true){
       
       display.clearDisplay();
       display.setContrast(55);
       display.clearDisplay(); 
       display.setTextSize(1); 
       display.setTextColor(BLACK);
      
      if (digitalRead(ESC) == HIGH){ 
       
        display.setCursor(0,0);
        display.print("ESC Unidirecional");
        sinalFinal = map(leitura_potenciometro, 0, 1023, 1000, 1900);
        porcentagem = map(leitura_potenciometro, 0, 1023, 0, 100);
        }
      
      else{
        
        display.setCursor(0,0);
        display.print("ESC bidirecional");
        sinalFinal = map(leitura_potenciometro, 0, 1023, 1000, 1900);
        porcentagem = map(leitura_potenciometro, 0, 1023, -100, 100);
        }
        
     
      Serial.print(sinalFinal);
      Serial.print("|");
      Serial.println(leitura_potenciometro);

      motor.writeMicroseconds(sinalFinal);

      //Parte que plota os sinais na tela
      display.setCursor(0, 10);
      display.print("Potencia: ");
      display.print(porcentagem);
      display.println("%");
      display.print("--------------");
  
      display.setCursor(0, 25);
      display.print("PWM (ms): ");
      display.println(sinalFinal);
      display.print("--------------");

      display.setCursor(0, 40);
      display.print("ADC:");
      display.print(leitura_potenciometro);

      display.display();    
      
  }
  else{
    //Isso vai enviar um sinal para o motor estabilizar no zero caso não esteja
    if(digitalRead(ESC) == HIGH){
      motor.writeMicroseconds(1000);
    }
    else{
      motor.writeMicroseconds(1500);
    }
    //Aviso para colocar o poteniometro no 0, vulgo o ponto de referencia inicial
    display.setContrast(55);
    display.clearDisplay(); 
    display.setTextSize(1); 
    display.setTextColor(BLACK);
    display.setCursor(0,20);
    display.print("Coloque no 0");
    display.println("para armar");
    display.display();
  }
}
  leituraInicial = leituraAtual;
}
  