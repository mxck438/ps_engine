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

#include "my_sum_args.h"

#include <thread>
#include <chrono>

// debug
//#define EVAL_LOCALLY

#ifndef EVAL_LOCALLY
#include "fb_iface.h"
#endif

typedef struct int_val_ {
    int value;
    bool is_null;
} int_val_t;

#ifdef EVAL_LOCALLY
static int_val_t perform_my_sum_args(int_val_t n1, int_val_t n2, int_val_t n3, 
    ::Firebird::ThrowStatusWrapper *status, 
    ::Firebird::IExternalContext *context)
{
    int_val_t result;
    result.is_null = n1.is_null || n2.is_null || n3.is_null;
    if (!result.is_null)
        result.value = n1.value + n2.value + n3.value;
    return result;
};

#else
static int_val_t perform_my_sum_args(int_val_t n1, int_val_t n2, int_val_t n3, 
    ::Firebird::ThrowStatusWrapper *status, 
    ::Firebird::IExternalContext *context)
{
    int_val_t result;
    result.is_null = true;

    IFBQuery query(context, status);
    if (query.prepare("select cast(? as integer) + cast(? as integer) + " \
        "cast(? as integer) res from RDB$DATABASE")) {
        n1.is_null ? query.setParamAsNull(0) : query.setParamAsInt(0, n1.value);
        n2.is_null ? query.setParamAsNull(1) : query.setParamAsInt(1, n2.value);
        n3.is_null ? query.setParamAsNull(2) : query.setParamAsInt(2, n3.value);

        if (query.openCursor()) {
            if (!query.getFieldIsNull(0))
            {
                result.value = query.asInt(0);
                result.is_null = false;
            } 
        }
        query.close();
    }

    return result;
}
#endif

int_val_t parseArg(PSERoutineMeta *meta, unsigned char *in,
            ::Firebird::ThrowStatusWrapper *status,
            int field_idx)
        {
            int_val_t result;
            unsigned nullOffset = meta->in_meta.getNullOffset(field_idx);
            result.is_null = *(ISC_SHORT*) (in + nullOffset);
            if (!result.is_null) {
                unsigned valOffset = meta->in_meta.getOffset(field_idx);
                result.value = *(ISC_LONG *)(in + valOffset);
            }
            return result;
        }

void do_exec_my_sum_args(::Firebird::ThrowStatusWrapper *status, 
                    ::Firebird::IExternalContext *context, 
                    PSERoutineMeta *meta,
                    unsigned char *in, 
                    unsigned char *out)
{
    int_val_t n1 = parseArg(meta, (unsigned char *)in, status, 0);
    int_val_t n2 = parseArg(meta, (unsigned char *)in, status, 1);
    int_val_t n3 = parseArg(meta, (unsigned char *)in, status, 2);  

    int_val_t result = perform_my_sum_args(n1, n2, n3, status, context);

    unsigned outNullOffset = meta->out_meta.getNullOffset(0);
    unsigned outOffset = meta->out_meta.getOffset(0); 
    *(ISC_SHORT *)(out + outNullOffset) = result.is_null ? '\1' : '\0';
    if (!result.is_null)
        *(ISC_LONG *)(out + outOffset) = result.value;
}

void exec_my_sum_args(::Firebird::ThrowStatusWrapper *status, 
                    ::Firebird::IExternalContext *context, 
                    PSERoutineMeta *meta,
                    unsigned char *in, 
                    unsigned char *out)
{
   // log_debug(0, "Executing my_sum_args()"); 

  //  std::this_thread::sleep_for(std::chrono::seconds(8));

    do_exec_my_sum_args(status, context, meta, in, out);
}


void exec_my_sum_args2(::Firebird::ThrowStatusWrapper *status, 
                    ::Firebird::IExternalContext *context, 
                    PSERoutineMeta *meta,
                    unsigned char *in, 
                    unsigned char *out)
{
   // log_debug(0, "Executing my_sum_args2()"); 
    do_exec_my_sum_args(status, context, meta, in, out);
}
