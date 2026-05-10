% =========================================================
% Fig 5(b) 终极图纸级: 血氧与心率特征点提取原理图
% 包含：AC幅度、峰峰间距(HR)、零基线、重搏波切迹
% =========================================================
clc; clear; close all;

%% 1. 数据读取与多周期截取
filepath = 'D:\DeskTop\Pulsewave_project\JComV1.2.0\LogData\signal12.csv';
data = readtable(filepath);

ir_filter  = data{:, 2};  
red_filter = data{:, 3};  
fs = 100; 
t = (0:length(ir_filter)-1) / fs; 

start_time = 151.25; 
end_time = 153.05;   
idx_start = round(start_time * fs);
idx_end   = round(end_time * fs);

t_cycle   = t(idx_start:idx_end);
ir_cycle  = ir_filter(idx_start:idx_end);
red_cycle = red_filter(idx_start:idx_end);

offset_red = -200; 

%% 2. 自动寻找周期内的【前两个】波峰和波谷
[ir_pks, ir_locs] = findpeaks(ir_cycle, 'MinPeakDistance', 0.5*fs);
t_ir_peak1 = t_cycle(ir_locs(1)); val_ir_peak1 = ir_pks(1);
t_ir_peak2 = t_cycle(ir_locs(2)); val_ir_peak2 = ir_pks(2);

[ir_valley_val, v_idx] = min(ir_cycle(ir_locs(1):ir_locs(2)));
t_ir_valley = t_cycle(ir_locs(1) + v_idx - 1);

[red_pks, red_locs] = findpeaks(red_cycle, 'MinPeakDistance', 0.5*fs);
t_red_peak1 = t_cycle(red_locs(1)); val_red_peak1 = red_pks(1) + offset_red;
[red_valley_val, rv_idx] = min(red_cycle(red_locs(1):red_locs(2)));
t_red_valley = t_cycle(red_locs(1) + rv_idx - 1);
val_red_valley = red_valley_val + offset_red;

%% 3. 开始绘制“图纸级”原理图
figure('Name', '血氧与心率综合特征提取', 'Position', [100, 100, 750, 550]);
hold on;

% 【特征 1：零刻度基线 (Zero Baseline)】
yline(0, 'k-.', 'LineWidth', 1.5, 'Color', [0.6 0.6 0.6]);
text(t_cycle(end), 50, 'Zero Baseline (IR)', 'FontSize', 11, 'Color', [0.4 0.4 0.4], 'HorizontalAlignment', 'right');

% 绘制波形主体
p1 = plot(t_cycle, ir_cycle, '-', 'Color', [0, 0.4470, 0.7410], 'LineWidth', 2.5);
p2 = plot(t_cycle, red_cycle + offset_red, '-', 'Color', [0.8500, 0.3250, 0.0980], 'LineWidth', 2.5);

% 打上关键点 
plot(t_ir_peak1, val_ir_peak1, 'o', 'MarkerFaceColor', [0, 0.4470, 0.7410], 'MarkerEdgeColor', 'w', 'MarkerSize', 8);
plot(t_ir_valley, ir_valley_val, 'o', 'MarkerFaceColor', 'w', 'MarkerEdgeColor', [0, 0.4470, 0.7410], 'MarkerSize', 8, 'LineWidth', 1.5);
plot(t_red_peak1, val_red_peak1, 'o', 'MarkerFaceColor', [0.8500, 0.3250, 0.0980], 'MarkerEdgeColor', 'w', 'MarkerSize', 8);
plot(t_red_valley, val_red_valley, 'o', 'MarkerFaceColor', 'w', 'MarkerEdgeColor', [0.8500, 0.3250, 0.0980], 'MarkerSize', 8, 'LineWidth', 1.5);
plot(t_ir_peak2, val_ir_peak2, 'o', 'MarkerFaceColor', [0, 0.4470, 0.7410], 'MarkerEdgeColor', 'w', 'MarkerSize', 8);

%% 4. 添加高逼格的几何标注 (CAD风格)
% --- (A) 标注 AC_IR (左侧) ---
plot([t_cycle(1), t_ir_peak1], [val_ir_peak1, val_ir_peak1], 'k--', 'Color', [0.5 0.5 0.5]);
plot([t_cycle(1), t_ir_valley], [ir_valley_val, ir_valley_val], 'k--', 'Color', [0.5 0.5 0.5]);
x_ir_arrow = t_cycle(1) + 0.04; 
plot([x_ir_arrow, x_ir_arrow], [ir_valley_val, val_ir_peak1], 'k-', 'LineWidth', 1.5);
plot(x_ir_arrow, val_ir_peak1, 'kv', 'MarkerFaceColor', 'k'); 
plot(x_ir_arrow, ir_valley_val, 'k^', 'MarkerFaceColor', 'k'); 
text(x_ir_arrow + 0.015, (val_ir_peak1 + ir_valley_val)/2, '\boldmath$AC_{IR}$', 'Interpreter', 'latex', 'FontSize', 14, 'Color', [0, 0.4470, 0.7410]);

