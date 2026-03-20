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
 * @brief  实时波峰/波谷状态机检测 (带有抗重搏波“不应期”机制)
 * @param  new_sample  最新输入的一个平滑波形点
 * @param  is_peak     输出参数：如果确认为波峰，输出 1
 * @param  is_valley   输出参数：如果确认为波谷，输出 1
 * @param  bpm         输出参数：计算出的实时心率
 */
void Track_Pulse_Wave(float new_sample, uint8_t *is_peak, uint8_t *is_valley, int32_t *bpm)
{
    static float max_v = 0.0f;            
    static float min_v = 99999.0f;        
    static uint8_t state = 1;             // 1: 寻找波峰, 0: 寻找波谷
    static float amplitude = 500.0f;      // 初始脉搏波幅值估算加大，防止初始误判
    static uint32_t time_tick = 0;        
    static uint32_t last_peak_tick = 0;   

    *is_peak = 0;
    *is_valley = 0;
    time_tick++;

    // 实时追踪极值
    if (new_sample > max_v) max_v = new_sample;
    if (new_sample < min_v) min_v = new_sample;

    // =============== 寻找波峰状态 ===============
    if (state == 1) 
    {
        // 确认波峰条件 1：回落超过幅值的 20%
        // 确认波峰条件 2：【不应期保护】距离上一个波峰至少过了 50 个点 (即 500ms, 对应最大心率 120 BPM)
        if ((new_sample < max_v - amplitude * 0.2f) && ((time_tick - last_peak_tick) > 50)) 
        {
            *is_peak = 1;         
            state = 0;            // 切换为寻找波谷
            min_v = new_sample;   // 重置最低点追踪器

            // 计算 BPM
            if (last_peak_tick > 0) 
            {
                uint32_t interval = time_tick - last_peak_tick; 
                // 限制合法心率在 40 ~ 180 BPM 之间，太离谱的直接过滤
                if (interval > 33 && interval < 150) 
                {
                    *bpm = (100 * 60) / interval; 
                }
            }
            last_peak_tick = time_tick;
        }
    }
    // =============== 寻找波谷状态 ===============
    else 
    {
        // 确认波谷条件 1：反弹超过幅值的 20%
        // 确认波谷条件 2：【不应期保护】防止把半山腰的重搏波当成真波谷，距离上次波峰至少过 15 个点
        if ((new_sample > min_v + amplitude * 0.2f) && ((time_tick - last_peak_tick) > 15)) 
        {
            *is_valley = 1;       
            state = 1;            // 切换为寻找波峰
            
            // 计算当前周期的交流幅值 AC
            amplitude = max_v - min_v;
            
            // 【极其关键的底线保护】：根据你图里的 Y 轴数值，波形上下浮动在 1000 左右
            // 如果幅值太小，说明没放手指或者按太紧了，强制恢复默认大阈值，防止被微小噪声欺骗
            if (amplitude < 100.0f) amplitude = 500.0f;   
            if (amplitude > 3000.0f) amplitude = 3000.0f; 

            max_v = new_sample;   // 重置最高点追踪器
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