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
 *   Copyright 2017  Geaaru <geaaru@gmail.com>
 */
// RCSID("$Id$")

#include <freeradius-devel/radiusd.h>
#include <freeradius-devel/rad_assert.h>

#include <sys/stat.h>

#include "rlm_sql.h"
#include "sql_ocioracle.h"

static short ocilib_not_initialized = 1;

// Local prototypes
static int
sql_release_statement(rlm_sql_ocioracle_conn_t *);

static int
sql_reconnect(rlm_sql_handle_t *, rlm_sql_ocioracle_conn_t *,
              rlm_sql_config_t *);

static char *
sql_ocioracle_get_column_data(const void *, OCI_Column *, OCI_Resultset *);

static int
sql_ocioracle_set_row2array(rlm_sql_ocioracle_conn_t *, char **, int);

static int
sql_num_fields(rlm_sql_handle_t *, rlm_sql_config_t *);


/**
 * @return NULL on error or if field of the column is null.
 * @return a pointer to allocated string from input context
 */
static char *
sql_ocioracle_get_column_data(const void *ctx, OCI_Column *col,
                              OCI_Resultset *rs)
{
   char *ans = NULL;
   const char *string = NULL;
   unsigned int type = 0;
   unsigned int index = 0;

   if (!ctx || !col || !rs) return ans;

   type = OCI_ColumnGetType(col);
   index = OCI_GetColumnIndex(rs, OCI_ColumnGetName(col));

   if (OCI_IsNull(rs, index))
      // No data to store
      return ans;

   switch (type) {
      case OCI_CDT_TEXT: // dtext *
         string = OCI_GetString(rs, index);
         if (string) {
            ans = talloc_asprintf(ctx, "%s", string);
            if (!ans) {
               SQL_OCI_WARN("Error on allocate memory for string %s", string);
            }
         }
         break;
      case OCI_CDT_NUMERIC: // short, int, long long, double
         // Temporary handle all number as int.

         ans = talloc_asprintf(ctx, "%d", OCI_GetInt(rs, index));
         if (!ans) {
            SQL_OCI_WARN("Error on allocate memory for string %s", string);
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
         SQL_OCI_ERROR("type of column %s (%d) not permitted",
                       (char *) OCI_ColumnGetName(col),
                       type);
         break;
   } // end switch

   return ans;
}

/**
 * Assign row_data point to rows vector.
 * If size of the row array is less of target position
 * then a new array is created.
 *
 * @return 0 on success
 * @return 1 on error
 */
static int
sql_ocioracle_set_row2array(rlm_sql_ocioracle_conn_t *conn,
                            char **row_data,
                            int pos)
{
   int ans = 0, arr_size = 0, i = 0;
   char ***nrows = NULL;

   if (!conn || !row_data || pos < 0)
      return 1;

   arr_size = (int) talloc_array_length(conn->rows);

   if (pos >= arr_size) {

      nrows = talloc_zero_array(conn->ctx, char **, arr_size * 2);
      if (!nrows) {
         SQL_OCI_ERROR("Error on allocate a new rows array.");
         return 1;
      }

      for (i = 0; i < arr_size; i++)
         nrows[i] = conn->rows[i];

      talloc_free(conn->rows);

      nrows[i] = row_data;
      conn->rows = nrows;

   } else
      conn->rows[pos] = row_data;

   return ans;
}

/**
 * Callback used by OCIOracle library on handle error on connections.
 */
static void
sql_error_handler_cb(OCI_Error *error)
{
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

         SQL_OCI_ERROR("Error to connection %s%s:\nOCICODE = %d\n%s",
               (st ? "for query " : ""), (st ? OCI_GetSql(st) : ""),
               conn->errCode, OCI_ErrorGetString(error));


      } else {

         SQL_OCI_ERROR("Error to unknown sql_socket %s%s:\nOCICODE = %d\n%s",
               (st ? "for query " : ""),
               (st ? OCI_GetSql(st) : ""),
               (OCI_ErrorGetOCICode(error) ? OCI_ErrorGetOCICode(error) :
                (OCI_ErrorGetInternalCode(error) ? OCI_ErrorGetInternalCode(error) : 0)),
               OCI_ErrorGetString(error));


      }

   } else {

      SQL_OCI_ERROR("Error to unknown connection %s%s:\nOCICODE = %d\n%s",
            (st ? "for query " : ""), (st ? OCI_GetSql(st) : ""),
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

         SQL_OCI_ERROR("Couldn't init Oracle OCI Lib environment (OCI_Initialize())");
         return -1;
      }

      ocilib_not_initialized = 0;
   }

   SQL_OCI_INFO("OCI Lib correctly initialized.");

   return 0;
}

