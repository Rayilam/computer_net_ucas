# 网络地址转换实验报告

热伊莱·图尔贡
2018K8009929030

## 一、实验题目：网络地址转换实验

## 二、实验内容：

1. 实验内容一：SNAT实验
   * 运行给定网络拓扑(nat_topo.py)
   * 在n1, h1, h2, h3上运行相应脚本
     n1: disable_arp.sh, disable_icmp.sh, disable_ip_forward.sh, disable_ipv6.sh
     h1-h3: disable_offloading.sh, disable_ipv6.sh
   * 在n1上运行nat程序：  n1# ./nat exp1.conf
   * 在h3上运行HTTP服务：h3# python ./http_server.py
   * 在h1, h2上分别访问h3的HTTP服务
     h1# wget http://159.226.39.123:8000
     h2# wget http://159.226.39.123:8000
2. 实验内容二：DNAT实验
   * 运行给定网络拓扑(nat_topo.py)
   * 在n1, h1, h2, h3上运行相应脚本
     n1: disable_arp.sh, disable_icmp.sh, disable_ip_forward.sh, disable_ipv6.sh
     h1-h3: disable_offloading.sh, disable_ipv6.sh
   * 在n1上运行nat程序：  n1# ./nat exp2.conf
   * 在h1, h2上分别运行HTTP Server：   h1/h2# python ./http_server.py
   * 在h3上分别请求h1, h2页面
     h3# wget http://159.226.39.43:8000
     h3# wget http://159.226.39.43:8001
3. 实验内容三：手动构造一个包含两个nat的拓扑
   * h1 <-> n1 <-> n2 <-> h2
   * 节点n1作为SNAT， n2作为DNAT，主机h2提供HTTP服务，主机h1穿过两个nat连接到h2并获取相应页面

## 三、实验过程：

1. 区分数据包方向

   <img src="pic/code_direct.png" width = 100% height = 30%/>
2. 合法数据包

   * 数据包的方向为DIR_IN

   <img src="pic/code_IN.png" width = 100% height = 10%/>

   * 数据包的方向为DIR_OUT

   <img src="pic/code_OUT.png" width = 100% height = 10%/>
3. NAT老化（Timeout）操作

   <img src="pic/code_TIMEOUT.png" width = 100% height = 10%/>

## 四、实验结果：

1. 实验内容一：SNAT实验

<img src="pic/1-h1.png">

<img src="pic/1-h2.png">

<img src="pic/1-h3.png">

<img src="pic/1.png">

2. 实验内容二：DNAT实验

<img src="pic/2-h3.png">

<img src="pic/2-h1.png">

<img src="pic/2-h2.png">

<img src="pic/2.png">

3. 实验内容三：手动构造一个包含两个nat的拓扑

<img src="pic/3-h1.png">

<img src="pic/3-h2.png">

<img src="pic/3.png">

## 五、思考题

1. 实验中的NAT系统可以很容易实现支持UDP协议，现实网络中NAT还需要对ICMP进行地址翻译，请调研说明NAT系统如何支持ICMP协议。

   ICMP协议没有端口号，因此无法像UDP协议一样，通过<IP, 端口号>建立唯一映射。
   ICMP协议报文格式如下：

   <img src="pic/ICMP.png">

   假设主机结点h1要ping结点h2，h1发送 ICMP报文，h1 会根据报文头部中包含的类型信息 Type 和代码信息 Code生成源端口号，根据报文头部中包含的标识符信息 Identifier 生成目的端口号，nat路由器收到包后，Type+Code作为内网端口，IDENTIFIER作为外网端口，生成响应nat表项，然后将ICMP报头的源端口和目的端口进行相应修改，在结点 h2 收到 ICMP 报文后，生成 ICMP 响应报文，Type+Code作为源端口，IDENTIFIER作为目的端口。nat路由器结点收到响应报文后，查找 nat表项并且确定应该转发给私网中的结点 h1 。两个结点完成⼀次来回通信。
2. 给定一个有公网地址的服务器和两个处于不同内网的主机，如何让两个内网主机建立TCP连接并进行数据传输。（提示：不需要DNAT机制）
   让两个处于不同nat网络下的结点直接建立连接，称为nat穿透。
   此时每个结点需要知道自己的内网地址和对方的公网地址端口，从而发送数据包、建立连接。
   可以通过发送一个数据包给server，server通过解析和查表，获得结点的公网地址，再将地址回传给节点的方法发现自己的公网地址端口，然后通过第三方server来交换双方的公网地址端口，从而建立连接。
