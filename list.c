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
#include "list.h"

#include <freeradius-devel/radiusd.h>


/**
 * @return NULL on error
 */
rlm_sql_ocioracle_node *
rlm_sql_ocioracle_node_create (void *data)
{
    rlm_sql_ocioracle_node *ans = NULL;

    ans = (rlm_sql_ocioracle_node *)
          rad_malloc(sizeof(rlm_sql_ocioracle_node));

    if (ans) {
        ans->data = data;
        ans->next = NULL;
        ans->prev = NULL;
    }

    return ans;
}

/**
 */
int
rlm_sql_ocioracle_node_destroy (rlm_sql_ocioracle_node *n)
{
    if (!n) return 1;

    n->data = NULL;
    n->next = NULL;
    n->prev = NULL;

    free(n);

    return 0;
}

/**
 * @return NULL on error
 */
rlm_sql_ocioracle_list *
rlm_sql_ocioracle_list_create (void *closure)
{
    rlm_sql_ocioracle_list *ans = NULL;

    ans = (rlm_sql_ocioracle_list *)
          rad_malloc(sizeof(rlm_sql_ocioracle_list));

    if (ans) {
        ans->first = ans->last = NULL;
        ans->size = 0;
        ans->closure = closure;
    }

    return ans;
}

/**
 * @return 1 on error
 * @return 0 on success
 */
int
rlm_sql_ocioracle_list_set_closure (rlm_sql_ocioracle_list *l, void *data)
{
    if (!l) return 1;

    l->closure = data;

    return 0;
}

/**
 * @return 1 on error
 * @return 0 on success
 */
int
rlm_sql_ocioracle_list_destroy (rlm_sql_ocioracle_list *l)
{
    rlm_sql_ocioracle_node *n = NULL;

    if (!l) return 1;

    while (l->first) {
        n = l->first;
        l->first = n->next;

        rlm_sql_ocioracle_node_destroy(n);
    }

    l->last = NULL;
    l->size = 0;
    l->closure = NULL;

    free(l);

    return 0;
}

/**
 * Append a new node to list.
 * @return 0 on success
 * @return 1 on error
 */
int
rlm_sql_ocioracle_list_add_node (rlm_sql_ocioracle_list *l,
                                 rlm_sql_ocioracle_node *n)
{
    if (!l || !n)
        return 1;

    if (l->last) {
        n->prev = l->last;
        l->last = n->prev->next = n;
    } else
        l->first = l->last = n;

    l->size++;

    return 0;
}

/**
 * Create a new node to list and set data as value of the node.
 * @return 0 on success
 * @return 1 on error
 */
int
rlm_sql_ocioracle_list_add_node_data (rlm_sql_ocioracle_list *l, void *data)
{
    rlm_sql_ocioracle_node *node = NULL;

    if (!l || !data)
        return 1;

    node = rlm_sql_ocioracle_node_create(data);
    if (!node) goto error;

    if (rlm_sql_ocioracle_list_add_node(l, node))
        goto error;

    return 0;

error:

    if (node)
        rlm_sql_ocioracle_node_destroy(node);

    return 1;
}

/**
 * @return closure of the rlm_sql_ocioracle_list object
 */
inline void *
rlm_sql_ocioracle_list_get_closure (rlm_sql_ocioracle_list *l)
{
    return (l ? l->closure : NULL);
}

/**
 * @return -1 on error
 * @return number of node in list
 */
inline int
rlm_sql_ocioracle_list_get_size (rlm_sql_ocioracle_list *l)
{
    return (l ? l->size : -1);
}

/**
 */
inline rlm_sql_ocioracle_node *
rlm_sql_ocioracle_list_get_first (rlm_sql_ocioracle_list *l)
{
    return (l ? l->first : NULL);
}

/**
 */
inline rlm_sql_ocioracle_node *
rlm_sql_ocioracle_list_get_last (rlm_sql_ocioracle_list *l)
{
    return (l ? l->last : NULL);
}

