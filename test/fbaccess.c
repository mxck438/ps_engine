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

#include <string.h>
#ifdef __linux__
    #define __USE_GNU
    #include <dlfcn.h>
#endif
#include <stdarg.h>
#include <limits.h>
#include <math.h>

#include <xsi.h>

#include "fbaccess.h"

// A fb_database_t is designed to be used from single thread at a time.
// It is not a problem to create multiple fb_database_t 'objects' for use
// in different threads.
struct fb_database_ {
	isc_db_handle DBhandle;
	ISC_STATUS StatusVector[20];
	fb_transaction_t* pDefaultTransaction;
	unsigned short DBdialect;
	int32_t id;
    char *tag;
};

#define MAX_FB_NAME 32
#define MAX_TRANS_PARAMS  10

struct fb_transaction_ {
	isc_tr_handle TRHandle;
	fb_database_t* pDatabase;
	char		  Params[MAX_TRANS_PARAMS + 1];
	uint8_t		  ParamsCount;
	default_tr_action_t DefaultAction;
};

typedef struct _Name2Param {
	struct _Name2Param * pNextAllocated;
	struct _Name2Param* pNext;
	XSQLVAR* pParam;
} t_Name2Param;

struct fb_sql_ {
	fb_database_t * pDatabase;
	// FBSQL object does not control pTransaction anyhow;
	// leave it to NULL to use DB default Transaction
	fb_transaction_t* pTransaction;  
	isc_stmt_handle hStatement;
	char* pSQLText;
	char* pPrepearedSQL;
	XSQLDA* pParams;
	str_hash_t* pParamNames;
	t_Name2Param* pName2ParamStorage;
	XSQLDA* pFields;
	str_hash_t* pFieldNames;
	bool mEOF;
};


//#include <jrd/build_no.h>
#ifndef PRODUCT_VER_STRING
#define PRODUCT_VER_STRING "TODO"
#endif

#ifdef _WIN32
#define FBCLIENT_DLL_NAME "fbclient.dll"
#define GDS_DLL_NAME "gds32.dll"
#else
#define FBCLIENT_DLL_NAME "libfbclient.so.3.0.7"
//#define FBCLIENT_DLL_NAME "libfbclient.so"
#endif
 
#define HAS_STATUS_VECTOR_ERRORS(x) ((x[0] == 1) && (x[1])) 
#define FIELD_IS_NULL(x) (((x->sqltype & 1) == 1) && (*x->sqlind == -1))
#define FIELD_BASE_TYPE(x)  (x->sqltype & ~1)
#define IS_NULLABLE(x)  (x->sqltype & 1)
#define DEFAULT_BLOB_CHUNK_SIZE 63 * 1024




typedef ISC_STATUS ISC_EXPORT tpf_isc_attach_database(ISC_STATUS*,
	short,
	const ISC_SCHAR*,
	isc_db_handle*,
	short,
	const ISC_SCHAR*);

typedef ISC_STATUS ISC_EXPORT tpf_isc_detach_database(ISC_STATUS*,
	isc_db_handle*);

typedef ISC_STATUS ISC_EXPORT_VARARG tpf_isc_start_transaction(ISC_STATUS*,
	isc_tr_handle*,
	short, ...);

typedef ISC_STATUS ISC_EXPORT tpf_isc_commit_transaction(ISC_STATUS*,
	isc_tr_handle*);

typedef ISC_STATUS ISC_EXPORT tpf_isc_rollback_transaction(ISC_STATUS*,
	isc_tr_handle*);


typedef ISC_STATUS ISC_EXPORT tpf_isc_dsql_allocate_statement(ISC_STATUS*,
	isc_db_handle*,
	isc_stmt_handle*);

typedef ISC_STATUS ISC_EXPORT tpf_isc_dsql_prepare(ISC_STATUS*,
	isc_tr_handle*,
	isc_stmt_handle*,
	unsigned short,
	const ISC_SCHAR*,
	unsigned short,
	XSQLDA*);

typedef ISC_STATUS ISC_EXPORT tpf_isc_dsql_describe(ISC_STATUS*,
	isc_stmt_handle*,
	unsigned short,
	XSQLDA*);

typedef ISC_STATUS ISC_EXPORT tpf_isc_dsql_describe_bind(ISC_STATUS*,
	isc_stmt_handle*,
	unsigned short,
	XSQLDA*);

typedef ISC_STATUS ISC_EXPORT tpf_isc_dsql_execute(ISC_STATUS*,
	isc_tr_handle*,
	isc_stmt_handle*,
	unsigned short,
	const XSQLDA*);

typedef ISC_STATUS ISC_EXPORT tpf_isc_dsql_execute2(ISC_STATUS*,
	isc_tr_handle*,
	isc_stmt_handle*,
	unsigned short,
	const XSQLDA*,
	const XSQLDA*);

typedef ISC_STATUS ISC_EXPORT tpf_isc_dsql_fetch(ISC_STATUS*,
	isc_stmt_handle*,
	unsigned short,
	const XSQLDA*);

typedef ISC_STATUS ISC_EXPORT tpf_isc_dsql_free_statement(ISC_STATUS*,
	isc_stmt_handle*,
	unsigned short);

                 //FB_API_DEPRECATED
typedef ISC_LONG  ISC_EXPORT tpf_isc_interprete(ISC_SCHAR*,
	ISC_STATUS**);

typedef ISC_STATUS ISC_EXPORT tpf_isc_database_info(ISC_STATUS*,
	isc_db_handle*,
	short,
	const ISC_SCHAR*,
	short,
	ISC_SCHAR*);

typedef ISC_LONG ISC_EXPORT tpf_isc_vax_integer(const ISC_SCHAR*,
	short);

typedef ISC_STATUS ISC_EXPORT tpf_isc_open_blob2(ISC_STATUS*,
	isc_db_handle*,
	isc_tr_handle*,
	isc_blob_handle*,
	ISC_QUAD*,
	ISC_USHORT,
	const ISC_UCHAR*);

typedef ISC_STATUS ISC_EXPORT tpf_isc_blob_info(ISC_STATUS*,
	isc_blob_handle*,
	short,
	const ISC_SCHAR*,
	short,
	ISC_SCHAR*);

typedef ISC_STATUS ISC_EXPORT tpf_isc_close_blob(ISC_STATUS*,
	isc_blob_handle*);

typedef ISC_STATUS ISC_EXPORT tpf_isc_get_segment(ISC_STATUS*,
	isc_blob_handle*,
	unsigned short*,
	unsigned short,
	ISC_SCHAR*);

typedef ISC_STATUS ISC_EXPORT tpf_isc_dsql_set_cursor_name(ISC_STATUS*,
	isc_stmt_handle*,
	const ISC_SCHAR*,
	unsigned short);

typedef ISC_STATUS ISC_EXPORT tpf_isc_dsql_sql_info(ISC_STATUS*,
	isc_stmt_handle*,
	short,
	const ISC_SCHAR*,
	short,
	ISC_SCHAR*);

typedef ISC_STATUS ISC_EXPORT tpf_isc_create_blob2(ISC_STATUS*,
	isc_db_handle*,
	isc_tr_handle*,
	isc_blob_handle*,
	ISC_QUAD*,
	short,
	const ISC_SCHAR*);

typedef ISC_STATUS ISC_EXPORT tpf_isc_cancel_blob(ISC_STATUS *,
									  isc_blob_handle *);    

typedef ISC_STATUS ISC_EXPORT tpf_isc_put_segment(ISC_STATUS*,
	isc_blob_handle*,
	unsigned short,
	const ISC_SCHAR*);

typedef void ISC_EXPORT tpf_isc_decode_timestamp(const ISC_TIMESTAMP*,
	void*);

typedef void ISC_EXPORT tpf_isc_encode_timestamp(const void*,
	ISC_TIMESTAMP*);


typedef struct _FBDll {
	bool BoundStatically;
#ifdef _WIN32
	HMODULE DllHandle;
#else
	void* DllHandle;
#endif
	tpf_isc_attach_database* p_isc_attach_database;
	tpf_isc_detach_database* p_isc_detach_database;
	tpf_isc_start_transaction* p_isc_start_transaction;
	tpf_isc_commit_transaction* p_isc_commit_transaction;
	tpf_isc_rollback_transaction* p_isc_rollback_transaction;
	tpf_isc_dsql_allocate_statement* p_isc_dsql_allocate_statement;
	tpf_isc_dsql_prepare* p_isc_dsql_prepare;
	tpf_isc_dsql_describe* p_isc_dsql_describe;
	tpf_isc_dsql_describe_bind* p_isc_dsql_describe_bind;
	tpf_isc_dsql_execute* p_isc_dsql_execute;
	tpf_isc_dsql_execute2* p_isc_dsql_execute2;
	tpf_isc_dsql_fetch* p_isc_dsql_fetch;
	tpf_isc_dsql_free_statement* p_isc_dsql_free_statement;
	tpf_isc_interprete* p_isc_interprete;
	tpf_isc_database_info* p_isc_database_info;
	tpf_isc_vax_integer* p_isc_vax_integer;
	tpf_isc_open_blob2* p_isc_open_blob2;
	tpf_isc_blob_info* p_isc_blob_info;
	tpf_isc_close_blob* p_isc_close_blob;
	tpf_isc_get_segment* p_isc_get_segment;
	tpf_isc_dsql_set_cursor_name* p_isc_dsql_set_cursor_name;
	tpf_isc_dsql_sql_info* p_isc_dsql_sql_info;
	tpf_isc_create_blob2* p_isc_create_blob2;
    tpf_isc_cancel_blob* p_isc_cancel_blob;
	tpf_isc_put_segment* p_isc_put_segment;
	tpf_isc_decode_timestamp* p_isc_decode_timestamp;
	tpf_isc_encode_timestamp* p_isc_encode_timestamp;
} t_FBDll;


t_FBDll g_FBDll = { 0 };


char* fb_get_dll_path(void)
{
	if (!g_FBDll.DllHandle) return NULL;
#ifdef _WIN32
	TCHAR szPath[MAX_PATH];

	if (!GetModuleFileNameW(g_FBDll.DllHandle, szPath, MAX_PATH))
		return NULL;
	else return str_widechar2utf8_alloc(szPath);
#else
	char LBuf[2048];  // TODO maybe
	if (0 != dlinfo(g_FBDll.DllHandle, RTLD_DI_ORIGIN, LBuf))
		return NULL;
	else return str_snprintf("%s/%s", LBuf, FBCLIENT_DLL_NAME);
#endif
}

