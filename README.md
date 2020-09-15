<p align="center">
	<h1 align="center">bpcs</h1>
</p>

<p align="center">
	<a href="LICENSE"><img src="https://img.shields.io/github/license/NotCompsky/bpcs"/></a>
	<a href="https://github.com/notcompsky/bpcs/releases"><img src="https://img.shields.io/github/v/release/NotCompsky/bpcs"/></a>
	<a href="https://hub.docker.com/repository/docker/notcompsky/bpcs/tags"><img src="https://img.shields.io/docker/image-size/notcompsky/bpcs?label=Docker%20image"/></a>
	<a href="https://github.com/notcompsky/bpcs/graphs/commit-activity"><img src="https://img.shields.io/github/commit-activity/w/NotCompsky/bpcs"/>
</p>

# INTRODUCTION

<figure class="image">
	<img src="https://user-images.githubusercontent.com/30552567/93227238-12abd080-f76c-11ea-8243-65ccb365759c.png">
	<figcaption>This entire repository, along with portable binaries for Linux, are contained - encrypted - in this vessel image. Photograph colourised by Marina Amaral.</figcaption>
</figure>

Bit Plane Complexity Segmentation (BPCS) steganography is a method of embedding a message file in an image through replacing the most "complex" regions with the data from the message file.

The idea behind it is that these complex grids appear as noise to the human eye.

The main draw of this method is the efficiency of data embedding. With sensible vessel images - for instance, pictures taken by cameras rather than digital cartoons - one can replace almost half of the vessel image with embedded data without visibly altering its appearance.

# COMPATIBILITY

The code assumes a UNIX-like operating system. It will work on any Linux platform except Linux kernel 2.6.x series before Linux 2.6.33, due to the use of `sendfile`. BSD and MacOS I have not tested, however they appear to implement `sendfile` and `splice` too, so they should be fine.

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
    * Skirting firewall rules (with infiltrating/exfiltrating of data over embedded transport layers)
    * Online puzzles
    * Obfuscating code that is to be executed

Due to its high efficiency and throughput - in a good image, roughly 50% of the vessel image's size can be used to store the hidden data, and even a single CPU thread has a throughput of 5MB/s without GPU acceleration - BPCS is viable for many applications where other stego techniques simply are not, such as streaming 4K video over HTTP using PNG images.

The efficiency depends very highly on the vessel images used. Some images can handily fit their originals within themselves, sometimes even resulting in a smaller file size.

In my view, BPCS steganography is not resilient to stego analysis; it is more difficult to detect than trivial stego techniques such as LSB, but it is not designed to evade detection, unlike [steghide](http://steghide.sourceforge.net/).

# FEATURES

    * High performance - between 5-10MB/s on a single CPU thread on my mid-tier machine.
    * Any number of files can be embedded and extracted.
    * Any encryption can be applied.

# USAGE

See [the manual](doc/bpcs.1.md) for general use.

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
