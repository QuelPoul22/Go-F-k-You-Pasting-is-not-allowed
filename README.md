# TypingSimulator

一个极简的 Windows 打字模拟小工具。写好文本，按下全局热键，自动将内容逐字键入到当前活动窗口。
可以破解大多数网页、软件的禁止粘贴。
软件的外观伪装了记事本，不要让你的老师发现这个开源项目哦。
这个项目的 readme 是用 ai 写的。

> 纯 Win32 API / MSVC 命令行编译 / 无第三方依赖 / 单文件可执行

![Platform](https://img.shields.io/badge/platform-Windows-blue)
![Arch](https://img.shields.io/badge/arch-x86%20%7C%20x64-lightgrey)
![License](https://img.shields.io/badge/license-MIT-green)

## ✨ 功能

| 特性 | 说明 |
|---|---|
| 全局热键触发 | 默认 `Ctrl + Alt + F8`，一键键入编辑框全文 |
| 托盘常驻 | 关闭窗口自动隐藏到系统托盘，不占任务栏 |
| 自定义快捷键 | 菜单「编辑 → 设置触发快捷键」，按下组合键即时生效 |
| 单实例运行 | 重复启动会自动激活已存在的窗口 |
| Unicode 键入 | 基于 `SendInput` + Unicode 方式，中文内容正常输出 |
| 零运行时依赖 | 静态链接，拷贝 exe 即可在目标机器直接运行 |
| 仿记事本界面 | 纯白背景、无边框编辑框、系统状态栏、完整菜单栏，外观与真记事本高度接近 |

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
cl /nologo /EHsc /W4 /O2 /utf-8 /DUNICODE /D_UNICODE main.cpp app.res /Fe:TypingSimulator.exe /link user32.lib gdi32.lib shell32.lib advapi32.lib comctl32.lib
```

> 注意：链接时比旧版多了一个 `comctl32.lib`，因为界面用到了系统状态栏控件。

### 编译 64 位版本

关闭当前窗口，重新打开 **x64 Native Tools Command Prompt**，执行完全相同的命令即可（建议输出名改为 `TypingSimulator64.exe` 方便区分）。

### 一键脚本（可选）

创建一个 `build.bat` 后输入下面的内容，双击即可完成资源编译 + 链接，并清理中间文件：

```bat
@echo off
rc /nologo /fo app.res app.rc || exit /b 1
cl /nologo /EHsc /W4 /O2 /utf-8 /DUNICODE /D_UNICODE main.cpp app.res ^
   /Fe:TypingSimulator.exe ^
   /link user32.lib gdi32.lib shell32.lib advapi32.lib comctl32.lib || exit /b 1
del /q main.obj app.res 2>nul
echo Build OK: TypingSimulator.exe
```

## 🖱️ 使用方式

1. 启动 `TypingSimulator.exe`，主窗口出现（外观与 Windows 记事本几乎一致）
2. 在多行编辑框中粘贴 / 输入你想要自动键入的文本
3. 把光标切到你想要填入的目标窗口（浏览器、聊天框、IDE 等）
4. 按下 `Ctrl + Alt + F8`，程序会逐字符模拟键入
5. 关闭主窗口不会退出程序，图标保留在托盘区
   - 右键托盘 → **显示窗口** / **退出**

### 修改/查看触发热键

菜单栏：`编辑 → 设置触发快捷键(&S)...`
菜单栏：`查看 → 当前快捷键(C)...`
按下任意组合键（如 `Ctrl+Shift+T`）即可完成绑定，状态栏会短暂提示当前快捷键。

### 界面说明

- 状态栏显示「第 x 行，第 y 列 / 100% / Windows (CRLF) / UTF-8」，与记事本一致
- 光标移动时行列会实时刷新（内部用 200ms 定时器轮询实现，标准 EDIT 控件不支持选区变化通知）
- 菜单栏项名与记事本对齐，但除「设置触发快捷键」「当前快捷键」「退出」外，其余为视觉占位，点击无动作

## ⚠️ 注意事项

- 程序通过 `SendInput` 模拟输入，当你以管理员身份运行目标窗口时，本程序也建议以管理员身份启动，否则按键可能被系统拦截
- 键入过程中不要移动焦点，否则文本会打入其他窗口
- 回车符会被转换为 `Enter` 键，空行 / 纯 `\r` 会被自动跳过
- 若编译后生成 `TypingSimulator.exe.manifest`，请与 exe 放在同一目录，否则 Common Controls v6 视觉样式不会生效（状态栏、菜单会退化为旧样式）

## 🛠️ 技术栈

- Language: C++17
- API: Win32 (`user32` / `gdi32` / `shell32` / `comctl32`)
- Build: MSVC `cl.exe` + `rc.exe`，无 CMake 强制依赖
- 视觉样式：Common Controls v6（通过链接器 manifest 启用）

## 📝 更新日志

### v0.0.3
- 界面重做，更接近真实记事本：
  - 窗口背景改为纯白，编辑框去掉凹陷边框、左右各留 8px 内边距
  - 启用系统状态栏，显示「第 x 行，第 y 列 / 100% / Windows (CRLF) / UTF-8」
  - 状态栏行列随光标移动实时更新
  - 菜单栏补全为 文件 / 编辑 / 格式 / 查看 / 帮助，项名与记事本一致
  - 启用 Common Controls v6 视觉样式（需链接 `comctl32.lib`）

### v0.0.2
- 支持自定义全局热键
- 托盘常驻、单实例运行

### v0.0.1
- 初版：全局热键触发自动键入

## 📄 License

MIT © 2026（虽然都用 mit 了，但是我还是想说，倒狗没有木琴）
