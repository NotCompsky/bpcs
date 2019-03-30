# INTRODUCTION

Bit Plane Complexity Segmentation (BPCS) steganography is a method of embedding a message file in an image through replacing the most "complex" regions with the data from the message file.

The idea behind it is that these complex grids appear as noise to the human eye.

The main draw of this method is the efficiency of data embedding. With sensible vessel images - for instance, pictures taken by cameras rather than digital cartoons - one can replace almost half of the vessel image with embedded data without visibly altering its appearance.

# IMPLEMENTATION

This project at its core is an implementation of the method Khaire and Nalbalwar in Shrikant Khaire et. al describe in "Review: Steganography – Bit Plane Complexity Segmentation (BPCS) Technique" in the International Journal of Engineering Science and Technology Vol. 2(9), 2010, 4860-4868.

The image is split into its constituent channels, and these into their consitutent bitplanes. The average image has a bit-depth of 8 and 3 channels, so gives us 24 bitplanes to work with.

Each bitplane is split up into 8x8 grids, each of which is assigned a 'complexity', where 'complexity' is defined as the sum of the changes in value (from 0 to 1 or vice versa) of horizontally and vertically adjacent elements.

Those whose 'complexity' is above a specified threshold will be entirely written over with data. If the resulting grid's complexity falls below the threshold, it is 'conjugated' such that its complexity again rises above the threshold (and is therefore retrievable).

# USES

Steganography in general has a variety of uses. For instance:

    * Watermarking copywrited materials so that its source can be tracked in the event of piracy
    * Watermarking banknotes designs to allow compliant software to refuse to edit them
    * Watermarking printer pages to embed serial numbers and datetime stamps.
    * Communication between intelligence agents
    * Online puzzles
    * Obfuscating code that is to be executed

This project, however, is particularly useful for 

# FEATURES

    * Embed any number of message files into any number of vessel images.
    * 

# USAGE

See [the manual](doc/bpcs.md) for general use.

See [the manual](doc/bpcs-v.md) for debugger usage.

# DISCLAIMER

This was my first taste of C++. I had only fleeting experience with C from writing a handful of things interacting with the Linux OS. I was self-teaching the language while creating this project, using a text editor, and a compile-run-rewrite cycle of debugging. In other words, please do not look into the commit history for inspiration of good coding practices!

# LICENSE

All rights reserved for now. Will likely move to a GPLv2 or GPLv3 license.

# VERSIONING

See [VERSIONING.md](VERSIONING.md)

# BENCHMARKS

Compared to [steghide](http://steghide.sourceforge.net/), this program has around 5x storage efficiency, and 10x more throughput (over 1MB/s, compared to 100KB/s on my machine). However, steghide uses a very different method that I suspect is more resistant to steganalysis.

# SEE ALSO

A [BPCS implementation in python](https://github.com/mobeets/bpcs)
