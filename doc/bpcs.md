% BPCS(1) BPCS User Manual
% NotCompsky
% 22 March 2019

# NAME

bpcs - embed/extract data stream to/from vessel PNG image(s)

# SYNOPSIS

bpcs [-o] *threshold* *vessel_image_1* ...

# USAGE

bpcs *threshold* *vessel_image_1* ... | [*operations_on_data_stream*] | bpcs-fmt [*options*]
:   Extracting

bpcs-fmt [*options*] -m msg_file_1 ... | [*operations_on_data_stream*] | bpcs [-o] *threshold* *vessel_image_1* ...
:   Embedding (Minimal)

N=$(bpcs *vessel_image_1* ... | wc --bytes) && bpcs-fmt [*options*] -N $N -m msg_file_1 ... | [*operations_on_data_stream*] | bpcs [-o] *threshold* *vessel_image_1* ...
:   Embedding (Full)
    Overwrites all complex grids of vessel images with data.
    If the *operations_on_data_stream* produce output that is sensitive about its size - for instance, encryption that will error out if it encounters junk bytes when decrypting - use this to 

# DESCRIPTION

Efficient steganographic tool using the BPCS method, using generic PNG images.

# ARGUMENTS

*threshold*
:   Integer between 0 and 6 (inclusive). The complexity threshold is this number plus 50.

*vessel_image_path(s)*
:   File path(s) of images that transport the message data.
    
    These must be PNG images with 3 channels and a bit depth of 8.
    
    If **-o** is specified, these images are used to create images that contain the message data. If not, message data is read from these images.

    The vessel images are used in series, in the order they are specified. When the message files are exhausted, any remaining vessel images are ignored.

# OPTIONS

-o *format*
:   Format of output files (using {curly braces} string substitution).

    This is the format of the transporting images created from embedding the messages into the vessels, formatted using the vessel image file paths.

    Parameters are: *basename*, *dir*, *ext*, *fname*, *fp*
    
    Sets mode to embedding.

# EXAMPLES

In descending order of usefulness.

`bpcs-fmt -m msg1.txt | bpcs -o '{basename}1.png' 1 foo.png`
:   Embed 'msg.txt' into 'foo.png', with a complexity threshold of 51, and write the resulting transporting image to foo1.png

`bpcs-fmt -m msg1.txt | openssl enc -e -aes-256-cbc -salt | bpcs -o '{basename}1.png' 1 foo.png`
:   Embed 'msg.txt' (with both its contents and metadata encrypted) into 'foo.png', with a complexity threshold of 51, and write the resulting transporting image to foo1.png

`bpcs 1 foo1.png | bpcs-fmt -o '{fname}'`
:   For each file content found embedded in foo1.png, extract it to the original file name (the name of the embedded file). If using foo.png from above, it would result in writing msg1.txt's contents to msg1.txt.

`bpcs 1 foo1.png | openssl enc -e -aes-256-cbc -salt -d | bpcs-fmt -o '{fname}'`
:   Decrypts the data extracted from foo1.png and then, for each file content found, extract it to the original file name (the name of the embedded file). If using foo.png from above, it would result in writing msg1.txt's contents to msg1.txt.

`bpcs 1 foo1.png | bpcs-fmt > msg1.txt`
:   Extract all the embedded file content of foo1.png, and pipe it to msg1.txt. Note that this does not include the embedded files' names. If using foo.png from above, it would have the same result as the previous command.

`bpcs-fmt -m msg1.txt msg2.txt | bpcs -o '{basename}12.png' 1 foo.png`
:   Embed two text files into 'foo.png', with a complexity threshold of 51, and write the resulting transporting image to foo12.png

`bpcs 1 foo12.png | bpcs-fmt -o 'extracted/{fname}'`
:   Extract from foo12.png, and stream each individual file found within it to files in the current directory that preserve the names the files were embedded with. So with the previous example, it would extract the files 'msg1.txt' and 'msg2.txt', and send them to 'extracted/msg1.txt' and 'extracted/msg2.txt' respectively.

`bpcs-fmt -m firefox.exe | bpcs -o 'ff.{basename}.png' 1 foo.png bar.png ipsum.png`
:   Embed 'firefox.exe' into three PNGs in series, with a complexity threshold of 51, and write the resulting transporting images to ff.foo.png, ff.bar.png, and ff.ipsum.png (the latter two only created if necessary).

`bpcs 1 ff.foo.png ff.bar.png ff.ipsum.png | bpcs-fmt > firefox.exe`
:   Extract from these vessel images, and stream the extracted data to ./firefox.exe

# UNUSUAL USES

`bpcs 1 foo.png | bpcs-fmt | vlc -`
:   Extract from foo.png and pipe to VLC

`(mkfifo dummy.pipe && bpcs 1 foo.png | bpcs-fmt > dummy.pipe && bpcs-fmt -m dummy.pipe | bpcs -o '{fname}' 1 foo.png)& typed-piper dummy.pipe`
:   This will extract the embedded contents of foo.png, pipe it to dummy.pipe, whereupon it will be opened by the 'typed-piper' text editor and available for editing. Upon saving, the edited contents will be piped back to dummy.pipe, and embedded back into foo.png, overwriting it.

# COMMON MISTAKES

`Using a 'bad' PNG`
:   No resources are wasted in checking/correcting PNG inputs. This expects 'good' PNG inputs. If there is a question about the 'quality' of PNGs, run image-magick or similar on the PNGs, remembering to **-alpha remove**.

`Not wrapping the key in quotation marks`
:   Spaces may exist within this key

# BUGS
Cannot read files embedded by systems with a different endianness.

Option parameters cannot begin with "-".

If vessel images must contain fewer than 2 complex grids, program will loop and crash.

Due to extraction not terminating at the end of embedded data (instead waiting for termination signal), junk data of effectively random bytes will be appended to the end of the extracted data. This will cause certain operations on the extracted stream to fail if either the operation accepts only a limited subset of bytes - for example, base64 encoding - or the operation expects a specific length of data.

Bad behaviour, including silent failure, for incorrect usage is not considered a bug, so long as it immediately exits with an error exit code.

# ROADMAP

This tool is still in active development.

Performance improvements are always planned.

Some lightweight features may be added.

# CONTRIBUTING

Bug reports/fixes, performance improvements/suggestions, documentation improvements (such as translations), etc. welcomed.

Repository is found at https://github.com/NotCompsky/bpcs

# SEE ALSO
steghide(1), outguess(1)
