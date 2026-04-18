#ifndef ADC_DEF_H
#define ADC_DEF_H

/* I2C1上INA238设备数量 */
#define I2C1_INA238_NUM 13
/* I2C2上INA238设备数量 */
#define I2C2_INA238_NUM 13

// for 238
/* 数据换算系数 */
#define INA238_VSHUNT_LSB_MV          0.005f
#define INA238_VBUS_LSB_V             0.003125f
#define INA238_CURRENT_LSB(max_cur)   ((max_cur) / 32768.0f)
#define INA238_POWER_LSB_MULTIPLIER   0.2f
#define INA238_TEMP_LSB_C             0.125f
/* 默认配置 (连续测量分流+总线+温度，转换时间1052μs，不平均) */
#define INA238_ADC_CONFIG_VAL   0xB344    //0xBC85
/* ADCRANGE = 0 (±163.84mV) */
#define INA238_CONFIG_VAL       0x0010    //10000
/* 传输超时 (ms) */
#define INA238_TIMEOUT      100     /* 传输超时时间 (ms) */
#define INA238_CONV_TIMEOUT 100      /* 转换等待超时 (ms) */


/* 分流电阻和最大电流 (根据实际硬件修改) */
#define R_SHUNT1      0.015f   /* 15mΩ */
#define R_SHUNT2      1.0f   /* 1mΩ */
#define MAX_CURRENT1  2.7306666f    /* 2.73A */
#define MAX_CURRENT2  0.04096f    /* 0.273A */


// for 260
/* 固定LSB值 (直接由硬件决定) */
#define INA260_CURRENT_LSB         0.00125f   /* 1.25 mA */
#define INA260_BUSVOLTAGE_LSB      0.00125f   /* 1.25 mV */
#define INA260_POWER_LSB           0.010f     /* 10 mW */
/* 默认配置值 (连续测量电流+总线电压, 转换时间1.1ms, 平均1次) */
#define INA260_CONFIG_DEFAULT      0x6887    //6905  6B47
/* 传输超时 (ms) */
#define INA260_TIMEOUT          100
#define INA260_CONV_TIMEOUT     50


#endif /* ADC_DEF_H */
