# Controle de Motor BLDC com Monitoramento em Tempo Real

Este projeto foi desenvolvido para o laboratório **NAUTEC** com o objetivo de padronizar e monitorar os testes de motores Brushless (BLDC). O sistema utiliza um **Arduino Mega 2560** para controlar a velocidade do motor via **ESC (Electronic Speed Controller)**, exibindo dados críticos em um display **Nokia 5110**.

# Funcionalidades

- **Controle de Precisão:** Mapeamento do potenciômetro para sinal PWM entre 1000ms e 1900ms.
- **Leitura em Tempo Real:** Exibição da porcentagem de potência, valor do PWM e leitura analógica bruta.
- **Segurança de Armação:** Protocolo de inicialização do ESC configurado no `setup()` com delay de 2 segundos para garantir a sincronia.
- **Interface Otimizada:** Layout do display organizado para máxima legibilidade em 84x48 pixels.

# Pré-requisitos (Bibliotecas)

Para compilar este código, você precisará instalar as seguintes bibliotecas na sua Arduino IDE:

* **[Servo.h](https://www.arduino.cc/reference/en/libraries/servo/):** Controle do sinal PWM para o ESC.
* **[Adafruit_PCD8544](https://github.com/adafruit/Adafruit-PCD8544-Nokia-5110-LCD-library):** Driver do display Nokia 5110.
* **[Adafruit_GFX](https://github.com/adafruit/Adafruit-GFX-Library):** Biblioteca gráfica para desenhar texto e formas.
* **[SPI.h](https://www.arduino.cc/en/reference/SPI):** Protocolo de comunicação entre Arduino e Display.

# Esquema de Pinagem (Arduino Mega)

| Componente        | Pino Arduino | Função                 |
| :-----------------| :------------|------------------------|
| **ESC (Sinal)**   | 8            | Saída PWM para o motor |
| **Potenciômetro** | A0           | Entrada Analógica      |
| **LCD CLK**       | 13           | Serial Clock           |
| **LCD DIN**       | 12           | Serial Data In         |
| **LCD DC**        | 11           | Data/Command Control   |
| **LCD CE**        | 10           | Chip Enable            |
| **LCD RST**       | 9            | Reset                  |

# Lógica do Código

O projeto utiliza a função `map()` para converter escalas de grandezas diferentes:
1.  **Entrada:** O potenciômetro gera um sinal analógico de 0 a 1023.
2.  **Processamento:** O código traduz esse valor para a linguagem do ESC (1000ms a 1900ms).
3.  **Saída:** A função `motor.writeMicroseconds()` envia a largura de pulso exata para definir a velocidade de rotação.

# Como Clonar e Usar

1. Clone o repositório:
   ```bash
   git clone [https://github.com/pedrobarroseng/motor_test.git](https://github.com/pedrobarroseng/motor_test.git)
