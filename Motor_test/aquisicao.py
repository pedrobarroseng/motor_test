import os
import serial
import csv
from datetime import datetime

PORTA = "/dev/ttyUSB0"  # Porta serial do Arduino (no Windows: "COM3", "COM4"...)
BAUD = 115200  # Taxa de transmissão

PASTA = "data/raw"
os.makedirs(PASTA, exist_ok=True)


def novo_arquivo():
    agora = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    nome = f"{PASTA}/teste_{agora}.csv"
    print(f"\n>>> Gravando em {nome}")
    return nome


print(f"Conectando em {PORTA} a {BAUD} baud...")
ser = serial.Serial(PORTA, BAUD, timeout=1)
print("Conectado! Pressione Ctrl+C para parar e encerrar.")

# Cada vez que o Arduino manda o cabeçalho (início de sessão, depois do menu
# ou do botão de reset), começa um arquivo novo. Assim cada teste (protocolo
# + direção) fica num CSV separado e o analise.py não se perde com cabeçalhos
# repetidos no meio do arquivo.
arquivo_csv = None
writer = None
linhas = 0

try:
    while True:
        linha = ser.readline().decode('utf-8', errors='ignore').strip()  # Lê uma linha da porta serial
        if not linha:
            continue  # Se a linha estiver vazia, continua para a próxima iteração
        print(linha)  # Exibe a linha recebida no console

        if linha.startswith("timestamp_ms"):  # cabeçalho -> nova sessão
            if arquivo_csv:
                arquivo_csv.close()
                print(f">>> {linhas} linhas salvas.")
            arquivo_csv = open(novo_arquivo(), mode='w', newline='')
            writer = csv.writer(arquivo_csv)
            linhas = 0

        if writer is None:
            continue  # ainda não chegou o cabeçalho (Arduino nos menus)

        writer.writerow(linha.split(','))  # Escreve a linha no arquivo CSV
        linhas += 1

except KeyboardInterrupt:
    print("\nInterrompido pelo usuário. Encerrando a aquisição de dados...")

finally:
    if arquivo_csv:
        arquivo_csv.close()
        print(f">>> {linhas} linhas salvas no último arquivo.")
    ser.close()  # Fecha a conexão serial
    print("Porta serial fechada. Aquisição de dados encerrada.")