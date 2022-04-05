#include "tcp_sending_window.hh"

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
void TCPSendingWindow::ack_received(const WrappingInt32 ackno, const uint16_t window_size)
{
    if (unwrap(ackno, _isn, _next_seqno) > _next_seqno ||
     unwrap(ackno, _isn, _next_seqno) < _first_seqno)
    {
        return;
    }
    if (buffer_empty())
    {
        return;
    }

    size_t i = buffer_size() - 1;
    for (; i != size_t(-1); --i)
    {
        auto& s = _buffer[i];

        if (unwrap(ackno, _isn, _next_seqno) > unwrap(s.segment.header().seqno, _isn, _next_seqno))
        {
            break;
        }
    }

    _buffer.erase(_buffer.begin(), _buffer.begin() + i + 1);

    _advance(unwrap(ackno, _isn, _next_seqno) - _first_seqno);
    _set_window_size(window_size);
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