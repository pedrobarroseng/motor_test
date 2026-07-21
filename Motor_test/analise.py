import matplotlib.pyplot as plt
import pandas as pd

modo = input("Modo? (1 = individual, 2 = comparação): ")

if modo == '1':
    
    arquivo = input("Digite o nome do arquivo CSV (ex: teste_2026-07-0-19_14-30-00.csv): ")
    label = input("Label para o gráfico (ex: UNI, BI): ").title()
    df = pd.read_csv(f"data/raw/{arquivo}")
    df = df[df['system_state'] == 'ARMED']
    df['timestamp_ms'] = df['timestamp_ms'] - df['timestamp_ms'].iloc[0]
    

    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 8))

    ax1.plot(df['timestamp_ms'], df['pwm_us'], label=label)
    ax1.set_ylabel('PWM (us)')
    ax1.set_xlabel('Timestamp (ms)')
    ax1.set_title(f'PWM vs Timestamp - {label}')
    ax1.legend()

    ax2.plot(df['timestamp_ms'], df['throttle_pct'], label=label)
    ax2.set_ylabel('Throttle (%)')
    ax2.set_xlabel('Timestamp (ms)')
    ax2.set_title(f'Throttle vs Timestamp - {label}')
    ax2.legend()

    ax3.plot(df['timestamp_ms'], df['adc_raw'], label=label)
    ax3.set_ylabel('ADC Raw')
    ax3.set_xlabel('Timestamp (ms)')
    ax3.set_title(f'ADC Raw vs Timestamp - {label}')
    ax3.legend()

    plt.tight_layout() #Evita sobreposição entre os subplots
    plt.savefig(f"data/raw/{arquivo.replace('.csv', '.png')}") #salva o gráfico como PNG
    plt.show() #Exibe o gráfico na tela

elif modo == '2':
    arquivo1 = input("Digite o nome do primeiro arquivo CSV (ex: teste_2026-07-0-19_14-30-00.csv): ")
    arquivo2 = input("Digite o nome do segundo arquivo CSV (ex: teste_2026-07-0-19_14-30-00.csv): ")

    label1 = input("Label para o primeiro arquivo (ex: UNI): ").title()
    label2 = input("Label para o segundo arquivo (ex: BI): ").title()

    df1 = pd.read_csv(f"data/raw/{arquivo1}")
    df2 = pd.read_csv(f"data/raw/{arquivo2}")

    df1 = df1[df1['system_state'] == 'ARMED']
    df2 = df2[df2['system_state'] == 'ARMED']

    df1['timestamp_ms'] = df1['timestamp_ms'] - df1['timestamp_ms'].iloc[0]
    df2['timestamp_ms'] = df2['timestamp_ms'] - df2['timestamp_ms'].iloc[0]

    tempo_max = min(df1['timestamp_ms'].max(), df2['timestamp_ms'].max())
    df1 = df1[df1['timestamp_ms'] <= tempo_max]
    df2 = df2[df2['timestamp_ms'] <= tempo_max]

    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 8))

    ax1.plot(df1['timestamp_ms'], df1['pwm_us'], label=label1)
    ax1.plot(df2['timestamp_ms'], df2['pwm_us'], label=label2)
    ax1.set_ylabel('PWM (us)')
    ax1.set_xlabel('Timestamp (ms)')
    ax1.set_title('Comparação PWM vs Timestamp')
    ax1.legend()

    ax2.plot(df1['timestamp_ms'], df1['throttle_pct'], label=label1)
    ax2.plot(df2['timestamp_ms'], df2['throttle_pct'], label=label2)
    ax2.set_ylabel('Throttle (%)')
    ax2.set_xlabel('Timestamp (ms)')
    ax2.set_title('Comparação Throttle vs Timestamp')
    ax2.legend()

    ax3.plot(df1['timestamp_ms'], df1['adc_raw'], label=label1)
    ax3.plot(df2['timestamp_ms'], df2['adc_raw'], label=label2)
    ax3.set_ylabel('ADC Raw')
    ax3.set_xlabel('Timestamp (ms)')
    ax3.set_title('Comparação ADC Raw vs Timestamp')
    ax3.legend()
    
    plt.tight_layout()
    plt.savefig(f"data/raw/comparacao_{arquivo1[:22]}_vs_{arquivo2[:22]}.png")
    plt.show()


