# NetDisk

一个基于 Linux/C++ 的多用户网盘服务器课程项目。当前版本保留了原项目的核心思路，并完成了协议、TLS、Token 会话、scrypt 密码哈希、参数化 SQL、线程池、超时回收、断点续传和文件完整性校验等工程化重构。

完整的源码导读、数据库初始化、证书生成、编译运行、测试方法、协议说明和简历描述见：

- [REPRODUCE.md](REPRODUCE.md)
- [Version4.0/qt_client/README.md](Version4.0/qt_client/README.md)
- [DOCKER_DEPLOY.md](DOCKER_DEPLOY.md)

## 主要能力

- 用户注册、登录和 Token 会话认证
- `ls`、`mkdir`、`rmdir`、`cd`、`rm`
- 文件上传、下载、断点续传
- SHA-256 内容寻址存储和重复文件秒传
- epoll 控制连接 + pthread 线程池处理文件传输
- MySQL 虚拟目录、用户信息和操作日志
- TLS 加密通信
- Qt Widgets 可视化客户端：登录、目录浏览、上传/下载进度和本地目录选择

## 目录

```text
Version4.0/
├── client/    客户端程序
├── qt_client/ Qt Widgets 可视化客户端（CMake/qmake）
├── common/    配置、TLS、协议和密码/摘要工具
├── conf/      服务端、客户端配置和数据库脚本
├── server/    服务端、线程池、会话、数据库实现
└── tests/     协议和密码/摘要基础测试

根目录还提供 `Dockerfile`、`docker-compose.yml` 和 `DOCKER_DEPLOY.md`，可一键启动 MySQL 与 NetDisk 服务端。
```