#ifdef  _MSC_VER
#pragma warning(disable : 4996)
#endif
static bool BindStaticFBClient(void)
{
	/*
	g_FBDll.BoundStatically = true;
	g_FBDll.DllHandle = NULL;
	g_FBDll.p_isc_attach_database = isc_attach_database;
	g_FBDll.p_isc_detach_database = isc_detach_database;
	g_FBDll.p_isc_start_transaction = isc_start_transaction;
	g_FBDll.p_isc_commit_transaction = isc_commit_transaction;
	g_FBDll.p_isc_rollback_transaction = isc_rollback_transaction;
	g_FBDll.p_isc_dsql_allocate_statement = isc_dsql_allocate_statement;
	g_FBDll.p_isc_dsql_prepare = isc_dsql_prepare;
	g_FBDll.p_isc_dsql_describe = isc_dsql_describe;
	g_FBDll.p_isc_dsql_describe_bind = isc_dsql_describe_bind;
	g_FBDll.p_isc_dsql_execute = isc_dsql_execute;
	g_FBDll.p_isc_dsql_execute2 = isc_dsql_execute2;
	g_FBDll.p_isc_dsql_fetch = isc_dsql_fetch;
	g_FBDll.p_isc_dsql_free_statement = isc_dsql_free_statement;
	g_FBDll.p_isc_interprete = isc_interprete;
	g_FBDll.p_isc_database_info = isc_database_info;
	g_FBDll.p_isc_vax_integer = isc_vax_integer;
	g_FBDll.p_isc_open_blob2 = isc_open_blob2;
	g_FBDll.p_isc_blob_info = isc_blob_info;
	g_FBDll.p_isc_close_blob = isc_close_blob;
	g_FBDll.p_isc_get_segment = isc_get_segment;
	g_FBDll.p_isc_dsql_set_cursor_name = isc_dsql_set_cursor_name;
	g_FBDll.p_isc_dsql_sql_info = isc_dsql_sql_info;
	g_FBDll.p_isc_create_blob2 = isc_create_blob2;
    g_FBDll.p_isc_cancel_blob = isc_cancel_blob;
	g_FBDll.p_isc_put_segment = isc_put_segment;
	g_FBDll.p_isc_decode_timestamp = isc_decode_timestamp;
	g_FBDll.p_isc_encode_timestamp = isc_encode_timestamp;
	return true;
	*/
	return false;
}

#ifdef _WIN32 
static bool LoadFBSharedLibrary(char* path) {
	char* LpBuf = NULL;
	unsigned int LBufLen = 0;
	char* LpFN = NULL;

	if (!g_FBDll.DllHandle)
	{
		if (!EMPTY_STRING(path))
			g_FBDll.DllHandle = LoadLibraryA(path);
	}

	if (!g_FBDll.DllHandle)
	{
		char* Lp;
		if (LBufLen < MAX_PATH * 2)
			LBufLen = MAX_PATH * 2;
	MAKE_BUFFER:
		Lp = realloc(LpBuf, LBufLen);
		if (!Lp) goto FAIL_AND_CLEANUP;
		LpBuf = Lp;
		while (LBufLen == GetModuleFileNameA(NULL, LpBuf, LBufLen))
		{
			if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
				goto FAIL_AND_CLEANUP;
			LBufLen *= 2;
			goto MAKE_BUFFER;
		}
		LpFN = STR_strdup(LpBuf);
		if (!LpFN)
			goto FAIL_AND_CLEANUP;
		if (fl_truncate_to_path_inplace(LpFN))
		{
			if (fl_make_filename(FBCLIENT_DLL_NAME, LpFN, &LpBuf, &LBufLen))
				g_FBDll.DllHandle = LoadLibraryA(LpBuf);
			if (!g_FBDll.DllHandle)
				if (fl_make_filename(GDS_DLL_NAME, LpFN, &LpBuf, &LBufLen))
					g_FBDll.DllHandle = LoadLibraryA(LpBuf);
		}
	}

	if (!g_FBDll.DllHandle)
		g_FBDll.DllHandle = LoadLibraryA(FBCLIENT_DLL_NAME);
	if (!g_FBDll.DllHandle)
		g_FBDll.DllHandle = LoadLibraryA(GDS_DLL_NAME);

	if (!g_FBDll.DllHandle)
		goto FAIL_AND_CLEANUP;

	free(LpBuf);
	free(LpFN);
	return true;

FAIL_AND_CLEANUP:
	free(LpBuf);
	free(LpFN);
	fb_finalize();
	return false;
}
#endif

#ifndef _WIN32
static bool LoadFBSharedLibrary(char* path) {
    if (!EMPTY_STRING(path))
        g_FBDll.DllHandle = dlopen(path, RTLD_NOW);
    if (!g_FBDll.DllHandle)
	    g_FBDll.DllHandle = dlopen(FBCLIENT_DLL_NAME, RTLD_NOW);
	bool result = (g_FBDll.DllHandle != NULL);
	char* error = dlerror();
	if (error != NULL) 
		log_error("**************** dlerror(): %s", error);
	if (!g_FBDll.DllHandle)
		fb_finalize();
	return result;  
}
#endif

