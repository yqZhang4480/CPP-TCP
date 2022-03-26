#include "stream_reassembler.hh"
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) :
    _output(capacity), _capacity(capacity),
    _eofed(false),
    _first_unread(0), _first_unassembled(0), _first_unacceptable(0),
    _buffer(capacity, 0), _count(capacity, 0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof)
{
    DUMMY_CODE(data, index, eof);

    _first_unread = _output.bytes_read();
    _buffer.resize(_first_unread + _capacity, 0);
    _count.resize(_first_unread + _capacity, 0);
    _eofed |= eof;

    for (size_t i = 0; i < data.length(); i++)
    {
        size_t real_index = index + i;
        _first_unacceptable = _first_unacceptable < (real_index + 1) ? (real_index + 1) : _first_unacceptable;
        if (real_index >= _buffer.size())
        {
            break;
        }
        _buffer[real_index] = data[i];
        ++_count[real_index];
    }


    auto s = std::string();
    while (_count[_first_unassembled])
    {
        if (_first_unassembled == _buffer.size())
        {
            break;
        }
        s.push_back(_buffer[_first_unassembled]);
        ++_first_unassembled;
    }
    
    _output.write(s);

    if (_eofed && empty())
    {
        _output.end_input();
    }
    
}

const ByteStream &StreamReassembler::stream_out() const
{
    return _output;
}
ByteStream &StreamReassembler::stream_out()
{
    return _output;
}

size_t StreamReassembler::unassembled_bytes() const
{
    size_t count = 0;

    for (size_t i = _first_unassembled; i < _first_unacceptable; i++)
    {
        count += !!_count[i];
    }
    
    return count;
}

bool StreamReassembler::empty() const
{
    return _first_unacceptable == _first_unassembled;
}
