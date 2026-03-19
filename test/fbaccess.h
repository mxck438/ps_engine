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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <inttypes.h>

#include "xsi/strhash.h"
#include "xsi/stringlist.h"
#include "xsi/xtime.h"

#include "ibase.h"

typedef struct fb_database_ fb_database_t;
typedef struct fb_transaction_ fb_transaction_t;
typedef struct fb_sql_ fb_sql_t;

typedef enum default_tr_action_ {
	DTA_COMMIT,
	DTA_ROLLBACK
} default_tr_action_t;


// init / finalize
bool fb_initialize(char *path, bool bind_static);
void fb_finalize(void);
char *fb_get_dll_path(void);
char *fb_get_client_library_descr(void);
void fb_log_db_error(fb_database_t *db);

// database
fb_database_t *fb_create_database(char *tag);
void fb_set_database_id(fb_database_t *db, int32_t id);
int32_t fb_get_database_id(fb_database_t *db);
char *fb_get_database_tag(fb_database_t *db);
void fb_free_database(fb_database_t *db);
bool fb_connect_database(fb_database_t *db, char *dbpath, char *username,
	char *password, char *charset);
bool fb_disconnect_database(fb_database_t *db);
bool fb_set_default_db_transaction(fb_database_t *db, fb_transaction_t *tr);
bool fb_get_has_errors(fb_database_t *db);
ISC_STATUS fb_get_last_primary_error(fb_database_t *db);
bool fb_get_last_error(fb_database_t *db, char *buf, size_t bufsize,
    char *separator_str);

// transaction
fb_transaction_t *fb_create_transaction(fb_database_t *db, 
	default_tr_action_t default_action, uint8_t param_count, char *params);   
void fb_free_transaction(fb_transaction_t *tr);
bool fb_start_transaction(fb_transaction_t *tr);
bool fb_commit_transaction(fb_transaction_t *tr);
bool fb_rollback_transaction(fb_transaction_t *tr);
bool fb_in_transaction(fb_transaction_t *tr);

// SQL query
fb_sql_t *fb_create_sql(fb_database_t *db);
void fb_free_sql(fb_sql_t *this);
bool fb_sql_set_text(fb_sql_t *this, char *text);
bool fb_sql_set_text_sl(fb_sql_t *this, string_list_t *sl);
void fb_close_sql(fb_sql_t *this);
bool fb_is_sql_prepared(fb_sql_t *this);
bool fb_is_sql_active(fb_sql_t *this);
bool fb_prepare_sql(fb_sql_t *this);
bool fb_debug_exec_immed(fb_sql_t *this);
bool fb_exec_sql(fb_sql_t *this);
bool fb_sql_set_transaction(fb_sql_t *this, fb_transaction_t *tr);
bool fb_is_eof(fb_sql_t *this);
bool fb_sql_next(fb_sql_t *this);

// Params; pbn == param_by_name
bool fb_set_param_as_int(fb_sql_t *this, short index, int value);
bool fb_set_pbn_as_int(fb_sql_t *this, char *name, int value);
bool fb_set_param_as_null(fb_sql_t *this, short index);
bool fb_set_pbn_as_null(fb_sql_t *this, char *name);
bool fb_set_param_as_blob(fb_sql_t *this, short index, char *buf, size_t size);
bool fb_set_pbn_as_blob(fb_sql_t *this, char *name, char *buf, size_t size);
bool fb_set_param_as_string(fb_sql_t *this, short index, char *s);
bool fb_set_pbn_as_string(fb_sql_t *this, char *name, char *s);
bool fb_set_param_as_epoch_time(fb_sql_t *this, short index, time_t val, 
    int milliseconds);
bool fb_set_param_as_timestamp(fb_sql_t *this, short index, timestamp_t *ts);
bool fb_set_pbn_as_timestamp(fb_sql_t *this, char *name, timestamp_t *ts);
bool fb_set_pbn_as_bigint(fb_sql_t *this, char *name, int64_t value);

// this blob API is currently unused (and untested)
bool fb_begin_param_as_blob(fb_sql_t *this, short index, 
    isc_blob_handle * hblob);
bool fb_sql_write_blob_segment(fb_sql_t *this, isc_blob_handle hblob, 
    char *buf, size_t size);
bool fb_end_param_as_blob(fb_sql_t *this, isc_blob_handle hblob);

bool fb_create_blob(fb_database_t *db, fb_transaction_t *transaction, 
    isc_blob_handle *hblob, ISC_QUAD *blobid);
bool fb_write_blob_segment(isc_blob_handle *hblob, char *data,
    size_t size);
bool fb_close_blob(isc_blob_handle *hblob);
bool fb_cancel_blob(isc_blob_handle *hblob);
bool fb_set_param_as_blobid(fb_sql_t *query, short index, ISC_QUAD *blobID);        
bool fb_open_field_blob(fb_sql_t *query, short field_idx, 
    isc_blob_handle *hblob, size_t *blob_size);
bool fb_read_blob_segment(isc_blob_handle *hblob, char *buf,
    size_t size, size_t *read_bytes);    

// Fields
int fb_sql_get_field_count(fb_sql_t *this);
XSQLVAR *fb_field_by_index(fb_sql_t *this, short index);
XSQLVAR *fb_field_by_name(fb_sql_t *this, char *name);

// direct field access
bool fb_field_is_null(XSQLVAR *field);
bool fb_field_try_as_int(XSQLVAR *field, int *value);
bool fb_field_try_as_string_realloc(XSQLVAR *field, char **value);
bool fb_field_try_as_datetime(XSQLVAR *field, struct tm *value, int *millis);
bool fb_field_try_as_timestamp(XSQLVAR *field, timestamp_t *ts);

// access by index
bool fb_try_field_is_null(fb_sql_t *this, short index, bool *value);
bool fb_try_field_as_int(fb_sql_t *this, short index, int *value);
bool fb_try_field_as_string_realloc(fb_sql_t *this, short index, char **value);
bool fb_try_field_as_datetime(fb_sql_t *this, short index, struct tm *value, 
    int* millis);
bool fb_try_field_as_timestamp(fb_sql_t *this, short index, timestamp_t *ts);

// the following coerce NULLs to default values (0,'', etc).
// Use with caution.
bool fb_get_field_is_null(fb_sql_t *this, short index);
int fb_field_as_int(fb_sql_t *this, short index);
bool fb_read_field_blob_realloc(fb_sql_t *this, short index,
	char **buf, size_t *size);

// access by name
bool fb_fbn_is_null(fb_sql_t *this, char *name);
int fb_fbn_as_int(fb_sql_t *this, char *name);
double fb_fbn_as_double(fb_sql_t *this, char *ApFieldName);
bool fb_read_fbn_blob_realloc(fb_sql_t *this, char *name,
    char **buf, size_t *size);
bool fb_fbn_as_string_realloc(fb_sql_t *this, char *name, char **value);
bool fb_try_fbn_as_timestamp(fb_sql_t *this, char *name, timestamp_t *ts);
