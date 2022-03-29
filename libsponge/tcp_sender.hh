#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <cassert>
#include <functional>
#include <queue>

struct TCPSegmentForSender
{
  TCPSegment segment;
  bool acked;
  uint32_t sent_tick;
}

class TCPSegmentWindow
{
  private:
    std::deque<TCPSegmentForSender> _buffer;
    size_t _first_index;
    size_t _window_size;

    TCPSegmentForSender& operator[] (const uint32_t index)
    {
      auto real_index = index - _first_index;
      if (real_index >= _buffer.size())
        throw std::out_of_range("TCPSegmentWindow::operator[]");
      return _buffer[real_index];
    }

  public:
    const TCPSegment& operator[] (const uint32_t index) const 
    {
      auto real_index = index - _first_index;
      if (real_index >= _buffer.size())
        throw std::out_of_range("TCPSegmentWindow::operator[]");
      return _buffer[real_index].segment;
    }

    uint32_t sent_tick(const uint32_t index) const
    {
      auto real_index = index - _first_index;
      if (real_index >= _buffer.size())
        throw std::out_of_range("TCPSegmentWindow::sent_tick");
      return _buffer[real_index].sent_tick;
    }

    size_t size() const
    {
      return _window_size;
    }

    size_t capcity() const
    {
      return _buffer.size();
    }

    size_t valid_size() const
    {
      return min(_window_size, _buffer.size());
    }

    size_t acked_size() const
    {
      for (size_t i = 0; i < valid_size(); i++)
      {
        if (!_buffer[i].acked)
          return i;
      }
      
      return _valid_size(); // all acked
    }

    void ack_received(const uint32_t ackno)
    {
      auto& seg = _buffer[ackno - _first_index];
      seg.acked = true;
    }

    void advance(size_t step = 1)
    {
      shrink_front(step);
      expend_back(step);
    }

    size_t shrink_front(size_t step = 1)
    {
      _first_index += step;
      while (step--)
      {
        _acked.pop_front();
        _buffer.pop_front();
      }
      return _first_index;
    }

    size_t expand_back(size_t step = 1)
    {
      _window_size += step;
      return _window_size;
    }

    void push_back(const TCPSegment& s, uint32_t tick)
    {
      _buffer.emplace_back({ s, false, tick });

    #ifdef NDEBUG
    #undef NDEBUG
    #define NDEBUG_UNDEF
    #endif
      assert(s.seqno() == _buffer.size() - 1 + _first_index);
    #ifdef NDEBUG_UNDEF
    #define NDEBUG
    #undef NDEBUG
    #endif
    }

    void clear()
    {
      _buffer.clear();
      _acked.clear();
      _first_index = 0;
      _window_size = 0;
    }

    uint32_t first_index() const
    {
      return _first_index;
    }

    uint32_t last_index() const
    {
      return _first_index + valid_size() - 1;
    }

    bool empty() const
    {
      return _window_size == 0;
    }

    bool full() const
    {
      return _window_size <= _buffer.size();
    }

    bool contains(const TCPSegment &segment) const
    {
      return contains(segment.header().seqno);
    }

    bool contains(const size_t index, const size_t length = 0) const
    {
      return index >= _first_index && index + length <= last_index();
    }
};


//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

    size_t _last_tick;
    TCPSegmentWindow _segmaents_buffer;
    size_t _window_left;
    size_t _window_right;
    size_t _first_not_acked;

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
