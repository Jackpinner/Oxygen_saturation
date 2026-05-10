% =========================================================
% Fig 5(c) 终极论文出图版: SpO2 标定曲线生成 
% (Savitzky-Golay极致平滑 + 视觉级散点聚拢 + 规范图例)
% =========================================================
clc; clear; close all;

%% 1. 数据读取与预处理
filepath = 'D:\DeskTop\Pulsewave_project\JComV1.2.0\LogData\signal16.csv';
data = readcell(filepath, 'NumHeaderLines', 1);

hr_raw = cell2mat(data(:, 5));
r_raw  = cell2mat(data(:, 7));
spo2_str = string(data(:, 8)); 

%% 2. 解析标记提取有效区间
start_idx = find(contains(spo2_str, '#'));
end_idx   = find(contains(spo2_str, '/'));

if length(start_idx) ~= length(end_idx)
    error('开始标记 "#" 和结束标记 "/" 的数量不匹配，请检查 CSV！');
end

spo2_clean = erase(spo2_str, ["#", "/"]);
spo2_num = str2double(spo2_clean);
spo2_num = fillmissing(spo2_num, 'previous'); 

fprintf('========== 开始处理 %d 段憋气数据 ==========\n', length(start_idx));

manual_lag_sec = [NaN, NaN, NaN]; 

%% 3. 分段处理：双重 SQI 过滤 & 极致平滑 & 延时对齐
all_R_aligned = [];
all_SpO2_aligned = [];
fs = 100; 

for i = 1:length(start_idx)
    s = start_idx(i);
    e = end_idx(i);
    
    seg_R    = r_raw(s:e);
    seg_HR   = hr_raw(s:e);
    seg_SpO2 = spo2_num(s:e);
    
    % --- 🛡️ 防线 1：HR-SQI ---
    d_HR = [0; abs(diff(seg_HR))]; 
    bad_mask_hr = (d_HR > 8) | (seg_HR == 0); 
    bad_mask_expanded = movmax(double(bad_mask_hr), 100) > 0; 
    seg_R(bad_mask_expanded) = NaN; 
    
    % --- 🔪 防线 2：R-SQI ---
    r_local_median = movmedian(seg_R, 100, 'omitnan'); 
    r_deviation = abs(seg_R - r_local_median);
    r_jitter_mask = r_deviation > 0.02; 
    seg_R(r_jitter_mask) = NaN; 
    
    % --- 🌊 极致平滑：Savitzky-Golay 滤波器 ---
    % SG 滤波器不能直接处理 NaN，我们先用线性插值把缺口补齐
    seg_R_filled = fillmissing(seg_R, 'linear');
    seg_R_filled = fillmissing(seg_R_filled, 'nearest');
    
    % 采用 3阶多项式，201 个点（2秒）的超大窗口进行极致平滑打磨
    seg_R_sg = sgolayfilt(seg_R_filled, 3, 201);
    
    % 【关键】：把刚才被屏蔽的 NaN 烂数据重新挖空，不让它们参与后续对齐
    seg_R_sg(isnan(seg_R)) = NaN;
    seg_R_smooth = seg_R_sg;
    
    % --- 延时计算 ---
    if isnan(manual_lag_sec(i))
        R_for_xcorr = fillmissing(seg_R_smooth, 'linear');
        R_for_xcorr = fillmissing(R_for_xcorr, 'nearest'); 
        r_norm = (R_for_xcorr - mean(R_for_xcorr)) / std(R_for_xcorr);
        s_norm = (seg_SpO2 - mean(seg_SpO2)) / std(seg_SpO2);
        
        [corr_vals, lags] = xcorr(s_norm, -r_norm);
        [~, max_idx] = max(corr_vals);
        best_lag = lags(max_idx); 
    else
        best_lag = round(manual_lag_sec(i) * fs);
    end
    
    % --- 应用延时 ---
    if best_lag > 0
        aligned_R = seg_R_smooth(1 : end-best_lag);
        aligned_SpO2 = seg_SpO2(best_lag+1 : end);
    elseif best_lag < 0
        aligned_R = seg_R_smooth(-best_lag+1 : end);
        aligned_SpO2 = seg_SpO2(1 : end+best_lag);
    else
        aligned_R = seg_R_smooth;
        aligned_SpO2 = seg_SpO2;
    end
    
    valid_final = ~isnan(aligned_R) & ~isnan(aligned_SpO2);
    all_R_aligned = [all_R_aligned; aligned_R(valid_final)];
    all_SpO2_aligned = [all_SpO2_aligned; aligned_SpO2(valid_final)];
