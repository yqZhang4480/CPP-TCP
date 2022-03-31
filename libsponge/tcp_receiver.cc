#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    if(header.syn){
        _isn = header.seqno.raw_value();
        _isSet = true;
        _checkpoint = 0;
    }
    uint64_t index = unwrap(header.seqno, WrappingInt32(_isn), _checkpoint);
    if(header.fin){
        if(index == 0){
            _reassembler.push_substring(seg.payload().str().data(), index, true);
        }else{
            _reassembler.push_substring(seg.payload().str().data(), index - 1, true);
        }
    }else{
        if(index == 0){
            _reassembler.push_substring(seg.payload().str().data(), index, false);
        }else{
            _reassembler.push_substring(seg.payload().str().data(), index - 1, false);
        }
    }

    _checkpoint = index + seg.length_in_sequence_space();
}

std::optional<WrappingInt32> TCPReceiver::ackno() const {
    if(!_isSet){
        return nullopt;
    }else{
        std::optional<WrappingInt32> opt(wrap(_reassembler.first_unassembled_value() + 1, WrappingInt32(_isn)));
        return opt;
    }
}

size_t TCPReceiver::window_size() const { return _reassembler.first_unaccepted_value() - _reassembler.first_unassembled_value();}