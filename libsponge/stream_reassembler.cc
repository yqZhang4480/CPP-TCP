#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) :
    _output(capacity), _capacity(capacity), _datas(), _data_begin_end_map(), _first_unread(0), _first_unassembled(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof)
{
    DUMMY_CODE(data, index, eof);

    _datas.insert({index, data});
    _data_begin_end_map.insert({index, index + data.size()});

    while (true)
    {
        auto it = _data_begin_end_map.find(_first_unassembled);

        if (it == _data_begin_end_map.end())
        {
            break;
        }

        _output.write(_datas[_first_unassembled]);
        _first_unassembled = (*it).second;
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
    return _capacity - _first_unassembled;
}

bool StreamReassembler::empty() const
{
    return unassembled_bytes() == 0;
}
