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

#include "ps_engine.h"
#include "pse_function.h"


static void return_result_text(int filed_idx, PSERoutineMeta *meta, 
    unsigned char *out, char *text)
{
    unsigned char *p = out + meta->out_meta.getOffset(filed_idx);
    size_t buflen = meta->out_meta.getLength(filed_idx);
    switch (meta->out_meta.getType(filed_idx)) {
        case SQL_TEXT:
        {                
            memset(p, 0x20, buflen);
            strncpy((char *)p, text, strlen(text));
            break;
        }
        case SQL_VARYING:
        {
            short len = (short)strlen(text);
            *((short*)p) = len;
            p += 2;
            strncpy_s((char *)p, buflen, text, len);
            break;
        }
        default: 
        {
            unsigned outNullOffset = meta->out_meta.getNullOffset(filed_idx);
            *(ISC_SHORT *)(out + outNullOffset) = '\1';
            break;
        }
    }
}  

PSEFuncImpl* PSEngine::getOrAddFunc(const char *entry, 
    IRoutineMetadata* metadata, ThrowStatusWrapper* status)
{
    std::lock_guard<std::mutex> guard(engine_mutex); 
            
    if (funcs.count(entry))
        return funcs.at(entry);

    PSEFuncImpl* result = NULL;
    pse_spec_fn_t spec_t = specFuncFromName(entry);
    if (spec_t != PSE_SPECFN_NONE) {
        result = new PSEFuncImpl(entry, metadata, NULL, status, this, spec_t, 
            module);
    }
    else {
        PSModule *mod = module.get();
        if (mod && mod->execPocs.count(entry)) {
            result = new PSEFuncImpl(entry, metadata, mod->execPocs.at(entry), 
                            status, this, PSE_SPECFN_NONE, module);
        }
    }

    if (result) funcs[entry] = result;
    return result;
}

bool PSEngine::reloadModule(char **return_mod_info)
{
    PSModule *newmod = new PSModule();
    char fn[512];
    snprintf(fn, sizeof(fn), "%s/" PSE_MODULE_FILENAME, pluginsDir.c_str());
  //  log_debug(0, "loading module: \"%s\"", fn);
    bool result = newmod->load(fn);
    if (!result) delete newmod;
    else {
        std::lock_guard<std::mutex> guard(engine_mutex); 
        module.reset(newmod);
        module_reload_count++;
        if (return_mod_info) *return_mod_info = getModuleInfo(newmod);
    }
    return result;
}

/*
    Various methods may be proposed to reload a module from the same filename, 
    each with distinct advantages and disadvantages.

    1. Load into an in-memory fd, and dlopen() from there (current method).
        pros: likely the smoothest and fastest approach.
        cons: 1) requires careful user module programming to allow for multiple
            copies executing concurrently; paying attention to shared 
            resources - files, thread variables, etc.
            2) the GDB may get sour not finding a module file on post-mortem 
            debugging (if your server does crash).
    2. Plain dlopen() each time. This method involves entering a critical 
        section to unload the old module, waiting for it to fully unload, 
        and then calling dlopen() for the new version. (A naive implementation 
        is provided below.)
        pros: avoids the issues found in method 1.
        cons: potentially long switching times that block the entire engine.
    3. Use dlmopen() to load into new linker namespace.
        cons: dlmopen() is a mine field. The module gets its own copy of glibc,
        including malloc() and free(). And something goes terribly wrong in
        interfacing with the Firebird core. I had no luck stabilizing this
        method. All I got were segfaults in nowere (corrupted stack) at 
        unpredictable times (after an hour of active multithreaded testing).
        And the syslog blaming different random module each time.
    4. Rename/copy the module to temp location and load from there. The method 
        has a mixture of pros and cons of the above methods.
*/


