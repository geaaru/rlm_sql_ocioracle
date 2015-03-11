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

#include <stdlib.h>
#include "sql_ocioracle.h"
#include "sql_ocioracle_row.h"
#include "sql_ocioracle_field.h"

#include <freeradius-devel/radiusd.h>

/**
 * @return NULL on error
 */
rlm_sql_ocioracle_row *
rlm_sql_ocioracle_row_create (int ncolumn, int rowcount,
                              void *closure)
{
    rlm_sql_ocioracle_row *ans = NULL;

    if (ncolumn <= 0 || rowcount < 0) goto error;

    ans = (rlm_sql_ocioracle_row *)
          rad_malloc(sizeof(rlm_sql_ocioracle_row));

    if (ans) {
        ans->colnum = ncolumn;
        ans->closure = closure;
        ans->rowcount = rowcount;

        ans->data = (rlm_sql_ocioracle_field **)
                    rad_malloc(sizeof(rlm_sql_ocioracle_field *) *
                               ncolumn);
        if (!ans->data) goto error;
        memset(ans->data, 0, sizeof(rlm_sql_ocioracle_field *) * ncolumn);
    }

    return ans;

error:

    if (ans) rlm_sql_ocioracle_row_destroy(ans);

    return NULL;
}

/**
 * @return 1 on error
 * @return 0 on success
 */
int
rlm_sql_ocioracle_row_destroy (rlm_sql_ocioracle_row *r)
{
    int i;

    if (!r) return 1;

    if (r->data) {

        for (i = 0; i < r->colnum; i++) {

            if (r->data[i]) {
                rlm_sql_ocioracle_field_destroy(r->data[i]);
                r->data[i] = NULL;
            }

        } // end for i

        free(r->data);
        r->data = NULL;
    }

    r->colnum = 0;
    r->rowcount = 0;
    r->closure = NULL;

    free(r);

    return 0;
}

/**
 * @return 1 on error
 * @return 0 on success
 * @pre
 *      Target field position must be not initialized
 */
int
rlm_sql_ocioracle_row_set_field (rlm_sql_ocioracle_row *r,
                                 rlm_sql_ocioracle_field *f,
                                 int pos)
{
    if (!r || !f || pos < 0 || pos > r->colnum)
        return 1;

    if (!r->data || r->data[pos]) return 1;

    r->data[pos] = f;

    return 0;
}

/**
 * @return NULL on error or if field is not initialized or present.
 */
inline rlm_sql_ocioracle_field *
rlm_sql_ocioracle_row_get_field (rlm_sql_ocioracle_row *r, int pos)
{
    return (r && pos >= 0 && pos <= r->colnum ? r->data[pos] : NULL); 
}

/**
 * @return closure
 */
inline void *
rlm_sql_ocioracle_row_get_closure (rlm_sql_ocioracle_row *r)
{
    return (r ? r->closure : NULL);
}

/**
 * @return 0 on error
 */
inline int
rlm_sql_ocioracle_row_get_colnum (rlm_sql_ocioracle_row *r)
{
    return (r ? r->colnum : 0);
}

/**
 * @return 0 on error
 */
inline int
rlm_sql_ocioracle_row_get_rowcount (rlm_sql_ocioracle_row *r)
{
    return (r ? r->rowcount : 0);
}

/**
 *  Create results array with strings
 *  @return 1 on error
 *  @return 0 on success
 */
int
rlm_sql_ocioracle_row_dump (rlm_sql_ocioracle_row *r, char ***res)
{
    int i;
    char **rr = NULL;

    if (!r || !res) return 1;

    rr = (char **) rad_malloc(sizeof(char *) * r->colnum);
    if (!rr) return 1;

    memset(rr, 0, sizeof(char *) * r->colnum);

    for (i = 0; i < r->colnum; i++) {
        rr[i] = rlm_sql_ocioracle_field_dump(r->data[i]);
    } // end for i

    *res = rr;

    return 0;
}

// vim: ts=4 shiftwidth=4 expandtab
