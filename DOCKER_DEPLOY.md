# NetDisk Docker 快速部署

Docker Compose 会启动两个服务：

```text
netdisk-server  ── TCP/TLS 2000 ──> Qt 客户端
      │
      └── mysql:3306
```

数据库、网盘文件和 TLS 私钥分别使用 Docker volume 持久化。

## 1. 环境要求

- Docker Engine 20.10+
- Docker Compose V2，命令为 `docker compose`
- 服务器防火墙放行 TCP `2000`

检查：

```bash
docker --version
docker compose version
```

## 2. 一键启动

在项目根目录执行：

```bash
docker compose up -d --build
```

第一次启动会：

1. 构建 C++ 服务端镜像；
2. 启动 MySQL；
3. 执行 `Version4.0/conf/schema.sql`；
4. 等待 MySQL 健康检查通过；
5. 自动生成开发环境 TLS 证书；
6. 启动 NetDisk 服务端并监听 `0.0.0.0:2000`。

查看状态和日志：

```bash
docker compose ps
docker compose logs -f netdisk-server
```

正常情况下服务端日志包含：

```text
[netdisk] starting server on 0.0.0.0:2000
NetDisk server listening on 0.0.0.0:2000
```

## 3. 客户端连接

客户端运行在同一台服务器时：

```text
服务器地址：127.0.0.1
端口：2000
```

客户端运行在其他电脑时，填写 Docker 服务器的实际 IP，并确保云安全组和系统防火墙放行 TCP 2000：

```bash
sudo ufw allow 2000/tcp
```

开发版 Qt 客户端会忽略自签名证书错误，可以直接连接。

## 4. 停止和重启

停止容器但保留数据：

```bash
docker compose down
```

启动已有容器：

```bash
docker compose up -d
```

重新构建服务端：

```bash
docker compose up -d --build netdisk-server
```

## 5. 数据持久化

```bash
docker volume ls | grep netdisk
```

```text
netdisk_mysql_data  MySQL 用户、目录和日志
netdisk_file_data   SHA-256 物理文件
netdisk_tls_data    server.crt 和 server.key
```

不要随意执行下面的命令，因为它会删除数据库和网盘文件：

```bash
docker compose down -v
```

## 6. 故障排查

```bash
ss -lntp | grep 2000
docker compose ps
docker compose logs netdisk-server
docker compose logs mysql
openssl s_client -connect 127.0.0.1:2000 -servername localhost
```

容器内数据库地址必须是：

```text
mysql
```

不能填写 `127.0.0.1`，因为在服务端容器内它指向服务端容器自身。

## 7. 首次验证

Qt 客户端启动后，输入服务器 IP 和 `2000` 端口，注册账号并登录，然后依次测试：

1. 新建远程文件夹；
2. 上传文件；
3. 在传输页面观察进度；
4. 下载文件到本地目录；
5. 检查下载完成后的 SHA-256 校验。
