#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <iostream>
#include <cassert>
#include <functional>
#include <vector>
#include <queue>
#include <unordered_map>

class TCPSegmentWindow
{
  private:

    struct TCPSegmentForSender
    {
      TCPSegment segment{};
      uint32_t remaining_rto{0};
    };
    std::deque<TCPSegmentForSender> _buffer;

    size_t _first_index;
    size_t _window_size;
    size_t _first_not_acked;

    const TCPSegmentForSender& _get(const uint32_t index) const
    {
      auto real_index = index - _first_index;
      if (real_index >= _buffer.size())
      {
        std::cout << "Index " << index << " RealIndex " << real_index << " is out of bounds" << std::endl;
        throw std::out_of_range("TCPSegmentWindow::operator[]");
      }
      return _buffer[real_index];
    }
    TCPSegmentForSender& _get(const uint32_t index)
    {
      auto real_index = index - _first_index;
      if (real_index >= _buffer.size())
        throw std::out_of_range("TCPSegmentWindow::operator[]");
      return _buffer[real_index];
    }

  public:
    TCPSegmentWindow(const size_t window_size)
      : _buffer(0), _first_index(0), _window_size(window_size), _first_not_acked(0) {}

    TCPSegment& operator[](const uint32_t index)
    {
      return _get(index).segment;
    }

    bool acked(const uint32_t index) const
    {
      return _first_not_acked > index;
    }

    uint32_t remaining_rto(const uint32_t index) const
    {
      return _get(index).remaining_rto;
    }

    void set_remaining_rto(const uint32_t index, const uint32_t remaining_rto)
    {
      _get(index).remaining_rto = remaining_rto;
    }

    std::vector<size_t> tick(uint32_t ms_since_last_tick)
    {
      auto ret = std::vector<size_t>();
      for (size_t i = 0; i < valid_size(); i++)
      {
        if (acked(i + _first_index)) continue;
        auto seg = _get(i + _first_index);
        if (seg.remaining_rto <= ms_since_last_tick)
        {
          ret.push_back(i + _first_index);
        }
        seg.remaining_rto -= ms_since_last_tick;
      }
      return ret;
    }

    size_t valid_size() const
    {
      return std::min(_window_size, _buffer.size());
    }

    size_t bytes_in_flight() const
    {
      size_t ret = 0;
      if (_buffer.empty())
      { 
        return 0;
      }
      
      for (size_t index = _first_not_acked; index <= last_index(); index++)
      {
        ret += _get(index).segment.payload().size();
      }
      return ret;
    }

    size_t acked_size() const
    {
      return _first_not_acked - _first_index;
    }

    void advance(size_t step = 1)
    {
      expand_back(step);
      shrink_front(step);
    }

    size_t shrink_front(size_t step = 1)
    {
      if (_window_size < step)
      {
        throw std::out_of_range("TCPSegmentWindow::shrink_front");
      }
      _window_size -= step;
      _first_index += step;
      while (step--)
      {
        _buffer.pop_front();
      }
      return _window_size;
    }

    size_t expand_back(size_t step = 1)
    {
      _window_size += step;
      return _window_size;
    }

    void set_window_size(size_t window_size)
    {
      _window_size = window_size;
    }

    void push_back(const TCPSegment& s, uint32_t tick)
    {
      _buffer.push_back( TCPSegmentForSender{s, tick} );
    }

    void ack_received(WrappingInt32 ackno)
    {
      if (ackno.raw_value() - 1 >
       _get(last_index()).segment.header().seqno.raw_value() + _get(last_index()).segment.payload().size())
      {
        std::cout << "Ackno " << ackno.raw_value() << " is out of bounds" << std::endl;
        return;
      }
      while (_first_not_acked < _first_index + valid_size() &&
             _get(_first_not_acked).segment.header().seqno.raw_value() <= ackno.raw_value())
      {
        ++_first_not_acked;
      }
      std::cout << "_first_not_acked " << _first_not_acked << std::endl;
      advance(acked_size());
    }

    void clear()
    {
      _buffer.clear();
      _first_index = 0;
      _window_size = 0;
    }

    size_t first_index() const
    {
      return _first_index;
    }

    size_t last_index() const
    {
      return _first_index + (valid_size() > 0 ? valid_size() - 1 : 0);
    }

    size_t window_size() const
    {
      return _window_size;
    }

    size_t bytes_in_window() const
    {
      auto& s = _get(last_index());
      std::cout << s.segment.header().seqno.raw_value() << " + " << s.segment.payload().size() << " - " << _get(first_index()).segment.header().seqno.raw_value() << std::endl;
      return s.segment.header().seqno.raw_value() -
       _get(first_index()).segment.header().seqno.raw_value() +
        s.segment.payload().size();
    }
    size_t space() const
    {
      std::cout << "space()" << std::endl;
      std::cout << "window size " << _window_size << " buffer size " << _buffer.size() << std::endl;
      if (_window_size == 0)
      {
        return 0;
      }
      else if (_buffer.empty())
      {
        return _window_size;
      }
      
      if (_window_size <= bytes_in_window())
      {
        return 0;
      }

      return _window_size - bytes_in_window();
    }


    bool empty() const
    {
      return _buffer.empty();
    }

    bool closed() const
    {
      return _window_size == 0;
    }

    bool full() const
    {
      return space() == 0;
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

    size_t _rto;
    TCPSegmentWindow _segments_buffer;

    bool _sent_syn;

    void send(const TCPSegment& segment);

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
