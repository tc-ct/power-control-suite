# host_usb_demo JSON 关键字说明

本文档说明配置文件 `config/power_config.json` 中各关键字的业务含义。

## 0. 关键字含义（重点）

以下是 `power_supplies` 单项中常用字段含义：

1. `r1`
- 反馈分压网络电阻 R1（单位通常为 kOhm，取决于硬件原理图标注）。

2. `r2`
- 反馈分压网络电阻 R2。

3. `r3`
- 与 DAC 注入支路相关的电阻 R3（用于调压公式补偿）。

4. `dac_dev`
- DAC 设备编号（第几个 DAC 芯片或逻辑设备）。

5. `dac_ch`
- DAC 通道编号（该设备上的第几路通道）。

6. `vfb`
- 电源芯片反馈基准电压（Feedback Reference Voltage）。

7. `vmax`
- 该路 DAC 允许或设计使用的最大输出参考电压。

8. `en_port`
- 使能 GPIO 所在端口编号（板级定义）。

9. `en_pin`
- 使能 GPIO 引脚号。

10. `means_pt`
- 该电源对应的电压测量点索引（采样数组中的通道号）。

补充常用字段：

1. `tgt_v`
- 该路电源的目标输出电压（单位 V）。

2. `id`
- 电源唯一编号。

3. `n`
- 电源名称（便于日志和配置识别）。

## 1. 顶层关键字

1. `power_supplies`（必填）
- 类型：数组
- 长度：必须等于 `POWER_SUPPLY_COUNT`（当前为 18）
- 作用：电源参数主配置，缺失或格式错误会导致加载失败。

2. `power_calibration_en`（可选）
- 类型：布尔
- 映射到：`PowersConfig.calibration_en`

3. `power_volt_sample_en`（可选）
- 类型：布尔
- 映射到：`PowersConfig.volt_sample_en`

4. `power_curr_sample_en`（可选）
- 类型：布尔
- 映射到：`PowersConfig.curr_sample_en`

5. `power_up_sequence`（可选）
- 类型：数组
- 长度：必须等于 `POWER_SUPPLY_COUNT`（18）
- 映射到：`PowersConfig.sequence[]`

## 2. power_up_sequence 子项关键字

每项应包含：

1. `id`（必填）
- 范围：`0 ~ POWER_SUPPLY_COUNT-1`
- 要求：全表唯一，且必须覆盖全部电源 ID。

2. `delay_ms`（必填）
- 范围：`>= 0`
- 映射到：`SequenceConfig.interval_ms`

3. `n`（可选）
- 类型：字符串
- 映射到：`SequenceConfig.name`
- 缺失时：写入空字符串。

## 3. power_supplies 子项关键字

每项必填字段：

1. `id`
2. `tgt_v`
3. `r1`
4. `r2`
5. `r3`
6. `dac_dev`
7. `dac_ch`
8. `vfb`
9. `vmax`
10. `en_port`
11. `en_pin`
12. `means_pt`

附加可选字段：

1. `n`（电源名）

字段约束：

1. `id` 范围必须在 `0 ~ POWER_SUPPLY_COUNT-1`
2. `id` 不可重复，且必须覆盖全部 18 路
3. `means_pt` 范围必须在 `0 ~ 26`

映射关系（结构体）：

1. `id -> power_id`
2. `n -> name`
3. `tgt_v -> tgt_volt`
4. `r1/r2/r3 -> R1/R2/R3`
5. `dac_dev/dac_ch -> dac_device/dac_channel`
6. `vfb/vmax -> Vfb/Vmax`
7. `en_port/en_pin -> enable_port/enable_pin`
8. `means_pt -> means_pt`

## 4. 默认值行为

在解析前会先清零整个 `PowersConfig`，并初始化：

1. `sequence[i].sequence = i`
2. `sequence[i].interval_ms = 0`
3. `sequence[i].name = ""`

如果 JSON 中提供 `power_up_sequence`，则会覆盖上述默认顺序和间隔。

## 5. 当前未解析关键字

当前代码中 `power_samples` 尚未接入 `SampleConfig sample_cfg[]` 的解析。
即：该关键字可以保留在 JSON 中，但不会写入 `PowersConfig.sample_cfg`。

## 6. 常见失败原因

1. `power_supplies` 缺失或不是数组
2. `power_supplies` 数量不是 18
3. 某个 `power_supplies` 子项缺少必填字段
4. `id` 越界、重复或不完整
5. `means_pt` 越界
6. `power_up_sequence` 数量不是 18
7. `power_up_sequence` 中 `id` 重复或未覆盖完整
8. `delay_ms` 为负数