static bool BindDynamicFBClient(char* path)
{
	if (g_FBDll.DllHandle) return true;

	g_FBDll.BoundStatically = false;

	if (!LoadFBSharedLibrary(path)) return false;


#ifdef _WIN32
#define BIND_TO_API(x) (void *)g_FBDll.p_##x = GetProcAddress(g_FBDll.DllHandle, #x); \
	if (!g_FBDll.p_##x) \
		goto FAIL_AND_CLEANUP;
#else
#define BIND_TO_API(x) g_FBDll.p_##x = dlsym(g_FBDll.DllHandle, #x); \
	if (!g_FBDll.p_##x) \
		goto FAIL_AND_CLEANUP;
#endif

	BIND_TO_API(isc_attach_database);
	BIND_TO_API(isc_detach_database);
	BIND_TO_API(isc_start_transaction);
	BIND_TO_API(isc_commit_transaction);
	BIND_TO_API(isc_rollback_transaction);
	BIND_TO_API(isc_dsql_allocate_statement);
	BIND_TO_API(isc_dsql_prepare);
	BIND_TO_API(isc_dsql_describe);
	BIND_TO_API(isc_dsql_describe_bind);
	BIND_TO_API(isc_dsql_execute);
	BIND_TO_API(isc_dsql_execute2);
	BIND_TO_API(isc_dsql_fetch);
	BIND_TO_API(isc_dsql_free_statement);
	BIND_TO_API(isc_interprete);
	BIND_TO_API(isc_database_info);
	BIND_TO_API(isc_vax_integer);
	BIND_TO_API(isc_open_blob2);
	BIND_TO_API(isc_blob_info);
	BIND_TO_API(isc_close_blob);
	BIND_TO_API(isc_get_segment);
	BIND_TO_API(isc_dsql_set_cursor_name);
	BIND_TO_API(isc_dsql_sql_info);
	BIND_TO_API(isc_create_blob2);
    BIND_TO_API(isc_cancel_blob);
	BIND_TO_API(isc_put_segment);
	BIND_TO_API(isc_decode_timestamp);
	BIND_TO_API(isc_encode_timestamp);

	// all the rest when needs be

	return true;
FAIL_AND_CLEANUP:
	fb_finalize();
	return false;
}


bool fb_initialize(char* path, bool Static)
{
	if (Static)
		return BindStaticFBClient();
	else return BindDynamicFBClient(path);
}


char* fb_get_client_library_descr(void)
{
	if (!g_FBDll.BoundStatically)
		return fb_get_dll_path();
	else
	{
		char LBuf[256];
		sprintf_s(LBuf, sizeof(LBuf), "built-in Firebird %s", PRODUCT_VER_STRING);
		return str_strdup(LBuf);
	}
}


void fb_finalize(void)
{
	if (g_FBDll.DllHandle)
#ifdef _WIN32
		FreeLibrary(g_FBDll.DllHandle);
#else
        dlclose(g_FBDll.DllHandle);
#endif
	memset(&g_FBDll, 0, sizeof(t_FBDll));
}

fb_database_t* fb_create_database(char *tag)
{
	fb_database_t* Result = (fb_database_t*)malloc(sizeof(fb_database_t));
	if (Result) {
		memset(Result, 0, sizeof(fb_database_t));
        Result->tag = str_strdup(tag);
    }
	return Result;
}

void fb_set_database_id(fb_database_t* ApDB, int32_t id) {
	ApDB->id = id;
}

int32_t fb_get_database_id(fb_database_t* ApDB) {
	return ApDB->id;
}

char *fb_get_database_tag(fb_database_t *db) {
    return db->tag;
}

void fb_free_database(fb_database_t* ApDB)
{
	if (ApDB->pDefaultTransaction != NULL)
		fb_free_transaction(ApDB->pDefaultTransaction);
	fb_disconnect_database(ApDB);
    free(ApDB->tag);
	free(ApDB);
}


bool fb_get_has_errors(fb_database_t* ApDB)
{
	return HAS_STATUS_VECTOR_ERRORS(ApDB->StatusVector);
}


bool fb_connect_database(fb_database_t* ApDB, char* ADBPath, char* AUserName,
	char* APassword, char* ACharset)
{
	if (ApDB->DBhandle)
		return false;
	
	size_t LNameLen = (EMPTY_STRING(AUserName) ? 0 : strlen(AUserName));
	if (LNameLen > UCHAR_MAX)
		return false;
	size_t LPassLen = (EMPTY_STRING(APassword) ? 0 : strlen(APassword));
	if (LPassLen > UCHAR_MAX)
		return false;
	size_t LCharsetLen = (EMPTY_STRING(ACharset) ? 0 : strlen(ACharset));
	if (LCharsetLen > UCHAR_MAX)
		return false;
	size_t LDPBLen = 1;
	if (LNameLen)
		LDPBLen += (LNameLen + 2 * sizeof(char));
	if (LPassLen)
		LDPBLen += (LPassLen + 2 * sizeof(char));
	if (LCharsetLen)
		LDPBLen += (LCharsetLen + 2 * sizeof(char));

	char* LpDPB = (char*)malloc(LDPBLen);
	if (LpDPB == NULL)
		return false;

	char* LpPB = LpDPB;
	*LpPB++ = isc_dpb_version1;
	if (LNameLen)
	{
		*LpPB++ = isc_dpb_user_name;
		*LpPB++ = (char)LNameLen;
		memcpy(LpPB, AUserName, LNameLen);
		LpPB += LNameLen;
	}
	if (LPassLen)
	{
		*LpPB++ = isc_dpb_password;
		*LpPB++ = (char)LPassLen;
		memcpy(LpPB, APassword, LPassLen);
		LpPB += LPassLen;
	}
	if (LCharsetLen)
	{
		*LpPB++ = isc_dpb_lc_ctype;
		*LpPB++ = (char)LCharsetLen;
		memcpy(LpPB, ACharset, LCharsetLen);
		LpPB += LCharsetLen;
	}

	g_FBDll.p_isc_attach_database(ApDB->StatusVector,
		(short)strlen(ADBPath),
		ADBPath,
		&ApDB->DBhandle,
		(short)LDPBLen,
		LpDPB);
	
	free(LpDPB);

	if (HAS_STATUS_VECTOR_ERRORS(ApDB->StatusVector)) {
		return false;
	}

	//DBdialect
	char LDBInfoCommand = (char)isc_info_db_sql_dialect;   // isc_info_db_SQL_dialect
	char LBuf[10];
	g_FBDll.p_isc_database_info(ApDB->StatusVector,
		&ApDB->DBhandle,
		1,
		&LDBInfoCommand,
		10,
		LBuf);
	if (HAS_STATUS_VECTOR_ERRORS(ApDB->StatusVector))
		return false;

	if (LBuf[0] != isc_info_db_sql_dialect)
		ApDB->DBdialect = 1;
	else
	{
		ISC_LONG len = g_FBDll.p_isc_vax_integer(&LBuf[1], 2);
		ApDB->DBdialect = (unsigned short)g_FBDll.p_isc_vax_integer(&LBuf[3], (short)len);
	}
		
	return true;
}

bool fb_disconnect_database(fb_database_t* ApDB)
{
	if (ApDB->DBhandle)
	{
		g_FBDll.p_isc_detach_database(ApDB->StatusVector, &ApDB->DBhandle);
		ApDB->DBhandle = 0;
		bool result = !HAS_STATUS_VECTOR_ERRORS(ApDB->StatusVector);
		if (!result)
			log_error("FB: p_isc_detach_database() failed in %s", __func__);
		return result;
	}
	else {
		log_warning(6, "FB: fb_database_t*->DBhandle == NULL in %s", __func__);
		return false;
	}		
}

bool fb_set_default_db_transaction(fb_database_t* ApDB, fb_transaction_t* ApTr)
{
	if (ApDB->pDefaultTransaction != NULL)
		return false;
	ApDB->pDefaultTransaction = ApTr;
	return true;
}

ISC_STATUS fb_get_last_primary_error(fb_database_t* ApDB) {
	return ApDB->StatusVector[1];
}


bool fb_get_last_error(fb_database_t* ApDB, char* ApBuf, size_t ABufSize, 
    char* ASeparatorStr)
{
	char LBuf[512];
	ISC_STATUS* LpStatusVector = ApDB->StatusVector;
	ISC_LONG nRet;
	while (true)
	{
		nRet = g_FBDll.p_isc_interprete(LBuf, &LpStatusVector);
		if (nRet)
		{
			if (0 != strcat_s(ApBuf, ABufSize, LBuf))
				return false;
			if (!EMPTY_STRING(ASeparatorStr))
			{
				if (0 != strcat_s(ApBuf, ABufSize, ASeparatorStr))
					return false;
			}
		}
		else return true;
	}
	return true;
}


// ------------------------------------------------------------------------------
//     Transactions

// list of (char) params
fb_transaction_t* fb_create_transaction(fb_database_t* ApDB, 
	default_tr_action_t DefaultAction, uint8_t ParamCount, char* params)
{
	fb_transaction_t *Result = malloc(sizeof(fb_transaction_t));
	if (!Result) return NULL;
	memset(Result, 0, sizeof(fb_transaction_t));
	Result->pDatabase = ApDB;
	Result->DefaultAction = DefaultAction;

	if (ParamCount > MAX_TRANS_PARAMS)
		ParamCount = MAX_TRANS_PARAMS;
	for (int i = 1; i <= ParamCount; i++)
	{
		Result->Params[i] = *params;
		params++;
	}
	Result->ParamsCount = ParamCount;
	Result->Params[0] = isc_tpb_version3;

	return Result;  
}

void fb_free_transaction(fb_transaction_t* ApTr)
{
	if (ApTr->TRHandle)
	{
		switch (ApTr->DefaultAction)
		{
			case DTA_COMMIT : 
			{
				fb_commit_transaction(ApTr);
				break;
			}
			default :
			{
				fb_rollback_transaction(ApTr);
				break;
			}
		}		
	}
	free(ApTr);	
}

bool fb_start_transaction(fb_transaction_t* ApTr)
{
	if ((ApTr->TRHandle) || (!ApTr->pDatabase))
		return false;

	if (ApTr->ParamsCount)
		g_FBDll.p_isc_start_transaction(ApTr->pDatabase->StatusVector,
			&ApTr->TRHandle,
			1,  // number of databases
			&ApTr->pDatabase->DBhandle,
			ApTr->ParamsCount,
			ApTr->Params);
	else g_FBDll.p_isc_start_transaction(ApTr->pDatabase->StatusVector,
		&ApTr->TRHandle,
		1, // number of databases
		&ApTr->pDatabase->DBhandle,
		0,
		NULL);

	return !HAS_STATUS_VECTOR_ERRORS(ApTr->pDatabase->StatusVector);
}


bool fb_commit_transaction(fb_transaction_t* ApTr)
{
	if ((ApTr->TRHandle) && (ApTr->pDatabase))
	{
		g_FBDll.p_isc_commit_transaction(ApTr->pDatabase->StatusVector, &ApTr->TRHandle);
		ApTr->TRHandle = 0;
        bool result = !HAS_STATUS_VECTOR_ERRORS(ApTr->pDatabase->StatusVector);
        return result;
	}
	else return false;
}


bool fb_rollback_transaction(fb_transaction_t* ApTr)
{
	if ((ApTr->TRHandle) && (ApTr->pDatabase))
	{
		g_FBDll.p_isc_rollback_transaction(ApTr->pDatabase->StatusVector, &ApTr->TRHandle);
		ApTr->TRHandle = 0;
		return !HAS_STATUS_VECTOR_ERRORS(ApTr->pDatabase->StatusVector);
	}
	else return false;
}


bool fb_in_transaction(fb_transaction_t* ApTr)
{
	return ApTr->TRHandle != 0;
}


// -----------------------------------------------------------------------------------------
//  SQL


fb_sql_t* fb_create_sql(fb_database_t* ApDB)
{
	fb_sql_t* Result = (fb_sql_t*)malloc(sizeof(fb_sql_t));
	if (!Result)
		return NULL;
	memset(Result, 0, sizeof(fb_sql_t));
	Result->pDatabase = ApDB;

	return Result;
}


bool fb_is_sql_prepared(fb_sql_t* this)
{
	return (this->pParams != NULL);
}

bool fb_is_sql_active(fb_sql_t* this)
{
	return (this->pFields != NULL);
}


void fb_free_sql(fb_sql_t* this)
{
	fb_close_sql(this);
	if (this->pSQLText !=NULL)
		free(this->pSQLText);
	if (this->pPrepearedSQL !=NULL)
		free(this->pPrepearedSQL);
	if (this->pFieldNames)
		sht_release(this->pFieldNames);
	if (this->pParamNames)
		sht_release(this->pParamNames);

	t_Name2Param* Lp = this->pName2ParamStorage;
	t_Name2Param* LpD = NULL;
	while (Lp != NULL)
	{
		LpD = Lp;
		Lp = Lp->pNextAllocated;
		free(LpD);
	}
	free(this);
}


bool fb_sql_set_text(fb_sql_t* this, char* ApText)
{
	if (fb_is_sql_prepared(this) || fb_is_sql_active(this))
		fb_close_sql(this);
	if (this->pSQLText)
	{
		free(this->pSQLText);
		this->pSQLText = NULL;
	}
	if (this->pPrepearedSQL)
	{
		free(this->pPrepearedSQL);
		this->pPrepearedSQL = NULL;
	}
	if (EMPTY_STRING(ApText))
		return true;


	this->pSQLText = str_strdup(ApText);

	// To prepare or not to prepare?
	// Not to. Trigger prepare in Execute() or SetParam().

	return (this->pSQLText != NULL);
}


bool fb_sql_set_text_sl(fb_sql_t* this, string_list_t* ASL)
{
	if (fb_is_sql_prepared(this) || fb_is_sql_active(this))
		fb_close_sql(this);
	if (this->pSQLText)
	{
		free(this->pSQLText);
		this->pSQLText = NULL;
	}
	if (this->pPrepearedSQL)
	{
		free(this->pPrepearedSQL);
		this->pPrepearedSQL = NULL;
	}

	return sl_get_text_realloc(ASL, &this->pSQLText, NULL, NULL);
}


char* FindMatchingDoubleQuote(char* Ap, char* ApMax)
{
	while (Ap < ApMax)
	{
		if (*Ap == '"')
		{
			if ((Ap + 1 < ApMax) && (Ap[1] == '"'))
				Ap += 2;
			else return Ap;
		}
		else Ap++;
	}
	return NULL;
}


// internal private method,
// preconditions: pSQLText != NULL, ApSL != NULL
bool PreprocessSQL(fb_sql_t* this, string_list_t * ApSL)
{
	if (EMPTY_STRING(this->pSQLText))
		return false;

	size_t len = strlen(this->pSQLText);
	char* T = (char*)realloc(this->pPrepearedSQL, len + 1);
	if (T == NULL)
		return false;
	this->pPrepearedSQL = T;
	*this->pPrepearedSQL = 0;

	char* Lp = this->pSQLText;
	char* LpMax = Lp + len;
//	size_t LLeftInOutBuf = len + 1;
	char* LpStart;
	size_t LSubstrLen;

	// setup param name chars
	char LValidNameChars[256];
	memset(LValidNameChars, 0, 256);
	// defined as belonging to set {'0..9','a..z','A..Z','_','$'}.
	for (int cc = '0'; cc <= '9'; cc++)
		LValidNameChars[cc] = 1;
	for (int cc = 'a'; cc <= 'z'; cc++)
		LValidNameChars[cc] = 1;
	for (int cc = 'A'; cc <= 'Z'; cc++)
		LValidNameChars[cc] = 1;

	LValidNameChars['_'] = 1;
	LValidNameChars['$'] = 1;
	 

	// if it is next substring to copy, it starts here
	LpStart = Lp;
	while (Lp < LpMax)
	{
		switch (*Lp)
		{
			default: 
			{
				Lp++;
				break;
			}

			case '-' :
			{
				if ((Lp + 1 < LpMax) && (Lp[1] == '-'))
				{
					// skip til eol comment
					Lp += 2;
					while (Lp < LpMax)
					{
						if ((*Lp == 'r') || (*Lp == '\n'))
							break;
						else Lp++;
					}
				}
				else Lp++;
				break;
			}
			case '/' :
			{
				if ((Lp + 1 < LpMax) && (Lp[1] == '*'))
				{
					// skip multiline comment
					Lp += 2;
					while (Lp < LpMax)
					{
						if (*Lp == '*')
						{
							if ((Lp + 1 < LpMax) && (Lp[1] == '/'))
								break;
							else Lp++;
						}
						else Lp++;
					}
					if (Lp >= LpMax)
						return false;
					else Lp += 2;
				}
				else Lp++;
				break;
			}

			case '\'':
			{
				// skip single quoted text
				Lp++;
				while (Lp < LpMax)
				{
					if (*Lp == '\'')
					{
						if ((Lp + 1 < LpMax) && (Lp[1] == '\''))
							Lp += 2;
						else break;
					}
					else Lp++;
				}
				if (Lp >= LpMax)
					return false;
				else Lp++;
				break;
			}

			case '"':
			{
				// skip double quoted text
				Lp++;
				char* LpMatchingQuote = FindMatchingDoubleQuote(Lp, LpMax);
				if (LpMatchingQuote == NULL)
					return false;
				else Lp = LpMatchingQuote + 1;
				break;
			}

			case '?' :
			case ':' :
			case '@' :
			{
				// here goes a possibly unnamed param (?),
				// or a named param (:,@), optionally enclosed in double quotes.
				// In a latter case anything between quotes is permitted.
				char LParamDesignator = *Lp;

				// relay whatever is up to this point
				LSubstrLen = (size_t)(Lp - LpStart);
				if (LSubstrLen == 0)
					return false;
				if (0 != strncat_s(this->pPrepearedSQL, len + 1, LpStart, LSubstrLen))
					return false;			
				if (0 != strcat_s(this->pPrepearedSQL, len + 1, "?"))
					return false;

				Lp++;
				LpStart = Lp;

				if (Lp >= LpMax)
				{
					if (LParamDesignator == '?')
					{
						if (!sl_add(ApSL, NULL))
							return false;
						else break;
					}
					else return false;  // need a name
				}

				if (*Lp == '"')
				{
					// quoted param name
					Lp++;
					char* LpMatchingQuote = FindMatchingDoubleQuote(Lp, LpMax);
					if (LpMatchingQuote == NULL)
						return false;
					size_t LParamNameLen = (size_t)(LpMatchingQuote - Lp);

					if (LParamNameLen == 0)
					{
						// quoted unnamed param, gee
						if (!sl_add(ApSL, NULL))
							return false;
					}
					else
					{
						char* T = (char*)malloc(LParamNameLen + 1);
						if (T == NULL)
							return false;
						if (0 != strncpy_s(T, LParamNameLen + 1, Lp, LParamNameLen))
							return false;
						if (!sl_add(ApSL, T))
						{
							free(T);
							return false;
						}
						free(T);
					}

					Lp = LpMatchingQuote + 1;
					LpStart = Lp;
				}
				else if (LValidNameChars[(int)(*Lp)])
				{
					char* LpT = Lp + 1;
					while (LpT < LpMax)
					{
						if (!LValidNameChars[(int)(*LpT)])
							break;
						else LpT++;
					}
					size_t LParamNameLen = (size_t)(LpT - Lp);
					char* T = (char*)malloc(LParamNameLen + 1);
					if (T == NULL)
						return false;
					if (0 != strncpy_s(T, LParamNameLen + 1, Lp, LParamNameLen))
						return false;
					if (!sl_add(ApSL, T))
					{
						free(T);
						return false;
					}
					free(T);

					Lp = LpT;
					LpStart = Lp;
				}
				else
				{
					// unnamed param
					if (LParamDesignator == '?')
					{
						if (!sl_add(ApSL, NULL))
							return false;
						else break;
					}
					else return false;  // need a name
				}
				break;
			}
		}
	}
	// Lp == LpMax here
	// if present, last text part is here from LpStart
	LSubstrLen = (size_t)(Lp - LpStart);
	if (LSubstrLen > 0)
	{
		if (0 != strncat_s(this->pPrepearedSQL, len + 1, LpStart, LSubstrLen))
			return false;
	}

	return true;  
}


void PurgeXSQLDA(XSQLDA** ppVars)
{
	if (*ppVars == NULL)
		return;

	ISC_SHORT i;
	XSQLVAR* LpVar;
	for (i = 0, LpVar = (*ppVars)->sqlvar;
		i < (*ppVars)->sqln;
		i++, LpVar++)
	{
		if (LpVar->sqldata != NULL)
			free(LpVar->sqldata);
		if (LpVar->sqlind != NULL)
			free(LpVar->sqlind);
	}
	free(*ppVars);
	*ppVars = NULL;
}



void fb_close_sql(fb_sql_t* this)
{
	if (this->hStatement)
	{
		if (this->pDatabase)
			g_FBDll.p_isc_dsql_free_statement(this->pDatabase->StatusVector,
				&this->hStatement,
				DSQL_drop);
		this->hStatement = 0;
	}

	PurgeXSQLDA(&this->pParams);
	if (this->pParamNames)
		sht_clear(this->pParamNames);

	t_Name2Param* Lp = this->pName2ParamStorage;
	t_Name2Param* LpD;
	while (Lp != NULL)
	{
		LpD = Lp;
		Lp = Lp->pNextAllocated;
		free(LpD);
	}
	this->pName2ParamStorage = NULL;

	PurgeXSQLDA(&this->pFields);
	if (this->pFieldNames)
		sht_clear(this->pFieldNames);

	this->mEOF = false;
}

#define MY_XSQLDA_LENGTH(n)	(sizeof (XSQLDA) + ((size_t)n - 1) * sizeof (XSQLVAR))

bool ReallocXSQLDA(XSQLDA** ppVars, short VarsCount)
{
	if (*ppVars)
		PurgeXSQLDA(ppVars);

	*ppVars = (XSQLDA*)malloc(MY_XSQLDA_LENGTH(VarsCount));
	if (!*ppVars)
		return false;
	memset(*ppVars, 0, MY_XSQLDA_LENGTH(VarsCount));
	(*ppVars)->version = SQLDA_VERSION1;
	(*ppVars)->sqln = VarsCount;
	return true;
}


bool SetupXSQLDAStorage(XSQLDA* pVars)
{
	if (pVars == NULL)
		return false;
	ISC_SHORT i;
	XSQLVAR* LpVar;
	for (i = 0, LpVar = pVars->sqlvar;
		i < pVars->sqld;
		i++, LpVar++)
	{
		switch (LpVar->sqltype & ~1)
		{
			case SQL_VARYING:
			{
				LpVar->sqldata = malloc(sizeof(char) * (size_t)(LpVar->sqllen) + 2);
				if (!LpVar->sqldata)
					return false;
				break;
			}
			default:
			{
				ISC_SHORT LLen = LpVar->sqllen;
				if (LLen == 0)
					LLen++;
				LpVar->sqldata = (char*)malloc((size_t)LLen);
				if (!LpVar->sqldata)
					return false;
				break;
			}
		}
		if (LpVar->sqltype & 1)
		{
			LpVar->sqlind = (short*)malloc(sizeof(short));
			if (!LpVar->sqlind)
				return false;
			*LpVar->sqlind = -1;  // init to NULL always?
		}
	}

	return true;
}


bool AreCompatibleTypes(XSQLVAR* v1, XSQLVAR* v2)
{
	ISC_SHORT t1 = FIELD_BASE_TYPE(v1);
	ISC_SHORT t2 = FIELD_BASE_TYPE(v2);

	switch (t1)
	{
		case SQL_TEXT :
		case SQL_VARYING :
		{
			return ((t2 == SQL_TEXT) || (t2 == SQL_VARYING));
		}
		case SQL_SHORT :
		case SQL_LONG :
		case SQL_INT64 :
		case SQL_FLOAT :
		case SQL_DOUBLE :
		case SQL_D_FLOAT :
		{
			return ((t2 == SQL_SHORT)
				|| (t2 == SQL_LONG)
				|| (t2 == SQL_INT64)
				|| (t2 == SQL_FLOAT)
				|| (t2 == SQL_DOUBLE)
				|| (t2 == SQL_D_FLOAT));
		}
		case SQL_TIMESTAMP :
		case SQL_TYPE_TIME :
		case SQL_TYPE_DATE :
		{
			return ((t2 == SQL_TIMESTAMP)
				|| (t2 == SQL_TYPE_TIME)
				|| (t2 == SQL_TYPE_DATE));
		}
	}

	return false;
}


bool fb_prepare_sql(fb_sql_t* this)
{
	if (this->pDatabase == NULL)
		return false;

	fb_transaction_t* LpTransaction = this->pTransaction;
	if (LpTransaction == NULL)
		LpTransaction = this->pDatabase->pDefaultTransaction;
	if ((LpTransaction == NULL)
		|| !fb_in_transaction(LpTransaction))
		return false;

	if (fb_is_sql_active(this) || fb_is_sql_prepared(this))
		fb_close_sql(this);

	if (EMPTY_STRING(this->pSQLText))
		return false;

	// param names
	string_list_t* LpSL = sl_create_stringlist();
	if (!PreprocessSQL(this, LpSL))
		goto CLOSE_AND_RETURN_ERROR;

	g_FBDll.p_isc_dsql_allocate_statement(this->pDatabase->StatusVector,
		&this->pDatabase->DBhandle,
		&this->hStatement);
	if (HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector))
		goto CLOSE_AND_RETURN_ERROR;

	g_FBDll.p_isc_dsql_prepare(this->pDatabase->StatusVector,
		&LpTransaction->TRHandle,
		&this->hStatement,
		0,
		this->pPrepearedSQL,
		this->pDatabase->DBdialect, // dialect
		NULL);
	if (HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector))
		goto CLOSE_AND_RETURN_ERROR;

	short LParamsCount = 10;
	while (true)
	{
		if (!ReallocXSQLDA(&this->pParams, LParamsCount))
			goto CLOSE_AND_RETURN_ERROR;

		g_FBDll.p_isc_dsql_describe_bind(this->pDatabase->StatusVector,
			&this->hStatement,
			1,
			this->pParams);
		if (HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector))
			goto CLOSE_AND_RETURN_ERROR;

		if (this->pParams->sqld > this->pParams->sqln)
			LParamsCount = this->pParams->sqld;
		else break;
	}

	if (sl_get_count(LpSL) != this->pParams->sqld)
		goto CLOSE_AND_RETURN_ERROR;

	// param names
	if (this->pParamNames == NULL)
		this->pParamNames = sht_create_strhash(20);
	else sht_clear(this->pParamNames);

	ISC_SHORT i;
	XSQLVAR* LpVar;
	for (i = 0, LpVar = this->pParams->sqlvar; 
		 i < this->pParams->sqld; 
		i++, LpVar++)
	{
		char* LpName = sl_get(LpSL, (unsigned int)i);
		if (!EMPTY_STRING(LpName))
		{
			t_Name2Param* LpOld = NULL;
			sht_get(this->pParamNames, LpName, (void*)(&LpOld));
			// check params' types
			if (LpOld != NULL)
			{
				if (!AreCompatibleTypes(LpOld->pParam,LpVar))
					goto CLOSE_AND_RETURN_ERROR;
			}
			t_Name2Param* LpNew = (t_Name2Param*)malloc(sizeof(t_Name2Param));
			if (LpNew == NULL)
				goto CLOSE_AND_RETURN_ERROR;
			LpNew->pNextAllocated = this->pName2ParamStorage;
			this->pName2ParamStorage = LpNew;
			LpNew->pParam = LpVar;
			LpNew->pNext = LpOld;
			if (LpOld == NULL)
			{
				if (!sht_add(this->pParamNames, LpName, (void*)LpNew))
					goto CLOSE_AND_RETURN_ERROR;
			}
			else
			{
				if (!sht_set_data(this->pParamNames, LpName, (void*)LpNew))
					goto CLOSE_AND_RETURN_ERROR;
			}
		}
	}

	if (!SetupXSQLDAStorage(this->pParams))
		goto CLOSE_AND_RETURN_ERROR;



	sl_free(LpSL);
	return true;

