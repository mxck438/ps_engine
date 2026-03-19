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
#include <vector>

#include "firebird/Interface.h"

using namespace Firebird;

class PSEMsgMeta
{
private:
    struct FieldMeta {
        unsigned type;
        FB_BOOLEAN is_nullable;
        int subtype;
        unsigned length;
        int scale;
        unsigned charset;
        unsigned offset;
        unsigned null_offset;
    };

    std::vector<struct FieldMeta> fields;
    unsigned messageLength;
public:
    void set(IMessageMetadata* meta, ThrowStatusWrapper *status)
    {
        for (unsigned i = 0; i < meta->getCount(status); i++) {
            struct FieldMeta field;
            field.type = meta->getType(status, i);
            field.is_nullable = meta->isNullable(status, i);
            field.subtype = meta->getSubType(status, i);
            field.length = meta->getLength(status, i);
            field.scale = meta->getScale(status, i);
            field.charset = meta->getCharSet(status, i);
            field.offset = meta->getOffset(status, i);
            field.null_offset = meta->getNullOffset(status, i);

            fields.push_back(field);
        }
        messageLength = meta->getMessageLength(status);
    }

    unsigned getCount() { return fields.size(); }
    unsigned getType(unsigned index) { return fields[index].type; }
    FB_BOOLEAN getIsNullable(unsigned index) 
        { return fields[index].is_nullable; }
    int getSubtype(unsigned index) { return fields[index].subtype; }
    unsigned getLength(unsigned index) { return fields[index].length; }
    int getScale(unsigned index) { return fields[index].scale; }
    unsigned getCharset(unsigned index) { return fields[index].charset; }
    unsigned getOffset(unsigned index) { return fields[index].offset; }
    unsigned getNullOffset(unsigned index) { return fields[index].null_offset; }
    unsigned getMessageLength() { return messageLength; }
};

class PSERoutineMeta
{
public:
    PSEMsgMeta in_meta;
    PSEMsgMeta out_meta;
public:
    PSERoutineMeta(IRoutineMetadata *metadata, ThrowStatusWrapper *status)
    {
        IMessageMetadata* in_imeta = metadata->getInputMetadata(status);
        in_meta.set(in_imeta, status);
        in_imeta->release();
        IMessageMetadata* out_imeta = metadata->getOutputMetadata(status);
        out_meta.set(out_imeta, status);
        out_imeta->release();
    }
};

