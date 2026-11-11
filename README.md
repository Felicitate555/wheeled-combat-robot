# 轮式格斗机器人

**一套面向擂台对抗的 STM32 轮式机器人控制工程。**

项目结合距离与边缘传感器，控制机器人完成上台、搜索目标、追踪攻击和边缘避险。代码围绕实车调试展开，包含工程配置、驱动、测试与中文说明。

## 功能概览

- **自主战斗**：在搜索、追踪与攻击状态间切换。
- **擂台保护**：优先处理边缘危险，并监控掉台与再次登台。
- **统一传感器快照**：周期采集数据，供状态机决策使用。
- **可调参数与测试**：集中配置速度及阈值，附 PC 端测试和静态检查。

**平台：** STM32H723ZGTx、Keil MDK / Arm Compiler 6、STM32 HAL/CMSIS。

## 快速开始

安装 Keil 和对应芯片包，打开 `MDK-ARM/ToothGeigei.uvprojx` 并执行 Rebuild All。硬件映射、控制流程和参数详见 [项目代码说明](项目代码说明.md)。速度与阈值需要根据实际车体、传感器和场地重新验证。

---


当前交接的单一最终工程版本：STM32H723ZGTx，Keil MDK / Arm Compiler 6。

## 快速入口

- Keil 工程：`MDK-ARM/ToothGeigei.uvprojx`
- CubeMX配置：`ToothGeigei.ioc`
- 中文代码说明：`项目代码说明.md`
- 参数集中配置：`MDK-ARM/User/fight_config.h`
- 主程序模式：`MDK-ARM/User/User.c`、`MDK-ARM/User/Mode.c`
- 战斗逻辑：`MDK-ARM/User/Fight.c`
- 传感器快照：`MDK-ARM/User/Sensor.c`
- 掉台监控：`MDK-ARM/User/StageMonitor.c`
- 电机输出：`MDK-ARM/User/Car.c`
- PC端回归测试和静态检查：`Tests/`

工程源码、HAL/CMSIS依赖、链接脚本、测试和中文说明均保留。为保持分享内容聚焦代码，本包不含旧版本、硬件照片、原始ZIP备份、Keil个人工作区文件和本机编译缓存/日志。克隆后用 Keil 打开上述工程并执行 Rebuild All。

主要逻辑：5ms周期采集统一传感器快照；Fight根据目标执行Search/Track/Attack，原始Edge危险最高优先级；StageMonitor只监听运动与传感器历史并产生高置信掉台事件；Mode管理首次上台和再次登台。参数、传感器映射与状态流程见`项目代码说明.md`。

速度与Range5阈值依赖车体、电池及场地。部署前应检查电机方向和Edge有效电平，再自行重新编译和实车验证。第三方HAL/CMSIS版权声明随源码保留；Keil及芯片包需使用者自行安装。
