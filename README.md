# Lab 1

## 部分设计

### 1. 方法选择
+ 我选择了GO-BACK-N的实现方法
+ 对于超时的包，会讲整个WINDOWS-SIZE大小的包进行重发
### 2. 部分参数设计
+ 我将WINDOWS-SIZE的大小设置成 5
+ 将TIMEOUT的数值设置为0.3
### 3. checksum算法选择
+ 我在网上选择了一种最普通的实现方法，代码见下
        ```c
        static short checksum(struct packet* pkt) {
            unsigned long sum = 0;
            for(int i = 2; i < RDT_PKTSIZE; i += 2) 
                sum += *(short *)(&(pkt->data[i]));
            while(sum >> 16) 
                sum = (sum >> 16) + (sum & 0xffff);
            return ~sum;
        }
        ```
### 4.sender和receiver中buffer的维护
+ 在sender中维护了一个static message* ms_buffer的数组，用来保存从上层发来的message，用static packet* windows存储message被切割以后的每个包（最多存储WINDOWS-SIZE个），用来进行信息的发送和重发
+ 在receiver，维护着static packet* ms_buffer，用来保存sender发送出去的包（防止乱序收到），并且在receiver将包拼接起来，放在static message* use_message中

## sender的收包和发包
#### 发出的包
+  ```
    |<-  2 byte  ->|<-  4 byte  ->|<-  1 byte  ->|<-  the rest  ->|
    |   checksum   |  packet seq  | payload size |   payload    |
    ```
    发出的包由checksum，packet seq，payload size，和payload组成，其中payload size会在receiver中使用起来，重新恢复message，同时receivers存在一个ms_location的全局变量当ms_location为0的时候说明此时的expected_packet是整个message的第一个包，对于这个第一个包，payload有着特殊的设计，他的payload分成两个部分，前四个byte表示一个整型，表示这个message的真正大小，后面的byte才是真实的数据
#### 收到的包
+ 收到的包来自于上层，是整个message的内容，需要将这个message切割成上面发出的包的样子。
+ 收到的包来自于下层，则是一个ACK包（在receiver中详细讲述）
## receiver的收包和发包
#### 收到的包
+ 收到的包是来自于sender发出的包，首先会更具packet seq的数值判断是否是需要的包（>= expected_packet && expected_packet + WINDOWS_SIZE），如果符合要求，将包存下来，并且看是否开始合并，否则将包简单的抛弃，并且要求sender重发。
#### 发出的包
+ ```
  |<-  2 byte  ->|<-  4 byte  ->|
  |   checksum   |    ack seq   |
   ```
   就是简单的向下层发送ack包，表示自己收到的seq是多少，同时receiver收到此ack后会发送WINDOWS-SIZE数量的下一批包



## 超时设计
+ 在sender中每次发送包之前都会Sender_StartTimer(TIMEOUT);给包加上超时时间
+ 一旦超时就会选择重发WINDOWS-SIZE数量的包

## 测试结果

+ 成功通过文档中的两个test case

    ```
    rdt_sim 1000 0.1 100 0.15 0.15 0.15 0
    ## Reliable data transfer simulation with:
	simulation time is 1000.000 seconds
	average message arrival interval is 0.100 seconds
	average message size is 100 bytes
	average out-of-order delivery rate is 15.00%
	average loss rate is 15.00%
	average corrupt rate is 15.00%
	tracing level is 0
    Please review these inputs and press <enter> to proceed.

    At 0.00s: sender initializing ...
    At 0.00s: receiver initializing ...
    At 1674.39s: sender finalizing ...
    At 1674.39s: receiver finalizing ...

    ## Simulation completed at time 1674.39s with
        985914 characters sent
        985914 characters delivered
        35176 packets passed between the sender and the receiver
    ## Congratulations! This session is error-free, loss-free, and in order.

    ```

    ```
    rdt_sim 1000 0.1 100 0.3 0.3 0.3 0
    ## Reliable data transfer simulation with:
	simulation time is 1000.000 seconds
	average message arrival interval is 0.100 seconds
	average message size is 100 bytes
	average out-of-order delivery rate is 30.00%
	average loss rate is 30.00%
	average corrupt rate is 30.00%
	tracing level is 0
    Please review these inputs and press <enter> to proceed.

    At 0.00s: sender initializing ...
    At 0.00s: receiver initializing ...
    At 3125.82s: sender finalizing ...
    At 3125.82s: receiver finalizing ...

    ## Simulation completed at time 3125.82s with
        1003648 characters sent
        1003648 characters delivered
        45655 packets passed between the sender and the receiver
    ## Congratulations! This session is error-free, loss-free, and in order.

    ```
## 个人信息
+ 姓名：斯金泽 学号：517015910034 邮箱：sijinze1999@sjtu.edu.cn