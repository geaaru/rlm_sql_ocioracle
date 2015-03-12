Oracle alternative module for FreeRADIUS
========================================

This module allow you to use PL/SQL function that return a cursor.
Instead of use directly OCI oracle library I use OCILIB library for
communication with database.

How it works ?
---------------

Where there is a PL/SQL function that return a cursor just use string ":Ret_Cursor".

For example inside freeradius/sql/ocioracle/dialup.conf you can have a row like this:

authorize_check_query = "begin :Ret_Cursor := pkRadius.authorize_check_query('%{NAS-IP-Address}', '%{SQL-User-Name}', '%{Calling-Station-Id}' ); end;"

Install (Freeradius 2.2.x)
--------------------------

For freeradius-2.2.x use tagged version 0.1.0.

Note: Freeradius-2.2.x module will not be supported in near future.


Install (Freeradius 3.x)
------------------------

* Check if present oracle-instantclient-basic
* Check if present ocilib library (http://orclib.sourceforge.net)
* Copy the directory rlm_sql_ocioracle sotto src/modules/rlm_sql/drivers/
* Add rlm_sql_ocioracle in src/modules/rlm_sql/stable
* Build as usual
  * ./configure
  * make
  * make install
* configure your tnsnames.ora file
* export TNS_ADMIN variable to directory path of your tnsnames.ora file.
* Edit your freeradius configuration to enable sql module and rlm_sql_ocioracle module
* Edit files under sql/ocioracle/*.conf files
* Run radiusd

Thanks
------

My special thanks to Vincent Rogier, creator of OCILIB library (http://orclib.sourceforge.net).

Credits
-------

Geaaru

