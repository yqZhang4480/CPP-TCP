#include "tcp_sending_window.hh"
#include <iostream>
#define PRIVATE

PRIVATE
void TCPSendingWindow::_shrink_front(size_t step)
{
    if (_window_size < step)
    {
        throw std::runtime_error("Cannot shrink window by more than its size");
    }
    _window_size -= step;
    _first_seqno += step;
}
PRIVATE
void TCPSendingWindow::_expand_back(size_t step)
{
    _window_size += step;
}
PRIVATE
void TCPSendingWindow::_advance(size_t step)
{
    _expand_back(step);
    _shrink_front(step);
}
PRIVATE
void TCPSendingWindow::_set_window_size(size_t window_size)
{
    this->_window_size = window_size;
}

TCPSendingWindow::TCPSendingWindow(WrappingInt32 isn, size_t window_size)
    : _buffer(0)
    , _isn(WrappingInt32{isn})
    , _first_seqno(0)
    , _window_size(window_size)
    , _next_seqno(0)
{}

WrappingInt32 TCPSendingWindow::isn() const
{
    return _isn;
}

void TCPSendingWindow::add_segment(const TCPSegment& segment, uint32_t rto)
{
    if (full())
    {
        throw std::runtime_error("Cannot add segment to full window");
    }

    _buffer.emplace_back(TCPSegmentForSender{segment, rto});
    _next_seqno = unwrap(segment.header().seqno, _isn, _next_seqno) + segment.length_in_sequence_space();
}
bool TCPSendingWindow::ack_received(const WrappingInt32 ackno, const uint16_t window_size)
{
    const auto ack_seqno = unwrap(ackno, _isn, _next_seqno);
    if (ack_seqno > _next_seqno || ack_seqno < _first_seqno)
    {
        return false;
    }
    if (ack_seqno == _first_seqno)
    {
        _set_window_size(window_size);
        return false;
    }

    // not buffer_empty() --
    // a segment might be acked partially so that it not be erased from the buffer.
    // buffer_empty() has to return true if the statment below returns true.
    if (_next_seqno == _first_seqno)
    {
        return true;
    }

    size_t i = buffer_size() - 1;
    for (; i != size_t(-1); --i)
    {
        auto& s = _buffer[i];

        if (ack_seqno > unwrap(s.segment.header().seqno, _isn, _next_seqno))
        {
            break;
        }
    }

    _buffer.erase(_buffer.begin(), _buffer.begin() + i);
    if (ack_seqno >= 
        unwrap(_buffer[0].segment.header().seqno, _isn, _next_seqno) +
        _buffer[0].segment.length_in_sequence_space())
    {
        _buffer.erase(_buffer.begin(), _buffer.begin() + 1);
    }

    _advance(unwrap(ackno, _isn, _next_seqno) - _first_seqno);
    _set_window_size(window_size);
    return true;
}

std::vector<TCPSegment> 
TCPSendingWindow::tick(const size_t ms_elapsed)
{
    std::vector<TCPSegment> ret;

    for (auto &segment : _buffer)
    {
        if (segment.remaining_rto > ms_elapsed)
        {
            segment.remaining_rto -= ms_elapsed;
        }
        else
        {
            ret.push_back(segment.segment);
        }
    }

    return ret;
}
void TCPSendingWindow::reset_timer(size_t rto)
{
    for (auto &segment : _buffer)
    {
        segment.remaining_rto = rto;
    }
}

bool TCPSendingWindow::buffer_empty() const
{
    return _buffer.empty();
}
size_t TCPSendingWindow::buffer_size() const
{
    return _buffer.size();
}

uint64_t TCPSendingWindow::first_seqno() const
{
    return _first_seqno;
}
uint64_t TCPSendingWindow::next_seqno() const
{
    return _next_seqno;
}

bool TCPSendingWindow::window_closed() const
{
    return _window_size == 0;
}
size_t TCPSendingWindow::window_size() const
{
    return _window_size;
}

size_t TCPSendingWindow::bytes_in_flight() const
{
    return _next_seqno - _first_seqno;
}
size_t TCPSendingWindow::space() const
{
    if (_window_size < bytes_in_flight())
    {
        return 0;
    }
    std::cout << "window size: " << _window_size << " bytes in flight: " << bytes_in_flight() << std::endl;
    return _window_size - bytes_in_flight();
}
bool TCPSendingWindow::full() const
{
    if (window_closed())
    {
        return true;
    }

    if (buffer_empty())
    {
        return false;
    }

    return space() <= 0;
}