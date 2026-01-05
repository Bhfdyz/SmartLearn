@echo off
chcp 65001 >nul
echo ============================================
echo    SmartLearn 一键测试脚本
echo ============================================
echo.

REM 设置路径
set SERVER_DIR=D:\code\c++\Qt\SmartLearn-server
set CLIENT_DIR=D:\code\c++\Qt\SmartLearn-main

echo [1/3] 编译服务器...
cd /d "%SERVER_DIR%"
qmake SmartLearnServer.pro
if errorlevel 1 (
    echo 错误: qmake 失败！
    pause
    exit /b 1
)
mingw32-make
if errorlevel 1 (
    echo 错误: 编译失败！
    pause
    exit /b 1
)
echo 服务器编译完成
echo.

echo [2/3] 启动服务器 (后台运行)...
start /B "" "%SERVER_DIR%\debug\SmartLearnServer.exe" 2>nul
timeout /t 2 /nobreak >nul
echo 服务器已启动
echo.

echo [3/3] 运行测试...
python "%SERVER_DIR%\test_register.py"

echo.
echo ============================================
echo 测试完成！
echo ============================================
pause
