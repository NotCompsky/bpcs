#pragma once

#include "bpcs.hpp"


#define WHILE_CONDITION while(not bpcs_stream.exhausted)
#ifdef ONLY_COUNT
# define DO_OR_WHILE WHILE_CONDITION
# define WHILE_OR_DO
#else
# define DO_OR_WHILE do
# define WHILE_OR_DO WHILE_CONDITION;
#endif

#ifdef _WIN32
inline
bool write_to_stdout(){
	long n_bytes_read;
	if (unlikely(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), io_buf, sizeof(io_buf), &n_bytes_read, nullptr) != 0))
		return true;
	return (n_bytes_read == sizeof(io_buf));
}
# define WRITE_STMT write_to_stdout()
inline
bool read_from_stdin(){
	long n_bytes_read;
	if (unlikely(ReadFile(GetStdHandle(STD_INPUT_HANDLE), io_buf, BYTES_PER_GRID, &n_bytes_read, nullptr) != 0))
		return true;
	return (n_bytes_read == BYTES_PER_GRID);
}
# define READ_STMT  read_from_stdin()
#else
# include <unistd.h>
# define WRITE_STMT write(STDOUT_FILENO, io_buf, sizeof(io_buf)) != sizeof(io_buf)
# define READ_STMT  read (STDIN_FILENO, io_buf, BYTES_PER_GRID) == BYTES_PER_GRID
#endif


namespace os {


size_t extract_to_stdout(BPCSStreamBuf& bpcs_stream);

#ifdef EMBEDDOR
void embed_from_stdin(BPCSStreamBuf& bpcs_stream);
#endif


} // namespace os
