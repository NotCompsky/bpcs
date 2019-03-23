% BPCS(3) BPCS User Manual
% NotCompsky
% 23 March 2019

# NAME

bpcs - embed/extract data stream to/from vessel PNG image(s)

# SYNOPSIS

**#include \<bpcs/bpcs.h\>**

**BPCSStreamBuf bpcs_stream(uint8_t *min_complexity*, std::vector\<char\*\>& *img_fps*, bool *embedding*, char\* *outfmt*);**

# USAGE

bpcs_stream.load_next_img(); // Initialise bpcs_stream for reading/writing

# DESCRIPTION

**BPCSStreamBuf** is a stream buffer that writes/reads bytes, via BPCS steganography, from a vector of PNG images.

# EXAMPLES

See the bpcs program for example usage.

# BUGS
No known bugs.

# ROADMAP

This tool is still in active development.

Performance improvements are always planned.

Some lightweight features may be added.

# CONTRIBUTING

Bug reports/fixes, performance improvements/suggestions, documentation improvements (such as translations), etc. welcomed.

Repository is found at https://github.com/NotCompsky/bpcs

# SEE ALSO
bpcs(1)
