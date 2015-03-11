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

#include <freeradius-devel/ident.h>
RCSID("$Id: sql_ocioracle.c 3845 2012-07-30 10:57:40Z geaaru $")

#include <freeradius-devel/radiusd.h>
#include <sys/stat.h>

#include "rlm_sql.h"
#include "sql_ocioracle.h"
#include "sql_ocioracle_row.h"
#include "sql_ocioracle_field.h"
#include "list.h"

static short ocilib_not_initialized = 1;

// Local prototypes
static int
sql_release_statement(SQLSOCK *, SQL_CONFIG *);

static int
sql_reconnect(SQLSOCK *, rlm_sql_ocioracle_sock *, SQL_CONFIG *);


/**
 * Callback used by OCIOracle library
 */
static void sql_error_handler_cb(OCI_Error *error) {

   OCI_Connection *c = NULL;
   SQLSOCK *sqlsocket = NULL;
   rlm_sql_ocioracle_sock *o_sock = NULL;
   OCI_Statement *st = NULL;

   if (!error) return;

   c = OCI_ErrorGetConnection(error);
   st = OCI_ErrorGetStatement(error);

   if (c) {

      sqlsocket = (SQLSOCK *) OCI_GetUserData(c);

      if (sqlsocket) {

         o_sock = (rlm_sql_ocioracle_sock *) sqlsocket->conn;

         if (o_sock) {
		o_sock->errHandle = error;
		o_sock->errCode = OCI_ErrorGetOCICode(error) ?
			          OCI_ErrorGetOCICode(error) : OCI_ErrorGetInternalCode(error);
			
	 }

         radlog(L_ERR, "rlm_sql_ocioracle: Error to connection %d %s%s:\n"
                "OCICODE = %d\n%s",
                sqlsocket->id,
                (st ? "for query " : ""),
                (st ? OCI_GetSql(st) : ""),
		o_sock->errCode,
                OCI_ErrorGetString(error));


      } else {

         radlog(L_ERR, "rlm_sql_ocioracle: Error to unknown sql_socket %s%s:\n"
                "OCICODE = %d\n%s",
                (st ? "for query " : ""),
                (st ? OCI_GetSql(st) : ""),
                (OCI_ErrorGetOCICode(error) ? OCI_ErrorGetOCICode(error) :
                 (OCI_ErrorGetInternalCode(error) ? OCI_ErrorGetInternalCode(error) : 0)),
                OCI_ErrorGetString(error));


      }

   } else {

      radlog(L_ERR, "rlm_sql_ocioracle: Error to unknown connection %s%s:\n"
             "OCICODE = %d\n%s",
             (st ? "for query " : ""),
             (st ? OCI_GetSql(st) : ""),
             (OCI_ErrorGetOCICode(error) ? OCI_ErrorGetOCICode(error) :
              (OCI_ErrorGetInternalCode(error) ? OCI_ErrorGetInternalCode(error) : 0)),
             OCI_ErrorGetString(error));

   }

}

/*************************************************************************
 *
 *      Function: sql_error
 *
 *      Purpose: database specific error. Returns error associated with
 *               connection
 *
 *************************************************************************/
static const char *sql_error(SQLSOCK *sqlsocket, SQL_CONFIG *config) {

   static char msgbuf[SQLERROR_MSG_BUFFER];
   rlm_sql_ocioracle_sock *o_sock = NULL;

   o_sock = (rlm_sql_ocioracle_sock *) sqlsocket->conn;
   if (!o_sock) return "rlm_sql_ocioracle: no connection to db";

   memset((void *) msgbuf, (int)'\0', sizeof(msgbuf));

    if (o_sock->errCode) {
        snprintf(msgbuf, SQLERROR_MSG_BUFFER,
                 "OCICODE = %d\n", o_sock->errCode);
        return msgbuf;
    }

    return NULL;
}

/*************************************************************************
 *
 *  Function: sql_check_error
 *
 *  Purpose: check the error to see if the server is down
 *
 *************************************************************************/
static int sql_check_error(SQLSOCK *sqlsocket, SQL_CONFIG *config) {

    const char *err = NULL;
    rlm_sql_ocioracle_sock *o_sock = NULL;

    o_sock = (rlm_sql_ocioracle_sock *) sqlsocket->conn;

    if (o_sock->errCode == 3113 || o_sock->errCode == 3114) {

  	radlog(L_ERR,"rlm_sql_ocioracle: OCI_SERVER_NOT_CONNECTED: %s", err);
  	return SQL_DOWN;
    } else {
	radlog(L_ERR,"rlm_sql_ocioracle: OCI_SERVER_NORMAL");
    }

    return -1;
}

