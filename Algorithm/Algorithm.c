#include "Algorithm.h"
#define FILTER_WINDOW_SIZE 10

/**
 * @brief  脉搏波平滑滤波器 (滑动窗口平均法)
 * @param  input_data: 经过初步降采样的波形数据
 * @return float: 深度平滑后的数据
 */
float Smooth_Filter(float input_data)
{
    static float window[FILTER_WINDOW_SIZE] = {0};
    static float sum = 0;
    static uint8_t index = 0;
    static uint8_t count = 0;

    // 滑动核心：减去旧数据，加上新数据
    sum -= window[index];
    sum += input_data;
    window[index] = input_data;

    index = (index + 1) % FILTER_WINDOW_SIZE;

    if (count < FILTER_WINDOW_SIZE)
    {
        count++;
        return sum / count;
    }

    return sum / FILTER_WINDOW_SIZE;
}

static int32_t ppg_buffer[PPG_BUFFER_SIZE];
static uint16_t ppg_idx = 0;
static uint8_t is_buffer_filled = 0; // 记录开机是否已经攒满过4秒

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