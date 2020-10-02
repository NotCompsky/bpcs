% BPCS(1) BPCS User Manual
% NotCompsky
% 22 March 2019

# NAME

bpcs - embed/extract data stream to/from vessel PNG image(s)

# SYNOPSIS

bpcs [*-o* *fmt*] *threshold* *vessel_image_1* ...

# USAGE

bpcs *threshold* *vessel_image_1* ... | [*operations_on_data_stream*] | bpcs-fmt [*options*]
:   Extracting

bpcs-fmt [*options*] -m msg_file_1 ... | [*operations_on_data_stream*] | bpcs [*-o* *fmt*] *threshold* *vessel_image_1* ...
:   Embedding

# DESCRIPTION

Efficient steganographic tool using the BPCS method, using generic PNG images.

# ARGUMENTS

*threshold*
:   A non-negative integer representing the complexity threshold for each grid. For a grid that is 9x9, the maximum threshold is `2 x 8 x 9 == 144`, and a sensible threshold value is usually around half the maximum grid complexity (for a 9x9 grid, that would be 72).

*vessel_image_path(s)*
:   File path(s) of images that transport the message data.
    
    These must be PNG images with 3 channels and a bit depth of 8.
    
    If **-o** is specified, these images are used to create images that contain the message data. If not, message data is read from these images.

    The vessel images are used in series, in the order they are specified. When the message files are exhausted, any remaining vessel images are ignored.

# OPTIONS

-o *fmt*
:   Format of output files (using {curly braces} string substitution).

    This is the format of the transporting images created from embedding the messages into the vessels, formatted using the vessel image file paths.

    Parameters are: *basename*, *dir*, *ext*, *fname*, *fp*
    
    Sets mode to embedding.

# EXAMPLES

In descending order of usefulness.

`bpcs-fmt -m msg1.txt | bpcs -o '{basename}1.png' 71 foo.png`
:   Embed 'msg.txt' into 'foo.png', with a complexity threshold of 71, and write the resulting transporting image to foo1.png

`bpcs-fmt -m msg1.txt | openssl enc -e -aes-256-cbc -salt | bpcs -o '{basename}1.png' 71 foo.png`
:   Embed 'msg.txt' (with both its contents and metadata encrypted) into 'foo.png', with a complexity threshold of 71, and write the resulting transporting image to foo1.png

`bpcs 71 foo1.png | bpcs-fmt -o '{fname}'`
:   For each file content found embedded in foo1.png, extract it to the original file name (the name of the embedded file). If using foo.png from above, it would result in writing msg1.txt's contents to msg1.txt.

`bpcs 71 foo1.png | openssl enc -e -aes-256-cbc -salt -d 2>/dev/null | bpcs-fmt -o '{fname}'`
:   Decrypts the data extracted from foo1.png and then, for each file content found, extract it to the original file name (the name of the embedded file). If using foo.png from above, it would result in writing msg1.txt's contents to msg1.txt.
    We ignore openssl's stderr output, as it will most likely complain about junk data.

`bpcs 71 foo1.png | bpcs-fmt > msg1.txt`
:   Extract all the embedded file content of foo1.png, and pipe it to msg1.txt. Note that this does not include the embedded files' names. If using foo.png from above, it would have the same result as the previous command.

`bpcs-fmt -m msg1.txt msg2.txt | bpcs -o '{basename}12.png' 71 foo.png`
:   Embed two text files into 'foo.png', with a complexity threshold of 71, and write the resulting transporting image to foo12.png

`bpcs 71 foo12.png | bpcs-fmt -o 'extracted/{fname}'`
:   Extract from foo12.png, and stream each individual file found within it to files in the current directory that preserve the names the files were embedded with. So with the previous example, it would extract the files 'msg1.txt' and 'msg2.txt', and send them to 'extracted/msg1.txt' and 'extracted/msg2.txt' respectively.

