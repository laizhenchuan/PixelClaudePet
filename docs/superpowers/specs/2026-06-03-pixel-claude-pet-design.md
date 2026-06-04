# Pixel Claude Pet — PC 状态监控副屏 设计文档

**日期**: 2026-06-03
**平台**: STM32F407VET6 + ILI9341 2.8" LCD + Windows PC
**状态**: 已确认

---

## 1. 项目概述

将现有的 Pixel Slime Pet 电子宠物项目改造为 **Pixel Claude Pet**，在保留宠物养成功能的基础上，新增 PC 状态监控副屏功能。

PC 端 Python 服务采集系统硬件状态（CPU/GPU 温度、占用率、内存使用率）和 Claude Code 工作状态，通过串口发送 JSON 数据给 STM32，STM32 驱动 ILI9341 LCD 显示 Claude 像素形象 + PC 实时数据。

## 2. 架构图

```
┌─────────────────────────────────────────────┐
│  PC (Windows)                                │
│                                              │
│  ┌──────────────┐    ┌────────────────────┐  │
│  │ Claude Code   │───▶│ claude_status.txt  │  │
│  │ (Hooks)       │    │ (状态文件)          │  │
│  └──────────────┘    └────────┬───────────┘  │
│                               │              │
│  ┌──────────────┐    ┌───────▼───────────┐  │
│  │ psutil/GPUtil │───▶│ Windows Service   │  │
│  │ (硬件监控)     │    │ (Python)          │  │
│  └──────────────┘    └────────┬───────────┘  │
│                               │ UART/USB     │
└───────────────────────────────┼──────────────┘
                                │ JSON/行 @115200
┌───────────────────────────────┼──────────────┐
│  STM32F407VET6                │              │
│                               ▼              │
│  ┌──────────────┐    ┌────────────────────┐  │
│  │ UART1 RX     │───▶│ JSON Parser        │  │
│  │ (PA9/PA10)   │    │ (pc_monitor.c)     │  │
│  └──────────────┘    └────────┬───────────┘  │
│                               │              │
│  ┌──────────────┐    ┌───────▼───────────┐  │
│  │ pc_monitor   │◀───│ PcData Struct     │  │
│  │ (状态管理)    │    └────────┬───────────┘  │
│  └──────┬───────┘             │              │
│         │              ┌──────▼──────────┐  │
│         ▼              │  pet_render     │  │
│  ┌──────────────┐     │  (Claude像素形象  │  │
│  │ ILI9341 LCD  │◀────│   PC数据叠加)    │  │
│  │ 320×240      │     └─────────────────┘  │
│  └──────────────┘                          │
└─────────────────────────────────────────────┘
```

## 3. 模块设计

### 3.1 PC 端 (`pc_host/`)

| 文件 | 职责 | 关键技术 |
|------|------|---------|
| `pc_monitor_service.py` | Windows 服务主入口，生命周期管理 | pywin32, win32serviceutil |
| `sys_monitor.py` | 采集 CPU/GPU 温度、占用率、内存 | psutil, GPUtil |
| `status_reader.py` | 读取 Claude 状态文件，监听变化 | 文件轮询 / watchdog |
| `serial_sender.py` | 串口连接管理，JSON 序列化发送 | pyserial |
| `config.json` | 串口号、波特率、间隔、状态文件路径 | JSON |
| `install_service.bat` | 安装/卸载/启停 Windows 服务 | batch |

**数据发送间隔**: 每秒 1 次
**串口参数**: 115200-8N1（与现有 UART1 配置一致）

### 3.2 单片机端 (`app/`)

| 文件 | 状态 | 职责 |
|------|------|------|
| `pc_monitor.c/h` | **新增** | PC 状态数据结构定义、JSON 解析器、状态管理 API |
| `pet_render.c/h` | **修改** | 新增 Claude 像素形象绘制、PC 状态叠加渲染、状态表情切换 |
| `pet_command.c/h` | **修改** | 新增 `pc` 命令进入监控模式、`pet` 命令切回宠物模式 |
| `pet_core.c/h` | **修改** | PetData 新增 `is_pc_mode` 标志 |
| `main.c` | **修改** | 主循环集成 pc_monitor 数据接收和渲染 |

