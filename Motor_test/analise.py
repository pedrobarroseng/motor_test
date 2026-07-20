import matplotlib.pyplot as plt
import pandas as pd

arquivo = input("Digite o nome do arquivo CSV (ex: teste_2026-07-0-19_14-30-00.csv): ")
df = pd.read_csv(f"data/raw/{arquivo}")

fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 8))

ax1.plot(df['timestamp_ms'], df['pwm_us'])
ax1.set_ylabel('PWM (us)')
ax1.set_xlabel('Timestamp (ms)')
ax1.set_title('PWM vs Timestamp')

ax2.plot(df['timestamp_ms'], df['throttle_pct'])
ax2.set_ylabel('Throttle (%)')
ax2.set_xlabel('Timestamp (ms)')
ax2.set_title('Throttle vs Timestamp')  

ax3.plot(df['timestamp_ms'], df['adc_raw'])
ax3.set_ylabel('ADC Raw')
ax3.set_xlabel('Timestamp (ms)')
ax3.set_title('ADC Raw vs Timestamp')

plt.tight_layout() #Evita sobreposição entre os subplots
plt.savefig(f"data/raw/{arquivo.replace('.csv', '.png')}") #salva o gráfico como PNG
plt.show() #Exibe o gráfico na tela



