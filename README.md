# 协议分析实验

[![standard-readme compliant](https://img.shields.io/badge/readme%20style-standard-brightgreen.svg?style=flat-square)](https://github.com/RichardLitt/standard-readme)

此仓库致力于通过代码复现常见的协议交互过程，正常交互或者异常交互。通过复现各种场景，来加强对协议本身的了解。帮助生产环境更快的定位问题

本仓库包含以下内容：

1. tcp三次握手

## 内容列表

- [背景](#背景)
- [安装](#安装)
- [使用说明](#使用说明)
- [示例](#示例)
- [维护者](#维护者)
- [如何贡献](#如何贡献)
- [使用许可](#使用许可)

## 背景

在实际的生产环境中，一些常见的协议交互过程，比如三次握手，四次挥手等比较常见，但是更多的，比如快速重传，早期重传，延迟确认等等没有一定的积累很难进行复现。一方面为了更好的测试一些不容易复现的协议交互过程，另一方面为了加深对协议本身的认识。因此就是有了建立这个项目的想法。


这个仓库的目标是：

1. 常见的协议交互过程实现
2. 各种异常复现


## 安装

这个项目使用 [make](https://cmake.org) 和 [gcc](https://www.gcc.com/) 进行安装。请确保你本地安装了它们。

```sh
$ make 
```

## 使用说明


```sh
$ ls bin
# 输出可以执行的命令

$ bin/handshake 
# 执行三次握手
# 执行前可在代码目录查看readme说明
```

## 示例
执行三次握手

```sh
$ iptables -A OUTPUT -p tcp --tcp-flags RST RST -j DROP
#  当发送收到syn-ack后，内核会发送一个RST包，导致tcp三次握手失败，所需要丢弃掉内核的RST包。

$ bin/handshake 
Useage: ./handshake <source_ip> <source_port> <dest_ip> <dest_port>
# 直接执行可获取帮助信息

$ bin/handshake 192.168.56.112 8888 192.168.56.115 8081
# 源ip地址为本机器ip地址
# 源端口也随意填写 12345 也可
# 目标ip地址，要填写准备建连的目标机器ip，可以目标机器上 通过 nc -l 8081 监控8081端口，当测试的服务端


# 在目标机器上执行命令查看连接是否已经建立
$ ss -na |grep 8081
tcp    ESTAB      0      0      192.168.56.115:8081               192.168.56.112:8888
```

## 维护者

[@smartxff](https://github.com/smartxff)。

## 如何贡献

非常欢迎你的加入！[提一个 Issue](https://github.com/smartxff/protocol-analysis/issues/new) 或者提交一个 Pull Request。


标准 Readme 遵循 [Contributor Covenant](http://contributor-covenant.org/version/1/3/0/) 行为规范。

### 贡献者

感谢以下参与项目的人：


## 使用许可

[MIT](LICENSE) © Richard Littauer

