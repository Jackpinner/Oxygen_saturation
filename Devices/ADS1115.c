#include "ads1115.h"

//#ifdef ADS1115

ADS1115_InitTypeDefine ADS1115_ADDR_GND;
ADS1115_InitTypeDefine ADS1115_ADDR_VDD;
float ADS1115_CHn_Vol[4];
/**
 * 可供外部调用的全局变量，记录了ADS1115采样的原始16位数据。调用void ADS1115_RefreshAllChannel( )函数可以刷新这个变量。
 * 通过定义宏ADS1115_USE_FILTER，可以将ADS1115的轮询采样数据经过滑动滤波后，保存到ADS1115_RawData[]中。
 * 通过float ADS1115_RawDataToVoltage(int16_t rawData)函数可以将ADS1115_RawData[]换算成对应的电压值。
 */
int16_t ADS1115_RawData[4] = {0};


/**
 * @brief 完成芯片控制端口初始化，并设置初始状态
 */
void ADS1115_Init()
{
	// 假设你的 ADS1115 ADDR 引脚接地，7位地址为 0x48
    // STM32 的 HAL 库要求地址左移 1 位，即 0x48 << 1 = 0x90
    ADS1115_ADDR_GND.ADDRESS = 0x90;
    
    // 初始化默认操作通道为 AIN0
    ADS1115_ADDR_GND.CHANNEL = ADS1115_CHANNEL0;
    
    // 也可以给电压和原始数据数组清零
    for(int i = 0; i < 4; i++){
        ADS1115_ADDR_GND.ADS1115_Vol[i] = 0.0f;
        ADS1115_ADDR_GND.ADS1115_RawData[i] = 0;
    }

}


/**
 * @brief Configuration of ADS1115, single-shot
 */
void ADS1115_UserConfig_SingleConver(ADS1115_InitTypeDefine* ADS1115_InitStruct,ADS1115_ADDRESS ADDRESS)
{
	ADS1115_InitStruct->COMP_LAT = ADS1115_COMP_LAT_0;
	ADS1115_InitStruct->COMP_MODE = ADS1115_COMP_MODE_0;
	ADS1115_InitStruct->COMP_POL = ADS1115_COMP_POL_0;
	ADS1115_InitStruct->DataRate = ADS1115_DataRate_475;
	ADS1115_InitStruct->MODE = ADS1115_MODE_SingleConver;
	ADS1115_InitStruct->MUX = ADS1115_MUX_Channel_0;
	ADS1115_InitStruct->OS = ADS1115_OS_SingleConverStart;
	ADS1115_InitStruct->PGA = ADS1115_PGA_4096;
	ADS1115_InitStruct->ADDRESS = ADDRESS;
	ADS1115_InitStruct->CHANNEL = ADS1115_CHANNEL0;
	ADS1115_InitStruct->COMP_QUE = ADS1115_COMP_QUE_3;
	ADS1115_Config(ADS1115_InitStruct);
	
}


/**
 * @brief Configuration of ADS1115, continuous conversion
 */
void ADS1115_UserConfig_ContinuConver(ADS1115_InitTypeDefine* ADS1115_InitStruct,ADS1115_ADDRESS ADDRESS)
{
	ADS1115_InitStruct->COMP_LAT = ADS1115_COMP_LAT_0;
	ADS1115_InitStruct->COMP_MODE = ADS1115_COMP_MODE_0;
	ADS1115_InitStruct->COMP_POL = ADS1115_COMP_POL_0;
	ADS1115_InitStruct->DataRate = ADS1115_DataRate_860;
	ADS1115_InitStruct->MODE = ADS1115_MODE_ContinuConver;
	ADS1115_InitStruct->MUX = ADS1115_MUX_Channel_0;
	ADS1115_InitStruct->OS = ADS1115_OS_OperationalStatus;
	ADS1115_InitStruct->PGA = ADS1115_PGA_4096;
	ADS1115_InitStruct->ADDRESS = ADDRESS;
	ADS1115_InitStruct->CHANNEL = ADS1115_CHANNEL0;
	ADS1115_InitStruct->COMP_QUE = ADS1115_COMP_QUE_0;
	ADS1115_Config(ADS1115_InitStruct);
	uint8_t threshold_cmd[3];
    
    // 1. 设置高阈值寄存器 (0x03) 为 0x8000
    threshold_cmd[0] = ADS1115_Pointer_HiThreshReg; 
    threshold_cmd[1] = 0x80;
    threshold_cmd[2] = 0x00;
    HAL_I2C_Master_Transmit(&hi2c1, ADS1115_InitStruct->ADDRESS, threshold_cmd, 3, 100);

    // 2. 设置低阈值寄存器 (0x02) 为 0x0000
    threshold_cmd[0] = ADS1115_Pointer_LoThreshReg; 
    threshold_cmd[1] = 0x00;
    threshold_cmd[2] = 0x00;
    HAL_I2C_Master_Transmit(&hi2c1, ADS1115_InitStruct->ADDRESS, threshold_cmd, 3, 100);
}
	



