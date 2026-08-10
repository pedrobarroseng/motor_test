import matplotlib.pyplot as plt
import pandas as pd
import numpy as np

mode = input("Mode? (1 = individual, 2 = comparison): ")

if mode == '1':

    file = input("Enter the CSV file name (e.g., teste_2026-07-0-19_14-30-00.csv): ")
    label = input("Label for the plot (e.g., UNI, BI): ").title()
    df = pd.read_csv(f"data/raw/{file}")
    df = df[df['system_state'] == 'ARMED']
    df['timestamp_ms'] = df['timestamp_ms'] - df['timestamp_ms'].iloc[0]

    # Linear fit of pwm_us vs adc_raw (output vs input at the same instant).
    # This measures how linear the firmware's ADC->PWM mapping is, NOT how
    # steady the manual potentiometer actuation was — hand tremor changes the
    # shape of the curve over time, but not the point-by-point proportionality
    # between adc_raw and pwm_us, unless the firmware mapping itself is non-linear.
    r_squared = np.corrcoef(df['adc_raw'], df['pwm_us'])[0, 1] ** 2

    # Printed with more decimal places so the exact value is available for the
    # paper text (the plot legend below only shows 3 decimals for readability).
    print(f"[{label}] R² (pwm_us vs adc_raw) = {r_squared:.5f}")

    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 8))

    # R² added to the legend as a quantitative linearity metric, replacing
    # purely qualitative claims like "smooth" or "no noticeable jitter".
    ax1.plot(df['timestamp_ms'], df['pwm_us'], label=f'{label} (R² = {r_squared:.3f})')
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

    plt.tight_layout()  # Avoids overlap between subplots
    plt.savefig(f"data/raw/{file.replace('.csv', '.png')}")  # Saves the plot as PNG
    plt.show()  # Displays the plot on screen

elif mode == '2':
    file1 = input("Enter the first CSV file name (e.g., teste_2026-07-0-19_14-30-00.csv): ")
    file2 = input("Enter the second CSV file name (e.g., teste_2026-07-0-19_14-30-00.csv): ")

    label1 = input("Label for the first file (e.g., UNI): ").title()
    label2 = input("Label for the second file (e.g., BI): ").title()

    df1 = pd.read_csv(f"data/raw/{file1}")
    df2 = pd.read_csv(f"data/raw/{file2}")

    df1 = df1[df1['system_state'] == 'ARMED']
    df2 = df2[df2['system_state'] == 'ARMED']

    df1['timestamp_ms'] = df1['timestamp_ms'] - df1['timestamp_ms'].iloc[0]
    df2['timestamp_ms'] = df2['timestamp_ms'] - df2['timestamp_ms'].iloc[0]

    # Same linear-fit logic as mode 1, computed separately for each file.
    # Calculated here (before the max_time cut below) so each R² reflects
    # its own full ARMED segment, not just the time window shared by both files.
    r2_1 = np.corrcoef(df1['adc_raw'], df1['pwm_us'])[0, 1] ** 2
    r2_2 = np.corrcoef(df2['adc_raw'], df2['pwm_us'])[0, 1] ** 2
    print(f"[{label1}] R² (pwm_us vs adc_raw) = {r2_1:.5f}")
    print(f"[{label2}] R² (pwm_us vs adc_raw) = {r2_2:.5f}")

    max_time = min(df1['timestamp_ms'].max(), df2['timestamp_ms'].max())
    df1 = df1[df1['timestamp_ms'] <= max_time]
    df2 = df2[df2['timestamp_ms'] <= max_time]

    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 8))

    # R² added to each label as a quantitative linearity metric.
    ax1.plot(df1['timestamp_ms'], df1['pwm_us'], label=f'{label1} (R² = {r2_1:.3f})')
    ax1.plot(df2['timestamp_ms'], df2['pwm_us'], label=f'{label2} (R² = {r2_2:.3f})')
    ax1.set_ylabel('PWM (us)')
    ax1.set_xlabel('Timestamp (ms)')
    ax1.set_title('PWM Comparison vs Timestamp')
    ax1.legend()

    ax2.plot(df1['timestamp_ms'], df1['throttle_pct'], label=label1)
    ax2.plot(df2['timestamp_ms'], df2['throttle_pct'], label=label2)
    ax2.set_ylabel('Throttle (%)')
    ax2.set_xlabel('Timestamp (ms)')
    ax2.set_title('Throttle Comparison vs Timestamp')
    ax2.legend()

    ax3.plot(df1['timestamp_ms'], df1['adc_raw'], label=label1)
    ax3.plot(df2['timestamp_ms'], df2['adc_raw'], label=label2)
    ax3.set_ylabel('ADC Raw')
    ax3.set_xlabel('Timestamp (ms)')
    ax3.set_title('Raw ADC Comparison vs Timestamp')
    ax3.legend()

    plt.tight_layout()
    plt.savefig(f"data/raw/comparison_{file1[:22]}_vs_{file2[:22]}.png")
    plt.show()