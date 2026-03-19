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

#include "fb_iface.h"
#include <limits.h>
#include <math.h>
#include <string.h>


/*  IFBQuery  */

IFBQuery::~IFBQuery() 
{
    att->release();
    tra->release();
    close();
}

bool IFBQuery::prepare(const char *sql)
{
    if (stmt) throw "IFBQuery: already prepared";

    stmt = att->prepare(status, tra, 0, sql, 
        SQL_DIALECT_V6, IStatement::PREPARE_PREFETCH_METADATA);
    if (!stmt) return false;
    in_meta = stmt->getInputMetadata(status);
    unsigned in_len = in_meta->getMessageLength(status);
    if (in_len) {
        //in_buffer = new unsigned char[in_len];
        in_buffer = (unsigned char *)malloc(in_len);
        memset(in_buffer, 0, in_len);
    }

    out_meta = stmt->getOutputMetadata(status);
    unsigned out_len = out_meta->getMessageLength(status);
 //   if (out_len) out_buffer = new unsigned char[out_len];
    if (out_len) out_buffer = (unsigned char *)malloc(out_len);

    return true;
}

bool IFBQuery::openCursor(void)
{
    curs = stmt->openCursor(status, tra, in_meta, in_buffer, out_meta, 0);
    if (curs) curs->fetchNext(status, out_buffer);
    return curs != NULL;
}
bool IFBQuery::eof(void)
{
    return !curs || curs->isEof(status);
}

bool IFBQuery::next(void)
{
    return curs->fetchNext(status, out_buffer) == IStatus::RESULT_OK;
}

void IFBQuery::close(void)
{
    if (curs) {
        curs->close(status);
        curs = NULL;
    }
    if (in_meta) {
        in_meta->release();
        in_meta = NULL;        
    }
    if (out_meta) {
        out_meta->release();
        out_meta = NULL;        
    }
    if (in_buffer) {
        //::delete in_buffer;
        free(in_buffer);
        in_buffer = NULL;
    }
    if (out_buffer) {
        //::delete out_buffer;
        free(out_buffer);
        out_buffer = NULL;
    }
    if (stmt) {
        stmt->free(status);
        stmt = NULL;
    }
}

bool IFBQuery::setParamAsInt(int par_idx, int value)
{
    unsigned char *p = in_buffer + in_meta->getOffset(status, par_idx);
    switch (in_meta->getType(status, par_idx))
    {
		default: return false;
		case SQL_SHORT:
		{
			*((short*)p) = (short)value;
			break;
		}
		case SQL_LONG:
		{
			*((int32_t*)p) = (int32_t)value;
			break;
		}
		case SQL_INT64:
		{
			*((int64_t*)p) = (int64_t)value;
			break;
		}
		case SQL_FLOAT:
		{
			*((float*)p) = (float)value;
			break;
		}
		case SQL_DOUBLE:
		case SQL_D_FLOAT:
		{
			*((double*)p) = (double)value;
			break;
		}
    }
    p = in_buffer + in_meta->getNullOffset(status, par_idx);
    *((short *)p) = 0;
    return true;
}

bool IFBQuery::setParamAsNull(int par_idx)
{
    unsigned char *p = in_buffer + in_meta->getNullOffset(status, par_idx);
    *((short *)p) = 1;
    return true;
}

bool IFBQuery::getFieldIsNull(int field_idx)
{
    unsigned char *p = out_buffer + out_meta->getNullOffset(status, field_idx);
    return *((short *)p) != 0;
}   

double adjustNumericScale(int64_t value, short scale)
{
	int64_t scaling = 1;
	if (scale > 0)
	{
		for (short i = 1; i <= scale; i++)
			scaling *= 10;
		return (double)((double)value * (double)scaling);
	}
	else if (scale < 0)
	{
		for (short i = -1; i >= scale; i--)
			scaling *= 10;
		return (double)((double)value / (double)scaling);
	}
	else return (double)value;
}

bool IFBQuery::tryAsInt(int field_idx, int *value)
{
    if (getFieldIsNull(field_idx)) return false;

    unsigned char *p = out_buffer + out_meta->getOffset(status, field_idx);
    int scale = out_meta->getScale(status, field_idx);

    double _value = 0;
    switch (out_meta->getType(status, field_idx))
    {
        default: return false;

        case SQL_SHORT:
        {
            _value = round(adjustNumericScale((int64_t) *((int16_t*)p), scale));
            break;
        }

        case SQL_LONG:
        {
            _value = round(adjustNumericScale((int64_t) *((int32_t*)p), scale));
            break;
        }
        case SQL_INT64:
        {
            _value = round(adjustNumericScale((int64_t) *((int64_t*)p), scale));
            break;
        }
        case SQL_FLOAT:
        {
            _value = round(*(float*)p);
            break;
        }
        case SQL_DOUBLE:
        case SQL_D_FLOAT:
        {
            _value = round(*(double*)(p));
            break;
        }
    }
    *value = (int)_value;

	return (_value > ((double)INT_MIN + 1)) && (_value <= (double)INT_MAX);    
}

