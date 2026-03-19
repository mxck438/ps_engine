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

#pragma once

#include "firebird/Interface.h"

#include "meta_cache.h"

typedef void (*pse_exec_proc_t)(::Firebird::ThrowStatusWrapper *status, 
            ::Firebird::IExternalContext *context, 
            PSERoutineMeta *meta,
            unsigned char *in, 
            unsigned char *out);

typedef struct pse_proc_entry_ {
    const char *name;
    pse_exec_proc_t proc;
} pse_proc_entry_t;

#define PSE_MODULE_ENTRY pse_module

typedef int (*pse_mod_entry)(bool init, const pse_proc_entry_t **table, 
    const char **version);

typedef enum pse_spec_fn_ {
    PSE_SPECFN_NONE = 0,
    PSE_SPECFN_RELOAD_MODULE = 1,
    PSE_SPECFN_MODULE_INFO = 2
} pse_spec_fn_t;

#define PSE_SPECFN_RELOAD_MODULE_NAME "pse_reload_module"
#define PSE_SPECFN_MODULE_INFO_NAME "pse_get_module_info"  

#define PSE_MODULE_FILENAME "libps_module.so"
