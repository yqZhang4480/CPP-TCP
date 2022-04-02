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
    if(!_isSet){//没有接收到 isn 直接忽略所接收到的 segment
        return;
    }
    uint64_t index = unwrap(header.seqno, WrappingInt32(_isn), _checkpoint);
    uint64_t stream_index = index - 1;
    if(header.syn){
        stream_index += 1;
    }
    if(header.fin){
        _isEnded = true;
    }
    string data (seg.payload().str().data(), seg.payload().str().size());
    if(header.fin){
        _reassembler.push_substring(data, stream_index, true);
    }else{
        _reassembler.push_substring(data, stream_index, false);
    }

    if(index == 0 || index == _checkpoint){
        _checkpoint = index + seg.length_in_sequence_space();
        if(_checkpoint < _reassembler.left() + 1){
            _checkpoint = _reassembler.left() + 1;
            if(_isEnded&&_reassembler.distance() == 0){
                _checkpoint = _checkpoint + 1;
            }
        }
    }
    
}

std::optional<WrappingInt32> TCPReceiver::ackno() const {
    if(!_isSet){
        return nullopt;
    }else{
        std::optional<WrappingInt32> opt(wrap(_checkpoint , WrappingInt32(_isn)));
        return opt;
    }
}

size_t TCPReceiver::window_size() const { 
    return  _reassembler.capacity_value() - _reassembler.stream_out().buffer_size();
}