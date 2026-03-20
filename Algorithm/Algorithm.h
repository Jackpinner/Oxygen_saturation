#ifndef ALGORITHM_H_
#define ALGORITHM_H_
#include "main.h"

#define PPG_SAMPLE_RATE 100  // 中层送入算法的频率 (100Hz)
#define PPG_BUFFER_SIZE 400  // 寻峰窗口大小：400点 = 4秒数据
#define UPDATE_INTERVAL 100  // 刷新间隔：每100个点(1秒)计算并刷新一次心率
float Smooth_Filter(float input_data);
//uint8_t Get_Heart_Rate(float new_sample, int32_t *bpm);
void Track_Pulse_Wave(float new_sample, uint8_t *is_peak, uint8_t *is_valley, int32_t *bpm);
float Smooth_Filter_IR(float input_data);
float Smooth_Filter_Red(float input_data);
#endif /* ALGORITHM_H_ */
