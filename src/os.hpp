#pragma once

#include <unistd.h>


#define WHILE_CONDITION while(not bpcs_stream.exhausted)
#ifdef ONLY_COUNT
# define DO_OR_WHILE WHILE_CONDITION
# define WHILE_OR_DO
#else
# define DO_OR_WHILE do
# define WHILE_OR_DO WHILE_CONDITION;
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
			if (unlikely(write(STDOUT_FILENO, io_buf, sizeof(io_buf)) != sizeof(io_buf)))
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
	while (likely(read(STDIN_FILENO, io_buf, BYTES_PER_GRID) == BYTES_PER_GRID)){
		// TODO: Optimise
		bpcs_stream.put(io_buf);
	}
	bpcs_stream.put(io_buf);
    bpcs_stream.save_im();
}
#endif


} // namespace os
