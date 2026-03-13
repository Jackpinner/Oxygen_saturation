#include "Algorithm.h"

// 滤波窗口大小（因为你已经在 10ms 降采样过了，这里设 8~10 就非常平滑了）
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