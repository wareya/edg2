EDG
===

*Easy Digital Graphics* is a modern bitmap format designed as a common denominator between network bitmap graphics and professional bitmap graphics.

All EDG images are stored in sRGB, but EDG supports arbitrary HDR value storage, so gamuts larger than sRGB can be represented. Grayscale and alpha channels are also supported.

EDG's specification is informal, but highly articulate, and uses formal/technical language in some places. This is for ease of understanding to people who already understand bitmap graphics.

For more information, continue reading this readme, and read the plaintext EDG2 file in this repository.

Motivation
----------

EDG is designed to be a replacement and alternative to formats like DIB (e.g. Windows BMP). While many of the image formats out there are very extensible and very powerful, they all have bad growing pains and have become excessively complicated to use in arbitrary colorspaces or for HDR or for dumping raw image data to disk.

libpng had has numerous security problems. DIB is a joke of poorly-supported extensions and complicated semantics. Farbfeld is a bikeshed that only stores consumer mountain bikes. EDG may be teetering on the edge of a cliff, but at least it works.

I made EDG so that I can write C++ programs to experiment with image processing. This is probably its most appropriate usecase.

Included
--------

This repository contains a library and a few programs:

* libedg, which can be included directly in your project if desired and compatible, is the reference encoder/decoder.
* edg2bmp, which uses stb\_image\_write, converts an EDG file to BMP, within stb\_image\_write's supported output formats.
* bmp2edg, which uses stb\_image, converts a BMP file to EDG.
* edgpop, an example program, upscales an EDG image to 2x using bilinear EDI.
* edgmain, a rudimentary make-save-load cycle tester.

Summary
-------
EDG stores a very simple 16-byte header followed by raw bitmap data. The bitmap field contains no padding or alignment semantics anywhere.

###Image Data

Image data is stored component-by-component, pixel-by-pixel, starting in the top left and moving right before moving downwards, wrapping around at the end of each scanline. For more details, read the plaintext file EDG2 in this repository.

###Header format

    00000000   45 44 47 00  hh hh hh hh  ww ww ww ww  FF mm xx xx  EDG.............

    h: Height value as 32-bit integer, starting at 0
    w: Width value as 32-bit integer, starting at 0
    m: A field of info flags (see below)
    x: An xorsum of the previous 16-bit groups in the header.

* *The dimensions are measured as spans instead of centers for two reasons. One, so that zero-size images cannot be represented. Two, so that images with the maximum power of two as its dimensions are representable. For example, if h or w were 8-bit, starting at 0 would be obvious, to store 256x256 images.*

As you can see, the header is very simple.

* Four bytes of magic
* Eight bytes of dimensions
* 0xFF byte to prevent heuristics from detecting text, and a binary mode sentry
* Image format bitfield
* Two-byte integrity sentry

###Info flags
More-significant bits to the left.

    ABCDEFGH

    A: If on, indicates that pixels are made of separate linear-gamma 32-bit IEEE floats. If off, pixel values are made of gamma-corrected unsigned 8-bit integers (i.e. like normal network graphics).
    B: If on, indicates that pixels have one value channel instead of three (grayscale).
    C: If on, indicates that pixels have an additional alpha channel.
    D: If on, indicates that the file was written in little endian. File is big endian otherwise. Endian affects only the byte order of individual multibyte values, including the dimensions. For example, the byte order of a 24-bit RGB triad is not affected by endian, nor is the magic number at the start of the header.
    EFGH: If on, each indicate that a particular edge should be treated a particular way in certain situations in interpolation. Each bit is non-indicative if off. For full explanation, read the informal specification in this repository, EDG2.

###Robust

EDG is highly defined, and tries to leave few holes for decoders to behave differently from eachother when they're not supposed to. EDG specifies exactly what's intended with regards to file truncation, file padding, alpha mixing, and more. Implementations are allowed to behave differently in regards to these, but only if they're special-purpose implementations.

There is one place that's left underspecified, because The Right Thing To Do is pretty much entirely dependent on the application at hand: Weird IEEE floating point values. Non-numeric floats are undefined, and can treated as an arbitrary color, a random color, a hole in the image, missing information that must be interpolated, or even an error-on-access. If your implementation must be resiliant, it's up to you to handle possible floating point exceptions yourself.

Additionally, the behavior of fully negative colors is application-defined, because it's useful for depicting imaginary colorspaces but useless for network graphics.

Wherein I Answer Possible Questions
-----------------------------------

**Q:** Why EDG?

**A:** Other image formats are extremely difficult to implement by hand. Even bmp images are problematic with the large number of useless color formats and the fact that scanlines are padded. Other uncompressed formats like pbm and farbfeld make useless tradeoffs that are only relevant to extremely esoteric usecases.

Image formats also always have holes in their specifications (since the formats are so complicated) that cause different implementations to interpret things differently. EDG is designed to be mildly futureproof by intentionally specifying as many edgecases as possible, including things that a good image parser should never do, while remaining as simple as possible for the formats it provides.

In other words, the following formats are all nightmares to correctly implement yourself: TGA (complex format with spec discrepancies), BMP (asinine design decisions), PNG (compression, meaningful chunks), JPG (DCT, compression, meaningful chunks), TIFF (monstrosity), Farbfeld (intentionally underspecified and with obtuse design decisions)

**Q:** What endian is xorsum in?

**A:** Are you ill?

**Q:** Why are the axes y,x instead of x,y?

**A:** With row-major memory layouts of tiles and image data, y,x is the big-endian ordering. A unit change in x results in a smaller address change than a unit change in y. Putting y on the left also makes it slightly more likely for programmers unfamiliar with memory layout performance to do the right thing (i.e. putting x on their inner loops instead of the outer ones)

**Q:** Compression?

**A:** EDG itself does not provide compression. However, direct compression with bzip2 is encouraged. bzip2-compressed EDG has a file extension of ".edz".

**Q:** Why do you recommend bzip2 instead of <X>?

**A:** bzip2 is the most common resilient general purpose compression format with a good tradeoff between speed, memory usage, and compression efficiency. It's appropriate for streaming and low-powered devices so that images with as many as 2 to 16 megapixels should not lag horribly on load on as many devices as LZMA based compression would, and it still provides good enough compression efficiency to be appropriate for mixed content images (e.g. web screenshots) unlike gzip. The reference implementation is permissively licensed and very appropriate for web imagery; LZMA is not appropriate for web imagery because high LZMA compression settings make decompression extremely resource intensive.

If I'm wrong, and EDG takes off, and people use a different compression function, that's fine, too. bzip2 is just my current recommendation.

**Q:** Why is the project licensed under the Apache License 2.0 instead of a simpler permissive license?

**A:** Software patents are my enemy. If you want to use the reference EDG implementation in a more permissive project, without infecting your project's license layout, please email me and ask for a more permissive license. Note: If your project is GPLv2-not-newer, I probably won't give you a more permissive license unless you have a good reason; please first consider putting sufficient indirection between your program and libedg.

**Q:** This looks like farbfeld with more stuff in the header and a different pixel format.
**A:** It's not farbfeld. Any close-to-raw image format will look like farbfeld. Comparisons are meaningless.


**Q:** Why do you specify how the image data should be used, like the alpha function?

**A:** It's exactly the same thing as specifying the colorspace of the image data: it directly controls how the image actually appears on the screen. If you're creating a program that is necessarily expected to behave in a different way than this spec, then by all means, ignore the recommendation. However, if you're creating a program that is *allowed* to mix alpha in linear light, then specifying that you must do the alpha mixing of linear sRGB values in linear sRGB space is the only way to guarantee that programs in a similar situation to yours are doing the same thing.
