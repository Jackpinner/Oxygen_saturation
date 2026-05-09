import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

# 1. 读取数据并清理列名可能存在的空格
df = pd.read_csv(r"D:\DeskTop\Pulsewave_project\JComV1.2.0\LogData\signal9.csv")
df.columns = df.columns.str.strip()

# 2. 截取有效数据段 (以第一个和最后一个标定的血氧值为界)
first_idx = df['Spo2'].first_valid_index()
last_idx = df['Spo2'].last_valid_index()
df_valid = df.loc[first_idx:last_idx].copy()

# 3. 填充商业血氧仪数据 (向后填充 Forward Fill，形成阶梯数据)
df_valid['Spo2_filled'] = df_valid['Spo2'].ffill()

# 4. 对你的 R 值进行滑动平均平滑 (去除高频毛刺，防止曲线拟合被噪点带偏)
# 窗口大小 200 代表大约 2秒的平滑
df_valid['R_smooth'] = df_valid['R/IR_baseline'].rolling(window=200, center=True).mean()

# 5. 解决时间延迟 (核心逻辑)
# 通过交叉相关性计算，你的商业设备大约有 5.76秒(576个点) 的延迟
# 我们将商业 SpO2 数据"向上/向左"移动 576 行，以对齐真正发生生理变化的 R 值
DELAY_SAMPLES = 600 
df_valid['Spo2_aligned'] = df_valid['Spo2_filled'].shift(-DELAY_SAMPLES)

# 丢弃因为位移产生空缺(NaN)的行
df_fit = df_valid.dropna(subset=['Spo2_aligned', 'R_smooth']).copy()

# 6. 二次多项式拟合: SpO2 = A * R^2 + B * R + C
x = df_fit['R_smooth'].values
y = df_fit['Spo2_aligned'].values

# 使用 numpy.polyfit 算出 A, B, C
coefficients = np.polyfit(x, y, 2)
A, B, C = coefficients

print("========== 标定计算完成 ==========")
print(f"最佳对齐延迟: {DELAY_SAMPLES/100} 秒")
print(f"拟合公式: SpO2 = {A:.4f} * R^2 + {B:.4f} * R + {C:.4f}")
print(f"float current_spo2 = {A:.4f}f * R * R + {B:.4f}f * R + {C:.4f}f;")

# 7. 画图看效果
plt.figure(figsize=(10, 6))
# 画出对齐后的散点图 (透明度调低以便看清密集区域)
plt.scatter(x, y, alpha=0.05, color='gray', label='Aligned Data Points')

# 画出你专属的拟合曲线
x_line = np.linspace(min(x), max(x), 100)
y_line = A * x_line**2 + B * x_line + C
plt.plot(x_line, y_line, color='red', linewidth=2, label='Fitted Curve')

plt.xlabel('R Value (Smoothed)')
plt.ylabel('Commercial SpO2 (%)')
plt.title(f'SpO2 Calibration Curve (Lag Compensated: {DELAY_SAMPLES/100}s)')
plt.legend()
plt.grid(True)
plt.show()