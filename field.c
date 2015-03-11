/*
 Copyright (C) 2010  Ge@@ru, geaaru@gmail.com 

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 License:  GPL 2.0
*/

#include <freeradius-devel/radiusd.h>
#include <freeradius-devel/rad_assert.h>

#include <stdlib.h>
#include <string.h>
#include "sql_ocioracle.h"
#include "sql_ocioracle_field.h"

/**
 * @return NULL on error
 */
rlm_sql_ocioracle_field *
rlm_sql_ocioracle_field_create (void *closure)
{
    rlm_sql_ocioracle_field *ans = NULL;

    ans = rad_malloc(sizeof(rlm_sql_ocioracle_field));
    if (ans) {
        ans->closure = closure;
        ans->colname = NULL;
        ans->data = NULL;
        ans->data_len = 0;
        ans->type = 0;
    }

    return ans;
}

/**
 * @return 1 on error
 * @return 0 on success
 */
int
rlm_sql_ocioracle_field_destroy (rlm_sql_ocioracle_field *f)
{
    if (!f) return 1;

    if (f->colname) {
        free(f->colname);
        f->colname = NULL;
    }

    if (f->data) {
        free(f->data);
        f->data = NULL;
    }

    f->closure = NULL;

    free(f);

    return 0;
}

/**
 * @return 1 on error
 * @return 0 on success
 */
int
rlm_sql_ocioracle_field_set_data (rlm_sql_ocioracle_field *f,
                                  void *data, size_t size,
                                  char usevalue)
{
    if (!f || !data || f->data || size <= 0) return 1;

    if (usevalue)
        f->data = data;
    else {
        f->data = rad_malloc(size);
        if (!f->data) return 1;
        memset(f->data, 0, size);
        memcpy(f->data, data, size);
    }

    f->data_len = size;

    return 0;
}

/**
 * @return 1 on error
 * @return 0 on success
 */
int
rlm_sql_ocioracle_field_set_colname (rlm_sql_ocioracle_field *f,
                                     char *colname, char usevalue)
{
    int len = 0;

    if (!f || !colname) return 1;

    if (usevalue)
        f->colname = colname;
    else {
        len = strlen(colname) + 1;
        f->colname = (char *) rad_malloc(sizeof(char) * len);
        if (!f->colname) return 1;

        memset(f->colname, 0, len);
        memcpy(f->colname, colname, len);
    }

    return 0;
}

/**
 * @return NULL on error or if data isn't initialized
 */
inline void *
rlm_sql_ocioracle_field_get_data (rlm_sql_ocioracle_field *f)
{
    return (f ? f->data : NULL);
}

/**
 * @return NULL on error or if colname isn't initialized
 */
inline const char *
rlm_sql_ocioracle_field_get_colname (rlm_sql_ocioracle_field *f)
{
    return (f ? f->colname : NULL);
}

/**
 * @return closure of the field
 */
inline void *
rlm_sql_ocioracle_field_get_closure (rlm_sql_ocioracle_field *f)
{
    return (f ? f->closure : NULL);
}

/**
 * @return 0 on error or if data isn't initialized
 */
inline size_t
rlm_sql_ocioracle_field_get_data_len (rlm_sql_ocioracle_field *f)
{
    return (f ? f->data_len : 0);
}

/**
 * Set type of the field.
 * Possible values are:
 * OCI_CDT_NUMERIC : short, int, long long, double
 * OCI_CDT_DATETIME : OCI_Date *
 * OCI_CDT_TEXT : dtext *
 * OCI_CDT_LONG : OCI_Long *
 * OCI_CDT_CURSOR : OCI_Statement *
 * OCI_CDT_LOB : OCI_Lob *
 * OCI_CDT_FILE : OCI_File *
 * OCI_CDT_TIMESTAMP : OCI_Timestamp *
 * OCI_CDT_INTERVAL : OCI_Interval *
 * OCI_CDT_RAW : void *
 * OCI_CDT_OBJECT : OCI_Object *
 * OCI_CDT_COLLECTION : OCI_Coll *
 * OCI_CDT_REF : OCI_Ref *
 * @return 1 on error
 * @return 0 on success
 */
