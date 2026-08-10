# T113 Tailscale 手动启动流程

## 1. 准备文件
将以下两个可执行文件放到开发板 `/root/` 目录：

- `/root/tailscale`
- `/root/tailscaled`

赋予执行权限：

```sh
chmod +x /root/tailscale
chmod +x /root/tailscaled
```

## 2. 创建状态目录
用于保存 Tailscale 登录状态：

```sh
mkdir -p /root/tailscale-state
```

## 3. 启动 `tailscaled`
启动 Tailscale 后台服务：

```sh
/root/tailscaled \
  --tun=userspace-networking \
  --socket=/tailscaled.sock \
  --state=/root/tailscale-state/tailscaled.state \
  >/tmp/tailscaled.log 2>&1 &
```

说明：

- `--tun=userspace-networking`：使用用户态网络模式
- `--socket=/tailscaled.sock`：指定本地控制 socket
- `--state=/root/tailscale-state/tailscaled.state`：指定状态文件保存路径
- `>/tmp/tailscaled.log 2>&1`：日志输出到文件
- `&`：后台运行

## 4. 让设备加入 Tailscale 网络
执行：

```sh
/root/tailscale --socket=/tailscaled.sock up --accept-dns=false
```

说明：

- `up`：让设备登录并加入当前 tailnet
- `--socket=/tailscaled.sock`：通过该 socket 与后台服务通信
- `--accept-dns=false`：不修改系统 DNS 配置

## 5. 检查 Tailscale 状态
查看是否上线成功：

```sh
/root/tailscale --socket=/tailscaled.sock status
```

查看分配的 IPv4 地址：

```sh
/root/tailscale --socket=/tailscaled.sock ip -4
```

## 6. 启动本地网页服务
进入网页目录并启动最简单的 HTTP 服务：

```sh
cd /lib/modules/5.4.61/extra/web
python3 -m http.server 8080
```

本地验证：

```sh
curl http://127.0.0.1:8080/
```

说明：

- `127.0.0.1:8080` 仅供开发板本机测试使用
- 该目录下应包含 `index.html`、`latest.jpg` 等网页资源

## 7. 通过 Tailscale 发布本地 8080 服务
将本地网页服务对外发布到 tailnet：

```sh
/root/tailscale --socket=/tailscaled.sock serve --bg --http=8080 http://127.0.0.1:8080
```

查看发布状态：

```sh
/root/tailscale --socket=/tailscaled.sock serve status
```

## 8. 外部访问方式
外部设备不要优先使用 `100.x.x.x:8080`，当前方案推荐使用 `serve status` 输出的域名，例如：

```text
http://t113-tronlong-1.tail5f295d.ts.net:8080/
```

说明：

- `127.0.0.1:8080`：本机内部服务地址
- `.ts.net:8080`：通过 Tailscale 发布后的外部访问地址

## 9. 常用排查命令
查看后台服务是否运行：

```sh
ps | grep tailscaled
```

查看控制 socket 是否存在：

```sh
ls -l /tailscaled.sock
```

查看后台日志：

```sh
cat /tmp/tailscaled.log
```

查看服务发布状态：

```sh
/root/tailscale --socket=/tailscaled.sock serve status
```
