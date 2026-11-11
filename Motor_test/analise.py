import matplotlib.pyplot as plt
import pandas as pd
import numpy as np

# ------------------------------------------------------------------
# Helpers for PWM and DShot files
# ------------------------------------------------------------------
# Old CSVs (PWM-only firmware) have no "protocol" column -> treated as PWM.
# PWM rows use pwm_us (microseconds); DShot rows use dshot_value (0..2047).
#
# command_pct puts both protocols on the same scale (% of the full range):
#   PWM uni   : 1000..2000 us        -> 0..100 %   (firmware limit 1900 us = 90 %)
#   PWM bi    : 1500 us = 0, 1000/2000 us = -100/+100 %
#   DShot uni : 0 = stop, 48..2047   -> 0..100 %   (firmware limit 1847 = 90 %)
#   DShot bi  : 0 = stop, 48..1047 = -0..-100 %, 1048..2047 = +0..+100 %  (3D mode)


def load(file):
    df = pd.read_csv(f"data/raw/{file}")
    if 'protocol' not in df.columns:
        df['protocol'] = 'PWM'
    df = df[df['system_state'] == 'ARMED'].copy()
    df['timestamp_ms'] = df['timestamp_ms'] - df['timestamp_ms'].iloc[0]
    return df


def is_dshot(df):
    return str(df['protocol'].iloc[0]).startswith('DShot')


def signal(df):
    """Native signal sent to the ESC + axis label."""
    if is_dshot(df):
        return df['dshot_value'].astype(float), 'DShot value (0-2047)'
    return df['pwm_us'].astype(float), 'PWM (us)'


def command_pct(df):
    bi = df['esc_type'].iloc[0] == 'BI'
    if is_dshot(df):
        v = df['dshot_value'].astype(float)
        if not bi:
            return np.where(v == 0, 0.0, (v - 48) / 1999 * 100)
        return np.where(v == 0, 0.0,
                        np.where(v >= 1048, (v - 1048) / 999 * 100, -(v - 48) / 999 * 100))
    us = df['pwm_us'].astype(float)
    return (us - 1500) / 500 * 100 if bi else (us - 1000) / 1000 * 100


def r2(df):
    # Linear fit of the command vs adc_raw (output vs input at the same instant).
    # This measures how linear the firmware's ADC->signal mapping is, NOT how
    # steady the manual potentiometer actuation was. command_pct is used so the
    # DShot 3D halves (48..1047 / 1048..2047) become one continuous scale.
    return np.corrcoef(df['adc_raw'], command_pct(df))[0, 1] ** 2


def name(df, label):
    return f"{label} - {df['protocol'].iloc[0]}"


mode = input("Mode? (1 = individual, 2 = comparison): ")

if mode == '1':

    file = input("Enter the CSV file name (e.g., teste_2026-11-11_14-30-00.csv): ")
    label = input("Label for the plot (e.g., UNI, BI): ").title()
    df = load(file)
    r_squared = r2(df)
    sig, sig_label = signal(df)

    # Printed with more decimal places so the exact value is available for the
    # paper text (the plot legend below only shows 3 decimals for readability).
    print(f"[{name(df, label)}] R² (command vs adc_raw) = {r_squared:.5f}")

    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 8))

    ax1.plot(df['timestamp_ms'], sig, label=f'{label} (R² = {r_squared:.3f})')
    ax1.set_ylabel(sig_label)
    ax1.set_xlabel('Timestamp (ms)')
    ax1.set_title(f'{df["protocol"].iloc[0]} vs Timestamp - {label}')
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
    file1 = input("Enter the first CSV file name (e.g., teste_2026-11-11_14-30-00.csv): ")
    file2 = input("Enter the second CSV file name (e.g., teste_2026-11-11_14-35-00.csv): ")

    label1 = input("Label for the first file (e.g., PWM, UNI): ").title()
    label2 = input("Label for the second file (e.g., DSHOT300, BI): ").title()

    df1 = load(file1)
    df2 = load(file2)

    # R² computed before the max_time cut below, so each value reflects
    # its own full ARMED segment.
    r2_1, r2_2 = r2(df1), r2(df2)
    print(f"[{name(df1, label1)}] R² (command vs adc_raw) = {r2_1:.5f}")
    print(f"[{name(df2, label2)}] R² (command vs adc_raw) = {r2_2:.5f}")

    max_time = min(df1['timestamp_ms'].max(), df2['timestamp_ms'].max())
    df1 = df1[df1['timestamp_ms'] <= max_time]
    df2 = df2[df2['timestamp_ms'] <= max_time]

    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 8))

    # Same protocol -> plot the native signal (us or DShot value).
    # Different protocols (e.g. PWM vs DShot300) -> plot both as % of range.
    same = df1['protocol'].iloc[0] == df2['protocol'].iloc[0]
    if same:
        y1, ylab = signal(df1)
        y2, _ = signal(df2)
        title = f'{df1["protocol"].iloc[0]} Comparison vs Timestamp'
    else:
        y1, y2 = command_pct(df1), command_pct(df2)
        ylab = 'Command (% of range)'
        title = 'Command Comparison vs Timestamp'

    ax1.plot(df1['timestamp_ms'], y1, label=f'{label1} (R² = {r2_1:.3f})')
    ax1.plot(df2['timestamp_ms'], y2, label=f'{label2} (R² = {r2_2:.3f})')
    ax1.set_ylabel(ylab)
    ax1.set_xlabel('Timestamp (ms)')
    ax1.set_title(title)
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