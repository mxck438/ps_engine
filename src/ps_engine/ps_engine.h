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

#include <memory>
#include <map>
#include <string>
#include <mutex>

#include "fb_classes.h"
#include "pse_function.h"
#include "pse_module.h"

class PSEFuncImpl;

class PSEngine : public IFBInterfacedClass
{
private:
    class ExternEngineVT : public FBVtable
    {
    public:
        DECLARE_VT_VOID_STUB(addRef, PSEngine*);
        DECLARE_VT_HANDLER(int, release, PSEngine*) { return 1; };
        DECLARE_VT_VOID_STUB(setOwner, PSEngine*, IReferenceCounted*);
        DECLARE_VT_HANDLER(IReferenceCounted*, getOwner, PSEngine*)
            { return NULL; };
        DECLARE_VT_VOID_STUB(open, PSEngine*, IStatus*, IExternalContext*, 
            char*, unsigned);
        DECLARE_VT_VOID_STUB(openAttachment, PSEngine*, IStatus*, 
            IExternalContext*);
        DECLARE_VT_VOID_STUB(closeAttachment, PSEngine*, IStatus*, 
            IExternalContext*);
        DECLARE_VT_HANDLER(IExternalFunction*, makeFunction, PSEngine* self, 
            IStatus* status, IExternalContext* context, 
            IRoutineMetadata* metadata, 
            IMetadataBuilder* inBuilder, 
            IMetadataBuilder* outBuilder)
            {   
                ThrowStatusWrapper st(status);
                try
                {
                    return self->makeFunction(&st, context, metadata, 
                        inBuilder, outBuilder);
                }
                catch(...)
                {
                    ThrowStatusWrapper::catchException(&st);
                    return static_cast<IExternalFunction*>(0);
                }  
            };
        DECLARE_VT_HANDLER(IExternalProcedure*, makeProcedure, 
            PSEngine*, IStatus*, 
            IExternalContext*, IRoutineMetadata*, 
            IMetadataBuilder*, IMetadataBuilder*)
            { return NULL; };
        DECLARE_VT_HANDLER(IExternalTrigger*, makeTrigger, PSEngine*, IStatus*, 
            IExternalContext*, IRoutineMetadata*, 
            IMetadataBuilder*)
            { return NULL; };
    };
   
    ExternEngineVT *pVT = new ExternEngineVT();
    std::string pluginsDir;
public:  
    ~PSEngine() {
        log_debug(0, "PSEngine dtor");
        delete pVT;
    }    

    void initialize(const char *dir) {
        pluginsDir = dir;
        reloadModule(NULL);
        reloadModule(NULL);  // in-place test 
    }

    IExternalFunction* makeFunction(ThrowStatusWrapper* status, 
        IExternalContext* context,
		IRoutineMetadata* metadata, IMetadataBuilder* inBuilder, 
        IMetadataBuilder* outBuilder)
    {
       // log_debug(0, "PSE engine %s()", __func__);

        const char *entry = metadata->getEntryPoint(status);

        PSEFuncImpl* func = getOrAddFunc(entry, metadata, status);
        if (!func) raise(status, "Invalid entry point");
        return reinterpret_cast<IExternalFunction*>(func);
    }

    bool rebindFunction(PSEFuncImpl* fn);

private:
    std::mutex engine_mutex;
    std::map<std::string, PSEFuncImpl*> funcs;
    std::shared_ptr<PSModule> module = nullptr;
    int module_reload_count = 0;

    void raise(ThrowStatusWrapper* status, const char *msg) {
        static const ISC_STATUS statusVector[] = {
            isc_arg_gds, isc_random,
            isc_arg_string, (ISC_STATUS) msg,        
            isc_arg_end
        };

        throw FbException(status, statusVector); 
    }

    pse_spec_fn_t specFuncFromName(const char *entry) {
        if (!strcasecmp(entry, PSE_SPECFN_RELOAD_MODULE_NAME)) 
            return PSE_SPECFN_RELOAD_MODULE;
        else if (!strcasecmp(entry, PSE_SPECFN_MODULE_INFO_NAME)) 
            return PSE_SPECFN_MODULE_INFO;
        else return PSE_SPECFN_NONE;
    }

    PSEFuncImpl* getOrAddFunc(const char *entry, IRoutineMetadata* metadata,
        ThrowStatusWrapper* status);

    char *getModuleInfo(PSModule *mod);
    bool doGetModuleInfo(char **text);
public:
    bool reloadModule(char **return_mod_info);
    void execSpecReloadModule(ThrowStatusWrapper *status, 
                    IExternalContext *context, 
                    PSERoutineMeta *meta,
                    unsigned char *in, 
                    unsigned char *out);
    void execSpecGetModuleInfo(ThrowStatusWrapper *status, 
                    IExternalContext *context, 
                    PSERoutineMeta *meta,
                    unsigned char *in, 
                    unsigned char *out);
};


