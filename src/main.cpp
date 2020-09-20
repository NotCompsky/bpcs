#include "bpcs.hpp"
#include "os.hpp"
#include "errors.hpp"
#include <compsky/macros/likely.hpp>
#define LIBCOMPSKY_NO_TESTS
#include <compsky/deasciify/a2n.hpp>
#ifdef _WIN32
# include <fcntl.h> // for O_BINARY
#endif


int main(const int argc, char* argv[]){
	static uchar io_buf[IO_BUF_SZ];
	
    int i = 0;
	if (unlikely(argc == 1))
		handler(WRONG_ARGUMENTS_TO_PROGRAM);
	
  #ifdef _WIN32
	setmode(fileno(stdout), O_BINARY);
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
    
	const unsigned min_complexity = a2n<unsigned>(argv[++i]);
    
    BPCSStreamBuf bpcs_stream(min_complexity, ++i, argc, argv
                              #ifdef EMBEDDOR
                              , embedding
                              , out_fmt
                              #endif
                              );
    bpcs_stream.load_next_img(); // Init
    
#ifdef EMBEDDOR
  if (!embedding){
#endif
	const size_t count = os::extract_to_stdout(bpcs_stream, io_buf);
#ifdef EMBEDDOR
  } else {
	os::embed_from_stdin(bpcs_stream, io_buf);
  }
#endif
#ifdef ONLY_COUNT
	printf("%lu\n", count);
#endif
	return 0;
}
