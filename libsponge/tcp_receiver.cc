#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();

    if (header.syn)
    {
        _isn = header.seqno.raw_value();
        _is_set = true;
        _checkpoint = 0;
    }

    // ignore segments if ISN is not set
    if (!_is_set)
    {
        return;
    }

    uint64_t index = unwrap(header.seqno, WrappingInt32(_isn), _checkpoint);
    uint64_t stream_index = index - 1; 
    if (header.syn)
    {
        stream_index += 1;
    }
    if (header.fin)
    {
        _ended = true;
    }
    string data (seg.payload().str().data(), seg.payload().str().size());
    _reassembler.push_substring(data, stream_index, header.fin);

    if (index == 0 || index == _checkpoint)
    {
        _checkpoint = index + seg.length_in_sequence_space();
        if (_checkpoint < _reassembler.left() + 1)
        {
            _checkpoint = _reassembler.left() + 1;
            if (_ended && _reassembler.distance() == 0)
            {
                _checkpoint = _checkpoint + 1;
            }
        }
    }
    
}

std::optional<WrappingInt32> TCPReceiver::ackno() const
{
    if (!_is_set)
    {
        return nullopt;
    }
    else
    {
        return wrap(_checkpoint, WrappingInt32(_isn));
    }
}

size_t TCPReceiver::window_size() const
{
    return  _reassembler.capacity_value() - _reassembler.stream_out().buffer_size();
}

TCPReceiver::ReceiverState TCPReceiver::state() const
{
    if (stream_out().error())
    {
        return ReceiverState::ERROR;
    }
    if (not ackno().has_value())
    {
        return ReceiverState::LISTEN;
    }
    else if (not stream_out().input_ended())
    {
        return ReceiverState::SYN_RECV;
    }
    else
    {
        return ReceiverState::FIN_RECV;
    }
}