/*************************************************************************
 *
 * Function: sql_close
 *
 * Purpose: database specific close. Closes an open database
 *          connection and cleans up any open handles.
 *
 *************************************************************************/
static int sql_close(SQLSOCK *sqlsocket, SQL_CONFIG *config) {

   rlm_sql_ocioracle_sock *o_sock = (rlm_sql_ocioracle_sock *) sqlsocket->conn;

   sql_release_statement(sqlsocket, config);

   if (o_sock->conn) {
      OCI_ConnectionFree(o_sock->conn);
      o_sock->conn = NULL;
   }

   free(o_sock);
   sqlsocket->conn = NULL;

   return 0;
}


/*************************************************************************
 *
 *	Function: sql_init_socket
 *
 *	Purpose: Establish connection to the db
 *
 *************************************************************************/
static int sql_init_socket(SQLSOCK *sqlsocket, SQL_CONFIG *config) {

    const char *err = NULL;
    OCI_Error *errHandle = NULL;
    rlm_sql_ocioracle_sock *o_sock = NULL;

    // Initialize library
    if (ocilib_not_initialized) {

        if (!OCI_Initialize(sql_error_handler_cb, NULL,
                            OCI_ENV_THREADED | OCI_ENV_CONTEXT)) {

           radlog(L_ERR,
                  "rlm_sql_ocioracle: Couldn't init Oracle OCI Lib environment (OCI_Initialize())");
            return -1;
        }

        ocilib_not_initialized = 0;
    }

    // Allocate object
    if (!sqlsocket->conn) {
       sqlsocket->conn = (rlm_sql_ocioracle_sock *)
          rad_malloc(sizeof(rlm_sql_ocioracle_sock));
       if (!sqlsocket->conn)
          return -1;
    }
    memset(sqlsocket->conn,0,sizeof(rlm_sql_ocioracle_sock));
    o_sock = sqlsocket->conn;

    o_sock->rows = NULL;
    o_sock->curr_row = NULL;
    o_sock->pos = -1;
    o_sock->rs = NULL;
    o_sock->cursorHandle = NULL;
    o_sock->queryHandle = NULL;
    o_sock->errHandle = NULL;
    o_sock->results = NULL;
    o_sock->errCode = 0;
    o_sock->affected_rows = -1;

    radlog(L_INFO, "rlm_sql_ocioracle: try to connect to service name %s",
           config->sql_db);

    // Connect to database
    o_sock->conn = OCI_ConnectionCreate(config->sql_db,
                                        config->sql_login,
                                        config->sql_password,
                                        OCI_SESSION_DEFAULT);
    if (!o_sock->conn) {

       err = sql_error(sqlsocket,config);
        if (!err) {
            errHandle = OCI_GetLastError();
            err = OCI_ErrorGetString(errHandle);
        }
        radlog(L_ERR, "rlm_sql_ocioracle: Oracle connection failed: '%s'",
               (err ? err : "Error description not available"));
        return -1;
    }

    // Set UserData to rlm_sql_ocioracle_sock object
    OCI_SetUserData(o_sock->conn, sqlsocket);

    // Disable autocommit
    if (!OCI_SetAutoCommit(o_sock->conn, 0)) {
       err = sql_error(sqlsocket,config);
       if (!err) {
          errHandle = OCI_GetLastError();
          err = OCI_ErrorGetString(errHandle);
       }
       radlog(L_ERR, "rlm_sql_ocioracle: Error on disable autommit '%s'",
             (err ? err : "Error description not available"));
       return -1;
    }


    return 0;
}

/*************************************************************************
 *
 * Function: sql_release_statement
 *
 * Purpose: Free statement and Resultset data
 *
 *************************************************************************/
