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
    : _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _rto(retx_timeout)
    , _window(fixed_isn.value_or(WrappingInt32{random_device()()}), 1)
    , _consecutive_retransmission_count{0}
    , _window_size_is_zero{false}
    {}

uint64_t TCPSender::bytes_in_flight() const
{
    return _window.bytes_in_flight();
}

void TCPSender::_send(const TCPSegment &segment)
{
    _segments_out.push(segment);
}

void TCPSender::fill_window()
{
    if (state() == SenderState::CLOSED)
    {
        // CLOSED -> SYN_SENT
        TCPSegment segment;
        segment.set_syn(true);
        segment.set_seqno(wrap(_window.next_seqno(), _window.isn()));

        _window.add_segment(segment, _rto);
        _send(segment);
    }

    while((!_stream.buffer_empty() || _stream.eof()) && !_window.full())
    {
        // FIN_SENT or FIN_ACKED
        if (state() == SenderState::FIN_SENT or state() == SenderState::FIN_ACKED)
        {
            break;
        }

        TCPSegment s;
        s.set_seqno(wrap(_window.next_seqno(), _window.isn()));
        s.set_payload(_stream.read(min(_window.space(), min(TCPConfig::MAX_PAYLOAD_SIZE, _stream.buffer_size()))));
        
        s.set_fin(_stream.eof() && s.payload().size() < _window.space());

        if (s.length_in_sequence_space() != 0)
        {
            _window.add_segment(s, _rto);
            _send(s);
        }
        else
        {
            break;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size)
{
    cout << "TCPSender::ack_received(ackno, win): " << ackno << " " << window_size << endl;
    _window_size_is_zero = window_size == 0;
    // When filling window, treat a '0' window size as equal to '1'
    auto ret = _window.ack_received(ackno, max(window_size, uint16_t(1)));

    // if we have acked some NEW data, reset the RTO and timers
    if (ret)
    {
        _rto = _initial_retransmission_timeout;
        _window.reset_timer(_rto);
        _consecutive_retransmission_count = 0;
    }

    fill_window();
}

//! \param[in] ms_since_last_tick the number of millisconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick)
{
    auto ret = _window.tick(ms_since_last_tick);

    if (ret.empty())
    {
        return;
    }

    // When filling window, treat a '0' window size as equal to '1'
    // but don't back off RTO
    if (!_window_size_is_zero)
    {
        _rto *= 2;
    }
    _window.reset_timer(_rto);
    
    _consecutive_retransmission_count++;
    _send(ret[0]);
}

unsigned int TCPSender::consecutive_retransmissions() const
{
    return _consecutive_retransmission_count;
}

void TCPSender::send_empty_segment()
{
    TCPSegment segment;
    segment.set_seqno(wrap(_window.next_seqno(), _window.isn()));
    _send(segment);
}

TCPSender::SenderState TCPSender::state() const
{
    if (stream_in().error())
    {
        return SenderState::ERROR;
    }
    if (next_seqno_absolute() == 0)
    {
        return SenderState::CLOSED;
    }
    if (next_seqno_absolute() > 0 and next_seqno_absolute() == bytes_in_flight())
    {
        return SenderState::SYN_SENT;
    }
    if (next_seqno_absolute() > bytes_in_flight() and not stream_in().eof())
    {
        return SenderState::SYN_ACKED;
    }
    if (stream_in().eof() and next_seqno_absolute() < stream_in().bytes_written() + 2)
    {
        return SenderState::SYN_ACKED;
    }
    if (stream_in().eof() and next_seqno_absolute() == stream_in().bytes_written() + 2 and bytes_in_flight() > 0)
    {
        return SenderState::FIN_SENT;
    }
    else
    {
        return SenderState::FIN_ACKED;
    }
}