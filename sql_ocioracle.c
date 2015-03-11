/*
 *   sql_ocioracle.c Oracle (OCIlib) routines for rlm_sql
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 *   Copyright 2011  Ge@@ru <geaaru@gmail.com>
 */
RCSID("$Id$")

#include <freeradius-devel/radiusd.h>
#include <freeradius-devel/rad_assert.h>

#include <sys/stat.h>

#include "rlm_sql.h"
#include "sql_ocioracle.h"
#include "sql_ocioracle_row.h"
#include "sql_ocioracle_field.h"
#include "list.h"

static short ocilib_not_initialized = 1;

// Local prototypes
static int
sql_release_statement(rlm_sql_ocioracle_conn_t *);

static int
sql_reconnect(SQLSOCK *, rlm_sql_ocioracle_sock *, SQL_CONFIG *);


/**
 * Callback used by OCIOracle library
 */
static void
sql_error_handler_cb(OCI_Error *error) {

  OCI_Connection *c = NULL;
  rlm_sql_handle_t *handle = NULL;
  rlm_sql_ocioracle_conn_t *conn = NULL;
  OCI_Statement *st = NULL;

  if (!error) return;

  c = OCI_ErrorGetConnection(error);
  st = OCI_ErrorGetStatement(error);

  if (c) {

    handle = (rlm_sql_handle_t *) OCI_GetUserData(c);

    if (handle) {

      conn = (rlm_sql_ocioracle_conn_t *) handle->conn;

      if (conn) {
        conn->errHandle = error;
        conn->errCode = OCI_ErrorGetOCICode(error) ?
          OCI_ErrorGetOCICode(error) : OCI_ErrorGetInternalCode(error);

      }

      ERROR("rlm_sql_ocioracle: Error to connection %d %s%s:\n"
          "OCICODE = %d\n%s",
          sqlsocket->id,
          (st ? "for query " : ""),
          (st ? OCI_GetSql(st) : ""),
          conn->errCode,
          OCI_ErrorGetString(error));


    } else {

      ERROR("rlm_sql_ocioracle: Error to unknown sql_socket %s%s:\n"
          "OCICODE = %d\n%s",
          (st ? "for query " : ""),
          (st ? OCI_GetSql(st) : ""),
          (OCI_ErrorGetOCICode(error) ? OCI_ErrorGetOCICode(error) :
           (OCI_ErrorGetInternalCode(error) ? OCI_ErrorGetInternalCode(error) : 0)),
          OCI_ErrorGetString(error));


    }

  } else {

    ERROR("rlm_sql_ocioracle: Error to unknown connection %s%s:\n"
        "OCICODE = %d\n%s",
        (st ? "for query " : ""),
        (st ? OCI_GetSql(st) : ""),
        (OCI_ErrorGetOCICode(error) ? OCI_ErrorGetOCICode(error) :
         (OCI_ErrorGetInternalCode(error) ? OCI_ErrorGetInternalCode(error) : 0)),
        OCI_ErrorGetString(error));

  }

}

/**
 * This function is called on initialization phase of the radius server.
 *
 * @return 0 on success
 * @return -1 on error
 */
static int
mod_instantiate (CONF_SECTION *conf, rlm_sql_config_t *config)

{
   // Initialize library
   if (ocilib_not_initialized) {

      if (!OCI_Initialize(sql_error_handler_cb, NULL,
               OCI_ENV_THREADED | OCI_ENV_CONTEXT)) {

         ERROR("rlm_sql_ocioracle: Couldn't init Oracle OCI Lib environment (OCI_Initialize())");
         return -1;
      }

      ocilib_not_initialized = 0;
   }

   INFO("rlm_sql_ocioracle: OCI Lib correctly initialized.")

   return 0;
}

/**
 * Free statement and result set data from rlm_sql_ocioracle_conn_t object.
 */
