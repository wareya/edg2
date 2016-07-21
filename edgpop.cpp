// Upscales an image using Bilinear EDI./*

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

// do EDI on the first-order pixel expansion
#define EDI
// do EDI on the second-order pixel expansion
// The EDI function is not perfect, and this brings out some pixel sparkling glitchiness stuff.
#define EDI2
// exaggerate edge normals towards diagonalness
#define EXAGGERATE_ANGLES

// TODO: Add a mode for quantized edge detection, instead of blending

#include "libedg.cpp"

#include <math.h> // roundf

#define values(edge) ((edge->info.grayscale?1:3)+edge->info.alpha)
#define value_length(edge) (edge->info.format?4:1)
#define pixel_length(edge) (values(edge)*value_length(edge))

int max(int a, int b)
{
    return (a>b)?a:b;
}
int min(int a, int b)
{
    return (a<b)?a:b;
}

// value index, not byte index
uint64_t index(edg* edge, int64_t y, int64_t x, int channel)
{
    if(y < 0) y = 0;
    if(y > edge->info.height) y = edge->info.height;
    if(x < 0) x = 0;
    if(x > edge->info.width) x = edge->info.width;
    if(channel < 0) channel = 0;
    if(channel > values(edge)-1) channel = values(edge)-1;
    return (y*(uint64_t(edge->info.width)+1)+x)*values(edge) + channel;
}
float edg_read(edg* edge, int64_t y, int64_t x, int channel)
{
    if(edge->info.format)
        return ((float*)(edge->data))[index(edge, y, x, channel)];
    else
        return ((edge->data[index(edge, y, x, channel)])/255.0f);
}
void edg_write(edg* edge, int64_t y, int64_t x, int channel, float value)
{
    if(edge->info.format)
        ((float*)(edge->data))[index(edge, y, x, channel)] = value;
    else
        edge->data[index(edge, y, x, channel)] = min(255, max(0, roundf((value)*255.0f)));
}

struct vector {
    float x;
    float y;
};

vector normal(edg* edge, int64_t y, int64_t x, int yup, int xup, int yright, int xright, int channel)
{
    vector retval;
    retval.x = 0;
    retval.y = 0;
    float v1 = edg_read(edge, y+yup       , x+xup       , channel);
    float v2 = edg_read(edge, y+yup+yright, x+xup+xright, channel);
    float v3 = edg_read(edge, y    +yright, x    +xright, channel);
    float v4 = edg_read(edge, y-yup+yright, x-xup+xright, channel);
    float v5 = edg_read(edge, y-yup       , x-xup       , channel);
    float v6 = edg_read(edge, y-yup-yright, x-xup-xright, channel);
    float v7 = edg_read(edge, y    -yright, x    -xright, channel);
    float v8 = edg_read(edge, y+yup-yright, x+xup-xright, channel);
    
    float v9 = edg_read(edge, y, x, channel);
    
    retval.x += (v2-v6);
    retval.x += (v3-v7)*2;
    retval.x += (v4-v8);
    retval.y += (v8-v4);
    retval.y += (v1-v5)*2;
    retval.y += (v2-v6);
    
    vector rotatedval;
    rotatedval.x = fabsf(retval.x-retval.y);
    rotatedval.y = fabsf(retval.x+retval.y);
    
    return rotatedval;
}


float badedi(edg* edge, int64_t y, int64_t x, int yup, int xup, int yright, int xright, int channel)
{
    vector p1 = normal(edge, y+yup+yright, x+xup+xright, yup, xup, yright, xright, channel);
    vector p2 = normal(edge, y+yup       , x+xup       , yup, xup, yright, xright, channel);
    vector p3 = normal(edge, y           , x           , yup, xup, yright, xright, channel);
    vector p4 = normal(edge, y+yright    , x+xright    , yup, xup, yright, xright, channel);
    
    float m1 = (p1.x+p2.x+p3.x+p4.x)/4;
    float m2 = (p1.y+p2.y+p3.y+p4.y)/4;
    
    #ifdef EXAGGERATE_ANGLES
    // exaggerating the vector increases perceptual quality for some reason.
    m1 *= m1;
    m2 *= m2;
    #endif
    
    float a = edg_read(edge, y+yup+yright, x+xup+xright, channel);
    
    float scale = m1+m2;
    
    float b = edg_read(edge, y+yup       , x+xup       , channel);
    float c = edg_read(edge, y           , x           , channel);
    float d = edg_read(edge, y+yright    , x+xright    , channel);
    
    float f1 = (a+c)/2;
    float f2 = (b+d)/2;
    
    if(scale == 0)
        return (f1+f2)/2; // no slope at all
    
    return (f1*m1 + f2*m2)/scale;
}


// diagonal crosshatch
float badedi1(edg* edge, int64_t y, int64_t x, int channel)
{
    return badedi(edge, y, x, 2, 0, 0, 2, channel);
}

// axial crosshatch
float badedi2(edg* edge, int64_t y, int64_t x, int channel)
{
    return badedi(edge, y, x, 1, 1, -1, 1, channel);
}

int main(int argc, char ** argv)
{
    if(argc < 3) return puts("Usage: edg2bmp in.edg out.edg"), 0;
    
    auto edge = edg_open(argv[1]);
    if(!edge) return printf("edg_open(\"%s\") failed: %s\n", argv[1], edgerr), 0;
    auto pop = edg_make(edge->info.height*2, edge->info.width*2, edge->info.format, edge->info.grayscale, edge->info.alpha);
    if(!pop) return printf("edg_make() failed: %s\n", edgerr), 0;
    
    int values = (edge->info.grayscale?1:3)+edge->info.alpha;
    
    // source pass
    for(int64_t y = 0; y <= pop->info.height; y++)
        for(int64_t x = 0; x <= pop->info.width; x++)
            for(int i = 0; i < values; i++)
                if(!(x&1) and !(y&1))
                    edg_write(pop, y, x, i, edg_read(edge, y/2, x/2, i));
    // cross hatch pass
    for(int64_t y = 0; y <= pop->info.height; y++)
    {
        for(int64_t x = 0; x <= pop->info.width; x++)
        {
            for(int i = 0; i < values; i++)
            {
                if((x&1) and (y&1))
                {
                    #ifdef EDI
                    edg_write(pop, y, x, i, badedi1(pop, y-1, x-1, i));
                    #else // EDI
                    edg_write(pop, y, x, i,
                        (edg_read(pop, y-1, x-1, i)+
                         edg_read(pop, y+1, x-1, i)+
                         edg_read(pop, y+1, x+1, i)+
                         edg_read(pop, y-1, x+1, i)
                        )/4);
                    #endif // EDI else
                }
                if((x&1) and !(y&1))
                    edg_write(pop, y, x, i, (edg_read(pop, y, x+1, i)+edg_read(pop, y, x-1, i))/2);
                #ifndef EDI2
                if(!(x&1) and (y&1))
                    edg_write(pop, y, x, i, (edg_read(pop, y-1, x, i)+edg_read(pop, y+1, x, i))/2);
                #endif
            }
        }
    }
    #ifdef EDI2
    // axial pass
    for(int64_t y = 0; y <= pop->info.height; y++)
        for(int64_t x = 0; x <= pop->info.width; x++)
            for(int i = 0; i < values; i++)
                if((x&1) xor (y&1))
                    edg_write(pop, y, x, i, badedi2(pop, y, x-1, i));
    #endif
    // save
    edg_save(pop, argv[2]);
}
