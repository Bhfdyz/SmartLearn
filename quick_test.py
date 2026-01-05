#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SmartLearn 快速注册测试
只测试一个正常的注册用例，用于快速验证
"""

import socket
import json
import time

SERVER_HOST = "127.0.0.1"
SERVER_PORT = 8080

# 生成唯一用户名
unique_username = f"user{int(time.time() * 1000) % 1000000}"

request_data = {
    "type": "RegisterType",
    "username": unique_username,
    "password": "Pass1234",
    "email": "test@example.com",
    "phone": "13800138000",
    "grade": "2023",
    "major": "计算机科学与技术",
    "role": "student"
}

print("=" * 50)
print("   快速注册测试")
print("=" * 50)
print(f"用户名: {unique_username}")
print(f"密码: Pass1234")
print("-" * 50)

try:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    sock.connect((SERVER_HOST, SERVER_PORT))

    json_str = json.dumps(request_data)
    sock.sendall(json_str.encode('utf-8'))

    response_data = sock.recv(1024)
    sock.close()

    response = json.loads(response_data.decode('utf-8'))

    print(f"响应: {json.dumps(response, ensure_ascii=False, indent=2)}")
    print("-" * 50)

    if response.get("status") == "success":
        print("✓ 注册成功！")
    else:
        print(f"✗ 注册失败: {response.get('message')}")

except Exception as e:
    print(f"✗ 错误: {e}")
    print("请确保服务器已启动！")