% --- (B) 标注 AC_Red (右侧) ---
plot([t_red_peak1, t_cycle(end)], [val_red_peak1, val_red_peak1], 'k--', 'Color', [0.5 0.5 0.5]);
plot([t_red_valley, t_cycle(end)], [val_red_valley, val_red_valley], 'k--', 'Color', [0.5 0.5 0.5]);
x_red_arrow = t_cycle(end) - 0.04;
plot([x_red_arrow, x_red_arrow], [val_red_valley, val_red_peak1], 'k-', 'LineWidth', 1.5);
plot(x_red_arrow, val_red_peak1, 'kv', 'MarkerFaceColor', 'k'); 
plot(x_red_arrow, val_red_valley, 'k^', 'MarkerFaceColor', 'k'); 
text(x_red_arrow - 0.015, (val_red_peak1 + val_red_valley)/2, '\boldmath$AC_{Red}$', 'Interpreter', 'latex', 'FontSize', 14, 'Color', [0.8500, 0.3250, 0.0980], 'HorizontalAlignment', 'right');

% --- 【特征 2：标注峰峰间距 Delta T (心率 HR)】 ---
% 增大了 Y 轴上的偏移量，防止箭头和文字重合 (Y轴跨度是几千，所以要加 100~200)
y_interval = max(val_ir_peak1, val_ir_peak2) + 100; 
plot([t_ir_peak1, t_ir_peak2], [y_interval, y_interval], 'k-', 'LineWidth', 1.5);
plot([t_ir_peak1, t_ir_peak1], [val_ir_peak1, y_interval], 'k:', 'Color', [0.5 0.5 0.5]);
plot([t_ir_peak2, t_ir_peak2], [val_ir_peak2, y_interval], 'k:', 'Color', [0.5 0.5 0.5]);
plot(t_ir_peak1, y_interval, 'k<', 'MarkerFaceColor', 'k');
plot(t_ir_peak2, y_interval, 'k>', 'MarkerFaceColor', 'k');
% 将文字再往上抬高 120 的绝对数值
text((t_ir_peak1+t_ir_peak2)/2, y_interval + 120, '\boldmath$\Delta T \ (Pulse \ Interval)$', 'Interpreter', 'latex', 'FontSize', 14, 'HorizontalAlignment', 'center');

% --- 【特征 3：标注重搏波切迹 (Dicrotic Notch)】 ---
% 根据你的截图精准调整了：小鼓包大概在 peak1 之后 0.23 秒，高度在 Y=100 左右
x_notch = t_ir_peak1 + 0.23; 
y_notch = 100; 
% 将文字稍微往右挪一点，留出箭头的位置
text(x_notch + 0.02, y_notch, '\leftarrow Dicrotic Notch', 'FontSize', 12, 'FontWeight', 'bold', 'FontAngle', 'italic', 'Color', 'k');


%% 5. 图表美化与收尾 
title('Feature Extraction for SpO_2 and Heart Rate Calculation', 'FontSize', 14, 'FontWeight', 'bold');
xlabel('Time (s)', 'FontSize', 12, 'FontWeight', 'bold');
ylabel('Amplitude (a.u.)', 'FontSize', 12, 'FontWeight', 'bold');

p_ir_peak   = plot(NaN, NaN, 'o', 'MarkerFaceColor', [0, 0.4470, 0.7410], 'MarkerEdgeColor', 'w', 'MarkerSize', 8);
p_ir_valley = plot(NaN, NaN, 'o', 'MarkerFaceColor', 'w', 'MarkerEdgeColor', [0, 0.4470, 0.7410], 'MarkerSize', 8, 'LineWidth', 1.5);
p_red_peak   = plot(NaN, NaN, 'o', 'MarkerFaceColor', [0.8500, 0.3250, 0.0980], 'MarkerEdgeColor', 'w', 'MarkerSize', 8);
p_red_valley = plot(NaN, NaN, 'o', 'MarkerFaceColor', 'w', 'MarkerEdgeColor', [0.8500, 0.3250, 0.0980], 'MarkerSize', 8, 'LineWidth', 1.5);

legend_handles = [p1, p_ir_peak, p_ir_valley, p2, p_red_peak, p_red_valley];
legend_labels  = {'IR Signal', 'IR Peak', 'IR Valley', 'Red Signal', 'Red Peak', 'Red Valley'};
legend(legend_handles, legend_labels, 'Location', 'northeast', 'NumColumns', 2, 'FontSize', 10);

box off;
set(gca, 'FontSize', 11, 'LineWidth', 1.2);
xlim([t_cycle(1), t_cycle(end) + 0.05]);
% 顶部多留一点空间，防止文字被边缘切掉
ylim([min(red_cycle)+offset_red-100, y_interval+350]);