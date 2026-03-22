#include "Algorithm.h"
#define FILTER_WINDOW_SIZE 10

static int32_t ppg_buffer[PPG_BUFFER_SIZE];
static uint16_t ppg_idx = 0;
static uint8_t is_buffer_filled = 0; // 记录开机是否已经攒满过4秒

float Smooth_Filter_IR(float input_data)
{
    static float window[FILTER_WINDOW_SIZE] = {0};
    static float sum = 0;
    static uint8_t index = 0;
    static uint8_t count = 0;

    sum -= window[index];
    sum += input_data;
    window[index] = input_data;
    index = (index + 1) % FILTER_WINDOW_SIZE;

    if (count < FILTER_WINDOW_SIZE) {
        count++;
        return sum / count;
    }
    return sum / FILTER_WINDOW_SIZE;
}

float Smooth_Filter_Red(float input_data)
{
    static float window[FILTER_WINDOW_SIZE] = {0};
    static float sum = 0;
    static uint8_t index = 0;
    static uint8_t count = 0;

    sum -= window[index];
    sum += input_data;
    window[index] = input_data;
    index = (index + 1) % FILTER_WINDOW_SIZE;

    if (count < FILTER_WINDOW_SIZE) {
        count++;
        return sum / count;
    }
    return sum / FILTER_WINDOW_SIZE;
}

/**
 * @brief  终极双光路状态机：同时计算心率 (BPM) 和血氧 (SpO2)
 * @param  ir_sample   最新输入的红外光平滑数据
 * @param  red_sample  最新输入的红光平滑数据
 * @param  is_peak     输出：波峰标志
 * @param  is_valley   输出：波谷标志
 * @param  bpm         输出：实时心率
 * @param  spo2        输出：实时血氧饱和度 (百分比)
 */
void Track_Pulse_Wave_Dual(float ir_sample, float red_sample, uint8_t *is_peak, uint8_t *is_valley, int32_t *bpm, float *spo2)
{
    // IR 光的极值追踪
    static float ir_max = 0.0f;            
    static float ir_min = 99999.0f;        
    
    // Red 光的极值追踪
    static float red_max = 0.0f;
    static float red_min = 99999.0f;

    static uint8_t state = 1;             // 1: 寻找波峰, 0: 寻找波谷
    static float ir_amplitude = 500.0f;   // IR 的动态幅值
    static uint32_t time_tick = 0;        
    static uint32_t last_peak_tick = 0;   

    *is_peak = 0;
    *is_valley = 0;
    time_tick++;

    // 实时追踪两条光路的最高点和最低点
    if (ir_sample > ir_max) ir_max = ir_sample;
    if (ir_sample < ir_min) ir_min = ir_sample;
    
    if (red_sample > red_max) red_max = red_sample;
    if (red_sample < red_min) red_min = red_sample;

    // =============== 寻找波峰状态 ===============
    // 【规则】：永远以 IR 光为基准来判断波峰，因为 IR 穿透力强，波形最稳
    if (state == 1) 
    {
        if ((ir_sample < ir_max - ir_amplitude * 0.2f) && ((time_tick - last_peak_tick) > 30)) 
        {
            *is_peak = 1;         
            state = 0;            
            
            // 重置所有最低点追踪器，准备迎接下坡
            ir_min = ir_sample;   
            red_min = red_sample;

            // 计算 BPM
            if (last_peak_tick > 0) 
            {
                uint32_t interval = time_tick - last_peak_tick; 
                if (interval > 33 && interval < 150) 
                {
                    *bpm = (100 * 60) / interval; 
                }
            }
            last_peak_tick = time_tick;
        }
    }
    // =============== 寻找波谷状态 (结算血氧的绝佳时机！) ===============
    else 
    {
        if ((ir_sample > ir_min + ir_amplitude * 0.2f) && ((time_tick - last_peak_tick) > 15)) 
        {
            *is_valley = 1;       
            state = 1;            
            
            // 1. 提取 IR 光的 AC 和 DC
            float ir_ac = ir_max - ir_min;
            float ir_dc = ir_min;
            ir_amplitude = ir_ac; // 更新动态幅值供下一次判断使用

            // 2. 提取 Red 光的 AC 和 DC
            float red_ac = red_max - red_min;
            float red_dc = red_min;

            // 3. 【核心计算】血氧 R 值和 SpO2%
            if (ir_dc > 0 && red_dc > 0 && ir_ac > 0) // 防止除零异常
            {
                // R = (AC_red / DC_red) / (AC_ir / DC_ir)
                float R = (red_ac / red_dc) / (ir_ac / ir_dc);
                
                // 使用美信开源库的二次拟合经验公式 (你之前发给我的代码里就是这个)
                float current_spo2 = -45.06f * R * R + 30.354f * R + 94.845f;
                
                // 上下限硬约束
                if (current_spo2 > 100.0f) current_spo2 = 100.0f;
                if (current_spo2 < 50.0f) current_spo2 = 0.0f; // 算出来太低说明波形不准，直接置零
                
                *spo2 = current_spo2;
            }

            // 底线保护
            if (ir_amplitude < 100.0f) ir_amplitude = 500.0f;   
            if (ir_amplitude > 3000.0f) ir_amplitude = 3000.0f; 

            // 重置最高点追踪器，准备迎接上坡
            ir_max = ir_sample;   
            red_max = red_sample;
        }
    }
}
/**
 * @brief  输入一个点，如果凑够了刷新周期，就算出最新心率
 * @param  new_sample: 从平滑滤波器出来的最新波形点
 * @param  bpm: 用来保存计算结果的指针
 * @return 1表示心率有更新，0表示正在采集中
 */