static int sql_release_statement(SQLSOCK *sqlsocket, SQL_CONFIG *config) {

    int i;
    rlm_sql_ocioracle_sock *o_sock = NULL;
    rlm_sql_ocioracle_node *n = NULL;
    rlm_sql_ocioracle_row *r = NULL;

    o_sock = (rlm_sql_ocioracle_sock *) (sqlsocket ? sqlsocket->conn : NULL);

    if (o_sock) {

        if (o_sock->cursorHandle) {
            OCI_StatementFree(o_sock->cursorHandle);
            o_sock->cursorHandle = NULL;
        }

        // OCI_Resultset is destroy by OCI_StatementFree
        o_sock->rs = NULL;
        // OCI_Error is destroy by OCI_StatementFree
        o_sock->errHandle = NULL;
        o_sock->errCode = 0;

        if (o_sock->queryHandle) {
            OCI_StatementFree(o_sock->queryHandle);
            o_sock->queryHandle = NULL;
        }

        if (o_sock->results) {
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

/*************************************************************************
 *
 *	Function: sql_destroy_socket
 *
 *	Purpose: Free socket and private connection data
 *
 *************************************************************************/
static int sql_destroy_socket(SQLSOCK *sqlsocket, SQL_CONFIG *config) {

    if (sqlsocket && sqlsocket->conn) {
        sql_close(sqlsocket, config);
        sqlsocket->conn = NULL;
    }

    return 0;
}

/*************************************************************************
 *
 *	Function: sql_num_fields
 *
 *	Purpose: database specific num_fields function. Returns number
 *               of columns from query
 *  In some case column number value is 0 if there are problem on PL/SQL procedure
 *  or other.
 *
 *************************************************************************/
static int sql_num_fields(SQLSOCK *sqlsocket, SQL_CONFIG *config) {

   int count = -1;
   rlm_sql_ocioracle_sock *o_sock = (sqlsocket ? sqlsocket->conn : NULL);
   rlm_sql_ocioracle_row *r = NULL;

   if (!o_sock) return -1;

   /* get the number of columns in the select list */
    if (o_sock->curr_row) {
        r = (rlm_sql_ocioracle_row *) rlm_sql_ocioracle_node_get_data(o_sock->curr_row);
        count = rlm_sql_ocioracle_row_get_colnum(r);
    } else if (o_sock->rs) {
        count = OCI_GetColumnCount(o_sock->rs);
        if (!count) {
            radlog(L_ERR,"rlm_sql_ocioracle: Error retrieving column count in sql_num_fields: %s",
                sql_error(sqlsocket, config));
            count = -1;
        }
    }

    return count;
}

/*************************************************************************
 *
 * Function: sql_query
 *
 * Purpose: Issue a non-SELECT query (ie: update/delete/insert) to
 *               the database.
 *  Pre: This query is done with autocommit enable
 *
 *************************************************************************/
static int sql_query(SQLSOCK *sqlsocket, SQL_CONFIG *config, char *querystr) {

    char plsqlCursorQuery = 0;
    int res = 0;
    unsigned int affected_rows = 0;
    rlm_sql_ocioracle_sock *o_sock = NULL;

    o_sock = (rlm_sql_ocioracle_sock *) (sqlsocket ? sqlsocket->conn : NULL);
    if (!o_sock) {
       radlog(L_ERR, "rlm_sql_ocioracle: unexpected invalid socket object");
       return SQL_DOWN;
    }

    /*if (config->sqltrace) DEBUG(querystr);*/
    sql_release_statement(sqlsocket, config);

    if (!o_sock->conn || !OCI_Ping(o_sock->conn)) {

       // Try to reconnecto to database
       if (sql_reconnect(sqlsocket, o_sock, config))
          return SQL_DOWN;
    }

    // Check if it is a procedure with a returned cursor
    plsqlCursorQuery = (strstr(querystr, RLM_SQL_OCIORACLE_CURSOR_PROCEDURE_STR) ? 1 : 0);

    o_sock->queryHandle = OCI_StatementCreate(o_sock->conn);
    if (plsqlCursorQuery)
        o_sock->cursorHandle = OCI_StatementCreate(o_sock->conn);

    if (!o_sock->queryHandle || (plsqlCursorQuery && !o_sock->cursorHandle)) {
       radlog(L_ERR,"rlm_sql_ocioracle: create OCI_Statement in sql_query: %s",
              querystr);
       goto error;
    }

    // Prepare Statement
    if (!OCI_Prepare(o_sock->queryHandle, querystr)) {
       radlog(L_ERR,"rlm_sql_ocioracle: prepare failed in sql_query for query %s",
              querystr);
       goto error;
    }

    // Bind Cursor returned statement
    if (plsqlCursorQuery && !OCI_BindStatement(o_sock->queryHandle,
                                               MT(RLM_SQL_OCIORACLE_CURSOR_PROCEDURE_STR),
                                               o_sock->cursorHandle)) {
       radlog(L_ERR,"rlm_sql_ocioracle: bind return cursor statetement.");
       goto error;
    }

    res = OCI_Execute(o_sock->queryHandle);

    o_sock->rs = (plsqlCursorQuery ?
                  OCI_GetResultset(o_sock->cursorHandle) :
                  OCI_GetResultset(o_sock->queryHandle));

    if (!res || (plsqlCursorQuery && !o_sock->rs)) {
       radlog(L_ERR,"rlm_sql_ocioracle: execute query failed in sql_query: %s", querystr);
       //return sql_check_error(sqlsocket, config);
       o_sock->affected_rows = -1;

    } else {

       affected_rows = OCI_GetAffectedRows(o_sock->queryHandle);
       o_sock->affected_rows = affected_rows;
       DEBUG("rlm_sql_ocioracle: Affected rows %u", o_sock->affected_rows);

    }

    // Commit
    OCI_Commit(o_sock->conn);

    return 0;

error:

    return sql_check_error(sqlsocket, config);
}


/*************************************************************************
 *
 *	Function: sql_select_query
 *
 *	Purpose: Issue a select query to the database
 *
 *************************************************************************/
static int sql_select_query(SQLSOCK *sqlsocket, SQL_CONFIG *config, char *querystr) {

   OCI_Column *col = NULL;
   rlm_sql_ocioracle_sock *o_sock = NULL;
   rlm_sql_ocioracle_row *r = NULL;
   rlm_sql_ocioracle_field *f = NULL;
   char plsqlCursorQuery = 0;
   int res = 0, colnum = 0, i, y;

   o_sock = (rlm_sql_ocioracle_sock *) (sqlsocket ? sqlsocket->conn : NULL);
   if (!o_sock) {
      radlog(L_ERR, "rlm_sql_ocioracle: unexpected invalid socket object");
      return SQL_DOWN;
   }

   /*if (config->sqltrace) DEBUG(querystr);*/
   sql_release_statement(sqlsocket, config);

   if (!o_sock->conn || !OCI_Ping(o_sock->conn)) {
       // Try to reconnecto to database
       if (sql_reconnect(sqlsocket, o_sock, config))
          return SQL_DOWN;
   }

   plsqlCursorQuery = (strstr(querystr, RLM_SQL_OCIORACLE_CURSOR_PROCEDURE_STR) ? 1 : 0);

   o_sock->queryHandle = OCI_StatementCreate(o_sock->conn);
   if (plsqlCursorQuery)
      o_sock->cursorHandle = OCI_StatementCreate(o_sock->conn);

   if (!o_sock->queryHandle || (plsqlCursorQuery && !o_sock->cursorHandle)) {
      goto error;
   }

   // Prepare Statement
   if (!OCI_Prepare(o_sock->queryHandle, querystr)) {
      radlog(L_ERR,"rlm_sql_ocioracle: prepare failed in sql_query for query %s",
             querystr);

      goto error;
   }

   // Bind Cursor returned statement
   if (plsqlCursorQuery && !OCI_BindStatement(o_sock->queryHandle,
                                              MT(RLM_SQL_OCIORACLE_CURSOR_PROCEDURE_STR),
                                              o_sock->cursorHandle)) {
      radlog(L_ERR,"rlm_sql_ocioracle: bind return cursor statetement.");
      goto error;
   }

   res = OCI_Execute(o_sock->queryHandle);

   o_sock->rs = (plsqlCursorQuery ?
         OCI_GetResultset(o_sock->cursorHandle) :
         OCI_GetResultset(o_sock->queryHandle));

   if (!res || (plsqlCursorQuery && !o_sock->rs)) {

      return sql_check_error(sqlsocket, config);
   }

   /*
    * Define where the output from fetch calls will go
    *
    * This is a gross hack, but it works - we convert
    * all data to strings for ease of use.  Fortunately, most
    * of the data we deal with is already in string format.
    */
   colnum = sql_num_fields(sqlsocket, config);
   if (!colnum) {
      radlog(L_ERR,"rlm_sql_ocioracle: query failed in sql_select_query: %s",
             querystr);
      goto error;
   }

   // Create rows list
   o_sock->rows = rlm_sql_ocioracle_list_create(o_sock);
   if (!o_sock->rows) {
      radlog(L_ERR,"rlm_sql_ocioracle: error on create list");
      goto error;
   }

   for (i = 0; OCI_FetchNext(o_sock->rs); i++, r = NULL) {

      // Create row object
      r = rlm_sql_ocioracle_row_create(colnum, i, o_sock->rows);
      if (!r) {
         radlog(L_ERR, "rlm_sql_ocioracle: error on create row object");
         goto error;
      }

      for (y = 1; y <= colnum; y++, col = NULL, f = NULL) {

         col = OCI_GetColumn(o_sock->rs, y);
         if (!col) {
            radlog(L_ERR, "rlm_sql_ocioracle: error on column at pos %d (row %d)",
                  y, i);
            goto error;
         }

         f = rlm_sql_ocioracle_field_create(r);
         if (!f) {
            radlog(L_ERR, "rlm_sql_ocioracle: error on create field at pos %d (row %d)",
                  y, i);
            goto error;
         }

         if (rlm_sql_ocioracle_field_store_data(f, col, o_sock->rs)) {
            radlog(L_ERR, "rlm_sql_ocioracle: error on store data field at pos %d (row %d)",
                  y, i);
            goto error;
         }

         if (rlm_sql_ocioracle_row_set_field(r, f, y - 1)) {
            radlog(L_ERR, "rlm_sql_ocioracle: error on set field on row at pos %d (row %d)",
                  y, i);
            goto error;
         }

      } // end for y

      // Add row to list
      if (rlm_sql_ocioracle_list_add_node_data(o_sock->rows, r)) {
         radlog(L_ERR, "rlm_sql_ocioracle: error on add row to list at pos %d", i);
         goto error;
      }

   } // end for i

   o_sock->pos = 0;
   o_sock->curr_row = rlm_sql_ocioracle_list_get_first(o_sock->rows);

   return 0;

error:

   return sql_check_error(sqlsocket, config);
}


/*************************************************************************
 *
 * Function: sql_store_result
 *
 * Purpose: database specific store_result function. Returns a result
 *               set for the query.
 *
 *************************************************************************/
static int sql_store_result(SQLSOCK *sqlsocket, SQL_CONFIG *config) {
   /* Not needed for Oracle */
   return 0;
}


/*************************************************************************
 *
 * Function: sql_num_rows
 *
 * Purpose: database specific num_rows. Returns number of rows in
 *          query
 *
 *************************************************************************/
static int sql_num_rows(SQLSOCK *sqlsocket, SQL_CONFIG *config) {

   rlm_sql_ocioracle_sock *o_sock = NULL;

   o_sock = (rlm_sql_ocioracle_sock *) (sqlsocket ?  sqlsocket->conn : NULL);

   return (o_sock && o_sock->rows ? rlm_sql_ocioracle_list_get_size(o_sock->rows) : 0);
}


/*************************************************************************
 *
 * Function: sql_fetch_row
 *
 * Purpose: database specific fetch_row. Returns a SQL_ROW struct
 *               with all the data for the query in 'sqlsocket->row'. Returns
 *          0 on success, -1 on failure, SQL_DOWN if database is down.
 *
 *************************************************************************/
static int sql_fetch_row (SQLSOCK *sqlsocket, SQL_CONFIG *config) {

   int ans = 0, i;
   rlm_sql_ocioracle_row *r = NULL;
   rlm_sql_ocioracle_sock *o_sock = NULL;

   o_sock = (rlm_sql_ocioracle_sock *) (sqlsocket ?  sqlsocket->conn : NULL);
   if (!o_sock) {
      radlog(L_ERR, "rlm_sql_ocioracle: unexpected invalid socket object");
      return SQL_DOWN;
   }

   if (!o_sock->conn || !OCI_Ping(o_sock->conn)) {
      radlog(L_ERR, "rlm_sql_ocioracle: Socket not connected");
      return SQL_DOWN;
   }

   if (!o_sock->rs) return -1;

   sqlsocket->row = NULL;

   r = (rlm_sql_ocioracle_row *) rlm_sql_ocioracle_node_get_data(o_sock->curr_row);

   if (o_sock->results) { // Fetch local next rows

      // Free results
      if (o_sock->results) {
         for (i = 0; r && i < rlm_sql_ocioracle_row_get_colnum(r); i++) {

            if (o_sock->results[i]) {
               free(o_sock->results[i]);
               o_sock->results[i] = NULL;
            }

         } // end for i

         free(o_sock->results);
         o_sock->results = NULL;
      }

      o_sock->curr_row = rlm_sql_ocioracle_node_get_next(o_sock->curr_row);
      r = (rlm_sql_ocioracle_row *) rlm_sql_ocioracle_node_get_data(o_sock->curr_row);

   } // else Fetch currect row: first call of this functio


   if (r) {
      rlm_sql_ocioracle_row_dump(r, &o_sock->results);
      sqlsocket->row = o_sock->results;
   } else
      // No other fetch data
      ans = -1;

   return ans;
}



/*************************************************************************
 *
 *	Function: sql_free_result
 *
 *	Purpose: database specific free_result. Frees memory allocated
 *               for a result set
 *
 *************************************************************************/
static int sql_free_result(SQLSOCK *sqlsocket, SQL_CONFIG *config) {

   sql_release_statement(sqlsocket, config);

   return 0;
}



/*************************************************************************
 *
 *	Function: sql_finish_query
 *
 *	Purpose: End the query, such as freeing memory
 *
 *************************************************************************/
static int sql_finish_query(SQLSOCK *sqlsocket, SQL_CONFIG *config)
{
   return 0;
}



/*************************************************************************
 *
 *	Function: sql_finish_select_query
 *
 *	Purpose: End the select query, such as freeing memory or result
 *
 *************************************************************************/
static int sql_finish_select_query(SQLSOCK *sqlsocket, SQL_CONFIG *config) {

   sql_release_statement(sqlsocket, config);

   return 0;
}


/*************************************************************************
 *
 * Function: sql_affected_rows
 *
 * Purpose: Return the number of rows affected by the query (update,
 *          or insert)
 *
 *************************************************************************/
static int sql_affected_rows(SQLSOCK *sqlsocket, SQL_CONFIG *config) {

   rlm_sql_ocioracle_sock *o_sock = NULL;

   o_sock = (rlm_sql_ocioracle_sock *) (sqlsocket ?  sqlsocket->conn : NULL);

   DEBUG("rlm_sql_ocioracle: Affected rows %u", o_sock->affected_rows);

   return (int) o_sock->affected_rows;
}

/*************************************************************************
 *
 * Function: sql_reconnect
 *
 * Purpose: This method must be called when there is an error on connection
 *          and system try to reconnect without return SQL_DOWN.
 *
 * @return 1 on error
 * @return 0 on success
 *
 *************************************************************************/
static int sql_reconnect (SQLSOCK *sqlsocket,
                          rlm_sql_ocioracle_sock *o_sock,
                          SQL_CONFIG *config) {

   // Try to reconnect to database
   radlog(L_INFO, "rlm_sql_ocioracle: Found an expired connection."
                  " I try to reconnect to database");

   if (!o_sock || !config)
      return 1;

   if (o_sock->conn) {
      OCI_ConnectionFree(o_sock->conn);
      o_sock->conn = NULL;
   }

   // Connect to database
   o_sock->conn = OCI_ConnectionCreate(config->sql_db,
                                       config->sql_login,
                                       config->sql_password,
                                       OCI_SESSION_DEFAULT);

   if (!o_sock->conn) {
      radlog(L_ERR, "rlm_sql_ocioracle: Oracle connection failed on socket %d",
             sqlsocket->id);
      return 1;
   }

   // Set UserData to rlm_sql_ocioracle_sock object
   OCI_SetUserData(o_sock->conn, sqlsocket);

   // Disable autocommit
   if (!OCI_SetAutoCommit(o_sock->conn, 0)) {
      radlog(L_ERR, "rlm_sql_ocioracle: Error on disable autommit on socket %d",
            sqlsocket->id);
      return 1;
   }

   return 0;
}


/* Exported to rlm_sql */
rlm_sql_module_t rlm_sql_ocioracle = {
   "rlm_sql_ocioracle",
   sql_init_socket,
   sql_destroy_socket,
   sql_query,
   sql_select_query,
   sql_store_result,
   sql_num_fields,
   sql_num_rows,
   sql_fetch_row,
   sql_free_result,
   sql_error,
   sql_close,
   sql_finish_query,
   sql_finish_select_query,
   sql_affected_rows
};
