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

#include "pse_function.h"

std::shared_ptr<PSModule> PSEFuncImpl::lock_module(pse_exec_proc_t *p)
{
    std::lock_guard<std::mutex> guard(func_mutex);
    *p = exec_proc;
    return weak_module_ptr.lock();
}

void PSEFuncImpl::execute(::Firebird::ThrowStatusWrapper *status, 
    ::Firebird::IExternalContext *context, 
    void *in, void *out) 
{ 
    log_debug(0, "executing func %s()", name.c_str());
    switch (spec_type)
    {
        case PSE_SPECFN_RELOAD_MODULE: {
            engine->execSpecReloadModule(status, context, &meta,
                (unsigned char *)in, 
                (unsigned char *)out);
            break;
        }
        
        case PSE_SPECFN_MODULE_INFO: {
            engine->execSpecGetModuleInfo(status, context, &meta,
                (unsigned char *)in, 
                (unsigned char *)out);
            break;
        }
        
        default: {
            pse_exec_proc_t eproc = NULL;
            while (true) {
                if (auto temp_shared_ptr = lock_module(&eproc)) {
                    eproc(status, context, &meta,
                        (unsigned char *)in, 
                        (unsigned char *)out);
                    break;
                }
                else {
                    if (!engine->rebindFunction(this))
                        throw "PSE: could not rebind function";
                    else log_debug(0, "rebinded func \"%s\"", name.c_str());
                }
            }
            
            break;
        }
    }        
}