uint8_t Get_Heart_Rate(float new_sample, int32_t *bpm)
{
    // 1. 数据进站：将浮点波形转化为整数，存入数组
    ppg_buffer[ppg_idx++] = (int32_t)new_sample;

    // 如果开机还没攒满 4 秒的数据，先按兵不动
    if (ppg_idx < PPG_BUFFER_SIZE && !is_buffer_filled)
    {
        return 0;
    }

    is_buffer_filled = 1;

    // 2. 核心算法触发：当 4 秒的数据集满时
    if (ppg_idx >= PPG_BUFFER_SIZE)
    {
        int32_t i;
        int32_t mean = 0;

        // [步骤 A] 提取直流分量 (DC Removal)
        for (i = 0; i < PPG_BUFFER_SIZE; i++)
            mean += ppg_buffer[i];
        mean /= PPG_BUFFER_SIZE;

        // [步骤 B] 去除直流，得到纯交流波形 (AC)，并找出最大幅度
        int32_t ac_data[PPG_BUFFER_SIZE];
        int32_t max_ac = 0;
        for (i = 0; i < PPG_BUFFER_SIZE; i++)
        {
            ac_data[i] = ppg_buffer[i] - mean;
            if (ac_data[i] > max_ac)
                max_ac = ac_data[i];
        }

        // [步骤 C] 自适应阈值生成：取最大波峰的三分之一作为门槛，滤除小毛刺
        int32_t threshold = max_ac / 3;
        if (threshold < 10)
            threshold = 10; // 兜底保护阈值

        // [步骤 D] 在时间轴上执行寻峰 (Peak Detection)
        int32_t peaks[20];
        int32_t num_peaks = 0;
        int32_t min_dist = 35; // 限制两峰最短距离：35点=350ms，约等于人体极限心率 170 BPM

        for (i = 1; i < PPG_BUFFER_SIZE - 1; i++)
        {
            // 条件1：大于自适应阈值，且是局部最高点
            if (ac_data[i] > threshold && ac_data[i] > ac_data[i - 1] && ac_data[i] > ac_data[i + 1])
            {
                // 条件2：满足与上一个峰的安全距离
                if (num_peaks == 0 || (i - peaks[num_peaks - 1]) >= min_dist)
                {
                    peaks[num_peaks++] = i;
                    if (num_peaks >= 20)
                        break; // 防溢出
                }
                else
                {
                    // 如果靠得太近，说明可能是伴生的重搏波，只保留更高的那个“真主峰”
                    if (ac_data[i] > ac_data[peaks[num_peaks - 1]])
                    {
                        peaks[num_peaks - 1] = i;
                    }
                }
            }
        }
        // [步骤 E] 根据所有波峰之间的平均距离，计算 BPM
        if (num_peaks >= 2)
        {
            int32_t interval_sum = 0;
            for (i = 1; i < num_peaks; i++)
            {
                interval_sum += (peaks[i] - peaks[i - 1]);
            }
            int32_t avg_interval = interval_sum / (num_peaks - 1);

            // 核心公式： (采样率 * 60秒) / 峰间距点数
            *bpm = (PPG_SAMPLE_RATE * 60) / avg_interval;
        }

        // [步骤 F] 数据平移更新 (Overlap-Add)
        // 将后 300 个点平移到前面，空出后 100 个位置给下一秒的新数据
        int32_t shift_len = PPG_BUFFER_SIZE - UPDATE_INTERVAL;
        for (i = 0; i < shift_len; i++)
        {
            ppg_buffer[i] = ppg_buffer[i + UPDATE_INTERVAL];
        }

        // 将写指针拨回 300，再收集 1秒 数据就会再次触发计算
        ppg_idx = shift_len;

        return (num_peaks >= 2) ? 1 : 0; // 返回 1 告诉主程序心率算出来了
    }

    return 0;
}