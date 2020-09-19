#pragma once

#include <compsky/macros/likely.hpp>


#ifdef _WIN32
typedef HANDLE fout_typ;
# define STDOUT_DESCR GetStdHandle(STD_OUTPUT_HANDLE);
#else
# include <unistd.h> // for STDOUT_FILENO
typedef int fout_typ;
# define STDOUT_DESCR STDOUT_FILENO;
#endif


namespace os {


void read_exact_number_of_bytes_from_stdin(char* const buf,  const size_t n);

void write_exact_number_of_bytes_to_stdout(char* const buf,  size_t n);

void sendfile_from_stdout_to_file(const char* const fp,  const size_t n_bytes);

void splice_from_stdin_to_fd(const fout_typ fout,  const size_t n_bytes);

int create_file_with_parent_dirs(char* const file_path,  const size_t file_path_len);

size_t get_file_sz(const char* const fp);


} // namespace os