### 3.3 Claude Code 状态 Hook

在 `~/.claude/settings.json` 中配置 Hooks，自动将 Claude 工作状态写入约定文件。

状态流转:
```
idle → thinking → executing → waiting → done → idle
```

## 4. 串口通信协议

### 4.1 数据帧格式

每行一条完整 JSON，以 `\n` 结尾，UTF-8 编码。

```json
{"cpu_temp":55.0,"cpu_usage":30,"gpu_temp":62.0,"gpu_usage":45,"mem_usage":68,"claude":"thinking"}
```

### 4.2 字段定义

| 字段 | 类型 | 范围 | 说明 |
|------|------|------|------|
| `cpu_temp` | float | 0-120 | CPU 封装温度 (℃)，无传感器时为 -1 |
| `cpu_usage` | int | 0-100 | CPU 总利用率 (%) |
| `gpu_temp` | float | 0-120 | GPU 核心温度 (℃)，无独显时为 -1 |
| `gpu_usage` | int | 0-100 | GPU 利用率 (%)，无独显时为 -1 |
| `mem_usage` | int | 0-100 | 物理内存使用率 (%) |
| `claude` | string | 见下表 | Claude Code 当前状态 |

### 4.3 Claude 状态枚举

| 值 | 中文显示 | 触发时机 |
|----|---------|---------|
| `idle` | 就绪 | 无任务进行中 |
| `thinking` | 思考中 | 收到用户消息，正在分析 |
| `executing` | 执行中 | 正在执行工具/代码 |
| `waiting` | 待确认 | 等待用户确认/输入 |
| `done` | 项目完成 | 任务达成完成条件 |

## 5. 屏幕 UI 设计

### 5.1 整体布局 (320×240)

```
┌──────────────────────────────────┐  y=0
│  CPU  55°C  ████████░░ 30%      │  y=20  状态栏
│  GPU  62°C  ██████████ 45%      │  y=40
│  MEM        ██████████ 68%      │  y=60
├──────────────────────────────────┤  y=65  分割线
│                                  │
│         ◉    ◉                   │  y=85  Claude 脸
│           ╭──╮                   │  y=105
│          │    │                  │  y=130
│           ╰──╯                   │
│         ╰──────╯                 │  y=155
│                                  │
│        思考中...                  │  y=180 状态文字
│                                  │
├──────────────────────────────────┤  y=210 分割线
│     🅰 PC模式    🅱 宠物模式     │  y=225 底部提示
└──────────────────────────────────┘  y=240
```

### 5.2 Claude 状态 → 视觉表现

| Claude 状态 | 中文显示 | 宠物表情特征 | 屏幕特效 |
|------------|---------|-------------|---------|
| `idle` | 就绪 | 眨眼动画、微笑嘴型 | 无 |
| `thinking` | 思考中💭 | 眼睛左右移动、眉头微皱 | 淡蓝色呼吸光晕 (外围) |
| `executing` | 执行中⚡ | 瞳孔放大、数据流粒子 | 绿色脉冲边框 |
| `waiting` | 待确认❓ | 歪头、头顶"?" | 黄色温和闪烁 |
| `done` | 项目完成✅ | 弯眼笑、蹦跳动画 | 彩色粒子庆祝 |

### 5.3 Claude 像素形象设计

基于 Claude 品牌特征设计 96×96 像素画：
- 橙色/暖色系主色调
- 圆润的脸部轮廓
- 友好的大眼睛（可做表情变化）
- 标志性的微笑

## 6. 数据流

### 6.1 PC → STM32 (下行)

```
[System Monitor] ──(1s周期)──▶ [status_reader]
                                     │
[Claude Hooks] ──(事件驱动)──▶ [claude_status.txt]
                                     │
         ┌───────────────────────────┘
         ▼
   [serial_sender] ── JSON Line ──▶ [UART1] ──▶ [pc_monitor JSON Parser]
                                                      │
                                                      ▼
                                               [PcData struct]
                                                      │
                                              ┌───────┴───────┐
                                              ▼               ▼
                                       [pet_render]    [pet_core]
                                       (LCD绘制)       (宠物状态联动)
```

