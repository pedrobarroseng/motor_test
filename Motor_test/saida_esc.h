/*
 * ================================================================
 *  saida_esc.h - Camada de saida para o ESC (geracao do sinal)
 * ================================================================
 *  Tudo o que mexe no pino 8 (sinal do ESC) fica aqui. O resto do
 *  firmware so chama estas funcoes.
 *
 *  Protocolos suportados:
 *    PWM       -> pulso de 1000 a 2000 us, 50 Hz (o mesmo da Servo)
 *    DShot150  -> quadro digital de 16 bits, 1 kHz
 *    DShot300  -> quadro digital de 16 bits, 1 kHz
 *
 *  Por que nao usamos mais a biblioteca Servo?
 *    A Servo so sabe gerar PWM de 50 Hz. O DShot precisa de pulsos de
 *    1,25 a 5 us com precisao de dezenas de nanossegundos, entao o sinal
 *    passou a ser gerado direto pelo Timer4 do Mega (hardware).
 *
 *  Hardware usado (Arduino Mega 2560, 16 MHz):
 *    - Pino 8 = OC4C -> saida do Timer4 (16 bits).
 *    - Timer4: gera a forma de onda (pulso PWM ou cada bit do DShot).
 *    - Timer3: interrupcao de 1 kHz que dispara cada quadro DShot.
 *    (A Servo nao e mais incluida, entao esses timers ficam livres.)
 * ================================================================
 */
#pragma once
#include <Arduino.h>

enum Protocolo : uint8_t {
  PROT_PWM = 0,
  PROT_DSHOT150,
  PROT_DSHOT300,
  PROT_QTD
};

const char* protocoloNome(uint8_t p);     // "PWM", "DShot150", "DShot300"
bool protocoloEhDShot(uint8_t p);

// Liga a geracao do sinal ja mandando a "parada" do protocolo:
//   PWM uni = 1000 us | PWM bi = 1500 us | DShot (uni ou bi) = valor 0
void saidaIniciar(uint8_t protocolo, bool bidirecional);

// Desliga tudo e deixa o pino 8 em LOW (o ESC ve "sem sinal").
void saidaParar();

// Manda a parada do protocolo atual (mesma regra de saidaIniciar).
void saidaEscreverParado();

// PWM: largura do pulso em microssegundos (1000 a 2000).
void saidaEscreverPWM(uint16_t us);

// DShot: valor de 0 a 2047.
//   0          = parado (desarmado)
//   48..2047   = aceleracao (modo normal)
//   no modo 3D: 48..1047 = um sentido, 1048..2047 = outro sentido
void saidaEscreverDShot(uint16_t valor);

// Ultimo quadro DShot enviado (16 bits: 11 de valor, 1 de telemetria,
// 4 de CRC). E o numero que aparece na tela e que o osciloscopio deve mostrar.
uint16_t saidaQuadroDShot();

// Comando especial DShot (1 a 47), ex.: 1 = bipe, 10 = 3D ligado,
// 9 = 3D desligado, 12 = salvar configuracao. Bloqueia ate enviar
// `repeticoes` quadros (1 por ms).
void saidaComandoDShot(uint8_t comando, uint8_t repeticoes);

// Monta um quadro DShot: [valor 11 bits][telemetria 1 bit][CRC 4 bits]
uint16_t dshotMontarQuadro(uint16_t valor, bool telemetria);