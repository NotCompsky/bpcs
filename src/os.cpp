#include "os.hpp"
#ifndef _WIN32
# include <unistd.h>
#endif


bool write_to_stdout(const uchar io_buf[IO_BUF_SZ],  const size_t n_bytes){
  #ifdef _WIN32
	return (unlikely(fwrite(io_buf, n_bytes, 1, stdout) != 1));
  #else
	return (write(STDOUT_FILENO, io_buf, n_bytes) != n_bytes);
  #endif
}


bool read_from_stdin(uchar io_buf[IO_BUF_SZ]){
  #ifdef _WIN32
	return (unlikely(fread(io_buf, BYTES_PER_GRID, 1, stdin) != 1));
  #else
	return (read(STDIN_FILENO, io_buf, BYTES_PER_GRID) != BYTES_PER_GRID);
  #endif
}


namespace os {


size_t extract_to_stdout(BPCSStreamBuf& bpcs_stream,  uchar io_buf[IO_BUF_SZ]){
	uchar* io_buf_itr = io_buf;
	size_t count = 0;
	while(true){
#ifdef ONLY_COUNT
		if (unlikely(bpcs_stream.exhausted))
			break;
		count += BYTES_PER_GRID;
#endif
		bpcs_stream.get(io_buf_itr);
		io_buf_itr += BYTES_PER_GRID;
#ifndef ONLY_COUNT
		if (unlikely((io_buf_itr == io_buf + IO_BUF_SZ) or (bpcs_stream.exhausted))){
			const size_t n_bytes = (uintptr_t)io_buf_itr - (uintptr_t)io_buf;
			if (unlikely(write_to_stdout(io_buf, n_bytes)))
				handler(COULD_NOT_WRITE_ENOUGH_BYTES_TO_STDOUT);
			if (unlikely(bpcs_stream.exhausted))
				break;
			io_buf_itr = io_buf;
		}
#endif
	}
	return count;
}


#ifdef EMBEDDOR
void embed_from_stdin(BPCSStreamBuf& bpcs_stream,  uchar io_buf[IO_BUF_SZ]){
	uchar* io_buf_itr = io_buf;
	while (likely(not read_from_stdin(io_buf))){
		// TODO: Optimise
		bpcs_stream.put(io_buf);
	}
	bpcs_stream.put(io_buf);
    bpcs_stream.save_im();
}
#endif


} // namespace os
