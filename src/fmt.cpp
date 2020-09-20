#include "utils.hpp" // for format_out_fp
#include "fmt_os.hpp"
#include "errors.hpp"

#include <inttypes.h>
#include <cerrno>
#include <compsky/macros/likely.hpp>

#ifdef _WIN32
# include <fcntl.h> // for O_BINARY
#endif
#ifdef CHITTY_CHATTY
# include <cstdio>
#endif


typedef unsigned char uchar;


inline uint64_t get_charp_len(char* chrp){
    uint64_t i = 0;
    while (*(chrp++) != 0)
        ++i;
    return i;
}


int main(const int argc,  char** argv){
    #ifdef EMBEDDOR
    bool embedding = false;
    #endif
    
    char* out_fmt = NULL;
	
  #ifdef _WIN32
	setmode(fileno(stdout), O_BINARY);
  #endif
    
	++argv;
    if (argc != 1){
		char* const arg = *argv;
		if (arg[2] == 0  &&  arg[0] == '-'){
			switch(arg[1]){
				case 'o': out_fmt=*(++argv); break;
				#ifdef EMBEDDOR
				case 'm': embedding=true; break;
				#endif
			}
		}
    }
	
#ifdef EMBEDDOR
	char** msg_fps = argv + 1;
#endif
    
    uint64_t n_msg_bytes;
    
    #ifdef EMBEDDOR
    
    if (embedding){
		for (;  *msg_fps != nullptr;  ++msg_fps){
            // At start, and between embedded messages, is a 64-bit integer telling us the size of the next embedded message
            // The first 32+ bits will almost certainly be 0 - but this will not aid decryption of the rest of the contents (assuming we are using an encryption method that is resistant to known-plaintext attack)
			char* const fp = *msg_fps;
            
            n_msg_bytes = get_charp_len(fp);
            
			os::write_exact_number_of_bytes_to_stdout((char*)(&n_msg_bytes), 8);
			os::write_exact_number_of_bytes_to_stdout(fp, n_msg_bytes);
            n_msg_bytes = os::get_file_sz(fp);
			if (unlikely(n_msg_bytes == 0))
				handler(TRYING_TO_ENCODE_MSG_OF_0_BYTES);
			os::write_exact_number_of_bytes_to_stdout((char*)(&n_msg_bytes), 8);
			os::sendfile_from_file_to_stdout(fp, n_msg_bytes);
        }
        // After all messages, signal end with signalled size of 0
		constexpr char zero[32] = {0};
		os::write_exact_number_of_bytes_to_stdout(const_cast<char*>(zero), sizeof(zero));
        // Some encryption methods require blocks of length 16 or 32 bytes, so this ensures that there is at least 8 zero bytes even if a final half-block is cut off.
    } else {
    #endif
		char fp_str[1024];
		char fp_str__formatted[1024];
		fout_typ fout = STDOUT_DESCR;
        
        for (auto i = 0;  true;  ++i) {
			os::read_exact_number_of_bytes_from_stdin((char*)(&n_msg_bytes), 8);
            
            if (n_msg_bytes == 0){
                // Reached end of embedded datas
                return 0;
            }
            
            if (i & 1){
                // First block of data is original file path, second is file contents
				if (unlikely(n_msg_bytes > 1099511627780))
                    // ~2**40
					handler(UNLIKELY_NUMBER_OF_MSG_BYTES);
                if (out_fmt != NULL){
						if (unlikely(fp_str[0] == 0))
							handler(FP_STR_IS_EMPTY, fp_str__formatted);
					fout = os::create_file_with_parent_dirs(fp_str__formatted, strlen(fp_str__formatted));
                }
				os::splice_from_stdin_to_fd(fout, n_msg_bytes);
            } else {
                if (unlikely(n_msg_bytes > sizeof(fp_str)))
					// An improbably long file name - indicating that the n_msg_bytes was most likely garbage, in turn indicating that the last message was truncated.
					handler(UNLIKELY_LONG_FILE_NAME);
				os::read_exact_number_of_bytes_from_stdin(fp_str, n_msg_bytes);
				fp_str[n_msg_bytes] = 0; // Terminating null byte
                if (out_fmt != NULL){
					format_out_fp(out_fmt, fp_str, fp_str__formatted);
                }
            }
        }
    #ifdef EMBEDDOR
    }
    #endif
}
