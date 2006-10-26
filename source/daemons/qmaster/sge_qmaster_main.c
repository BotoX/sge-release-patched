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
 *   Copyright: 2003 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/resource.h>

#include "basis_types.h"
#include "sge_qmaster_main.h"
#include "sgermon.h"
#include "sge_mt_init.h"
#include "sge_prog.h"
#include "sge_log.h"
#include "sge_unistd.h"
#include "sge_answer.h"
#include "setup_qmaster.h"
#include "sge_security.h"
#include "sge_manop.h"
#include "sge_mtutil.h"
#include "sge_lock.h"
#include "sge_qmaster_process_message.h"
#include "sge_event_master.h"
#include "sge_persistence_qmaster.h"
#include "sge_reporting_qmaster.h"
#include "sge_qmaster_timed_event.h"
#include "sge_host_qmaster.h"
#include "sge_userprj_qmaster.h"
#include "sge_give_jobs.h"
#include "sge_all_listsL.h"
#include "sge_calendar_qmaster.h"
#include "lock.h"
#include "qmaster_heartbeat.h"
#include "shutdown.h"
#include "setup.h"
#include "sge_spool.h"
#include "cl_commlib.h"
#include "sge_uidgid.h"
#include "sge_bootstrap.h"
#include "msg_common.h"
#include "msg_qmaster.h"
#include "msg_daemons_common.h"
#include "msg_utilib.h"  /* remove once 'daemonize_qmaster' did become 'sge_daemonize' */
#include "sge_any_request.h"
#include "sge.h"
#include "sge_qmod_qmaster.h"
#include "reschedule.h"
#include "sge_qmaster_threads.h"
#include "sge_job_qmaster.h"
#include "sge_profiling.h"
#include "uti/sge_monitor.h"
#include "sge_conf.h"

#ifdef TEST_QMASTER_GDI2
#include "sge_gdi_ctx.h"
#endif

#if !defined(INTERIX)
static void init_sig_action_and_mask(void);
#endif
static int set_file_descriptor_limit(void);

/****** qmaster/sge_qmaster_main/sge_qmaster_application_status() ************
*  NAME
*     sge_qmaster_application_status() -- commlib status callback function  
*
*  SYNOPSIS
*      unsigned long sge_qmaster_application_status(char** info_message) 
*
*  FUNCTION
*      This is the implementation of the commlib application status callback
*      function. This function is called from the commlib when a connected
*      client wants to get a SIRM (Status Information Response Message).
*      The standard client for this action is the qping command.
*
*      The callback function is set with cl_com_set_status_func() after
*      commlib initalization.
*
*      The function is called by a commlib thread which is not in the
*      context of the qmaster application. This means no qmaster specific
*      functions should be called (locking of global variables).
*
*      status 0:  no errors
*      status 1:  one or more threads has reached warning timeout
*      status 2:  one or more threads has reached error timeout
*      status 3:  thread alive timeout struct not initalized
*
*  INPUTS
*     char** info_message - pointer to an char* inside commlib.
*                           info message must be malloced, commlib will
*                           free this memory. 
*  RESULT
*     unsigned long status - status of application
*
*  NOTES
*     This function is MT save
* 
*******************************************************************************/
unsigned long sge_qmaster_application_status(char** info_message) 
{

   return sge_monitor_status(info_message, mconf_get_monitor_time());
}