/**
 */
inline rlm_sql_ocioracle_node *
rlm_sql_ocioracle_node_get_next (rlm_sql_ocioracle_node *n)
{
    return (n ? n->next : (rlm_sql_ocioracle_node *) NULL);
}

/**
 */
inline rlm_sql_ocioracle_node *
rlm_sql_ocioracle_node_get_prev (rlm_sql_ocioracle_node *n)
{
    return (n ? n->prev : NULL);
}

/**
 */
inline void *
rlm_sql_ocioracle_node_get_data (rlm_sql_ocioracle_node *n)
{
    return (n ? n->data : NULL);
}

/**
 * @return 1 on error
 * @return 0 on success
 */
inline int
rlm_sql_ocioracle_node_set_data (rlm_sql_ocioracle_node *n, void *data)
{
    if (!n) return 1;

    n->data = data;

    return 0;
}

/**
 * @return 1 if node is on list
 * @return 0 if node isn't on list
 * @return -1 on error
 */
int
rlm_sql_ocioracle_list_node_is_onlist (rlm_sql_ocioracle_list *l,
                                       rlm_sql_ocioracle_node *n)
{
    int i;
    rlm_sql_ocioracle_node *un = NULL;

    if (!l || !n) return -1;

    if (l->size) {
        for (i = 0, un = l->first; i < l->size;
             i++, un = (un ? un->next : NULL)) {
            if (n == un) return 1;
        } // end for
    }

    return 0;
}

/**
 * \note
 *  This method works only if data of rlm_sql_ocioracle_node are string.
 * @return 0 if doesn't exits
 * @return 1 if exists
 * @return -1 on error
 */
int
rlm_sql_ocioracle_list_exist_node_string (rlm_sql_ocioracle_list *l, char *str)
{
    rlm_sql_ocioracle_node *n = NULL;

    if (!l || !str) return -1;

    n = l->first;
    while (n) {
        if (n->data &&
            !strncmp(str, (char *) n->data,
                     strlen(str) > strlen((char *) n->data) ?
                     strlen(str) : strlen((char *)n->data)))
            return 1;
        n = n->next;
    }

    return 0;
}

/**
 * Insert on common list commons string found between l1 and l2
 * (there aren't check for repeated string).
 * \pre
 *      input list MUST contains only node with string
 * @param copy 1 --> copy string
 *        copy 0 --> use same pointer
 * @param l1  list input 1
 * @param l2  list input 2
 * @param common  target common list where are added common string
 * @return 0 on success
 * @return 1 on error
 */
int
rlm_sql_ocioracle_list_string_get_common (rlm_sql_ocioracle_list *l1,
                                          rlm_sql_ocioracle_list *l2,
                                          rlm_sql_ocioracle_list *common,
                                          short copy)
{
    int i, ret;
    rlm_sql_ocioracle_node *n = NULL;
    char *value = NULL, *copy_value = NULL;

    if (!l1 || !l2 || !common) return 1;

    for (i = 0, n = rlm_sql_ocioracle_list_get_first(l1);
         i < rlm_sql_ocioracle_list_get_size(l1) && n;
         i++, n = rlm_sql_ocioracle_node_get_next(n), copy_value = NULL) {

        value = (char *) rlm_sql_ocioracle_node_get_data(n);
        if (!value) goto error;

        ret = rlm_sql_ocioracle_list_exist_node_string(l2, value);
        if (ret < 0) goto error;

        if (ret) {
            if (copy) {
                copy_value = (char *) rad_malloc(strlen(value) + 1);
                if (!copy_value) goto error;
            } else
                copy_value = value;

            if (rlm_sql_ocioracle_list_add_node_data(common, copy_value))
                goto error;
        }

    } // end for i

    return 0;

error:

    if (copy && copy_value)
        free(copy_value);

    return 1;
}

/**
 * @return 0 if doesn't exits
 * @return 1 if exists
 * @return -1 on error
 */
