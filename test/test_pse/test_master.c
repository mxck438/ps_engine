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

#include "test_master.h"

#define TEST_MASTER_NAME "PSE_TM"



static void tm_dtor(test_master_t *master) {
    sl_free(master->errors);
    if (master->config) tc_free_test_config(master->config);
}

bool tm_init_test_master(void)
{
    test_master_t *master = (test_master_t*)obj_alloc(sizeof(*master), 
        (obj_destructor_t)tm_dtor,
        TEST_MASTER_NAME);
    bool result = master != NULL;   
    if (result) {
        master->errors = sl_create_stringlist();
        tmhs_tick(&master->start_tick);
        master->config = tc_load_test_config();
        result = (master->config != NULL);
        if (result) {
            log_debug(0, "loaded test config. databases: %d; funcs: %d",
                master->config->databases->count,
                master->config->funcs->count);
            result = obj_put_global_named_obj(master, TEST_MASTER_NAME);
        }

        if (!result) OBJ_RELEASE(master);
    } 
    return result;
}

void tm_test_master_shutdown(void) {
    test_master_t *master = 
        (test_master_t*)obj_extract_global_named_obj(TEST_MASTER_NAME);
    if (master) {
        OBJ_RELEASE(master);
        log_debug(0, "test master finalized.");
    }
    else log_warning(0, "%s() FAILED", __func__);    
}

test_master_t *tm_get_n_lock_master(void) {
    return obj_get_and_lock_global_named_obj(TEST_MASTER_NAME);
}

void tm_release_master(test_master_t *master) {
    obj_unlock(master);
    OBJ_RELEASE(master);
}

static void do_fail_tests(test_master_t *master, char *error) {
    master->test_failed = true;
    if (!EMPTY_STRING(error))
        sl_add(master->errors, error);
}

void tm_fail_tests(char *error) {
    test_master_t *master = tm_get_n_lock_master();
    if (master) {
        do_fail_tests(master, error);
        tm_release_master(master);
    }
}

bool tm_has_already_failed(void) {
    test_master_t *master = tm_get_n_lock_master();
    if (master) {
        bool result = master->test_failed;
        tm_release_master(master);
        return result;
    }
    else return true;    
}

bool tm_run_thread(char *job_name, thread_proc_t thread_proc, void *param)
{
    test_master_t *master = tm_get_n_lock_master();
    bool result = (master != NULL);
    if (result) {
        result = !master->test_failed;
        if (result) {
            master->running_count++;
            result = thrp_run(job_name, thread_proc, NULL, param);
            if (!result) {
                master->running_count--;
                char *error = str_snprintf("run thread %s failed.", job_name);
                do_fail_tests(master, error);
                free(error);
            }
        }
        tm_release_master(master);
    }
    return result;
}

static char *build_reloader_report(test_master_t *master) {
    char *result = NULL;
    if (master->reload_module_count < 1)
        result = str_strdup(" ");
    else {
        double reloader_time = tmhs_fms_elapsed(&master->first_reload_tick);
        double rate = reloader_time / (double)master->reload_module_count;
        result = 
            str_snprintf(" reloaded module %d times; on ave. every %0.1f ms",
                master->reload_module_count, rate);
    }
    return result;
}

static void report_test_results(test_master_t *master) {
    char *status_msg = NULL;
    if (!master->test_failed) {
        char *reloader_report = build_reloader_report(master);
        status_msg = 
            str_snprintf("\033[1;32mtests succeeded OK in %0.1f ms.%s\033[0m", 
                tmhs_fms_elapsed(&master->start_tick), reloader_report);
        log_success(0, status_msg);  
        free(reloader_report);
    }
    else {
        status_msg = str_snprintf("\033[1;31mtests FAILED in %0.1f ms.\033[0m", 
            tmhs_fms_elapsed(&master->start_tick));
        log_error(status_msg);
        log_error("error count: %d", master->errors->count);
        for (int i = 0; i < master->errors->count; i++) {
            log_error("error %d: %s", i, sl_get(master->errors, i));
        }
    }
    xapp_set_exit_message(status_msg);
    free(status_msg);
}

void tm_thread_done(void) 
{
    bool all_tests_done = false;
    int exit_code = 0;
    int need_wait_reloader_thread = 0;
    test_master_t *master = tm_get_n_lock_master();
    if (master) {
        master->running_count--;
        if (!master->running_count) {
            all_tests_done = true;
            need_wait_reloader_thread = master->reload_module_thread;
            if (!need_wait_reloader_thread) {
                report_test_results(master);                
                exit_code = master->test_failed ? 1 : 0; 
            }            
        }
        tm_release_master(master);
    }

    if (need_wait_reloader_thread) {
        timespan_hs_t tick;
        tmhs_tick(&tick);
        log_debug(0, "began to wait reloader termination");
        bool reloader_exited_ok = 
            at_terminate_athread_wait(need_wait_reloader_thread, 20000);
        log_debug(0, "waited %0.1f ms for reloader termination",
            tmhs_fms_elapsed(&tick));

        test_master_t *master = tm_get_n_lock_master();
        if (master) {  
            if (!reloader_exited_ok)
                do_fail_tests(master, "reload module thread did not return");
            report_test_results(master);
            exit_code = master->test_failed ? 1 : 0; 
            tm_release_master(master);
        };
    }

    if (all_tests_done) xapp_exit(exit_code);
}

test_config_t *tm_clone_config(void)
{
    test_config_t *config = NULL;
    test_master_t *master = tm_get_n_lock_master();
    if (master) {
        config = tc_clone(master->config);
        if (!tc_compare_configs(master->config, config))
            do_fail_tests(master, "internal err: check config cloning failed");
        tm_release_master(master);
    }

    return config;
}

bool tm_set_server_pid(int pid)
{
    test_master_t *master = tm_get_n_lock_master();
    bool result = (master != NULL);
    if (result) {
        master->server_pid = pid;
        tm_release_master(master);
    }
    return result;
}

bool tm_check_server_pid(int pid) {
    test_master_t *master = tm_get_n_lock_master();
    bool result = (master != NULL);
    if (result) {
        result = (master->server_pid == pid);
        tm_release_master(master);
    }
    return result;
}

int tm_estimate_thread_count(void) {
    int result = 0;
    test_master_t *master = tm_get_n_lock_master();
    if (master) {
        result = tc_estimate_thread_count(master->config);
        tm_release_master(master);
    }
    return result;
}

void tm_create_reload_module_thread(athread_proc_t cb)
{
    test_master_t *master = tm_get_n_lock_master();
    if (master) {
        if (master->config->run_module_reloder) {
            master->reload_module_thread = at_create_alertable_thread(cb, 
                "reload_mod", NULL);
            if (!master->reload_module_thread)
                do_fail_tests(master, "tm_create_reload_module_thread() FILED.");
        }
        tm_release_master(master);
    }
}

void tm_inc_reload_count(void) {
    test_master_t *master = tm_get_n_lock_master();
    if (master) {
        master->reload_module_count++;
        if (master->reload_module_count == 1)
            tmhs_tick(&master->first_reload_tick);
        tm_release_master(master);
    }        
}

void tm_run_reload_module(void) {
    test_master_t *master = tm_get_n_lock_master();
    if (master) {
        if (master->reload_module_thread)
            at_signale_thread(master->reload_module_thread);
        tm_release_master(master);
    }   
}