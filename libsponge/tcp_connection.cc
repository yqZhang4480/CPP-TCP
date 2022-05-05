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
    cout << "== TCPConnecton::segment_received ==" << endl;
    cout << TCPState(_sender, _receiver, _active, _linger_after_streams_finish).name() << endl;

    _time_since_last_segment_received = 0;
    
    if (seg.header().rst)
    {
        _reset_received();

        return;
    }

    auto state = TCPState(_sender, _receiver, _active, _linger_after_streams_finish);

    // ********************************************************
    // CONNECTING
    // ********************************************************
    // case 1:
    //         |CLOSED |SYN_SENT        |ESTABLISHED          >
    // peer A: +-------+----------------+--------------------->
    //                  \              / \       ^ ^ ^ ^ ^ ^ ^
    //                   --S--   --SA--   --A--  | | | | | | |
    //                        \ /              \ v v v v v v v
    // peer B: +-------+-------+----------------+------------->
    //         |CLOSED |LISTEN |SYN_RCVD        |ESTABLISHED  >
    // ********************************************************
    // case 2:
    //         |CLOSED |SYN_SENT |SYN_RCVD |ESTABLISHED  >
    // peer A: +-------+---------+---------+------------->
    //                  \       / \       / ^ ^ ^ ^ ^ ^ ^
    //                   ---S---   ---A---  | | | | | | |
    //                  /       \ /       \ v v v v v v v
    // peer B: +-------+---------+---------+------------->
    //         |CLOSED |SYN_SENT |SYN_RCVD |ESTABLISHED  >
    // ********************************************************

    // peer B: LISTEN -> SYN_RCVD
    if (state == TCPState(TCPState::State::LISTEN))
    {
        cout << "TCPConnection::segment_received: LISTEN -> SYN_RCVD" << endl;
        if (seg.header().syn)
        {
            _receiver.segment_received(seg);
            connect();

            _active = true;
        }
    }
    else if (state == TCPState(TCPState::State::SYN_SENT))
    {
        // peer A: SYN_SENT -> ESTABLISHED
        if (seg.header().syn and seg.header().ack)
        {
            cout << "TCPConnection::segment_received: SYN_SENT -> ESTABLISHED" << endl;
            _receiver.segment_received(seg);
            _sender.ack_received(seg.header().ackno, seg.header().win);
            
            TCPSegment ack_seg;
            _add_ack_info(ack_seg);

            _segments_out.push(ack_seg);

        }
        // peer AB: SYN_SENT -> SYN_RCVD
        else if (seg.header().syn)
        {
            cout << "TCPConnection::segment_received: SYN_SENT -> SYN_RCVD" << endl;
            _receiver.segment_received(seg);

            TCPSegment ack_seg;
            _add_ack_info(ack_seg);
            _segments_out.push(ack_seg);
        }
    }
    // peer B: SYN_RCVD -> ESTABLISHED
    else if (state == TCPState(TCPState::State::SYN_RCVD))
    {
        cout << "TCPConnection::segment_received: SYN_RCVD -> ESTABLISHED" << endl;
        if (seg.header().ack)
        {
            _receiver.segment_received(seg);
            _sender.ack_received(seg.header().ackno, seg.header().win);
        }
    }

    // ********************************************************
    // ESTABLISHED
    // ********************************************************

    // ESTABLISHED or peer A: FIN_WAIT_1 -> FIN_WAIT_2/TIME_WAIT/CLOSING
    else if (state == TCPState(TCPState::State::ESTABLISHED) || state == TCPState(TCPState::State::FIN_WAIT_1))
    {
        cout << "TCPConnection::segment_received: ESTABLISHED OR FIN_WAIT_1 -> FIN_WAIT_2" << endl;
        
        if (seg.header().fin)
        {
            // ESTABLISHED -> CLOSED_WAIT
            if (state != TCPState(TCPState::State::FIN_WAIT_1))
            {
                _linger_after_streams_finish = false;
            }
        }

        if (seg.length_in_sequence_space() != 0 or !seg.header().ack)
        {
            _receiver.segment_received(seg);

            TCPSegment ack_seg;
            _add_ack_info(ack_seg);
            _sender.segments_out().push(ack_seg);

            _collect_sender_segments_out();
        }

        if (seg.header().ack)
        {
            _sender.ack_received(seg.header().ackno, seg.header().win);
            _collect_sender_segments_out();
        }


    }

    /* ********************************************************
    // CLOSING
    // ********************************************************
    // case 1:
    //         >ESTABLISHED |FIN_WAIT_1 |FIN_WAIT_2 |TIME_WAIT |CLOSED
    // peer A: >------------+-----------+-----------+----------+
    //          ^ ^ ^ ^ ^ ^  \         /           / \
    //          | | | | | |   -F-   -A-          FA   -A-
    //          v v v v v v      \ /            /        \
    // peer B: >------------------+------------+----------+
    //         >ESTABLISHED       |CLOSED_WAIT |LAST_ACK  |CLOSED
    // ********************************************************
    // case 2:
    //         >ESTABLISHED |FIN_WAIT_1 |CLOSING  |TIME_WAIT |CLOSED
    // peer A: >------------+-----------+---------+----------+
    //         ^ ^ ^ ^ ^ ^ ^ \         / \       / 
    //         | | | | | | |  ----F----   ---A---  
    //         v v v v v v v /         \ /       \ 
    // peer B: >------------+-----------+---------+----------+
    //         >ESTABLISHED |FIN_WAIT_1 |CLOSING  |TIME_WAIT |CLOSED
    // ********************************************************/
    
    // peer A: FIN_WAIT_2 -> TIME_WAIT
    else if (state == TCPState(TCPState::State::FIN_WAIT_2))
    {
        cout << "TCPConnection::segment_received: FIN_WAIT_2 -> TIME_WAIT" << endl;
        if (seg.header().fin and seg.header().ack)
        {
            _receiver.segment_received(seg);
            _sender.ack_received(seg.header().ackno, seg.header().win);
            _collect_sender_segments_out();
        }

        // ...
    }
    // peer B: LAST_ACK -> CLOSED
    else if (state == TCPState(TCPState::State::LAST_ACK))
    {
        cout << "TCPConnection::segment_received: LAST_ACK -> CLOSED" << endl;
        if (seg.header().ack)
        {
            _linger_after_streams_finish = false;
            _active = false;
        }
    }
    // peer AB: CLOSING -> TIME_WAIT
    else if (state == TCPState(TCPState::State::CLOSING))
    {
        cout << "TCPConnection::segment_received: CLOSING -> TIME_WAIT" << endl;
        if (seg.header().ack)
        {
            _receiver.segment_received(seg);
            _sender.ack_received(seg.header().ackno, seg.header().win);
            _collect_sender_segments_out();
        }
    }

    if (_receiver.ackno().has_value() and
        (seg.length_in_sequence_space() == 0) and
        seg.header().seqno == _receiver.ackno().value() - 1)
    {
        _sender.send_empty_segment();
        _collect_sender_segments_out();
    }
        
    cout << TCPState(_sender, _receiver, _active, _linger_after_streams_finish).name() << endl;
    cout << "== TCPConnecton::segment_received ENDED ==" << endl;

}