int
rlm_sql_ocioracle_field_set_type (rlm_sql_ocioracle_field *f,
                                  unsigned int type)
{
    if (!f) return 1;

    f->type = type;

    return 0;
}

/**
 * @return type of the field or 0 if it isn't initialized or on error
 */
inline unsigned int
rlm_sql_ocioracle_field_get_type (rlm_sql_ocioracle_field *f)
{
    return (f ? f->type : 0);
}

/**
 * @return 1 on error
 * @return 0 on success
 */
int
rlm_sql_ocioracle_field_store_data (rlm_sql_ocioracle_field *f,
                                    OCI_Column *col,
                                    OCI_Resultset *rs)
{
    int number = 0;
    unsigned int type = 0;
    unsigned int index = 0;
    const char *string = NULL;
    char buffer[256];

    if (!f || !col || !rs) return 1;

    type = OCI_ColumnGetType(col);
    rlm_sql_ocioracle_field_set_colname(f, (char *) OCI_ColumnGetName(col), 0);
    rlm_sql_ocioracle_field_set_type(f, type);

    index = OCI_GetColumnIndex(rs, OCI_ColumnGetName(col));
    if (OCI_IsNull(rs, index))
        // No data to store
        return 0;

    switch (type) {
        case OCI_CDT_TEXT: // dtext *
            string = OCI_GetString(rs, index);
            if (rlm_sql_ocioracle_field_set_data(f, (void *) string,
                                                 strlen(string) + 1, 0))
                goto error;
            break;
        case OCI_CDT_NUMERIC: // short, int, long long, double
            // Temporary handle all number as int
            number = OCI_GetInt(rs, index);
            memset(buffer, 0, 256);
            snprintf(buffer, 256, "%d", number);
            if (rlm_sql_ocioracle_field_set_data(f, (void *) buffer,
                                                 strlen(buffer) + 1, 0))
                goto error;
            break;
        case OCI_CDT_DATETIME: // OCI_Date
            // For now convert date to string
        case OCI_CDT_TIMESTAMP: // OCI_Timestamp *
        case OCI_CDT_LONG: // OCI_Long *
        case OCI_CDT_LOB: // OCI_Lob
        case OCI_CDT_FILE: // OCI_File *
        case OCI_CDT_CURSOR: // OCI_Statement *
        case OCI_CDT_INTERVAL: // OCI_Interval *
        case OCI_CDT_RAW: // void *
        case OCI_CDT_OBJECT: // OCI_Object *
        case OCI_CDT_COLLECTION: // OCI_Coll *
        case OCI_CDT_REF: // OCI_Ref *
        default:
            radlog(L_ERR, "rlm_sql_ocioracle: type of column %s (%d) not permitted",
                   (f->colname ? f->colname : ""), type);
            break;
    } // end switch

    return 0;

error:

    return 1;
}

/**
 * @return NULL on error or if there aren't data for field
 */
char *
rlm_sql_ocioracle_field_dump (rlm_sql_ocioracle_field *f)
{
    char *ans = NULL;

    if (!f) return ans;

    switch (f->type) {
        case OCI_CDT_TEXT: // dtext *
        case OCI_CDT_NUMERIC: // short, int, long long, double
            if (f->data) {
                ans = (char *) rad_malloc(f->data_len);
                if (!ans) goto error;

                memset(ans, 0, f->data_len);
                memcpy(ans, f->data, f->data_len);
            }
            break;
        case OCI_CDT_DATETIME: // OCI_Date
            // For now convert date to string
        case OCI_CDT_TIMESTAMP: // OCI_Timestamp *
        case OCI_CDT_LONG: // OCI_Long *
        case OCI_CDT_LOB: // OCI_Lob
        case OCI_CDT_FILE: // OCI_File *
        case OCI_CDT_CURSOR: // OCI_Statement *
        case OCI_CDT_INTERVAL: // OCI_Interval *
        case OCI_CDT_RAW: // void *
        case OCI_CDT_OBJECT: // OCI_Object *
        case OCI_CDT_COLLECTION: // OCI_Coll *
        case OCI_CDT_REF: // OCI_Ref *
        default:
            break;

    } // end switch

    return ans;

error:

    return NULL;
}

// vim: ts=4 shiftwidth=4 expandtab
