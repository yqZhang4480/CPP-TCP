#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>
#include <iostream>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _rto(retx_timeout)
    , _segments_buffer(1)
    , _sent_syn{false}
    {
        cout << "<< TCPSender::TCPSender() >>" << endl;
    }

uint64_t TCPSender::bytes_in_flight() const
{
    auto ret = _segments_buffer.bytes_in_flight();
    cout << "bytes_in_flight " << ret << endl;
    return ret;
}

void TCPSender::send(const TCPSegment &segment)
{
    _segments_out.push(segment);
}

void TCPSender::fill_window()
{
    cout << "fill_window()" << endl;
    if (!_sent_syn && _stream.bytes_read() == 0)
    {
        auto segment = TCPSegment{};
        segment.set_seqno(_isn + _next_seqno);
        segment.set_fin(_stream.eof());
        segment.set_syn(true);
        _next_seqno += 1;

        _segments_buffer.push_back(segment, _rto);
        send(segment);
        _sent_syn = true;

        cout << "Sending SYN " << "ISN " << _isn << endl;
    }
    
    if (_segments_buffer.full())
    {
        cout << "Window full" << endl;
    }
    

    while (!_segments_buffer.full())
    {
        if (_stream.buffer_empty())
        {
            cout << "Buffer empty" << endl;
            break;
        }

        cout << "Filling window" << endl;
        auto segment = TCPSegment{};
        segment.set_seqno(_isn + _next_seqno);
        _next_seqno += _stream.buffer_size();
        if (_stream.eof())
        {
            _next_seqno += 1;
            segment.set_fin(true);
            cout << "Fin set" << endl;
        }
        segment.set_syn(false);
        segment.set_payload(_stream.read(min(_segments_buffer.space(), min(TCPConfig::MAX_PAYLOAD_SIZE, _stream.buffer_size()))));
        cout << "Sending segment with seqno " << segment.header().seqno << endl;

        _segments_buffer.push_back(segment, _rto);
        send(segment);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size)
{
    cout << "Received ACK with ackno " << ackno << " and window size " << window_size << endl;
    _segments_buffer.ack_received(ackno);

    if (_segments_buffer.window_size() < window_size)
    {
        _segments_buffer.set_window_size(window_size);
    }
    cout << "window size: " << _segments_buffer.window_size() << endl;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of millisconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick)
{ 
    cout << "Tick " << ms_since_last_tick << endl;
    fill_window();

    auto need_retransmission = _segments_buffer.tick(ms_since_last_tick);

    for (auto&& index : need_retransmission)
    {
        auto segment = _segments_buffer[index];
        send(segment);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment()
{
    TCPSegment seg;
    seg.set_seqno(WrappingInt32(_next_seqno));
    _segments_out.push(seg);
}
