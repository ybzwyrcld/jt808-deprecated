# Jt808

中华人民共和国交通运输行业标准JT/T808协议的简单测试后台;


## 快速使用

以下操作命令基于 Ubuntu 16.04 系统


### 下载源码及编译
```bash
$ git clone https://github.com/hanoi404/jt808 && cd jt808
$ make all
```

### 运行后台服务
```bash
$ ./jt808service
```

然后需要将支持808协议的终端连接到这个后台, `IP`为你自己服务器的`IP`, 端口为`8193`, 可以在`main/service_main.cc`文件中自己修改.
 后台可识别的终端识别号为`13826539847`等, 如果有需要在源码目录下`examples/devices.list`文件里自行添加,
 并将该文件拷贝到`/etc/jt808/service`目录下.
 终端连接到本后台后, 如果终端实现了上报位置信息命令，后台会进行解析.


使用控制命令操作后台, 下发命令到终端. 下面是查询终端参数操作例子, 假设终端识别号为`13826539850`,
 要查询的终端参数ID为`0x0020`(位置汇报策略), 输入以下命令并得到返回结果:
```bash
$ ./jt808command 13826539850 getterminalparameter 0020
terminal parameter(id:value): 0020:0
```


## CMake

### 编译准备
```bash
$ sudo apt-get install cmake cmake-curses-gui
```

### 下载源码
```bash
$ git clone https://github.com/hanoi404/jt808 && cd jt808
$ mkdir build
```

### 下载googletest
```bash
$ git clone https://github.com/google/googletest
```
但还是推荐下载`release`版本源码包, 然后解压到源码根目录下, 我用的是1.8.1版本.

### 编译
```bash
$ cmake .. && make
```

### 使用
先确保以下目录存在并且有读写权限, 不存在请自行创建:
```bash
$ /etc/jt808/service
$ /etc/jt808/terminal
$ /tmp
$ /upgrade
$ /var/tmp
```
拷贝`examples`目录下文件到`/etc/jt808`目录下:
```bash
$ cp examples/devices.txt /etc/jt808/service/
$ cp examples/arearoute.txt /etc/jt808/terminal/
$ cp examples/terminalparameter.txt /etc/jt808/terminal/
```

运行后台服务:
```bash
$ ./jt808service
```
运行终端:
```bash
$ ./jt808terminal
```
使用命令行控制工具:
```bash
$ ./jt808command
```
然后根据提示正确输入完整命令.

### 生成 debug 版和 release 版的程序
```bash
$ ccmake ..
```
编辑`CMAKE_BUILD_TYPE`行, 填写`Debug`或`Release`.
