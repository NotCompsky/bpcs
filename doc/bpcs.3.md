% BPCS(3) BPCS User Manual
% NotCompsky
% 27 March 2019

# NAME

bpcs - embed/extract data stream to/from vessel PNG image(s)

# SYNOPSIS

**#include \<bpcs/bpcs.h\>**

**BPCSStreamBuf bpcs_stream(uint8_t *min_complexity*, int16_t *img_n*, int16_t *n_imgs*, char\*\* *img_fps*, bool *embedding*, char\* *out_fmt*);**

# ARGUMENTS

*min_complexity*
:   Grids with complexity below this value are ignored

*img_n*
:   Index offset of img_fps

*n_imgs*
:   Number of elements of img_fps

*embedding*
:   If writing data to output vessel images

*out_fmt*
:   See **-o** option in bpcs(1)

# USAGE

bpcs_stream.load_next_img(); // Initialise bpcs_stream for reading/writing

# DESCRIPTION

**BPCSStreamBuf** is a stream buffer that writes/reads bytes, via BPCS steganography, from a vector of PNG images.

# EXAMPLES

See the bpcs program for example usage.

# MISC

See bpcs(1) for other sections including bugs, roadmap, contributing.

# SEE ALSO
bpcs(1)
