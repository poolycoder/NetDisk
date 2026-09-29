# NetDisk 完整复现与源码学习手册

## 1. 项目定位

NetDisk 是一个 Linux C++ 多用户网盘原型。它不是标准 FTP 服务器，而是自定义了一个基于 TCP/TLS 的长度分帧协议。

本次修复保留了原项目的核心设计：

```text
客户端
  │ TLS/TCP 自定义协议
  ▼
服务端 epoll 主循环
  ├── 登录、注册、ls、mkdir、rmdir、cd、rm
  └── PUT/GET 任务进入线程池
                    │
                    ▼
              文件传输 Worker
                    │
              ┌─────┴─────┐
              ▼           ▼
           MySQL       本地存储
        虚拟目录/日志    SHA-256 文件
```

## 2. 修复内容总览

| 类别 | 原问题 | 当前实现 |
|---|---|---|
| epoll | 就绪事件数组与客户端数组错误对应 | 直接遍历 `epoll_wait` 返回的每个 fd |
| 线程池 | Worker 持有队列锁执行完整传输 | 取出任务后立即解锁 |
| Socket | 上传下载 fd 未可靠关闭 | `Connection` RAII 自动关闭 |
| 超时 | 无锁访问容器、遍历时删除、3 秒硬编码 | `Timer` 使用 mutex、条件变量和安全过期列表，默认 30 秒 |
| 协议 | native int、无长度上限、缓冲区可能溢出 | 网络字节序、最大帧长度、64 KiB 文件块 |
| 认证 | 上传下载连接信任客户端 username | 登录后签发随机 Token，每次操作验证 Token |
| 密码 | 明文 Password、传统 crypt、弱随机 Salt | OpenSSL scrypt + `RAND_bytes` |
| SQL | 字符串拼接导致注入 | MySQL prepared statement |
| 传输 | mmap 空文件失败、偏移越界、断点文件伪完整 | 流式分块、偏移检查、`.part` 临时文件和 SHA-256 校验 |
| 存储 | MD5、物理文件无原子提交、删除不回收 | SHA-256、临时文件 + hard link 提交、引用检查后删除 |
| 配置 | `server.conf` 没有读取 | key=value 配置解析器 |
| 工程 | 实现堆积在头文件、旧 `.o` 和旧传输文件 | 实现移动到 `.cpp`，删除废弃文件并增加 `.gitignore` |

密码哈希使用 scrypt，是用户要求的 Argon2id/bcrypt/scrypt 三类方案之一。TLS 客户端在本地开发模式下暂时不验证自签名证书；生产环境应配置 CA 并开启证书校验。

## 3. 文件说明

### 3.1 `Version4.0/common/`

#### `config.h/.cpp`

实现简单的 `key=value` 配置读取，支持字符串、整数和无符号 64 位整数。

#### `connection.h/.cpp`

封装 TCP/TLS 连接：

- `Connection`：持有 fd 和 `SSL*`
- `sendAll()`：处理部分发送
- `receiveAll()`：处理部分接收
- `createServerTlsContext()`：加载服务端证书和私钥
- `createClientTlsContext()`：创建客户端 TLS 上下文
- `acceptTls()` / `connectTls()`：执行 TLS 握手
- 析构函数：释放 SSL 并关闭 fd

#### `protocol.h/.cpp`

定义统一长度分帧：

```text
4 字节 uint32 网络字节序长度 + payload
```

控制命令采用文本 payload，文件内容采用最多 64 KiB 的二进制 frame。文件大小使用 8 字节网络字节序整数。

#### `crypto.h/.cpp`

实现：

- `randomToken()`：生成 32 字节随机 Token
- `hashPassword()`：scrypt 密码哈希
- `verifyPassword()`：常量时间比较密码哈希
- `sha256File()`：计算文件摘要

### 3.2 `Version4.0/client/`

#### `client.cpp`

客户端主程序：

1. 读取 `client.conf`
2. 建立 TLS 连接
3. 注册或登录
4. 保存服务端签发的 Token
5. 处理交互命令
6. 调用上传/下载函数

客户端发送的控制命令格式示例：

