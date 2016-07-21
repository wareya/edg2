#ifndef EDGUP_LIB
#define EDGUP_LIB

/*
   Copyright 2016 Alexander Nadeau <wareya@gmail.com>

   Licensed under the Apache License, Version 2.0 (the "LICENSE");
   you may not use this file except in compliance with the LICENSE.
   You may obtain a copy of the LICENSE at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the LICENSE is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the LICENSE for the specific language governing permissions and
   limitations under the LICENSE.
*/

/*
   Note:
   
   This file's license is incompatible with old versions of the GPL and
   related licenses. To use this file's functionality with such software,
   you need to put sufficient indirection between the two that their
   licenses do not apply to eachothers' covered material. The necessary
   level and kind of indirection differs between the LGPL, GPL, and AGPL.
   
   If you maintain a public domain or permissively licensed library and
   want to implement EDG, please email me and ask for permission to use
   this library under more permissive terms for porting.
*/

#include <stdint.h>

/* IMPORTANT!
// height/width START AT 0. An image of one pixel has a height and with of zero! A square with four pixels has a height and width of one!
// This is to allow images with 2^32 in a given dimension to be represented, and to prevent alternative implementations from accidentally having a "zero pixel hole" of undefined behavior.
*/

// format: if true, values are 32-bit floats instead of u8 chars.
// grayscale: if true, pixels have one color value instead of three.
// alpha: if true, pixels have an additional "alpha" value.
// tile___: if true, indicates that the image specifically specifies that a given edge should be center-aligned / match edges (yes, those are the same thing). if false, indicates undefined alignment/matching, not opposite matching.

extern const char * edgerr;

struct edginfo
{
    uint32_t height, width;
    
    bool format;
    bool grayscale;
    bool alpha;
    bool endian;
    
    bool tileup;
    bool tiledown;
    bool tileleft;
    bool tileright;
};

// data format: top-left origin, next pixel is rightwards before downwarrds, values packed per pixel in RGBA order, one at a time
struct edg
{
    edginfo info;
    uint64_t size;
    unsigned char * data;
};

// Loads an EDG file from disk, then closes the file, without modifying it. Allocates a buffer and an info struct.
// Returns nullptr and sets edgerr on failure.
// Note: Does NOT return an edg with the same endian as the file. ONLY loads edg files into native endian. Sets edginfo's endian field to native endian.
edg * edg_open(const char * filename);
// Makes an EDG in RAM. Allocates a buffer and an info struct.
// Returns nullptr and sets edgerr on failure.
// NOTE: HEIGHT AND WIDTH MEASURE PIXEL SPANS, NOT PIXEL CENTERS (counting starts at 0, not 1, for non-empty images)
edg * edg_make(uint32_t height, uint32_t width, bool format, bool grayscale, bool alpha);
// Saves an EDG from ram to disk. Does not modify the EDG in ram.
// Returns an error code and sets edgerr on error.
int32_t edg_save(edg * edge, const char * filename);
// Kills an EDG that exists in RAM. Deallocates the buffer, and the info struct.
// Returns an error code and sets edgerr on error.
int32_t edg_kill(edg * edge);

#endif // EDGUP_LIB
