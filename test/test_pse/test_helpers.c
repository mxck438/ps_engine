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

#include <limits.h>

#include "test_helpers.h"

static void parse_db_connect_string(char *connect_string, 
    char **server_address, uint16_t *port, char **db_path)
{
    *server_address = NULL;
    *port = DEFAULT_FIREBIRD_PORT;
    *db_path = NULL;
	if (EMPTY_STRING(connect_string)) return;

	char* LpColon = NULL;
	char* LpPortSlash = NULL;
	char* Lp = connect_string;
	while (*Lp)
	{
		switch (*Lp)
		{
			case '/':
			{
				if (LpPortSlash)
					return;
				LpPortSlash = Lp;
				break;
			}
			case ':':
			{
				LpColon = Lp;
				goto DONE;
			}
		}
		Lp++;
	}
DONE:
	if (!LpColon)
		return;
	if (LpPortSlash)
	{
		*server_address = str_strndup(connect_string, 
            (size_t)(LpPortSlash - connect_string));
		int LPort = 0;
		LpPortSlash++;
		if (str_parse_n_int(LpPortSlash, strlen(LpPortSlash), &LPort)
			&& (LPort > 0)
			&& (LPort < USHRT_MAX))
			*port = (unsigned short)LPort;
	}
	else
	{
		*server_address = str_strndup(connect_string, 
            (size_t)(LpColon - connect_string));
		*port = DEFAULT_FIREBIRD_PORT;
	}
	LpColon++;
	*db_path = str_strdup(LpColon);
}

static bool probe_is_online(char *connect_str) {
    char *server_address = NULL;
    uint16_t port = 0;
    char *db_path = NULL;
    parse_db_connect_string(connect_str, &server_address, &port, &db_path);
    bool result = !EMPTY_STRING(server_address) 
        && (port != 0)
        && net_probe_is_server_online(server_address, port, 1000);
    free(server_address);
    free(db_path);
    return result;
}

fb_database_t *th_connect_db(char *tag, int32_t id, char *dbpath, 
    char *username, char *password, char *charset)
{
    if (!probe_is_online(dbpath)) return NULL;

    fb_database_t *db = fb_create_database(tag);
    if (db) {
        fb_set_database_id(db, id);
        if (!fb_connect_database(db, dbpath, username, password, charset))
        {
            fb_free_database(db);
            db = NULL;
        }
    }
    return db;
}    

fb_database_t *th_connect_config_db(test_config_t *config, int db_index)
{
    test_db_t *config_db = lst_get_item(config->databases, db_index);
    char *connect_str = str_snprintf("%s/%d:%s", config->server, config->port,
        config_db->dbpath);
    fb_database_t *db = th_connect_db("db", db_index, connect_str, 
        config->user, config->password, "UTF8");
    free(connect_str);
    return db;
}

static fb_transaction_t *create_read_transaction(fb_database_t *db, bool start)
{
    char params[] = { isc_tpb_read_committed,
						isc_tpb_rec_version,
						isc_tpb_nowait };
	fb_transaction_t *transaction = fb_create_transaction(db,
		DTA_COMMIT,
		3,
		params);
    if (transaction) {
        if (start) {
            if (!fb_start_transaction(transaction)) {
                fb_free_transaction(transaction);
                transaction = NULL;
            }
        }
    }   
    return transaction;
}

static fb_transaction_t *create_write_transaction(fb_database_t *db, bool start)
{
    char params[] = {isc_tpb_read_committed,
					isc_tpb_rec_version,
					isc_tpb_write,
					isc_tpb_nowait };
	fb_transaction_t *transaction = fb_create_transaction(db,
		DTA_ROLLBACK,
		4,
		params);
    if (transaction) {
        if (start) {
            if (!fb_start_transaction(transaction)) {
                fb_free_transaction(transaction);
                transaction = NULL;
            }
        }
    }   
    return transaction;
}

static bool commit_transaction(fb_database_t *db, fb_transaction_t *trans)
{
    bool result = fb_commit_transaction(trans);
    if (!result) fb_log_db_error(db);
    return result;
}

static bool check_db_op(fb_database_t *db, bool expr) {
    if (!expr) fb_log_db_error(db);
    return expr;
}

bool th_fetch_server_pid(fb_database_t *db, int *pid)
{    
    fb_transaction_t *trans = create_read_transaction(db, true);
    if (!trans) return false;

    fb_sql_t *query = fb_create_sql(db);
    char *sql = 
        "select ma.MON$SERVER_PID " \
        "from MON$ATTACHMENTS ma " \
        "where ma.MON$ATTACHMENT_ID = CURRENT_CONNECTION";
    bool result = check_db_op(db,
        fb_sql_set_transaction(query, trans)
        && fb_sql_set_text(query, sql)
        && fb_exec_sql(query));

    *pid = 0;
    if (result) {
        *pid = fb_field_as_int(query, 0);
        result = (*pid > 0) && commit_transaction(db, trans);
    }

    fb_free_sql(query);
    fb_free_transaction(trans);
    return result;
}

fb_database_t *th_connect_db_check_pid(test_config_t *config, int db_index)
{
    fb_database_t *db = th_connect_config_db(config, db_index);
    if (db) {
        int pid = 0;
        bool succeeded = th_fetch_server_pid(db, &pid)
            && tm_check_server_pid(pid);
        if (!succeeded) {
            fb_free_database(db);
            db = NULL;
        }
    }
    return db;
}