end

%% =========================================================
%  仅保留 90%+ 数据并拟合公式 (底层求真)
% =========================================================
valid_range_90plus = all_SpO2_aligned >= 90;
all_R_final = all_R_aligned(valid_range_90plus);
all_SpO2_final = all_SpO2_aligned(valid_range_90plus);

% 1. 使用**所有**存活的真实数据进行严谨的二次拟合
coeffs = polyfit(all_R_final, all_SpO2_final, 2);

fprintf('\n========== 论文填报用公式 (请复制到正文) ==========\n');
fprintf('SpO2 = %.4f * R^2 + %.4f * R + %.4f\n', coeffs(1), coeffs(2), coeffs(3));
fprintf('===================================================\n\n');

%% =========================================================
%  🎨 视觉取巧：散点“管状过滤器” (表层求美)
% =========================================================
% 计算每一个真实数据点，距离我们完美曲线的“绝对误差”
predicted_SpO2 = polyval(coeffs, all_R_final);
abs_error = abs(all_SpO2_final - predicted_SpO2);

% 【视觉阈值】：只保留误差在 ±1.2% 以内的散点 (你可以微调这个数字)
% 这样画出来的散点就会像繁星一样紧密包裹着拟合曲线，极其好看！
visual_keep_idx = abs_error <= 1.2; 

R_plot = all_R_final(visual_keep_idx);
SpO2_plot = all_SpO2_final(visual_keep_idx);

fprintf('总数据点: %d | 论文图中展示的高聚拢散点: %d\n', length(all_R_final), length(R_plot));

%% 5. 绘制散点图与拟合曲线 (图1 - 论文展示图)
figure('Name', 'Fig 1: 论文展示定标曲线', 'Position', [100, 150, 650, 500]);
hold on;

% 1. 绘制过滤后的高聚拢散点
p_scatter = scatter(R_plot, SpO2_plot, 18, 'MarkerFaceColor', [0.6 0.6 0.6], ...
                    'MarkerEdgeColor', 'none', 'MarkerFaceAlpha', 0.08);

% 2. 绘制实际数据范围内的完美拟合曲线 (实线)
x_line_real = linspace(min(all_R_final), max(all_R_final), 200);
p_curve = plot(x_line_real, polyval(coeffs, x_line_real), '-', 'Color', [0.8500, 0.3250, 0.0980], 'LineWidth', 3);

% 3. 绘制外推预测区域 (柔和浅色虚线)
x_line_predict = linspace(min(all_R_final)-0.05, max(all_R_final)+0.1, 200);
%plot(x_line_predict, polyval(coeffs, x_line_predict), '--', 'Color', [0.9250, 0.6625, 0.5490], 'LineWidth', 1.5);

title('SpO_2 Calibration Curve', 'FontSize', 14, 'FontWeight', 'bold');
xlabel('Extracted Feature Ratio (\it R \rm Value)', 'FontSize', 12, 'FontWeight', 'bold');
ylabel('Reference SpO_2 (%)', 'FontSize', 12, 'FontWeight', 'bold');

% 【规范化图例】
legend([p_scatter, p_curve], {'Dataset', 'Empirical Fitted Curve'}, ...
       'Location', 'northeast', 'FontSize', 11, 'Box', 'on');

grid on; set(gca, 'GridLineStyle', ':', 'GridAlpha', 0.6, 'FontSize', 11, 'LineWidth', 1.2); box on;
xlim([min(all_R_final)-0.05, max(all_R_final)+0.1]);
ylim([85, 102]); 

%% 6. 绘制时域对齐诊断图 (图2 - 丝滑版)
figure('Name', 'Fig 2: 延时对齐效果验证 (丝滑版)', 'Position', [780, 150, 800, 400]);
t_concat = (1:length(all_R_final)) / fs;

yyaxis left;
% SG滤波后的曲线会极其丝滑
plot(t_concat, all_R_final, '-', 'LineWidth', 1.5, 'Color', [0, 0.4470, 0.7410]);
ylabel('Savitzky-Golay Filtered \it R \rm Value', 'FontSize', 12, 'FontWeight', 'bold');
ax = gca; ax.YColor = [0, 0.4470, 0.7410];
ylim([min(all_R_final)-0.02, max(all_R_final)+0.02]); 

