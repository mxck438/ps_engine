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

#include "test_config.h"

#define GENERAL_CAT "general"

static func_test_t *create_fn_test(void) {
    func_test_t *test = malloc(sizeof(*test));
    if (test) {
        memset(test, 0, sizeof(*test));
    }
    return test;     
}

static bool validate_fn_test(func_test_t *test) {
    return !EMPTY_STRING(test->sql) && !EMPTY_STRING(test->result);
}

static void free_fn_test(func_test_t *test) {
    free(test->sql);
    free(test->result);
    free(test);
}

static ext_func_t *create_ext_func(void) {
    ext_func_t *func = malloc(sizeof(*func));
    if (func) {
        memset(func, 0, sizeof(*func));
        func->tests = lst_create_list();
    }
    return func;    
}

static bool validate_ext_func(ext_func_t *func) {
    bool result = !EMPTY_STRING(func->name)
        && !EMPTY_STRING(func->param_list)
        && !EMPTY_STRING(func->returns)
        && !EMPTY_STRING(func->ext_name)
        && !EMPTY_STRING(func->engine);
    if (result && (func->tests->count > 0)) {
        for (int i = 0 ; i < func->tests->count; i++) {
            func_test_t *test = lst_get_item(func->tests, i);
            result = validate_fn_test(test);
            if (!result) break;  
        }
    }
    return result;
}

static void free_ext_func(ext_func_t *func) {
    free(func->name);
    free(func->param_list);
    free(func->returns);
    free(func->ext_name);
    free(func->engine);
    for (int i = 0 ; i < func->tests->count; i++) {
        free_fn_test(lst_get_item(func->tests, i));
    }
    lst_free_list(func->tests);
    free(func);
}

static test_db_t *create_db_rec(void) {
    test_db_t *db = malloc(sizeof(*db));
    if (db) {
        memset(db, 0, sizeof(*db));
        db->funcs = lst_create_list();
    }
    return db;    
}

static bool validate_db_rec(test_db_t *db) {
    return !EMPTY_STRING(db->dbpath)
        && (db->funcs->count > 0);
}

static void free_db_rec(test_db_t *db) {
    free(db->dbpath);
    lst_free_list(db->funcs);
    free(db);
}

static test_config_t *create_config(void) {
    test_config_t *config = malloc(sizeof(*config));
    if (config) {
        memset(config, 0, sizeof(*config));
        config->databases = lst_create_list();
        config->funcs = lst_create_list();
    }
    return config;
}

static bool validate_config(test_config_t *config) {
    return !EMPTY_STRING(config->server)
        && (config->port)
        && !EMPTY_STRING(config->user)
        && !EMPTY_STRING(config->password)
        && (config->databases->count > 0)
        && (config->funcs->count > 0);
}

static void free_config(test_config_t *config) {
    free(config->libfbclient);
    free(config->server);
    free(config->user);
    free(config->password);
    for (int  i = 0; i < config->databases->count; i++)
        free_db_rec(lst_get_item(config->databases, i));
    lst_free_list(config->databases);

    for (int  i = 0; i < config->funcs->count; i++)
        free_ext_func(lst_get_item(config->funcs, i));
    lst_free_list(config->funcs);    

    free(config);
}

static bool get_next_token(char **s, char **start, char **end)
{
    while ((**s == ' ') || (**s == '\t')) (*s)++;
    if (!(**s)) return false;
    *start = *s;
    while ((**s) && (**s != ' ') && (**s != '\t') && (**s != ',')) (*s)++;
    *end = *s;
    while ((**s == ' ') || (**s == '\t')) (*s)++;
    if (**s == ',') (*s)++;
    return *end > *start;
}

static ext_func_t *func_by_name(test_config_t *config, char *name) {
    for (int i = 0; i < config->funcs->count; i++) {
        ext_func_t *func = lst_get_item(config->funcs, i);
        if (!str_stricmp(name, func->name)) return func;
    }
    return NULL;
}

static bool func_already_in_db(test_db_t *db, char *name) {
    for (int i = 0; i < db->funcs->count; i++) {
        ext_func_t *func = lst_get_item(db->funcs, i);
        if (!str_stricmp(name, func->name)) return true;
    }
    return false;    
}

static bool parse_db_functions(test_config_t *config, test_db_t *db,
    char *db_funcs)
{
    if (EMPTY_STRING(db_funcs)) {
        log_error("no functions specified for a database.");
        return false;
    } 
    char *funcs = str_strdup(db_funcs);
    char *p = funcs;
    char *start;
    char *end;
    while (get_next_token(&p, &start, &end)) {
        *end = 0;
        ext_func_t *func = func_by_name(config, start);
        if (!func) {
            log_error("function %s not found", start);
            return false;
        }
        else if (func_already_in_db(db, start)) {
            log_error("function %s duplicate for db.", start);
            return false;
            
        }
        else lst_add(db->funcs, func);
    }
    free(funcs);
    return true;
}    

