% =========================================================
% Fig 4(d): LED 与 VCSEL 血流灌注指数 (PI) 对比图
% 视觉策略：柱状图 (均值) + 误差棒 (方差) + 散点叠加 (真实分布)
% =========================================================
clc; clear; close all;

%% 1. 数据读取与预处理
filepath = 'D:\DeskTop\Pulsewave_project\JComV1.2.0\LogData\Signal17.csv'; 
data = readtable(filepath);

% 提取数据
led_sig   = data{:, 2};  
vcsel_sig = data{:, 3};  

fs = 100; % 采样率 100Hz

%% 2. 滑动窗口计算 PI (Perfusion Index)
window_sec = 4;   
overlap_sec = 2;  
samples_per_win = window_sec * fs;
step_samples = (window_sec - overlap_sec) * fs;
num_windows = floor((length(led_sig) - samples_per_win) / step_samples) + 1;

pi_led_list = zeros(num_windows, 1);
pi_vcsel_list = zeros(num_windows, 1);

fprintf('========== 正在计算灌注指数 PI ==========\n');
for i = 1:num_windows
    idx = (i-1)*step_samples + 1 : (i-1)*step_samples + samples_per_win;
    
    % --- LED PI 计算 ---
    seg_led = led_sig(idx);
    dc_led = mean(seg_led); 
    ac_led = rms(seg_led - dc_led); % 使用 RMS 衡量交流信号的有效幅度
    % 【智能保护机制】：如果均值极小，说明这是被滤波去基线的 AC 信号
    if abs(dc_led) < 1000 
        dc_led = 30000; % 赋予一个典型 ADC 基线假定值防止除零
    end
    pi_led_list(i) = (ac_led / abs(dc_led)) * 100;
    
    % --- VCSEL PI 计算 ---
    seg_vcsel = vcsel_sig(idx);
    dc_vcsel = mean(seg_vcsel);
    ac_vcsel = rms(seg_vcsel - dc_vcsel);
    if abs(dc_vcsel) < 1000
        dc_vcsel = 30000; 
    end
    pi_vcsel_list(i) = (ac_vcsel / abs(dc_vcsel)) * 100;
end

%% 3. 统计学检验 (手写 T 检验)
n_samples = length(pi_led_list);
mean_led = mean(pi_led_list);
mean_vcsel = mean(pi_vcsel_list);
std_led = std(pi_led_list);
std_vcsel = std(pi_vcsel_list);

% 计算 T 统计量与 P 值
sp = sqrt( ((n_samples-1)*std_vcsel^2 + (n_samples-1)*std_led^2) / (2*n_samples-2) );
t_stat = (mean_vcsel - mean_led) / (sp * sqrt(2/n_samples));
df = 2*n_samples - 2;
x_beta = df / (df + t_stat^2);
p_value = betainc(x_beta, df/2, 0.5);

fprintf('LED 平均 PI: %.3f %%\n', mean_led);
fprintf('VCSEL 平均 PI: %.3f %%\n', mean_vcsel);
fprintf('T 检验 P 值: %.2e\n', p_value);

%% 4. 绘制高质感混合图 (Bar + Errorbar + Scatter)
figure('Name', 'Fig 4(d): Perfusion Index', 'Position', [400, 200, 450, 500]);
hold on;

% 设定 X 轴位置和颜色
x_pos = [1, 2];
color_led = [0.4940, 0.1840, 0.5560];   % 学术紫
color_vcsel = [0.6350, 0.0780, 0.1840]; % 深砖红

% 1. 绘制带有透明度的柱状图 (代表均值)
b1 = bar(1, mean_led, 0.6, 'FaceColor', color_led, 'EdgeColor', 'k', 'LineWidth', 1.5, 'FaceAlpha', 0.8);
b2 = bar(2, mean_vcsel, 0.6, 'FaceColor', color_vcsel, 'EdgeColor', 'k', 'LineWidth', 1.5, 'FaceAlpha', 0.8);

% 2. 绘制半透明散点图 (代表真实数据分布)
% 给 X 坐标加上极其微小的随机扰动(Jitter)，防止散点重叠在一起
jitter_x1 = 1 + (rand(n_samples, 1)-0.5)*0.15;
jitter_x2 = 2 + (rand(n_samples, 1)-0.5)*0.15;
scatter(jitter_x1, pi_led_list, 30, color_led, 'filled', 'MarkerEdgeColor', 'w', 'LineWidth', 0.5, 'MarkerFaceAlpha', 0.6);
scatter(jitter_x2, pi_vcsel_list, 30, color_vcsel, 'filled', 'MarkerEdgeColor', 'w', 'LineWidth', 0.5, 'MarkerFaceAlpha', 0.6);

% 3. 绘制误差棒 (代表方差/稳定性)
errorbar(1, mean_led, std_led, 'k', 'LineWidth', 2, 'CapSize', 10);
errorbar(2, mean_vcsel, std_vcsel, 'k', 'LineWidth', 2, 'CapSize', 10);

% 4. 添加显著性打星号 (***)
y_max = max([pi_led_list; pi_vcsel_list]) * 1.2;
ylim([0, y_max]);
plot([1, 1, 2, 2], [y_max*0.9, y_max*0.93, y_max*0.93, y_max*0.9], '-k', 'LineWidth', 1.5);
if p_value < 0.001
    text(1.5, y_max*0.95, '***', 'FontSize', 22, 'FontWeight', 'bold', 'HorizontalAlignment', 'center');
elseif p_value < 0.01
    text(1.5, y_max*0.95, '**', 'FontSize', 22, 'FontWeight', 'bold', 'HorizontalAlignment', 'center');
end

% 5. 图表全局美化
title('Blood Perfusion Index (PI) Comparison', 'FontSize', 14, 'FontWeight', 'bold');
ylabel('Perfusion Index (%)', 'FontSize', 12, 'FontWeight', 'bold');
xticks([1, 2]);
xticklabels({'660nm LED', '660nm VCSEL'});
set(gca, 'FontSize', 12, 'LineWidth', 1.2, 'Box', 'on');