CLOSE_AND_RETURN_ERROR:
	sl_free(LpSL);
	fb_close_sql(this);
	return false;
}

bool fb_debug_exec_immed(fb_sql_t* this)
{
	if (this->pDatabase == NULL)
		return false;

	fb_transaction_t* LpTransaction = this->pTransaction;
	if (LpTransaction == NULL)
		LpTransaction = this->pDatabase->pDefaultTransaction;
	if ((LpTransaction == NULL)
		|| !fb_in_transaction(LpTransaction))
		return false;

	if (fb_is_sql_active(this))
		fb_close_sql(this);

	if (EMPTY_STRING(this->pSQLText))
		return false;

	if (!fb_is_sql_prepared(this))
	{
		if (!fb_prepare_sql(this))
			goto CLOSE_AND_RETURN_ERROR;
	}


	short LFieldsCount = 10;
	while (true)
	{
		if (!ReallocXSQLDA(&this->pFields, LFieldsCount))
			goto CLOSE_AND_RETURN_ERROR;

		g_FBDll.p_isc_dsql_describe(this->pDatabase->StatusVector,
			&this->hStatement,
			1,
			this->pFields);
		if (HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector))
			goto CLOSE_AND_RETURN_ERROR;

		if (this->pFields->sqld > this->pFields->sqln)
			LFieldsCount = this->pFields->sqld;
		else break;
	}

	if (!SetupXSQLDAStorage(this->pFields))
		goto CLOSE_AND_RETURN_ERROR;

	if (this->pFieldNames == NULL)
		this->pFieldNames = sht_create_strhash(50);
	// build field names
	ISC_SHORT i;
	XSQLVAR* LpVar;
	for (i = 0, LpVar = this->pFields->sqlvar;
		i < this->pFields->sqld;
		i++, LpVar++)
	{
		char LName[50];
		if (0 == strncpy_s(LName, sizeof(LName), LpVar->aliasname, (size_t)LpVar->aliasname_length))
			sht_add(this->pFieldNames, LName, LpVar);
		else log_error("FB: get filedname failed in %s", __func__);
	}

    g_FBDll.p_isc_dsql_execute2(this->pDatabase->StatusVector,
        &LpTransaction->TRHandle,
        &this->hStatement,
        1, // sqlda version
        this->pParams,
        this->pFields);
    if (HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector))
        return false;
    this->mEOF = true;

	return true;