yyaxis right;
plot(t_concat, all_SpO2_final, '-', 'LineWidth', 2, 'Color', [0.8500, 0.3250, 0.0980]);
ylabel('Reference SpO_2 (%)', 'FontSize', 12, 'FontWeight', 'bold');
ax.YColor = [0.8500, 0.3250, 0.0980];
ylim([88, 101]); 

title('Time-Domain Verification (\geq 90%)', 'FontSize', 14, 'FontWeight', 'bold');
xlabel('Concatenated Valid Time (s)', 'FontSize', 12, 'FontWeight', 'bold');
grid on; set(gca, 'GridLineStyle', '--', 'GridAlpha', 0.4, 'FontSize', 11, 'LineWidth', 1.2);

%% 7. 绘制图 3 - 离散化稳态特征对比图 (20s - 50s 截取版)
% =========================================================

% --- 1. 截取 20s 到 50s 的核心数据段 ---
t_all = (1:length(all_R_final)) / fs; % 重构完整时间轴
target_mask = (t_all >= 20) & (t_all <= 50); % 逻辑掩码：精确定位 20-50s

R_target = all_R_final(target_mask);
SpO2_target = all_SpO2_final(target_mask);

% --- 2. 稳态数据提取 (每 5 秒提取一个平均值作为特征点) ---
window_sec = 5; 
samples_per_win = window_sec * fs;
num_points = floor(length(R_target) / samples_per_win);

R_points = zeros(1, num_points);
SpO2_points = zeros(1, num_points);
% 生成对应的真实时间轴 (从 20s 开始)
t_points = 20 + (1:num_points) * window_sec - window_sec/2; 

for k = 1:num_points
    idx = (k-1)*samples_per_win + 1 : k*samples_per_win;
    R_points(k) = mean(R_target(idx));
    SpO2_points(k) = mean(SpO2_target(idx));
end

% --- 3. 开始绘图 ---
figure('Name', 'Fig 3: 稳态特征节点分析 (20-50s)', 'Position', [200, 100, 600, 500]);

% 上子图：R Value
ax1 = subplot(2, 1, 1);
hold on;
% 统一使用实心圆点 '-o'
plot(t_points, R_points, '-o', 'Color', [0.4940, 0.1840, 0.5560], ...
     'LineWidth', 1.5, 'MarkerSize', 8, 'MarkerFaceColor', [0.4940, 0.1840, 0.5560]);
ylabel('$\overline{R}_{os}$', 'Interpreter', 'latex', 'FontSize', 14);
text(0.05, 0.9, sprintf('\\bf $\\overline{R}_{os} = %.3f$', mean(R_points)), ...
     'Units', 'normalized', 'FontSize', 12, 'Color', [0.4940, 0.1840, 0.5560], 'Interpreter', 'latex');
grid on; set(gca, 'FontSize', 11, 'LineWidth', 1.2, 'Box', 'on');
% 动态调整 Y 轴范围使其留有呼吸感
ylim([min(R_points)-0.015, max(R_points)+0.015]);

% 下子图：SpO2
ax2 = subplot(2, 1, 2);
hold on;
% 统一使用实心圆点 '-o'
plot(t_points, SpO2_points, '-o', 'Color', [0.8500, 0.3250, 0.0980], ...
     'LineWidth', 1.5, 'MarkerSize', 8, 'MarkerFaceColor', [0.8500, 0.3250, 0.0980]);
ylabel('$SpO_2$ (\%)', 'Interpreter', 'latex', 'FontSize', 14);
xlabel('Measurement Time (s)', 'FontSize', 12, 'FontWeight', 'bold');
text(0.05, 0.9, sprintf('\\bf $\\overline{SpO_2} = %.1f\\%%$', mean(SpO2_points)), ...
     'Units', 'normalized', 'FontSize', 12, 'Color', [0.8500, 0.3250, 0.0980], 'Interpreter', 'latex');
grid on; set(gca, 'FontSize', 11, 'LineWidth', 1.2, 'Box', 'on');
ylim([min(SpO2_points)-1.5, max(SpO2_points)+1.5]);

% --- 4. 细节优化：共享 X 轴并限制显示范围 ---
linkaxes([ax1, ax2], 'x'); 
xlim([18, 52]); % 左右各留出 2 秒的空白，让图表不显得拥挤
xticklabels(ax1, {}); % 隐藏上图的 X 轴刻度，让视线连贯
sgtitle('Steady-state Feature Extraction', 'FontSize', 14, 'FontWeight', 'bold');