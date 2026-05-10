% =========================================================
% Fig 5(a): 双波长时域同步性验证 (无归一化全画幅)
% 视觉策略：粗实线打底 + 细虚线叠加，展现完美的零相位延迟
% =========================================================
clc; clear; close all;

%% 1. 数据读取与预处理
% 设置CSV文件的绝对路径
filepath = 'D:\DeskTop\Pulsewave_project\JComV1.2.0\LogData\signal12.csv';
data = readtable(filepath);

% 提取双路滤波后的交流信号 (AC Component)
ir_filter  = data{:, 2};  % 第2列: IR 交流信号
red_filter = data{:, 3};  % 第3列: RED 交流信号

% 生成时间轴
fs = 100; % 采样率 100Hz
t = (0:length(ir_filter)-1) / fs; 

%% 2. 绘制图表 5(a)
figure('Name', 'Fig 5(a): 双波长信号完整对比', 'Position', [100, 200, 800, 400]);

% --- 核心视觉呈现：粗蓝底 + 细黄虚线 ---

% 1. 绘制底部的 IR 波形 
% 使用较粗的线条 (LineWidth = 3) 和经典的 MATLAB 蓝色
p1 = plot(t, ir_filter, '-', 'Color', [0, 0.4470, 0.7410], 'LineWidth', 3); 
hold on;

% 2. 绘制顶部的 RED 波形 
% 使用细虚线 (LineWidth = 1.5) 和醒目的学术明黄色
% 这样蓝色底会从虚线的空隙中透出来，极其直观地展现高度重合
p2 = plot(t, red_filter-200, '-', 'Color', [0.9300, 0.6940, 0.1250], 'LineWidth', 1.5); 

%% 3. 图表全局美化
title('Dual-Wavelength PPG AC Signals Synchronization', 'FontSize', 14, 'FontWeight', 'bold');
xlabel('Time (s)', 'FontSize', 12, 'FontWeight', 'bold');
ylabel('AC Amplitude (a.u.)', 'FontSize', 12, 'FontWeight', 'bold');

% 图例设置
legend([p1, p2], {'IR Signal', 'Red Signal'}, 'Location', 'northeast');

% 坐标轴与网格设置
grid on;
xlim([min(t), max(t)]); % 显示完整的时间轴
set(gca, 'FontSize', 11, 'LineWidth', 1);

fprintf('图 5(a) 完整代码已运行完毕！\n');
fprintf('提示：你可以使用图窗上的放大镜工具，框选几个连续的心跳周期进行局部放大，截图作为论文插图效果最佳！\n');