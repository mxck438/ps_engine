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

#include "ps_engine_iface.h"

#include "my_sum_args.h"

#define PSMOD_VERSION "1.08"
#define EXEC_PROC_COUNT 2

const pse_proc_entry_t procs[EXEC_PROC_COUNT] =
    {
        { "my_sum_args", exec_my_sum_args },
        { "my_sum_args2", exec_my_sum_args2 },
    };

#define FB_EXPORTED

extern "C" int FB_EXPORTED PSE_MODULE_ENTRY(bool init, 
    const pse_proc_entry_t **table, 
    const char **version)
{
    if (table) *table = procs;
    if (version) *version = PSMOD_VERSION;
    return EXEC_PROC_COUNT;
} 