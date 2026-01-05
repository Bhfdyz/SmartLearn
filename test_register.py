#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SmartLearn 注册功能自动测试脚本
运行此脚本前，请确保服务器已经启动
"""

import socket
import json
import time
import sys

# 服务器配置
SERVER_HOST = "127.0.0.1"
SERVER_PORT = 8080

# 颜色输出
class Colors:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    RESET = '\033[0m'

def print_success(msg):
    print(f"{Colors.GREEN}✓ {msg}{Colors.RESET}")

def print_error(msg):
    print(f"{Colors.RED}✗ {msg}{Colors.RESET}")

def print_info(msg):
    print(f"{Colors.BLUE}ℹ {msg}{Colors.RESET}")

def print_test(msg):
    print(f"\n{Colors.YELLOW}▶ {msg}{Colors.RESET}")

def send_register_request(data):
    """发送注册请求到服务器"""
    try:
        # 创建 TCP socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)  # 5秒超时
        sock.connect((SERVER_HOST, SERVER_PORT))

        # 发送 JSON 请求
        json_str = json.dumps(data)
        print_info(f"发送请求: {json_str}")
        sock.sendall(json_str.encode('utf-8'))

        # 接收响应
        response_data = b""
        while True:
            chunk = sock.recv(1024)
            if not chunk:
                break
            response_data += chunk

        sock.close()

        # 解析响应
        if response_data:
            response = json.loads(response_data.decode('utf-8'))
            return response
        return None

    except socket.timeout:
        print_error("连接超时！请确保服务器已启动")
        return None
    except ConnectionRefusedError:
        print_error("连接被拒绝！请确保服务器已启动")
        return None
    except Exception as e:
        print_error(f"发生错误: {e}")
        return None

def test_case(name, data, expected_status="success"):
    """执行单个测试用例"""
    print_test(name)
    print(f"   数据: {data}")

    response = send_register_request(data)

    if response is None:
        print_error("测试失败 - 无响应")
        return False

    print(f"   响应: {response}")

    # 检查响应类型
    if response.get("type") != "RegisterResponse":
        print_error(f"响应类型错误: {response.get('type')}")
        return False

    # 检查状态
    if response.get("status") == expected_status:
        print_success(f"测试通过 - {response.get('message')}")
        return True
    else:
        print_error(f"测试失败 - 期望状态: {expected_status}, 实际: {response.get('status')}")
        return False

def check_server_listening():
    """检查服务器是否正在监听端口"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(1)
        result = sock.connect_ex((SERVER_HOST, SERVER_PORT))
        sock.close()
        return result == 0
    except:
        return False

def main():
    print("=" * 60)
    print("       SmartLearn 注册功能自动测试")
    print("=" * 60)

    # 首先检查服务器是否在监听
    print_info(f"检查服务器 {SERVER_HOST}:{SERVER_PORT}...")
    if not check_server_listening():
        print_error("服务器未在监听！请先启动服务器")
        print("\n启动方法:")
        print("  方法1: 运行 debug/SmartLearnServer.exe")
        print("  方法2: 在Qt Creator中打开项目并运行")
        return 1
    print_success("服务器正在运行")

    # 测试用例列表
    test_cases = [
        {
            "name": "测试1: 正常注册 (用户名: test001)",
            "data": {
                "type": "RegisterType",
                "username": f"test{int(time.time()) % 10000}",
                "password": "Pass1234",
                "email": f"test{int(time.time()) % 10000}@example.com",
                "phone": "13800138000",
                "grade": "2023",
                "major": "计算机科学与技术",
                "role": "student"
            },
            "expected": "success"
        },
        {
            "name": "测试2: 用户名过短 (少于4字符)",
            "data": {
                "type": "RegisterType",
                "username": "abc",
                "password": "Pass1234",
                "email": "",
                "phone": "",
                "grade": "",
                "major": "",
                "role": "student"
            },
            "expected": "error"
        },
        {
            "name": "测试3: 用户名过长 (超过20字符)",
            "data": {
                "type": "RegisterType",
                "username": "a" * 25,
                "password": "Pass1234",
                "email": "",
                "phone": "",
                "grade": "",
                "major": "",
                "role": "student"
            },
            "expected": "error"
        },
        {
            "name": "测试4: 密码过短 (少于8字符)",
            "data": {
                "type": "RegisterType",
                "username": f"user{int(time.time()) % 10000}",
                "password": "pass123",
                "email": "",
                "phone": "",
                "grade": "",
                "major": "",
                "role": "student"
            },
            "expected": "error"
        },
        {
            "name": "测试5: 密码纯数字 (缺少字母)",
            "data": {
                "type": "RegisterType",
                "username": f"user{int(time.time()) % 10000}",
                "password": "12345678",
                "email": "",
                "phone": "",
                "grade": "",
                "major": "",
                "role": "student"
            },
            "expected": "error"
        },
        {
            "name": "测试6: 邮箱格式错误",
            "data": {
                "type": "RegisterType",
                "username": f"user{int(time.time()) % 10000}",
                "password": "Pass1234",
                "email": "invalid-email",
                "phone": "",
                "grade": "",
                "major": "",
                "role": "student"
            },
            "expected": "error"
        },
        {
            "name": "测试7: 手机号格式错误",
            "data": {
                "type": "RegisterType",
                "username": f"user{int(time.time()) % 10000}",
                "password": "Pass1234",
                "email": "",
                "phone": "12345",
                "grade": "",
                "major": "",
                "role": "student"
            },
            "expected": "error"
        },
    ]

    # 执行所有测试
    passed = 0
    failed = 0

    for test in test_cases:
        time.sleep(0.5)  # 避免请求过快
        if test_case(test["name"], test["data"], test["expected"]):
            passed += 1
        else:
            failed += 1

    # 输出总结
    print("\n" + "=" * 60)
    print(f" 测试完成: {Colors.GREEN}{passed} 通过{Colors.RESET} | {Colors.RED}{failed} 失败{Colors.RESET}")
    print("=" * 60)

    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
