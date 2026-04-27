import pandas as pd
import matplotlib.pyplot as plt

# =================================================================
# 1. 算法封装 (1:1 翻译你的 Algorithm.c，加入了时间戳回溯法)
# =================================================================

class PPG_Algorithm:
    def __init__(self):
        # ==================================
        # 1. 升级版二阶级联滤波器状态变量
        # ==================================
        self.ir_dc1 = 0.0
        self.ir_dc2 = 0.0
        self.red_dc1 = 0.0
        self.red_dc2 = 0.0
        
        self.ir_ac_filtered = 0.0
        self.red_ac_filtered = 0.0
        self.is_filter_init = False

        # ==================================
        # 2. 状态机与寻峰相关变量 (补回这部分)
        # ==================================
        self.ir_max = -9999.0
        self.ir_min = 9999.0
        self.red_max = -9999.0
        self.red_min = 9999.0
        self.state = 1
        self.ir_amplitude = 100.0
        self.last_peak_tick = 0
        
        # 之前优化抗干扰的参数 (盲区防重搏波)
        self.rise_value = 0.55 
        self.fall_value = 0.40  
        
        # 时间戳回溯变量
        self.ir_max_tick = 0
        self.ir_min_tick = 0

    def IIR_Bandpass_Filter(self, ir_raw, red_raw):
        if not self.is_filter_init:
            # 冷启动初始化
            self.ir_dc1 = ir_raw
            self.ir_dc2 = 0.0
            self.red_dc1 = red_raw
            self.red_dc2 = 0.0
            self.is_filter_init = True
            return 0.0, 0.0, ir_raw, red_raw

        # ==================== 红外通道双重滤波 ====================
        self.ir_dc1 = 0.98 * self.ir_dc1 + 0.02 * ir_raw
        ir_ac1 = ir_raw - self.ir_dc1
        
        self.ir_dc2 = 0.98 * self.ir_dc2 + 0.02 * ir_ac1
        ir_pure_ac = ir_ac1 - self.ir_dc2
        
        self.ir_ac_filtered = 0.7610 * self.ir_ac_filtered + 0.2390 * ir_pure_ac

        # ==================== 红光通道双重滤波 ====================
        self.red_dc1 = 0.98 * self.red_dc1 + 0.02 * red_raw
        red_ac1 = red_raw - self.red_dc1
        
        self.red_dc2 = 0.98 * self.red_dc2 + 0.02 * red_ac1
        red_pure_ac = red_ac1 - self.red_dc2
        
        self.red_ac_filtered = 0.7610 * self.red_ac_filtered + 0.2390 * red_pure_ac

        return self.ir_ac_filtered, self.red_ac_filtered, self.ir_dc1, self.red_dc1

   
    # =================================================================
# 优化后的 Track_Pulse_Wave_Dual (针对“降中峡”陷阱优化)
# =================================================================

    def Track_Pulse_Wave_Dual(self, ir_ac, red_ac, ir_dc, red_dc, current_tick):
        actual_peak_tick = None
        actual_valley_tick = None
        
        # 1. 极值实时追踪
        if ir_ac > self.ir_max: 
            self.ir_max = ir_ac
            self.ir_max_tick = current_tick
            
        if ir_ac < self.ir_min: 
            self.ir_min = ir_ac
            self.ir_min_tick = current_tick
        
        # 2. 状态机逻辑
        if self.state == 1: # 状态 1：寻找波峰
            # 确认波峰条件：跌落足够深且超过死区时间
            if (ir_ac < self.ir_max - self.ir_amplitude * self.fall_value) and (current_tick - self.last_peak_tick > 35):
                actual_peak_tick = self.ir_max_tick 
                
                self.state = 0 # 切换到寻找波谷
                self.ir_min = ir_ac # 重置最小值寻找基准
                self.ir_min_tick = current_tick
                self.last_peak_tick = self.ir_max_tick 
                
        else: # 状态 0：寻找波谷
            # --- 关键修改：增加上升确认的严苛度 ---
            # 1. 提高上升门槛 (rise_value 建议设为 0.5 以上)
            # 2. 确保距离上一个波峰至少过去了 450ms (跨过重搏波的典型时间)
            
            is_rise_enough = (ir_ac > self.ir_min + self.ir_amplitude * self.rise_value)
            is_time_enough = (current_tick - self.last_peak_tick > 50) # 450ms 盲区，专门躲避重搏波
            
            if is_rise_enough and is_time_enough:
                actual_valley_tick = self.ir_min_tick
                
                self.state = 1 # 切换回寻找波峰
                
                # 计算最新的振幅，用于下一个周期的判定
                temp_amplitude = self.ir_max - self.ir_min
                self.ir_amplitude = temp_amplitude
                
                # 防坍塌保护
                if self.ir_amplitude < 300.0: self.ir_amplitude = 800.0
                if self.ir_amplitude > 5000.0: self.ir_amplitude = 5000.0
                
                self.ir_max = ir_ac
                self.ir_max_tick = current_tick
                
        return actual_peak_tick, actual_valley_tick

