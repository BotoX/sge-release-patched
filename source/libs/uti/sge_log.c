/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "sge_log.h"

#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "sge.h"
#include "sge_time.h"
#include "sge_dstring.h"
#include "sgermon.h"
#include "sge_prog.h"
#include "sge_uidgid.h"
#include "sge_mtutil.h"
#include "msg_utilib.h"


typedef struct {
   pthread_rwlock_t rwlock;  
   char*            log_file;
   u_long32         log_level;
   int              log_as_admin_user;
   int              verbose;
   int              gui_log;
} log_state_t;

typedef struct {
   char  log_buffer[4 * MAX_STRING_SIZE]; /* a.k.a. SGE_EVENT */
} log_buffer_t;


static log_state_t Log_State;

static pthread_once_t log_once = PTHREAD_ONCE_INIT;
static pthread_key_t  log_buffer_key;

static void          log_once_init(void);
static void          log_buffer_destroy(void* theState);
static log_buffer_t* log_buffer_getspecific(pthread_key_t aKey);

static void sge_do_log(int, const char*, const char*); 


/****** uti/log/log_get_log_buffer() ******************************************
*  NAME
*     log_get_log_buffer() -- Return a buffer that can be used to build logging 
*                                   strings
*
*  SYNOPSIS
*     char *log_get_log_buffer(void) 
*
*  FUNCTION
*     Return a buffer that can be used to build logging strings
*
*  RESULT
*     char * 
*
******************************************************************************/
char *log_get_log_buffer(void)
{
   log_buffer_t *buf = NULL;

   pthread_once(&log_once, log_once_init);

   buf = log_buffer_getspecific(log_buffer_key);

   return buf->log_buffer;
}

/****** uti/log/log_state_get_log_level() ******************************************
*  NAME
*     log_state_get_log_level() -- Return log level.
*
*  SYNOPSIS
*     u_long32 log_state_get_log_level(void) 
*
*  FUNCTION
*     Return log level
*
*  RESULT
*     u_long32
*
******************************************************************************/
u_long32 log_state_get_log_level(void)
{
   u_long32 level = 0;

   pthread_once(&log_once, log_once_init);

   sge_rwlock_rdlock("Log_State_Lock", "log_state_get_log_level", __LINE__, &(Log_State.rwlock));

   level = Log_State.log_level;

   sge_rwlock_unlock("Log_State_Lock", "log_state_get_log_level", __LINE__, &(Log_State.rwlock));

   return level;
}

/****** uti/sge_log/log_state_get_log_file() ***********************************
*  NAME
*     log_state_get_log_file() -- get log file name
*
*  SYNOPSIS
*     const char* log_state_get_log_file(void) 
*
*  FUNCTION
*     Return name of current log file. The string returned may or may not 
*     contain a path.
*
*  INPUTS
*     void - none 
*
*  RESULT
*     const char* - log file name (with relative or absolute path)
*
*  NOTES
*     MT-NOTE: log_state_get_log_file() is not MT safe.
*     MT-NOTE:
*     MT-NOTE: It is safe, however, to call this function from within multiple
*     MT-NOTE: threads as long as no other thread does change 'Log_File'.
*
*  BUGS
*     BUGBUG-AD: This function should use something like a barrier for
*     BUGBUG-AD: synchronization.
*
*******************************************************************************/
const char *log_state_get_log_file(void)
{
   char* file = NULL;

   pthread_once(&log_once, log_once_init);

   sge_rwlock_rdlock("Log_State_Lock", "log_state_get_log_file", __LINE__, &(Log_State.rwlock));

   file = Log_State.log_file;

   sge_rwlock_unlock("Log_State_Lock", "log_state_get_log_file", __LINE__, &(Log_State.rwlock));

   return file;
}

