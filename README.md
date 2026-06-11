# Pixel Claude Pet

> STM32F407 桌面副屏：PC 状态监控 + Claude 角色 + 万年历 + 番茄钟

![Platform](https://img.shields.io/badge/Platform-STM32F407VET6-blue)
![LCD](https://img.shields.io/badge/LCD-ILI9341%202.8%22-orange)
![Python](https://img.shields.io/badge/Python-3.x-green)
![GitHub stars](https://img.shields.io/github/stars/laizhenchuan/PixelClaudePet)

## 页面

| 页面 | 内容 |
|------|------|
| **PC 监控** | CPU/GPU 温度占用率、内存使用率、Claude 状态、实时时钟 |
| **万年历** | 公历日历 + DHT11 室内温湿度 + 实时时钟 |
| **番茄钟** | 25 分钟工作 / 5 分钟休息，番茄动画、跳动指示器 |

## 角色

Claude 珊瑚橙方块机器人，5 种表情随状态变化：

| 状态 | 显示 |
|------|------|
| Ready | 正常眼睛、温和微笑 |
| Thinking | 皱眉、眼向上 |
| Working | 正常瞳孔 |
| Waiting | 歪头、不对称眼 |
| Done! | 弯眼笑 |

## 硬件

- 魔女科技 STM32F407VET6 开发板
- ILI9341 2.8 寸 LCD（FSMC 接口）
- DHT11 温湿度模块（PE3）
- 板载 W25Q128 Flash、USB-TTL（USART1 PA9/PA10）

## 按键

| 按键 | PC 监控页 | 番茄钟页 |
|------|----------|---------|
| KEY1 | ⏯️ 播放/暂停 | ▶️ 开始/暂停 |
| KEY2 | ⏭️ 下一首 | 🔄 重置 |
| KEY3 | 📄 切换页面（PC → 日历 → 番茄钟） |

## PC 端

### 安装

```bash
pip install psutil pyserial
```

### 启动

双击 `pc_host/monitor.pyw` 静默后台运行（无窗口，无弹窗）。

开机自启：

```powershell
copy pc_host\PixelClaudePet.vbs "%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\"
```

### 配置

编辑 `pc_host/config.json`：

```json
{
  "serial": { "port": "COM3", "baudrate": 115200 },
  "monitor": { "interval_sec": 1.0 },
  "claude": {
    "status_file": "C:\\Users\\你的用户名\\.claude\\claude_status.txt",
    "default_status": "idle"
  }
}
```

### Claude Code 状态同步

`~/.claude/settings.json` 配置 hooks：

```json
{
  "hooks": {
    "UserPromptSubmit": [{"matcher":"","hooks":[{"type":"command","command":"echo thinking > C:/Users/.../.claude/claude_status.txt"}]}],
    "PreToolUse": [{"matcher":"","hooks":[{"type":"command","command":"echo executing > C:/Users/.../.claude/claude_status.txt"}]}],
    "Stop": [{"matcher":"","hooks":[{"type":"command","command":"echo idle > C:/Users/.../.claude/claude_status.txt"}]}]
  }
}
```

## 单片机编译

1. Keil MDK 5 打开 `STM32F407.uvprojx`
2. F7 编译，F8 烧录

## 数据协议

PC → STM32（115200-8N1，每秒 1 帧）：

```json
{
  "date": "2026-06-07",
  "time": "21:35:42",
  "weekday": 3,
  "cpu_temp": 55.0,
  "cpu_usage": 30,
  "gpu_temp": 62.0,
  "gpu_usage": 45,
  "mem_usage": 68,
  "claude": "thinking"
}
```

STM32 → PC（按键）：

```
KEY:PAUSE   KEY:NEXT
```

PC 收到后模拟媒体键控制音乐播放。

## 项目结构

```
├── User/main.c               # 主程序
├── app/
│   ├── pc_monitor.c/h        # PC 数据解析、状态管理
│   ├── pet_render.c/h        # LCD 渲染（角色、状态栏、日历、番茄钟）
│   ├── pet_command.c/h       # 串口命令
│   └── pet_core.c/h          # 数据结构
├── BSP/
│   ├── LCD_ILI9341/          # LCD 驱动 + 字库
│   ├── DHT11/                # 温湿度传感器
│   ├── W25Q128/              # Flash 存储
│   └── UART/ LED/ KEY/       # 外设驱动
├── pc_host/
│   ├── monitor.pyw           # 主服务（静默后台）
│   ├── sys_monitor.py        # 硬件监控（psutil + nvidia-smi）
│   ├── serial_sender.py      # 串口通信
│   ├── status_reader.py      # Claude 状态文件读取
│   └── config.json           # 配置文件
└── docs/                     # 设计文档
```

## License

MIT
