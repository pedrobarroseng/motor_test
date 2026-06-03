#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <Servo.h>

// Variaveis para controlar o motor
Servo motor;
int potenciometro = A0;
int pinoEsc = 8;
int buttonESC = 22;

// Variaveis para a camada de segurança
unsigned long leituraInicial = 0;
bool sistemaTravado = false;
bool motorArmado = false;

// Configuração: CLK, DIN, DC, CE (CR), RST
Adafruit_PCD8544 display = Adafruit_PCD8544(13, 12, 11, 10, 9);

// Menu para a escolha da tela
bool menuConfirmado = false;

// 0 -> unidirecional
// 1 -> bidirecional
int escType = 0;

bool sinalLiberado = false;

// tempo para o calculo da tela
unsigned long tempoInicio = 0; 

// variáveis para a lógica confirmar o ESC
unsigned long tempoSegurando = 0;
bool botaoAnterior = false;

int botaoResetar = 23;

void setup() {
  Serial.begin(9600);

  // Parte do controle dos motores com potenciometro
  pinMode(potenciometro, INPUT);

  // Essa entrada vai decidir o tipo de ESC (unidirecional e bidirecional)
  pinMode(buttonESC, INPUT_PULLUP);

  // Parte que recebe a informação de irá resetar
  pinMode(botaoResetar, INPUT_PULLUP);

  // Parte da tela inicialização da tela
  display.begin();
  display.setContrast(55);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(10, 20);
  display.println("MOTOR TEST");
  display.display();
  delay(2000);

  // Alinha o tempo de início logo antes de abrir o menu de fato
  tempoInicio = millis();

  while (menuConfirmado == false) {
    bool estadoBotao = (digitalRead(buttonESC) == LOW); // Verifica se apertou

    // 1. Momento exato em que o botão foi pressionado
    if (estadoBotao && !botaoAnterior) {
      tempoSegurando = millis(); // Marca quando começou a segurar
    }

    // 2. Momento exato em que o botão foi solto (Clique Curto)
    if (!estadoBotao && botaoAnterior) {
      if (millis() - tempoSegurando < 800) { // Se soltou rápido, alterna o modo
        escType = !escType;
      }
      tempoSegurando = 0; // Reseta para sumir a barra
    }

    // 3. Mantido pressionado por 1 segundo (Confirmação)
    if (estadoBotao && tempoSegurando > 0 && (millis() - tempoSegurando >= 1000)) {
      menuConfirmado = true;
    }

    botaoAnterior = estadoBotao;

    // Atualiza a tela do Menu
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Escolha o ESC:");

    display.setCursor(0, 15);
    display.print(escType == 0 ? "> Uni-dir" : " Uni-dir");
    display.setCursor(0, 30);
    display.print(escType == 1 ? "> Bi-dir" : " Bi-dir");

    // Barra de progresso da confirmação gráfica
    if (estadoBotao && tempoSegurando > 0) {
      int progresso = map(millis() - tempoSegurando, 0, 1000, 0, 84);
      progresso = constrain(progresso, 0, 84);
      
      display.drawRect(0, 40, 84, 8, BLACK);   // Caixinha da borda
      display.fillRect(0, 40, progresso, 8, BLACK); // Preenchimento da barra
    } else {
      display.setCursor(0, 40);
      display.print("segure p/ conf.");
    }
    display.display();
    delay(20); // Delay curto para debouncing
  }
  
  motor.attach(pinoEsc);
  if (escType == 0) {
    motor.writeMicroseconds(1000);
  } else {
    motor.writeMicroseconds(1500);
  }

  leituraInicial = analogRead(potenciometro);
  delay(2000);
}

