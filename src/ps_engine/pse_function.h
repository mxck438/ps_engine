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

#include "ps_engine_iface.h"
#include "meta_cache.h"
#include "fb_classes.h"
#include "ps_engine.h"

class PSEngine;
class PSModule;

class PSEFuncImpl : public IFBInterfacedClass
{
private:
    class PSEFuncVT : public FBVtable
    {
    public:
        DECLARE_VT_VOID_STUB(dispose, PSEFuncImpl*);
        DECLARE_VT_HANDLER(void, getCharSet, PSEFuncImpl* self, 
            IStatus* status, 
            IExternalContext* context, char* name, 
            unsigned nameSize)
            {
                memset(name, 0, nameSize);
                strcpy(name, "UTF8");
            };
        DECLARE_VT_HANDLER(void, execute, PSEFuncImpl* self, 
            IStatus* status, 
            IExternalContext* context, void* inMsg, 
            void* outMsg)
            {
                ThrowStatusWrapper st(status);
                self->execute(&st, context, inMsg, outMsg);
            };
    } *pVT = new PSEFuncVT();
 
    PSERoutineMeta meta;      
    PSEngine *engine;
    pse_spec_fn_t spec_type;  

public:       
    std::string name;
    std::mutex func_mutex;
    pse_exec_proc_t exec_proc; 
    std::weak_ptr<PSModule> weak_module_ptr;

    explicit PSEFuncImpl(const char *name,
        IRoutineMetadata *const metadata__,
        pse_exec_proc_t exec_proc, ThrowStatusWrapper *status,
        PSEngine *engine, pse_spec_fn_t spec_type,
        std::shared_ptr<PSModule> &module) 
        :
        name(name),
        meta(metadata__, status), 
        exec_proc(exec_proc),
        engine(engine),
        spec_type(spec_type),
        weak_module_ptr(module)
    {}

    std::shared_ptr<PSModule> lock_module(pse_exec_proc_t *p);

    void execute(::Firebird::ThrowStatusWrapper *status, 
        ::Firebird::IExternalContext *context, 
        void *in, void *out); 
};

