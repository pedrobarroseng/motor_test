/*
 * saida_esc.cpp - Camada de saida para o ESC. Visao geral em saida_esc.h.
 *
 * Referencias:
 *  - Datasheet ATmega640/1280/2560, cap. 17 "16-bit Timer/Counter":
 *    modo Fast PWM 14 (TOP = ICR4), OCR4C com buffer duplo.
 *  - Documentacao DShot do Betaflight (formato do quadro, CRC, tempos):
 *    https://betaflight.com/docs/development/API/Dshot
 */
#include "saida_esc.h"
#include <util/atomic.h>

static const char* const NOMES[PROT_QTD] = { "PWM", "DShot150", "DShot300" };
const char* protocoloNome(uint8_t p) { return p < PROT_QTD ? NOMES[p] : "?"; }
bool protocoloEhDShot(uint8_t p)     { return p == PROT_DSHOT150 || p == PROT_DSHOT300; }

// ---------------------------------------------------------------
// PWM: Timer4 em Fast PWM, prescaler 8 -> 1 tick = 0,5 us.
//   Periodo = 40000 ticks = 20 ms (50 Hz). Pulso = OCR4C + 1 ticks.
// ---------------------------------------------------------------
static const uint16_t PWM_TOP = 39999;

// ---------------------------------------------------------------
// DShot: Timer4 sem prescaler -> 1 tick = 62,5 ns.
//   Cada periodo do timer e UM bit. O bit "1" fica 75 % em alto e o
//   bit "0" fica 37,5 % em alto (tabela do Betaflight).
// ---------------------------------------------------------------
struct ParamDShot { uint8_t bitTicks, t1hTicks, t0hTicks; };
static const ParamDShot DSHOT[2] = {
  { 107, 80, 40 },   // DShot150: bit 6,69 us | "1" = 5,00 us | "0" = 2,50 us
  {  53, 40, 20 },   // DShot300: bit 3,31 us | "1" = 2,50 us | "0" = 1,25 us
};

static const uint8_t WGM4_B = _BV(WGM43) | _BV(WGM42);   // modo 14 (parte em TCCR4B)
static const uint8_t WGM4_A = _BV(WGM41);                // modo 14 (parte em TCCR4A)

// ---------------------------------------------------------------
// Estado
// ---------------------------------------------------------------
static uint8_t protAtual  = PROT_PWM;
static bool    bidirAtual = false;
static bool    ativo      = false;

static volatile uint16_t quadroAtual     = 0;   // quadro enviado a cada 1 ms
static volatile uint16_t quadroComando   = 0;
static volatile uint8_t  comandoRestante = 0;
static uint8_t ocrZero = 0, ocrUm = 0;          // OCR4C para bit "0" e bit "1"

// ---------------------------------------------------------------
uint16_t dshotMontarQuadro(uint16_t valor, bool telemetria) {
  uint16_t v = (uint16_t)((valor & 0x07FF) << 1) | (telemetria ? 1 : 0);
  uint8_t crc = (v ^ (v >> 4) ^ (v >> 8)) & 0x0F;
  return (uint16_t)(v << 4) | crc;
}

// Envia 1 quadro de 16 bits no pino 8 (chamada dentro da ISR do Timer3).
//
// Como funciona:
//   * o Timer4 roda sem parar com periodo = 1 bit DShot;
//   * entre quadros a saida OC4C fica DESCONECTADA (pino em LOW);
//   * no Fast PWM o pino sobe no inicio do periodo e desce quando o
//     contador chega em OCR4C -> cada periodo vira um bit;
//   * o OCR4C tem buffer duplo: o valor escrito durante o bit k vale
//     para o bit k+1, entao a CPU sempre esta 1 bit adiantada;
//   * a CPU so precisa entregar o proximo valor dentro de 1 periodo;
//     as bordas saem do hardware, sem atraso de software.
static void enviarQuadro(uint16_t q) {
  uint8_t ocr[16];
  for (uint8_t i = 0; i < 16; i++) {
    ocr[i] = (q & 0x8000) ? ocrUm : ocrZero;
    q <<= 1;
  }

  OCR4C = ocr[0];
  TIFR4 = _BV(TOV4);
  while (!(TIFR4 & _BV(TOV4))) { }     // sincroniza com o inicio de um periodo
  TIFR4 = _BV(TOV4) | _BV(OCF4C);
  while (!(TIFR4 & _BV(OCF4C))) { }    // espera a descida (pino ja esta LOW)
  TCCR4A = _BV(COM4C1) | WGM4_A;       // conecta OC4C ao pino 8

  const uint8_t* p = &ocr[1];
  for (uint8_t i = 1; i < 16; i++) {
    while (!(TIFR4 & _BV(TOV4))) { }   // comecou o bit i-1
    TIFR4 = _BV(TOV4);
    OCR4C = *p++;                      // valor do bit i (fica no buffer)
  }

  while (!(TIFR4 & _BV(TOV4))) { }     // comecou o bit 15
  TIFR4 = _BV(OCF4C);
  while (!(TIFR4 & _BV(OCF4C))) { }    // descida do bit 15
  TCCR4A = WGM4_A;                     // desconecta antes do proximo periodo
}

