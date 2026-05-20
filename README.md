# Gerador de Sinais para Teste de ESC (Unidirecional e Bidirecional)

![Arduino](https://img.shields.io/badge/Platform-Arduino-00979D?style=for-the-badge&logo=Arduino&logoColor=white)
![Linux](https://img.shields.io/badge/OS-Ubuntu-E95420?style=for-the-badge&logo=Ubuntu&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)

Este projeto consiste em um sistema de bancada profissional desenvolvido em microcontrolador ATmega (Arduino) para teste, calibração e validação de Controladores Eletrônicos de Velocidade (ESCs). Projetado sob rígidos critérios de segurança em sistemas embarcados, o dispositivo impede acionamentos mecânicos acidentais através de validações em tempo real por hardware e software.

---

## 📋 Índice

- [Descrição do Projeto](#-descrição-do-projeto)
- [Funcionalidades e Camadas de Segurança](#-funcionalidades-e-camadas-de-segurança)
- [Dependências e Bibliotecas](#-dependências-e-bibliotecas)
- [Arquitetura e Explicação do Código](#-arquitetura-e-explicação-do-código)
- [Mapeamento de Hardware (Pinout)](#-mapeamento-de-hardware-pinout)
- [Instalação e Configuração no Ubuntu](#-instalação-e-configuração-no-ubuntu)
- [Como Operar](#-como-operar)
- [Licença](#-licença)

---

## 🚀 Descrição do Projeto

Em bancadas de testes de motores CC sem escovas (BLSI/Brushless), a troca de sinais PWM brutos sem uma interface de controle pode causar acionamentos abruptos, danos estruturais ou acidentes. Este dispositivo centraliza o controle gerando sinais PPM/PWM padronizados através de uma interface visual clara em um display gráfico Nokia 5110, gerenciada por um único botão multifuncional e um potenciômetro de precisão.

---

## 🛡️ Funcionalidades e Camadas de Segurança

- **Menu de Seleção Trancado (Safety Interlock):** O tipo de ESC (Unidirecional ou Bidirecional) é definido rigidamente na inicialização. Por segurança normativa, **a troca de modo é totalmente bloqueada durante o loop de execução do motor**, prevenindo inversões de marcha acidentais (de aceleração para ré total).
- **Filtro de Ruído e Detecção de Desconexão (Anti-Floating Check):** O software monitora o gradiente de variação analógica. Se houver um salto instantâneo na leitura ($\Delta V \ge 2.5V$, equivalente a `512` passos do ADC), o sistema interpreta como rompimento de cabo ou potenciômetro danificado, corta o sinal do motor instantaneamente para o estado neutro seguro e exibe `connection error`.
- **Rotina de Armação Obrigatória:** O motor só recebe o sinal de liberação operacional se o potenciômetro físico for detectado abaixo do limite seguro de partida (`ADC < 50`).
- **Trava de Centro (Neutral Lock para Bi-dir):** No modo bidirecional, o motor permanece travado eletronicamente em 1500ms até que o operador passe fisicamente o potenciômetro pela zona morta central ($1500ms \pm 30ms$).

---

## 📚 Dependências e Bibliotecas

O projeto faz uso de três bibliotecas principais para garantir a modulação de sinal precisa e a renderização gráfica de baixo nível:

1. **`Servo.h` (Nativa do Arduino):**
   - *Função:* Utilizada para abstrair a geração de pulsos PWM/PPM.
   - *Explicação Técnica:* Em vez de utilizar delays via software, esta biblioteca configura os Timers internos de hardware do microcontrolador para gerar um sinal periódico estável de 50 Hz, enviando larguras de pulso precisas controladas em microssegundos (`writeMicroseconds`).
2. **`Adafruit_GFX.h` (Adafruit GFX Library):**
   - *Função:* Biblioteca core de computação gráfica.
   - *Explicação Técnica:* Fornece primitivas de desenho geométrico em matriz de pontos (linhas, retângulos, círculos e tratamento de fontes de texto), servindo de base matemática para a construção da interface do display.
3. **`Adafruit_PCD8544.h` (Adafruit Nokia 5110 Library):**
   - *Função:* Driver de controle de hardware do display.
   - *Explicação Técnica:* Controla diretamente o chip PCD8544 do display LCD Nokia 5110 via protocolo SPI por software, gerenciando os pinos de dados, clock, reset e seleção para atualizar o buffer de tela de 84x48 pixels.

---

## ⚙️ Arquitetura e Explicação do Código

O fluxo lógico do firmware está estruturado como uma **Máquina de Estados Finita (FSM)** dividida em duas etapas principais:

### 1. Rotina de Inicialização (`setup`)
- Inicializa os periféricos de E/S e o display.
- Executa um laço `while(!menuConfirmado)` que lê o botão sob uma lógica de temporização diferencial:
  - *Clique curto (<800ms):* Inverte a variável de estado `escType`.
  - *Clique longo (>=1000ms):* Processa um mapeamento dinâmico (`map`) que desenha uma barra de progresso geométrica (`drawRect` e `fillRect`) na tela. Atingindo 100%, confirma a seleção e encerra o laço.

### 2. Ciclo de Execução Contínuo (`loop`)
- **Filtro de Segurança:** Executa o cálculo da diferença absoluta `abs(leituraAtual - leituraInicial)` para identificar falhas elétricas abertas.
- **Exclusividade de Renderização (Anti-Flicker):** O código limpa o buffer de tela uma única vez por ciclo usando `display.clearDisplay()`. A exibição é segregada estritamente por estruturas condicionais `if (esperandoNeutro) / else`. Isso garante que as mensagens de aviso e as strings de telemetria operem em memória isolada, impedindo a sobreposição física de caracteres no LCD. 
- **Envio de Dados:** O comando `display.display()` e o envio de PWM para o atuador (`motor.writeMicroseconds()`) são executados de forma atômica no final da cadeia de decisões do loop.

---

## 📐 Mapeamento de Hardware (Pinout)

### Conexões do Display LCD Nokia 5110
| Pino do LCD  | Pino do Arduino | Função                              |
| :----------- | :-------------- | :---------------------------------- |
| **CLK**      | D13             | Clock do barramento serial          |
| **DIN**      | D12             | Entrada de Dados (MOSI)             |
| **DC**       | D11             | Comando / Seleção de Dados          |
| **CE** ou CS | D10             | Chip Enable (Seleção do Periférico) |
| **RST**      | D9              | Reset do controlador de tela        |
| **VCC**      | 3.3V            | Alimentação Lógica (Máx 3.3V)       |
| **LIGHT**    | GND / Resistor  | Controle do Backlight               |
| **GND**      | GND             | Referência de aterramento comum     |

### Periféricos de Controle e Atuação
- **Entrada Analógica do Potenciômetro:** Pino **A0**
- **Saída de Controle PWM para o ESC:** Pino **D8**
- **Entrada do Botão do Menu:** Pino **D22** *(Configurado como INPUT_PULLUP interno)*

---

## 💻 Instalação e Configuração no Ubuntu

Para clonar, compilar e gravar este projeto utilizando o sistema operacional **Ubuntu (Linux)**, siga as diretrizes abaixo:

1. Certifique-se de que seu usuário possui acesso direto à interface de comunicação serial e gravação de microcontroladores (permissão `dialout`):
   ```bash
   sudo usermod -a -G dialout $USER
