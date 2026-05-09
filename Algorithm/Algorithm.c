#include "Algorithm.h"
#define FILTER_WINDOW_SIZE 10

static int32_t ppg_buffer[PPG_BUFFER_SIZE];
static uint16_t ppg_idx = 0;
static uint8_t is_buffer_filled = 0; // 记录开机是否已经攒满过4秒

static PPG_Filter_State ir_state = {0, 0, 0};
static PPG_Filter_State red_state = {0, 0, 0};

static float rise_value = 0.25f;//波谷检测
static float fall_value = 0.25f;//波峰检测

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

    if (count < FILTER_WINDOW_SIZE)
    {
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

    if (count < FILTER_WINDOW_SIZE)
    {
        count++;
        return sum / count;
    }
    return sum / FILTER_WINDOW_SIZE;
}

/**
 * @brief  升级版双光路状态机：利用 AC/DC 分离信号计算心率和血氧
 * @param  ir_ac       红外光经过高通/低通后的交流分量 (围绕0波动)
 * @param  red_ac      红光经过高通/低通后的交流分量 (围绕0波动)
 * @param  ir_dc       红外光的实时直流基线 (由超低通滤波器提取)
 * @param  red_dc      红光的实时直流基线 (由超低通滤波器提取)
 * @param  is_peak     输出：波峰标志
 * @param  is_valley   输出：波谷标志
 * @param  bpm         输出：实时心率 (BPM)
 * @param  spo2        输出：实时血氧饱和度 (%)
 */
void Track_Pulse_Wave_Dual(float ir_ac, float red_ac, float ir_dc, float red_dc, 
                           uint8_t *is_peak, uint8_t *is_valley, int32_t *bpm, float *spo2, float *out_r)
{
    static float ir_max = -9999.0f;            
    static float ir_min = 9999.0f;        
    static float red_max = -9999.0f;
    static float red_min = 9999.0f;

    static uint8_t state = 1;             
    static float ir_amplitude = 100.0f;   
    static uint32_t time_tick = 0;        
    static uint32_t last_peak_tick = 0;   

    *is_peak = 0;
    *is_valley = 0;
    time_tick++;

    if (ir_ac > ir_max) ir_max = ir_ac;
    if (ir_ac < ir_min) ir_min = ir_ac;
    if (red_ac > red_max) red_max = red_ac;
    if (red_ac < red_min) red_min = red_ac;

    if (state == 1) 
    {
        if ((ir_ac < ir_max - ir_amplitude * fall_value) && (time_tick - last_peak_tick > 35)) 
        {
            *is_peak = 1;         
            state = 0;            
            
            ir_min = ir_ac;   
            red_min = red_ac;

            if (last_peak_tick > 0) 
            {
                uint32_t interval = time_tick - last_peak_tick; 
                // 确保 interval 不是 0 才会除
                if (interval >= 33 && interval <= 150) 
                {
                    *bpm = (PPG_SAMPLE_RATE * 60) / interval; 
                }
            }
            last_peak_tick = time_tick;
        }
    }
    else 
    {
        if ((ir_ac > ir_min + ir_amplitude * rise_value) && (time_tick - last_peak_tick > 40)) 
        {
            *is_valley = 1;       
            state = 1;            
            
            float ir_ac_pp = ir_max - ir_min;
            float red_ac_pp = red_max - red_min;
            
            ir_amplitude = ir_ac_pp; 

            // ====================================================
            // 绝地防崩溃校验：严格防止分母为0或极其微小的值导致溢出
            // ====================================================
            if (ir_dc > 10.0f && red_dc > 10.0f && ir_ac_pp > 1.0f) 
            {
                float ir_ratio = ir_ac_pp / ir_dc;
                float red_ratio = red_ac_pp / red_dc;
                
                // 只有当分母(ir_ratio)有合理值时才进行除法计算
                if (ir_ratio > 0.00001f) 
                {
                    float R = red_ratio / ir_ratio;
                    *out_r = R;
                    float current_spo2 = -42369.8085f * R * R + 75929.1431f * R + -33921.0470f;
                    
                    if (current_spo2 > 100.0f) current_spo2 = 100.0f;
                    if (current_spo2 < 50.0f)  current_spo2 = 0.0f; 
                    
                    *spo2 = current_spo2;
                }
            }

            if (ir_amplitude < 20.0f)  ir_amplitude = 100.0f;   
            if (ir_amplitude > 5000.0f) ir_amplitude = 5000.0f; 

            ir_max = ir_ac;   
            red_max = red_ac;
        }
    }
}

/*
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

/**
 * @brief AC/DC 分离式带通滤波器
 * @param input_data 最新采集的一个ADC点
 * @param state      保存当前通道滤波状态的结构体指针
 */
void IIR_Bandpass_Filter(float input_data, PPG_Filter_State *state)
{
    // 冷启动初始化，防止基线从0开始缓慢爬升
    if (state->is_init == 0)
    {
        state->dc_baseline = input_data;
        state->ac_filtered = 0.0f;
        state->is_init = 1;
        return;
    }

    // 1. 提取极低频基线 (DC) - 截止频率约 0.2Hz
    // 权重 0.0124 对应 100Hz 采样率下的极慢速跟随
    state->dc_baseline = 0.9876f * state->dc_baseline + 0.0124f * input_data;

    // 2. 扣除基线，得到初步的纯交流波 (高通效果)
    float pure_ac = input_data - state->dc_baseline;

    // 3. 对交流波进行低通滤波 - 截止频率约 5Hz，去除高频毛刺
    // 权重 0.239 对应 100Hz 采样率下的平滑
    state->ac_filtered = 0.7610f * state->ac_filtered + 0.2390f * pure_ac;
}