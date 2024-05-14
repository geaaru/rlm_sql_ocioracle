/*
 Copyright 2017-2024  Daniele Rondina <geaaru@gmail.com>

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


#ifndef  SQL_OCIORACLE_H
#define  SQL_OCIORACLE_H

// Opensource OCILib library
#include <ocilib.h>

#define SQL_OCI_ERROR(msg, ...) ERROR("rlm_sql_ocioracle: " msg, ## __VA_ARGS__)
#define SQL_OCI_DEBUG(msg, ...) DEBUG("rlm_sql_ocioracle: " msg, ## __VA_ARGS__)
#define SQL_OCI_INFO(msg, ...)   INFO("rlm_sql_ocioracle: " msg, ## __VA_ARGS__)
#define SQL_OCI_WARN(msg, ...)   WARN("rlm_sql_ocioracle: " msg, ## __VA_ARGS__)

#define RLM_SQL_OCIORACLE_CURSOR_PROCEDURE_STR  ":Ret_Cursor"
#define SQL_OCI_INITIAL_ROWS_SIZE 10

typedef struct rlm_sql_ocioracle_conn rlm_sql_ocioracle_conn_t;

struct rlm_sql_ocioracle_conn {
    OCI_Connection  *conn;
    OCI_Statement   *queryHandle;
    OCI_Error       *errHandle;
    int             errCode;

    OCI_Resultset   *rs;
    // This pointer is used only
    // for procedure that return cursor
    OCI_Statement   *cursorHandle;

    // Talloc context used for data result.
    TALLOC_CTX      *ctx;

    /// Rows array pointer used to store result
    /// of the query.
    /// Contains an array of pointer to a row data.
    /// Every row data contains an array of pointer
    /// to column data (string)
    char            ***rows;
    int             num_fetched_rows;
    unsigned int    num_columns;

    int             pos;
    int             affected_rows;
};


#endif   /* ----- #ifndef SQL_OCIORACLE_H  ----- */

// vim: ts=4 shiftwidth=4 expandtab
