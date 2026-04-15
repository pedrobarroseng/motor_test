#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <Servo.h>

//Variaveis para controlar o motor
Servo motor;
int potenciometro = A0;
int pinoEsc = 8;
int buttonESC = 22;

//Variaveis para a camada de segurança
int long leituraInicial = 0;
bool sistemaTravado = false;
bool motorArmado = false;

// Configuração: CLK, DIN, DC, CE (CR), RST
Adafruit_PCD8544 display = Adafruit_PCD8544(13, 12, 11, 10, 9);

//Tempo para medir realizar o calculo em millis
unsigned long ultimoClique = 0;

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

  //essa entrada vai decidir o tipo de ESC (unidirecional e bidirecional)
  pinMode(buttonESC, INPUT_PULLUP);

  //Envio do sinal para o motor
  motor.attach(pinoEsc);
  motor.writeMicroseconds(1000);

  //Variavel que faz a leitura incial do potenciometro para os calculos da camada de segurança
  leituraInicial = analogRead(potenciometro);
  delay(2000);

}
// 0 -> unidirecional
// 1 -> bidirecional
int escType = 0;

void loop(){

  int leitura_potenciometro;
  int sinalFinal;
  int porcentagem;
  unsigned tempoAtual = millis();

  leitura_potenciometro = analogRead(potenciometro);

  //leitura do potenciometro para o calculo da camada de segurança
  int long leituraAtual = analogRead(potenciometro);

  if (abs(leituraAtual - leituraInicial) >= 512)
  {
    Serial.print(leituraAtual);
    Serial.print("-");
    Serial.println(leituraInicial);
    Serial.print("=");
    Serial.println(leituraAtual - leituraInicial);
    sistemaTravado = true;
  }

  if (sistemaTravado == true)
  {
    motor.writeMicroseconds(1000);
    display.setContrast(55);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(BLACK);
    display.setCursor(0,20);
    display.print("connection error");
    display.display();
  }

  else
  {
    if (leitura_potenciometro < 50)
    {
      motorArmado = true;
    }
    
    if (motorArmado == true)  
    {
      display.clearDisplay();
      display.setContrast(55);
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(BLACK);

      if (digitalRead(buttonESC) == LOW && tempoAtual - ultimoClique >= 1000)
      {   
        escType = (escType + 1) % 2;
        ultimoClique = tempoAtual;
        Serial.println("Modo Alterado");

        //Mostrando na tela a mudança do ESC
        display.setCursor(10,20);
        display.print("Trocando");
      }
    
      if (escType == 0)
      {
        display.setCursor(0,0);
        display.print("ESC Uni-dir");
        sinalFinal = map(leitura_potenciometro, 0, 1023, 1000, 1900);
        porcentagem = map(leitura_potenciometro, 0, 1023, 0, 100);
      }

      else
        {
        display.setCursor(0,0);
        display.print("ESC bi-dir");
        sinalFinal = map(leitura_potenciometro, 0, 1023, 1000, 2000);
        porcentagem = map(leitura_potenciometro, 0, 1023, -100, 100);
        }
    
      Serial.print(sinalFinal);
      Serial.print("|");
      Serial.println(leitura_potenciometro);
      motor.writeMicroseconds(sinalFinal);

      //Parte que plota os sinais na tela
      display.setCursor(0, 10);
      display.print("Pot: ");
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

    else  
    {
    //Isso vai enviar um sinal para o motor estabilizar no zero caso não esteja
      if(escType == 0)
      {
       motor.writeMicroseconds(1000);
      }
      else
      {
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
  delay(10);
}