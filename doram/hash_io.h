#pragma once
#include <emp-tool/emp-tool.h>

namespace emp {
// currently using has-a rather than is-a model to have easier access to underlying RepNetIO
template <typename IO> class HashSendIO
{
  public:
    IO *io;
    Hash hash;
    int num_blocks_hashed = 0;
    HashSendIO(IO *io_) : io(io_)
    {
        // hash = Hash();
    }
    void send_block(const block *data, size_t nblock)
    {
        hash.put_block(data, nblock);
        num_blocks_hashed += nblock;
    }
    void flush()
    {
        block digest_output[2];
        hash.digest(digest_output);
        // send only first block
        io->send_block(digest_output, 1);
    }
};

template <typename IO> class HashRecvIO
{
  public:
    IO *io;
    Hash hash;
    int num_blocks_hashed = 0;
    HashRecvIO(IO *io_) : io(io_)
    {
        // hash = Hash();
    }
    void recv_block(block *data, size_t nblock)
    {
        io->recv_block(data, nblock);
        hash.put_block(data, nblock);
        num_blocks_hashed += nblock;
    }
    block digest()
    {
        block digest_output[2];
        hash.digest(digest_output);
        return digest_output[0];
    }
};

}