/****** qmaster/sge_qmaster_main/set_file_descriptor_limit() ********************
*  NAME
*     set_file_descriptor_limit() -- check and set file descriptor limit
*
*  SYNOPSIS
*     static int set_file_descriptor_limit(void) 
*
*  FUNCTION
*     This function will check the file descriptor limit for the qmaster. If
*     soft limit < hard limit the soft limit will set to the hard limit, but
*     max. to 8192 file descriptors, even when the hard limit is higher.
*
*  RESULT
*     0  - success 
*     1  - can't set limit because FD_SETSIZE is to small
*
*
*  NOTES
*     MT-NOTE: set_file_descriptor_limit() is not MT safe because the limit
*              is a process specific parameter. This function should only be
*              called before starting up the threads.
*
*******************************************************************************/
static int set_file_descriptor_limit(void) {

   /* define the max qmaster file descriptor limit */
#define SGE_MAX_QMASTER_SOFT_FD_LIMIT 8192
   int modified_hard_limit = 0;
   int return_value = 0;

#if defined(IRIX) || (defined(LINUX) && defined(TARGET32_BIT))
   struct rlimit64 qmaster_rlimits;
#else
   struct rlimit qmaster_rlimits;
#endif


   /* 
    * check file descriptor limits for qmaster 
    */
#if defined(IRIX) || (defined(LINUX) && defined(TARGET32_BIT))
   getrlimit64(RLIMIT_NOFILE, &qmaster_rlimits);
#else
   getrlimit(RLIMIT_NOFILE, &qmaster_rlimits);
#endif

   /* check hard limit and set it to SGE_MAX_QMASTER_SOFT_FD_LIMIT
      if hard limit is smaller AND
      smaller than FD_SETSIZE */

   if (qmaster_rlimits.rlim_max < SGE_MAX_QMASTER_SOFT_FD_LIMIT) {
      qmaster_rlimits.rlim_max = SGE_MAX_QMASTER_SOFT_FD_LIMIT;
      if (qmaster_rlimits.rlim_cur > SGE_MAX_QMASTER_SOFT_FD_LIMIT) {
         qmaster_rlimits.rlim_cur = SGE_MAX_QMASTER_SOFT_FD_LIMIT;
      }
      modified_hard_limit = 1;
   }

   if (qmaster_rlimits.rlim_max > FD_SETSIZE) {
      qmaster_rlimits.rlim_max = FD_SETSIZE;
      if (qmaster_rlimits.rlim_cur > FD_SETSIZE) {
         qmaster_rlimits.rlim_cur = FD_SETSIZE;
      }
      modified_hard_limit = 1;
      return_value = 1;
   }

   if (modified_hard_limit == 1) {
#if defined(IRIX) || (defined(LINUX) && defined(TARGET32_BIT))
      setrlimit64(RLIMIT_NOFILE, &qmaster_rlimits);
#else
      setrlimit(RLIMIT_NOFILE, &qmaster_rlimits);
#endif
   }

#if defined(IRIX) || (defined(LINUX) && defined(TARGET32_BIT))
   getrlimit64(RLIMIT_NOFILE, &qmaster_rlimits);
#else
   getrlimit(RLIMIT_NOFILE, &qmaster_rlimits);
#endif

   if (modified_hard_limit == 1) {
      /* if we have modified the hard limit by ourselfs we set 
         SGE_MAX_QMASTER_SOFT_FD_LIMIT as soft limit (if possible) */ 
      if ( qmaster_rlimits.rlim_cur < SGE_MAX_QMASTER_SOFT_FD_LIMIT &&
           qmaster_rlimits.rlim_max < SGE_MAX_QMASTER_SOFT_FD_LIMIT ) {
         qmaster_rlimits.rlim_cur = qmaster_rlimits.rlim_max;
      } else {
         qmaster_rlimits.rlim_cur = SGE_MAX_QMASTER_SOFT_FD_LIMIT;
      }
#if defined(IRIX) || (defined(LINUX) && defined(TARGET32_BIT))
      setrlimit64(RLIMIT_NOFILE, &qmaster_rlimits);
#else
      setrlimit(RLIMIT_NOFILE, &qmaster_rlimits);
#endif
   } else {
      /* if limits are set high enough through user we use the
         hard limit setting for the soft limit */
      qmaster_rlimits.rlim_cur = qmaster_rlimits.rlim_max;
#if defined(IRIX) || (defined(LINUX) && defined(TARGET32_BIT))
      setrlimit64(RLIMIT_NOFILE, &qmaster_rlimits);
#else
      setrlimit(RLIMIT_NOFILE, &qmaster_rlimits);
#endif
   }
   return return_value;
}