```text
LS token directory_id
MKDIR token directory_id name
CD token directory_id name
RM token directory_id name
```

#### `function.h`

定义客户端配置、客户端上下文和上传下载函数接口。

#### `upLoad_downLoad.cpp`

实现上传和下载：

- 上传前计算 SHA-256
- 服务器发现已有相同内容时执行秒传
- 普通上传使用分块 frame
- 下载使用 `filename.part`
- 中断后读取 `.part` 文件大小作为偏移
- 下载完成后重新计算 SHA-256
- 校验成功后将 `.part` 原子重命名为正式文件

### 3.3 `Version4.0/server/`

#### `server.cpp`

服务端入口和事件循环。核心原则是：

```cpp
for (每一个 epoll_wait 返回的事件) {
    if (fd == listen_fd) {
        accept 新连接;
    } else {
        处理这个具体 fd;
    }
}
```

不再把 `evs[i]` 和 `clients[i]` 强行认为是一一对应关系。

新连接经过 TLS 后：

- `LOGIN` / `REGISTER` 进入认证流程
- `PUT` / `GET` 验证 Token 后进入线程池
- 其他命令拒绝

#### `MyDb.h/.cpp`

数据库封装。动态参数全部通过 MySQL prepared statement 绑定，不再拼接 SQL。

主要操作：

- 用户存在性查询
- 用户创建
- 密码哈希查询
- 虚拟目录列表和节点查询
- 创建目录
- 删除空目录
- 删除文件
- 查询 SHA-256 引用
- 插入文件元数据
- 写入日志

#### `factory.h/.cpp`

线程池实现。Worker 的正确顺序是：

```text
加锁 → 等待任务 → 取出任务 → 解锁 → 执行上传/下载
```

文件传输过程中不再持有任务队列锁，因此多个 Worker 可以并行工作。

#### `workQue.h/.cpp`

使用 `std::deque`、`std::mutex` 和 `std::condition_variable` 实现任务队列。

#### `timer.h/.cpp`

线程安全的连接空闲计时器：

- 主线程通过 `update(fd)` 更新时间
- 计时器只在锁内访问 map
- 先收集过期 fd，再释放锁后执行回调
- 不在 range-for 中直接 erase 当前元素

#### `session.h/.cpp`

维护：

```text
token → {username, 登录连接 fd}
fd    → token
```

上传下载连接不需要与登录连接使用同一个 fd，但必须持有有效 Token。

#### `tcp_init.cpp`

创建监听 Socket、绑定地址、设置监听队列。

### 3.4 `Version4.0/conf/`

- `server.conf`：服务端网络、数据库、TLS、线程和存储配置
- `client.conf`：客户端连接配置
- `schema.sql`：数据库初始化脚本

### 3.5 `Version4.0/tests/`

`protocol_crypto_test.cpp` 使用 `socketpair()` 验证：

- 分帧发送和接收
- SHA-256 文件摘要基础流程

## 4. 数据库模型

### `Shadow`

```text
User         用户名
PasswordHash scrypt 编码后的密码哈希
```

不再保存明文密码。

### `Virtual_Dir`

```text
FileId   当前节点 ID
Dir      父目录 ID，0 表示根目录
FileName 虚拟名称
FileSize 文件大小
FileType f 文件 / d 目录
SHA256   真实存储文件名
User     文件所属用户
```

示例：

```text
FileId | Dir | FileName | FileType | User
------------------------------------------
1      | 0   | docs     | d        | alice
2      | 1   | a.txt    | f        | alice
```

### `Log`

保存用户操作和数据库时间 `NOW()`。

## 5. 运行环境

源码使用 Linux API，包括：

- `epoll`
- `pthread/std::thread`
- `send/recv`
- `accept4`
- `mmap` 已不再用于核心传输，但保留 Linux 文件 API
- OpenSSL
- MySQL C API

Windows 原生环境不能直接运行服务端。推荐使用 Ubuntu、WSL2、Docker 或 Linux 虚拟机。

## 6. 安装依赖

Ubuntu 示例：

```bash
sudo apt update
sudo apt install -y \
  g++ make openssl mysql-server \
  libmysqlclient-dev libssl-dev
```