void loop() {
  int leitura_potenciometro = analogRead(potenciometro);
  int sinalFinal;
  int porcentagem;

  // Leitura para a verificação de desconexão
  unsigned long leituraAtual = analogRead(potenciometro);

  //verificação se o botão que reinicia foi apertado
  if (digitalRead(botaoResetar) == LOW)
  {
    //Desliga o motor imediatamente 
    motor.writeMicroseconds(escType == 0 ? 1000:1500);

    //Resetar as variáveis do sistema
    menuConfirmado = false;
    motorArmado = false;
    sinalLiberado = false;
    tempoSegurando = 0;

    //Forçar a volta para o setup
    setup();
    //Saí do loop atual para reiniciar o fluxo correto
    return;

  }

  if (abs((long)(leituraAtual - leituraInicial)) >= 512) {
    sistemaTravado = true;
  }

  if (sistemaTravado == true) {
    motor.writeMicroseconds(1000);
    display.clearDisplay();
    display.setCursor(0, 20);
    display.print("connection error");
    display.display();
  } 
  else 
  {
    if (leitura_potenciometro < 50) {
      motorArmado = true;
    }
    
    if (motorArmado == true) {
      
      // NOTA DE SEGURANÇA: A verificação do botão que alterava o modo em tempo 
      // de execução foi removida daqui para evitar acidentes com inversão de sinal.

      // Cálculos e Mapeamento de sinais fixos com base no setup()
      if (escType == 0) {
        sinalFinal = map(leitura_potenciometro, 0, 1023, 1000, 1900);
        porcentagem = map(leitura_potenciometro, 0, 1023, 0, 100);
        sinalLiberado = true; 
      } 
      else {
        sinalFinal = map(leitura_potenciometro, 0, 1023, 1000, 2000);
        porcentagem = map(leitura_potenciometro, 0, 1023, -100, 100);
        
        // Verifica se passou pela zona neutra (trava mecânica inicial do bi-dir)
        if (abs(leitura_potenciometro - 512) < 30) {
          sinalLiberado = true;
        }
      }

      // Prepara e limpa o display para a renderização controlada
      display.clearDisplay();
      display.setContrast(55);
      display.setTextSize(1);
      display.setTextColor(BLACK);

      // Flag que dita se estamos no estado de bloqueio de centro
      bool esperandoNeutro = (escType == 1 && !sinalLiberado);

      if (esperandoNeutro) {
        // --- TELA A: Apenas aviso de ajuste mecânico ---
        sinalFinal = 1500; // Protege o motor forçando neutro
        
        display.setCursor(0, 0);
        display.println("Bi-dir:Coloque");
        display.print("no zero %");
        display.setCursor(0, 20);
        display.print("Pot: "); display.print(porcentagem); display.println("%");
        display.print("--------------");
        display.setCursor(0, 35);
        display.print("PWM Alvo: "); display.print(sinalFinal);
      } 
      else {
        // --- TELA B: Telemetria regular (Modo Livre) ---
        display.setCursor(0, 0);
        display.print(escType == 0 ? "ESC Uni-dir" : "ESC bi-dir");
        
        display.setCursor(0, 10);
        display.print("Pot: "); display.print(porcentagem); display.println("%");
        display.print("--------------");
        
        display.setCursor(0, 25);
        display.print("PWM: "); display.print(sinalFinal); display.print("ms");
        display.print("--------------");
        
        display.setCursor(0, 40);
        display.print("ADC: "); display.print(leitura_potenciometro);
      }
      
      // Atualização física única da tela e do motor
      display.display();
      motor.writeMicroseconds(sinalFinal);

      Serial.print(sinalFinal); Serial.print("|"); Serial.println(leitura_potenciometro);
    }
    else {
      // Motor não armado -> Pede calibração inicial no zero físico
      motor.writeMicroseconds(escType == 0 ? 1000 : 1500);

      display.clearDisplay();
      display.setCursor(0, 10);
      display.print("Coloque no 0");
      display.setCursor(0, 20);
      display.print("do pot para");
      display.setCursor(0, 30);
      display.println("armar. (gira");
      display.setCursor(0, 40);
      display.println("p/esquerda)");
      display.display();
    }
  }
  
  leituraInicial = leituraAtual;
  delay(10);
}