### 6.2 单片机内部数据流

```
main.c 主循环 (200ms周期)
  │
  ├── Cmd_Process()          ← 处理串口命令 (含 JSON 行)
  │     └── PC_Monitor_Parse()  ← JSON 解析
  │           └── g_pc_data 更新
  │
  ├── Pet_Tick100ms()        ← 宠物逻辑 (PC模式下暂停衰减)
  │
  └── Render_DrawAll()       ← LCD 渲染
        ├── 如果在 PC 模式:
        │     ├── Render_PCStatusBar()    ← 顶部状态条
        │     ├── Render_ClaudeFace()     ← Claude 像素形象
        │     ├── Render_StatusText()     ← 底部状态文字
        │     └── Render_Particles()      ← 特效粒子
        └── 如果在 Pet 模式:
              └── (原有宠物渲染)
```

## 7. 模式切换

- **默认启动**: 显示 Claude + PC 监控模式
- **串口命令 `pet`**: 切换到宠物养成模式
- **串口命令 `pc`**: 切换到 PC 监控模式
- **按键 KEY1 长按 2 秒**: 切换模式
- **LED 指示**: 蓝灯 = PC 模式，红灯 = 宠物模式

## 8. 错误处理

| 场景 | 处理方式 |
|------|---------|
| 串口断开 | STM32 LCD 显示 "串口断开"，Claude 表情变难过，等待重连 |
| JSON 解析失败 | 丢弃该帧，记录错误计数，连续 5 帧错误显示 "数据错误" |
| GPU 无传感器 | 显示 "- -"，不显示 GPU 行 |
| CPU 温度不可用 | 显示 "- -" |
| Claude 状态文件不存在 | 默认显示 `idle` |
| PC 端串口打开失败 | 服务记录日志，定期重试，LCD 显示等待连接 |

## 9. 文件清单

```
D:\Apps\pixel_pet\
├── pc_host\                         # PC 端程序 (新增)
│   ├── pc_monitor_service.py        # Windows 服务主程序
│   ├── sys_monitor.py               # 系统硬件信息采集
│   ├── status_reader.py             # Claude 状态文件读取
│   ├── serial_sender.py             # 串口发送模块
│   ├── config.json                  # 配置文件
│   ├── requirements.txt             # Python 依赖
│   └── install_service.bat          # 服务安装脚本
├── app\                             # 单片机应用层
│   ├── pc_monitor.c                 # PC 监控模块 (新增)
│   ├── pc_monitor.h                 # PC 监控头文件 (新增)
│   ├── pet_core.c/h                 # 宠物核心 (修改)
│   ├── pet_render.c/h               # 渲染模块 (修改)
│   ├── pet_command.c/h              # 命令处理 (修改)
│   └── pet_save.c/h                 # 存档模块 (不变)
├── BSP\                             # 底层驱动 (不变)
├── User\                            # 用户代码
│   └── main.c                       # 主程序 (修改)
├── Core\                            # CMSIS (不变)
├── STM32F4xx_StdPeriph_Driver\      # 标准外设库 (不变)
└── docs\
    └── superpowers\
        └── specs\
            └── 2026-06-03-pixel-claude-pet-design.md  # 本文档
```

## 10. 技术决策

| 决策 | 选择 | 原因 |
|------|------|------|
| 串口协议 | JSON 文本行 | 易调试、易扩展、容错好 |
| 单片机 JSON 解析 | 手写轻量解析器 | Flash/RAM 受限，不用 cJSON |
| PC 端语言 | Python | 库丰富 (psutil/GPUtil/pyserial/pywin32) |
| GPU 监控库 | GPUtil | 简单够用，NVIDIA GPU 支持好 |
| 部署方式 | Windows Service | 开机自启，静默运行，无 UI 干扰 |
| 状态传递 | 文件轮询 | Claude Code Hook 无法直接串口通信，文件是通用 IPC |
| 保留宠物功能 | 双模式切换 | 不丢失已有功能，增加复用价值 |