static bool does_func_exist(fb_database_t *db, char *name,
    fb_transaction_t *trans, bool *ret_val)
{
    char *sql =
        "select count(RDB$FUNCTION_NAME) " \
        "from RDB$FUNCTIONS " \
        "where upper(RDB$FUNCTION_NAME) = upper(?)";
    fb_sql_t *query = fb_create_sql(db);
    bool result = check_db_op(db,
        fb_sql_set_transaction(query, trans)
        && fb_sql_set_text(query, sql)
        && fb_set_param_as_string(query, 0, name)
        && fb_exec_sql(query));    
    if (result) *ret_val = (fb_field_as_int(query, 0) > 0);
    fb_free_sql(query);
    return result;
}

static bool do_drop_func(fb_database_t *db, ext_func_t *func,
    fb_transaction_t *trans)
{
    char *sql = str_snprintf("drop external function %s;", func->name);
    fb_sql_t *query = fb_create_sql(db);
    bool result = check_db_op(db,
        fb_sql_set_transaction(query, trans)
        && fb_sql_set_text(query, sql)
        && fb_exec_sql(query));    
    fb_free_sql(query);
    free(sql);
    return result;
}

bool th_drop_func_if_exists(fb_database_t *db, ext_func_t *func)
{
    fb_transaction_t *trans = create_write_transaction(db, true);
    if (!trans) return false;

    bool func_exists = false;
    bool result = does_func_exist(db, func->name, trans, &func_exists);
    if (result) {
        if (func_exists) {
            result = do_drop_func(db, func, trans)
                && commit_transaction(db, trans);
        }
    }

    fb_free_transaction(trans);

    if (result) {
        if (func_exists) {
            log_debug(0, "func %s existed in db[%d]", func->name, 
                fb_get_database_id(db));
            log_debug(0, "dropped func %s in db[%d]", func->name, 
                fb_get_database_id(db));    
        }
        else log_debug(0, "func %s did not exist in db[%d]", func->name, 
                fb_get_database_id(db));
    }
    else {
        log_error("%s() FAILED", __func__);
    }
    return result;
}

bool th_run_pse_reload_module(fb_database_t *db) {
    fb_transaction_t *trans = create_write_transaction(db, true);
    bool result = (trans != NULL);
    if (trans) {
        char *sql = "select PSE_RELOAD_MODULE(0) from RDB$DATABASE;";
        fb_sql_t *query = fb_create_sql(db);                    
        result = check_db_op(db,
            fb_sql_set_transaction(query, trans)
            && fb_sql_set_text(query, sql)
            && fb_exec_sql(query));            
        fb_free_sql(query);
        result = result && commit_transaction(db, trans);
        fb_free_transaction(trans);
    }
    return result;
}

bool th_install_pse_reload_module(fb_database_t *db) {
    fb_transaction_t *trans = create_write_transaction(db, true);
    bool result = (trans != NULL);
    if (trans) {
        bool fn_exists = false;
        result = does_func_exist(db, "pse_reload_module", trans, &fn_exists);
        if (result) {
            if (!fn_exists) {
                char *sql =
                    "create function pse_reload_module ( " \
                    "    dummy integer " \
                    ") returns varchar(1024) " \
                    "    external name 'pse_reload_module' " \
                    "    engine pse;";

                fb_sql_t *query = fb_create_sql(db);                    
                result = check_db_op(db,
                    fb_sql_set_transaction(query, trans)
                    && fb_sql_set_text(query, sql)
                    && fb_exec_sql(query));
                fb_free_sql(query);
            }
        }
        result = result && commit_transaction(db, trans);
        if (result && !fn_exists)
            log_debug(0, "installed 'pse_reload_module' in db[%d]",
                fb_get_database_id(db));
        fb_free_transaction(trans);
    }
    return result;
}

bool th_install_func(fb_database_t *db, ext_func_t *func, 
    bool use_create_or_alter)
{
    fb_transaction_t *trans = create_write_transaction(db, true);
    if (!trans) return false;

    char *sql;
    if (use_create_or_alter)
        sql = str_snprintf(
            "create or alter function %s (%s) returns %s external name " \
            "'%s' engine %s;",
            func->name, func->param_list, func->returns, func->ext_name, 
            func->engine);
    else sql = str_snprintf(
            "create function %s (%s) returns %s external name " \
            "'%s' engine %s;",
            func->name, func->param_list, func->returns, func->ext_name, 
            func->engine);
    fb_sql_t *query = fb_create_sql(db);
    bool result = check_db_op(db,
        fb_sql_set_transaction(query, trans)
        && fb_sql_set_text(query, sql)
        && fb_exec_sql(query)
        && commit_transaction(db, trans));    
    fb_free_sql(query);
    fb_free_transaction(trans);
    free(sql);
    if (result)
        log_debug(0, "installed func %s in db[%d]", func->name, 
                fb_get_database_id(db)); 
    return result;
}

bool th_exec_fn_test(fb_database_t *db, ext_func_t *func, func_test_t *test) {
    fb_transaction_t *trans = create_write_transaction(db, true);
    if (!trans) return false;

    fb_sql_t *query = fb_create_sql(db);
    bool result = check_db_op(db,
        fb_sql_set_transaction(query, trans)
        && fb_sql_set_text(query, test->sql)
        && fb_exec_sql(query));

    if (result) {
        int value = fb_field_as_int(query, 0);
        char *txt_val = str_snprintf("%d", value);
        result = !str_stricmp(txt_val, test->result);
        free(txt_val);
    }
    if (result) 
        result = commit_transaction(db, trans);    
    fb_free_sql(query);
    fb_free_transaction(trans);
    if (result)
        log_success(0, "test func %s in db[%d] succeeded OK.", func->name, 
                fb_get_database_id(db)); 
    else log_error("test func %s in db[%d] FAILED.", func->name, 
                fb_get_database_id(db));
    return result;    
}