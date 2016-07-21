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

#define STBI_ONLY_BMP
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb/stb_image.h"
#include "libedg.cpp"

int main(int argc, char ** argv)
{
    if(argc < 3) return puts("Usage: bmp2edg in.bmp out.edg"), 0;
    
    int x, y, n;
    auto data = stbi_load(argv[1], &x, &y, &n, 0);
    if(!data) return printf("stbi_load with file \"%s\" failed: %s\n", argv[1], stbi_failure_reason()), 0;
    
    auto edge = edg_make(y-1, x-1, 0, n<=2, !(n&1));
    if(!edge) return printf("edg_make failed: %s\n", edgerr), 0;
    
    auto olddata = edge->data;
    edge->data = data;
    if(edg_save(edge, argv[2])) return printf("edg_save with file %s failed: %s\n", argv[2], edgerr), 0;
    edge->data = olddata;
    
    edg_kill(edge);
    stbi_image_free(data);
    
    return 0;
}
