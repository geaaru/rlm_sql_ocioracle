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

#ifndef  RLM_SQL_OCIORACLE_LIST_H_H
#define  RLM_SQL_OCIORACLE_LIST_H_H

/**
 * Node element.
 */
struct rlm_sql_ocioracle_node {
    void *data;
    rlm_sql_ocioracle_node *next;
    rlm_sql_ocioracle_node *prev;
};

/**
 * List container.
 */
struct rlm_sql_ocioracle_list {
    rlm_sql_ocioracle_node *first;
    rlm_sql_ocioracle_node *last;
    int size;
    void *closure;
};

// Prototypes
rlm_sql_ocioracle_node *
rlm_sql_ocioracle_node_create (void *);

int
rlm_sql_ocioracle_node_destroy (rlm_sql_ocioracle_node *);

inline rlm_sql_ocioracle_node *
rlm_sql_ocioracle_node_get_next (rlm_sql_ocioracle_node *);

inline rlm_sql_ocioracle_node *
rlm_sql_ocioracle_node_get_prev (rlm_sql_ocioracle_node *);

inline void *
rlm_sql_ocioracle_node_get_data (rlm_sql_ocioracle_node *);

inline int
rlm_sql_ocioracle_node_set_data (rlm_sql_ocioracle_node *, void *);

rlm_sql_ocioracle_list *
rlm_sql_ocioracle_list_create(void *);

inline void *
rlm_sql_ocioracle_list_get_closure (rlm_sql_ocioracle_list *);

int
rlm_sql_ocioracle_list_set_closure (rlm_sql_ocioracle_list *, void *);

inline int
rlm_sql_ocioracle_list_get_size (rlm_sql_ocioracle_list *);

inline rlm_sql_ocioracle_node *
rlm_sql_ocioracle_list_get_first (rlm_sql_ocioracle_list *);

inline rlm_sql_ocioracle_node *
rlm_sql_ocioracle_list_get_last (rlm_sql_ocioracle_list *);

int
rlm_sql_ocioracle_list_destroy (rlm_sql_ocioracle_list *);

int
rlm_sql_ocioracle_list_add_node (rlm_sql_ocioracle_list *,
                                 rlm_sql_ocioracle_node *);

int
rlm_sql_ocioracle_list_node_is_onlist(rlm_sql_ocioracle_list *,
                                      rlm_sql_ocioracle_node *);

int
rlm_sql_ocioracle_list_exist_node_string(rlm_sql_ocioracle_list *, char *);

int
rlm_sql_ocioracle_list_exist_node_ptr(rlm_sql_ocioracle_list *, void *);

int
rlm_sql_ocioracle_list_del_node (rlm_sql_ocioracle_list *,
                                 rlm_sql_ocioracle_node *);

int
rlm_sql_ocioracle_list_del_node_by_ptr (rlm_sql_ocioracle_list *, void *);

rlm_sql_ocioracle_node *
rlm_sql_ocioracle_list_del_last_node (rlm_sql_ocioracle_list *);

int
rlm_sql_ocioracle_list_add_node_data (rlm_sql_ocioracle_list *, void *);

int
rlm_sql_ocioracle_list_string_get_common (rlm_sql_ocioracle_list *,
                                          rlm_sql_ocioracle_list *,
                                          rlm_sql_ocioracle_list *,
                                          short);

int
rlm_sql_ocioracle_list_string_destroy (rlm_sql_ocioracle_list *l);

#endif   /* ----- #ifndef RLM_SQL_OCIORACLE_LIST_H_H  ----- */

// vim: ts=4 shiftwidth=4 expandtab
