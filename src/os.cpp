#include "os.hpp"
#ifndef _WIN32
# include <unistd.h>
#endif


bool write_to_stdout(uchar io_buf[IO_BUF_SZ],  const size_t n_bytes){
  #ifdef _WIN32
	LPDWORD n_bytes_read_ptr;
	if (unlikely(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), io_buf, n_bytes, n_bytes_read_ptr, nullptr) == 0))
		handler(CANNOT_WRITE_TO_STDOUT);
	return (*n_bytes_read_ptr != n_bytes);
  #else
	return (write(STDOUT_FILENO, io_buf, n_bytes) != n_bytes);
  #endif
}


bool read_from_stdin(uchar io_buf[IO_BUF_SZ]){
  #ifdef _WIN32
	LPDWORD n_bytes_read_ptr;
	if (unlikely(ReadFile(GetStdHandle(STD_INPUT_HANDLE), io_buf, BYTES_PER_GRID, n_bytes_read_ptr, nullptr) == 0))
		handler(CANNOT_READ_FROM_STDIN);
	return (*n_bytes_read_ptr != BYTES_PER_GRID);
  #else
	return (read(STDIN_FILENO, io_buf, BYTES_PER_GRID) != BYTES_PER_GRID);
  #endif
}


namespace os {


size_t extract_to_stdout(BPCSStreamBuf& bpcs_stream,  uchar io_buf[IO_BUF_SZ]){
	uchar* io_buf_itr = io_buf;
	size_t count = 0;
	while(true){
		bpcs_stream.get(io_buf_itr);
		io_buf_itr += BYTES_PER_GRID;
#ifdef ONLY_COUNT
		count += BYTES_PER_GRID;
#endif
		if (unlikely((io_buf_itr == io_buf + IO_BUF_SZ) or (bpcs_stream.exhausted))){
			const size_t n_bytes = (uintptr_t)io_buf_itr - (uintptr_t)io_buf;
#ifndef ONLY_COUNT
			if (unlikely(write_to_stdout(io_buf, n_bytes)))
				handler(COULD_NOT_WRITE_ENOUGH_BYTES_TO_STDOUT);
#endif
			if (unlikely(bpcs_stream.exhausted))
				break;
			io_buf_itr = io_buf;
		}
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