/*
bool PSEngine::reloadModule(char **return_mod_info)
{
    std::lock_guard<std::mutex> guard(engine_mutex);    

    module.reset();

    char fn[512];
    snprintf(fn, sizeof(fn), "%s/" PSE_MODULE_FILENAME, pluginsDir.c_str());

    int iter_count = 0;
    while (true) {
        void *h = dlopen(fn, RTLD_NOLOAD);
        if (!h) break;
        dlclose(h);
        if (++iter_count > 200) return false;
    }

    PSModule *newmod = new PSModule();

    bool result = newmod->load(fn);
    if (!result) delete newmod;
    else {
         
        module.reset(newmod);
        module_reload_count++;
        if (return_mod_info) *return_mod_info = getModuleInfo(newmod);
    }
    return result;
}*/

void PSEngine::execSpecReloadModule(ThrowStatusWrapper *status, 
                IExternalContext *context, 
                PSERoutineMeta *meta,
                unsigned char *in, 
                unsigned char *out)
{
    char *text = NULL;
    bool succeeded = reloadModule(&text);
    if (!succeeded) text = str_strdup((char *)"reload module FAILED.");
    unsigned outNullOffset = meta->out_meta.getNullOffset(0);
    *(ISC_SHORT *)(out + outNullOffset) = succeeded ? '\0' : '\1';
    if (succeeded) {
        return_result_text(0, meta, out, text);
    }
    free(text);
}

char * print_human_time_elapsed(timestamp_t *ts) {
    int64_t msAgo = tm_milliseconds_since(ts);

    long long seconds = msAgo / 1000;
    
    long long d = seconds / 86400;
    int h = (seconds % 86400) / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;

    if (d > 0) {
        return str_snprintf("%lldd %dh %dm %ds", d, h, m, s);
    } else if (h > 0) {
        return str_snprintf("%dh %dm %ds", h, m, s);
    } else if (m > 0) {
        return str_snprintf("%dm %ds", m, s);
    } else {
        return str_snprintf("%ds", s);
    }    
}

char *PSEngine::getModuleInfo(PSModule *mod) {
    char *loaded_str = tm_print_local_timestamp_alloc(&mod->loaded_time);
    char *elapsed_str = print_human_time_elapsed(&mod->loaded_time);
    char *file_time_str = tm_print_local_timestamp_alloc(&mod->file_time);

    char *result = str_snprintf("module: \"%s\"; version: \"%s\"; " \
        "loaded: %s (%s ago); file time: %s; engine module reload count: %d",
        PSE_MODULE_FILENAME, 
        mod->version,
        loaded_str,
        elapsed_str,
        file_time_str,
        module_reload_count);

    free(loaded_str);
    free(elapsed_str);
    free(file_time_str);

    return result;
}

bool PSEngine::doGetModuleInfo(char **text)
{
    std::lock_guard<std::mutex> guard(engine_mutex); 

    PSModule *mod = module.get();
    if (!mod) return false;
    
    *text = getModuleInfo(mod);
    return true;
}

void PSEngine::execSpecGetModuleInfo(ThrowStatusWrapper *status, 
                IExternalContext *context, 
                PSERoutineMeta *meta,
                unsigned char *in, 
                unsigned char *out)
{
    char *text = NULL;
    bool succeeded = doGetModuleInfo(&text);
    unsigned outNullOffset = meta->out_meta.getNullOffset(0);
    *(ISC_SHORT *)(out + outNullOffset) = succeeded ? '\0' : '\1';
    if (succeeded) {
        return_result_text(0, meta, out, text);
    }
    free(text);
}   

bool PSEngine::rebindFunction(PSEFuncImpl* fn)
{
    std::lock_guard<std::mutex> guard(engine_mutex);

    PSModule *mod = module.get();
    bool result = (mod && mod->execPocs.count(fn->name));
    if (result) {
        std::lock_guard<std::mutex> guard(fn->func_mutex);
        fn->exec_proc = mod->execPocs.at(fn->name);
        fn->weak_module_ptr = module;
    }

    return result;
}