static int
sql_release_statement(rlm_sql_ocioracle_conn_t *conn)
{
   int i;
   rlm_sql_ocioracle_node *n = NULL;
   rlm_sql_ocioracle_row *r = NULL;

   if (conn) {

      if (conn->cursorHandle) {
         OCI_StatementFree(conn->cursorHandle);
         conn->cursorHandle = NULL;
      }

      // OCI_Resultset is destroy by OCI_StatementFree
      conn->rs = NULL;
      // OCI_Error is destroy by OCI_StatementFree
      conn->errHandle = NULL;
      conn->errCode = 0;

      if (conn->queryHandle) {
         OCI_StatementFree(conn->queryHandle);
         conn->queryHandle = NULL;
      }

      if (conn->results) {
         r = (rlm_sql_ocioracle_row *)
            rlm_sql_ocioracle_node_get_data(o_sock->curr_row);
         for (i = 0; r && i < rlm_sql_ocioracle_row_get_colnum(r); i++) {

            if (o_sock->results[i]) {
               free(o_sock->results[i]);
               o_sock->results[i] = NULL;
            }

         } // end for i

         free(o_sock->results);
         o_sock->results = NULL;
      }

      o_sock->curr_row = NULL;
      o_sock->pos = -1;
      o_sock->affected_rows = -1;

      if (o_sock->rows) {
         for (i = 0, n = rlm_sql_ocioracle_list_get_first(o_sock->rows);
               i < rlm_sql_ocioracle_list_get_size(o_sock->rows) && n;
               i++, n = rlm_sql_ocioracle_node_get_next(n), r = NULL) {

            r = (rlm_sql_ocioracle_row *)
               rlm_sql_ocioracle_node_get_data(n);

            if (r) {
               rlm_sql_ocioracle_row_destroy(r);
               rlm_sql_ocioracle_node_set_data(n, NULL);
            }

         } // end for i

         rlm_sql_ocioracle_list_destroy(o_sock->rows);
         o_sock->rows = NULL;
      }
   }

   return 0;
}


/**
 * Destructor callback for talloc that is executed on free of rlm_sql_ocioracle_conn_t object.
 * When connection is close.
 * @return 0 on success
 */
static int
_sql_socket_destructor(rlm_sql_ocioracle_conn_t *conn)
{
   sql_release_statement(conn);

   if (conn->conn) {
      OCI_ConnectionFree(conn->conn);
      conn->conn = NULL;
   }

   return 0;
}

/**
 * Function related with connection initialization.
 * @return RLM_SQL_ERROR on error
 * @return RLM_SQL_OK on success
 */
static sql_rcode_t
sql_socket_init(rlm_sql_handle_t *handle, rlm_sql_config_t *config)
{
    const char *err = NULL;
    OCI_Error *errHandle = NULL;
    rlm_sql_ocioracle_conn_t *conn = NULL;

    // Allocate object
    MEM(conn = handle->conn = talloc_zero(handle, rlm_sql_ocioracle_conn_t));
    talloc_set_destructor(conn, _sql_socket_destructor);

    conn->rows = NULL;
    conn->curr_row = NULL;
    conn->pos = -1;
    conn->rs = NULL;
    conn->cursorHandle = NULL;
    conn->queryHandle = NULL;
    conn->errHandle = NULL;
    conn->results = NULL;
    conn->errCode = 0;
    conn->affected_rows = -1;

    INFO("rlm_sql_ocioracle: I try to connect to service name %s", config->sql_db);

    // Connect to database
    conn->conn = OCI_ConnectionCreate(config->sql_db,
                                      config->sql_login,
                                      config->sql_password,
                                      OCI_SESSION_DEFAULT);
    if (!conn->conn) {
        ERROR("rlm_sql_ocioracle: Oracle connection failed: '%s'",
              (conn->errHandle ? OCI_ErrorGetString(conn->errHandle) :
               "Error description not available"));

        return RLM_SQL_ERROR;
    }

    // Set UserData to rlm_sql_handle_t object
    OCI_SetUserData(conn->conn, handle);

    // Disable autocommit
    if (!OCI_SetAutoCommit(conn->conn, 0)) {
       ERROR("rlm_sql_ocioracle: Error on disable autommit '%s'",
              (conn->errHandle ? OCI_ErrorGetString(conn->errHandle) :
               "Error description not available"));
       return RLM_SQL_ERROR;
    }


    return 0;
}

/**
 * Retrieves any errors associated with the connection handle.
 *
 * @note Caller should free any memory allocated in ctx (talloc_free_children()).
 *
 * @param ctx to allocate temporary error buffers in.
 * @param out Array of sql_log_entrys to fill.
 * @param outlen Length of out array.
 * @param handle rlm_sql connection handle.
 * @param config rlm_sql config.
 *
 * @return number of errors written to the sql_log_entry array.
 */