OpenSSL 版本需要支持 `EVP_PBE_scrypt`，建议使用 OpenSSL 1.1.1 或更高版本。

## 7. 生成本地 TLS 证书

从 `Version4.0/server` 目录执行：

```bash
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout ../conf/server.key \
  -out ../conf/server.crt \
  -days 3650 \
  -subj "/CN=localhost"
```

开发模式客户端不校验证书，因此自签名证书可以直接使用。生产环境需要：

1. 使用可信 CA 或内部 CA 签发证书。
2. 客户端加载 CA 文件。
3. 设置 `SSL_VERIFY_PEER`。
4. 校验主机名。

## 8. 初始化数据库

```bash
mysql -u root -p < Version4.0/conf/schema.sql
```

脚本会创建：

- `Netdisk` 数据库
- `netdisk@localhost` 开发账号
- `Shadow`
- `Virtual_Dir`
- `Log`

这是新版本 schema。原项目使用 `Password`、`Salt`、`Cipher` 和 `MD5` 字段，不能只靠 `CREATE TABLE IF NOT EXISTS` 自动迁移到新结构；复现修复版时请使用全新的 `Netdisk` 数据库，或按业务需要编写迁移脚本。

如果实际数据库密码不同，请修改：

```text
Version4.0/conf/server.conf
```

不要在生产环境使用示例密码，也不要使用 root 连接应用。

## 9. 编译

```bash
cd Version4.0/server
make

cd ../client
make

cd ../tests
make
./protocol_crypto_test
```

清理编译产物：

```bash
make clean
```

服务端 makefile 使用 `mysql_config` 获取 MySQL 头文件和链接参数。如果发行版没有该命令，需要安装 MySQL 开发包，或根据实际路径修改 makefile。

## 10. 启动程序

必须从指定目录启动，因为配置中的相对路径以当前工作目录为基础。

终端一：

```bash
cd Version4.0/server
./server
```

终端二：

```bash
cd Version4.0/client
./client
```

服务端默认监听：

```text
0.0.0.0:2000
```

客户端默认连接：

```text
127.0.0.1:2000
```

也可以指定服务端配置路径：

```bash
./server ../conf/server.conf
```

## 11. 功能验证流程

客户端启动后：

```text
2                 注册
alice             用户名
password123       密码

1                 登录
alice             用户名
password123       密码
```

登录后验证：

```text
ls
mkdir docs
cd docs
puts /tmp/test.txt
ls
gets test.txt
cd ..
rm test.txt
rmdir docs
quit
```

上传同一内容但换一个虚拟文件名，可以验证秒传；下载过程中终止客户端，再次执行 `gets`，可以验证 `.part` 断点续传。

## 12. 协议时序

### 登录

```text
Client → Server: LOGIN username password
Server → Client: OK token
```

密码处于 TLS 加密通道中，数据库只保存 scrypt 哈希。

### 上传

```text
Client → Server: PUT token dir filename
Client → Server: SHA-256
Server → Client: INSTANT 或 READY

READY 情况：
Client → Server: uint64 文件大小
Client → Server: 文件 chunk frame...
Client → Server: 空 frame
Server → Client: OK
```

### 下载

```text
Client → Server: GET token dir filename offset
Server → Client: FOUND sha256
Server → Client: uint64 完整文件大小
Server → Client: chunk frame...
Server → Client: 空 frame
```

客户端只在收到完整文件并通过 SHA-256 校验后才把 `.part` 改名为正式文件。

## 13. 仍需关注的生产级增强点

当前版本已经修复课程项目中的主要 P0/P1/P2 问题，但仍有以下生产增强方向：

1. 客户端开启 TLS 证书校验。
2. 将数据库连接改为真正的连接池。
3. 将日志写入改为异步日志队列。
4. 使用更完整的二进制协议，避免控制命令只支持空格分隔参数。
5. 将文件存储拆分为分层目录，避免单目录文件过多。
6. 增加 Token 过期时间、主动注销和刷新机制。
7. 增加上传限速、用户配额和并发数限制。
8. 增加完整的 integration test、压力测试和协议 fuzz 测试。
9. 对数据库操作增加事务，确保元数据和物理文件一致。
10. 增加后台垃圾文件扫描和引用计数回收。

