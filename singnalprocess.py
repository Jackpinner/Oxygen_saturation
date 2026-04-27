import pandas as pd
import matplotlib.pyplot as plt

# =================================================================
# 1. 算法封装 (1:1 翻译你的 Algorithm.c)
# =================================================================

class PPG_Algorithm:
    def __init__(self):
        # 滤波器状态
        self.ir_dc_baseline = 0.0
        self.ir_ac_filtered = 0.0
        self.red_dc_baseline = 0.0
        self.red_ac_filtered = 0.0
        self.is_filter_init = False

        # Track_Pulse_Wave_Dual 状态机变量
        self.ir_max = -9999.0
        self.ir_min = 9999.0
        self.red_max = -9999.0
        self.red_min = 9999.0
        self.state = 1
        self.ir_amplitude = 100.0
        self.last_peak_tick = 0
        self.rise_value = 0.30
        self.fall_value = 0.30

    def IIR_Bandpass_Filter(self, ir_raw, red_raw):
        if not self.is_filter_init:
            self.ir_dc_baseline = ir_raw
            self.red_dc_baseline = red_raw
            self.is_filter_init = True
            return 0.0, 0.0, ir_raw, red_raw

        # 红外通道
        self.ir_dc_baseline = 0.9876 * self.ir_dc_baseline + 0.0124 * ir_raw
        ir_pure_ac = ir_raw - self.ir_dc_baseline
        self.ir_ac_filtered = 0.7610 * self.ir_ac_filtered + 0.2390 * ir_pure_ac

        # 红光通道
        self.red_dc_baseline = 0.9876 * self.red_dc_baseline + 0.0124 * red_raw
        red_pure_ac = red_raw - self.red_dc_baseline
        self.red_ac_filtered = 0.7610 * self.red_ac_filtered + 0.2390 * red_pure_ac

        return self.ir_ac_filtered, self.red_ac_filtered, self.ir_dc_baseline, self.red_dc_baseline

    def Track_Pulse_Wave_Dual(self, ir_ac, red_ac, ir_dc, red_dc, current_tick):
        is_peak = False
        is_valley = False
        
        # 更新最大最小值
        if ir_ac > self.ir_max: self.ir_max = ir_ac
        if ir_ac < self.ir_min: self.ir_min = ir_ac
        
        if self.state == 1: # 寻找波峰
            if (ir_ac < self.ir_max - self.ir_amplitude * self.fall_value) and (current_tick - self.last_peak_tick > 35):
                is_peak = True
                self.state = 0
                self.ir_min = ir_ac
                self.last_peak_tick = current_tick
        else: # 寻找波谷
            if (ir_ac > self.ir_min + self.ir_amplitude * self.rise_value) and (current_tick - self.last_peak_tick > 25):
                is_valley = True
                self.state = 1
                self.ir_amplitude = self.ir_max - self.ir_min
                # 修改后：
                if self.ir_amplitude < 300.0: 
                    self.ir_amplitude = 800.0  # 恢复到一个比较典型的正常振幅，而不是极小值
                if self.ir_amplitude > 5000.0: 
                    self.ir_amplitude = 5000.0
                self.ir_max = ir_ac
                
        return is_peak, is_valley

# =================================================================
# 2. 数据读取与处理
# =================================================================

file_path = "D:/DeskTop/Pulsewave_project/JComV1.2.0/LogData/signal4.csv"
df = pd.read_csv(file_path, encoding_errors='ignore')
df.columns = ['time', 'MCU_IR_AC', 'MCU_Red_AC', 'Marker', 'IR_raw', 'Red_raw', 'MCU_IR_DC']

algo = PPG_Algorithm()
py_results = []

for i in range(len(df)):
    # 模拟单片机 10ms 的一次处理循环
    ir_raw = df['IR_raw'].iloc[i]
    red_raw = df['Red_raw'].iloc[i]
    
    # 第一步：滤波
    ir_ac, red_ac, ir_dc, red_dc = algo.IIR_Bandpass_Filter(ir_raw, red_raw)
    
    # 第二步：波形追踪 (状态机)
    is_peak, is_valley = algo.Track_Pulse_Wave_Dual(ir_ac, red_ac, ir_dc, red_dc, i + 1)
    
    py_results.append({
        'ir_ac': ir_ac,
        'is_peak': is_peak,
        'is_valley': is_valley
    })

# 转化为 DataFrame 方便画图
py_df = pd.DataFrame(py_results)

# =================================================================
# 3. 对齐验证绘图
# =================================================================

plt.figure(figsize=(15, 6))
# 绘制 Python 滤波后的 IR AC 波形
plt.plot(df.index, py_df['ir_ac'], label='Python IR AC (Filtered)', color='royalblue', alpha=0.6)

# --- 标记 Python 离线算出的峰谷 (实心亮色图形) ---
py_peaks = py_df[py_df['is_peak']].index
py_valleys = py_df[py_df['is_valley']].index
plt.scatter(py_peaks, py_df.loc[py_peaks, 'ir_ac'], color='lime', marker='^', s=100, label='Python Peak', zorder=4, edgecolors='black')
plt.scatter(py_valleys, py_df.loc[py_valleys, 'ir_ac'], color='magenta', marker='v', s=100, label='Python Valley', zorder=4, edgecolors='black')

# --- 标记 MCU 原始的打针标记 (空心大图形，作为底层参考) ---
mcu_peaks = df[df['Marker'] > 1000].index
mcu_valleys = df[df['Marker'] < -1000].index
# MCU 波峰用空心黑圆圈
plt.scatter(mcu_peaks, py_df.loc[mcu_peaks, 'ir_ac'], facecolors='none', edgecolors='black', s=180, linewidths=2, label='MCU Peak (Online)', zorder=3)
# MCU 波谷用空心红方块
plt.scatter(mcu_valleys, py_df.loc[mcu_valleys, 'ir_ac'], facecolors='none', edgecolors='red', marker='s', s=180, linewidths=2, label='MCU Valley (Online)', zorder=3)

plt.title("Full Algorithm Alignment: Filter + State Machine")
plt.xlabel("Sample Index (10ms/point)")
plt.ylabel("Amplitude")
plt.legend(loc='upper right')
plt.grid(True, linestyle='--', alpha=0.6)
plt.tight_layout()
plt.show()