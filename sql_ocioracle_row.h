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


#ifndef  SQL_OCIORACLE_ROW_H
#define  SQL_OCIORACLE_ROW_H

struct rlm_sql_ocioracle_row {
    /// Contains 
    int colnum;
    /// Contains position of the row
    int rowcount;
    rlm_sql_ocioracle_field **data;
};

// Prototypes
rlm_sql_ocioracle_row *
rlm_sql_ocioracle_row_create (int, int, void *);

int
rlm_sql_ocioracle_row_destroy (rlm_sql_ocioracle_row *);

int
rlm_sql_ocioracle_row_set_field (rlm_sql_ocioracle_row *,
                                 rlm_sql_ocioracle_field *,
                                 int);

inline rlm_sql_ocioracle_field *
rlm_sql_ocioracle_row_get_field (rlm_sql_ocioracle_row *, int);

inline void *
rlm_sql_ocioracle_row_get_closure (rlm_sql_ocioracle_row *);

inline int
rlm_sql_ocioracle_row_get_colnum (rlm_sql_ocioracle_row *);

inline int
rlm_sql_ocioracle_row_get_rowcount (rlm_sql_ocioracle_row *);

int
rlm_sql_ocioracle_row_dump (rlm_sql_ocioracle_row *, char ***);

#endif   /* ----- #ifndef SQL_OCIORACLE_ROW_H  ----- */

// vim: ts=4 shiftwidth=4 expandtab