static size_t
sql_error(TALLOC_CTX *ctx, sql_log_entry_t out[], size_t outlen,
          rlm_sql_handle_t *handle, rlm_sql_config_t *config)
{
   const char *error;
   rlm_sql_ocioracle_conn_t *conn = NULL;

   rad_assert(outlen > 0);

   conn = (rlm_sql_ocioracle_conn_t *) handle->conn;
   if (conn && conn->errCode) {
      if (conn->errHandle)
         error = talloc_asprintf(ctx, "OCICODE = %d, %s", conn->errCode,
                                 OCI_ErrorGetString(conn->errCode));
      else
         error = talloc_asprintf(ctx, "OCICODE = %d", conn->errCode);
   } else {
      error = talloc_asprintf(ctx, "rlm_sql_ocioracle: no connection to db");
   }

   out[0].type = L_ERR;
   out[0].msg = error;

   return 1;
}

/**
 * Analyse the last error that occurred on the socket, and determine an action
 *
 * @param server Socket from which to extract the server error. May be NULL.
 * @param client_errno Error from the client.
 * @return an action for rlm_sql to take.
 */
static int
sql_check_error(rlm_sql_handle_t *handle, rlm_sql_config_t *config)
{
   int ans = -1;
   rlm_sql_ocioracle_sock *o_sock = NULL;

   o_sock = (rlm_sql_ocioracle_sock *) handle->conn;

   if (o_sock &&
       (o_sock->errCode == 3113 || o_sock->errCode == 3114)) {
      ERROR("rlm_sql_ocioracle: OCI_SERVER_NOT_CONNECTED");
      ans = RLM_SQL_RECONNECT;
   } else {
      ERROR("rlm_sql_ocioracle: OCI_SERVER_NORMAL");
   }

   return ans;
}

/**
 * Function for handle a non-SELECT query (ex.: update/delete/insert) to
 * database.
 * PRE: this query is done with autocommit enable.
 *
 * @return RLM_SQL_RECONNECT when rlm_sql_ocioracle_conn_t is invalid.
 * @return RLM_SQL_ERROR on error
 * @return RLM_SQL_OK on success
 */
static sql_rcode_t
sql_query(rlm_sql_handle_t *handle, rlm_sql_config_t *config, char *querystr)
{
    char plsqlCursorQuery = 0;
    int res = 0;
    unsigned int affected_rows = 0;
    rlm_sql_ocioracle_conn_t *conn = NULL;

    conn = (rlm_sql_ocioracle_conn_t *) (handle ? handle->conn : NULL);
    if (!conn) {
       ERROR("rlm_sql_ocioracle: unexpected invalid socket object");
       return RLM_SQL_RECONNECT;
    }

    /*if (config->sqltrace) DEBUG(querystr);*/
    sql_release_statement(conn);

    if (!conn->conn || !OCI_Ping(conn->conn)) {

       // Try to reconnecto to database
       if (sql_reconnect(sqlsocket, o_sock, config))
          return RLM_SQL_RECONNECT;
    }

    // Check if it is a procedure with a returned cursor
    plsqlCursorQuery = (strstr(querystr, RLM_SQL_OCIORACLE_CURSOR_PROCEDURE_STR) ? 1 : 0);

    conn->queryHandle = OCI_StatementCreate(conn->conn);
    if (plsqlCursorQuery)
        conn->cursorHandle = OCI_StatementCreate(conn->conn);

    if (!conn->queryHandle || (plsqlCursorQuery && !conn->cursorHandle)) {
       ERROR("rlm_sql_ocioracle: create OCI_Statement in sql_query: %s", querystr);
       goto error;
    }

    // Prepare Statement
    if (!OCI_Prepare(conn->queryHandle, querystr)) {
       ERROR("rlm_sql_ocioracle: prepare failed in sql_query for query %s",
              querystr);
       goto error;
    }

    // Bind Cursor returned statement
    if (plsqlCursorQuery && !OCI_BindStatement(conn->queryHandle,
                                               MT(RLM_SQL_OCIORACLE_CURSOR_PROCEDURE_STR),
                                               conn->cursorHandle)) {
       ERROR("rlm_sql_ocioracle: bind return cursor statetement.");
       goto error;
    }

    res = OCI_Execute(conn->queryHandle);

    conn->rs = (plsqlCursorQuery ?
                OCI_GetResultset(conn->cursorHandle) :
                OCI_GetResultset(conn->queryHandle));

    if (!res || (plsqlCursorQuery && !conn->rs)) {
       ERROR("rlm_sql_ocioracle: execute query failed in sql_query: %s", querystr);
       //return sql_check_error(sqlsocket, config);
       conn->affected_rows = -1;

    } else {

       affected_rows = OCI_GetAffectedRows(conn->queryHandle);
       conn->affected_rows = affected_rows;
       DEBUG("rlm_sql_ocioracle: Affected rows %u", conn->affected_rows);

    }

    // Commit
    OCI_Commit(conn->conn);

    return RLM_SQL_OK;

error:

    return sql_check_error(handle, config);
}

