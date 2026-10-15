# TypingSimulator

一个极简的 Windows 打字模拟小工具。写好文本，按下全局热键，自动将内容逐字键入到当前活动窗口。
可以破解大多数网页、软件的禁止粘贴。
软件的外观伪装了记事本，不要让你的老师发现这个开源项目哦。
这个项目的readme是用ai写的。

> 纯 Win32 API / MSVC 命令行编译 / 无第三方依赖 / 单文件可执行

![Platform](https://img.shields.io/badge/platform-Windows-blue)
![Arch](https://img.shields.io/badge/arch-x86%20%7C%20x64-lightgrey)
![License](https://img.shields.io/badge/license-MIT-green)

## ✨ 功能

| 特性 | 说明 |
|---|---|
| 全局热键触发 | 默认 `Ctrl + Alt + F8`，一键键入编辑框全文 |
| 托盘常驻 | 关闭窗口自动隐藏到系统托盘，不占任务栏 |
| 自定义快捷键 | 菜单「编辑 → 设置快捷键」，按下组合键即时生效 |
| 单实例运行 | 重复启动会自动激活已存在的窗口 |
| Unicode 键入 | 基于 `SendInput` + Unicode 方式，中文内容正常输出 |
| 零运行时依赖 | 静态链接，拷贝 exe 即可在目标机器直接运行 |

## 📦 编译

### 前置条件

- Visual Studio 2022（安装「使用 C++ 的桌面开发」工作负载）
- 打开 x86 / x64 Native Tools Command Prompt for VS 2022

### 项目结构

```
TypingSimulator/
├── main.cpp       # 程序主体
├── app.rc         # 资源描述
├── app.ico        # 托盘图标
└── resource.h     # 资源 ID 定义
```

### 命令行编译（x86）

```bat
rc /nologo /fo app.res app.rc
cl /nologo /EHsc /W4 /O2 /utf-8 main.cpp app.res /Fe:TypingSimulator.exe /link user32.lib gdi32.lib shell32.lib advapi32.lib
```

### 编译 64 位版本

关闭当前窗口，重新打开 **x64 Native Tools Command Prompt**，执行完全相同的命令即可（建议输出名改为 `TypingSimulator64.exe` 方便区分）。

### 一键脚本（可选）

创建一个 `build.bat` 后输入下面的内容，双击即可完成资源编译 + 链接：

```bat
@echo off
rc /nologo /fo app.res app.rc || exit /b 1
cl /nologo /EHsc /W4 /O2 /utf-8 main.cpp app.res /Fe:TypingSimulator.exe /link user32.lib gdi32.lib shell32.lib advapi32.lib
```

## 🖱️ 使用方式

1. 启动 `TypingSimulator.exe`，主窗口出现（标题显示为「记事本」风格）
2. 在多行编辑框中粘贴 / 输入你想要自动键入的文本
3. 把光标切到你想要填入的目标窗口（浏览器、聊天框、IDE 等）
4. 按下 `Ctrl + Alt + F8`，程序会逐字符模拟键入
5. 关闭主窗口不会退出程序，图标保留在托盘区
   - 右键托盘 → **显示窗口** / **退出**

### 修改触发热键

菜单栏：`编辑 → 设置快捷键(&S)`

按下任意组合键（如 `Ctrl+Shift+T`）即可完成绑定，状态栏会短暂提示当前快捷键。

## ⚠️ 注意事项

- 程序通过 `SendInput` 模拟输入，当你以管理员身份运行目标窗口时，本程序也建议以管理员身份启动，否则按键可能被系统拦截
- 键入过程中不要移动焦点，否则文本会打入其他窗口
- 回车符会被转换为 `Enter` 键，空行 / 纯 `\r` 会被自动跳过

## 🛠️ 技术栈

- Language: C++17
- API: Win32 (`user32` / `gdi32` / `shell32`)
- Build: MSVC `cl.exe` + `rc.exe`，无 CMake 强制依赖

## 📄 License

MIT © 2026（虽然都用mit了，但是我还是想说，倒狗没有木琴）