`bpcs-fmt -m firefox.exe | bpcs -o 'ff.{basename}.png' 71 foo.png bar.png ipsum.png`
:   Embed 'firefox.exe' into three PNGs in series, with a complexity threshold of 71, and write the resulting transporting images to ff.foo.png, ff.bar.png, and ff.ipsum.png (the latter two only created if necessary).

`bpcs 71 ff.foo.png ff.bar.png ff.ipsum.png | bpcs-fmt > firefox.exe`
:   Extract from these vessel images, and stream the extracted data to ./firefox.exe

# UNUSUAL USES

`tar -czf0 /tmp/messages/*.pdf | bpcs -o '/tmp/vessels/{fname}' 71 /tmp/img/*.png`
:   Create tarball and embed into vessel images.

`bpcs 71 /tmp/vessels/*.png | tar -zxf -`
:   Extract and untar from the vessel images created in the previous example. If your version of tar complains about the extra 'padding' bytes bpcs produces, pipe through bpcs-fmt as normal in this and the above examples.

`bpcs 71 foo.png | bpcs-fmt | vlc -`
:   Extract from foo.png and pipe to VLC

`(mkfifo dummy.pipe && bpcs 71 foo.png | bpcs-fmt > dummy.pipe && bpcs-fmt -m dummy.pipe | bpcs -o '{fname}' 71 foo.png)& typed-piper dummy.pipe`
:   This will extract the embedded contents of foo.png, pipe it to dummy.pipe, whereupon it will be opened by the 'typed-piper' text editor and available for editing. Upon saving, the edited contents will be piped back to dummy.pipe, and embedded back into foo.png, overwriting it.

# COMMON MISTAKES

`Using a 'bad' PNG`
:   No resources are wasted in checking/correcting PNG inputs. This expects 'good' PNG inputs. If there is a question about the 'quality' of PNGs, run image-magick or similar on the PNGs, remembering to **-alpha remove**.

`Not wrapping the key in quotation marks`
:   Spaces may exist within this key

# BUGS

**Bad behaviour, including silent failure, for incorrect usage is not considered a bug, so long as it immediately exits with an error exit code.**

## CRITICAL BUGS

None known.

## SECONDARY BUGS

If vessel images contains fewer than 2 complex grids, program will loop and crash.

## MISSING FEATURES

Cannot read files embedded by systems with a different endianness.

No 'cannot find message file' error - program writes first vessel image and exits.

Due to extraction not terminating at the end of embedded data (instead waiting for termination signal), junk data of effectively random bytes will be appended to the end of the extracted data. This will cause certain operations on the extracted stream to fail if either the operation accepts only a limited subset of bytes - for example, base64 encoding - or the operation expects a specific length of data.

# ROADMAP

This tool is still in active development.

Performance improvements are always planned.

Some lightweight features may be added.

Backwards compatibility is not a priority at this time.

## Examples of features that will not be added

* QoL bloat such as checking for the existence of directories before writing output files

# RETURN CODES

| Value | Meaning |
| ----- | ------- |
| 0 | Success |
| 4 | Memory error |
| 5 | Cannot write to file |
| 10 | Bitplane is empty (has no columns) |
| 60 | Incorrect vessel image format error: Bad signature in file |
| 61 | Incorrect vessel image format error: Non-RGB type |
| 64 | png_write_image failed |
| 65 | png_write_end failed |
| 66 | png_create_write_struct failed |
| 67 | png_create_info_struct failed |
| 68 | png_init_io failed |
| 69 | Misc libpng error |
| 222 | Conjugation map not set |
| 255 | Exhausted all vessel images before exhausting input |

# CONTRIBUTING

Bug reports/fixes, performance improvements/suggestions, documentation improvements (such as translations), etc. welcomed.

Repository is found at https://github.com/NotCompsky/bpcs

# SEE ALSO
bpcs-fmt(1), bpcs-v(1), bpcs(3), steghide(1), outguess(1)
