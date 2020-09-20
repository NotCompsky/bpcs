#pragma once

#include "bpcs.hpp"
#ifdef _WIN32
# include "windows.h"
#endif


#define DESIRED_IO_BUF_SZ (1024 * 64)
#define IO_BUF_SZ ((DESIRED_IO_BUF_SZ / BYTES_PER_GRID) * BYTES_PER_GRID) // Ensure it is divisible by BYTES_PER_GRID


namespace os {


size_t extract_to_stdout(BPCSStreamBuf& bpcs_stream,  uchar io_buf[IO_BUF_SZ]);

#ifdef EMBEDDOR
void embed_from_stdin(BPCSStreamBuf& bpcs_stream,  uchar io_buf[IO_BUF_SZ]);
#endif


} // namespace os
