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

#include "pse_xsi.h"
#include "pse_factory.h"

#define FB_EXPORTED

extern "C" void FB_EXPORTED FB_PLUGIN_ENTRY_POINT(Firebird::IMaster* master)
{
    static XSIEnvironment xsi_environment(master->getConfigManager()->
        getDirectory(IConfigManager::DIR_LOG));     

    log_debug(0, "ps_engine entry point");

    static PSEFactory factory;

    factory.plugin.initialize(master->getConfigManager()->
        getDirectory(IConfigManager::DIR_PLUGINS));

    master->getPluginManager()->registerPluginFactory(
        IPluginManager::TYPE_EXTERNAL_ENGINE, 
        "PSE", 
        reinterpret_cast<IPluginFactory*>(&factory));

    log_debug(0, "ps_engine entry point leaves");
}