/**
 * @brief 配置ADS1115
 * @param ADS1115_InitStruct: 用来配置ADS1115的结构体变量指针
 * @return 配置结果
 * 		@arg: fail
 * 		@arg: success
 */
void ADS1115_Config(ADS1115_InitTypeDefine *ADS1115_InitStruct)
{
	uint16_t Config;
	uint8_t Writebuff[3];

	Config = ADS1115_InitStruct->OS + ADS1115_InitStruct->MUX + ADS1115_InitStruct->PGA + ADS1115_InitStruct->MODE
			+ ADS1115_InitStruct->DataRate + ADS1115_InitStruct->COMP_MODE + ADS1115_InitStruct->COMP_POL
			+ ADS1115_InitStruct->COMP_LAT + ADS1115_InitStruct->COMP_QUE;
  Writebuff[0] = ADS1115_Pointer_ConfigReg;
	Writebuff[1] = (unsigned char) ((Config >> 8) & 0xFF);
	Writebuff[2] = (unsigned char) (Config & 0xFF);
	HAL_I2C_Master_Transmit(&hi2c1, ADS1115_InitStruct->ADDRESS, Writebuff, 3, 100);

}



/**
 * @brief 读取ADS1115当前通道下的原始数据
 * @param rawData: 传入一个int16_t整型变量的指针，ADS1115的原始数据将保存在这个变量中
 * @return 读取结果
 * 		@arg 0: fail
 * 		@arg 1: success
 */

void ADS1115_ReadRawData(ADS1115_InitTypeDefine *ADS1115_InitStruct)
{
	unsigned char Result[2];
	uint8_t Writebuff[1];	
	Writebuff[0] = ADS1115_Pointer_ConverReg;
	
	HAL_I2C_Master_Transmit(&hi2c1, ADS1115_InitStruct->ADDRESS, Writebuff, 1, 100);
	
	
	HAL_I2C_Master_Receive(&hi2c1, ADS1115_InitStruct->ADDRESS , Result, 2, 100);
	
	ADS1115_InitStruct->ADS1115_RawData[ADS1115_InitStruct->CHANNEL]  =  (int16_t) (((Result[0] << 8) & 0xFF00) | (Result[1] & 0xFF));


}


/**
 * @brief Switch the channel of ADS1115
 * @param channel
 */
void ADS1115_ScanChannel(ADS1115_InitTypeDefine *ADS1115_InitStruct)
{
	switch (ADS1115_InitStruct->CHANNEL)
	{
	case ADS1115_CHANNEL0:
		ADS1115_InitStruct->MUX = ADS1115_MUX_Channel_0;
		break;
	case ADS1115_CHANNEL1:
		ADS1115_InitStruct->MUX = ADS1115_MUX_Channel_1;
		break;
	case ADS1115_CHANNEL2:
		ADS1115_InitStruct->MUX = ADS1115_MUX_Channel_2;
		break;
	case ADS1115_CHANNEL3:
		ADS1115_InitStruct->MUX = ADS1115_MUX_Channel_3;
		break;
	default:
		break;
	}
	ADS1115_Config(ADS1115_InitStruct);
}


/**
 * @brief 将传感器的原始采样数据转化为电压数据，
 * 			根据ADS1115_InitType结构体中包含的增益信息计算
 * @param rawData: 待转换的原始数据
 * @retval 返回经过计算的电压值
 */
