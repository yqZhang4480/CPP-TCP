#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity):channel(capacity+1),cap(capacity)
                        ,r_index(0),w_index(0),read_bytes(0),written_bytes(0),end_flag(false){ 
}

size_t ByteStream::write(const string &data) {
    uint32_t i = 0;
    if(end_flag == true){
        return 0;
    }
    for(i = 0; (w_index+1)%(cap+1)!= r_index&&i < data.size(); i++){
        channel[w_index] = data[i];
        w_index = (w_index + 1)%(cap+1);
        written_bytes++;
    }
    return i;//probably error
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
   std::vector<char> s(len);
   size_t i = 0;
   size_t r_i = r_index;
   while(r_i!=w_index&&i < len){
       s[i] = channel[r_i];
       r_i = (r_i + 1)%(cap+1);
       i++;
   }
   std::string str(s.begin(),s.begin() + i );
   return str;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t i = 0;
    while(r_index!=w_index&& i < len){
        r_index = (r_index + 1)%(cap + 1);
        read_bytes++;
        i++;
    }
    
 }

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::vector<char> s(len);
    size_t i = 0;
    while(r_index!=w_index&&i < len){
        s[i] = channel[r_index];
        r_index = (r_index + 1)%(cap + 1);
        read_bytes++;
        i++;
    }

    std::string str(s.begin(),s.begin() + i);
    return str;
}

void ByteStream::end_input() {
    end_flag = true;
}

bool ByteStream::input_ended() const { 
    return end_flag;
}

size_t ByteStream::buffer_size() const { 
    return (w_index - r_index + cap + 1)%(cap + 1);
}

bool ByteStream::buffer_empty() const {
    return w_index == r_index;
}

bool ByteStream::eof() const { return end_flag&&w_index == r_index; }

size_t ByteStream::bytes_written() const { return written_bytes; }

size_t ByteStream::bytes_read() const {return read_bytes;} 

size_t ByteStream::remaining_capacity() const { 
    return (r_index - w_index + cap)%(cap + 1);
 }
