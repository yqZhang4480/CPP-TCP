#include "stream_reassembler.hh"
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) :
    _output(capacity), _capacity(capacity),
    _eofed(false),
    _first_unassembled(0), _first_unacceptable(0),
    _buffer(capacity, 0), _count(capacity, 0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof)
{
    _first_unacceptable = _output.bytes_read() + _capacity;
    _buffer.resize(_capacity - _output.buffer_size(), 0);
    _count.resize(_capacity - _output.buffer_size(), 0);
    if (index + data.size() <= _first_unacceptable)
    {
        _eofed |= eof;
    }

    _put(data, index);
    _assemble();

    if (_eofed && empty())
    {
        _output.end_input();
    }
}

void StreamReassembler::_put(const string &data, const size_t index)
{
    for (size_t i = 0; i < data.length(); i++)
    {
        size_t real_index = index + i;
        if (real_index < _first_unassembled)
        {
            continue;
        }
        if (real_index >= _first_unacceptable)
        {
            break;
        }
        size_t _buffer_index = real_index - _first_unassembled;
        assert(_buffer_index < _buffer.size());
        _buffer[_buffer_index] = data[i];
        ++_count[_buffer_index];
    }
}

void StreamReassembler::_assemble()
{
    auto s = std::string();
    while (_count.front() && _output.buffer_size() < _capacity && !_buffer.empty())
    {
        s.push_back(_buffer.front());
        _count.pop_front();
        _buffer.pop_front();
        ++_first_unassembled;
    }
    _output.write(s);
}


const ByteStream &StreamReassembler::stream_out() const { return _output; }
ByteStream &StreamReassembler::stream_out() { return _output; }

size_t StreamReassembler::unassembled_bytes() const
{
    size_t count = 0;

    for (const auto& n : _count)
    {
        count += !!n;
    }

    return count;
}

bool StreamReassembler::empty() const
{
    return unassembled_bytes() == 0;
}
