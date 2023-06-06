# Exata添加srip路由协议

## 一. 安装好exata

按EXATA 7.2安装编译.pdf 安装好exata，安装完成后exata安装目录如下：

![1663312218023](.\assets\1663312218023.png)

## 二. 修改exata配置文件，将自己的代码加入exata工程

1. 修改exata配置文件

修改~\Scalable\exata\7.2.0\gui\settings\protocol_models\ **routing_protocols.prt**，添加下列标签，让srip协议在ExataGUI中可见。

![1663312770187](.\assets\1663312770187.png)

2. 将自己的代码加入exata工程

在~\Scalable\exata-7.2.0-source\exata-7.2.0\libraries 目录下新建user_models目录，user_models目录下新建目录src和文本文件CMakeLists.txt，建议用户要添加的的源代码都放在src目录中。

![1663322050442](.\assets\1663322050442.png)

修改~\Scalable\exata-7.2.0-source\exata-7.2.0\libraries目录下的CMakeLists.txt，在foreach()括号中添加user_models

![1663313992981](.\assets\1663313992981.png)

~\Scalable\exata-7.2.0-source\exata-7.2.0\libraries\user_models目录下的CMakeLists.txt如下

![1663314178042](.\assets\1663314178042.png)

打开CMake工具重新Generate之后，打开exata工程，发现routing_srip.cpp和routing_srip.h已经加入工程

![1663314747295](.\assets\1663314747295.png)

接下来需要在exata自带的代码中定义srip协议需要的常量，以及添加调用关系。

## 三. 添加srip协议

1. 定义SRIP需要的常量

在enum_NetworkRoutingProtocolType.h枚举列表中定义常量NETWORK_PROTOCOL_SRIP

![1663316283156](.\assets\1663316283156.png)

在network_ip.h中定义常量IPPROTO_SRIP

![1663316685650](.\assets\1663316685650.png)

2. 添加调用关系，让exata主程序在运行时能够调用routing_srip.cpp中的方法。主程序中与路由协议相关联的方法，基本都是在network_ip.cpp中定义的，在添加对srip的调用时，每部分仿照其他exata自定义的路由协议稍作修改即可。

在network_ip.cpp中，把srip的头文件include进来

![1663318355379](.\assets\1663318355379.png)

让network_ip.cpp中的IpRoutingInit()方法调用SripInit()

在network_ip.cpp中的IpRoutingInit()方法的switch语句中，添加如下代码

![1663318733550](.\assets\1663318733550.png)

让network_ip.cpp中的NetworkIpLayer()方法调用SripHandleProtocolEvent()

在network_ip.cpp中的NetworkIpLayer()方法的switch语句中，添加如下代码

![1663319121369](.\assets\1663319121369.png)

让network_ip.cpp中的DeliverPacket()方法调用SripHandleProtocolPacket()

在network_ip.cpp中的DeliverPacket()方法的switch语句中，添加如下代码

![1663319863783](.\assets\1663319863783.png)

让network_ip.cpp中的NetworkFinalize()方法调用SripFinalize()

在network_ip.cpp中的NetworkFinalize()方法的switch语句中，添加如下代码

![1663320310905](.\assets\1663320310905.png)

在network_ip.cpp的NetworkIpParseAndSetRoutingProtocolType()方法中添加如下代码

该方法用于解析路由协议参数、设置路由协议类型

![1663321091066](.\assets\1663321091066.png)

添加完毕，生成解决方案...

一般需要40~60min...

![1663385419737](.\assets\1663385419737.png)

最后把~\Scalable\exata\7.2.0\bulid\bin目录下的exata.exe和libexata_scalable_FNP.dll两个文件复制到

~\Scalable\exata\7.2.0\bin目录下，替换掉原来的两个同名文件，这样EXataGUI才能运行刚刚生成的结果。

## 四. 跑一个小Demo

在EXataGUI中搭如下场景（4个节点，1号节点给4号节点发cbr包）

![1663386481446](.\assets\1663386481446.png)

为了方便查看结果，可以设置一下节点的最大传输距离，这里通过设置让每个节点都只有一个邻居

![1663386904962](.\assets\1663386904962.png)

.config场景文件建议保存在~Scalable\exata\7.2.0\scenarios\user目录下，方便查找

![1663387069994](.\assets\1663387069994.png)

运行仿真，仿真完毕后查看输出窗口，这里会显示SripFinalize()函数中输出的每个节点最终的路由表

![1663387981745](.\assets\1663387981745.png)

查看cbr包的发送与接收情况

![1663405031391](.\assets\1663405031391.png)

![1663405062138](.\assets\1663405062138.png)

