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
*/

#include "libedg.cpp"

#include <stdio.h>

int main()
{
    auto myedg = edg_make(255, 255, 0, 0, 0);
    if(!myedg) return printf("failed to make: %s\n", edgerr), 0;
    
    if(edg_save(myedg, "testedg.edg") != 0) return printf("failed to save: %s\n", edgerr), 0;
    
    edg_kill(myedg);
    
    if(!(myedg = edg_open("testedg.edg"))) return printf("failed to open: %s\n", edgerr), 0;
    
    puts("EDG load-save-load cycle successful.");
    
    return 0;
}