/****** uti/log/log_state_get_log_verbose() ******************************************
*  NAME
*     log_state_get_log_verbose() -- Is verbose logging enabled?
*
*  SYNOPSIS
*     int log_state_get_log_verbose(void) 
*
*  FUNCTION
*     Is verbose logging enabled? 
*     With verbose logging enabled not only ERROR/CRITICAL messages are 
*     printed to stderr but also WARNING/INFO.
*
*  RESULT
*     int - 0 or 1 
*
*  SEE ALSO
*     uti/log/log_state_set_log_verbose()
******************************************************************************/
int log_state_get_log_verbose(void)
{
   int verbose = 0;

   pthread_once(&log_once, log_once_init);

   sge_rwlock_rdlock("Log_State_Lock", "log_state_get_log_verbose", __LINE__, &(Log_State.rwlock));

   verbose = Log_State.verbose;

   sge_rwlock_unlock("Log_State_Lock", "log_state_get_log_verbose", __LINE__, &(Log_State.rwlock));

   return verbose;
}

/****** uti/log/log_state_get_log_gui() ******************************************
*  NAME
*     log_state_get_log_gui() -- Is GUI logging enabled?
*
*  SYNOPSIS
*     int log_state_get_log_gui(void) 
*
*  FUNCTION
*     Is GUI logging enabled? 
*     With GUI logging enabled messages are printed to stderr/stdout.  
*
*  RESULT
*     int - 0 or 1 
*
*  SEE ALSO
*     uti/log/log_state_set_log_gui()
******************************************************************************/
int log_state_get_log_gui(void)
{
   int gui_log = 0;

   pthread_once(&log_once, log_once_init);

   sge_rwlock_rdlock("Log_State_Lock", "log_state_get_log_gui", __LINE__, &(Log_State.rwlock));

   gui_log = Log_State.gui_log;

   sge_rwlock_unlock("Log_State_Lock", "log_state_get_log_gui", __LINE__, &(Log_State.rwlock));

   return gui_log;
}

/****** uti/log/log_state_get_log_as_admin_user() ******************************************
*  NAME
*     log_state_get_log_as_admin_user() -- Needs sge_log() to change into admin user?
*
*  SYNOPSIS
*     trace_func_type log_state_get_log_as_admin_user(void) 
*
*  FUNCTION
*     Returns whether logging shall be done as admin user.
*
*  RESULT
*     int
*
*  SEE ALSO
*     uti/log/log_state_set_log_as_admin_user()
******************************************************************************/
int log_state_get_log_as_admin_user(void)
{
   int log_as_admin_user = 0;

   pthread_once(&log_once, log_once_init);

   sge_rwlock_rdlock("Log_State_Lock", "log_state_get_log_as_admin_user", __LINE__, &(Log_State.rwlock));

   log_as_admin_user = Log_State.log_as_admin_user;

   sge_rwlock_unlock("Log_State_Lock", "log_state_get_log_as_admin_user", __LINE__, &(Log_State.rwlock));

   return log_as_admin_user;
}

/****** uti/log/log_state_set_log_level() *****************************************
*  NAME
*     log_state_set_log_level() -- Set log level to be used.
*
*  SYNOPSIS
*     void log_state_set_log_level(int i) 
*
*  FUNCTION
*     Set log level to be used.
*
*  INPUTS
*     u_long32 
*
*  SEE ALSO
*     uti/log/log_state_get_log_level() 
******************************************************************************/
void log_state_set_log_level(u_long32 theLevel)
{ 
   pthread_once(&log_once, log_once_init);

   sge_rwlock_wrlock("Log_State_Lock", "log_state_set_log_level", __LINE__, &(Log_State.rwlock));

   Log_State.log_level = theLevel;

   sge_rwlock_unlock("Log_State_Lock", "log_state_set_log_level", __LINE__, &(Log_State.rwlock));

   return;
}

