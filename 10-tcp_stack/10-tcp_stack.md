网络传输机制实验一报告

<center>热伊莱·图尔贡 2018K8009929030</center>

## 一、实验题目：网络传输机制实验一

## 二、实验内容

### 内容一：连接管理

- 运行给定网络拓扑(tcp_topo.py)
- 在节点h1上执行TCP程序
  - 执行脚本(disable_tcp_rst.sh, disable_offloading.sh)，禁止协议栈的相应功能
  - 在h1上运行TCP协议栈的服务器模式  (./tcp_stack server 10001)
- 在节点h2上执行TCP程序
  - 执行脚本(disable_tcp_rst.sh, disable_offloading.sh)，禁止协议栈的相应功能
  - 在h2上运行TCP协议栈的客户端模式，连接至h1，显示建立连接成功后自动断开连接 (./tcp_stack client 10.0.0.1 10001)
- 可以在一端用tcp_stack_conn.py替换tcp_stack执行，测试另一端
- 通过wireshark抓包来来验证建立和断开连接的正确性

### 内容二：短消息收发

- 参照tcp_stack_trans.py，修改tcp_apps.c，使之能够收发短消息
- 运行给定网络拓扑(tcp_topo.py)
- 在节点h1上执行TCP程序
  - 执行脚本(disable_offloading.sh , disable_tcp_rst.sh)
  - 在h1上运行TCP协议栈的服务器模式  (./tcp_stack server 10001)
- 在节点h2上执行TCP程序
  - 执行脚本(disable_offloading.sh, disable_tcp_rst.sh)
  - 在h2上运行TCP协议栈的客户端模式，连接h1并正确收发数据 (./tcp_stack client 10.0.0.1 10001)
    - client向server发送数据，server将数据echo给client
- 使用tcp_stack_trans.py替换其中任意一端，对端都能正确收发数据

### 内容三：大文件传送

- 修改tcp_apps.c(以及tcp_stack_trans.py)，使之能够收发文件
- 执行create_randfile.sh，生成待传输数据文件client-input.dat
- 运行给定网络拓扑(tcp_topo.py)
- 在节点h1上执行TCP程序
  - 执行脚本(disable_offloading.sh , disable_tcp_rst.sh)
  - 在h1上运行TCP协议栈的服务器模式  (./tcp_stack server 10001)
- 在节点h2上执行TCP程序
  - 执行脚本(disable_offloading.sh, disable_tcp_rst.sh)
  - 在h2上运行TCP协议栈的客户端模式 (./tcp_stack client 10.0.0.1 10001)
    - Client发送文件client-input.dat给server，server将收到的数据存储到文件server-output.dat
- 使用md5sum比较两个文件是否完全相同
- 使用tcp_stack_trans.py替换其中任意一端，对端都能正确收发数据

## 三、实验过程

### 1.连接管理

<img src="pic/sock_listen.png" width = 100% height = 50%/>

<img src="pic/sock_accept.png" width = 100% height = 50%/>

<img src="pic/sock_connect.png" width = 100% height = 50%/>

### 2.状态机转移

<img src="pic/微信截图_20220609211347.png" width = 100% height = 80%/>

<img src="pic/tcp_process.png" width = 80% height = 30%/>

### 3.TCP计时器管理

<img src="pic/timer_list.png" width = 100% height = 50%/>

<img src="pic/timewait.png" width = 100% height = 50%/>

## 四、实验结果

由于实验环境出问题，未能进行验证。