CLOSE_AND_RETURN_ERROR:
	fb_close_sql(this);
	return false;  
}

bool fb_exec_sql(fb_sql_t* this)
{
	if (this->pDatabase == NULL)
		return false;

	fb_transaction_t* LpTransaction = this->pTransaction;
	if (LpTransaction == NULL)
		LpTransaction = this->pDatabase->pDefaultTransaction;
	if ((LpTransaction == NULL)
		|| !fb_in_transaction(LpTransaction))
		return false;

	if (fb_is_sql_active(this))
		fb_close_sql(this);

	if (EMPTY_STRING(this->pSQLText))
		return false;

	if (!fb_is_sql_prepared(this))
	{
		if (!fb_prepare_sql(this))
			goto CLOSE_AND_RETURN_ERROR;
	}


	short LFieldsCount = 10;
	while (true)
	{
		if (!ReallocXSQLDA(&this->pFields, LFieldsCount))
			goto CLOSE_AND_RETURN_ERROR;

		g_FBDll.p_isc_dsql_describe(this->pDatabase->StatusVector,
			&this->hStatement,
			1,
			this->pFields);
		if (HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector))
			goto CLOSE_AND_RETURN_ERROR;

		if (this->pFields->sqld > this->pFields->sqln)
			LFieldsCount = this->pFields->sqld;
		else break;
	}

	if (!SetupXSQLDAStorage(this->pFields))
		goto CLOSE_AND_RETURN_ERROR;

	if (this->pFieldNames == NULL)
		this->pFieldNames = sht_create_strhash(50);
	// build field names
	ISC_SHORT i;
	XSQLVAR* LpVar;
	for (i = 0, LpVar = this->pFields->sqlvar;
		i < this->pFields->sqld;
		i++, LpVar++)
	{
		char LName[50];
		if (0 == strncpy_s(LName, sizeof(LName), LpVar->aliasname, (size_t)LpVar->aliasname_length))
			sht_add(this->pFieldNames, LName, LpVar);
		else log_error("FB: get filedname failed in %s", __func__);
	}

	// statement type
	char LRequest = isc_info_sql_stmt_type;
	char LResponse[10] = { 0 };
	g_FBDll.p_isc_dsql_sql_info(this->pDatabase->StatusVector,
		&this->hStatement,
		1,
		&LRequest,
		sizeof(LResponse),
		LResponse);
	if (HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector)
		|| (LResponse[0] != isc_info_sql_stmt_type))
		goto CLOSE_AND_RETURN_ERROR;
	ISC_LONG LStatementType = g_FBDll.p_isc_vax_integer(&LResponse[3],
		(short)g_FBDll.p_isc_vax_integer(&LResponse[1], 2));

	// Following common practise, execution results are handled like this:
	// - for SELECT statements a cursor is open, first fetch is executed,
	//   and a user may read Fields and execute subsequent fetches untill EOF;
	// - for EXECUTE PROCEDURE the statement is executed, result fields
	//   (if any) are placed in Fields, and an EOF is set to prevent
	//   subsequent fetches. INSERT INTO ... RETURNING is treated the same way.
	//   So the user must not check EOF for these statements, but query fields
	//   instead.
	// - any other statement types execute just like EXECUTE PROCEDURE,
	//   but they do not return any fields.

	switch (LStatementType)
	{
		case isc_info_sql_stmt_select :
		case isc_info_sql_stmt_select_for_upd :
		{
			g_FBDll.p_isc_dsql_execute2(this->pDatabase->StatusVector,
				&LpTransaction->TRHandle,
				&this->hStatement,
				1, // sqlda version
				this->pParams,
				NULL);
			if (HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector))
				return false;
			g_FBDll.p_isc_dsql_set_cursor_name(this->pDatabase->StatusVector,
				&this->hStatement,
				"cursor_name",
				0);
			if (HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector))
				return false;

			if (this->pFields->sqld > 0)
				return fb_sql_next(this);
			else
			{
				this->mEOF = true;
				return true;
			}			
		}
		case isc_info_sql_stmt_exec_procedure :
		{
			g_FBDll.p_isc_dsql_execute2(this->pDatabase->StatusVector,
				&LpTransaction->TRHandle,
				&this->hStatement,
				1, // sqlda version
				this->pParams,
				this->pFields);
			if (HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector))
				return false;
			this->mEOF = true;
			break;
		}

		default :
		{
			g_FBDll.p_isc_dsql_execute(this->pDatabase->StatusVector,
				&LpTransaction->TRHandle,
				&this->hStatement,
				1, // sqlda version
				this->pParams);
			if (HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector)) {
				return false;
			}

			this->mEOF = true;
			break;
		}
	}

	return true;

CLOSE_AND_RETURN_ERROR:
	fb_close_sql(this);
	return false;  
}


bool fb_sql_set_transaction(fb_sql_t* this, fb_transaction_t* ApTr)
{
	if (fb_is_sql_active(this))
		return false;
	this->pTransaction = ApTr;
	return true;
}


bool fb_is_eof(fb_sql_t* this)
{
	return this->mEOF;
}


bool fb_sql_next(fb_sql_t* this)
{
	if (!fb_is_sql_active(this) 
		|| this->mEOF
		|| (this->pFields->sqld == 0))
		return false;

	ISC_STATUS nRet = g_FBDll.p_isc_dsql_fetch(this->pDatabase->StatusVector,
		&this->hStatement,
		1, // sqlda version
		this->pFields);
	this->mEOF = false;
	if (nRet != 0)
	{
		if (nRet == 100L)
			this->mEOF = true;
		else if (HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector))
			return false;
	}

	return true;
}


int fb_sql_get_field_count(fb_sql_t* this)
{
	if (this->pFields)
		return this->pFields->sqld;
	else return 0;
}


XSQLVAR* fb_field_by_index(fb_sql_t* this, short Index)
{
	if ((this->pFields == NULL)
		|| (Index < 0)
		|| (Index >= this->pFields->sqld))
		return NULL;
	return &this->pFields->sqlvar[Index];
}


bool CheckPrepared(fb_sql_t* this)
{
	if (fb_is_sql_prepared(this))
		return true;
	return fb_prepare_sql(this);
}


XSQLVAR* ParamByIndex(fb_sql_t* this, short Index)
{
	if (!CheckPrepared(this))
		return NULL;
	if ((this->pParams == NULL)
		|| (Index < 0)
		|| (Index >= this->pParams->sqld))
		return NULL;
	return &this->pParams->sqlvar[Index];
}

t_Name2Param* ParamByName(fb_sql_t* this, char* ApName)
{
	if (!CheckPrepared(this))
		return NULL;
	t_Name2Param* Result = NULL;
	if (sht_get(this->pParamNames, ApName, (void*)&Result))
		return Result;
	else return NULL;
}


XSQLVAR* fb_field_by_name(fb_sql_t* this, char* ApName)
{
	if (this->pFieldNames)
	{
		void* Result = NULL;
		if (sht_get(this->pFieldNames, ApName, &Result))
			return (XSQLVAR*)Result;
	}
	
	return NULL;
}


double AdjustNumericScale(int64_t value, short scale)
{
	int64_t LScaling = 1;
	if (scale > 0)
	{
		for (short i = 1; i <= scale; i++)
			LScaling *= 10;
		return (double)((double)value * (double)LScaling);
	}
	else if (scale < 0)
	{
		for (short i = -1; i >= scale; i--)
			LScaling *= 10;
		return (double)((double)value / (double)LScaling);
	}
	else return (double)value;
}


// -------------------------------------------------------------------------------
//  Fields / Params

bool fb_field_is_null(XSQLVAR* ApField)
{
	return FIELD_IS_NULL(ApField);
}


static bool SetParamAsDateTime(XSQLVAR* ApParam, struct tm* pVal, int milliseconds) {
	if (ApParam == NULL) return false;

	switch (FIELD_BASE_TYPE(ApParam))
	{
		default: return false;

		case SQL_TIMESTAMP:
		{
			ISC_TIMESTAMP LValue;
			g_FBDll.p_isc_encode_timestamp(pVal, &LValue);
			if (milliseconds > 1000)
				milliseconds %= 1000;
			LValue.timestamp_time += (ISC_TIME)(milliseconds * 10);
			*((ISC_TIMESTAMP*)(ApParam->sqldata)) = LValue;
			break;
		}
	}
	if (IS_NULLABLE(ApParam))
		*(short*)ApParam->sqlind = 0;
	return true; 
}

static bool SetParamAsEpochTimeT(XSQLVAR* ApParam, time_t Val, int milliseconds) {
	struct tm _tm = { 0 };
#ifdef _WIN32
	if (0 != gmtime_s(&_tm, &Val))
		return false;
#else
	gmtime_r(&Val, &_tm);
#endif
	return SetParamAsDateTime(ApParam, &_tm, milliseconds);
}

static bool SetParamAsTimestamp(XSQLVAR* ApParam, timestamp_t* ts) {
	struct tm _tm;
#ifdef _WIN32
	if (0 != gmtime_s(&_tm, &ts->tv_sec))
		return false;
#else
	time_t* t = (time_t*)(&ts->tv_sec);
	gmtime_r(t, &_tm);
#endif
	return SetParamAsDateTime(ApParam, &_tm, (int)(ts->tv_usec / 1000));
}