void log_state_set_log_file(char *theFile)
{
   pthread_once(&log_once, log_once_init);

   sge_rwlock_wrlock("Log_State_Lock", "log_state_set_log_file", __LINE__, &(Log_State.rwlock));

   Log_State.log_file = theFile;

   sge_rwlock_unlock("Log_State_Lock", "log_state_set_log_file", __LINE__, &(Log_State.rwlock));

   return;
}

/****** uti/log/log_state_set_log_verbose() *****************************************
*  NAME
*     log_state_set_log_verbose() -- Enable/disable verbose logging 
*
*  SYNOPSIS
*     void log_state_set_log_verbose(int i) 
*
*  FUNCTION
*     Enable/disable verbose logging 
*
*  INPUTS
*     int i - 0 or 1  
*
*  SEE ALSO
*     uti/log/log_state_get_log_verbose() 
******************************************************************************/
void log_state_set_log_verbose(int i)
{
   pthread_once(&log_once, log_once_init);

   sge_rwlock_wrlock("Log_State_Lock", "log_state_set_log_verbose", __LINE__, &(Log_State.rwlock));

   Log_State.verbose = i;

   sge_rwlock_unlock("Log_State_Lock", "log_state_set_log_verbose", __LINE__, &(Log_State.rwlock));

   return;
}

/****** uti/log/log_state_set_log_gui() ********************************************
*  NAME
*     log_state_set_log_gui() -- Enable/disable logging for GUIs 
*
*  SYNOPSIS
*     void log_state_set_log_gui(int i) 
*
*  FUNCTION
*     Enable/disable logging for GUIs 
*     With GUI logging enabled messages are printed to stderr/stdout.  
*
*  INPUTS
*     int i - 0 or 1 
******************************************************************************/
void log_state_set_log_gui(int i)
{
   pthread_once(&log_once, log_once_init);

   sge_rwlock_wrlock("Log_State_Lock", "log_state_set_log_gui", __LINE__, &(Log_State.rwlock));

   Log_State.gui_log = i;

   sge_rwlock_unlock("Log_State_Lock", "log_state_set_log_gui", __LINE__, &(Log_State.rwlock));

   return;
}   

/****** uti/log/log_state_set_log_as_admin_user() *****************************
*  NAME
*     log_state_set_log_as_admin_user() -- Enable/Disable logging as admin user 
*
*  SYNOPSIS
*     void log_state_set_log_as_admin_user(int i)
*
*  FUNCTION
*     This function enables/disables logging as admin user. This 
*     means that the function/macros switches from start user to 
*     admin user before any messages will be written. After that 
*     they will switch back to 'start' user. 
*
*  INPUTS
*     int i - 0 or 1 
*  
*  SEE ALSO
*     uti/uidgid/sge_switch2admin_user
*     uti/uidgid/sge_switch2start_user
******************************************************************************/
void log_state_set_log_as_admin_user(int i)
{
   pthread_once(&log_once, log_once_init);

   sge_rwlock_wrlock("Log_State_Lock", "log_state_set_log_as_admin_user", __LINE__, &(Log_State.rwlock));

   Log_State.log_as_admin_user = i;

   sge_rwlock_unlock("Log_State_Lock", "log_state_set_log_as_admin_user", __LINE__, &(Log_State.rwlock));

   return;
}

