#include "os.hpp"


#define WHILE_CONDITION while(not bpcs_stream.exhausted)
#ifdef ONLY_COUNT
# define DO_OR_WHILE WHILE_CONDITION
# define WHILE_OR_DO
#else
# define DO_OR_WHILE do
# define WHILE_OR_DO WHILE_CONDITION;
#endif


#ifdef _WIN32
template<size_t sz>
bool write_to_stdout(uchar(&io_buf)[sz]){
	LPDWORD n_bytes_read_ptr;
	if (unlikely(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), io_buf, sz, n_bytes_read_ptr, nullptr) != 0))
		return true;
	return (*n_bytes_read_ptr == sz);
}
# define WRITE_STMT write_to_stdout(io_buf)
template<size_t sz>
bool read_from_stdin(uchar(&io_buf)[sz]){
	LPDWORD n_bytes_read_ptr;
	if (unlikely(ReadFile(GetStdHandle(STD_INPUT_HANDLE), io_buf, BYTES_PER_GRID, n_bytes_read_ptr, nullptr) != 0))
		return true;
	return (*n_bytes_read_ptr == BYTES_PER_GRID);
}
# define READ_STMT  read_from_stdin(io_buf)
#else
# include <unistd.h>
# define WRITE_STMT write(STDOUT_FILENO, io_buf, sizeof(io_buf)) != sizeof(io_buf)
# define READ_STMT  read (STDIN_FILENO, io_buf, BYTES_PER_GRID) == BYTES_PER_GRID
#endif


namespace os {


size_t extract_to_stdout(BPCSStreamBuf& bpcs_stream){
	static uchar io_buf[((1024 * 64) / BYTES_PER_GRID) * BYTES_PER_GRID]; // Ensure it is divisible by BYTES_PER_GRID
	uchar* io_buf_itr = io_buf;
	size_t count = 0;
	DO_OR_WHILE {
		bpcs_stream.get(io_buf_itr);
		io_buf_itr += BYTES_PER_GRID;
#ifdef ONLY_COUNT
		count += BYTES_PER_GRID;
#endif
		if (unlikely((io_buf_itr == io_buf + sizeof(io_buf)) or (bpcs_stream.exhausted))){
#ifndef ONLY_COUNT
			if (unlikely(WRITE_STMT))
				break;
#endif
			io_buf_itr = io_buf;
		}
	} WHILE_OR_DO
	return count;
}


#ifdef EMBEDDOR
void embed_from_stdin(BPCSStreamBuf& bpcs_stream){
	static uchar io_buf[((1024 * 64) / BYTES_PER_GRID) * BYTES_PER_GRID]; // Ensure it is divisible by BYTES_PER_GRID
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
