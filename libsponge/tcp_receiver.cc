#include "tcp_receiver.hh"
#include <iostream>
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
        cout<<"sqno:"<<header.seqno.raw_value()<<endl;
        cout<<"_isn:"<<_isn<<"\n";
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
        _reassembler.push_substring(seg.payload().str().data(), stream_index, true);
    }else{
        _reassembler.push_substring(seg.payload().str().data(), stream_index, false);
    }
    cout<<"index: "<<index<<"\n";
    if(index + seg.length_in_sequence_space() >= _checkpoint){
        _checkpoint  = index + seg.length_in_sequence_space();
    }

}

std::optional<WrappingInt32> TCPReceiver::ackno() const {
    if(!_isSet){
        return nullopt;
    }else{
        cout<<"unassembled_bytes:"<<_reassembler.unassembled_bytes()<<"\n";
        cout<<"_checkpoint:"<<_checkpoint<<"\n";
        std::optional<WrappingInt32> opt(wrap( _checkpoint - _reassembler.distance(), WrappingInt32(_isn)));
        return opt;
    }
}

size_t TCPReceiver::window_size() const { 

    return  _reassembler.capacity_value() - _reassembler.stream_out().buffer_size();
}