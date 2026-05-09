% =========================================================
% 基于VCSEL的健康监测系统 - PPG波形对比绘图脚本 (改进版)
% =========================================================
clc; clear; close all;

% 1. 设置CSV文件的绝对路径
filepath = 'D:\DeskTop\Pulsewave_project\JComV1.2.0\LogData\signal12.csv';

% 2. 读取数据 (使用最基础的读取方式即可)
data = readtable(filepath);

% 3. 按列号提取数据 (彻底规避表头特殊字符导致的报错)
% 使用大括号 {} 直接提取该列的数值数组
ir_filter   = data{:, 2};  % 第2列：滤波后的交流信号
signal_sign = data{:, 4};  % 第4列：标记针数据 (2500波峰, -2500波谷)
ir_raw      = data{:, 5};  % 第5列：原始未滤波信号

% 4. 生成时间轴 (根据中断设定，采样率 100Hz)
fs = 500; 
t = (0:length(ir_filter)-1) / fs;

% 5. 提取算法识别出的波峰位置，用于图3D打标
peak_indices = find(signal_sign == 2500); 
peak_times = t(peak_indices);
peak_values = ir_filter(peak_indices);

% ===================== 绘制图 3B：原始 PPG 波形 =====================
figure('Name', '图3B：原始PPG波形', 'Position', [100, 500, 800, 300]); 

plot(t, ir_raw, 'Color', [0.4, 0.4, 0.4], 'LineWidth', 1.2); 
title('原始红外(IR) PPG信号 (Raw Signal)', 'FontSize', 14, 'FontWeight', 'bold');
xlabel('时间 / Time (s)', 'FontSize', 12);
ylabel('信号幅度 / Amplitude (a.u.)', 'FontSize', 12);
grid on;
xlim([min(t), max(t)]); 
set(gca, 'FontSize', 11, 'LineWidth', 1);

% ===================== 绘制图 3D：滤波与寻峰后 PPG 波形 =====================
figure('Name', '图3D：滤波与寻峰后PPG波形', 'Position', [100, 100, 800, 300]);

plot(t, ir_filter, 'Color', [0, 0.4470, 0.7410], 'LineWidth', 1.5); 
hold on;
%plot(peak_times, peak_values, 'ro', 'MarkerFaceColor', 'r', 'MarkerSize', 5); 

title('滤波后的交流波形 (Filtered & Peaks)', 'FontSize', 14, 'FontWeight', 'bold');
xlabel('时间 / Time (s)', 'FontSize', 12);
ylabel('交流幅度 / AC Amplitude', 'FontSize', 12);
%legend('滤波后IR波形', '算法检测波峰', 'Location', 'northeast');
grid on;
xlim([min(t), max(t)]);
set(gca, 'FontSize', 11, 'LineWidth', 1);