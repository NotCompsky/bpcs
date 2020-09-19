#pragma once

#include "bpcs.hpp"
#ifdef _WIN32
# include "windows.h"
#endif


namespace os {


size_t extract_to_stdout(BPCSStreamBuf& bpcs_stream);

#ifdef EMBEDDOR
void embed_from_stdin(BPCSStreamBuf& bpcs_stream);
#endif


} // namespace os
