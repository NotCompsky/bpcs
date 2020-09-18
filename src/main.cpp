#include "bpcs.hpp"
#include "errors.hpp"
#include <compsky/macros/likely.hpp>
#include <unistd.h> // for STD(IN|OUT)_FILENO


#define WHILE_CONDITION while(not bpcs_stream.exhausted)
#ifdef ONLY_COUNT
# define DO_OR_WHILE WHILE_CONDITION
# define WHILE_OR_DO
#else
# define DO_OR_WHILE do
# define WHILE_OR_DO WHILE_CONDITION;
#endif


int main(const int argc, char* argv[]){
    int i = 0;
  #ifdef TESTS
	if (unlikely(argc == 1))
		handler(WRONG_ARGUMENTS_TO_PROGRAM);
  #endif
	
#ifdef EMBEDDOR
    const bool embedding = (argv[1][0] == '-' && argv[1][1] == 'o' && argv[1][2] == 0);
    
    char* out_fmt;
    
    if (embedding){
        ++i;
        out_fmt = argv[++i];
    } else
        out_fmt = NULL;
#endif
	
#ifdef ONLY_COUNT
	uint64_t count = 0;
#endif
    
    
	const uint8_t min_complexity = a2i_1or2digits(argv[++i]);
    
    BPCSStreamBuf bpcs_stream(min_complexity, ++i, argc, argv
                              #ifdef EMBEDDOR
                              , embedding
                              , out_fmt
                              #endif
                              );
    bpcs_stream.load_next_img(); // Init
	
	uchar io_buf[((1024 * 64) / BYTES_PER_GRID) * BYTES_PER_GRID]; // Ensure it is divisible by BYTES_PER_GRID
	uchar* io_buf_itr = io_buf;
    
#ifdef EMBEDDOR
  if (!embedding){
#endif
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
#ifdef EMBEDDOR
  } else {
	while (likely(read(STDIN_FILENO, io_buf, BYTES_PER_GRID) == BYTES_PER_GRID)){
		// TODO: Optimise
        // read() returns the number of bytes written
		bpcs_stream.put(io_buf);
	}
	bpcs_stream.put(io_buf);
    bpcs_stream.save_im();
  }
#endif
#ifdef ONLY_COUNT
	printf("%lu\n", count);
#endif
	return 0;
}
