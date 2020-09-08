#include <fcntl.h> // for open, O_WRONLY
#include <unistd.h> // for STD(IN|OUT)_FILENO
#include <inttypes.h>
#include "utils.hpp" // for format_out_fp
#include <sys/sendfile.h>
#include <compsky/macros/likely.hpp>


#ifdef EMBEDDOR
    #include <sys/stat.h> // for stat
#endif
#ifdef TESTS
    #include <assert.h>
#endif


enum {
	NO_ERROR,
	MISC_ERROR,
	FP_STR_IS_EMPTY,
	WRONG_NUMBER_OF_BYTES_READ,
	COULD_NOT_STAT_FILE,
	TRYING_TO_ENCODE_MSG_OF_0_BYTES,
	WROTE_WRONG_NUMBER_OF_BYTES_TO_STDOUT,
	CANNOT_OPEN_FILE,
	DID_NOT_WRITE_AS_MANY_BYTES_AS_READ,
	SENDFILE_ERROR,
	SENDFILE_ERROR__EAGAIN,
	SENDFILE_ERROR__EBADF,
	SENDFILE_ERROR__EFAULT,
	SENDFILE_ERROR__EINVAL,
	SENDFILE_ERROR__EIO,
	SENDFILE_ERROR__ENOMEM,
	SENDFILE_ERROR__EOVERFLOW,
	SENDFILE_ERROR__ESPIPE,
	SPLICE_ERROR__EBADF,
	SPLICE_ERROR__EINVAL,
	SPLICE_ERROR__ENOMEM,
	SPLICE_ERROR__ESPIPE,
	UNLIKELY_NUMBER_OF_MSG_BYTES,
	UNLIKELY_LONG_FILE_NAME,
	N_ERRORS
};
#ifdef NO_EXCEPTIONS
# define handler(msg) exit(msg)
# define handler2(msg, arg) handler(msg)
#else
# include <stdexcept>
# include <cstdio>
# define handler(msg) throw std::runtime_error(handler_msgs[msg])
# define handler2(msg, arg) fprintf(stderr, "Arg1: %s\n", arg); handler(msg)
constexpr static
const char* const handler_msgs[] = {
	"No error",
	"Misc error",
	"fp_str is empty",
	"Wrong number of bytes read",
	"Could not stat file",
	"Trying to encode a message of 0 bytes",
	"Wrote the wrong number of bytes to stdout",
	"Cannot open file",
	"Did not write as many bytes as read",
	"Sendfile error",
	"Sendfile error: Nonblocking IO has been selected using O_NONBLOCK and the write would block",
	"Sendfile error: The input file was not opened for reading, or the output file was not opened for writing",
	"Sendfile error: Bad address",
	"Sendfile error: Descriptor is not valid or locked, or an mmap-like operation is not available for in_fd, or count is negative. OR (???) out_fd has the O_APPEND flag set, not currently supported by sendfile.",
	"Sendfile error: Unspecified error while reading from in_fd",
	"Sendfile error: Insufficient memory to read from in_fd",
	"Sendfile error: Count is too large",
	"Sendfile error: Offset is not NULL but the input file is not seek-able",
	"Splice error: Bad file descriptor",
	"Splice error: Invalid",
	"Splice error: Out of memory",
	"Splice error: Either off_in or off_out was not NULL, but the corresponding file descriptor refers to a pipe",
	"Improbably large file",
	"Improbably long file name"
};
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
    struct stat stat_buf;
    
    if (embedding){
		for (;  *msg_fps != nullptr;  ++msg_fps){
            // At start, and between embedded messages, is a 64-bit integer telling us the size of the next embedded message
            // The first 32+ bits will almost certainly be 0 - but this will not aid decryption of the rest of the contents (assuming we are using an encryption method that is resistant to known-plaintext attack)
			char* const fp = *msg_fps;
            
            n_msg_bytes = get_charp_len(fp);
            
			const size_t rc1 = write(STDOUT_FILENO, (char*)(&n_msg_bytes), 8);
			const size_t rc2 = write(STDOUT_FILENO, fp, n_msg_bytes);
			const auto rc3 = stat(fp, &stat_buf);
		  #ifdef TESTS
			if (unlikely((rc1 != 8) or (rc2 != n_msg_bytes)))
				handler(WRONG_NUMBER_OF_BYTES_READ);
			if (unlikely(rc3 == -1)){
				handler2(COULD_NOT_STAT_FILE, fp);
			}
		  #endif
            n_msg_bytes = stat_buf.st_size;
#ifdef TESTS
			if (unlikely(n_msg_bytes == 0)){
				handler(TRYING_TO_ENCODE_MSG_OF_0_BYTES);
			}
#endif
            
			const size_t rc4 = write(STDOUT_FILENO, (char*)(&n_msg_bytes), 8);
			const int msg_file = open(fp, O_RDONLY);
		  #ifdef TESTS
			if (unlikely(rc4 != 8)){
				handler(WROTE_WRONG_NUMBER_OF_BYTES_TO_STDOUT);
			}
			if (unlikely(msg_file == 0)){
				handler(CANNOT_OPEN_FILE);
			}
		  #endif
			const auto rc5 = sendfile(STDOUT_FILENO, msg_file, nullptr, n_msg_bytes);
		  #ifdef TESTS
			if (unlikely(rc5 == -1)){
				handler(SENDFILE_ERROR);
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
			if (unlikely(eight_bytes_read != 8)){
				handler(WRONG_NUMBER_OF_BYTES_READ);
			}
		  #endif
            
            if (n_msg_bytes == 0){
                // Reached end of embedded datas
                return 0;
            }
            
            if (i & 1){
                // First block of data is original file path, second is file contents
              #ifdef TESTS
				if (unlikely(n_msg_bytes > 1099511627780)){
                    // ~2**40
					handler(UNLIKELY_NUMBER_OF_MSG_BYTES);
                }
              #endif
                if (out_fmt != NULL){
                    #ifdef TESTS
						if (unlikely(fp_str[0] == 0)){
							handler2(FP_STR_IS_EMPTY, fp_str__formatted);
						}
                    #endif
                    fout = open(fp_str__formatted,  O_WRONLY | O_CREAT,  S_IRUSR | S_IWUSR | S_IXUSR);
                }
				loff_t n_bytes_written = 0;
				size_t n_bytes_yet_to_write = n_msg_bytes;
				do {
					auto n_writ = splice(STDIN_FILENO, NULL, fout, &n_bytes_written, n_bytes_yet_to_write, SPLICE_F_MOVE);
				  #ifdef TESTS
					if (unlikely(n_writ == -1)){
						auto msg_id = MISC_ERROR;
						switch(errno){
							case EBADF:
								msg_id = SPLICE_ERROR__EBADF;
								break;
							case EINVAL:
								msg_id = SPLICE_ERROR__EINVAL;
								break;
							case ENOMEM:
								msg_id = SPLICE_ERROR__ENOMEM;
								break;
							case ESPIPE:
								msg_id = SPLICE_ERROR__ESPIPE;
								break;
						}
						handler2(msg_id, fp_str__formatted);
					}
				  #endif
					n_bytes_yet_to_write -= n_writ;
				} while(n_bytes_yet_to_write != 0);
            } else {
              #ifdef TESTS
                if (unlikely(n_msg_bytes > sizeof(fp_str))){
					// An improbably long file name - indicating that the n_msg_bytes was most likely garbage, in turn indicating that the last message was truncated.
					handler(UNLIKELY_LONG_FILE_NAME);
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
