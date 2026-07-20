/*
 * ================================================================
 *  Motor Test - ESC Controller with Nokia 5110 Display
 *  Hardware: Arduino Mega 2560
 * ================================================================
 *  SERIAL OUTPUT (115200 baud) - CSV format:
 *  timestamp_ms, pwm_us, adc_raw, throttle_pct, esc_type, system_state
 *
 *  system_state values:
 *    DISARMED        -> Motor not yet armed (waiting for zero throttle)
 *    WAITING_NEUTRAL -> Bidirectional ESC waiting for center position
 *    ARMED           -> Normal operation
 *    LOCKED          -> Potentiometer disconnection detected (safety lock)
 * ================================================================
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <Servo.h>

// -- Pin definitions --------------------------------------------
const int PIN_POTENCIOMETRO = A0;
const int PIN_ESC           = 8;
const int PIN_BUTTON_ESC    = 22;
const int PIN_BUTTON_RESET  = 23;

// -- Display: CLK, DIN, DC, CE, RST -----------------------------
Adafruit_PCD8544 display = Adafruit_PCD8544(13, 12, 11, 10, 9);

// -- Motor ------------------------------------------------------
Servo motor;

// -- ESC configuration ------------------------------------------
// 0 = unidirectional | 1 = bidirectional
int escType = 0;

// -- Safety layer -----------------------------------------------
int  leituraInicial  = 0;
bool sistemaTravado  = false;
bool motorArmado     = false;
bool sinalLiberado   = false;

// -- Menu state ------------------------------------------------
bool menuConfirmado  = false;
unsigned long tempoSegurando = 0;
bool botaoAnterior   = false;

// -- Serial header flag ----------------------------------------
bool headerEnviado = false;

// ==============================================================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_POTENCIOMETRO, INPUT);
  pinMode(PIN_BUTTON_ESC,    INPUT_PULLUP);
  pinMode(PIN_BUTTON_RESET,  INPUT_PULLUP);

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

  // -- ESC Type selection menu --
  menuConfirmado = false;
  botaoAnterior  = false;
  tempoSegurando = 0;

  while (!menuConfirmado) {
    bool estadoBotao = (digitalRead(PIN_BUTTON_ESC) == LOW);

    // Rising edge: button just pressed
    if (estadoBotao && !botaoAnterior) {
      tempoSegurando = millis();
    }

    // Falling edge: button just released (short press -> toggle)
    if (!estadoBotao && botaoAnterior) {
      if (millis() - tempoSegurando < 800) {
        escType = !escType;
      }
      tempoSegurando = 0;
    }

    // Held for 1 second -> confirm
    if (estadoBotao && tempoSegurando > 0 && (millis() - tempoSegurando >= 1000)) {
      menuConfirmado = true;
    }

    botaoAnterior = estadoBotao;

    // -- Menu display --
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Escolha o ESC:");
    display.setCursor(0, 15);
    display.print(escType == 0 ? "> Uni-dir" : "  Uni-dir");
    display.setCursor(0, 28);
    display.print(escType == 1 ? "> Bi-dir " : "  Bi-dir ");

    if (estadoBotao && tempoSegurando > 0) {
      int progresso = map(millis() - tempoSegurando, 0, 1000, 0, 84);
      progresso = constrain(progresso, 0, 84);
      display.drawRect(0, 40, 84, 8, BLACK);
      display.fillRect(0, 40, progresso, 8, BLACK);
    } else {
      display.setCursor(0, 40);
      display.print("segure p/ conf.");
    }

    display.display();
    delay(20);
  }

  // -- ESC arming signal --
  motor.attach(PIN_ESC);
  motor.writeMicroseconds(escType == 0 ? 1000 : 1500);

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
    motor.writeMicroseconds(escType == 0 ? 1000 : 1500);
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
    Serial.println("timestamp_ms,pwm_us,adc_raw,throttle_pct,esc_type,system_state");
    headerEnviado = true;
  }

  // == LOCKED state ==========================================
  if (sistemaTravado) {
    motor.writeMicroseconds(1000);

    display.clearDisplay();
    display.setCursor(0, 10);
    display.println("!! ERRO !!");
    display.setCursor(0, 22);
    display.println("Pot desconectado");
    display.setCursor(0, 34);
    display.println("Pressione reset");
    display.display();

    // Report locked state
    enviarCSV(1000, leituraAtual, 0, "LOCKED");

  // == NORMAL state ==========================================
  } else {

    // -- Arming condition: throttle must be at zero first --
    if (leituraAtual < 50) {
      motorArmado = true;
    }

    if (motorArmado) {
      int sinalFinal;
      int porcentagem;

      //-- Signal mapping per ESC type--
      if (escType == 0) {
        sinalFinal  = map(leituraAtual, 0, 1023, 1000, 1900);
        porcentagem = map(leituraAtual, 0, 1023, 0, 100);
        sinalLiberado = true;
      } else {
        sinalFinal  = map(leituraAtual, 0, 1023, 1000, 2000);
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
        sinalFinal = 1500;

        display.setCursor(0, 0);
        display.println("Bi-dir: va ao");
        display.println("centro (0%)");
        display.setCursor(0, 20);
        display.print("Pot: "); display.print(porcentagem); display.println("%");
        display.println("--------------");
        display.setCursor(0, 35);
        display.print("PWM alvo: "); display.print(sinalFinal);
        display.display();

        motor.writeMicroseconds(sinalFinal);
        enviarCSV(sinalFinal, leituraAtual, porcentagem, "WAITING_NEUTRAL");

      // -- ARMED: normal telemetry --
      } else {
        display.setCursor(0, 0);
        display.print(escType == 0 ? "ESC Uni-dir" : "ESC Bi-dir");
        display.setCursor(0, 10);
        display.print("Pot: "); display.print(porcentagem); display.println("%");
        display.println("--------------");
        display.setCursor(0, 25);
        display.print("PWM: "); display.print(sinalFinal); display.println("us");
        display.println("--------------");
        display.setCursor(0, 40);
        display.print("ADC: "); display.print(leituraAtual);
        display.display();

        motor.writeMicroseconds(sinalFinal);
        enviarCSV(sinalFinal, leituraAtual, porcentagem, "ARMED");
      }

    // -- DISARMED: waiting for zero throttle --
    } else {
      motor.writeMicroseconds(escType == 0 ? 1000 : 1500);

      display.clearDisplay();
      display.setCursor(0, 8);
      display.println("Gire o pot");
      display.println("para o minimo");
      display.println("(esquerda)");
      display.println("para armar.");
      display.display();

      enviarCSV(escType == 0 ? 1000 : 1500, leituraAtual, 0, "DISARMED");
    }
  }

  leituraInicial = leituraAtual;
  delay(10);
}

// ==============================================================
// Helper: sends one CSV line over serial
// Format: timestamp_ms,pwm_us,adc_raw,throttle_pct,esc_type,system_state
// ==============================================================
void enviarCSV(int pwm, int adcRaw, int pct, const char* state) {
  Serial.print(millis());       Serial.print(",");
  Serial.print(pwm);            Serial.print(",");
  Serial.print(adcRaw);         Serial.print(",");
  Serial.print(pct);            Serial.print(",");
  Serial.print(escType == 0 ? "UNI" : "BI"); Serial.print(",");
  Serial.println(state);
}