static bool SetParamAsBigInt(XSQLVAR* ApParam, int64_t ApValue) {
	if (ApParam == NULL) return false;

	switch (FIELD_BASE_TYPE(ApParam))
	{
	default: return false;
	case SQL_INT64:
	{
		*((int64_t*)(ApParam->sqldata)) = ApValue;
		break;
	}
	case SQL_FLOAT:
	{
		*((float*)(ApParam->sqldata)) = (float)ApValue;
		break;
	}
	case SQL_DOUBLE:
	case SQL_D_FLOAT:
	{
		*((double*)(ApParam->sqldata)) = (double)ApValue;
		break;
	}
	// TODO maybe - SQL_TEXT,SQL_VARYING
	}

	ApParam->sqlscale = 0;
	if (IS_NULLABLE(ApParam))
		*(short*)ApParam->sqlind = 0;

	return true;
}

static bool SetParamAsInt(XSQLVAR* ApParam, int ApValue)
{
	if (ApParam == NULL) return false;

	switch (FIELD_BASE_TYPE(ApParam))
	{
		default: return false;
		case SQL_SHORT:
		{
			*((short*)(ApParam->sqldata)) = (short)ApValue;
			break;
		}
		case SQL_LONG:
		{
			*((int32_t*)(ApParam->sqldata)) = (int32_t)ApValue;
			break;
		}
		case SQL_INT64:
		{
			*((int64_t*)(ApParam->sqldata)) = (int64_t)ApValue;
			break;
		}
		case SQL_FLOAT:
		{
			*((float*)(ApParam->sqldata)) = (float)ApValue;
			break;
		}
		case SQL_DOUBLE:
		case SQL_D_FLOAT:
		{
			*((double*)(ApParam->sqldata)) = (double)ApValue;
			break;
		}
		// TODO maybe - SQL_TEXT,SQL_VARYING
	}

	ApParam->sqlscale = 0;
	if (IS_NULLABLE(ApParam))
		*(short*)ApParam->sqlind = 0;

	return true;
}

bool SetParamAsNULL(XSQLVAR* ApParam)
{
	if ((ApParam == NULL) || !IS_NULLABLE(ApParam))
		return false;
	*(short*)ApParam->sqlind = -1;
	return true;
}

bool BeginParamAsBlob(fb_sql_t* this, XSQLVAR* ApParam, isc_blob_handle* phBlob) {
	if ((ApParam == NULL)
		|| (FIELD_BASE_TYPE(ApParam) != SQL_BLOB))
		return false;

	fb_transaction_t* LpTransaction = this->pTransaction;
	if (LpTransaction == NULL)
		LpTransaction = this->pDatabase->pDefaultTransaction;
	if ((LpTransaction == NULL)
		|| !fb_in_transaction(LpTransaction))
		return false;

	*phBlob = 0;
	ISC_QUAD LBlobID = { 0 };

	g_FBDll.p_isc_create_blob2(this->pDatabase->StatusVector,
		&this->pDatabase->DBhandle,
		&LpTransaction->TRHandle,
		phBlob,
		&LBlobID,
		0,
		NULL);
	if (HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector))
		return false;

	// update param blobid
	*(ISC_QUAD*)ApParam->sqldata = LBlobID;

	if (IS_NULLABLE(ApParam))
		*(short*)ApParam->sqlind = 0;

	return true;
}

bool fb_begin_param_as_blob(fb_sql_t* this, short Index, isc_blob_handle* phBlob) {
	return BeginParamAsBlob(this, ParamByIndex(this, Index), phBlob);
}

bool fb_sql_write_blob_segment(fb_sql_t* this, isc_blob_handle hBlob, char* ApBuf, 
    size_t ASize) 
{
	// do write blob
	char* LpCurr = ApBuf;
	size_t LTotalBytesWritten = 0;
	unsigned short LChunkSize = DEFAULT_BLOB_CHUNK_SIZE;
	while (LTotalBytesWritten < ASize)
	{
		if (LTotalBytesWritten + LChunkSize > ASize)
			LChunkSize = (unsigned short)(ASize - LTotalBytesWritten);
		if (g_FBDll.p_isc_put_segment(this->pDatabase->StatusVector,
			&hBlob,
			LChunkSize,
			LpCurr) != 0)
			goto CLEANUP_AND_FAIL;
		LpCurr += LChunkSize;
		LTotalBytesWritten += LChunkSize;
	}
//	g_FBDll.p_isc_close_blob(this->pDatabase->StatusVector, &hBlob);
	return !(HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector));

CLEANUP_AND_FAIL:
	g_FBDll.p_isc_close_blob(this->pDatabase->StatusVector, &hBlob);
	return false;
}

bool fb_end_param_as_blob(fb_sql_t* this, isc_blob_handle hBlob) {
	g_FBDll.p_isc_close_blob(this->pDatabase->StatusVector, &hBlob);
	if (HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector))
		return false;

	return true; 
}
 
bool fb_create_blob(fb_database_t *db, fb_transaction_t *transaction, 
    isc_blob_handle *hblob, ISC_QUAD *blobid)
{
    ISC_STATUS StatusVector[20];
	g_FBDll.p_isc_create_blob2(StatusVector,
		&db->DBhandle,
		&transaction->TRHandle,
		hblob,
		blobid,
		0,
		NULL);
	return !HAS_STATUS_VECTOR_ERRORS(StatusVector);
}

bool fb_write_blob_segment(isc_blob_handle *hblob, char *data,
    size_t size)
{
    ISC_STATUS StatusVector[20];
    char *p = data;
	size_t total_bytes_written = 0;
	unsigned short chunk_size = DEFAULT_BLOB_CHUNK_SIZE;
	while (total_bytes_written < size)
	{
		if (total_bytes_written + chunk_size > size)
			chunk_size = (unsigned short)(size - total_bytes_written);
		if (g_FBDll.p_isc_put_segment(StatusVector,
			hblob,
			chunk_size,
			p) != 0)
			return false;
		p += chunk_size;
		total_bytes_written += chunk_size;
	}
    return !HAS_STATUS_VECTOR_ERRORS(StatusVector);
}    

void fb_log_error(ISC_STATUS* sv);  // forward

bool fb_read_blob_segment(isc_blob_handle *hblob, char *buf,
    size_t size, size_t *read_bytes)
{
    ISC_STATUS StatusVector[20];
	char *pcurr = buf;
	size_t total_bytes_read = 0;
	unsigned short chunk_size = DEFAULT_BLOB_CHUNK_SIZE;
	unsigned short bytes_read = 0;
	while (total_bytes_read < size) {
		if (total_bytes_read + chunk_size > size)
			chunk_size = (unsigned short)(size - total_bytes_read);
		g_FBDll.p_isc_get_segment(StatusVector,
			hblob,
			&bytes_read,
			chunk_size,
			pcurr);
		if (HAS_STATUS_VECTOR_ERRORS(StatusVector)) {
            if (StatusVector[1] != isc_segment) {
                fb_log_error(StatusVector);
                return false;
            }
        }
			
		pcurr += bytes_read;
		total_bytes_read += bytes_read;
		bytes_read = 0;
	}
    *read_bytes = total_bytes_read;
    return true;
}

bool fb_close_blob(isc_blob_handle *hblob) {
    ISC_STATUS StatusVector[20];
    g_FBDll.p_isc_close_blob(StatusVector, hblob);
    return !HAS_STATUS_VECTOR_ERRORS(StatusVector);
}

bool fb_cancel_blob(isc_blob_handle *hblob) {
    ISC_STATUS StatusVector[20];
    g_FBDll.p_isc_cancel_blob(StatusVector, hblob);
    return !HAS_STATUS_VECTOR_ERRORS(StatusVector);
}

bool set_param_as_blobid(fb_sql_t *query, XSQLVAR *param, ISC_QUAD *blobID)
{
	if ((param == NULL)
		|| (FIELD_BASE_TYPE(param) != SQL_BLOB))
		return false;

    *(ISC_QUAD*)param->sqldata = *blobID;
	if (IS_NULLABLE(param))
		*(short*)param->sqlind = 0;

	return true;
}

bool fb_set_param_as_blobid(fb_sql_t *query, short index, ISC_QUAD *blobID) {
    return set_param_as_blobid(query, ParamByIndex(query, index), blobID);
}

bool open_field_blob(fb_sql_t *query, XSQLVAR *field, isc_blob_handle *hblob,
    size_t *blob_size) 
{
	if ((field == NULL) 
        || (FIELD_BASE_TYPE(field) != SQL_BLOB)
        || FIELD_IS_NULL(field))
		return false;

	fb_transaction_t* LpTransaction = query->pTransaction;
	if (LpTransaction == NULL)
		LpTransaction = query->pDatabase->pDefaultTransaction;
	if ((LpTransaction == NULL)
		|| !fb_in_transaction(LpTransaction))  
		return false;

	*hblob = 0;

	g_FBDll.p_isc_open_blob2(query->pDatabase->StatusVector,
		&query->pDatabase->DBhandle,
		&LpTransaction->TRHandle,
		hblob,
		(ISC_QUAD*)field->sqldata,
		0,
		NULL);
	if (HAS_STATUS_VECTOR_ERRORS(query->pDatabase->StatusVector))
		return false;

	size_t LBlobSize = 0;
	char LRequest = isc_info_blob_total_length;
	char LResult[50];
	g_FBDll.p_isc_blob_info(query->pDatabase->StatusVector,
		hblob,
		1, // size of request
		&LRequest,
		sizeof(LResult), // size of results
		LResult);
	if (HAS_STATUS_VECTOR_ERRORS(query->pDatabase->StatusVector))
		goto CLOSE_AND_FAIL;

	bool LSizeFound = false;
	int i = 0;
	while (i < sizeof(LResult))
	{
		char LItem = LResult[i++];
		if (LItem == isc_info_end)
			break;
		ISC_LONG LItemLen = g_FBDll.p_isc_vax_integer(&LResult[i], 2);
		i += 2;
		if (LItem == isc_info_blob_total_length)
		{
			LBlobSize = (size_t)g_FBDll.p_isc_vax_integer(&LResult[i], (short)LItemLen);
			LSizeFound = true;
			break;
		}
	}
	if (!LSizeFound) goto CLOSE_AND_FAIL;     
    *blob_size = LBlobSize;
    return true;

CLOSE_AND_FAIL:
	g_FBDll.p_isc_close_blob(query->pDatabase->StatusVector, hblob);
	return false;
}

bool fb_open_field_blob(fb_sql_t *query, short field_idx, isc_blob_handle *hblob,
    size_t *blob_size) 
{
    XSQLVAR *field = fb_field_by_index(query, field_idx);
    return open_field_blob(query, field, hblob, blob_size);
}

