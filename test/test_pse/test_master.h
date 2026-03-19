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

#include "test_config.h"

typedef struct test_master_ {
    bool test_failed;
    int server_pid;
    int running_count;
    string_list_t *errors;
    timespan_hs_t start_tick;
    test_config_t *config;
    alertable_thread_t reload_module_thread;
    int reload_module_count;
    timespan_hs_t first_reload_tick;
} test_master_t;

bool tm_init_test_master(void);
void tm_test_master_shutdown(void);
test_master_t *tm_get_n_lock_master(void);
void tm_release_master(test_master_t *master);
bool tm_run_thread(char *job_name, thread_proc_t thread_proc, void *param);
void tm_thread_done(void);
void tm_fail_tests(char *error);
bool tm_has_already_failed(void);
test_config_t *tm_clone_config(void);
bool tm_set_server_pid(int pid);
bool tm_check_server_pid(int pid);
int tm_estimate_thread_count(void);
void tm_create_reload_module_thread(athread_proc_t cb);
void tm_inc_reload_count(void);
void tm_run_reload_module(void);
