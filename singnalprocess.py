import pandas as pd
import matplotlib.pyplot as plt

# 1. 读取数据 (使用方案三解决乱码)
file_path = "D:/DeskTop/Pulsewave_project/JComV1.2.0/LogData/signal1.csv" 
df = pd.read_csv(file_path, encoding_errors='ignore')
df.columns = ['time', 'IR_AC', 'Red_AC', 'Marker', 'R_value']

# 2. 提取下位机 (MCU) 在线跑出来的结果（为了做 1:1 对齐验证）
mcu_peaks = df[df['Marker'] > 1000].index.tolist()
mcu_valleys = df[df['Marker'] < -1000].index.tolist()

# 3. 准备离线运行 Python 版本的 C 算法
ir_ac_data = df['IR_AC'].values
red_ac_data = df['Red_AC'].values

# 初始化状态机变量 (严格与 C 语言 static 变量一致)
ir_max = -9999.0
ir_min = 9999.0
red_max = -9999.0
red_min = 9999.0

state = 1
ir_amplitude = 100.0
last_peak_tick = 0
rise_value = 0.25
fall_value = 0.25

python_peaks = []
python_valleys = []

# 开始遍历数据，模拟单片机每 10ms 进来一个点的情况
for time_tick in range(len(ir_ac_data)):
    ir_ac = ir_ac_data[time_tick]
    red_ac = red_ac_data[time_tick]
    
    # C 语言里 time_tick 是在判断前 ++ 的，所以这里加 1
    current_tick = time_tick + 1 
    
    if ir_ac > ir_max: ir_max = ir_ac
    if ir_ac < ir_min: ir_min = ir_ac
    if red_ac > red_max: red_max = red_ac
    if red_ac < red_min: red_min = red_ac
    
    if state == 1:
        # 波峰检测逻辑
        if (ir_ac < ir_max - ir_amplitude * fall_value) and (current_tick - last_peak_tick > 35):
            python_peaks.append(time_tick) # 记录 Python 算出的波峰索引
            state = 0
            
            ir_min = ir_ac
            red_min = red_ac
            last_peak_tick = current_tick
            
    else:
        # 波谷检测逻辑
        if (ir_ac > ir_min + ir_amplitude * rise_value) and (current_tick - last_peak_tick > 17):
            python_valleys.append(time_tick) # 记录 Python 算出的波谷索引
            state = 1
            
            ir_ac_pp = ir_max - ir_min
            red_ac_pp = red_max - red_min
            
            ir_amplitude = ir_ac_pp
            
            # 防崩溃限幅
            if ir_amplitude < 20.0: ir_amplitude = 100.0
            if ir_amplitude > 5000.0: ir_amplitude = 5000.0
            
            ir_max = ir_ac
            red_max = red_ac

# 4. 绘图对比
fig, ax1 = plt.subplots(figsize=(15, 6))

# 绘制红外和红光的波形
ax1.plot(df.index, df['IR_AC'], label='IR AC (940nm)', color='royalblue', alpha=0.8)
ax1.plot(df.index, df['Red_AC'], label='Red AC (660nm)', color='crimson', alpha=0.5)

# 画出 MCU 原本的标记 (作为底层参照，使用大且半透明的灰色方块)
ax1.scatter(mcu_peaks, df.loc[mcu_peaks, 'IR_AC'], color='gray', marker='s', s=180, alpha=0.6, label='MCU Online Peak')
ax1.scatter(mcu_valleys, df.loc[mcu_valleys, 'IR_AC'], color='gray', marker='s', s=180, alpha=0.6, label='MCU Online Valley')

# 画出 Python 离线算法跑出来的标记 (使用亮色的三角形)
ax1.scatter(python_peaks, df.loc[python_peaks, 'IR_AC'], color='lime', marker='^', s=100, label='Python Offline Peak', zorder=4, edgecolors='black')
ax1.scatter(python_valleys, df.loc[python_valleys, 'IR_AC'], color='magenta', marker='v', s=100, label='Python Offline Valley', zorder=4, edgecolors='black')

ax1.set_title('PPG Offline Algorithm Validation (Python vs MCU)')
ax1.set_xlabel('Sample Index (10ms/point)')
ax1.set_ylabel('Amplitude')
ax1.legend(loc='upper right')
ax1.grid(True, linestyle='--', alpha=0.6)

plt.tight_layout()
plt.show()