/**
 * Purpose of this function is handle select query to the database.
 *
 * @return RLM_SQL_RECONNECT when rlm_sql_ocioracle_conn_t is invalid.
 * @return RLM_SQL_ERROR on error
 * @return RLM_SQL_OK on success
 */
static sql_rcode_t
sql_select_query(rlm_sql_handle_t *handle, rlm_sql_config_t *config, char *querystr)
{
   OCI_Column *col = NULL;
   rlm_sql_ocioracle_conn_t *conn = NULL;
   rlm_sql_ocioracle_row *r = NULL;
   rlm_sql_ocioracle_field *f = NULL;
   char plsqlCursorQuery = 0;
   int res = 0, colnum = 0, i, y;

   conn = (rlm_sql_ocioracle_conn_t *) (handle ? handle->conn : NULL);
   if (!conn) {
      ERROR("rlm_sql_ocioracle: unexpected invalid socket object");
      return RLM_SQL_RECONNECT;
   }

   /*if (config->sqltrace) DEBUG(querystr);*/
   sql_release_statement(conn);

   if (!conn->conn || !OCI_Ping(conn->conn)) {
       // Try to reconnecto to database
       if (sql_reconnect(handle, conn, config))
          return RLM_SQL_RECONNECT;
   }

   plsqlCursorQuery = (strstr(querystr, RLM_SQL_OCIORACLE_CURSOR_PROCEDURE_STR) ? 1 : 0);

   conn->queryHandle = OCI_StatementCreate(conn->conn);
   if (plsqlCursorQuery)
      conn->cursorHandle = OCI_StatementCreate(conn->conn);

   if (!conn->queryHandle || (plsqlCursorQuery && !conn->cursorHandle)) {
      goto error;
   }

   // Prepare Statement
   if (!OCI_Prepare(conn->queryHandle, querystr)) {
      ERROR("rlm_sql_ocioracle: prepare failed in sql_query for query %s",
            querystr);
      goto error;
   }

   // Bind Cursor returned statement
   if (plsqlCursorQuery && !OCI_BindStatement(conn->queryHandle,
                                              MT(RLM_SQL_OCIORACLE_CURSOR_PROCEDURE_STR),
                                              conn->cursorHandle)) {
      ERROR("rlm_sql_ocioracle: bind return cursor statetement.");
      goto error;
   }

   res = OCI_Execute(conn->queryHandle);

   conn->rs = (plsqlCursorQuery ?
         OCI_GetResultset(conn->cursorHandle) :
         OCI_GetResultset(conn->queryHandle));

   if (!res || (plsqlCursorQuery && !conn->rs)) {
      return sql_check_error(handle, config);
   }

   /*
    * Define where the output from fetch calls will go
    *
    * This is a gross hack, but it works - we convert
    * all data to strings for ease of use.  Fortunately, most
    * of the data we deal with is already in string format.
    */
   colnum = sql_num_fields(handle, config);
   if (!colnum) {
      ERROR("rlm_sql_ocioracle: query failed in sql_select_query: %s",
            querystr);
      goto error;
   }

   // Create rows list
   conn->rows = rlm_sql_ocioracle_list_create(conn);
   if (!conn->rows) {
      ERROR("rlm_sql_ocioracle: error on create list");
      goto error;
   }

   for (i = 0; OCI_FetchNext(o_sock->rs); i++, r = NULL) {

      // Create row object
      r = rlm_sql_ocioracle_row_create(colnum, i, conn->rows);
      if (!r) {
         ERROR("rlm_sql_ocioracle: error on create row object");
         goto error;
      }

      for (y = 1; y <= colnum; y++, col = NULL, f = NULL) {

         col = OCI_GetColumn(conn->rs, y);
         if (!col) {
            ERROR("rlm_sql_ocioracle: error on column at pos %d (row %d)", y, i);
            goto error;
         }

         f = rlm_sql_ocioracle_field_create(r);
         if (!f) {
            ERROR("rlm_sql_ocioracle: error on create field at pos %d (row %d)",
                  y, i);
            goto error;
         }

         if (rlm_sql_ocioracle_field_store_data(f, col, conn->rs)) {
            ERROR("rlm_sql_ocioracle: error on store data field at pos %d (row %d)",
                  y, i);
            goto error;
         }

         if (rlm_sql_ocioracle_row_set_field(r, f, y - 1)) {
            ERROR("rlm_sql_ocioracle: error on set field on row at pos %d (row %d)",
                  y, i);
            goto error;
         }

      } // end for y

      // Add row to list
      if (rlm_sql_ocioracle_list_add_node_data(conn->rows, r)) {
         ERROR("rlm_sql_ocioracle: error on add row to list at pos %d", i);
         goto error;
      }

   } // end for i

   conn->pos = 0;
   conn->curr_row = rlm_sql_ocioracle_list_get_first(conn->rows);

   return 0;

error:

   return sql_check_error(handle, config);
}

