# Pixel Claude Pet

> STM32F407 桌面副屏：实时 PC 状态监控 + Claude 像素角色 + 万年历

![Platform](https://img.shields.io/badge/Platform-STM32F407VET6-blue)
![LCD](https://img.shields.io/badge/LCD-ILI9341%202.8%22-orange)
![Python](https://img.shields.io/badge/Python-3.x-green)

## 功能

| 页面 | 内容 |
|------|------|
| **PC 监控** | CPU/GPU 温度占用率、内存使用率、Claude Code 工作状态 |
| **万年历** | 公历日历 + DHT11 温湿度 + 实时时钟 |
| **角色** | Claude 像素方块机器人，5 种表情随状态变化 |

## 硬件

- 魔女科技 STM32F407VET6 开发板
- ILI9341 2.8 寸 LCD（FSMC）
- DHT11 温湿度模块（PE3）
- 板载 W25Q128 Flash、USB-TTL

## 按键

| 按键 | 功能 |
|------|------|
| KEY1 | ⏯️ 播放/暂停 |
| KEY2 | ⏭️ 下一首 |
| KEY3 | 📅 PC 监控 ⇄ 万年历 |

## PC 端使用

### 安装依赖

```bash
pip install psutil pyserial GPUtil
```

### 启动

双击 `pc_host/monitor.pyw` 静默启动（无窗口）。

或设为开机自启：

```powershell
# 创建启动快捷方式
copy pc_host\PixelClaudePet.vbs "%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\"
```

### Claude Code 状态同步

在 `~/.claude/settings.json` 中配置 hooks：

```json
{
  "hooks": {
    "UserPromptSubmit": [{"matcher":"","hooks":[{"type":"command","command":"echo thinking > ~/.claude/claude_status.txt"}]}],
    "PreToolUse": [{"matcher":"","hooks":[{"type":"command","command":"echo executing > ~/.claude/claude_status.txt"}]}],
    "Stop": [{"matcher":"","hooks":[{"type":"command","command":"echo idle > ~/.claude/claude_status.txt"}]}]
  }
}
```

## 单片机编译

1. Keil MDK 5 打开 `STM32F407.uvprojx`
2. F7 编译，F8 烧录

## 数据协议

PC → STM32 每秒发送一行 JSON（115200-8N1）：

```json
{"date":"2026-06-06","time":"21:35:42","weekday":3,"cpu_temp":55.0,"cpu_usage":30,"gpu_temp":62.0,"gpu_usage":45,"mem_usage":68,"claude":"thinking"}
```

STM32 → PC 按键指令：

```
KEY:PAUSE   KEY:NEXT   KEY:PREV
```

## 状态流转

```
用户发消息 → Thinking → 执行工具 → Working → 弹确认 → Waiting → 结束 → Ready
```

## 项目结构

```
├── User/main.c              # 主程序
├── app/
│   ├── pc_monitor.c/h       # PC 数据解析、状态管理
│   ├── pet_render.c/h       # LCD 渲染（角色、状态栏、日历）
│   ├── pet_command.c/h      # 串口命令
│   └── pet_core.c/h         # 数据结构
├── BSP/
│   ├── LCD_ILI9341/         # LCD 驱动 + 字库
│   ├── DHT11/               # 温湿度传感器
│   ├── W25Q128/             # Flash 存储
│   └── UART/ LED/ KEY/      # 外设驱动
├── pc_host/
│   ├── monitor.pyw          # 主服务（静默后台）
│   ├── sys_monitor.py       # 硬件监控
│   ├── serial_sender.py     # 串口通信
│   └── status_reader.py     # Claude 状态
└── docs/                    # 设计文档
```

## License

MIT