int
rlm_sql_ocioracle_list_exist_node_ptr (rlm_sql_ocioracle_list *l, void *ptr)
{
    rlm_sql_ocioracle_node *n = NULL;

    if (!l || !ptr) return -1;

    for (n = l->first; n; n = (n ? n->next : NULL))
        if (ptr == n->data) return 1;

    return 0;
}

/**
 * @return NULL on error or now node available
 * @return deleted node
 */
rlm_sql_ocioracle_node *
rlm_sql_ocioracle_list_del_last_node (rlm_sql_ocioracle_list *l)
{
    rlm_sql_ocioracle_node *ans = NULL;

    if (!l) return ans;

    if (l->last) {
        ans = l->last;

        if (l->last == l->first) {
            l->last = l->first = NULL;
        } else {
            ans->prev->next = NULL;
            l->last = ans->prev;
        }

        l->size--;
    }

    return ans;
}

/**
 * @return 1 on error
 * @return 0 on success (node founded and removed)
 */
int
rlm_sql_ocioracle_list_del_node (rlm_sql_ocioracle_list *l,
                                 rlm_sql_ocioracle_node *n)
{
    rlm_sql_ocioracle_node *node = NULL, *prev = NULL;
    int notfounded = 1;

    if (!l || !n || !l->size) return 1;

    for (node = l->first; node && l->size;
         prev = node, node = (node ? node->next : NULL)) {
        if (node == n) {
            if (prev) {
                prev->next = node->next;
                if (!node->next)
                    l->last = prev;
                else
                    node->next->prev = prev;

            } else {
                l->first = node->next;
                if (!node->next)
                    l->last = NULL;
                else
                    node->next->prev = NULL;
            }

            rlm_sql_ocioracle_node_destroy(node);

            notfounded = 0;
            l->size--;

            break;
        }
    }

    return notfounded;
}

/**
 * @return 1 on error
 * @return 0 on success (node founded and removed)
 */
int
rlm_sql_ocioracle_list_del_node_by_ptr (rlm_sql_ocioracle_list *l, void *ptr)
{
    rlm_sql_ocioracle_node *node = NULL, *prev = NULL;
    void *data = NULL;
    int notfounded = 1;

    if (!l || !ptr || !l->size) return 1;

    for (node = l->first; node && l->size;
         prev = node,
         node = rlm_sql_ocioracle_node_get_next(node)) {

        data = rlm_sql_ocioracle_node_get_data(node);
        if (!data) return 1;

        if (ptr == data) {
            if (prev) {
                prev->next = node->next;
                if (!node->next)
                    l->last = prev;
                else
                    node->next->prev = prev;

            } else {
                l->first = node->next;
                if (!node->next)
                    l->last = NULL;
                else
                    node->next->prev = NULL;
            }

            rlm_sql_ocioracle_node_destroy(node);
            notfounded = 0;
            l->size--;

            break;
        }
    }

    return notfounded;
}

/**
 * Destroy a list with string and free internal strings.
 * @return 1 on error
 * @return 0 on success
 */
int
rlm_sql_ocioracle_list_string_destroy (rlm_sql_ocioracle_list *l)
{
    int i;
    char *value = NULL;
    rlm_sql_ocioracle_node *n = NULL;

    if (!l) return 1;

    for (i = 0, n = rlm_sql_ocioracle_list_get_first(l);
         i < rlm_sql_ocioracle_list_get_size(l) && n;
         i++, n = rlm_sql_ocioracle_node_get_next(n)) {

        value = (char *) rlm_sql_ocioracle_node_get_data(n);
        if (value)
            free(value);

        rlm_sql_ocioracle_node_set_data(n, NULL);

    }  // end for i

    if (rlm_sql_ocioracle_list_destroy(l)) goto error;

    return 0;

error:

    return 1;
}

// vim: ts=4 shiftwidth=4 expandtab