/**
 * Return number of columns from query.
 * In some case column number value is 0 if there are problem on PL/SQL procedure
 * or other.
 */
static int
sql_num_fields(rlm_sql_handle_t *handle, rlm_sql_config_t *config)
{
   int count = -1;
   rlm_sql_ocioracle_conn_t *conn = (handle ? handle->conn : NULL);
   rlm_sql_ocioracle_row *r = NULL;

   if (!conn) return -1;

   /* get the number of columns in the select list */
    if (conn->curr_row) {
        r = (rlm_sql_ocioracle_row *) rlm_sql_ocioracle_node_get_data(conn->curr_row);
        count = rlm_sql_ocioracle_row_get_colnum(r);
    } else if (conn->rs) {
        count = OCI_GetColumnCount(conn->rs);
        if (!count) {
            ERROR("rlm_sql_ocioracle: Error retrieving column count in sql_num_fields: %s",
                  (conn->errHandle ? OCI_ErrorGetString(conn->errHandle) :
                   "Unexpected error."));
            count = -1;
        }
    }

    return count;
}

/**
 * Return number of rows in query.
 */
static int
sql_num_rows(rlm_sql_handle_t *handle, rlm_sql_config_t *config)
{
   rlm_sql_ocioracle_conn_t *conn = NULL;

   conn = (rlm_sql_ocioracle_conn_t *) (handle ?  handle->conn : NULL);

   return (conn && conn->rows ? rlm_sql_ocioracle_list_get_size(conn->rows) : 0);
}

/**
 * Database specific store_result function. Returns a result
 * set for the query. Not needed for Oracle.
 */
static sql_rcode_t
sql_store_result(rlm_sql_handle_t *handle, rlm_sql_config_t *config)
{
   /* Not needed for Oracle */
   return RLM_SQL_OK;
}

/**
 * Return number of rows affected by the query (update or insert).
 */
static int
sql_affected_rows(rlm_sql_handle_t *handle, rlm_sql_config_t *config)
{
   rlm_sql_ocioracle_conn_t *conn = NULL;

   conn = (rlm_sql_ocioracle_conn_t *) (handle ?  handle->conn : NULL);

   DEBUG("rlm_sql_ocioracle: Affected rows %u", conn->affected_rows);

   return (int) conn->affected_rows;
}

/**
 * Database specific fetch row.
 * @return RLM_SQL_RECONNECT if connection is not available.
 * @return RLM_SQL_ERROR on error
 * @return RLM_SQL_OK on success
 */