void ADS1115_RawDataToVoltage(ADS1115_InitTypeDefine *ADS1115_InitStruct)
{
	

	switch (ADS1115_InitStruct->PGA)
	{
	case ADS1115_PGA_0256:
		//voltage = rawData * 0.0078125;
	ADS1115_InitStruct->ADS1115_Vol[ADS1115_InitStruct->CHANNEL] = ADS1115_InitStruct->ADS1115_RawData[ADS1115_InitStruct->CHANNEL]* 0.0078125;
		break;

	case ADS1115_PGA_0512:
	//	voltage = rawData * 0.015625;
	ADS1115_InitStruct->ADS1115_Vol[ADS1115_InitStruct->CHANNEL] = ADS1115_InitStruct->ADS1115_RawData[ADS1115_InitStruct->CHANNEL]* 0.015625;
		break;

	case ADS1115_PGA_1024:
		//voltage = rawData * 0.03125;
	ADS1115_InitStruct->ADS1115_Vol[ADS1115_InitStruct->CHANNEL] = ADS1115_InitStruct->ADS1115_RawData[ADS1115_InitStruct->CHANNEL]* 0.03125;
		break;

	case ADS1115_PGA_2048:
	//	voltage = rawData * 0.0625;
	ADS1115_InitStruct->ADS1115_Vol[ADS1115_InitStruct->CHANNEL] = ADS1115_InitStruct->ADS1115_RawData[ADS1115_InitStruct->CHANNEL]* 0.0625;
		break;

	case ADS1115_PGA_4096:
		//voltage = rawData * 0.125;
	ADS1115_InitStruct->ADS1115_Vol[ADS1115_InitStruct->CHANNEL] = ADS1115_InitStruct->ADS1115_RawData[ADS1115_InitStruct->CHANNEL]* 0.125;
		break;

	case ADS1115_PGA_6144:
	//	voltage = rawData * 0.1875;
	ADS1115_InitStruct->ADS1115_Vol[ADS1115_InitStruct->CHANNEL] = ADS1115_InitStruct->ADS1115_RawData[ADS1115_InitStruct->CHANNEL]* 0.1875;
		break;

	default:
		//voltage = 0;
	ADS1115_InitStruct->ADS1115_Vol[ADS1115_InitStruct->CHANNEL] = 0;
		break;
	}


}


/**
 * @brief 直接获取ADS1115当前通道的电压采样值
 * @return 电压采样值
 */
void ADS1115_GetVoltage(ADS1115_InitTypeDefine *ADS1115_InitStruct)
{
	

	ADS1115_ReadRawData(ADS1115_InitStruct);

	ADS1115_RawDataToVoltage(ADS1115_InitStruct);
	
}


/**
 * @brief 获取并计算ADC采样的平均电压值
 * @param num: 计算平均值的数量
 * @retval 电压采样的平均值
 */
void ADS1115_GetAverageVoltage(ADS1115_InitTypeDefine *ADS1115_InitStruct)
{

//	    ADS1115_ReadRawData(&ADS1115_InitType);
//        
//      ADS1115_GetVoltage(&ADS1115_InitType);
//			ADS1115_RefreshAllChannel(&ADS1115_InitType);

//	return ADS1115_RawDataToVoltage(sum/num);
}

/**
 * @brief 刷新ADS1115全部通道的采样数据
 * 		由于ADS1115通道切换后需要等待较长时间数据才能够稳定，
 * 		在进行多路数据采集的时候，切换通道后延时阻塞等待切换完成会占用过多的系统时间，
 * 		因此需要在一个定时器中轮询采集ADS1115数据，每次采集完成后，切换到下一个通道
 * 		大幅度提高了系统工作的效率。
 *
 * 		调用此函数可以刷新全局变量ADS1115_RawData[4]的值。
 *
 * 		应当在一个定时器更新中断服务函数中周期性的调用此函数，更新周期最好小于200Hz
 */
void ADS1115_RefreshAllChannel(ADS1115_InitTypeDefine *ADS1115_InitStruct)
{


	//通道切换时可能有不确定的数据读出，因此需要将前1~2次读出的数据舍弃
	ADS1115_ReadRawData(ADS1115_InitStruct);
	ADS1115_ReadRawData(ADS1115_InitStruct);
	ADS1115_ReadRawData(ADS1115_InitStruct);
	//ADS1115总共4个通道
	ADS1115_InitStruct->CHANNEL++;


	
	

	
	if(ADS1115_InitStruct->CHANNEL > ADS1115_MAX_CHANNEL-1)
		ADS1115_InitStruct->CHANNEL = ADS1115_CHANNEL0;

	//结束采样后切换至下一通道
	ADS1115_ScanChannel(ADS1115_InitStruct);
}
