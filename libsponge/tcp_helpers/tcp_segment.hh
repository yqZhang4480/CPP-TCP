#ifndef SPONGE_LIBSPONGE_TCP_SEGMENT_HH
#define SPONGE_LIBSPONGE_TCP_SEGMENT_HH

#include "buffer.hh"
#include "tcp_header.hh"

#include <cstdint>

//! \brief [TCP](\ref rfc::rfc793) segment
class TCPSegment {
  private:
    TCPHeader _header{};
    Buffer _payload{};

  public:
    //! \brief Parse the segment from a string
    ParseResult parse(const Buffer buffer, const uint32_t datagram_layer_checksum = 0);

    //! \brief Serialize the segment to a string
    BufferList serialize(const uint32_t datagram_layer_checksum = 0) const;

    //! \name Accessors
    //!@{
    const TCPHeader &header() const { return _header; }
    TCPHeader &header() { return _header; }

    const Buffer &payload() const { return _payload; }
    Buffer &payload() { return _payload; }
    //!@}

    //! \brief Segment's length in sequence space
    //! \note Equal to payload length plus one byte if SYN is set, plus one byte if FIN is set
    size_t length_in_sequence_space() const;

    void set_payload(const Buffer &payload) { _payload = payload; }
    void set_ack(const bool ack) { _header.ack = ack; }
    void set_syn(const bool syn) { _header.syn = syn; }
    void set_fin(const bool fin) { _header.fin = fin; }
    void set_rst(const bool rst) { _header.rst = rst; }
    void set_psh(const bool psh) { _header.psh = psh; }
    void set_urg(const bool urg) { _header.urg = urg; }
    void set_seqno(const WrappingInt32 seqno) { _header.seqno = seqno; }
    void set_ackno(const WrappingInt32 ackno) { _header.ackno = ackno; }
    void set_cksum(const uint16_t cksum) { _header.cksum = cksum; }
    void set_urgptr(const uint16_t urgptr) { _header.uptr = urgptr; }
    void set_window_size(const uint16_t window_size) { _header.win = window_size; }
};

#endif  // SPONGE_LIBSPONGE_TCP_SEGMENT_HH
