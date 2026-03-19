/*
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2026 Maxim Kryukov
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy 
 *  of this software and associated documentation files (the “Software”), 
 *  to deal in the Software without restriction, including without limitation 
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 *  and/or sell copies of the Software, and to permit persons to whom the 
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included 
 *  in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, 
 *  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 *  DEALINGS IN THE SOFTWARE.
 * 
 */  

#include <stdio.h>
#include <dlfcn.h>

#include <xsi.h>



int main(int argc, char **argv) {

    printf("args:\n"); 
    for (int i = 0; i < argc; i++) {
        printf("%d: \"%s\"\n", i, argv[i]);
    }
    printf("module to load: \"%s\"\n", argv[1]);

    char *fn = fl_compose_filename_in_this_module_directory(argv[1]);


    void *handle = dlopen(fn, RTLD_NOW);
    if (!handle) printf("Load FAILED: \"%s\"\n", dlerror());
    else {
       printf("Load OK.\n");
       dlclose(handle);  
    }
    free(fn);
    return 0;
}