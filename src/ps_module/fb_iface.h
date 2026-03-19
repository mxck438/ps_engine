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

#include <map>
#include <string>
#include "firebird/Interface.h"


using namespace Firebird;

class IFBQuery 
{
private:
    IAttachment *att;
    ITransaction* tra;
    ThrowStatusWrapper *status;
    IStatement* stmt = NULL;
    unsigned char* in_buffer = NULL;
    unsigned char* out_buffer = NULL; 
    IMessageMetadata* in_meta = NULL;
    IMessageMetadata* out_meta = NULL;
    IResultSet* curs = NULL;

    // A struct to provide a case-insensitive comparison function
    struct CaseInsensitiveLess {
        bool operator()(const std::string& lhs, const std::string& rhs) const {
            return std::lexicographical_compare(
                lhs.begin(), lhs.end(),
                rhs.begin(), rhs.end(),
                [](unsigned char a, unsigned char b) { 
                    return std::tolower(a) < std::tolower(b);
                }
            );
        }
    };
    std::map<std::string, int, CaseInsensitiveLess> field_names;

    int fieldIdxByName(const char *name);
public:
    explicit IFBQuery(IExternalContext *context, ThrowStatusWrapper *status)
        : att(context->getAttachment(status)),
        tra(context->getTransaction(status)),
        status(status)
    {}
    ~IFBQuery();

    bool prepare(const char *sql);
    bool openCursor(void);
    bool eof(void);
    bool next(void);
    void close(void);

    bool setParamAsInt(int par_idx, int value);
    bool setParamAsNull(int par_idx);
    bool getFieldIsNull(int field_idx);
    bool tryAsInt(int field_idx, int *value);
    int asInt(int field_idx);
    bool tryAsDouble(int field_idx, double *value);
    double asDouble(int field_idx);

    bool fbn_is_null(const char *name);
    int fbn_as_int(const char *name);
    bool field_try_as_string_realloc(int field_idx, char **value);
    bool fbn_as_string_realloc(const char *name, char **res);
    double fbn_as_double(const char *name);
};
