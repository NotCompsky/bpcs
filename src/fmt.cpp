#include <fcntl.h> // for open, O_WRONLY
#include <unistd.h> // for STD(IN|OUT)_FILENO
#include <inttypes.h>
#include "utils.hpp" // for format_out_fp
#include <sys/sendfile.h>
#include <cstdio> // for fprintf etc // NOTE: tmp
#include <compsky/macros/likely.hpp>


#ifdef EMBEDDOR
    #include <sys/stat.h> // for stat
#endif
#ifdef TESTS
    #include <assert.h>
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
    
	++argv;
    if (argc != 1){
		char* const arg = *argv;
		++argv;
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
	char** msg_fps = argv;
#endif
    
    uint64_t n_msg_bytes;
    
    #ifdef EMBEDDOR
    struct stat stat_buf;
    
    if (embedding){
		for (;  *msg_fps != nullptr;  ++msg_fps){
            // At start, and between embedded messages, is a 64-bit integer telling us the size of the next embedded message
            // The first 32+ bits will almost certainly be 0 - but this will not aid decryption of the rest of the contents (assuming we are using an encryption method that is resistant to known-plaintext attack)
			char* const fp = *msg_fps;
            
            n_msg_bytes = get_charp_len(fp);
			fprintf(stderr, "fp_file_len %s\n", (char*)(&n_msg_bytes));
			fprintf(stderr, "fp_file_len %lu\n", n_msg_bytes);
			fflush(stderr);
            
			const size_t rc1 = write(STDOUT_FILENO, (char*)(&n_msg_bytes), 8);
			const size_t rc2 = write(STDOUT_FILENO, fp, n_msg_bytes);
			const auto rc3 = stat(fp, &stat_buf);
		  #ifdef TESTS
            if ((rc1 != 8) or (rc2 != n_msg_bytes))
				handler(WRONG_NUMBER_OF_BYTES_READ);
			if (rc3 == -1){
				fprintf(stderr,  "While encoding file: %s\n",  fp);
				fflush(stderr);
				handler(COULD_NOT_STAT_FILE);
			}
		  #endif
            n_msg_bytes = stat_buf.st_size;
#ifdef TESTS
			if (unlikely(n_msg_bytes == 0)){
				fprintf(stderr,  "When encoding file: %s\n",  fp);
				fflush(stderr);
				handler(TRYING_TO_ENCODE_MSG_OF_0_BYTES);
			}
#endif
			fprintf(stderr, "n_msg_bytes %s\n", (char*)(&n_msg_bytes));
			fprintf(stderr, "n_msg_bytes %lu\n", n_msg_bytes);
			fflush(stderr);
            
			const size_t rc4 = write(STDOUT_FILENO, (char*)(&n_msg_bytes), 8);
			const int msg_file = open(fp, O_RDONLY);
		  #ifdef TESTS
			if ((rc4 != 8) or (msg_file == 0)){
				return 100;
			}
		  #endif
			const auto rc5 = sendfile(STDOUT_FILENO, msg_file, nullptr, n_msg_bytes);
		  #ifdef TESTS
			if (rc5 == -1){
				return 20;
			}
		  #endif
			close(msg_file);
        }
        // After all messages, signal end with signalled size of 0
		const uchar zero[32] = {0};
		const size_t rc1 = write(STDOUT_FILENO, zero, sizeof(zero));
	  #ifdef TESTS
		assert(rc1 == sizeof(zero));
	  #endif
        // Some encryption methods require blocks of length 16 or 32 bytes, so this ensures that there is at least 8 zero bytes even if a final half-block is cut off.
    } else {
    #endif
		char fp_str[1024];
		char fp_str__formatted[1024];
		int fout = STDOUT_FILENO;
        
        for (auto i = 0;  true;  ++i) {
			const size_t eight_bytes_read = read(STDIN_FILENO, (char*)(&n_msg_bytes), 8);
		  #ifdef TESTS
			if (eight_bytes_read != 8){
				return 33;
			}
		  #endif
			fprintf(stderr, "n_msg_bytes %s\n", (char*)(&n_msg_bytes));
			fprintf(stderr, "n_msg_bytes %lu\n", n_msg_bytes);
			fflush(stderr);
            
            if (n_msg_bytes == 0){
                // Reached end of embedded datas
                return 0;
            }
            
            if (i & 1){
                // First block of data is original file path, second is file contents
              #ifdef TESTS
                if (n_msg_bytes > 1099511627780){
                    // ~2**40
                    return 1;
                }
              #endif
                if (out_fmt != NULL){
                    #ifdef TESTS
                        assert(fp_str[0] != 0);
                    #endif
                    fout = open(fp_str__formatted,  O_WRONLY | O_CREAT,  S_IRUSR | S_IWUSR | S_IXUSR);
					fprintf(stderr,  "Created file: %s\n",  fp_str__formatted);
                }
				sendfile(fout, STDIN_FILENO, nullptr, n_msg_bytes);
            } else {
              #ifdef TESTS
                if (n_msg_bytes > sizeof(fp_str)){
					// An improbably long file name - indicating that the n_msg_bytes was most likely garbage, in turn indicating that the last message was truncated.
					return 2;
                }
              #endif
				const size_t rc1 = read(STDIN_FILENO, fp_str, n_msg_bytes);
			  #ifdef TESTS
				assert(rc1 == n_msg_bytes);
			  #endif
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
