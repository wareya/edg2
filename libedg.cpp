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

// g++ --std=c++14 edgup.cpp

#include "libedg.hpp"

#include <stdio.h>
#include <fstream> // C IO does not guarantee 64-bit offset support. C++11 std::streamoff guarentees "a signed integral type of sufficient size to represent the maximum possible file size supported by the operating system".
#include <stdint.h>
#include <string.h> // memcmp
#include <math.h>
#include <functional> // std::function (for defer)
#include <limits.h> // CHAR_BIT

static_assert(CHAR_BIT == 8, "Platform does not use 8-bit bytes.");

#ifndef NULL
#define NULL 0
#endif // NULL

#define HAVE_LITTLE_ENDIAN_PLATFORM (!(*(uint16_t *)"\0\xff" < 0x100))

float srgb2linear(float srgb) {
    if(srgb < 0) srgb = 0;
    if(srgb > 0.0404482362771082f)
        return powf(((srgb+0.055f)/1.055f), 2.4f);
    else
        return srgb/12.92f;
}

float linear2srgb(float linear) {
    if(linear < 0) linear = 0;
    if(linear > 0.00313066844250063f)
        return powf(linear,1/2.4f)*1.055f-0.055f;
    else
        return linear*12.92f;
}

void reverse(unsigned char * location, int length)
{
    if(length <= 1) return; // nothing to do
    unsigned char temp;
    for(auto i = 0; i < length/2; i++) // int div truncates so length 5 -> stop before index 2 (third element)
    {
        temp = location[i];
        location[i] = location[length-i-1];
        location[length-i-1] = temp;
    }
}

struct defer
{
    std::function<void(void)> deferred;
    defer(std::function<void(void)> f) { deferred = f; }
    ~defer() { deferred(); }
};

const char * edgerr = "";

// "header" must be a pointer to a block of at least ten bytes.
// data stored in "info".
// arguments must not be nullptr
// undefined behavior if arguments are bogus pointers
// Sets edgerr on negative return
enum {
    EDG_HEADER_API_MISUSE = -4,
    EDG_HEADER_INVALID_XORSUM = -3,
    EDG_HEADER_INVALID_BARRIER = -2,
    EDG_HEADER_NOT_AN_EDG_HEADER = -1,
    EDG_HEADER_SUCCESS = 0
};
int edg_parse_header(const unsigned char * header, edginfo * info)
{
    if(header == nullptr or info == nullptr) return EDG_HEADER_API_MISUSE;
    
    if(memcmp(header, "EDG", 4) != 0) return (edgerr = "Not an EDG file."), EDG_HEADER_NOT_AN_EDG_HEADER;
    
    uint32_t height = *(uint32_t*)(header+4);
    uint32_t width = *(uint32_t*)(header+8);
    
    uint8_t barrier = header[12];
    uint8_t flags = header[13];
    
    uint8_t xorsum[2];
    xorsum[0] = header[14];
    xorsum[1] = header[15];
    
    // Recalculate xorsum
    uint8_t xorsum_check[2];
    xorsum_check[0] = 0;
    xorsum_check[1] = 0;
    for(auto i = 0; i < 7; i++)
    {
        xorsum_check[0] ^= header[i*2];
        xorsum_check[1] ^= header[i*2+1];
    }
    
    // Interpret data
    
    bool format    = flags&0b1000'0000;
    bool grayscale = flags&0b0100'0000;
    bool alpha     = flags&0b0010'0000;
    bool endian    = flags&0b0001'0000;
    
    bool tileup    = flags&0b0000'1000;
    bool tiledown  = flags&0b0000'0100;
    bool tileleft  = flags&0b0000'0010;
    bool tileright = flags&0b0000'0001;
    
    if(endian != HAVE_LITTLE_ENDIAN_PLATFORM)
    {
        reverse((unsigned char *)&height, 4);
        reverse((unsigned char *)&width, 4);
    }
    
    // Validate
    if(barrier != 0xFF)
        return (edgerr = "Invalid barrier."), EDG_HEADER_INVALID_BARRIER;
    if(xorsum_check[0] != xorsum[0] or xorsum_check[1] != xorsum[1])
        return (edgerr = "Invalid xorsrum."), EDG_HEADER_INVALID_XORSUM;
    
    info->height = height;
    info->width = width;
    
    info->format    = format;
    info->grayscale = grayscale;
    info->alpha     = alpha;
    info->endian    = endian;
    
    info->tileup    = tileup;
    info->tiledown  = tiledown;
    info->tileleft  = tileleft;
    info->tileright = tileright;
    
    return EDG_HEADER_SUCCESS;
}

inline unsigned pixel_length(bool grayscale, bool alpha, bool format)
{
    return ((grayscale?1:3)+(alpha?1:0))*(format?4:1);
}

/*
1) read header
2) determine byte length of image data
3) check if file is truncated
4) truncate copy size appropriately
5) check if buffer size surpasses size_t capacity
6) allocate buffer
7) copy existing image data to buffer
8) fill in truncated image area if needed
*/

