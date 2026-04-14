# Controle de Motor Brushless DC - Laboratório NAUTEC

Este projeto consiste em um sistema de controle e monitoramento em tempo real para motores Brushless (BLDC), utilizando um **Arduino Mega 2560** e um **Display Nokia 5110**. O foco principal é a segurança da operação e a versatilidade de teste para diferentes tipos de ESC.

---

## Funcionalidades Principais

* **Dual Mode ESC:** Suporte para ESCs Unidirecionais e Bidirecionais.
* **Segurança Ativa (Failsafe):** Monitoramento constante da integridade do sinal do potenciômetro.
* **Protocolo de Armação (Safety Arming):** Proteção contra partidas acidentais ao ligar o sistema.
* **Monitoramento Real-time:** Exibição de Potência (%), Sinal PWM (ms) e leitura bruta do ADC no LCD.

---

## Bibliotecas Necessárias

Para compilar este projeto, instale as seguintes bibliotecas através do Gerenciador de Bibliotecas da Arduino IDE:

1. **Servo** (por Arduino) - Nativa, usada para o sinal PWM do ESC.
2. **Adafruit GFX Library** (por Adafruit) - Base para gráficos no display.
3. **Adafruit PCD8544 Nokia 5110 LCD library** (por Adafruit) - Driver específico para o display.
4. **SPI** (Nativa) - Protocolo de comunicação com o display.

---

## Camadas de Segurança Implementadas

### 1. Failsafe de Conexão
O sistema calcula a variação do sinal entre cada ciclo. Se houver um salto brusco ($\Delta \geq 512$ unidades analógicas), o sistema assume uma falha de hardware (fio solto ou mau contato) e trava o motor instantaneamente, exibindo `connection error`.

### 2. Proteção de Partida (Arming)
Para evitar que o motor ligue em alta rotação caso o potenciômetro tenha sido esquecido em uma posição alta, o motor inicia desarmado. Ele só liberará o sinal de aceleração após o operador levar o potenciômetro manualmente para a posição zero (valor < 50).

---

## 🔌 Esquema de Ligação (Pinout)

| Componente | Pino Arduino | Descrição |
| :--- | :--- | :--- |
| **Potenciômetro** | `A0` | Controle de velocidade |
| **Chave Gangorra** | `22` | HIGH: Uni / LOW: Bi |
| **Sinal do ESC** | `8` | Saída PWM (Servo) |
| **LCD CLK** | `13` | Serial Clock |
| **LCD DIN** | `12` | Serial Data In |
| **LCD DC** | `11` | Data/Command Control |
| **LCD CE** | `10` | Chip Enable |