/****** uti/log/sge_log() *****************************************************
*  NAME
*     sge_log() -- Low level logging function 
*
*  SYNOPSIS
*     int sge_log(int log_level, char *mesg, char *file__, 
*                 char *func__, int line__) 
*
*  FUNCTION
*     Low level logging function. Used by various macros.
*     Do not use this function directly. 
*
*  INPUTS
*     int log_level - Logging Level
*     char *mesg    - Message
*     char *file__  - Filename
*     char *func__  - Function Name
*     int line__    - Line within 'file__'
*
*  RESULT
*     int - 0
*
*  SEE ALSO
*     uti/log/CRITICAL
*     uti/log/ERROR
*     uti/log/WARNING
*     uti/log/NOTICE
*     uti/log/INFO
*     uti/log/DEBUG
*
*  NOTES
*     MT-NOTE: sge_log() is not MT safe due to sge_switch2admin_user()
*     MT-NOTE: sge_log() can be used in clients where no admin user switching 
*     MT-NOTE: takes place
*     MT-NOTE: sge_log() is not MT safe due rmon_condition()
*     MT-NOTE: sge_log() can be used if DENTER_MAIN() is called only by one 
*     MT-NOTE: thread
******************************************************************************/
int sge_log(int log_level, const char *mesg, const char *file__, const char *func__, int line__) 
{
   char buf[128*4];
   char newline[2*4];
   int levelchar;
   char levelstring[32*4];

   DENTER(TOP_LAYER, "sge_log");

   pthread_once(&log_once, log_once_init);

   /* Make sure to have at least a one byte logging string */
   if (!mesg || mesg[0] == '\0') {
      sprintf(buf, MSG_LOG_CALLEDLOGGINGSTRING_S, 
              mesg ? MSG_LOG_ZEROLENGTH : MSG_POINTER_NULL);
      mesg = buf;
   }
/*
   if (mesg[strlen(mesg)-1] != '\n') {
      strcpy(newline,"\n");
   } else {
      strcpy(newline, "\0");
   }
*/

   DPRINTF(("%s %d %s%s", file__, line__, mesg, newline));

   /* quick exit if nothing to log */
   if (log_level > MAX(log_state_get_log_level(), LOG_WARNING)) {
      return 0;
   }

   if (!log_state_get_log_gui()) {
      return 0;
   }

   switch(log_level) {
      case LOG_PROF:
         strcpy(levelstring, MSG_LOG_PROFILING);
         levelchar = 'P';
         break;
      case LOG_CRIT:
         strcpy(levelstring, MSG_LOG_CRITICALERROR);
         levelchar = 'C';
         break;
      case LOG_ERR:
         strcpy(levelstring, MSG_LOG_ERROR);
         levelchar = 'E';
         break;
      case LOG_WARNING:
         strcpy(levelstring, "");
         levelchar = 'W';
         break;
      case LOG_NOTICE:
         strcpy(levelstring, "");
         levelchar = 'N';
         break;
      case LOG_INFO:
         strcpy(levelstring, "");
         levelchar = 'I';
         break;
      case LOG_DEBUG:
         strcpy(levelstring, "");
         levelchar = 'D';
         break;
      default:
         strcpy(levelstring, "");
         levelchar = 'L';
         break;
   }

   /* avoid double output in debug mode */
   if (!uti_state_get_daemonized() && !rmon_condition(TOP_LAYER, INFOPRINT) && 
       (log_state_get_log_verbose() || log_level <= LOG_ERR)) {
/*
      fprintf(stderr, "%s%s%s", levelstring, mesg, newline);
*/
      fprintf(stderr, "%s%s\n", levelstring, mesg);
   } 
   if (uti_state_get_mewho() == QMASTER || uti_state_get_mewho() == EXECD   || uti_state_get_mewho() == QSTD ||
       uti_state_get_mewho() == SCHEDD ||  uti_state_get_mewho() == SHADOWD || uti_state_get_mewho() == COMMD) {
      sge_do_log(levelchar, mesg, newline);
   }

   DEXIT;
   return 0;
} /* sge_log() */

