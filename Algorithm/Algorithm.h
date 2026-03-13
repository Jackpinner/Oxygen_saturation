#ifndef ALGORITHM_H_
#define ALGORITHM_H_
#include "main.h"

#define FILTER_WINDOW_SIZE 12  // 滤波窗口大小（建议范围 8 ~ 20，越大越平滑，但波形会有少许延迟）

float Smooth_Filter(float input_data);

#endif /* ALGORITHM_H_ */