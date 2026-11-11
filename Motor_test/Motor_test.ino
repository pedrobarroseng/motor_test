/*
 * ================================================================
 *  Motor Test - ESC Controller with Nokia 5110 Display
 *  Hardware: Arduino Mega 2560
 *  Protocolos: PWM, DShot150, DShot300
 * ================================================================
 *  SERIAL OUTPUT (115200 baud) - CSV format:
 *  timestamp_ms, pwm_us, adc_raw, throttle_pct, esc_type, system_state,
 *  protocol, dshot_value, dshot_frame
 *
 *    pwm_us      -> preenchido so no PWM (vazio no DShot)
 *    dshot_value -> preenchido so no DShot (0..2047)
 *    dshot_frame -> quadro DShot de 16 bits em hexa (ex.: 0x7623)
 *
 *  system_state values:
 *    DISARMED        -> Motor not yet armed (waiting for zero throttle)
 *    WAITING_NEUTRAL -> Bidirectional ESC waiting for center position
 *    ARMED           -> Normal operation
 *    LOCKED          -> Potentiometer disconnection detected (safety lock)
 * ================================================================
 *  O QUE MUDOU EM RELACAO A VERSAO SO PWM:
 *    - A biblioteca Servo saiu. O sinal agora e gerado em saida_esc.cpp
 *      (Timer4 do Mega), que sabe fazer PWM e DShot.
 *    - Novo menu de protocolo antes do menu Uni/Bi.
 *    - No DShot ha ferramentas: bipe no ESC e ativar/desativar 3D.
 *    - "Parada" depende do protocolo: PWM uni 1000 us, PWM bi 1500 us,
 *      DShot 0 (uni e bi). A trava LOCKED agora manda essa parada
 *      (antes mandava 1000 us, que no modo bi e re maxima).
 *    - CSV ganhou as colunas protocol, dshot_value e dshot_frame.
 *    As travas de seguranca continuam exatamente iguais.
 * ================================================================
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include "saida_esc.h"          // substitui a biblioteca Servo

// -- Pin definitions --------------------------------------------
const int PIN_POTENCIOMETRO = A0;
const int PIN_ESC           = 8;    // usado dentro de saida_esc.cpp (OC4C)
const int PIN_BUTTON_ESC    = 22;
const int PIN_BUTTON_RESET  = 23;

// -- Display: CLK, DIN, DC, CE, RST -----------------------------
Adafruit_PCD8544 display = Adafruit_PCD8544(13, 12, 11, 10, 9);

// -- ESC configuration ------------------------------------------
// 0 = unidirectional | 1 = bidirectional
int escType   = 0;
int protocolo = PROT_PWM;

// -- Safety layer -----------------------------------------------
int  leituraInicial  = 0;
bool sistemaTravado  = false;
bool motorArmado     = false;
bool sinalLiberado   = false;

// -- Serial header flag ----------------------------------------
bool headerEnviado = false;

// -- Prototypes (functions defined below) ------------------------
void enviarCSV(int sinal, int adcRaw, int pct, const char* state);
void imprimirHex4(Print& p, uint16_t v);

// -- Menu texts ---------------------------------------------------
const char* const OPC_PROTOCOLO[] = { "PWM", "DShot150", "DShot300" };
const char* const OPC_DSHOT[]     = { "Controlar", "Bipe no ESC", "Ativar 3D", "Desativar 3D" };
const char* const OPC_DIRECAO[]   = { "Uni-dir", "Bi-dir" };

// ==============================================================
//  Menu generico (mesma logica do menu original):
//    toque curto  -> proxima opcao
//    segurar 1 s  -> confirma
// ==============================================================
int menuEscolher(const char* titulo, const char* const opcoes[], int n, int inicial) {
  int escolha = inicial;
  bool confirmado = false;
  bool botaoAnterior = false;
  unsigned long tempoSegurando = 0;

  // espera o botao ser solto (evita confirmar o menu seguinte sem querer)
  while (digitalRead(PIN_BUTTON_ESC) == LOW) delay(10);

  while (!confirmado) {
    bool estadoBotao = (digitalRead(PIN_BUTTON_ESC) == LOW);

    if (estadoBotao && !botaoAnterior) {            // acabou de apertar
      tempoSegurando = millis();
    }
    if (!estadoBotao && botaoAnterior) {            // acabou de soltar
      if (millis() - tempoSegurando < 800) {
        escolha = (escolha + 1) % n;                // toque curto -> proxima
      }
      tempoSegurando = 0;
    }
    if (estadoBotao && tempoSegurando > 0 && (millis() - tempoSegurando >= 1000)) {
      confirmado = true;                            // segurou 1 s
    }
    botaoAnterior = estadoBotao;

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(titulo);
    for (int i = 0; i < n; i++) {
      display.setCursor(0, 9 + i * 8);
      display.print(i == escolha ? "> " : "  ");
      display.print(opcoes[i]);
    }
    if (estadoBotao && tempoSegurando > 0) {
      int progresso = map(millis() - tempoSegurando, 0, 1000, 0, 84);
      progresso = constrain(progresso, 0, 84);
      display.drawRect(0, 41, 84, 7, BLACK);
      display.fillRect(0, 41, progresso, 7, BLACK);
    } else {
      display.setCursor(0, 41);
      display.print("segure p/ conf.");
    }
    display.display();
    delay(20);
  }
  return escolha;
}

// Tela simples de 5 linhas (14 caracteres por linha)
void tela(const char* l1, const char* l2, const char* l3,
          const char* l4, const char* l5) {
  const char* l[5] = { l1, l2, l3, l4, l5 };
  display.clearDisplay();
  for (int i = 0; i < 5; i++) {
    display.setCursor(0, i * 10);
    display.print(l[i]);
  }
  display.display();
}

// ==============================================================
//  Ferramentas DShot (nao giram o motor)
//    1 = bipe | 2 = ativar 3D | 3 = desativar 3D
// ==============================================================
void ferramentaDShot(int acao) {
  saidaIniciar(protocolo, false);          // quadros com valor 0 (ESC arma)
  tela("Ligue o ESC", "", "Enviando 0", "(armando ESC)", "");
  delay(3000);

  if (acao == 1) {
    for (int i = 0; i < 3; i++) {
      tela("Bipe no ESC", "", i == 0 ? "1/3" : i == 1 ? "2/3" : "3/3", "", "");
      saidaComandoDShot(1, 1);             // comando 1 = beacon (bipe)
      delay(600);
    }
  } else {
    tela(acao == 2 ? "Ativar 3D" : "Desativar 3D", "", "Enviando...", "", "");
    saidaComandoDShot(acao == 2 ? 10 : 9, 10);   // 10 = 3D on, 9 = 3D off
    delay(50);
    saidaComandoDShot(12, 10);             // 12 = salvar no ESC
    delay(100);
  }

  saidaParar();
  tela("Pronto.", "", "Desligue e", "religue o ESC", "");
  delay(2500);
}

// ==============================================================
//  Valor do sinal a partir do potenciometro
//    PWM   -> microssegundos
//    DShot -> valor 0..2047
// ==============================================================
int valorParada() {
  if (protocoloEhDShot(protocolo)) return 0;
  return escType == 0 ? 1000 : 1500;
}

int calcularSinal(int adc) {
  if (!protocoloEhDShot(protocolo)) {
    // Igual a versao original
    if (escType == 0) return map(adc, 0, 1023, 1000, 1900);   // uni: limite de 90 %
    return map(adc, 0, 1023, 1000, 2000);                     // bi
  }

  if (escType == 0) {
    // DShot uni: 0 = parado, 48..2047 = aceleracao.
    // Mesmo limite de 90 % do PWM: maximo 48 + 1799 = 1847.
    int passo = map(adc, 0, 1023, 0, 1799);
    return passo == 0 ? 0 : 48 + passo;
  }

  // DShot bi (modo 3D no ESC): centro = 0 (parado),
  //   pot abaixo do centro -> 48..1047 (um sentido)
  //   pot acima do centro  -> 1048..2047 (outro sentido)
  // Em cada metade, o menor valor e a menor velocidade.
  int d = adc - 512;
  if (abs(d) < 30) return 0;
  if (d > 0) return 1048 + map(d, 30, 511, 0, 999);
  return 48 + map(-d, 30, 512, 0, 999);
}

void escreverSinal(int sinal) {
  if (protocoloEhDShot(protocolo)) saidaEscreverDShot(sinal);
  else                             saidaEscreverPWM(sinal);
}

// Linha da tela com o valor enviado: "PWM: 1449us" ou "DShot: 945"
void imprimirSinal(int sinal) {
  if (protocoloEhDShot(protocolo)) {
    display.print("DShot: "); display.print(sinal);
  } else {
    display.print("PWM: "); display.print(sinal); display.print("us");
  }
}

// ==============================================================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_POTENCIOMETRO, INPUT);
  pinMode(PIN_BUTTON_ESC,    INPUT_PULLUP);
  pinMode(PIN_BUTTON_RESET,  INPUT_PULLUP);

  saidaParar();                            // pino 8 em LOW durante os menus

  // -- Display init ---------------------------------------------
  display.begin();
  display.setContrast(55);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(10, 15);
  display.println("MOTOR TEST");
  display.setCursor(10, 28);
  display.println("ESC Bench");
  display.display();
  delay(2000);

  // -- Protocol menu (+ DShot tools) --
  while (true) {
    protocolo = menuEscolher("Protocolo:", OPC_PROTOCOLO, PROT_QTD, protocolo);
    if (protocoloEhDShot(protocolo)) {
      int acao = menuEscolher("DShot:", OPC_DSHOT, 4, 0);
      if (acao != 0) {                     // ferramenta -> volta ao menu
        ferramentaDShot(acao);
        continue;
      }
    }
    break;
  }

  // -- ESC Type selection menu (uni / bi) --
  escType = menuEscolher("Escolha o ESC:", OPC_DIRECAO, 2, escType);

  // -- ESC arming signal --
  saidaIniciar(protocolo, escType == 1);   // ja manda a parada do protocolo
  tela(protocoloNome(protocolo), escType == 0 ? "Uni-dir" : "Bi-dir",
       "Sinal ativo.", "Trocou o prot.?", "Religue o ESC");

  // -- Reset safety state --
  leituraInicial = analogRead(PIN_POTENCIOMETRO);
  sistemaTravado = false;
  motorArmado    = false;
  sinalLiberado  = false;
  headerEnviado  = false;

  delay(2000);
}

// =============================================================
void loop() {
  int leituraAtual = analogRead(PIN_POTENCIOMETRO);

  // -- Reset button --
  if (digitalRead(PIN_BUTTON_RESET) == LOW) {
    saidaEscreverParado();
    delay(200); // debounce
    setup();
    return;
  }

  // -- Disconnection detection --
  if (abs(leituraAtual - leituraInicial) >= 512) {
    sistemaTravado = true;
  }

  // == CSV header (sent once per session, after menu) ========
  if (!headerEnviado) {
    Serial.println("timestamp_ms,pwm_us,adc_raw,throttle_pct,esc_type,system_state,protocol,dshot_value,dshot_frame");
    headerEnviado = true;
  }

  // == LOCKED state ==========================================
  if (sistemaTravado) {
    saidaEscreverParado();

    display.clearDisplay();
    display.setCursor(0, 10);
    display.println("!! ERRO !!");
    display.setCursor(0, 22);
    display.println("Pot desconectado");
    display.setCursor(0, 34);
    display.println("Pressione reset");
    display.display();

    // Report locked state
    enviarCSV(valorParada(), leituraAtual, 0, "LOCKED");

  // == NORMAL state ==========================================
  } else {

    // -- Arming condition: throttle must be at zero first --
    if (leituraAtual < 50) {
      motorArmado = true;
    }

    if (motorArmado) {
      int sinalFinal;
      int porcentagem;

      // -- Pot percentage and neutral release per ESC type --
      if (escType == 0) {
        porcentagem = map(leituraAtual, 0, 1023, 0, 100);
        sinalLiberado = true;
      } else {
        porcentagem = map(leituraAtual, 0, 1023, -100, 100);
        if (abs(leituraAtual - 512) < 30) {
          sinalLiberado = true;
        }
      }

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(BLACK);

      // -- WAITING NEUTRAL (bidirectional only) --
      if (escType == 1 && !sinalLiberado) {
        sinalFinal = valorParada();

        display.setCursor(0, 0);
        display.println("Bi-dir: va ao");
        display.println("centro (0%)");
        display.setCursor(0, 20);
        display.print("Pot: "); display.print(porcentagem); display.println("%");
        display.println("--------------");
        display.setCursor(0, 35);
        imprimirSinal(sinalFinal);
        display.display();

        saidaEscreverParado();
        enviarCSV(sinalFinal, leituraAtual, porcentagem, "WAITING_NEUTRAL");

      // -- ARMED: normal telemetry --
      } else {
        sinalFinal = calcularSinal(leituraAtual);
        escreverSinal(sinalFinal);

        display.setCursor(0, 0);
        display.print(protocoloNome(protocolo));
        display.print(escType == 0 ? " Uni" : " Bi");
        display.setCursor(0, 10);
        display.print("Pot: "); display.print(porcentagem); display.println("%");
        display.setCursor(0, 20);
        imprimirSinal(sinalFinal);
        display.setCursor(0, 30);
        if (protocoloEhDShot(protocolo)) {
          display.print("Quadro: "); imprimirHex4(display, saidaQuadroDShot());
        } else {
          display.print("--------------");
        }
        display.setCursor(0, 40);
        display.print("ADC: "); display.print(leituraAtual);
        display.display();

        enviarCSV(sinalFinal, leituraAtual, porcentagem, "ARMED");
      }

    // -- DISARMED: waiting for zero throttle --
    } else {
      saidaEscreverParado();

      display.clearDisplay();
      display.setCursor(0, 8);
      display.println("Gire o pot");
      display.println("para o minimo");
      display.println("(esquerda)");
      display.println("para armar.");
      display.display();

      enviarCSV(valorParada(), leituraAtual, 0, "DISARMED");
    }
  }

  leituraInicial = leituraAtual;
  delay(10);
}

// ==============================================================
// Helper: sends one CSV line over serial
// Format: timestamp_ms,pwm_us,adc_raw,throttle_pct,esc_type,system_state,
//         protocol,dshot_value,dshot_frame
// ==============================================================
void enviarCSV(int sinal, int adcRaw, int pct, const char* state) {
  bool dshot = protocoloEhDShot(protocolo);
  Serial.print(millis());       Serial.print(",");
  if (!dshot) Serial.print(sinal);
  Serial.print(",");
  Serial.print(adcRaw);         Serial.print(",");
  Serial.print(pct);            Serial.print(",");
  Serial.print(escType == 0 ? "UNI" : "BI"); Serial.print(",");
  Serial.print(state);          Serial.print(",");
  Serial.print(protocoloNome(protocolo)); Serial.print(",");
  if (dshot) {
    Serial.print(sinal); Serial.print(",0x");
    imprimirHex4(Serial, dshotMontarQuadro(sinal, false));
  } else {
    Serial.print(",");
  }
  Serial.println();
}

// Imprime 4 digitos hexa com zeros a esquerda (ex.: 0012, 7623)
void imprimirHex4(Print& p, uint16_t v) {
  for (int8_t s = 12; s >= 0; s -= 4) p.print((v >> s) & 0xF, HEX);
}