# SmartLearn Server - 知识库模块代码解析

## 概述

SmartLearn Server 是一个基于 Qt 的 TCP 服务器，为 SmartLearn 客户端提供后端服务。本文档主要分析 `mainwindow.cpp` 中**知识库相关**的代码实现。

---

## 目录结构

```
SmartLearn-server/
├── main.cpp              # 程序入口
├── mainwindow.h/cpp      # 主窗口 + TCP 服务器逻辑
├── databasemanager.h/cpp # SQLite 数据库管理（单例模式）
├── user.h                # 用户数据结构
├── config.h              # 配置文件
└── smartlearn.db         # SQLite 数据库
```

---

## 知识库相关接口

### 请求类型定义

在 `config.h` 中定义的请求类型常量：

```cpp
#define SaveKnowledgeType "SaveKnowledge"
#define GetKnowledgeType  "GetKnowledge"
```

### 1. 保存知识库

**接口**: `SaveKnowledgeType`

**请求格式**:
```json
{
  "type": "SaveKnowledge",
  "username": "用户名",
  "learning_goal": "学习目标",
  "knowledge_points": ["知识点1", "知识点2", ...]
}
```

**代码位置**: `mainwindow.cpp:272-352`

**处理函数**: `MainWindow::handleSaveKnowledgeRequest()`

**处理流程**:

```
1. 解析 JSON 请求
   └─> 提取 username, learning_goal, knowledge_points

2. 验证用户
   └─> 通过 username 获取 user_id
   └─> 用户不存在则返回错误

3. 开启数据库事务
   └─> db.transaction()
   └─> 避免数据库锁定问题

4. 保存学习目标
   └─> UPDATE users SET learning_goal = :goal WHERE user_id = :user_id

5. 保存知识点（追加模式）
   └─> 遍历 knowledge_points 数组
   └─> 检查知识点是否已存在
   └─> 不存在则 INSERT，已存在则跳过

6. 提交/回滚事务
   └─> 有错误 -> rollback()
   └─> 全部成功 -> commit()
```

**关键代码片段**:

```cpp
// 知识点去重逻辑 (mainwindow.cpp:312-338)
for (const QJsonValue &value : knowledgeArray) {
    QString point = value.toString();

    // 先检查是否已存在
    QSqlQuery checkQuery(db);
    checkQuery.prepare(
        "SELECT COUNT(*) FROM user_knowledge "
        "WHERE user_id = :user_id AND knowledge_point = :point"
    );
    checkQuery.bindValue(":user_id", user.user_id);
    checkQuery.bindValue(":point", point);

    if (checkQuery.exec() && checkQuery.next() && checkQuery.value(0).toInt() > 0) {
        qDebug() << "知识点已存在，跳过:" << point;
        continue;
    }

    // 插入新知识点
    QSqlQuery insertQuery(db);
    insertQuery.prepare(
        "INSERT INTO user_knowledge (user_id, knowledge_point) "
        "VALUES (:user_id, :point)"
    );
    insertQuery.bindValue(":user_id", user.user_id);
    insertQuery.bindValue(":point", point);
    insertQuery.exec();
}
```

---

### 2. 获取知识库

**接口**: `GetKnowledgeType`

**请求格式**:
```json
{
  "type": "GetKnowledge",
  "username": "用户名"
}
```

**代码位置**: `mainwindow.cpp:354-374`

**处理函数**: `MainWindow::handleGetKnowledgeRequest()`

**处理流程**:

```
1. 解析 JSON 请求
   └─> 提取 username

2. 验证用户
   └─> 通过 username 获取 user_id
   └─> 用户不存在则返回错误

3. 查询知识点
   └─> DatabaseManager::getUserKnowledgePoints(user_id)

4. 返回响应
   └─> 包含知识点数组的 JSON 响应
```

---

### 3. 响应格式

**响应类型**: `KnowledgeResponse`

**成功响应**:
```json
{
  "type": "KnowledgeResponse",
  "status": "success",
  "message": "知识库保存成功",
  "knowledge_points": ["知识点1", "知识点2", ...]
}
```

**错误响应**:
```json
{
  "type": "KnowledgeResponse",
  "status": "error",
  "message": "用户不存在"
}
```

**代码位置**: `mainwindow.cpp:376-409`

**处理函数**: `MainWindow::sendKnowledgeResponse()`

---

## 数据库表结构

### user_knowledge 表

| 字段 | 类型 | 说明 |
|------|------|------|
| id | INTEGER | 主键 |
| user_id | INTEGER | 用户 ID（外键） |
| knowledge_point | TEXT | 知识点内容 |
| mastery_level | INTEGER | 掌握等级（暂未使用） |
| created_at | TIMESTAMP | 创建时间 |

### users 表（相关字段）

| 字段 | 类型 | 说明 |
|------|------|------|
| user_id | INTEGER | 用户 ID（主键） |
| username | TEXT | 用户名 |
| learning_goal | TEXT | 学习目标 |

---

## 代码特点总结

| 特点 | 说明 |
|------|------|
| **追加模式** | 保存知识点时不会删除已有数据，只追加新内容 |
| **去重逻辑** | 插入前先检查 `user_knowledge` 表是否已存在相同知识点 |
| **事务保护** | 使用 SQLite 事务保证数据一致性（atomic 操作） |
| **错误处理** | 有错误时 rollback，全部成功才 commit |
| **长连接** | TCP 连接保持不关闭，支持多次请求 |
| **调试日志** | 使用 `qDebug()` 输出详细日志 |

---

## 请求路由

代码位置: `mainwindow.cpp:48-81`

```cpp
void MainWindow::SlotReadFromClient()
{
    // ... 解析 JSON

    QString type = jsonobj["type"].toString();

    if (type == LoginType) {
        handleLoginRequest(jsonobj, tmpsocket);
    }
    else if (type == RegisterType) {
        handleRegisterRequest(jsonobj, tmpsocket);
    }
    else if (type == SaveKnowledgeType) {
        handleSaveKnowledgeRequest(jsonobj, tmpsocket);  // 保存知识库
    }
    else if (type == GetKnowledgeType) {
        handleGetKnowledgeRequest(jsonobj, tmpsocket);   // 获取知识库
    }
}
```

---

## 测试脚本

项目包含 Python 测试脚本用于测试知识库功能：

- `test_register.py` - 注册功能测试
- `quick_test.py` - 快速测试
- `start_test.bat` - 测试启动脚本

---

## 服务器配置

| 配置项 | 值 |
|--------|-----|
| 监听地址 | `HOSTPORT` (定义在 config.h) |
| 监听端口 | `8080` |
| 通信协议 | TCP |
| 数据格式 | JSON |

---

## 潜在改进建议

1. **学习目标更新逻辑**: 当前每次保存都会覆盖 `learning_goal`
2. **知识点批量插入**: 可优化为批量 INSERT 提高性能
3. **知识点删除/修改**: 当前只支持追加，缺少删除和修改功能
4. **掌握等级功能**: 数据库有 `mastery_level` 字段但代码未使用
5. **错误码标准化**: 建议使用数字错误码代替文本消息

---

*文档生成日期: 2026-01-09*
