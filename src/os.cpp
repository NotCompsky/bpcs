#include "os.hpp"
#ifndef _WIN32
# include <unistd.h>
#endif


#ifdef _WIN32
bool write_to_stdout(uchar io_buf[IO_BUF_SZ],  const size_t n_bytes){
	LPDWORD n_bytes_read_ptr;
	if (unlikely(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), io_buf, n_bytes, n_bytes_read_ptr, nullptr) == 0))
		handler(CANNOT_WRITE_TO_STDOUT);
	return (*n_bytes_read_ptr != n_bytes);
}
# define WRITE_STMT write_to_stdout(io_buf)


bool read_from_stdin(uchar io_buf[IO_BUF_SZ]){
	LPDWORD n_bytes_read_ptr;
	if (unlikely(ReadFile(GetStdHandle(STD_INPUT_HANDLE), io_buf, BYTES_PER_GRID, n_bytes_read_ptr, nullptr) == 0))
		handler(CANNOT_READ_FROM_STDIN);
	return (*n_bytes_read_ptr == BYTES_PER_GRID);
}
# define READ_STMT  read_from_stdin(io_buf)
#else
# include <unistd.h>
# define WRITE_STMT write(STDOUT_FILENO, io_buf, sizeof(io_buf)) != sizeof(io_buf)
# define READ_STMT  read (STDIN_FILENO, io_buf, BYTES_PER_GRID) == BYTES_PER_GRID
#endif


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
		if (unlikely((io_buf_itr == io_buf + sizeof(io_buf)) or (bpcs_stream.exhausted))){
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
	while (likely(READ_STMT)){
		// TODO: Optimise
		bpcs_stream.put(io_buf);
	}
	bpcs_stream.put(io_buf);
    bpcs_stream.save_im();
}
#endif


} // namespace os