ISR(TIMER3_COMPA_vect) {
  uint16_t q = quadroAtual;
  if (comandoRestante) {
    q = quadroComando;
    comandoRestante--;
  }
  enviarQuadro(q);
}

// ---------------------------------------------------------------
void saidaParar() {
  TIMSK3 = 0;
  TCCR3A = 0; TCCR3B = 0;
  TCCR4A = 0; TCCR4B = 0; TCCR4C = 0;
  pinMode(8, OUTPUT);
  digitalWrite(8, LOW);
  ativo = false;
  quadroAtual = 0;
  comandoRestante = 0;
}

// Garante que a saida OC4C comece em LOW antes de ligar o PWM.
static void limparLatchOC4C() {
  TCCR4B = 0;
  TCCR4A = _BV(COM4C1);
  TCCR4C = _BV(FOC4C);
}

void saidaIniciar(uint8_t protocolo, bool bidirecional) {
  saidaParar();
  protAtual  = protocolo;
  bidirAtual = bidirecional;
  limparLatchOC4C();

  if (!protocoloEhDShot(protocolo)) {
    ICR4  = PWM_TOP;
    TCNT4 = 0;
    ativo = true;
    saidaEscreverParado();                 // 1000 ou 1500 us
    TCCR4A = _BV(COM4C1) | WGM4_A;
    TCCR4B = WGM4_B | _BV(CS41);           // prescaler 8 -> 0,5 us por tick
  } else {
    const ParamDShot& d = DSHOT[protocolo - PROT_DSHOT150];
    ICR4    = d.bitTicks - 1;
    ocrUm   = d.t1hTicks - 1;
    ocrZero = d.t0hTicks - 1;
    OCR4C   = ocrZero;
    TCNT4   = 0;
    TCCR4A  = WGM4_A;                      // OC4C desconectada: pino em LOW
    TCCR4B  = WGM4_B | _BV(CS40);          // sem prescaler, 1 periodo = 1 bit
    ativo   = true;
    saidaEscreverParado();                 // quadros com valor 0

    // Timer3: CTC, prescaler 64 -> 250 kHz, OCR3A = 249 -> 1 kHz
    TCCR3A = 0;
    TCCR3B = _BV(WGM32) | _BV(CS31) | _BV(CS30);
    OCR3A  = 249;
    TCNT3  = 0;
    TIFR3  = _BV(OCF3A);
    TIMSK3 = _BV(OCIE3A);
  }
}

void saidaEscreverPWM(uint16_t us) {
  if (!ativo || protocoloEhDShot(protAtual)) return;
  us = constrain(us, 1000, 2000);
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { OCR4C = us * 2 - 1; }   // pulso = OCR + 1 ticks
}

void saidaEscreverDShot(uint16_t valor) {
  if (!ativo || !protocoloEhDShot(protAtual)) return;
  if (valor > 2047) valor = 2047;
  uint16_t q = dshotMontarQuadro(valor, false);
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { quadroAtual = q; }
}

void saidaEscreverParado() {
  if (!ativo) return;
  if (protocoloEhDShot(protAtual)) saidaEscreverDShot(0);
  else                             saidaEscreverPWM(bidirAtual ? 1500 : 1000);
}

uint16_t saidaQuadroDShot() {
  if (!ativo || !protocoloEhDShot(protAtual)) return 0;
  uint16_t q;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { q = quadroAtual; }
  return q;
}

void saidaComandoDShot(uint8_t comando, uint8_t repeticoes) {
  if (!ativo || !protocoloEhDShot(protAtual) || comando == 0 || comando > 47) return;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    quadroComando   = dshotMontarQuadro(comando, true);
    comandoRestante = repeticoes;
  }
  while (comandoRestante) { }   // a ISR desconta 1 por quadro (1 por ms)
}