static sql_rcode_t
sql_fetch_row (rlm_sql_handle_t *handle, rlm_sql_config_t *config)
{
   int ans = RLM_SQL_OK, i;
   rlm_sql_ocioracle_row *r = NULL;
   rlm_sql_ocioracle_conn_t *conn = NULL;

   conn = (rlm_sql_ocioracle_conn_t *) (handle ?  handle->conn : NULL);
   if (!conn) {
      ERROR("rlm_sql_ocioracle: unexpected invalid socket object");
      return RLM_SQL_RECONNECT;
   }

   if (!conn->conn || !OCI_Ping(conn->conn)) {
      ERROR("rlm_sql_ocioracle: Socket not connected");
      return RLM_SQL_RECONNECT;
   }

   if (!conn->rs) return RLM_SQL_ERROR;

   handle->row = NULL;

   r = (rlm_sql_ocioracle_row *) rlm_sql_ocioracle_node_get_data(conn->curr_row);

   if (conn->results) { // Fetch local next rows

      // Free results
      for (i = 0; r && i < rlm_sql_ocioracle_row_get_colnum(r); i++) {

         if (conn->results[i]) {
            free(conn->results[i]);
            conn->results[i] = NULL;
         }

      } // end for i

      free(conn->results);
      conn->results = NULL;

      conn->curr_row = rlm_sql_ocioracle_node_get_next(conn->curr_row);
      r = (rlm_sql_ocioracle_row *) rlm_sql_ocioracle_node_get_data(conn->curr_row);

   } // else Fetch currect row: first call of this functio

   if (r) {
      rlm_sql_ocioracle_row_dump(r, &conn->results);
      handle->row = conn->results;
   } else
      // No other fetch data
      ans = RLM_SQL_ERROR; // TODO: check if is correct set -1

   return ans;
}

/**
 * Free memory allocated for a result set.
 * @return RLM_SQL_OK on succes
 */
static sql_rcode_t
sql_free_result(rlm_sql_handle_t *handle, rlm_sql_config_t *config)
{
   sql_release_statement(handle ? handle->conn : NULL);

   return RLM_SQL_OK;
}


/**
 * Function called at end of the query for update/insert. Nothing to do in my case.
 */
static sql_rcode_t
sql_finish_query(rlm_sql_handle_t *handle, rlm_sql_config_t *config)
{
   return RLM_SQL_OK;
}

/**
 * Function called at end of select query. Nothing to do in my case.
 */
static sql_rcode_t
sql_finish_select_query(rlm_sql_handle_t *handle, rlm_sql_config_t *config)
{
   sql_release_statement(handle ? handle->conn : NULL);

   return RLM_SQL_OK;
}

/**
 * This method must be called when there is an error on connection
 * and system try to reconnect without return SQL_DOWN.
 *
 * @return 1 on error
 * @return 0 on success
 */
static int
sql_reconnect (rlm_sql_handle_t *handle,
               rlm_sql_ocioracle_conn_t *conn,
               rlm_sql_config_t *config)
{
   // Try to reconnect to database
   INFO("rlm_sql_ocioracle: Found an expired connection. I try to reconnect to database");

   if (!conn || !config)
      return 1;

   if (conn->conn) {
      OCI_ConnectionFree(conn->conn);
      conn->conn = NULL;
   }

   // Connect to database
   conn->conn = OCI_ConnectionCreate(config->sql_db,
                                     config->sql_login,
                                     config->sql_password,
                                     OCI_SESSION_DEFAULT);

   if (!conn->conn) {
      ERROR("rlm_sql_ocioracle: Oracle connection failed on socket %d",
            sqlsocket->id);
      return 1;
   }

   // Set UserData to rlm_sql_handle_t object
   OCI_SetUserData(conn->conn, handle);

   // Disable autocommit
   if (!OCI_SetAutoCommit(conn->conn, 0)) {
      ERROR("rlm_sql_ocioracle: Error on disable autommit on socket %d",
            sqlsocket->id);
      return 1;
   }

   return 0;
}

/* Exported to rlm_sql */
extern rlm_sql_module_t rlm_sql_ocioracle;
rlm_sql_module_t rlm_sql_ocioracle = {
   .name                    = "rlm_sql_ocioracle",
   .mod_instantiate         = mod_instantiate,
   .sql_socket_init         = sql_socket_init,
   .sql_query               = sql_query,
   .sql_select_query        = sql_select_query,
   .sql_store_result        = sql_store_result,
   .sql_num_fields          = sql_num_fields,
   .sql_num_rows            = sql_num_rows,
   .sql_affected_rows       = sql_affected_rows,
   .sql_fetch_row           = sql_fetch_row,
   .sql_free_result         = sql_free_result,
   .sql_error               = sql_error,
   .sql_finish_query        = sql_finish_query,
   .sql_finish_select_query = sql_finish_select_query,
};

// vim: ts=3 sw=3 expandtab