/****** qmaster/sge_qmaster_main/main() ****************************************
*  NAME
*     main() -- qmaster entry point 
*
*  SYNOPSIS
*     int main(int argc, char* argv[]) 
*
*  FUNCTION
*     Qmaster entry point.
*
*     NOTE: The main thread must block all signals before any additional thread
*     is created. Failure to do so will ruin signal handling!
*
*  INPUTS
*     int argc     - number of commandline arguments 
*     char* argv[] - commandline arguments 
*
*  RESULT
*     0 - success 
*
*  NOTES
*     We check whether 'SGE_ROOT' is set before we daemonize. Once qmaster is
*     a daemon, we are no longer connected to a terminal and hence can not
*     output an error message to stdout or stderr.
*
*     We need to inovke 'prepare_enroll()' *before* the user id is switched via
*     'become_admin_user()'. This is because qmaster must be able to bind a so
*     called reserved port (requires root privileges) if configured to do so.
*
*******************************************************************************/
int main(int argc, char* argv[])
{
   int max_enroll_tries;
   int ret_val;
   int file_descriptor_settings_result = 0;
   int qmaster_exit_state = 0;
   bool do_final_spool = true;
   bool has_daemonized = false;
#ifdef TEST_QMASTER_GDI2
   sge_gdi_ctx_class_t *ctx = NULL;
#else   
   int last_prepare_enroll_error = CL_RETVAL_OK;
#endif
   DENTER_MAIN(TOP_LAYER, "qmaster");

   sge_prof_setup();

   sge_get_root_dir(true, NULL, 0, true);
   
#ifdef __SGE_COMPILE_WITH_GETTEXT__  
   sge_init_language_func((gettext_func_type)gettext, (setlocale_func_type)setlocale, (bindtextdomain_func_type)bindtextdomain, (textdomain_func_type)textdomain);
   sge_init_language(NULL,NULL);   
#endif 

   /* qmaster doesn't support any commandline anymore,
      but we should show version string and -help option */
   if (argc != 1)
   {
      sigset_t sig_set;
      sge_mt_init();
      sigfillset(&sig_set);
      pthread_sigmask(SIG_SETMASK, &sig_set, NULL);
#ifdef TEST_QMASTER_GDI2
      sge_qmaster_thread_init((void**)&ctx, true);
#else      
      sge_qmaster_thread_init(NULL, true);
#endif      
      sge_process_qmaster_cmdline(argv);
#ifdef TEST_GDI2      
      SGE_EXIT((void**)&ctx, 1);
#else
      SGE_EXIT(NULL, 1);
#endif      
   }

   has_daemonized = sge_daemonize_qmaster();

   file_descriptor_settings_result = set_file_descriptor_limit();

   sge_mt_init();

/* EB: TODO: INTERIX: might be enabled later */
#if !defined(INTERIX)
   init_sig_action_and_mask();
#endif

#ifdef TEST_QMASTER_GDI2
   /* init qmaster threads without becomming admin user */
   sge_qmaster_thread_init((void**)&ctx, false);
#else
   sge_qmaster_thread_init(NULL, false);
#endif

#ifdef TEST_QMASTER_GDI2
   ctx->set_daemonized(ctx, has_daemonized);
#else
   uti_state_set_daemonized(has_daemonized);
#endif
   /* this must be done as root user to be able to bind ports < 1024 */
   max_enroll_tries = 30;
   while ( cl_com_get_handle(prognames[QMASTER],1) == NULL) {
#ifdef TEST_QMASTER_GDI2
      ctx->prepare_enroll(ctx);
#else
      prepare_enroll(prognames[QMASTER], &last_prepare_enroll_error); 
#endif      
      max_enroll_tries--;
      if ( max_enroll_tries <= 0 ) {
         /* exit after 30 seconds */
         CRITICAL((SGE_EVENT, MSG_QMASTER_COMMUNICATION_ERRORS ));
#ifdef TEST_GDI2
         SGE_EXIT((void**)&ctx, 1);
#else
         SGE_EXIT(NULL, 1);
#endif         
      }
      if (  cl_com_get_handle(prognames[QMASTER],1) == NULL) {
        /* sleep when prepare_enroll() failed */
        sleep(1);
      }
   }

   /*
    * now the commlib up and running. Set qmaster application status function 
    * ( commlib callback function for qping status information response 
    *   messages (SIRM) )
    */
   ret_val = cl_com_set_status_func(sge_qmaster_application_status);
   if (ret_val != CL_RETVAL_OK) {
      ERROR((SGE_EVENT, cl_get_error_text(ret_val)) );
   }

#ifdef TEST_QMASTER_GDI2
   /* now we become admin user */
   sge_become_admin_user(ctx->get_admin_user(ctx));
#else
   /* now we become admin user */
   sge_become_admin_user(bootstrap_get_admin_user());
#endif   

#ifdef TEST_QMASTER_GDI2
   sge_chdir_exit(ctx->get_qmaster_spool_dir(ctx), 1);
#else
   sge_chdir_exit(bootstrap_get_qmaster_spool_dir(), 1);
#endif   

   log_state_set_log_file(ERR_FILE);

#ifdef TEST_GDI2
   ctx->set_exit_func(ctx, sge_exit_func);
#else
   uti_state_set_exit_func(sge_exit_func);
#endif   

   sge_start_heartbeat();

   sge_register_event_handler(); 

#ifdef TEST_QMASTER_GDI2
   sge_setup_qmaster(ctx, argv);
#else
   sge_setup_qmaster(NULL, argv);
#endif   

   if (file_descriptor_settings_result == 1) {
      WARNING((SGE_EVENT, MSG_QMASTER_FD_SETSIZE_LARGER_THAN_LIMIT_U, sge_u32c(FD_SETSIZE)));
      WARNING((SGE_EVENT, MSG_QMASTER_FD_SETSIZE_COMPILE_MESSAGE1_U, sge_u32c(FD_SETSIZE - 20)));
      WARNING((SGE_EVENT, MSG_QMASTER_FD_SETSIZE_COMPILE_MESSAGE2));
      WARNING((SGE_EVENT, MSG_QMASTER_FD_SETSIZE_COMPILE_MESSAGE3));
   }

   sge_start_periodic_tasks();

   sge_init_job_number();

   sge_setup_job_resend();

#ifdef TEST_QMASTER_GDI2
   sge_create_and_join_threads(ctx);
#else
   sge_create_and_join_threads(NULL);
#endif   

   qmaster_exit_state = sge_get_qmaster_exit_state();
   if (qmaster_exit_state == 100) {
      /*
       * another qmaster has taken over !!!
       * sge_shutdown_qmaster_via_signal_thread()
       * was used to set qmaster exit state 
       */
      do_final_spool = false;
   }

   if (do_final_spool == true) {
      monitoring_t monitor;
#ifdef TEST_QMASTER_GDI2
      sge_store_job_number(ctx, NULL, &monitor);
#else
      sge_store_job_number(NULL, NULL, &monitor);
#endif      
   }

#ifdef TEST_QMASTER_GDI2
   sge_qmaster_shutdown((void*)ctx, do_final_spool);
#else
   sge_qmaster_shutdown(NULL, do_final_spool);
#endif      

   /* TODO CR: do we need this function? (its empty) */ 
   sge_teardown_lock_service();

#ifdef TEST_GDI2
   sge_shutdown((void**)&ctx, qmaster_exit_state);
#else
   sge_shutdown(NULL, qmaster_exit_state);
#endif   
   /* the code above will never be executed, sge_shutdown does an exit() */

   /* TODO CR: this function is not called, because sge_shutdown is doing an
               SGE_EXIT() */
   sge_prof_cleanup();
   DEXIT;
   return 0;
} /* main() */

#if !defined(INTERIX)

/****** qmaster/sge_qmaster_main/init_sig_action_and_mask() *******************
*  NAME
*     init_sig_action_and_mask() -- initialize signal action and mask 
*
*  SYNOPSIS
*     static void init_sig_action_and_mask(void)
*
*  FUNCTION
*     Initialize signal action and mask.
*
*     NOTE: We ignore SIGCHLD. This, together with the 'SA_NOCLDWAIT' flag,
*     does make sure, that an unwaited for child process will not become
*     a zombie process.
*
*  INPUTS
*     none 
*
*  RESULT
*     none
*
*******************************************************************************/
static void init_sig_action_and_mask(void)
{
   struct sigaction sa;
   sigset_t sig_set;
   
   sa.sa_handler = SIG_IGN;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = SA_NOCLDWAIT;
   sigaction(SIGCHLD, &sa, NULL);
   
   sigfillset(&sig_set);
   pthread_sigmask(SIG_SETMASK, &sig_set, NULL);
   
   return;
}

#endif

