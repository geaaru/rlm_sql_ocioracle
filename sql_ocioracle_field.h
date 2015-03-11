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


#ifndef  SQL_OCIORACLE_FIELD_H
#define  SQL_OCIORACLE_FIELD_H

struct rlm_sql_ocioracle_field {
    void *closure;
    char *colname;
    // As slq_oracle module convert all data to string
    void *data;
    size_t data_len;
    unsigned int type;
};


// Prototypes
rlm_sql_ocioracle_field *
rlm_sql_ocioracle_field_create (void *);

int
rlm_sql_ocioracle_field_destroy (rlm_sql_ocioracle_field *);

int
rlm_sql_ocioracle_field_set_data (rlm_sql_ocioracle_field *,
                                  void *, size_t, char);

int
rlm_sql_ocioracle_field_set_colname (rlm_sql_ocioracle_field *,
                                     char *, char);

inline void *
rlm_sql_ocioracle_field_get_data (rlm_sql_ocioracle_field *);

inline const char *
rlm_sql_ocioracle_field_get_colname (rlm_sql_ocioracle_field *);

inline void *
rlm_sql_ocioracle_field_get_closure (rlm_sql_ocioracle_field *);

inline size_t
rlm_sql_ocioracle_field_get_data_len (rlm_sql_ocioracle_field *);

int
rlm_sql_ocioracle_field_set_type (rlm_sql_ocioracle_field *,
                                  unsigned int);

inline unsigned int
rlm_sql_ocioracle_field_get_type (rlm_sql_ocioracle_field *);

int
rlm_sql_ocioracle_field_store_data (rlm_sql_ocioracle_field *,
                                    OCI_Column *, 
                                    OCI_Resultset *);

char *
rlm_sql_ocioracle_field_dump (rlm_sql_ocioracle_field *);

#endif   /* ----- #ifndef SQL_OCIORACLE_FIELD_H  ----- */

// vim: ts=4 shiftwidth=4 expandtab