edg * edg_open(const char * fname)
{
    if(!fname) return (edgerr = "Filename is null"), nullptr;
    std::ifstream file;
    file.open(fname, file.binary|file.in);
    if(!file) return (edgerr = "Failed to open file."), nullptr;
    
    edg * edge = new (std::nothrow) edg;
    if(!edge) return (edgerr = "Failed to allocate edg metadata structure."), nullptr;
    
    // Check file length
    
    // "The type std::streamoff is a signed integral type of sufficient size to represent the maximum possible file size supported by the operating system."
    std::streamoff length;
    // iostream exceptions are disabled by default as of C++17 and earlier
    file.seekg(0, file.end);
    length = -1; // streamoff is SIGNED.
    length = file.tellg(); // streampos (arbitrary) converts to streamoff (integer type)
    if(!file) return (edgerr = "Failed while determining file length."), nullptr;
    if(length < 0x10) return (edgerr = "Invalid EDG file - is not long enough to contain a header, or is so long that the length counter overflowed."), nullptr;
    
    uint64_t imagebytes = length-0x10;
    
    // Get info
    
    file.seekg(0, file.beg);
    unsigned char header[0x10];
    file.read((char *)header, 0x10);
    if(!file) return (edgerr = "Failed to read header from file."), nullptr;
    
    edginfo info;
    int rcode = edg_parse_header(header, &info);
    if(rcode < 0) return nullptr; // edgerr already set by edg_parse_header
    
    // Bytes per full pixel
    unsigned pixelsize = pixel_length(info.grayscale, info.alpha, info.format);
    uint64_t pixels_tall = uint64_t(info.height)+1;
    uint64_t pixels_wide = uint64_t(info.width)+1;
    uint64_t bytes_to_store = pixels_wide*pixels_tall*pixelsize;
    
    // note to non-C programmers: C integer division truncates when possible
    bool truncated = (pixelsize*pixels_tall > ((imagebytes)/pixels_wide));
    
    uint64_t bytes_to_read; 
    if(truncated)
    {
        // truncated: use stream information
        bytes_to_read = uint64_t(imagebytes);
        if(bytes_to_read != imagebytes) // might happen due to signed-unsigned conversion oddities or if streamoff is larger than uint64_t
            return (edgerr = "Can't load EDG file - file length somehow failed to convert to unsigned size. File may be too large to be supported, or platform is bugged."), nullptr;
        // cut off any incomplete pixel that may be at the end of the image data
        bytes_to_read = (bytes_to_read/pixelsize)*pixelsize;
    }
    else
    {
        // not truncated: use format and dimension information
        bytes_to_read = bytes_to_store;
    }
    
    // If size was limited in any way (like overflow, which might happen), this division will truncate to lower than pixelsize
    
    if(bytes_to_store/pixels_wide/pixels_tall != pixelsize)
        return (edgerr = "Can't load EDG file - image data contains too many byte values to address in 64-bit space."), nullptr;
    if(uint64_t(size_t(bytes_to_store)) != bytes_to_store)
        return (edgerr = "Can't load EDG file - image data too large to fit into size_t."), nullptr;
    
    
    if (bytes_to_read > bytes_to_store)
        return (edgerr = "Something went wrong. We need to read more bytes than we store. This is a bug. Please report it."), nullptr;
    
    // allocate buffer and defer free
    
    unsigned char * data = (unsigned char *)malloc(bytes_to_store);
    if (!data) return (edgerr = "Failed to allocate memory for buffer. If it's a large image, use a stream."), nullptr;
    
    defer data_free
    ([data](){
        free(data);
    });
    
    // copy image data into RAM
    
    file.seekg(0x10, file.beg);
    if(!file) return (edgerr = "Failed to return to beginning of file to read data into RAM."), nullptr;
    
    file.read((char *)data, bytes_to_read);
    if(!file) return (edgerr = "Failed to read image data into RAM."), nullptr;
    
    file.close();
    
    // byteswap to native if needed
    
    int vallength = info.format?4:1;
    int values = (info.grayscale?1:3)+info.alpha;
    
    if(info.endian != HAVE_LITTLE_ENDIAN_PLATFORM)
    {
        for(uint64_t i = 0; i < bytes_to_read/values; i++)
            reverse(&data[i*vallength], vallength);
    }
    
    info.endian = HAVE_LITTLE_ENDIAN_PLATFORM;
    
    // fill in truncated values
    
    if(truncated)
    {
        for(uint64_t i = bytes_to_read/vallength; i < bytes_to_store/vallength; i += 1)
        {
            if(info.format == 0) // 8-bit unsigned, vallength 1
                data[i] = 255;
            if(info.format == 1) // 32-bit float, vallength 4
                ((float*)(data))[i] = 1.0f;
        }
    }
    
    // build edg* and return it
    
    edge->info = info;
    edge->size = bytes_to_store;
    edge->data = data;
    
    // cancel defer before returning
    
    data_free.deferred = [](){};
    
    return edge;
}

/*
1) turn arguments into info struct
2) allocate edg metadata struct
3) check buffer size
4) allocate buffer
5) fill buffer with white
6) hook buffer and info into metadata struct
7) return metadata struct
*/

