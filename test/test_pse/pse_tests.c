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

#include "pse_tests.h"
#include "test_helpers.h"
#include "test_master.h"

DEFINE_ALERTABLE_THREADPROC(module_reload_thread)
{
    test_config_t *config = tm_clone_config();

	while (true)
	{
		uint32_t wait_result = 
            evn_wait_for_event(tcb->alert_event, INFINITE);
		if ((WAIT_OBJECT_0 != wait_result) 
            || atm_flag_get(tcb->request_terminate))
			goto THREAD_EXIT;

        if (config->databases->count < 1) continue;
        // throw a dice
        int db_idx = rand() % config->databases->count;
        fb_database_t *db = th_connect_db_check_pid(config, db_idx);
        if (!db) {
            tm_fail_tests("connect db FAILED.");
            continue;
        }
        if (!th_install_pse_reload_module(db))
            tm_fail_tests("th_install_pse_reload_module() FAILED.");
        else if (!th_run_pse_reload_module(db))
                tm_fail_tests("th_run_pse_reload_module() FAILED.");
        else tm_inc_reload_count();

        fb_free_database(db);

    }
THREAD_EXIT:
    tc_free_test_config(config);
    at_athread_done(tcb);  
}

static bool exec_fn_test(test_config_t *config, int db_idx, ext_func_t *func,
    func_test_t *test)
{
    if ((rand() % 5) == 1) tm_run_reload_module();

    fb_database_t *db = th_connect_db_check_pid(config, db_idx);
    if (!db) return false;

    bool result = th_exec_fn_test(db, func, test);

    fb_free_database(db);
    return result;
}

DEFINE_THREADPROC(fn_test_thread)
{
    int db_idx = (intptr_t)thread_arg;
    test_config_t *config = tm_clone_config();
    test_db_t *conf_db = lst_get_item(config->databases, db_idx);

    for (int i = 0; i < conf_db->test_repeat_count; i++) {
        for (int j = 0; j < conf_db->funcs->count; j ++) {
            ext_func_t *func = lst_get_item(conf_db->funcs, j);
            for (int k = 0; k < func->tests->count; k++) {
                if (tm_has_already_failed()) goto DONE;
                func_test_t *test = lst_get_item(func->tests, k);
                if (!exec_fn_test(config, db_idx, func, test)) {
                    tm_fail_tests("function test FAILED.");
                    goto DONE;
                }
            }
        }
    }
DONE:
    tc_free_test_config(config);
    tm_thread_done();
}

DEFINE_THREADPROC(db_test_thread)
{
    int db_idx = (intptr_t)thread_arg;
    test_config_t *config = tm_clone_config();
    fb_database_t *db = th_connect_db_check_pid(config, db_idx);

    if (!db) {
        tm_fail_tests("connect db FAILED.");
        goto DONE;
    }

    test_db_t *conf_db = lst_get_item(config->databases, db_idx);

    if (config->drop_funcs_before_use) {
        for (int i = 0; i < conf_db->funcs->count; i++) {
            ext_func_t *func = lst_get_item(conf_db->funcs, i);
            if (!th_drop_func_if_exists(db, func)) {
                tm_fail_tests("th_drop_func_if_exists() FAILED.");
                goto DONE;
            }
        }
    }

    for (int i = 0; i < conf_db->funcs->count; i++) {
        ext_func_t *func = lst_get_item(conf_db->funcs, i);
        if (!th_install_func(db, func, config->use_create_or_alter)) {
            tm_fail_tests("th_install_func() FAILED.");
            goto DONE;
        }
    }    

    if (tm_has_already_failed()) goto DONE;

    for (int i = 0; i < conf_db->test_threads; i++) {
        tm_run_thread("fn_test_thread", fn_test_thread, thread_arg);
    }

DONE: 
    if (db) fb_free_database(db);
    tc_free_test_config(config);
    tm_thread_done();
}

DEFINE_THREADPROC(main_test_thread)
{
    test_config_t *config = tm_clone_config();

    // find out server pid, keep it    
    int server_pid = 0;
    fb_database_t *db = th_connect_config_db(config, 0);
    bool succeeded = (db != NULL) 
        && th_fetch_server_pid(db, &server_pid)
        && tm_set_server_pid(server_pid);
    if (db) fb_free_database(db);
    if (!succeeded) {
        tm_fail_tests("fetch server pid FAILED.");
        goto DONE;
    }
    else log_success(0, "fetched server pid: %d", server_pid);

    for (int i = 0; i < config->databases->count; i++) {
        tm_run_thread("db_test_thread", db_test_thread, (void*)(intptr_t)i);
    }

DONE:    
    tc_free_test_config(config);
    tm_thread_done();
}

#define MAX_TEST_THREADS 30
#define MIN_TEST_THREADS 10

bool perform_pse_tests(void) {
    srand(time(NULL));
    thrp_initialize_threadpool(MIN_TEST_THREADS);    

    if (!tm_init_test_master()) {
        log_error("tm_init_test_master() FAILED.");
        xapp_set_exit_message("\033[1;31mtests FAILED\033[0m");
        return false;
    }

    test_config_t *config = tm_clone_config();
    fb_initialize(config->libfbclient, false);
    tc_free_test_config(config);

    int threads = tm_estimate_thread_count() + 6;
    if (threads > MAX_TEST_THREADS) {
        log_error("too many test threads in config.");
        xapp_set_exit_message("\033[1;31mtests FAILED\033[0m");
        return false;
    }

    if (threads > MIN_TEST_THREADS)
        thrp_ensure_threads_count(threads);

    tm_create_reload_module_thread(module_reload_thread);

    tm_run_thread("main_test_thread", main_test_thread, NULL);
    
    return true;
}

bool pse_tests_finished(void) {
    tm_test_master_shutdown();

    thrp_shutdown_threadpool();
    fb_finalize();
    return true;
}