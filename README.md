# Jt808

中华人民共和国交通运输行业标准JT/T808协议的简单测试后台;


## 快速使用

以下操作命令基于 Ubuntu 16.04 系统


### 下载源码及编译
```bash
$ git clone https://github.com/hanoi404/jt808 && cd jt808/jt808
$ make all
```

### 运行后台服务
```bash
$ ./jt808service
```

然后需要将支持808协议的终端连接到这个后台, IP为你自己服务器的IP, 端口为`8193`, 可以在`main.cc`文件中自己修改.
后台可识别的终端识别号为`13826539847`等, 如果有需要在源码目录下`devices/devices.list`文件里自行添加.
终端连接到本后台后, 如果终端实现了上报位置信息命令，后台会进行解析.


使用控制命令操作后台, 下发命令到终端. 下面是查询终端参数操作例子, 假设终端识别号为`13826539850`, 要查询的终端参数ID为`0x0020`(位置汇报策略), 输入以下命令并得到返回结果:
```bash
$ ./jt808command 13826539850 getterminalparameter 0020
terminal parameter(id:value): 0020:0
```

## CMake

### 编译准备
```bash
$ sudo apt install cmake cmake-curses-gui
```

### 下载源码
```bash
$ git clone https://github.com/hanoi404/jt808 && cd jt808/jt808
$ mkdir build
```

### 编译后台服务程序
```bash
$ mkdir build/service && cd build/service
$ cmake ../.. && make
```

### 编译命令行控制程序
```bash
$ mkdir build/command && cd build/command
$ cmake ../../command && make
```

### 生成 debug 版和 release 版的程序

以后台程序为例:
```bash
$ ccmake ../..
```

第一次可能出现以下界面, 输入`c`继续下一步:
```bash
                                                     Page 0 of 1
                                                      EMPTY CACHE


















EMPTY CACHE:
Press [enter] to edit option                                                                                                 CMake Version 3.5.1]
Press [c] to configure
Press [h] for help           Press [q] to quit without generating
Press [t] to toggle advanced mode (Currently Off)
```

出现以下界面时，光标移动到`CMAKE_BUILD_TYPE`行上面, 输入`Enter`键进行编辑, 填写`Debug`或`Release`, 完成后再次输入`Enter`键退出编辑, 然后`c`键进行配置, 再输入`g`键生成`Makefile`:
```bash
                                                     Page 1 of 1
CMAKE_BUILD_TYPE                *
CMAKE_INSTALL_PREFIX            */usr/local
















CMAKE_BUILD_TYPE: Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel.)
Press [enter] to edit option                                                                                                 CMake Version 3.5.1]
Press [c] to configure
Press [h] for help           Press [q] to quit without generating
Press [t] to toggle advanced mode (Currently Off)
```

编译
```bash
$ make
```