int IFBQuery::asInt(int field_idx)
{
    int result = 0;
    tryAsInt(field_idx, &result);

    return result;
}

int IFBQuery::fieldIdxByName(const char *name)
{
    if (!out_meta) return -1;
    if (field_names.empty()) {
        for (int i = 0; i < out_meta->getCount(status); i++)
            field_names[out_meta->getAlias(status, i)] = i;
    }

    int result = -1;
    try {
        result = field_names.at(name);
    } catch (...) {
        result = -1;
    }
    return result;
}

int IFBQuery::fbn_as_int(const char *name)
{
    int idx = fieldIdxByName(name);
    int result = 0;
    if (idx >= 0) result = asInt(idx);
    return result;
}

long __vax_integer(const unsigned char* ptr, short length)
{
/**************************************
 *
 *	g d s _ $ v a x _ i n t e g e r
 *
 **************************************
 *
 * Functional description
 *	Pick up (and convert) a VAX style integer of variable length.
 *
 **************************************/
	if (!ptr || length <= 0 || length > 4)
		return 0;

	long value = 0;
	int shift = 0;

	while (--length > 0)
	{
		value += ((long) *ptr++) << shift;
		shift += 8;
	}
	value += ((long)(char) *ptr) << shift;

	return value;
}

bool IFBQuery::field_try_as_string_realloc(int field_idx, char **value)
{
	if (getFieldIsNull(field_idx)) return false;

    unsigned char *p = out_buffer + out_meta->getOffset(status, field_idx);
    switch (out_meta->getType(status, field_idx))
 	{
		default: return false;

		case SQL_TEXT:
		{
            size_t len = out_meta->getLength(status, field_idx);
			char *T = (char*)realloc(*value, len + 1);
			if (T == NULL)
				return false;
			if (len == 0)
				*T = 0;
			else
			{
				strncpy(T, (const char *)p, len);
			}
			*value = T;
			return true;
		}
		case SQL_VARYING:
		{
			size_t len = (size_t)__vax_integer(p, 2);
			char *T = (char*)realloc(*value, len + 1);
			if (T == NULL)
				return false;
			if (len == 0)
				*T = 0;
			else
			{
				if (0 != strncpy(T, (const char*)(p + 2), len))
				{
					free(T);
					*value = NULL;
					return false;
				}
			}
			*value = T;
			return true;
		}
	}
	return false;
}

bool IFBQuery::fbn_as_string_realloc(const char *name, char **res)
{
    int idx = fieldIdxByName(name);
    if ((idx >= 0) && field_try_as_string_realloc(idx, res)) return true;

    free(*res);
    res = NULL;
    return false;
}

bool IFBQuery::fbn_is_null(const char *name) {
    int idx = fieldIdxByName(name);
    return ((idx < 0) || getFieldIsNull(idx));
}

bool IFBQuery::tryAsDouble(int field_idx, double *value) {
    if (getFieldIsNull(field_idx)) return false;

    unsigned char *p = out_buffer + out_meta->getOffset(status, field_idx);
    int scale = out_meta->getScale(status, field_idx);

    double _value = 0;
    switch (out_meta->getType(status, field_idx))
	{
	default: return false;

	case SQL_SHORT:
	{
		_value = adjustNumericScale((int64_t) *((int16_t*)p), scale);
		break;
	}

	case SQL_LONG:
	{
		_value = adjustNumericScale((int64_t) *((int32_t*)p), scale);
		break;
	}
	case SQL_INT64:
	{
		_value = adjustNumericScale((int64_t) *((int64_t*)p), scale);
		break;
	}
	case SQL_FLOAT:
	{
		_value = *(float*)p;
		break;
	}
	case SQL_DOUBLE:
	case SQL_D_FLOAT:
	{
		_value = *(double*)p;
		break;
	}
	case SQL_TEXT:
	{
		// hope nobody uses this shit
		char *stop = NULL;
		_value = strtod((const char*)p, &stop);
		if ((stop == NULL) || (*stop != 0))
			return false;
		break;
	}
	case SQL_VARYING:
	{
		size_t len = (size_t)__vax_integer(p, 2);
		if (len < 1)
			return false;
		char *T = (char*)malloc(len + 1);
		if (T == NULL)
			return false;
		if (0 != strncpy(T, (const char *)(p + 2), len))
		{
			free(T);
			return false;
		}
		char *stop = NULL;
		_value = strtod(T, &stop);
		if ((stop == NULL) || (*stop != 0))
		{
			free(T);
			return false;
		}
		free(T);
		break;
	}
	}
	*value = _value;

	return true;
}

double IFBQuery::asDouble(int field_idx)
{
    double result = 0;
    tryAsDouble(field_idx, &result);

    return result;
}

double IFBQuery::fbn_as_double(const char *name) {
    int idx = fieldIdxByName(name);
    double result = 0;
    if (idx >= 0) result = asDouble(idx);
    return result;
}