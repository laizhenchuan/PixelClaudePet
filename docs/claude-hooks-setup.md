# Claude Code Hooks — 状态文件配置

## 概述

Pixel Claude Pet 通过读取 `~/.claude/claude_status.txt` 文件来显示 Claude 的工作状态。

## 状态值

| 值 | 中文含义 | LCD 显示 |
|----|---------|---------|
| idle | 就绪 | Ready |
| thinking | 思考中 | Thinking... |
| executing | 执行中 | Working... |
| waiting | 待确认 | Waiting... |
| done | 项目完成 | Done! |

## 手动更新状态

在终端中执行以下命令来更新状态：

```bash
echo idle > %USERPROFILE%\.claude\claude_status.txt
echo thinking > %USERPROFILE%\.claude\claude_status.txt
echo executing > %USERPROFILE%\.claude\claude_status.txt
echo waiting > %USERPROFILE%\.claude\claude_status.txt
echo done > %USERPROFILE%\.claude\claude_status.txt
```

## Claude Code Hooks 自动更新（未来功能）

在 `~/.claude/settings.json` 中配置 hooks 可实现自动状态同步：

```json
{
  "hooks": {
    "PostToolUse": [
      {
        "matcher": "",
        "command": "echo idle > %USERPROFILE%\\.claude\\claude_status.txt"
      }
    ]
  }
}
```

**注意：** Claude Code Hooks 的精确事件匹配可能需要根据版本调整。
当前推荐手动管理状态文件，或编写批处理脚本辅助。
