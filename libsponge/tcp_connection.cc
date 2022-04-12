#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const
{
    return outbound_stream().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const
{
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const
{
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const
{
    return _time_since_last_segment_received;
}

void TCPConnection::segment_received(const TCPSegment &seg)
{
    _time_since_last_segment_received = 0;
    
    if (seg.header().rst)
    {
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();

        return;
    }

    _receiver.segment_received(seg);

    if (seg.header().ack)
    {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    if (seg.length_in_sequence_space())
    {
        TCPSegment ack_seg;
        ack_seg.header().ack = true;
        ack_seg.header().ackno = _receiver.ackno();
        ack_seg.header().win = _receiver.window_size();
        _segments_out.push(ack_seg);
    }

    if (_receiver.ackno().has_value() and
        (seg.length_in_sequence_space() == 0) and
        seg.header().seqno == _receiver.ackno().value() - 1)
    {
        _sender.send_empty_segment();
    }
}

bool TCPConnection::active() const
{
    return true;
}

size_t TCPConnection::write(const string &data)
{
    return outbound_stream().write(data);
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick)
{
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);

    if (_sender.consecutive_retransmissions() > _cfg.max_retransmissions)
    {
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();

        TCPSegment rst_seg;
        rst_seg.header().rst = true;
        _segments_out.push(rst_seg);
    }

    _sender.fill_window();
}

void TCPConnection::end_input_stream()
{
    outbound_stream().end_input();
}

void TCPConnection::connect()
{
    _sender.fill_window();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
