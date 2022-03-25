# CPP-TCP: LAB 1

## 问题

**TCP 发送方将其字节流分成短段**（每个子串不超过1460字节），以便使它们各自符合数据报的要求。**但网络在传输数据时可能会将这些数据报重新排序、丢弃、或者重复发送**。接收方必须**将这些片段重新组合成它们开始时的连续字节流**。

**本实验需要编写负责重新组装的数据结构**，在实验代码中定义为 `StreamReassembler`。它将接收由一串字节组成的**子串**，以及该串在大流中的**第一个字节的索引**。流中的每个字节都有自己的唯一索引，从零开始递增。`StreamReassembler` 拥有一个用于输出的 `ByteStream`（在 lab0 实现），**一旦 `Reassembler` 知道流的下一个字节，就把它写入 `ByteStream` 中**。所有者可以随时访问并从 `ByteStream` 中读取。

代码中需要实现的 API 如下：

``` C++
class StreamReassembler {
   public:
    
    // 给定 capacity 构造流重组器。capacity 在运行过程中保持不变。
    StreamReassembler(const size_t capacity);
	
    // 接收一个子串，并将任何新的连续字节写入 ByteStream 中，同时保持在 capacity 的内存限制之内。
    // 超过容量的字节将被静默地丢弃。
    //
    // data:  子串
    // index: 表示 data 中第一个字节的索引（序列中的位置）。
    // eof:   为 true 时表示这个子串的最后一个字节是整个流中的最后一个字节。
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    // 返回已经收到但并未写入 ByteStream 中的字节数。
    size_t unassembled_bytes() const;
    
    // 没有已经收到但并未写入 ByteStream 中的字节时返回 true。
    bool empty() const;
};
```

## 要点

### 组装

组装就是把已经收到的字节排好序并写入 `ByteStream` 的过程。由于 `StreamReassembler` 收到的子串可能是乱序、断续、重叠（不只是重复）的，因此需要将多个数据包的数据整合，将其连接成一段有序连续的字节串，然后写入 `ByteStream`。

### `StreamReassembler` 需要维护一个字节缓冲

`ByteStream` 不允许随机写入，只能写到其已有数据的结尾，因此不是收到数据就写入，而需要先判断收到的数据中是否包含 `ByteStream` 所需的下一个字节，如有才可以写入。这样的过程必然需要维护一个字节缓冲。

### 尽可能组装

实验要求“*一旦 `Reassembler` 知道流的下一个字节，就把它写入 `ByteStream` 中*”。

### 正确处理 EOF

`StreamReassembler` 需要在合适的时机调用 `ByteStream::end_input()` 函数结束输入，向字节流表明此为数据的结尾。但 `StreamReassembler` 可能提前收到 `EOF` 标识，所以还要判断所有字节是否已被全部组装。若是则可结束输入。

### 正确处理 capacity

capacity 参数表示的含义是：字节流缓冲中未被读出部分的最大长度。”被读出“指已被 `ByteStream::read(n)` 输出。已被读出部分的长度可由 `ByteStream::bytes_read()` 获得。

因此，`StreamReassembler` 允许的最大索引为 `ByteStream::bytes_read() + capacity - 1 `，超出部分应当被丢弃，否则可能会过不了测试。

## 实现

``` C++
/* stream_reassembler.hh */
class StreamReassembler {
  private:
    ByteStream _output;
    size_t _capacity;
    
    bool _eofed;
    size_t _first_unread;
    size_t _first_unassembled;
    size_t _first_unacceptable;
    std::vector<char> _buffer;
    std::vector<size_t> _count;

  public:
    StreamReassembler(const size_t capacity);
    void push_substring(const std::string &data, const uint64_t index, const bool eof);
    const ByteStream &stream_out() const;
    ByteStream &stream_out();
    size_t unassembled_bytes() const;
    bool empty() const;
};

/* stream_reassembler.cc */
StreamReassembler::StreamReassembler(const size_t capacity) :
    _output(capacity), _capacity(capacity),
    _eofed(false),
    _first_unread(0), _first_unassembled(0), _first_unacceptable(0),
    _buffer(capacity, 0), _count(capacity, 0) {}

void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    _first_unread = _output.bytes_read();
    _buffer.resize(_first_unread + _capacity, 0);
    _count.resize(_first_unread + _capacity, 0);
    _eofed |= eof;

    for (size_t i = 0; i < data.length(); i++) {
        size_t real_index = index + i;
        _first_unacceptable = _first_unacceptable < (real_index + 1) ? (real_index + 1) : _first_unacceptable;
        if (real_index >= _buffer.size()) break;
        _buffer[real_index] = data[i];
        ++_count[real_index];
    }

    auto s = std::string();
    while (_count[_first_unassembled]) {
        if (_first_unassembled == _buffer.size()) break;
        s.push_back(_buffer[_first_unassembled]);
        ++_first_unassembled;
    }
    
    _output.write(s);

    if (_eofed && empty()) _output.end_input();
}

const ByteStream &StreamReassembler::stream_out() const { return _output; }
ByteStream &StreamReassembler::stream_out() { return _output; }

size_t StreamReassembler::unassembled_bytes() const {
    size_t count = 0;
    for (size_t i = _first_unassembled; i < _first_unacceptable; i++)
        count += !!_count[i];
    
    return count;
}

bool StreamReassembler::empty() const { 
    return _first_unacceptable == _first_unassembled;
}
```

