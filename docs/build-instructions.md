# Pixel Claude Pet — 编译与烧录指南

## 前提条件

- Keil MDK 5.27+ (ARM Compiler 6)
- STM32F407VET6 开发板 + ILI9341 2.8" LCD
- USB-TTL 串口连接

## 新增文件

以下文件已在代码中创建，需要添加到 Keil 工程：

| 文件 | 位置 | 操作 |
|------|------|------|
| pc_monitor.c | app/pc_monitor.c | 添加到 APP 组 |
| pc_monitor.h | app/pc_monitor.h | 头文件，已通过 include 引用 |

## 在 Keil MDK 中添加文件

1. 打开工程文件：`D:\Apps\pixel_pet\STM32F407.uvprojx`
2. 在 Project 窗口中，右键点击 **APP** 组
3. 选择 **Add Existing Files to Group 'APP'**
4. 浏览到 `D:\Apps\pixel_pet\app\pc_monitor.c`，选中并点击 **Add**
5. 点击 **Close**

## 验证包含路径

1. 点击 **Project → Options for Target → C/C++**
2. 确认 `..\app` 已添加到 **Include Paths** 列表
3. 如未添加，点击 `...` 按钮，添加 `..\app` 路径

## 编译

- 按 **F7** 编译整个工程
- 预期结果：**0 Errors, 0 Warnings**

## 烧录

- 按 **F8** 将固件烧录到 STM32F407VET6
- 烧录完成后，LCD 应显示 "Pixel Claude Pet" 欢迎画面
- 默认进入 PC Monitor 模式，等待串口数据

## 修改的文件清单

| 文件 | 修改内容 |
|------|---------|
| app/pc_monitor.h | 新建 — PC 状态数据结构、API 声明 |
| app/pc_monitor.c | 新建 — JSON 解析器、状态管理 |
| app/pet_core.h | 添加 `is_pc_mode` 字段 |
| app/pet_core.c | 初始化 `is_pc_mode = 1` |
| app/pet_render.h | 添加 PC 模式常量、渲染 API |
| app/pet_render.c | 添加 Claude 角色绘制、PC 模式渲染 |
| app/pet_command.c | JSON 路由、pc/pet 命令、缓冲区增大 |
| User/main.c | PC Monitor 集成、LED 指示、按键门控 |
