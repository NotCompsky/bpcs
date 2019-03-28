% BPCS(1) BPCS User Manual
% NotCompsky
% 06 March 2019

# NAME

bpcs-v - bpcs debug build

# SYNOPSIS

Same as bpcs

# EXAMPLES

`bpcs-v -Q -f 114975 -t 115975 1 /tmp/vsl/*.png`
:   Debug the extraction process from byte 114975 to byte 115975

# ADDITIONAL OPTIONS

-f *n*
:   Maximum debug output starting from byte number *n*

-t *n*
:   Exit after byte number *n*

-Q
:   Do not pipe extraction output to stdout

-q
:   Decrease verbosity.

-v
:   Increase verbosity.

# SEE ALSO
bpcs(1)
