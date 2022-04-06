# CPP-TCP: LAB 3

## 问题

TCP是一个协议，通过不可靠的数据报可靠地传递一对流量控制的字节流（每个方向一个）。两方参与TCP连接，每一方同时作为 "发送方"（其自身发出的字节流）和 "接收方"（传入的字节流）。

TCP 发送方和接收方各自负责 TCPSegment 的一部分。TCP 发送方写入 LAB2 中与 TCP 接收方相关的 TCPSegment 的所有字段：即序列号、SYN 标志、有效载荷和 FIN 标志。然而，TCP发送方只读取由接收方写入的段中的字段：ackno 和窗口大小。

TCPSender 的功能有：

- 跟踪接收方的窗口（处理传入的acknos和窗口大小）。
- 尽可能从 ByteStream 中读取数据并创建新的TCP段（如果需要的话，包括SYN和FIN标志），并发送之，来填充窗口。发送方应不断发送数据段，直到窗口满了或 ByteStream 空了。
- 追踪哪些段已经发送但尚未被接收方完全确认（我们称这些段为未完成的段）。
- 如果已经发送了足够长的时间，但还没有被确认，就重新发送未完成的段。

代码中需要实现的 API 如下：

``` C++
class TCPSender {
   public:
    // 告知 TCPSender 收到了 ACK
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    // 发送一个空段
    void send_empty_segment();
	
    // 创建尽可能多的数据段，直到填满窗口或读空 ByteStream。
    void fill_window();
	
    // 告知 TCPSender 流逝的时间
    void tick(const size_t ms_since_last_tick);
    
    // 返回有多少字节的数据（包括 SYN/FIN）已发送但未被确认
    size_t bytes_in_flight() const;
    
    // 返回已经连续进行了多少次重传
    unsigned int consecutive_retransmissions() const;
};
```

## 细节

### window size 是字节数不是段数

看了两遍文档才发现，很蠢的错误了。

### ByteStream 为空的条件：`buffer_empty() && !eof()`

以下的循环控制语句可能会导致不发送 FIN：

 ``` C++
while(!_stream.buffer_empty() && !_window.full()) {...}
 ```

正确的处理是：

 ``` C++
while((!_stream.buffer_empty() || _stream.eof()) && !_window.full()) {...}
 ```

### 填满窗口

填充窗口时，从流中读取的字节数应为 `_window.space(), TCPConfig::MAX_PAYLOAD_SIZE, _stream.buffer_size()` 三者中的最小值（`_window.space()` 为窗口剩余空间）。

如果读到 EOF，应当尽量将 FIN 附在已有段上，除非窗口已满。

### 超时重传

当发生超时重传时有以下规则：

* 重传最早的未完成段。
* 如果**窗口大小不为零**：
  * 增加连续重传次数。
  * RTO 变为原来的二倍。
* 以上几条完成后，重置重传计时器。
* 当 ACK 了**新的**数据：
  * RTO 变为初始值。
  * 重置重传计时器。

### ACK 包的处理

有一些特殊情况值得注意：

* 如果 ackno 在**窗口外**，则丢弃之。
* 如果 ackno 在**窗口最左端**，应当根据此包设置新的窗口大小。（依据具体实现，这种情况可能不需要单独考虑。）
* **当且仅当**在以上两种情况下，收到的 ACK 包**没有确认新的数据**。
* 发出的段可能被**部分** ACK。此时这个段依然是未完成的段。