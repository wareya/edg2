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

#include <math.h> // roundf

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include "libedg.cpp"

#define values(edge) ((edge->info.grayscale?1:3)+edge->info.alpha)
#define value_length(edge) (edge->info.format?4:1)
#define pixel_length(edge) (values(edge)*value_length(edge))

int main(int argc, char ** argv)
{
    if(argc < 3) return puts("Usage: edg2bmp in.edg out.bmp"), 0;
    
    auto edge = edg_open(argv[1]);
    if(!edge) return printf("edg_open(\"%s\") failed: %s\n", argv[1], edgerr), 0;
    
    auto buffer = edge->data;
    if(edge->info.format)
    {
        buffer = (unsigned char *)malloc(edge->size/4);
        for(uint64_t i = 0; i < edge->size; i += 4)
            buffer[i/4] = roundf(255*fmin(1.0f, fmax(0.0f, linear2srgb(((float*)(edge->data))[i]))));
    }
    
    if(!stbi_write_bmp(argv[2], uint64_t(edge->info.width)+1, uint64_t(edge->info.height)+1, values(edge), buffer));
    
    if(buffer != edge->data) free(buffer);
    edg_kill(edge);
    
    return 0;
}