bool TCPConnection::active() const
{
    return _active;
}

size_t TCPConnection::write(const string &data)
{
    auto ret = outbound_stream().write(data);
    _collect_sender_segments_out();
    return ret;
}

void TCPConnection::tick(const size_t ms_since_last_tick)
{
    _time_since_last_segment_received += ms_since_last_tick;
    auto state = TCPState(_sender, _receiver, _active, _linger_after_streams_finish);
    if (state == TCPState(TCPState::State::TIME_WAIT) and
        _linger_after_streams_finish and _time_since_last_segment_received >= 10 * _cfg.rt_timeout)
    {
        cout << "TCPConnection::tick: LINGER_AFTER_STREAMS_FINISH" << endl;
        _linger_after_streams_finish = false;
        _active = false;
    }

    _sender.tick(ms_since_last_tick);

    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS)
    {
        cout << "TCPConnection::tick: MAX_RETX_ATTEMPTS" << endl;
        _unclean_shutdown();
        return;
    }
    if (state == TCPState(TCPState::State::ESTABLISHED) || state == TCPState(TCPState::State::FIN_WAIT_1))
    {
        _collect_sender_segments_out();
    }
}

void TCPConnection::_collect_sender_segments_out()
{
    cout << "TCPConnecton::_collect_sender_segments_out, size:" << _sender.segments_out().size() << endl;
    _sender.fill_window();
    auto& _sender_queue = _sender.segments_out();
    TCPSegment segment;
    while (!_sender_queue.empty())
    {
        segment = _sender_queue.front();
        _sender_queue.pop();
        if (_receiver.ackno().has_value())
        {
            cout <<"ackno: "<<_receiver.ackno().value() << ", size: " << _sender_queue.size() << endl;
            _add_ack_info(segment);
        }
       _segments_out.push(segment);
    }
}

void TCPConnection::end_input_stream()
{
    outbound_stream().end_input();
    _collect_sender_segments_out();
}

void TCPConnection::connect()
{
    cout << "connect" << endl;
    _collect_sender_segments_out();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            _sender.send_empty_segment();
            _unclean_shutdown();
            // Your code here: need to send a RST segment to the peer
            
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::_add_ack_info(TCPSegment& ack_seg)
{
    cout << "TCPConnecton::_add_ack_info" << endl;

    ack_seg.set_ack(true);
    ack_seg.set_ackno(_receiver.ackno().value());
    ack_seg.set_window_size(_receiver.window_size());
}

void TCPConnection::_unclean_shutdown()
{
    cout << "TCPConnecton::_unclean_shutdown" << endl;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
    _linger_after_streams_finish = false;

    TCPSegment rst_seg;
    rst_seg.header().rst = true;
    _segments_out.push(rst_seg);
}

void TCPConnection::_clean_shutdown()
{
    _unclean_shutdown();
}

// Test #39-T1-E10
// TCP is expected
// in state sender=`error (connection was reset)`, receiver=`error (connection was reset)`, active=0, linger_after_streams_finish=0
// when a segment is received with the RST flag set
void TCPConnection::_reset_received()
{
    cout << "TCPConnecton::_reset_received" << endl;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();

    _active = false;
    _linger_after_streams_finish = false;

}