

#ifndef POWER_CONFIG_H
#define POWER_CONFIG_H

#include <stdbool.h>
#include "proto_pkg.h"

#ifdef __cplusplus
extern "C" {
#endif

#define POWER_SUPPLY_NAME_MAX               32

typedef struct {
	float R1;
	float R2;
	float R3;
	float tgt_volt;
	int dac_device;
	int dac_channel;
	float Vfb;
	float Vmax;
	int enable_port;
	int enable_pin;
	int means_pt;
	int power_id;
	int dac_value;
	char name[POWER_SUPPLY_NAME_MAX];
} PowerSupplyConfig;


typedef struct {
	bool volt_en;     // 是否采样电压
	bool current_en;  // 是否采样电流
	float shunt_ohm;
	float max_current_a;
	char name[POWER_SUPPLY_NAME_MAX];
} SampleConfig;

typedef struct {
	int sequence;       // 0~17的排列组合，表示上电顺序
	int interval_ms;    // 上电间隔，单位毫秒
	char name[POWER_SUPPLY_NAME_MAX];
} SequenceConfig;

typedef struct {
	bool calibration_en;
	bool volt_sample_en;
	bool curr_sample_en;
	SequenceConfig sequences[POWER_SUPPLY_COUNT];
	PowerSupplyConfig supplies[POWER_SUPPLY_COUNT];
	SampleConfig sample_cfg[SAMPLE_DATA_COUNT]; // 采样配置
} PowersConfig;

#ifdef __cplusplus
}
#endif

#endif // POWER_CONFIG_H


