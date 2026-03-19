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

#include <dlfcn.h>
#include <map>
#include <string>
#include <sys/mman.h>

#define STRINGIZE_AUX(x)	#x
#define STRINGIZE(x)		STRINGIZE_AUX(x)

class PSModule
{
private:
    void *handle = NULL;     
    pse_mod_entry pmod = NULL;
    int memfd = -1;

public:  
    std::string version;
    std::map<std::string, pse_exec_proc_t> execPocs;
    timestamp_t  file_time;
    timestamp_t loaded_time;


    bool dlmemopen(const char *filename) {
        char path[512];
        size_t sz = 0;
        char *data = fl_load_from_file((char *)filename, &sz);
        if (!data) return false;
        memfd = memfd_create(filename, MFD_CLOEXEC);
        if (memfd < 0) {
            free(data);
            return false; 
        }
        if (write(memfd, data, sz) < 0) {
            close(memfd);
            memfd = -1;
            free(data);
            return false;
        }
        free(data);

        fcntl(memfd, F_ADD_SEALS, F_SEAL_SEAL | F_SEAL_WRITE);
        lseek(memfd, 0, SEEK_SET);
        snprintf(path, sizeof(path), "/proc/self/fd/%d", memfd);

        handle = dlopen(path, RTLD_NOW);
        if (!handle) {
            close(memfd);
            memfd = -1;
            return false;
        }

        pmod = (pse_mod_entry)dlsym(handle, 
                STRINGIZE(PSE_MODULE_ENTRY));
        if (!pmod) {
            dlclose(handle);
            close(memfd);
            memfd = -1;
            return false;
        };

        const pse_proc_entry_t *table = NULL;
        const char *ver = NULL;
        int count = pmod(true, &table, &ver);
        if (ver) version = ver;
        for (int i = 0; i < count; i++) {
            execPocs[table->name] = table->proc;
            table++;
        }
        fl_get_file_modified_time_ts((char *)filename, &file_time);
        tm_now(&loaded_time);
        log_debug(0, "loaded module version \"%s\"", version.c_str());
        return true;
    }

/*
    Various methods may be proposed to reload a module from the same filename, 
        each with distinct advantages and disadvantages.
    See ps_engine.cpp for a discussion.
*/

/*
    bool plain_dlopen(const char *filename) {
        handle = dlopen(filename, RTLD_NOW);
        if (!handle) return false;

        pmod = (pse_mod_entry)dlsym(handle, 
                STRINGIZE(PSE_MODULE_ENTRY));
        if (!pmod) {
            dlclose(handle);
            return false;
        };

        const pse_proc_entry_t *table = NULL;
        const char *ver = NULL;
        int count = pmod(true, &table, &ver);
        if (ver) version = ver;
        for (int i = 0; i < count; i++) {
            execPocs[table->name] = table->proc;
            table++;
        }
        fl_get_file_modified_time_ts((char *)filename, &file_time);
        tm_now(&loaded_time);
        log_debug(0, "loaded module version \"%s\"", version.c_str());
        return true;        
    }
*/
    bool load(const char *filename)
    {
        return dlmemopen(filename);
        //return plain_dlopen(filename);
    }

    ~PSModule() {
        if (handle) {            
            if (pmod) pmod(false, NULL, NULL);
            dlclose(handle);
            if (memfd > -1) close(memfd);
            log_debug(0, "unloaded module version \"%s\"", version.c_str());
        }
    }
       
};