# =================================================================
# 2. 数据读取与处理
# =================================================================

# 请确认这里换成你的实际路径
file_path = "D:\DeskTop\Pulsewave_project\JComV1.2.0\LogData\signal6.csv"
df = pd.read_csv(file_path, encoding_errors='ignore')

# 匹配新的 CSV 列名
df.columns = ['time', 'MCU_IR_AC', 'MCU_Red_AC', 'Marker', 'IR_raw', 'Red_raw', 'MCU_IR_DC']

algo = PPG_Algorithm()

# 存储整个波形和标定的索引
py_ir_ac_list = []
py_peaks = []
py_valleys = []

for i in range(len(df)):
    ir_raw = df['IR_raw'].iloc[i]
    red_raw = df['Red_raw'].iloc[i]
    
    # 1. 运行滤波
    ir_ac, red_ac, ir_dc, red_dc = algo.IIR_Bandpass_Filter(ir_raw, red_raw)
    py_ir_ac_list.append(ir_ac)
    
    # 2. 运行状态机，传入 i 作为当前时间戳(tick)
    peak_tick, valley_tick = algo.Track_Pulse_Wave_Dual(ir_ac, red_ac, ir_dc, red_dc, i)
    
    # 3. 收集标记点 (如果有返回值，直接存入列表)
    if peak_tick is not None:
        py_peaks.append(peak_tick)
    if valley_tick is not None:
        py_valleys.append(valley_tick)

# =================================================================
# 3. 对齐验证绘图
# =================================================================

plt.figure(figsize=(16, 6))

# 绘制 Python 滤波后的 IR AC 波形
plt.plot(df.index, py_ir_ac_list, label='Python IR AC (Filtered)', color='royalblue', alpha=0.6)

# --- 标记 Python 离线算出的精准峰谷 (亮色实心图形) ---
# 注意：现在的 py_peaks 直接就是 X 轴坐标索引
peak_values = [py_ir_ac_list[p] for p in py_peaks]
valley_values = [py_ir_ac_list[v] for v in py_valleys]

plt.scatter(py_peaks, peak_values, color='lime', marker='^', s=100, label='Python True Peak', zorder=4, edgecolors='black')
plt.scatter(py_valleys, valley_values, color='magenta', marker='v', s=100, label='Python True Valley', zorder=4, edgecolors='black')

# --- 标记 MCU 原始滞后的打针标记 (空心图形，用于对比视觉延迟) ---
mcu_peaks = df[df['Marker'] > 1000].index
mcu_valleys = df[df['Marker'] < -1000].index

mcu_peak_values = [py_ir_ac_list[i] for i in mcu_peaks]
mcu_valley_values = [py_ir_ac_list[i] for i in mcu_valleys]

plt.scatter(mcu_peaks, mcu_peak_values, facecolors='none', edgecolors='black', s=180, linewidths=2, label='MCU Delayed Peak (Old)', zorder=3)
plt.scatter(mcu_valleys, mcu_valley_values, facecolors='none', edgecolors='red', marker='s', s=180, linewidths=2, label='MCU Delayed Valley (Old)', zorder=3)

plt.title("Algorithm Alignment: Accurate Peak Tracing (Timestamp Backtracking)")
plt.xlabel("Sample Index (10ms/point)")
plt.ylabel("Amplitude")
plt.legend(loc='upper right')
plt.grid(True, linestyle='--', alpha=0.6)
plt.tight_layout()
plt.show()