bool SetParamAsBlob(fb_sql_t* this, XSQLVAR* ApParam, char* ApBuf, size_t ASize)
{
	if ((ApParam == NULL)
		|| (ASize == 0)
		|| (FIELD_BASE_TYPE(ApParam) != SQL_BLOB))
		return false;

	fb_transaction_t* LpTransaction = this->pTransaction;
	if (LpTransaction == NULL)
		LpTransaction = this->pDatabase->pDefaultTransaction;
	if ((LpTransaction == NULL)
		|| !fb_in_transaction(LpTransaction))
		return false;

	isc_blob_handle LhBlob = 0;
	ISC_QUAD LBlobID = { 0 };

	g_FBDll.p_isc_create_blob2(this->pDatabase->StatusVector,
		&this->pDatabase->DBhandle,
		&LpTransaction->TRHandle,
		&LhBlob,
		&LBlobID,
		0,
		NULL);
	if (HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector))
		return false;
	 
	// do write blob
	char* LpCurr = ApBuf;
	size_t LTotalBytesWritten = 0;
	unsigned short LChunkSize = DEFAULT_BLOB_CHUNK_SIZE;
	while (LTotalBytesWritten < ASize)
	{
		if (LTotalBytesWritten + LChunkSize > ASize)
			LChunkSize = (unsigned short)(ASize - LTotalBytesWritten);
		if (g_FBDll.p_isc_put_segment(this->pDatabase->StatusVector,
				&LhBlob,
				LChunkSize,
				LpCurr) != 0)
			goto CLEANUP_AND_FAIL;
		LpCurr += LChunkSize;
		LTotalBytesWritten += LChunkSize;
	}
	g_FBDll.p_isc_close_blob(this->pDatabase->StatusVector, &LhBlob);
	if (HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector))
		return false;

	// update param blobid
	*(ISC_QUAD*)ApParam->sqldata = LBlobID;

	if (IS_NULLABLE(ApParam))
		*(short*)ApParam->sqlind = 0;

	return true;

CLEANUP_AND_FAIL:
	g_FBDll.p_isc_close_blob(this->pDatabase->StatusVector, &LhBlob);
	return false;
}


bool SetParamAsString(fb_sql_t* this, XSQLVAR* ApParam, char* s)
{
	if ((ApParam == NULL) || EMPTY_STRING(s))
		return false;

	switch (FIELD_BASE_TYPE(ApParam))
	{
		default: return false;

		case SQL_TEXT :
		{
			int LStrLen = (int)strlen(s);
			if (LStrLen > ApParam->sqllen)
				LStrLen = ApParam->sqllen;
			memcpy(ApParam->sqldata, s, (size_t)LStrLen);
			ApParam->sqllen = (ISC_SHORT)LStrLen;
			break;
		}
		case SQL_VARYING :
		{
			short len = (short)strlen(s);
			*(short*)ApParam->sqldata = len;
			strncpy_s((char*)ApParam->sqldata + 2, (size_t)(ApParam->sqllen), s, (size_t)len);
			break;
		}
		case SQL_BLOB :
		{
			return SetParamAsBlob(this, ApParam, s, strlen(s));
		}

	}
	if (IS_NULLABLE(ApParam)) 
		*(short*)ApParam->sqlind = 0;
	return true;
}


bool fb_set_param_as_string(fb_sql_t* this, short Index, char* s)
{
	if (EMPTY_STRING(s))
		return fb_set_param_as_null(this, Index);

	XSQLVAR* LpParam = ParamByIndex(this, Index);
	if (!LpParam)
		return false;
	return SetParamAsString(this,LpParam, s);
}

bool fb_set_param_as_timestamp(fb_sql_t* this, short Index, timestamp_t* ts) {
	XSQLVAR* LpParam = ParamByIndex(this, Index);
	if (!LpParam)
		return false;
	return SetParamAsTimestamp(LpParam, ts);
}

bool fb_set_param_as_int(fb_sql_t* this, short Index, int AValue)
{
	XSQLVAR* LpParam = ParamByIndex(this,Index);
	if (!LpParam)
		return false;
	return SetParamAsInt(LpParam, AValue);
}


bool fb_set_param_as_null(fb_sql_t* this, short Index)
{
	return SetParamAsNULL(ParamByIndex(this, Index));
}


bool fb_set_param_as_blob(fb_sql_t* this, short Index, char* ApBuf, size_t ASize)
{
	return SetParamAsBlob(this, ParamByIndex(this, Index), ApBuf, ASize);
}


bool fb_set_param_as_epoch_time(fb_sql_t* this, short Index, time_t Val, int milliseconds) {
	return SetParamAsEpochTimeT(ParamByIndex(this, Index), Val, milliseconds);
}

bool fb_set_pbn_as_timestamp(fb_sql_t* this, char* Name, timestamp_t* ts) {
	t_Name2Param* Lpp = ParamByName(this, Name);
	if (Lpp == NULL)
		return false;
	while (Lpp)
	{
		if (!SetParamAsTimestamp(Lpp->pParam, ts))
			return false;
		Lpp = Lpp->pNext;
	}
	return true;

}

bool fb_set_pbn_as_int(fb_sql_t* this, char* Name, int AValue)
{
	t_Name2Param* Lpp = ParamByName(this, Name);
	if (Lpp == NULL)
		return false;
	while (Lpp)
	{
		if (!SetParamAsInt(Lpp->pParam, AValue))
			return false;
		Lpp = Lpp->pNext;
	}
	return true;
}

bool fb_set_pbn_as_bigint(fb_sql_t* this, char* Name, int64_t AValue) {
	t_Name2Param* Lpp = ParamByName(this, Name);
	if (Lpp == NULL)
		return false;
	while (Lpp)
	{
		if (!SetParamAsBigInt(Lpp->pParam, AValue))
			return false;
		Lpp = Lpp->pNext;
	}
	return true;
}


bool fb_set_pbn_as_null(fb_sql_t* this, char* Name)
{
	t_Name2Param* Lpp = ParamByName(this, Name);
	if (Lpp == NULL)
		return false;
	while (Lpp)
	{
		if (!SetParamAsNULL(Lpp->pParam))
			return false;
		Lpp = Lpp->pNext;
	}
	return true;
}


bool fb_set_pbn_as_blob(fb_sql_t* this, char* Name, char* ApBuf, size_t ASize)
{
	t_Name2Param* Lpp = ParamByName(this, Name);
	if ((Lpp == NULL) || (Lpp->pNext != NULL))
		return false;
	return SetParamAsBlob(this, Lpp->pParam, ApBuf, ASize);
}


bool fb_set_pbn_as_string(fb_sql_t* this, char* Name, char* s)
{
	if (EMPTY_STRING(s))
		return fb_set_pbn_as_null(this, Name);

	t_Name2Param* Lpp = ParamByName(this, Name);
	if ((Lpp == NULL) || (Lpp->pNext != NULL))
		return false;
	return SetParamAsString(this, Lpp->pParam, s);

}


bool fb_field_try_as_datetime(XSQLVAR* ApField, struct tm* ApValue, int* millis)
{
	if ((ApField == NULL) || FIELD_IS_NULL(ApField))
		return false;

	ISC_TIMESTAMP LValue;

	switch (FIELD_BASE_TYPE(ApField))
	{
		default: return false;

		case SQL_TIMESTAMP:
		{
			LValue = *((ISC_TIMESTAMP*)(ApField->sqldata));
			g_FBDll.p_isc_decode_timestamp(&LValue, ApValue);
			if (millis)
				*millis = (int)(LValue.timestamp_time) % 10000 / 10;
			break;
		}
		
	}
	return true;
}

// TODO -- works only for date+time, doesnt work for date!
bool fb_field_try_as_timestamp(XSQLVAR* ApField, timestamp_t* ts) {
	struct tm LTm = { 0 };
	int LMillis = 0;
	bool result = fb_field_try_as_datetime(ApField, &LTm, &LMillis);
	if (result)
		tm_from_struct_tm(ts, &LTm, LMillis);
	return result;
}


bool fb_field_try_as_int(XSQLVAR* ApField, int* ApValue)
{
	if ((ApField == NULL) || FIELD_IS_NULL(ApField))
		return false;

	double LValue = 0;

	switch (FIELD_BASE_TYPE(ApField))
	{
	default: return false;

	case SQL_SHORT:
	{
		LValue = round(AdjustNumericScale((int64_t) *((int16_t*)(ApField->sqldata)),
			ApField->sqlscale));
		break;
	}

	case SQL_LONG:
	{
		LValue = round(AdjustNumericScale((int64_t) *((int32_t*)(ApField->sqldata)),
			ApField->sqlscale));
		break;
	}
	case SQL_INT64:
	{
		LValue = round(AdjustNumericScale((int64_t) *((int64_t*)(ApField->sqldata)),
			ApField->sqlscale));
		break;
	}
	case SQL_FLOAT:
	{
		LValue = round(*(float*)(ApField->sqldata));
		break;
	}
	case SQL_DOUBLE:
	case SQL_D_FLOAT:
	{
		LValue = round(*(double*)(ApField->sqldata));
		break;
	}
	case SQL_TEXT:
	{
		// hope nobody uses this shit
		char* LpStop = NULL;
		LValue = strtod(ApField->sqldata, &LpStop);
		if ((LpStop == NULL) || (*LpStop != 0))
			return false;
		break;
	}
	case SQL_VARYING:
	{
		size_t len = (size_t)g_FBDll.p_isc_vax_integer(ApField->sqldata, 2);
		if (len < 1)
			return false;
		char* T = (char*)malloc(len + 1);
		if (T == NULL)
			return false;
		if (0 != strncpy_s(T, len + 1, ApField->sqldata + 2, len))
		{
			free(T);
			return false;
		}
		char* LpStop = NULL;
		LValue = strtod(T, &LpStop);
		if ((LpStop == NULL) || (*LpStop != 0))
		{
			free(T);
			return false;
		}
		free(T);
		break;
	}
	}
	*ApValue = (int)LValue;

	return (LValue > ((double)INT_MIN + 1)) && (LValue <= (double)INT_MAX);
}

bool fb_field_try_as_double(XSQLVAR *ApField, double *ApValue)
{
	if ((ApField == NULL) || FIELD_IS_NULL(ApField))
		return false;

	double LValue = 0;

	switch (FIELD_BASE_TYPE(ApField))
	{
	default: return false;

	case SQL_SHORT:
	{
		LValue = AdjustNumericScale((int64_t) *((int16_t*)(ApField->sqldata)),
			ApField->sqlscale);
		break;
	}

	case SQL_LONG:
	{
		LValue = AdjustNumericScale((int64_t) *((int32_t*)(ApField->sqldata)),
			ApField->sqlscale);
		break;
	}
	case SQL_INT64:
	{
		LValue = AdjustNumericScale((int64_t) *((int64_t*)(ApField->sqldata)),
			ApField->sqlscale);
		break;
	}
	case SQL_FLOAT:
	{
		LValue = *(float*)(ApField->sqldata);
		break;
	}
	case SQL_DOUBLE:
	case SQL_D_FLOAT:
	{
		LValue = *(double*)(ApField->sqldata);
		break;
	}
	case SQL_TEXT:
	{
		// hope nobody uses this shit
		char* LpStop = NULL;
		LValue = strtod(ApField->sqldata, &LpStop);
		if ((LpStop == NULL) || (*LpStop != 0))
			return false;
		break;
	}
	case SQL_VARYING:
	{
		size_t len = (size_t)g_FBDll.p_isc_vax_integer(ApField->sqldata, 2);
		if (len < 1)
			return false;
		char* T = (char*)malloc(len + 1);
		if (T == NULL)
			return false;
		if (0 != strncpy_s(T, len + 1, ApField->sqldata + 2, len))
		{
			free(T);
			return false;
		}
		char* LpStop = NULL;
		LValue = strtod(T, &LpStop);
		if ((LpStop == NULL) || (*LpStop != 0))
		{
			free(T);
			return false;
		}
		free(T);
		break;
	}
	}
	*ApValue = LValue;

	return true;
}

