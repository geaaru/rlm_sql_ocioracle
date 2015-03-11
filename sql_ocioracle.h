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


#ifndef  SQL_OCIORACLE_H
#define  SQL_OCIORACLE_H

// Opensource OCILib library
#include <ocilib.h>

#define MAX_DATASTR_LEN 64
#define RLM_SQL_OCIORACLE_CURSOR_PROCEDURE_STR  ":Ret_Cursor"
#define SQLERROR_MSG_BUFFER 512

typedef struct rlm_sql_ocioracle_conn rlm_sql_ocioracle_conn_t;
typedef struct rlm_sql_ocioracle_row rlm_sql_ocioracle_row;
typedef struct rlm_sql_ocioracle_field rlm_sql_ocioracle_field;
typedef struct rlm_sql_ocioracle_list rlm_sql_ocioracle_list;
typedef struct rlm_sql_ocioracle_node rlm_sql_ocioracle_node;

struct rlm_sql_ocioracle_conn {
    OCI_Connection  *conn;
    OCI_Statement   *queryHandle;
    OCI_Error       *errHandle;
    int             errCode;

    OCI_Resultset   *rs;
    // This pointer is used only
    // for procedure that return cursor
    OCI_Statement   *cursorHandle;

    char            **results;
    int             id;
    int             in_use;
    struct timeval  tv;

    int              pos;
    unsigned int     affected_rows;
    rlm_sql_ocioracle_list *rows;
    rlm_sql_ocioracle_node *curr_row;
};


#endif   /* ----- #ifndef SQL_OCIORACLE_H  ----- */

// vim: ts=4 shiftwidth=4 expandtab
