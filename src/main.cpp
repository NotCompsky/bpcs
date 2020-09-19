#include "bpcs.hpp"
#include "os.hpp"
#include "errors.hpp"
#include <compsky/macros/likely.hpp>
#include <unistd.h> // for STD(IN|OUT)_FILENO


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
    
	const uint8_t min_complexity = a2i_1or2digits(argv[++i]);
    
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
	const size_t count = os::extract_to_stdout(bpcs_stream);
#ifdef EMBEDDOR
  } else {
	os::embed_from_stdin(bpcs_stream);
  }
#endif
#ifdef ONLY_COUNT
	printf("%lu\n", count);
#endif
	return 0;
}
