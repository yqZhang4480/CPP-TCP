#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _channel(capacity + 1)
    , _capacity(capacity)
    , _read_index(0)
    , _write_index(0)
    , _bytes_read(0)
    , _bytes_written(0)
    , _end_flag(false) {}

size_t ByteStream::write(const string &data)
{
    if (_end_flag)
    {
        return 0;
    }

    uint32_t bytes_num = 0;
    while (!_buffer_full() && bytes_num < data.size())
    {
        _channel[_write_index] = data[bytes_num];
        _write_index = _next_index_of(_write_index);
        _bytes_written++;
        bytes_num++;
    }
    return bytes_num;
}

string ByteStream::peek_output(const size_t len) const
{
   std::string ret;
   size_t byte_num = 0;

   auto i = _read_index;
   // There is no data to read if the read index is equal to the write index.
   while(i != _write_index && byte_num < len)
   {
       ret.push_back(_channel[i]);
       i = _next_index_of(i);
       byte_num++;
   }
   return ret;
}

void ByteStream::pop_output(const size_t len)
{
    size_t bytes_num = min(len, buffer_size());

    _read_index = _next_index_of(_read_index, bytes_num);
    _bytes_read += bytes_num;
}

std::string ByteStream::read(const size_t len)
{
    std::string ret;

    size_t bytes_num = 0;
    while (!buffer_empty() && bytes_num < len)
    {
        ret.push_back(_channel[_read_index]);
        _read_index = _next_index_of(_read_index);
        _bytes_read++;
        bytes_num++;
    }

    return ret;
}

size_t ByteStream::_next_index_of(size_t index, size_t step) const
{
    return (index + step % _channel.size()) % _channel.size();
}

bool ByteStream::_buffer_full() const
{
    return _next_index_of(_write_index) == _read_index;
}

void ByteStream::end_input()
{
    _end_flag = true;
}

bool ByteStream::input_ended() const { return _end_flag; }

size_t ByteStream::buffer_size() const
{
    return (_channel.size() + _write_index - _read_index) % _channel.size();
}

bool ByteStream::buffer_empty() const { return _write_index == _read_index; }

bool ByteStream::eof() const { return _end_flag && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; } 

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