test_config_t *tc_load_test_config(void)
{
    ini_file_t *ini = xapp_load_config();
    if (!ini) return NULL;

    test_config_t *config = create_config();
    if (config) {
        config->libfbclient =
            str_strdup(ini_get_raw_value(ini, GENERAL_CAT, "libfbclient"));
        config->server = 
            str_strdup(ini_get_raw_value(ini, GENERAL_CAT, "fbserver"));
        config->port = ini_get_as_portnumber(ini, GENERAL_CAT, "port", 0);
        config->user = 
            str_strdup(ini_get_raw_value(ini, GENERAL_CAT, "user"));
        config->password = 
            str_strdup(ini_get_raw_value(ini, GENERAL_CAT, "password"));    

        int db_count = 0;
        ini_get_as_integer(ini, GENERAL_CAT, "database_count", &db_count);
        int fn_count = 0;
        ini_get_as_integer(ini, GENERAL_CAT, "functions_count", &fn_count);

        config->drop_funcs_before_use =
            ini_get_as_bool(ini, GENERAL_CAT, "drop_funcs_before_use");
        config->use_create_or_alter =
            ini_get_as_bool(ini, GENERAL_CAT, "use_create_or_alter"); 
        config->run_module_reloder =
            ini_get_as_bool(ini, GENERAL_CAT, "run_module_reloder"); 

        int error_count = 0;
        for (int i = 0; i < fn_count; i++) {
            char *cat = str_snprintf("function_%d", i + 1);
            if (ini_category_exists(ini, cat)) {
                ext_func_t *func = create_ext_func();
                func->name = str_strdup(ini_get_raw_value(ini, cat, "name"));
                func->param_list = 
                    str_strdup(ini_get_raw_value(ini, cat, "parameter_list"));
                func->returns = 
                    str_strdup(ini_get_raw_value(ini, cat, "returns"));    
                func->ext_name = 
                    str_strdup(ini_get_raw_value(ini, cat, "external_name"));  
                func->engine = 
                    str_strdup(ini_get_raw_value(ini, cat, "engine"));  

                // load tests
                int test_count = 0;
                ini_get_as_integer(ini, cat, "test_count", &test_count);
                for (int j = 1; j <= test_count; j++) {
                    func_test_t *test = create_fn_test();
                    lst_add(func->tests, test);
                    
                    char *testn = str_snprintf("test%d", j);
                    test->sql = str_strdup(ini_get_raw_value(ini, cat, testn));
                    free(testn);

                    char *testrn = str_snprintf("test_result%d", j);
                    test->result = 
                        str_strdup(ini_get_raw_value(ini, cat, testrn));
                    free(testrn);
                }

                if (validate_ext_func(func))
                    lst_add(config->funcs, func);
                else {
                   log_error("config category %s validation failed", cat); 
                   free_ext_func(func);
                   error_count++;
                }
            }
            else {
                log_error("config category %s not found", cat);
                error_count++;
            }
            free(cat);
        }

        if (error_count) goto FAIL_N_QUIT;

        for (int i = 0; i < db_count; i++) {
            char *cat = str_snprintf("database_%d", i + 1);
            if (ini_category_exists(ini, cat)) {
                test_db_t *db = create_db_rec();
                db->dbpath = str_strdup(ini_get_raw_value(ini, cat, "dbpath"));
                char *funcs = ini_get_raw_value(ini, cat, "functions");
                ini_get_as_integer(ini, cat, "test_repeat_count", 
                    &db->test_repeat_count);
                ini_get_as_integer(ini, cat, "test_threads", 
                    &db->test_threads);    

                if (parse_db_functions(config, db, funcs)
                    && validate_db_rec(db))
                    lst_add(config->databases, db);
                else {
                   log_error("config category %s validation failed", cat); 
                   free_db_rec(db);
                   error_count++;
                }
            }
            else {
                log_error("config category %s not found", cat);
                error_count++;
            }
            free(cat);
        }

        if (error_count || !validate_config(config)) goto FAIL_N_QUIT;        
    }
    ini_free_inifile_struct(ini);
    return config;

FAIL_N_QUIT:

    free_config(config);
    ini_free_inifile_struct(ini);
    return NULL;
}

void tc_free_test_config(test_config_t *config)
{
    free_config(config);
}

