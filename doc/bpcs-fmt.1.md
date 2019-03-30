% BPCS-FMT(1) BPCS-FMT User Manual
% NotCompsky
% 22 March 2019

# NAME

bpcs-fmt - format data piped to/from bpcs

# SYNOPSIS

bpcs-fmt [*-v*] [*-o* *fmt*] [*options*]

# DESCRIPTION

Formats data piped to/from bpcs, optionally redirecting to output files.

As the formatting is seperate from the embedding/extracting, any stream operation can be applied to the written/read data - for instance, encryption/decryption.

# OPTIONS

-v
:   Verbose mode. Prints paths of files written. Should generally not be used when piping extracted output.

-Q
:   Do not pipe extraction output to stdout

-o *fmt*
:   Format of output files (using {curly braces} string substitution).

    This flag sets the mode to 'extracting to disk', and streams the extracted contents to output files to paths formed using the content file paths.

    Parameters are: *basename*, *dir*, *ext*, *fname*, *fp*

-m *filepath1* ...
:   Message files (delineated by space) to embed.
    Can be any type of file, even named pipes.
    Sets mode to 'embedding'.
    **-o** and **-m** are mutually exclusive.

# SEE ALSO
bpcs(1)
