#include "fmt_os.hpp"
#include "errors.hpp"
#include "std_file_handles.hpp"
#include <cerrno>

#ifdef _WIN32
#else
# include <unistd.h>
# include <fcntl.h> // for open, O_WRONLY
# include <sys/stat.h>
# include <sys/sendfile.h>
#endif


#ifdef _WIN32
static const char path_sep = '\\';
constexpr HANDLE INVALID_HANDLE_VALUE2 = INVALID_HANDLE_VALUE;
#else
static const char path_sep = '/';
constexpr int INVALID_HANDLE_VALUE  = -1;
constexpr int INVALID_HANDLE_VALUE2 = 0;
#endif


char* get_parent_dir(char* path){
	do {
		--path;
	} while ((*path != path_sep));
	// No need to check whether path has overflown the full file path beginning, because we can assume the root path begins with a slash - and we would not encounter root anyway.
	return path;
}


char* get_child_dir(char* path,  char* const end_of_full_file_path){
	do {
		++path;
	} while ((*path != path_sep) and (path != end_of_full_file_path));
	return (*path == path_sep) ? path : nullptr;
}


bool mkdir_path_between_pointers(char* const start,  char* const end){
	*end = 0;
  #ifdef _WIN32
	const bool rc = CreateDirectoryA(start, nullptr); // TODO: Set security attributes
	// Return code doesn't distinguish between ERROR_ALREADY_EXISTS and ERROR_PATH_NOT_FOUND
  #else
	const int rc = mkdir(start,  S_IRUSR | S_IWUSR | S_IXUSR);
	if (rc == -1){
		if (unlikely(errno != ENOENT))
			handler(CANNOT_CREATE_FILE, start);
	}
  #endif
	*end = path_sep;
	return (rc == 0); // i.e. return true on a success
}


fout_typ create_file(const char* const file_path){
  #ifdef _WIN32
	return CreateFileA(file_path,  GENERIC_WRITE,  0,  nullptr,  CREATE_ALWAYS,  FILE_ATTRIBUTE_NORMAL,  nullptr);
  #else
	return open(file_path,  O_WRONLY | O_CREAT,  S_IRUSR | S_IWUSR | S_IXUSR);
  #endif
}


fout_typ open_file_for_reading(const char* const file_path){
  #ifdef _WIN32
	return CreateFileA(file_path,  GENERIC_READ,  0,  nullptr,  OPEN_EXISTING,  FILE_ATTRIBUTE_NORMAL,  nullptr);
  #else
	return open(file_path, O_RDONLY);
  #endif
}


void close_file_handle(const fout_typ fd){
  #ifdef _WIN32
	CloseHandle(fd);
  #else
	close(fd);
  #endif
}


namespace os {


fout_typ create_file_with_parent_dirs(char* const file_path,  const size_t file_path_len){
#ifdef CHITTY_CHATTY
	fprintf(stderr,  "Creating file: %s\n",  file_path);
#endif
	
	fout_typ fd = create_file(file_path);
	
	if (likely(fd != INVALID_HANDLE_VALUE))
		// File successfully created
		return fd;
  #ifdef _WIN32
	// No information in documentation about possible error codes of GetLastError(), so we'll just assume that the error was due to a parent directory not existing
  #else
	if (unlikely(errno != ENOENT)){
		handler(CANNOT_CREATE_FILE, file_path);
	}
  #endif
	
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
	
	return create_file(path);
}


void read_exact_number_of_bytes_from_stdin(char* const buf,  const size_t n){
	size_t offset = 0;
	do {
	  #ifdef _WIN32
		DWORD n_bytes_read;
		if (unlikely(ReadFile(stdin_handle,  buf + offset,  n - offset,  &n_bytes_read,  nullptr) == 0))
			handler(CANNOT_READ_FROM_STDIN);
		offset += n_bytes_read;
	  #else
		offset += read(STDIN_FILENO,  buf + offset,  n - offset);
	  #endif
	} while (offset != n);
}


void write_exact_number_of_bytes_to_stdout(char* const buf,  size_t n){
	do {
	  #ifdef _WIN32
		DWORD n_bytes_read;
		if (unlikely(WriteFile(stdout_handle,  buf,  n,  &n_bytes_read,  nullptr) == 0))
			handler(CANNOT_WRITE_TO_STDOUT);
		n -= n_bytes_read;
	  #else
		n -= write(STDOUT_FILENO, buf, n);
	  #endif
	} while (n != 0);
}


#ifdef _WIN32
void win__transfer_data_between_files(HANDLE in,  HANDLE out,  size_t n_bytes){
	do {
		static char buf[1024 * 64];
		
		size_t n_bytes_to_transfer = sizeof(buf);
		if (n_bytes_to_transfer > n_bytes)
			n_bytes_to_transfer = n_bytes;
		
		DWORD n_bytes_read;
		if (unlikely(ReadFile(in,  buf,  n_bytes_to_transfer,  &n_bytes_read,  nullptr) == 0))
			handler(CANNOT_READ_FROM_STDIN);
		DWORD n_bytes_written;
		if (unlikely(WriteFile(out,  buf,  n_bytes_to_transfer,  &n_bytes_written,  nullptr) == 0))
			handler(CANNOT_WRITE_TO_STDOUT);
		
		if (unlikely((n_bytes_read != n_bytes_to_transfer) or (n_bytes_written != n_bytes_to_transfer)))
			handler(MISMATCH_BETWEEN_BYTES_READ_AND_WRITTEN);
		
		n_bytes -= n_bytes_to_transfer;
	} while (n_bytes != 0);
}
#endif


void sendfile_from_file_to_stdout(const char* const fp,  const size_t n_bytes){
	const fout_typ msg_file = open_file_for_reading(fp);
	if (unlikely(msg_file == INVALID_HANDLE_VALUE2))
		handler(CANNOT_OPEN_FILE);
  #ifdef _WIN32
	win__transfer_data_between_files(msg_file, stdout_handle, n_bytes);
  #else
	const auto rc5 = sendfile(STDOUT_FILENO, msg_file, nullptr, n_bytes);
	if (unlikely(rc5 == -1))
		handler(SENDFILE_ERROR);
  #endif
	close_file_handle(msg_file);
}


void splice_from_stdin_to_fd(const fout_typ fout,  const size_t n_bytes){
  #ifdef _WIN32
	win__transfer_data_between_files(stdin_handle, fout, n_bytes);
  #else
	loff_t n_bytes_written = 0;
	size_t n_bytes_yet_to_write = n_bytes;
	do {
		auto n_writ = splice(STDIN_FILENO, NULL, fout, &n_bytes_written, n_bytes_yet_to_write, SPLICE_F_MOVE);
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
			handler(msg_id);
		}
		n_bytes_yet_to_write -= n_writ;
	} while(n_bytes_yet_to_write != 0);
  #endif
}


#ifdef EMBEDDOR
size_t get_file_sz(const char* const fp){
  #ifdef _WIN32
	HANDLE const f = open_file_for_reading(fp);
	_LARGE_INTEGER f_sz; // For x86_32 compatibility
	if (unlikely(GetFileSizeEx(f, &f_sz) == 0))
		handler(COULD_NOT_GET_FILE_SIZE);
	close_file_handle(f);
	return f_sz.QuadPart;
  #else
	static struct stat stat_buf;
	const auto rc3 = stat(fp, &stat_buf);
	if (unlikely(rc3 == -1))
		handler(COULD_NOT_STAT_FILE, fp);
	return stat_buf.st_size;
  #endif
}
#endif


}
