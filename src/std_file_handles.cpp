#include "std_file_handles.hpp"


#ifdef _WIN32
const HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
const HANDLE stdin_handle  = GetStdHandle(STD_INPUT_HANDLE);
#endif