/****** uti/sge_log/sge_do_log() ***********************************************
*  NAME
*     sge_do_log() -- Write message to log file 
*
*  SYNOPSIS
*     static void sge_do_log(int aLevel, const char *aMessage, const char 
*     *aNewLine) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     int aLevel           - log level
*     const char *aMessage - log message
*     const char *aNewLine - either newline or '\0' 
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: sge_do_log() is MT safe.
*
*******************************************************************************/
static void sge_do_log(int aLevel, const char *aMessage, const char *aNewLine) 
{
   int fd;

   if ((fd = SGE_OPEN3(log_state_get_log_file(), O_WRONLY | O_APPEND | O_CREAT, 0666)) >= 0) {
      char msg2log[4*MAX_STRING_SIZE];
      char date[256], tmp_date[256], time_buf[256];
      dstring ds, msg;
      sge_dstring_init(&ds, time_buf, sizeof(time_buf));
      sge_dstring_init(&msg, msg2log, sizeof(msg2log));
      sprintf(tmp_date, "%s", sge_ctime(0, &ds));
      sscanf(tmp_date, "%[^\n]", date);

      sge_dstring_sprintf(&msg, "%s|%s|%s|%c|%s\n",
              date,
              uti_state_get_sge_formal_prog_name(),
              uti_state_get_unqualified_hostname(),
              aLevel,
              aMessage);
/*
              aNewLine);
*/
      write(fd, msg2log, strlen(msg2log));
      close(fd);
   }

   return;
} /* sge_do_log() */

/****** uti/log/log_once_init() ********************************************
*  NAME
*     log_once_init() -- One-time logging initialization.
*
*  SYNOPSIS
*     static log_once_init(void) 
*
*  FUNCTION
*     Create access key for thread local storage. Register cleanup function.
*
*     This function must be called exactly once.
*
*  INPUTS
*     void - none
*
*  RESULT
*     void - none 
*
*  NOTES
*     MT-NOTE: log_once_init() is MT safe. 
*
*******************************************************************************/
static void log_once_init(void)
{
   pthread_key_create(&log_buffer_key, &log_buffer_destroy);

   pthread_rwlock_init(&(Log_State.rwlock), NULL);

   Log_State.log_file          = TMP_ERR_FILE_SNBU;
   Log_State.log_level         = LOG_WARNING;
   Log_State.log_as_admin_user = 0;
   Log_State.verbose           = 1;
   Log_State.gui_log           = 1;

   return;
} /* log_once_init */

/****** uti/log/log_buffer_destroy() ****************************************
*  NAME
*     log_buffer_destroy() -- Free thread local storage
*
*  SYNOPSIS
*     static void log_buffer_destroy(void* theState) 
*
*  FUNCTION
*     Free thread local storage.
*
*  INPUTS
*     void* theState - Pointer to memroy which should be freed.
*
*  RESULT
*     static void - none
*
*  NOTES
*     MT-NOTE: log_buffer_destroy() is MT safe.
*
*******************************************************************************/
static void log_buffer_destroy(void* theBuffer)
{
   sge_free((char*)theBuffer);
}

/****** uti/log/log_buffer_getspecific() ****************************************
*  NAME
*     log_buffer_getspecific() -- Get thread local log state
*
*  SYNOPSIS
*     static log_buffer_t* log_buffer_getspecific(pthread_key_t aKey) 
*
*  FUNCTION
*     Return thread local log state.
*
*     If a given thread does call this function for the first time, no thread
*     local log state is available for this particular thread. In this case the
*     thread local log state is allocated and set.
*
*  INPUTS
*     pthread_key_t aKey - Key for thread local log state 
*
*  RESULT
*     static log_buffer_t* - Pointer to thread local log state.
*
*  NOTES
*     MT-NOTE: log_buffer_getspecific() is MT safe 
*
*******************************************************************************/
static log_buffer_t* log_buffer_getspecific(pthread_key_t aKey)
{
   log_buffer_t *buf = NULL;
   int res = -1;

   if ((buf = pthread_getspecific(aKey)) != NULL) {
      return buf;
   }

   buf = (log_buffer_t*)sge_malloc(sizeof(log_buffer_t));
   memset((void*)(buf), 0, sizeof(log_buffer_t));

   res = pthread_setspecific(log_buffer_key, (const void*)buf);

   if (0 != res) {
      fprintf(stderr, "pthread_set_specific(%s) failed: %s\n", "log_buffer_getspecific", strerror(res));
      abort();
   }
   
   return buf;
} /* log_buffer_getspecific() */

