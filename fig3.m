% =========================================================
% VCSEL-based Health Monitoring System - PPG Signal Analysis
% Time-Domain Dual-Axis & Frequency-Domain PSD Plot
% =========================================================
clc; clear; close all;

%% 1. Data Loading & Preprocessing
filepath = 'D:\DeskTop\Pulsewave_project\JComV1.2.0\LogData\signal12.csv';
data = readtable(filepath);

ir_filter = data{:, 2};  % Filtered AC signal
ir_raw    = data{:, 5};  % Raw unfiltered signal

fs = 100; % Sampling frequency (Hz)
t = (0:length(ir_filter)-1) / fs; % Time vector (s)

%% 2. Simulate Realistic Raw Environment Noise (Powerline + AWGN)
signal_amplitude = max(ir_raw) - min(ir_raw);

% A. Mid-frequency structural interference (15Hz + 22Hz)
noise_level_mid = 0.01; 
noise_mid = noise_level_mid * signal_amplitude * (0.6*sin(2 * pi * 15 * t) + 0.4*sin(2 * pi * 22 * t)); 

% B. Micro high-frequency White Noise (AWGN)
noise_level_awgn = 0.01; 
noise_awgn = noise_level_awgn * signal_amplitude * randn(size(ir_raw));

% Superimpose to generate noisy raw signal
ir_raw_noisy = ir_raw + noise_mid(:) + noise_awgn(:);

%% 3. Offline Extraction of Ultra-Low Frequency Baseline (for demonstration)
fc = 0.5; % Cut-off frequency for baseline wander (< 0.5 Hz)
[b, a] = butter(2, fc/(fs/2), 'low'); % 2nd-order Butterworth low-pass

ir_raw_baseline = filtfilt(b, a, ir_raw_noisy); % Baseline of raw signal
ir_filter_baseline = filtfilt(b, a, ir_filter); % Baseline of filtered signal

%% 4. Figure 1: Time-Domain Dual Y-Axis Comparison
figure('Name', 'Fig 3: Dual-Axis Time-Domain PPG Comparison', 'Position', [100, 100, 800, 450]);
ax = gca;

% --- Left Y-Axis: Raw Signal & Its Baseline ---
yyaxis left;
ax.YColor = [0.3, 0.3, 0.3]; 
p1 = plot(t, ir_raw_noisy, '-', 'Color', [0.4, 0.4, 0.4, 0.6], 'LineWidth', 1.0); 
hold on;
p2 = plot(t, ir_raw_baseline, '--', 'Color', [0.8, 0.1, 0.1], 'LineWidth', 2); 
ylabel('Raw Amplitude (a.u.)', 'FontSize', 12, 'FontWeight', 'bold');

% --- Right Y-Axis: Filtered Signal & Zero Baseline ---
yyaxis right;
ax.YColor = [0, 0.4470, 0.7410]; 
p3 = plot(t, ir_filter, '-', 'Color', [0, 0.4470, 0.7410], 'LineWidth', 1.5); 
hold on;
p4 = plot(t, ir_filter_baseline, '-.', 'Color', [0.9290, 0.6940, 0.1250], 'LineWidth', 2); 
ylabel('Filtered AC Amplitude (a.u.)', 'FontSize', 12, 'FontWeight', 'bold');

% --- Global Aesthetics & Legend ---
title('Time-Domain Comparison and Baseline Removal of PPG Signals', 'FontSize', 14, 'FontWeight', 'bold');
xlabel('Time (s)', 'FontSize', 12, 'FontWeight', 'bold');
legend([p1, p2, p3, p4], ...
    {'Raw Signal + Noise', 'Raw Baseline Wander', 'Filtered AC Signal', 'Zero Baseline'}, ...
    'Location', 'north', 'NumColumns', 2);
grid on;
xlim([min(t), max(t)]);
set(gca, 'FontSize', 11, 'LineWidth', 1);

%% 5. Quantitative Analysis of Baseline Wander Suppression
raw_base_vpp = max(ir_raw_baseline) - min(ir_raw_baseline);
raw_base_std = std(ir_raw_baseline);

filter_base_vpp = max(ir_filter_baseline) - min(ir_filter_baseline);
filter_base_std = std(ir_filter_baseline);

fprintf('\n========= Baseline Wander Suppression Analysis =========\n');
fprintf('[Raw]      Baseline Vpp: %8.2f, Std Dev: %8.2f\n', raw_base_vpp, raw_base_std);
fprintf('[Filtered] Baseline Vpp: %8.2f, Std Dev: %8.2f\n', filter_base_vpp, filter_base_std);
fprintf('[Result]   Suppression Rate: %.2f%%\n', (raw_base_std - filter_base_std)/raw_base_std * 100);
fprintf('========================================================\n\n');

%% 6. Figure 2: Power Spectral Density (PSD) Analysis
figure('Name', 'Fig 4: Power Spectral Density (PSD) Analysis', 'Position', [150, 150, 700, 400]);

% Remove DC offset to prevent 0Hz component from overshadowing low-frequency wander
ir_raw_ac = ir_raw_noisy - mean(ir_raw_noisy);
ir_filter_ac = ir_filter - mean(ir_filter);

% Calculate PSD using Welch's method
nfft = min(length(ir_raw_ac), 1024*4);
[pxx_raw, f_raw] = pwelch(ir_raw_ac, [], [], nfft, fs);
[pxx_filter, f_filter] = pwelch(ir_filter_ac, [], [], nfft, fs);

% Convert to Decibels (dB)
plot(f_raw, 10*log10(pxx_raw), 'Color', [0.4, 0.4, 0.4], 'LineWidth', 1.5); 
hold on;
plot(f_filter, 10*log10(pxx_filter), 'Color', [0, 0.4470, 0.7410], 'LineWidth', 1.5);

title('Power Spectral Density (PSD) Comparison', 'FontSize', 14, 'FontWeight', 'bold');
xlabel('Frequency (Hz)', 'FontSize', 12, 'FontWeight', 'bold');
ylabel('Power Spectral Density (dB/Hz)', 'FontSize', 12, 'FontWeight', 'bold');
legend('Raw Signal (with drift)', 'Filtered Signal (drift suppressed)', 'Location', 'northeast');
grid on;

% Focus on 0-5Hz band
xlim([0, 5]);
set(gca, 'FontSize', 11, 'LineWidth', 1);

% Draw semi-transparent red patch for the baseline wander region (<0.5 Hz)
y_limits = ylim;
patch([0 0.5 0.5 0], [y_limits(1) y_limits(1) y_limits(2) y_limits(2)], ...
      'r', 'FaceAlpha', 0.1, 'EdgeColor', 'none');
text(0.25, y_limits(2) - 0.1*(y_limits(2)-y_limits(1)), 'Baseline Wander\n(< 0.5 Hz)', ...
     'HorizontalAlignment', 'center', 'Color', 'r', 'FontSize', 11, 'FontWeight', 'bold');