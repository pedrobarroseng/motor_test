import serial
import csv
from datetime import datetime

PORTA = "/dev/ttyUSB0"  # Porta serial do Arduino
BAUD = 115200  # Taxa de transmissão

agora = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
nome_arquivo = f"data/raw/teste_{agora}.csv"

print(f"Conectando em {PORTA} a {BAUD} baud...")
ser = serial.Serial(PORTA, BAUD, timeout=1)
print("Conectado! Precione Ctrl+C para parar a encerrar.")

try:
    with open(nome_arquivo, mode = 'w', newline='') as arquivo_csv:
        writer = csv.writer(arquivo_csv)

        while True:
            linha = ser.readline().decode('utf-8').strip()  # Lê uma linha da porta serial
            print(linha)  # Exibe a linha recebida no console
            writer.writerow(linha.split(','))  # Escreve a linha no arquivo CSV

    ser.close()  # Fecha a conexão serial
    print("port serial fechada. Aquisição de dados encerrada.")

except KeyboardInterrupt:
    print("\nInterrompido pelo usuário. Encerrando a aquisição de dados...")


