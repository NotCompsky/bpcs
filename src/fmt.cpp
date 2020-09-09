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
	COULD_NOT_STAT_FILE,
	TRYING_TO_ENCODE_MSG_OF_0_BYTES,
	CANNOT_OPEN_FILE,
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
	CANNOT_CREATE_FILE,
	N_ERRORS
};
#ifdef NO_EXCEPTIONS
# define handler(msg, ...) exit(msg)
#else
# include <stdexcept>
# include <cstdio>
constexpr static
const char* const handler_msgs[] = {
	"No error",
	"Misc error",
	"fp_str is empty",
	"Could not stat file",
	"Trying to encode a message of 0 bytes",
	"Cannot open file",
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
	"Improbably long file name",
	"Cannot create file"
};
# define LOG(...) fprintf(stderr, __VA_ARGS__); fflush(stderr);
void handler(const int msg){
	throw std::runtime_error(handler_msgs[msg]);
}
void log(const char* const str){
	LOG("log %s\n", str)
}
void log(const size_t n){
	LOG("log %lu\n", n)
}
template<typename... Args,  typename T>
void handler(const int msg,  const T arg,  Args... args){
	log(arg);
	handler(msg, args...);
}
#endif


void read_exact_number_of_bytes(const int fd,  char* const buf,  const size_t n){
	size_t offset = 0;
	do {
		offset += read(fd,  buf + offset,  n - offset);
	} while (offset != n);
}
void write_exact_number_of_bytes(const int fd,  char* const buf,  size_t n){
	do {
		n -= write(fd, buf, n);
	} while (n != 0);
}


char* get_parent_dir(char* path){
	do {
		--path;
	} while ((*path != '/'));
	// No need to check whether path has overflown the full file path beginning, because we can assume the root path begins with a slash - and we would not encounter root anyway.
	return path;
}


char* get_child_dir(char* path,  char* const end_of_full_file_path){
	do {
		++path;
	} while ((*path != '/') and (path != end_of_full_file_path));
	return (*path == '/') ? path : nullptr;
}


bool mkdir_path_between_pointers(char* const start,  char* const end){
	*end = 0;
	const int rc = mkdir(start,  S_IRUSR | S_IWUSR | S_IXUSR);
	if (rc == -1){
		if (unlikely(errno != ENOENT))
			handler(CANNOT_CREATE_FILE, start);
	}
	*end = '/';
	return (rc == 0); // i.e. return true on a success
}


int create_file_with_parent_dirs(char* const file_path,  const size_t file_path_len){
	int fd = open(file_path,  O_WRONLY | O_CREAT,  S_IRUSR | S_IWUSR | S_IXUSR);
	if (likely(fd != -1))
		// File successfully created
		return fd;
	if (unlikely(errno != ENOENT)){
		handler(CANNOT_CREATE_FILE, file_path);
	}
	
	// ENOENT: Either a directory component in pathname does not exist or is a dangling symbolic link
	
	// Traverse up the path until a directory is successfully created
	char* path = file_path + file_path_len;
	while(true){
		path = get_parent_dir(path);
		if (mkdir_path_between_pointers(file_path, path))
			break;
	}
	
	// Traverse back down the path
	while(true){
		path = get_child_dir(path,  file_path + file_path_len);
		if (path == nullptr)
			break;
		mkdir_path_between_pointers(file_path, path);
	}
	
	return open(file_path,  O_WRONLY | O_CREAT,  S_IRUSR | S_IWUSR | S_IXUSR);
}


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
            
			write_exact_number_of_bytes(STDOUT_FILENO, (char*)(&n_msg_bytes), 8);
			write_exact_number_of_bytes(STDOUT_FILENO, fp, n_msg_bytes);
			const auto rc3 = stat(fp, &stat_buf);
		  #ifdef TESTS
			if (unlikely(rc3 == -1)){
				handler(COULD_NOT_STAT_FILE, fp);
			}
		  #endif
            n_msg_bytes = stat_buf.st_size;
#ifdef TESTS
			if (unlikely(n_msg_bytes == 0)){
				handler(TRYING_TO_ENCODE_MSG_OF_0_BYTES);
			}
#endif
            
			write_exact_number_of_bytes(STDOUT_FILENO, (char*)(&n_msg_bytes), 8);
			const int msg_file = open(fp, O_RDONLY);
		  #ifdef TESTS
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
		const char zero[32] = {0};
		write_exact_number_of_bytes(STDOUT_FILENO, const_cast<char*>(zero), sizeof(zero));
        // Some encryption methods require blocks of length 16 or 32 bytes, so this ensures that there is at least 8 zero bytes even if a final half-block is cut off.
    } else {
    #endif
		char fp_str[1024];
		char fp_str__formatted[1024];
		int fout = STDOUT_FILENO;
        
        for (auto i = 0;  true;  ++i) {
			read_exact_number_of_bytes(STDIN_FILENO, (char*)(&n_msg_bytes), 8);
            
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
							handler(FP_STR_IS_EMPTY, fp_str__formatted);
						}
                    #endif
					fout = create_file_with_parent_dirs(fp_str__formatted, strlen(fp_str__formatted));
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
						handler(msg_id, fp_str__formatted);
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
				read_exact_number_of_bytes(STDIN_FILENO, fp_str, n_msg_bytes);
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