test_config_t *tc_clone(test_config_t *config)
{
    test_config_t *result = create_config();
    result->libfbclient = str_strdup(config->libfbclient);
    result->server = str_strdup(config->server);
    result->port = config->port;
    result->user = str_strdup(config->user);
    result->password = str_strdup(config->password);
    result->drop_funcs_before_use = config->drop_funcs_before_use;
    result->use_create_or_alter = config->use_create_or_alter;
    result->run_module_reloder = config->run_module_reloder;

    for (int i = 0; i < config->funcs->count; i++) {
        ext_func_t *srcfn = lst_get_item(config->funcs, i);
        ext_func_t *newfn = create_ext_func();
        newfn->name = str_strdup(srcfn->name);
        newfn->param_list = str_strdup(srcfn->param_list);
        newfn->returns = str_strdup(srcfn->returns);
        newfn->ext_name = str_strdup(srcfn->ext_name);
        newfn->engine = str_strdup(srcfn->engine);

        for (int j = 0; j < srcfn->tests->count; j++) {
            func_test_t *srct = lst_get_item(srcfn->tests, j);
            func_test_t *newt = create_fn_test();
            lst_add(newfn->tests, newt);
            newt->sql = str_strdup(srct->sql);
            newt->result = str_strdup(srct->result);
        }

        lst_add(result->funcs, newfn);
    }

    for (int i = 0; i < config->databases->count; i++) {
        test_db_t *srcdb = lst_get_item(config->databases, i);   
        test_db_t *newdb = create_db_rec();
        newdb->dbpath = str_strdup(srcdb->dbpath);
        newdb->test_repeat_count = srcdb->test_repeat_count;
        newdb->test_threads = srcdb->test_threads;
        lst_add(result->databases, newdb);

        for (int j = 0; j < srcdb->funcs->count; j++ ) {
            ext_func_t *srcfn = lst_get_item(srcdb->funcs, j);
            ext_func_t *newfn = func_by_name(result, srcfn->name);
            if (!newfn) {
                log_error("unexpected @%s:%d", __FILE__, __LINE__);
                free_config(result);
                return NULL;
            }
            else lst_add(newdb->funcs, newfn);
        }
    }

    if (!validate_config(result)) {
        free_config(result);
        result = NULL;
    }

    return result;
}

int tc_estimate_thread_count(test_config_t *config) {
    int result = 0;
    for (int i = 0; i < config->databases->count; i++) {
        test_db_t *db = lst_get_item(config->databases, i);
        result += db->test_threads;        
    }
    return result;
}

static void md5_str(MD5Context *ctx, char *s) {
    if (!EMPTY_STRING(s))
        md5Update(ctx, s, strlen(s));
}

static void md_i16(MD5Context *ctx, uint16_t v) {
    md5Update(ctx, (uint8_t*)&v, sizeof(v));
}

static void md_bool(MD5Context *ctx, bool v) {
    md5Update(ctx, (uint8_t*)&v, sizeof(v));
}

static void md_int(MD5Context *ctx, int v) {
    md5Update(ctx, (uint8_t*)&v, sizeof(v));
}

static char *calc_config_hash(test_config_t *config)
{
    MD5Context ctx;
	md5Init(&ctx);

    md5_str(&ctx, config->libfbclient);
    md5_str(&ctx, config->server);
    md_i16(&ctx, config->port);
    md5_str(&ctx, config->user);
    md5_str(&ctx, config->password);

    for (int i = 0; i < config->databases->count; i++) {
        test_db_t *db = lst_get_item(config->databases, i);
        md5_str(&ctx, db->dbpath);
        for (int j = 0; j < db->funcs->count; j++) {
            ext_func_t *fn = lst_get_item(db->funcs, j);  
            md5_str(&ctx, fn->name);
        }
        md_int(&ctx, db->test_repeat_count);
        md_int(&ctx, db->test_threads);
    }

    for (int i = 0; i < config->funcs->count; i++) {
        ext_func_t *fn = lst_get_item(config->funcs, i);
        md5_str(&ctx, fn->name);
        md5_str(&ctx, fn->param_list);
        md5_str(&ctx, fn->returns);
        md5_str(&ctx, fn->ext_name);
        md5_str(&ctx, fn->engine);
        for (int j = 0; j < fn->tests->count; j++) {
            func_test_t *test = lst_get_item(fn->tests, j);  
            md5_str(&ctx, test->sql);
            md5_str(&ctx, test->result);
        }
    }

    md_bool(&ctx, config->drop_funcs_before_use);
    md_bool(&ctx, config->use_create_or_alter);
    md_bool(&ctx, config->run_module_reloder);

    md5Finalize(&ctx);
    return md5_to_str(ctx.digest); 
}

bool tc_compare_configs(test_config_t *config1, test_config_t *config2)
{
    char *hash1 = calc_config_hash(config1);
    char *hash2 = calc_config_hash(config2);
    bool result = !str_stricmp(hash1, hash2);
    free(hash1);
    free(hash2);
    return result;
}