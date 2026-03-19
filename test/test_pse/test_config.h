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

typedef struct func_test_ {
    char *sql;
    char *result;
} func_test_t;

typedef struct ext_func_ {
    char *name;
    char *param_list;
    char *returns;
    char *ext_name;
    char *engine;
    list_t *tests;
} ext_func_t;

typedef struct test_db_ {
    char *dbpath;
    list_t *funcs;
    int test_repeat_count;
    int test_threads;
} test_db_t;

typedef struct test_config_ {
    char *libfbclient;
    char *server;
    uint16_t port;
    char *user;
    char *password;
    list_t *databases;
    list_t *funcs;
    bool drop_funcs_before_use;
    bool use_create_or_alter;
    bool run_module_reloder;
} test_config_t;


test_config_t *tc_load_test_config(void);
void tc_free_test_config(test_config_t *config);
test_config_t *tc_clone(test_config_t *config);
int tc_estimate_thread_count(test_config_t *config);
bool tc_compare_configs(test_config_t *config1, test_config_t *config2);
