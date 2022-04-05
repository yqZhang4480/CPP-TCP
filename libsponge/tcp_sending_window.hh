#include <functional>
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <vector>
#include <queue>

class TCPSendingWindow
{
  private:
    struct TCPSegmentForSender
    {
      TCPSegment segment{};
      uint32_t remaining_rto{0};
    };

    std::deque<TCPSegmentForSender> _buffer;

    WrappingInt32 _isn;

    uint64_t _first_seqno;
    uint64_t _window_size;
    uint64_t _next_seqno;

    void _shrink_front(size_t step = 1);
    void _expand_back(size_t step = 1);

    void _advance(size_t step = 1);

    void _set_window_size(size_t window_size);

  public:
    TCPSendingWindow(WrappingInt32 isn, size_t window_size);

    WrappingInt32 isn() const;

    void add_segment(const TCPSegment& segment, uint32_t rto);
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    std::vector<TCPSegment> tick(const size_t ms_elapsed);

    bool buffer_empty() const;
    size_t buffer_size() const;

    size_t window_size() const;
    bool window_closed() const;

    uint64_t first_seqno() const;
    uint64_t next_seqno() const;

    size_t bytes_in_flight() const;
    size_t space() const;
    bool full() const;

};