/**
 * Free statement and result set data from rlm_sql_ocioracle_conn_t object.
 */
static int
sql_release_statement(rlm_sql_ocioracle_conn_t *conn)
{
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

      // Free all memory allocations related with
      // query result.
      talloc_free_children(conn->ctx);

      conn->rows = NULL;
      conn->num_fetched_rows = 0;
      conn->pos = -1;
      conn->affected_rows = -1;
      conn->num_columns = 0;

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
   rlm_sql_ocioracle_conn_t *conn = NULL;

   // Allocate object
   MEM(conn = handle->conn = talloc_zero(handle, rlm_sql_ocioracle_conn_t));
   talloc_set_destructor(conn, _sql_socket_destructor);

   conn->ctx = talloc_new(conn);
   if (!conn->ctx) {
      SQL_OCI_ERROR("Error on allocate socket internal talloc context.");
      return RLM_SQL_ERROR;
   }

   conn->pos = -1;
   conn->affected_rows = -1;

   SQL_OCI_INFO("I try to connect to service name %s", config->sql_db);

   // Connect to database
   conn->conn = OCI_ConnectionCreate(config->sql_db,
                                     config->sql_login,
                                     config->sql_password,
                                     OCI_SESSION_DEFAULT);
   if (!conn->conn) {
      SQL_OCI_ERROR("Oracle connection failed: '%s'",
            (conn->errHandle ? OCI_ErrorGetString(conn->errHandle) :
             "Error description not available"));

      return RLM_SQL_ERROR;
   }

   // Set UserData to rlm_sql_handle_t object
   OCI_SetUserData(conn->conn, handle);

   // Disable autocommit
   if (!OCI_SetAutoCommit(conn->conn, 0)) {
      SQL_OCI_ERROR("Error on disable autommit '%s'",
            (conn->errHandle ? OCI_ErrorGetString(conn->errHandle) :
             "Error description not available"));
      return RLM_SQL_ERROR;
   }

   return RLM_SQL_OK;
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
               OCI_ErrorGetString(conn->errHandle));
      else
         error = talloc_asprintf(ctx, "OCICODE = %d", conn->errCode);
   } else
      error = talloc_asprintf(ctx, "rlm_sql_ocioracle: no connection to db");

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
   rlm_sql_ocioracle_conn_t *conn = NULL;

   conn = (rlm_sql_ocioracle_conn_t *) handle->conn;

   if (conn &&
         (conn->errCode == 3113 || conn->errCode == 3114)) {
      SQL_OCI_ERROR("OCI_SERVER_NOT_CONNECTED");
      ans = RLM_SQL_RECONNECT;
   } else {
      SQL_OCI_ERROR("OCI_SERVER_NORMAL");
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
sql_query(rlm_sql_handle_t *handle, rlm_sql_config_t *config, const char *querystr)
{
   char plsqlCursorQuery = 0;
   int res = 0;
   unsigned int affected_rows = 0;
   rlm_sql_ocioracle_conn_t *conn = NULL;

   conn = (rlm_sql_ocioracle_conn_t *) (handle ? handle->conn : NULL);
   if (!conn) {
      SQL_OCI_ERROR("unexpected invalid socket object");
      return RLM_SQL_RECONNECT;
   }

   /*if (config->sqltrace) DEBUG(querystr);*/
   sql_release_statement(conn);

   if (!conn->conn || !OCI_Ping(conn->conn)) {

      // Try to reconnecto to database
      if (sql_reconnect(handle, conn, config))
         return RLM_SQL_RECONNECT;
   }

   // Check if it is a procedure with a returned cursor
   plsqlCursorQuery = (strstr(querystr, RLM_SQL_OCIORACLE_CURSOR_PROCEDURE_STR) ? 1 : 0);

   conn->queryHandle = OCI_StatementCreate(conn->conn);
   if (plsqlCursorQuery)
      conn->cursorHandle = OCI_StatementCreate(conn->conn);

   if (!conn->queryHandle || (plsqlCursorQuery && !conn->cursorHandle)) {
      SQL_OCI_ERROR("Error on create OCI_Statement in sql_query: %s", querystr);
      goto error;
   }

   // Prepare Statement
   if (!OCI_Prepare(conn->queryHandle, querystr)) {
      SQL_OCI_ERROR("Error on prepare failed in sql_query for query %s", querystr);
      goto error;
   }

   // Bind Cursor returned statement
   if (plsqlCursorQuery && !OCI_BindStatement(conn->queryHandle,
                                              MT(RLM_SQL_OCIORACLE_CURSOR_PROCEDURE_STR),
                                              conn->cursorHandle)) {
      SQL_OCI_ERROR("Error on bind return cursor statetement.");
      goto error;
   }

   res = OCI_Execute(conn->queryHandle);

   conn->rs = (plsqlCursorQuery ?
               OCI_GetResultset(conn->cursorHandle) :
               OCI_GetResultset(conn->queryHandle));

   if (!res || (plsqlCursorQuery && !conn->rs)) {
      SQL_OCI_ERROR("Error on execute query failed in sql_query: %s", querystr);
      //return sql_check_error(sqlsocket, config);
      conn->affected_rows = -1;

   } else {

      affected_rows = OCI_GetAffectedRows(conn->queryHandle);
      conn->affected_rows = affected_rows;
      SQL_OCI_DEBUG("Affected rows %u", conn->affected_rows);

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
sql_select_query(rlm_sql_handle_t *handle, rlm_sql_config_t *config, const char *querystr)
{
   OCI_Column *col = NULL;
   rlm_sql_ocioracle_conn_t *conn = NULL;
   char **row_data = NULL;
   char plsqlCursorQuery = 0;
   int res = 0, colnum = 0, i, y;

   conn = (rlm_sql_ocioracle_conn_t *) (handle ? handle->conn : NULL);
   if (!conn) {
      SQL_OCI_ERROR("unexpected invalid socket object");
      return RLM_SQL_RECONNECT;
   }

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
      SQL_OCI_ERROR("prepare failed in sql_query for query %s",
            querystr);
      goto error;
   }

   // Bind Cursor returned statement
   if (plsqlCursorQuery && !OCI_BindStatement(conn->queryHandle,
                                              MT(RLM_SQL_OCIORACLE_CURSOR_PROCEDURE_STR),
                                              conn->cursorHandle)) {
      SQL_OCI_ERROR("bind return cursor statetement.");
      goto error;
   }

   res = OCI_Execute(conn->queryHandle);

   conn->rs = (plsqlCursorQuery ?
               OCI_GetResultset(conn->cursorHandle) :
               OCI_GetResultset(conn->queryHandle));

   if (!res || (plsqlCursorQuery && !conn->rs)) {
      return sql_check_error(handle, config);
   }

   colnum = sql_num_fields(handle, config);
   if (colnum <= 0) {
      SQL_OCI_ERROR("query failed in sql_select_query: %s", querystr);
      goto error;
   }

   // Create rows pointer array
   conn->rows = talloc_zero_array(conn->ctx, char **,
                                  SQL_OCI_INITIAL_ROWS_SIZE);
   if (!conn->rows) {
      SQL_OCI_ERROR("error on create row array");
      goto error;
   }

   for (i = 0; OCI_FetchNext(conn->rs); i++, row_data = NULL) {

      conn->num_fetched_rows++;

      // Note: I use same father talloc ctx. I haven't time
      // to change parent of already allocated chunks.
      row_data = talloc_zero_array(conn->ctx, char *, colnum);
      if (!row_data) {
         SQL_OCI_ERROR("Error on allocate memory for row data");
         goto error;
      }

      if (sql_ocioracle_set_row2array(conn, row_data, i)) {
         SQL_OCI_ERROR("error on assign row pointer at position %d", i);
      }

      for (y = 1; y <= colnum; y++, col = NULL) {

         col = OCI_GetColumn(conn->rs, y);
         if (!col) {
            SQL_OCI_ERROR("error on column at pos %d (row %d)", y, i);
            goto error;
         }

         row_data[y-1] = sql_ocioracle_get_column_data((const void *) row_data,
                                                       col, conn->rs);

      } // end for y

   } // end for i

   conn->pos = 0;

   return RLM_SQL_OK;

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
   int ans = -1;
   rlm_sql_ocioracle_conn_t *conn = (handle ? handle->conn : NULL);

   if (!conn) return -1;

   /* get the number of columns in the select list */
    if (conn->num_columns) {
       ans = conn->num_columns;
    } else if (conn->rs) {
       ans = OCI_GetColumnCount(conn->rs);
       if (!ans) {
          SQL_OCI_ERROR("Error retrieving column count in sql_num_fields: %s",
                        (conn->errHandle ? OCI_ErrorGetString(conn->errHandle) :
                                           "Unexpected error."));
          ans = -1;
       }
       conn->num_columns = ans;
    }

    return ans;
}

/**
 * Return number of rows in query.
 */
static int
sql_num_rows(rlm_sql_handle_t *handle, rlm_sql_config_t *config)
{
   rlm_sql_ocioracle_conn_t *conn = NULL;

   conn = (rlm_sql_ocioracle_conn_t *) (handle ?  handle->conn : NULL);

   return (conn ? conn->num_fetched_rows : 0);
}

/**
 * Return number of rows affected by the query (update or insert).
 */
static int
sql_affected_rows(rlm_sql_handle_t *handle, rlm_sql_config_t *config)
{
   rlm_sql_ocioracle_conn_t *conn = NULL;

   conn = (rlm_sql_ocioracle_conn_t *) (handle ?  handle->conn : NULL);

   SQL_OCI_DEBUG("Affected rows %u", conn->affected_rows);

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
   int ans = RLM_SQL_OK;
   rlm_sql_ocioracle_conn_t *conn = NULL;

   conn = (rlm_sql_ocioracle_conn_t *) (handle ?  handle->conn : NULL);
   if (!conn) {
      SQL_OCI_ERROR("unexpected invalid socket object");
      return RLM_SQL_RECONNECT;
   }

   if (!conn->conn || !OCI_Ping(conn->conn)) {
      SQL_OCI_ERROR("Socket not connected");
      return RLM_SQL_RECONNECT;
   }

   if (!conn->rs) return RLM_SQL_ERROR;

   handle->row = NULL;

   if (conn->num_fetched_rows) {
      if (conn->pos < conn->num_fetched_rows) {
         handle->row = conn->rows[conn->pos];
      }

      conn->pos++;
   } else
      ans = RLM_SQL_NO_MORE_ROWS;

   return ans;
}

/**
 * Free memory allocated for a result set.
 * @return RLM_SQL_OK always
 */
static sql_rcode_t
sql_free_result(rlm_sql_handle_t *handle, rlm_sql_config_t *config)
{
   sql_release_statement(handle ? handle->conn : NULL);

   return RLM_SQL_OK;
}

/**
 * Function called at end of the query for update/insert. Nothing to do in my case.
 * @return RLM_SQL_OK on succes
 */
static sql_rcode_t
sql_finish_query(rlm_sql_handle_t *handle, rlm_sql_config_t *config)
{
   return RLM_SQL_OK;
}

/**
 * Function called at end of select query. Nothing to do in my case.
 * @return RLM_SQL_OK always
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
   SQL_OCI_INFO("Found an expired connection. I try to reconnect to database");

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
      SQL_OCI_ERROR("Oracle connection failed on socket.");
      return 1;
   }

   // Set UserData to rlm_sql_handle_t object
   OCI_SetUserData(conn->conn, handle);

   // Disable autocommit
   if (!OCI_SetAutoCommit(conn->conn, 0)) {
      SQL_OCI_ERROR("Error on disable autommit on socket.");
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