## 14. 推荐调试方法

查看网络连接：

```bash
ss -lntp | grep 2000
```

查看系统调用：

```bash
strace -f -e trace=network,file ./server
```

使用 GDB：

```bash
gdb ./server
run
break ServerApp::handleTransfer
```

查看数据库文件引用：

```sql
USE Netdisk;
SELECT User, Dir, FileName, FileSize, FileType, SHA256
FROM Virtual_Dir;
```

## 15. 简历项目描述

### 标准中文简历版本

**基于 C++/Linux 的多用户网盘服务器**

- 基于 TCP/TLS 设计自定义长度分帧协议，统一封装完整收发逻辑，解决 TCP 粘包、拆包、部分读写和非法帧长度问题。
- 使用 epoll 构建事件驱动网络主循环，负责登录、注册、虚拟目录管理等控制请求；使用 pthread 线程池异步处理文件上传下载，降低主线程阻塞风险。
- 设计 MySQL 邻接表虚拟目录模型，实现用户级文件隔离、目录切换、文件元数据管理和操作日志记录。
- 使用 SHA-256 构建内容寻址存储，将物理文件与虚拟文件名解耦，实现重复文件检测、秒传和引用安全删除。
- 使用 TLS 加密客户端通信，基于随机 Token 管理跨连接会话，避免信任客户端提交的用户名导致的越权访问。
- 使用 OpenSSL scrypt 保存密码哈希，使用 `RAND_bytes` 生成随机 Salt；通过 MySQL prepared statement 防止 SQL 注入。
- 采用临时文件、原子 hard-link 提交、`.part` 断点文件和 SHA-256 完整性校验，实现可靠的大文件上传下载。
- 针对 epoll 事件错配、线程池锁粒度过大、计时器数据竞争、fd 泄漏、偏移越界和异常断点文件等问题完成系统性重构。

### 精简版

**C++/Linux 多用户网盘系统**：使用 epoll + pthread 线程池实现事件驱动文件服务器；基于 MySQL 维护虚拟目录，使用 TLS、Token、scrypt 和 prepared statement 完成安全认证；采用 SHA-256 内容寻址存储实现秒传，结合分块传输、临时文件和校验机制实现断点续传。

### 面试介绍模板

这个项目本质上是一个自定义协议的网盘服务器。控制连接由 epoll 主线程管理，上传下载任务进入线程池。数据库只保存用户、虚拟目录和文件元数据，真实文件按照 SHA-256 存储，因此相同内容可以复用。登录成功后服务端签发 Token，后续控制操作和独立的上传下载连接都必须提交 Token。文件下载使用 `.part` 文件记录进度，完整接收后重新计算 SHA-256，通过校验才正式提交。

## 16. Qt Widgets 可视化客户端

源码位置：`Version4.0/qt_client/`。

客户端复用服务端现有的 TLS 和长度分帧协议，提供：

- 登录、注册并登录；
- 远程目录浏览、进入目录、返回上级、新建和删除；
- 本地文件夹列表和双击切换；
- 选中文件直接上传；
- 文件下载、本地保存目录选择；
- 上传/下载进度条和任务状态；
- 下载 `.part` 文件断点续传和 SHA-256 完整性校验。

### Qt Creator + CMake

使用 Qt Creator 打开 `Version4.0/qt_client/CMakeLists.txt`，选择 Qt 5.15 或 Qt 6 Desktop Kit，配置并运行 `netdisk_qt_client`。

### qmake

```bash
cd Version4.0/qt_client
qmake netdisk_qt_client.pro
make
```

默认连接 `127.0.0.1:2000`。启动服务端后打开 Qt 客户端，在登录页输入服务器地址、端口和账号即可使用。

开发环境客户端会忽略自签名证书错误；正式环境需要配置 CA 验证并删除 `ignoreSslErrors()`。当前服务端上传协议不支持服务端临时上传文件恢复，因此 Qt 客户端目前重点实现下载 `.part` 续传；上传断点续传需要后续扩展 `PUT offset` 协议。