edg * edg_make(uint32_t height, uint32_t width, bool format, bool grayscale, bool alpha)
{
    edginfo info;
    info.height = height;
    info.width = width;
    info.format = format;
    info.grayscale = grayscale;
    info.alpha = alpha;
    info.endian = HAVE_LITTLE_ENDIAN_PLATFORM;
    info.tileup = false;
    info.tiledown = false;
    info.tileleft = false;
    info.tileright = false;
    edg * edge = new (std::nothrow) edg;
    if(!edge) return (edgerr = "Failed to allocate edg metadata structure."), nullptr;
    
    // Bytes per full pixel
    unsigned pixelsize = pixel_length(info.grayscale, info.alpha, info.format);
    uint64_t pixels_tall = uint64_t(info.height)+1;
    uint64_t pixels_wide = uint64_t(info.width)+1;
    uint64_t bytes_to_store = pixels_wide*pixels_tall*pixelsize;
    
    // If size was limited in any way (like overflow, which might happen), this division will truncate to lower than pixelsize
    
    if(bytes_to_store/pixels_wide/pixels_tall != pixelsize)
        return (edgerr = "Can't make EDG file - image data contains too many byte values to address in 64-bit space."), nullptr;
    if(uint64_t(size_t(bytes_to_store)) != bytes_to_store)
        return (edgerr = "Can't make EDG file - image data too large to fit into size_t."), nullptr;
    
    // allocate buffer and defer free
    
    unsigned char * data = (unsigned char *)malloc(bytes_to_store);
    if (!data) return (edgerr = "Failed to allocate memory for buffer. If it's a large image, use a stream."), nullptr;
    
    int vallength = info.format?4:1;
    
    // fill allocated pixels with white
    for(uint64_t i = 0; i < bytes_to_store/vallength; i += 1)
    {
        if(info.format == 0) // 8-bit unsigned, vallength 1
            data[i] = 255;
        if(info.format == 1) // 32-bit float, vallength 4
            ((float*)(data))[i] = 1.0f;
    }
    
    // build edg* and return it
    
    edge->info = info;
    edge->size = bytes_to_store;
    edge->data = data;
    
    return edge;
}

int32_t edg_save(edg * edge, const char * fname)
{
    if(!edge) return (edgerr = "EDG is null"), -1;
    if(!edge->data) return (edgerr = "EDG's data is null"), -1;
    if(!fname) return (edgerr = "Filename is null"), -1;
    
    std::ofstream file;
    file.open(fname, file.out|file.binary);
    if(!file) return (edgerr = "Failed to open file."), -2;
    
    // write header
    
    file.write((const char *)"EDG", 4);
    if(!file) return (edgerr = "Failed to write magic to file."), -3;
    
    // height/width are always native endian in memory
    if(edge->info.endian != HAVE_LITTLE_ENDIAN_PLATFORM)
    {
        reverse((unsigned char *)&edge->info.height, 4);
        reverse((unsigned char *)&edge->info.width, 4);
    }
    file.write((const char *)&edge->info.height, 4);
    file.write((const char *)&edge->info.width, 4);
    
    file.put((char)(0xFF)); // barrier
    
    unsigned char flags = 0;
    if(edge->info.format)    flags |= 0b1000'0000;
    if(edge->info.grayscale) flags |= 0b0100'0000;
    if(edge->info.alpha)     flags |= 0b0010'0000;
    if(edge->info.endian)    flags |= 0b0001'0000;
    if(edge->info.tileup)    flags |= 0b0000'1000;
    if(edge->info.tiledown)  flags |= 0b0000'0100;
    if(edge->info.tileleft)  flags |= 0b0000'0010;
    if(edge->info.tileright) flags |= 0b0000'0001;
    file.put((char)flags);
    
    unsigned char xor1 = 0;
    unsigned char xor2 = 0;
    
    xor1 ^= 'E';
    xor2 ^= 'D';
    
    xor1 ^= 'G';
    xor2 ^= 0;
    
    xor1 ^= ((char *)(&edge->info.height))[0];
    xor2 ^= ((char *)(&edge->info.height))[1];
    
    xor1 ^= ((char *)(&edge->info.height))[2];
    xor2 ^= ((char *)(&edge->info.height))[3];
    
    xor1 ^= ((char *)(&edge->info.width))[0];
    xor2 ^= ((char *)(&edge->info.width))[1];
    
    xor1 ^= ((char *)(&edge->info.width))[2];
    xor2 ^= ((char *)(&edge->info.width))[3];
    
    xor1 ^= 0xFF; // barrier
    xor2 ^= flags;
    
    file.put(xor1);
    file.put(xor2);
    
    if(!file) return (edgerr = "Failed to write header to file. File may be truncated."), -3;
    
    // write image data to file
    
    file.write((char *)edge->data, edge->size);
    
    if(!file) return (edgerr = "Failed to write image data to file. File may be truncated."), -3;
    
    file.flush();
    
    return 0;
}

int32_t edg_kill(edg * edge)
{
    if(!edge) return (edgerr = "EDG is null"), -1;
    if(!edge->data) return (edgerr = "EDG's data is null"), -1;
    free(edge->data);
    free(edge);
    
    return 0;
}
