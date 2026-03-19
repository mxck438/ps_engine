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

#include <xsi.h>
#include "../fbaccess.h"

#include "test_config.h"
#include "test_master.h"

#define DEFAULT_FIREBIRD_PORT 3050

fb_database_t *th_connect_db(char *tag, int32_t id, char *dbpath, 
    char *username, char *password, char *charset);
fb_database_t *th_connect_config_db(test_config_t *config, int db_index);
bool th_fetch_server_pid(fb_database_t *db, int *pid);
fb_database_t *th_connect_db_check_pid(test_config_t *config, int db_index);
bool th_drop_func_if_exists(fb_database_t *db, ext_func_t *func);
bool th_install_func(fb_database_t *db, ext_func_t *func,
    bool use_create_or_alter);
bool th_exec_fn_test(fb_database_t *db, ext_func_t *func, func_test_t *test);
bool th_install_pse_reload_module(fb_database_t *db);
bool th_run_pse_reload_module(fb_database_t *db);

