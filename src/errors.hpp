#pragma once


#include <cstdlib> // for exit


enum {
	NO_ERROR,
	MISC_ERROR,
	
	TOO_MUCH_DATA_TO_ENCODE,
	INVALID_PNG_MAGIC_NUMBER,
	OOM,
	CANNOT_CREATE_PNG_READ_STRUCT,
	EMPTY_BITPLANE,
	COULD_NOT_OPEN_PNG_FILE,
	CANNOT_CREATE_PNG_INFO_STRUCT,
	PNG_ERROR_1,
	PNG_ERROR_2,
	PNG_ERROR_3,
	PNG_ERROR_4,
	TOO_MANY_BITPLANES,
	IMAGE_IS_NOT_RGB,
	WRONG_NUMBER_OF_CHANNELS,
	WRONG_ARGUMENTS_TO_PROGRAM,
	
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
# define LOG(...)
#else
# include <cstdio>
# define LOG(...) fprintf(stderr, __VA_ARGS__); fflush(stderr);
constexpr static
const char* const handler_msgs[] = {
	"No error",
	"Misc error",
	
	"Reached last image (too much data to encode?)",
	"Invalid PNG magic number",
	"Out of memory",
	"Cannot create PNG read struct",
	"Empty bitplane",
	"Could not open PNG file",
	"Could not create PNG info struct",
	"PNG error 1",
	"PNG error 2",
	"PNG error 3",
	"PNG error 4",
	"Bitdepth is too high",
	"Image is not RGB",
	"Wrong number of colour channels in image",
	"Wrong arguments given to this program - reference the manual",
	
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
	"Cannot create file",
	
	""
};
#endif

inline
void handler(const int rc){
	// Do nothing on a bad test result in order for the test itself to be optimised out
  #ifdef TESTS
   #ifndef NO_EXCEPTIONS
	fprintf(stderr, "%s\n", handler_msgs[rc]);
	fflush(stderr);
   #endif
	exit(rc);
  #endif
}

inline
void log(const char* const str){
	LOG("log %s\n", str)
}
inline
void log(const size_t n){
	LOG("log %lu\n", n)
}
template<typename... Args,  typename T>
void handler(const int msg,  const T arg,  Args... args){
	log(arg);
	handler(msg, args...);
}
