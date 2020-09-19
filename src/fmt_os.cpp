#include "fmt_os.hpp"
#include "errors.hpp"
#include <unistd.h>
#include <fcntl.h> // for open, O_WRONLY
#include <sys/stat.h>
#include <cerrno>


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


namespace os {


int create_file_with_parent_dirs(char* const file_path,  const size_t file_path_len){
#ifdef CHITTY_CHATTY
	fprintf(stderr,  "Creating file: %s\n",  file_path);
#endif
	
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


void read_exact_number_of_bytes_from_stdin(char* const buf,  const size_t n){
	size_t offset = 0;
	do {
		offset += read(STDIN_FILENO,  buf + offset,  n - offset);
	} while (offset != n);
}


void write_exact_number_of_bytes_to_stdout(char* const buf,  size_t n){
	do {
		n -= write(STDOUT_FILENO, buf, n);
	} while (n != 0);
}


void sendfile_from_stdout_to_file(const char* const fp,  const size_t n_bytes){
	const int msg_file = open(fp, O_RDONLY);
  #ifdef TESTS
	if (unlikely(msg_file == 0)){
		handler(CANNOT_OPEN_FILE);
	}
  #endif
	const auto rc5 = sendfile(STDOUT_FILENO, msg_file, nullptr, n_bytes);
	if (unlikely(rc5 == -1))
		handler(SENDFILE_ERROR);
	close(msg_file);
}


void splice_from_stdin_to_fd(const fout_typ fout,  const size_t n_bytes){
	loff_t n_bytes_written = 0;
	size_t n_bytes_yet_to_write = n_bytes;
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
			handler(msg_id);
		}
		#endif
		n_bytes_yet_to_write -= n_writ;
	} while(n_bytes_yet_to_write != 0);
}


#ifdef EMBEDDOR
size_t get_file_sz(const char* const fp){
	static struct stat stat_buf;
	const auto rc3 = stat(fp, &stat_buf);
  #ifdef TESTS
	if (unlikely(rc3 == -1)){
		handler(COULD_NOT_STAT_FILE, fp);
	}
  #endif
	return stat_buf.st_size;
}
#endif


}
