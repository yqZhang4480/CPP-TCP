#include "tcp_receiver.hh"
// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();

    if(header.syn)
    {
        _syn_received = true;
        _isn = header.seqno.raw_value();
    }
    if(!_syn_received)
    {
        return;
    }
    uint64_t index = unwrap(header.seqno, WrappingInt32(_isn), _checkpoint);
    uint64_t stream_index = header.syn ? index : index - 1;
    string data (seg.payload().str().data(), seg.payload().str().size());
    _reassembler.push_substring(data, stream_index, header.fin);

    _checkpoint = stream_out().bytes_written() + 1;
    if(stream_out().input_ended())
    {
        _checkpoint += 1;
    }
}

std::optional<WrappingInt32> TCPReceiver::ackno() const {
    if(!_syn_received)
    {
        return nullopt;
    }
    return(wrap(_checkpoint, WrappingInt32(_isn)));
}

size_t TCPReceiver::window_size() const {
    return  _capacity - stream_out().buffer_size();
}