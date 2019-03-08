% BPCS(1) BPCS User Manual
% NotCompsky
% 06 March 2019

# NAME

bpcs - embed/extract message files(s) to/from vessel PNG image(s)

# SYNOPSIS

bpcs [*options*] *threshold* *vessel_image_1* ...

# DESCRIPTION

Efficient steganographic tool using the BPCS method.

# ARGUMENTS

*threshold*
:   Integer between 1 and 56 (inclusive). Values between 50 and 56 are highly recommended.
    Grids with complexity greater than this number are overwritten with message data.
    Higher values (up to 112) can be used, but 56 is the limit of what is guaranteed to be embeddable.

*vessel_image_path(s)*
:   File path(s) of images that transport the message data.

    If **-m** is specified, these images are used to create images that contain the message data. If not, message data is read from these images.

    The vessel images are used in series, in the order they are specified. When the message files are exhausted, any remaining vessel images are ignored.

# OPTIONS

-m *filepath1* ...
:   Message files (delineated by space) to embed.
    Can be any type of file, even named pipes.
    If not specified, mode will be set to 'extract'.

-o *format*
:   Format of output files (using {curly braces} string substitution).

    If embedding, this is the format of the transporting images created from embedding the messages into the vessels, formatted using the vessel image file paths.

    If extracting, this flag sets the mode to 'save to disk', and streams the extracted contents to output files to paths formed using the content file paths.

    Parameters are: *basename*, *dir*, *ext*, *fname*, *fp*

-v
:   Verbose mode. Prints paths of files written. Should generally not be used when piping extracted output.

# EXAMPLES

In descending order of usefulness.

`bpcs -m msg1.txt -o '{basename}1.png' 51 foo.png`
:   Embed 'msg.txt' into 'foo.png', with a complexity threshold of 51, and write the resulting transporting image to foo1.png

`bpcs -o '{fname}' 51 foo1.png`
:   For each file content found embedded in foo1.png, extract it to the original file name (the name of the embedded file). If using foo.png from above, it would result in writing msg1.txt's contents to msg1.txt.

`bpcs 51 foo1.png > msg1.txt`
:   Extract all the embedded file content of foo1.png, and pipe it to msg1.txt. Note that this does not include the embedded files' names. If using foo.png from above, it would have the same result as the previous command.

`bpcs -m msg1.txt msg2.txt -o '{basename}12.png' 51 foo.png`
:   Embed two text files into 'foo.png', with a complexity threshold of 51, and write the resulting transporting image to foo12.png

`bpcs 51 -o 'extracted/{fname}' foo12.png`
:   Extract from foo12.png, and stream each individual file found within it to files in the current directory that preserve the names the files were embedded with. So with the previous example, it would extract the files 'msg1.txt' and 'msg2.txt', and send them to 'extracted/msg1.txt' and 'extracted/msg2.txt' respectively.

`bpcs -m firefox.exe -o 'ff.{basename}.png' 51 foo.png bar.png ipsum.png`
:   Embed 'firefox.exe' into three PNGs in series, with a complexity threshold of 51, and write the resulting transporting images to ff.foo.png, ff.bar.png, and ff.ipsum.png (the latter two only created if necessary).

`bpcs 51 ff.foo.png ff.bar.png ff.ipsum.png > firefox.exe`
:   Extract from these vessel images, and stream the extracted data to ./firefox.exe

# UNUSUAL USES

`bpcs 51 foo.png | vlc -`
:   Extract from foo.png and pipe to VLC

`(mkfifo dummy.pipe && bpcs 51 foo.png > dummy.pipe && bpcs -m dummy.pipe -o '{fname}' 51 foo.png)& typed-piper dummy.pipe`
:   This will extract the embedded contents of foo.png, pipe it to dummy.pipe, whereupon it will be opened by the 'typed-piper' text editor and available for editing. Upon saving, the edited contents will be piped back to dummy.pipe, and embedded back into foo.png, overwriting it.

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
steghide(1), outguess(1)