bool fb_field_try_as_string_realloc(XSQLVAR* ApField, char** ApValue)
{
	if ((ApField == NULL) || FIELD_IS_NULL(ApField))
		return false;
	switch (FIELD_BASE_TYPE(ApField))
	{
		default: return false;

		case SQL_TEXT:
		{
			char* T = (char*)realloc(*ApValue,(size_t)(ApField->sqllen) + 1);
			if (T == NULL)
				return false;
			if (ApField->sqllen == 0)
				*T = 0;
			else
			{
#ifdef _WIN32
				if (0 != strncpy_s(T, (size_t)(ApField->sqllen) + 1, ApField->sqldata, ApField->sqllen))
				{
					free(T);
					*ApValue = NULL;
					return false;
				}
#else
				strncpy_s(T, (size_t)(ApField->sqllen) + 1, ApField->sqldata, (size_t)(ApField->sqllen));
#endif
			}
			*ApValue = T;
			return true;
		}
		case SQL_VARYING:
		{
			size_t len = (size_t)g_FBDll.p_isc_vax_integer(ApField->sqldata, 2);
			char* T = (char*)realloc(*ApValue, len + 1);
			if (T == NULL)
				return false;
			if (len == 0)
				*T = 0;
			else
			{
				if (0 != strncpy_s(T, len + 1, ApField->sqldata + 2, len))
				{
					free(T);
					*ApValue = NULL;
					return false;
				}
			}
			*ApValue = T;
			return true;
		}
	}
	return false;
}


bool ReadFieldBlobReAlloc(fb_sql_t* this, XSQLVAR* ApField,
	char** ApBuf, size_t* ApSize)
{
	if ((ApField == NULL) || (FIELD_BASE_TYPE(ApField) != SQL_BLOB))
		return false;
	if (FIELD_IS_NULL(ApField))
	{
		*ApSize = 0;
		return true;
	}

	fb_transaction_t* LpTransaction = this->pTransaction;
	if (LpTransaction == NULL)
		LpTransaction = this->pDatabase->pDefaultTransaction;
	if ((LpTransaction == NULL)
		|| !fb_in_transaction(LpTransaction))  
		return false;

	isc_blob_handle LhBlob = 0;

	g_FBDll.p_isc_open_blob2(this->pDatabase->StatusVector,
		&this->pDatabase->DBhandle,
		&LpTransaction->TRHandle,
		&LhBlob,
		(ISC_QUAD*)ApField->sqldata,
		0,
		NULL);
	if (HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector))
		return false;

	size_t LBlobSize = 0;
	char LRequest = isc_info_blob_total_length;
	char LResult[50];
	g_FBDll.p_isc_blob_info(this->pDatabase->StatusVector,
		&LhBlob,
		1, // size of request
		&LRequest,
		sizeof(LResult), // size of results
		LResult);
	if (HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector))
		goto CLOSE_AND_FAIL;

	bool LSizeFound = false;
	int i = 0;
	while (i < sizeof(LResult))
	{
		char LItem = LResult[i++];
		if (LItem == isc_info_end)
			break;
		ISC_LONG LItemLen = g_FBDll.p_isc_vax_integer(&LResult[i], 2);
		i += 2;
		if (LItem == isc_info_blob_total_length)
		{
			LBlobSize = (size_t)g_FBDll.p_isc_vax_integer(&LResult[i], (short)LItemLen);
			LSizeFound = true;
			break;
		}
	}
	if (!LSizeFound)
		goto CLOSE_AND_FAIL;

	char* T = (char*)realloc(*ApBuf, LBlobSize);
	if (T == NULL)
		goto CLOSE_AND_FAIL;
	*ApBuf = T;
	*ApSize = LBlobSize;

	// do read
	char* LpCurr = *ApBuf;
	size_t LTotalBytesRead = 0;
	unsigned short LChunkSize = DEFAULT_BLOB_CHUNK_SIZE;
	unsigned short LBytesRead = 0;
	while (LTotalBytesRead < LBlobSize)
	{
		if (LTotalBytesRead + LChunkSize > LBlobSize)
			LChunkSize = (unsigned short)(LBlobSize - LTotalBytesRead);
		g_FBDll.p_isc_get_segment(this->pDatabase->StatusVector,
			&LhBlob,
			&LBytesRead,
			LChunkSize,
			LpCurr);
		if (HAS_STATUS_VECTOR_ERRORS(this->pDatabase->StatusVector)) {
            if (this->pDatabase->StatusVector[1] != isc_segment) {
                fb_log_error(this->pDatabase->StatusVector);
                goto CLOSE_AND_FAIL;
            }
        }			
		LpCurr += LBytesRead;
		LTotalBytesRead += LBytesRead;
		LBytesRead = 0;
	}

	g_FBDll.p_isc_close_blob(this->pDatabase->StatusVector, &LhBlob);
	return true;

CLOSE_AND_FAIL:
	g_FBDll.p_isc_close_blob(this->pDatabase->StatusVector, &LhBlob);
	return false;
}


bool fb_read_field_blob_realloc(fb_sql_t* this, short FieldIdx,
	char** ApBuf, size_t* ApSize)
{
	XSQLVAR* LpField = fb_field_by_index(this, FieldIdx);
	return ReadFieldBlobReAlloc(this, LpField, ApBuf, ApSize);
}


bool fb_read_fbn_blob_realloc(fb_sql_t* this, char* ApFieldName,
	char** ApBuf, size_t* ApSize)
{
	XSQLVAR* LpField = fb_field_by_name(this, ApFieldName);
	return ReadFieldBlobReAlloc(this, LpField, ApBuf, ApSize);
}


bool fb_try_field_is_null(fb_sql_t* this, short FieldIdx, bool* ApValue)
{
	XSQLVAR* LpFld = fb_field_by_index(this, FieldIdx);
	if (LpFld == NULL)
		return false;  
	*ApValue = fb_field_is_null(LpFld);
	return true;
}


bool fb_try_field_as_int(fb_sql_t* this, short FieldIdx, int* ApValue)
{
	return fb_field_try_as_int(fb_field_by_index(this, FieldIdx), ApValue);
}


bool fb_try_field_as_datetime(fb_sql_t* this, short FieldIdx, struct tm* ApValue, int* millis)
{
	return fb_field_try_as_datetime(fb_field_by_index(this, FieldIdx), ApValue, millis);
}

bool fb_try_field_as_timestamp(fb_sql_t* this, short FieldIdx, timestamp_t* ts) {
	struct tm LTm = { 0 };
	int LMillis = 0;
	bool result = fb_try_field_as_datetime(this, FieldIdx, &LTm, &LMillis);
	if (result)
		tm_from_struct_tm(ts, &LTm, LMillis);
	return result;
}


bool fb_try_field_as_string_realloc(fb_sql_t* this, short FieldIdx, char** ApValue)
{
	return fb_field_try_as_string_realloc(fb_field_by_index(this, FieldIdx), ApValue);
}



// The following <Get..> routines return default 'empty' values 
// (0,0.0,false,'', etc) upon ANY error:
// - field does not exist;
// - field is NULL;
// - field type incompatible with result;
// - coerced result value overflow, etc.
// Use <TryGet..> routines for more descrete operation.
// Use with caution.
bool fb_get_field_is_null(fb_sql_t* this, short FieldIdx)
{
	bool Result = false;
	if (!fb_try_field_is_null(this, FieldIdx, &Result))
		return false;
	else return Result;
}


int fb_field_as_int(fb_sql_t* this, short FieldIdx)
{
	int Result = 0;
	if (!fb_try_field_as_int(this, FieldIdx, &Result))
		return 0;
	else return Result;
}

bool fb_fbn_is_null(fb_sql_t* this, char* ApFieldName) {
	XSQLVAR* LpFld = fb_field_by_name(this, ApFieldName);
	return (LpFld && fb_field_is_null(LpFld));
}


int fb_fbn_as_int(fb_sql_t* this, char* ApFieldName)
{
	XSQLVAR* LpFld = fb_field_by_name(this, ApFieldName);
	int Result = 0;
	if (fb_field_try_as_int(LpFld, &Result))
		return Result;
	else return 0;
}

double fb_fbn_as_double(fb_sql_t *this, char *ApFieldName)
{
	XSQLVAR* LpFld = fb_field_by_name(this, ApFieldName);
	double Result = 0;
	if (fb_field_try_as_double(LpFld, &Result))
		return Result;
	else return 0;
}

bool fb_fbn_as_string_realloc(fb_sql_t* this, char* ApFieldName,
	char** ApValue)
{
	XSQLVAR* LpFld = fb_field_by_name(this, ApFieldName);
	if (fb_field_try_as_string_realloc(LpFld, ApValue))
		return true;

	free(*ApValue);
	*ApValue = NULL;
	return true;
}


bool fb_try_fbn_as_timestamp(fb_sql_t* this, char* ApFieldName, timestamp_t* ts) {
	XSQLVAR* LpFld = fb_field_by_name(this, ApFieldName);
	return fb_field_try_as_timestamp(LpFld, ts);
}

void fb_log_db_error(fb_database_t *db) {
    char err_msg[1024] = { 0 };
    if (fb_get_last_error(db, err_msg, 1024, "; "))
        log_error("FB: fb_get_last_error() on \"%s\" db returns: %s", 
            db->tag, err_msg);
    else log_error("FB: fb_get_last_error() on \"%s\" db failed.");    
}

static bool interprete_error(ISC_STATUS* sv, char* ApBuf, size_t ABufSize, 
    char* ASeparatorStr)
{
	char LBuf[512];
	ISC_STATUS* LpStatusVector = sv;
	ISC_LONG nRet;
	while (true)
	{
		nRet = g_FBDll.p_isc_interprete(LBuf, &LpStatusVector);
		if (nRet)
		{
			if (0 != strcat_s(ApBuf, ABufSize, LBuf))
				return false;
			if (!EMPTY_STRING(ASeparatorStr))
			{
				if (0 != strcat_s(ApBuf, ABufSize, ASeparatorStr))
					return false;
			}
		}
		else return true;
	}
	return true;
}

void fb_log_error(ISC_STATUS* sv) {
    char err_msg[1024] = { 0 };
    if (interprete_error(sv, err_msg, 1024, "; "))
        log_error("fb_log_error(): %s", err_msg);
    else log_error("fb_log_error(): interprete_error() failed.");   
}
