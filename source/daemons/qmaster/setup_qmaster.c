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

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>  
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/resource.h>

#include "sge_bootstrap.h"
#include "sge.h"
#include "sge_conf.h"
#include "commlib.h"
#include "sge_subordinate_qmaster.h"
#include "sge_calendar_qmaster.h"
#include "sge_sched.h"
#include "sge_all_listsL.h"
#include "sge_host.h"
#include "sge_host_qmaster.h"
#include "sge_pe_qmaster.h"
#include "sge_cqueue_qmaster.h"
#include "sge_manop_qmaster.h"
#include "sge_job_qmaster.h"
#include "configuration_qmaster.h"
#include "qmaster_heartbeat.h"
#include "qm_name.h"
#include "sched_conf_qmaster.h"
#include "sge_sharetree.h"
#include "sge_sharetree_qmaster.h"
#include "sge_userset.h"
#include "sge_feature.h"
#include "sge_userset_qmaster.h"
#include "sge_ckpt_qmaster.h"
#include "sge_utility.h"
#include "setup_qmaster.h"
#include "sge_prog.h"
#include "sgermon.h"
#include "sge_log.h"
#include "config_file.h"
#include "sge_qmod_qmaster.h"
#include "sge_give_jobs.h"
#include "setup_path.h"
#include "msg_daemons_common.h"
#include "msg_qmaster.h"
#include "reschedule.h"
#include "sge_job.h"
#include "sge_unistd.h"
#include "sge_uidgid.h"
#include "sge_io.h"
#include "sge_answer.h"
#include "sge_pe.h"
#include "sge_qinstance.h"
#include "sge_qinstance_state.h"
#include "sge_cqueue.h"
#include "sge_ckpt.h"
#include "sge_userprj.h"
#include "sge_manop.h"
#include "sge_calendar.h"
#include "sge_hgroup.h"
#include "sge_cuser.h"
#include "sge_centry.h"
#include "sge_reporting_qmaster.h"
#include "parse.h"
#include "usage.h"
#include "qmaster_to_execd.h"
#include "shutdown.h"
#include "sge_hostname.h"
#include "sge_any_request.h"
#include "sge_os.h"
#include "lock.h"
#include "sge_persistence_qmaster.h"
#include "sge_spool.h"
#include "setup.h"
#include "sge_event_master.h"
#include "msg_common.h"
#include "spool/sge_spooling.h"


static void   process_cmdline(char**);
static lList* parse_cmdline_qmaster(char**, lList**);
static lList* parse_qmaster(lList**, u_long32*);
static void   qmaster_init(char**);
static void   communication_setup(void);
static bool   is_qmaster_already_running(void);
static void   qmaster_lock_and_shutdown(int);
static int    setup_qmaster(void);
static int    remove_invalid_job_references(int user);
static int    debit_all_jobs_from_qs(void);


/****** qmaster/setup_qmaster/sge_setup_qmaster() ******************************
*  NAME
*     sge_setup_qmaster() -- setup qmaster 
*
*  SYNOPSIS
*     int sge_setup_qmaster(char* anArgv[]) 
*
*  FUNCTION
*     Process commandline arguments. Remove qmaster lock file. Write qmaster
*     host to the 'act_qmaster' file. Initialize qmaster and reporting. Write
*     qmaster PID file.  
*
*     NOTE: Before this function is invoked, qmaster must become admin user.
*
*  INPUTS
*     char* anArgv[] - commandline argument vector 
*
*  RESULT
*     0 - success 
*
*  NOTES
*     MT-NOTE: sge_setup_qmaster() is NOT MT safe! 
*     MT-NOTE:
*     MT-NOTE: This function must be called exclusively, with the qmaster main
*     MT-NOTE: thread being the *only* active thread. In other words, do not
*     MT-NOTE: invoke this function after any additional thread (directly or
*     MT-NOTE: indirectly) has been created.
*
*     Do *not* write the qmaster pid file, before 'qmaster_init()' did return
*     successfully. Otherwise, if we do have a running qmaster and a second
*     qmaster is started (illegally) on the same host, the second qmaster will
*     overwrite the pid of the qmaster started first. The second qmaster will
*     detect it's insubordinate doing and terminate itself, thus leaving behind
*     a useless pid.
*
*******************************************************************************/
int sge_setup_qmaster(char* anArgv[])
{
   char err_str[1024];

   DENTER(TOP_LAYER, "sge_setup_qmaster");

   umask(022); /* this needs a better solution */

   process_cmdline(anArgv);

   INFO((SGE_EVENT, MSG_STARTUP_BEGINWITHSTARTUP));

   qmaster_unlock(QMASTER_LOCK_FILE);

   if (write_qm_name(uti_state_get_qualified_hostname(), path_state_get_act_qmaster_file(), err_str)) {
      ERROR((SGE_EVENT, "%s\n", err_str));
      SGE_EXIT(1);
   }

   qmaster_init(anArgv);

   sge_write_pid(QMASTER_PID_FILE);

   reporting_initialize(NULL);

   DEXIT;
   return 0;
} /* sge_setup_qmaster() */

/****** qmaster/setup_qmaster/sge_qmaster_thread_init() ************************
*  NAME
*     sge_qmaster_thread_init() -- Initialize a qmaster thread.
*
*  SYNOPSIS
*     int sge_qmaster_thread_init(bool switch_to_admin_user) 
*
*  FUNCTION
*     Subsume functions which need to be called immediately after thread
*     startup. This function does make sure that the thread local data
*     structures do contain reasonable values.
*
*  INPUTS
*     bool switch_to_admin_user - become admin user if set to true
*
*  RESULT
*     0 - success 
*
*  NOTES
*     MT-NOTE: sge_qmaster_thread_init() is MT safe 
*     MT-NOTE:
*     MT-NOTE: sge_qmaster_thread_init() should be invoked at the beginning
*     MT-NOTE: of a thread function.
*
*******************************************************************************/
int sge_qmaster_thread_init(bool switch_to_admin_user)
{
   DENTER(TOP_LAYER, "sge_qmaster_thread_init");

   lInit(nmv);

   sge_setup(QMASTER, NULL);

   reresolve_me_qualified_hostname();

   DEBUG((SGE_EVENT,"%s: qualified hostname \"%s\"\n", SGE_FUNC, uti_state_get_qualified_hostname()));
  
#if defined(LINUX86) || defined(LINUXAMD64) || defined(LINUXIA64)
   uidgid_mt_init();
#endif

   if ( switch_to_admin_user == true ) {   
      char str[1024];
      if (sge_set_admin_username(bootstrap_get_admin_user(), str) == -1) {
         CRITICAL((SGE_EVENT, str));
         SGE_EXIT(1);
      }

      if (sge_switch2admin_user()) {
         CRITICAL((SGE_EVENT, MSG_ERROR_CANTSWITCHTOADMINUSER));
         SGE_EXIT(1);
      }
   }

   DEXIT;
   return 0;
} /* sge_qmaster_thread_init() */

/****** qmaster/setup_qmaster/sge_setup_job_resend() ***************************
*  NAME
*     sge_setup_job_resend() -- Setup job resend events.
*
*  SYNOPSIS
*     void sge_setup_job_resend(void) 
*
*  FUNCTION
*     Register a job resend event for each job or array task which does have a
*     'JTRANSFERING' status.
*
*  INPUTS
*     void - none 
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: sge_setup_job_resend() is not MT safe 
*
*******************************************************************************/
void sge_setup_job_resend(void)
{
   lListElem *job = NULL;

   DENTER(TOP_LAYER, "sge_setup_job_resend");

   job = lFirst(*(object_type_get_master_list(SGE_TYPE_JOB)));

   while (NULL != job)
   {
      lListElem *task;
      u_long32 job_num;

      job_num = lGetUlong(job, JB_job_number);

      task = lFirst(lGetList(job, JB_ja_tasks));
      
      while (NULL != task)
      {
         if (lGetUlong(task, JAT_status) == JTRANSFERING)
         {
            lListElem *granted_queue, *qinstance, *host;
            const char *qname;
            u_long32 task_num, when;
            te_event_t ev;

            task_num = lGetUlong(task, JAT_task_number);

            granted_queue = lFirst(lGetList(task, JAT_granted_destin_identifier_list));

            qname = lGetString(granted_queue, JG_qname);

            qinstance = cqueue_list_locate_qinstance(*(object_type_get_master_list(SGE_TYPE_CQUEUE)), qname);

            host = host_list_locate(Master_Exechost_List, lGetHost(qinstance, QU_qhostname)); 

            when = lGetUlong(task, JAT_start_time);

            when += MAX(load_report_interval(host), MAX_JOB_DELIVER_TIME);

            ev = te_new_event((time_t)when, TYPE_JOB_RESEND_EVENT, ONE_TIME_EVENT, job_num, task_num, "job-resend_event");           
            te_add_event(ev);
            te_free_event(&ev);

            DPRINTF(("Did add job resend for "sge_u32"/"sge_u32" at %d\n", job_num, task_num, when)); 
         }

         task = lNext(task);
      }

      job = lNext(job);
   }

   DEXIT;
   return;
} /* sge_setup_job_resend() */

/****** setup_qmaster/sge_process_qmaster_cmdline() ****************************
*  NAME
*     sge_process_qmaster_cmdline() -- global available function for qmaster
*
*  SYNOPSIS
*     void sge_process_qmaster_cmdline(char**anArgv) 
*
*  FUNCTION
*     This function simply calls the static function process_cmdline()
*
*  INPUTS
*     char**anArgv - command line arguments from main()
*
*  NOTES
*     MT-NOTE: sge_process_qmaster_cmdline() is NOT MT safe 
*
*  SEE ALSO
*     qmaster/setup_qmaster/process_cmdline()
*******************************************************************************/
void sge_process_qmaster_cmdline(char**anArgv) {
   process_cmdline(anArgv);
}
/****** qmaster/setup_qmaster/process_cmdline() ********************************
*  NAME
*     process_cmdline() -- Handle command line arguments 
*
*  SYNOPSIS
*     static void process_cmdline(char **anArgv) 
*
*  FUNCTION
*     Handle command line arguments. Parse argument vector and handle options.
*
*  INPUTS
*     char **anArgv - pointer to agrument vector 
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: process_cmdline() is NOT MT safe. 
*
*******************************************************************************/
static void process_cmdline(char **anArgv)
{
   lList *alp, *pcmdline;
   lListElem *aep;
   u_long32 help = 0;

   DENTER(TOP_LAYER, "process_cmdline");

   alp = pcmdline = NULL;

   alp = parse_cmdline_qmaster(&anArgv[1], &pcmdline);
   if(alp) {
      /*
      ** high level parsing error! show answer list
      */
      for_each(aep, alp) {
         fprintf(stderr, "%s", lGetString(aep, AN_text));
      }
      lFreeList(&alp);
      lFreeList(&pcmdline);
      SGE_EXIT(1);
   }

   alp = parse_qmaster(&pcmdline, &help);
   if(alp) {
      /*
      ** low level parsing error! show answer list
      */
      for_each(aep, alp) {
         fprintf(stderr, "%s", lGetString(aep, AN_text));
      }
      lFreeList(&alp);
      lFreeList(&pcmdline);
      SGE_EXIT(1);
   }

   if(help) {
      /* user wanted to see help. we can exit */
      lFreeList(&pcmdline);
      SGE_EXIT(0);
   }

   DEXIT;
   return;
} /* process_cmdline */

/****** qmaster/setup_qmaster/parse_cmdline_qmaster() **************************
*  NAME
*     parse_cmdline_qmaster() -- Parse command line arguments
*
*  SYNOPSIS
*     static lList* parse_cmdline_qmaster(char **argv, lList **ppcmdline) 
*
*  FUNCTION
*     Decompose argument vector. Handle options and option arguments. 
*
*  INPUTS
*     char **argv       - pointer to argument vector 
*     lList **ppcmdline - pointer to lList pointer which does contain the 
*                         command line arguments upon return. 
*
*  RESULT
*     lList* - pointer to answer list 
*
*  NOTES
*     MT-NOTE: parse_cmdline_qmaster() is MT safe. 
*
*******************************************************************************/
static lList *parse_cmdline_qmaster(char **argv, lList **ppcmdline )
{
   char **sp;
   char **rp;
   stringT str;
   lList *alp = NULL;

   DENTER(TOP_LAYER, "parse_cmdline_qmaster");

   rp = argv;
   while(*(sp=rp))
   {
      /* -help */
      if ((rp = parse_noopt(sp, "-help", NULL, ppcmdline, &alp)) != sp)
         continue;

      /* oops */
      sprintf(str, MSG_PARSE_INVALIDOPTIONARGUMENTX_S, *sp);
      printf("%s\n", *sp);
      sge_usage(stderr);
      answer_list_add(&alp, str, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DEXIT;
      return alp;
   }

   DEXIT;
   return alp;
} /* parse_cmdline_qmaster() */

/****** qmaster/setup_qmaster/parse_qmaster() **********************************
*  NAME
*     parse_qmaster() -- Process options. 
*
*  SYNOPSIS
*     static lList* parse_qmaster(lList **ppcmdline, u_long32 *help) 
*
*  FUNCTION
*     Process options 
*
*  INPUTS
*     lList **ppcmdline - list of options
*     u_long32 *help    - flag is set upon return if help has been requested
*
*  RESULT
*     lList* - answer list 
*
*  NOTES
*     MT-NOTE: parse_qmaster() is not MT safe. 
*
*******************************************************************************/
static lList *parse_qmaster(lList **ppcmdline, u_long32 *help )
{
   stringT str;
   lList *alp = NULL;
   int usageshowed = 0;

   DENTER(TOP_LAYER, "parse_qmaster");

   /* Loop over all options. Only valid options can be in the 
      ppcmdline list.
   */
   while(lGetNumberOfElem(*ppcmdline))
   {
      /* -help */
      if(parse_flag(ppcmdline, "-help", &alp, help)) {
         usageshowed = 1;
         sge_usage(stdout);
         break;
      }
   }

   if(lGetNumberOfElem(*ppcmdline)) {
      sprintf(str, MSG_PARSE_TOOMANYOPTIONS);
      if(!usageshowed)
         sge_usage(stderr);
      answer_list_add(&alp, str, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DEXIT;
      return alp;
   }
   DEXIT;
   return alp;
} /* parse_qmaster() */

/****** qmaster/setup_qmaster/qmaster_init() ***********************************
*  NAME
*     qmaster_init() -- Initialize qmaster 
*
*  SYNOPSIS
*     static void qmaster_init(char **anArgv) 
*
*  FUNCTION
*     Initialize qmaster. Do general setup and communication setup. 
*
*  INPUTS
*     char **anArgv - process argument vector 
*
*  RESULT
*     void - none 
*
*  NOTES
*     MT-NOTE: qmaster_init() is NOT MT safe. 
*
*******************************************************************************/
static void qmaster_init(char **anArgv)
{
   DENTER(TOP_LAYER, "qmaster_init");

   if (setup_qmaster()) {
      CRITICAL((SGE_EVENT, MSG_STARTUP_SETUPFAILED));
      SGE_EXIT(1);
   }

   uti_state_set_exit_func(qmaster_lock_and_shutdown); /* CWD is spool directory */
  
   communication_setup();

   starting_up(); /* write startup info message to message file */

   DEXIT;
   return;
} /* qmaster_init() */

/****** qmaster/setup_qmaster/communication_setup() ****************************
*  NAME
*     communication_setup() -- set up communication
*
*  SYNOPSIS
*     static void communication_setup(void) 
*
*  FUNCTION
*     Initialize qmaster communication. 
*
*     This function will fail, if the configured qmaster port is already in
*     use.
*
*     This could happen if either qmaster has been terminated shortly before
*     and the operating system did not get around to free the port or there
*     is a qmaster already running.
*
*  INPUTS
*     void - none 
*
*  RESULT
*     void - none 
*
*  NOTES
*     MT-NOTE: communication_setup() is NOT MT safe 
*
*******************************************************************************/
static void communication_setup(void)
{
   cl_com_handle_t* com_handle = NULL;
#if defined(IRIX) || (defined(LINUX) && defined(TARGET32_BIT))
   struct rlimit64 qmaster_rlimits;
#else
   struct rlimit qmaster_rlimits;
#endif

   DENTER(TOP_LAYER, "communication_setup");

   DEBUG((SGE_EVENT,"my resolved hostname name is: \"%s\"\n", uti_state_get_qualified_hostname()));

   com_handle = cl_com_get_handle((char*)prognames[QMASTER], 1);

   if (com_handle == NULL)
   {
      ERROR((SGE_EVENT, "port %d already bound\n", sge_get_qmaster_port()));

      if (is_qmaster_already_running() == true)
      {
         char *host = NULL;
         int res = -1; 

         res = cl_com_gethostname(&host, NULL, NULL,NULL);

         CRITICAL((SGE_EVENT, MSG_QMASTER_FOUNDRUNNINGQMASTERONHOSTXNOTSTARTING_S, ((CL_RETVAL_OK == res ) ? host : "unknown")));

         if (CL_RETVAL_OK == res) { free(host); }
      }

      SGE_EXIT(1);
   }

   if (com_handle) {
      unsigned long max_connections = 0;
      u_long32 old_ll = 0;

      /* 
       * re-check file descriptor limits for qmaster 
       */
#if defined(IRIX) || (defined(LINUX) && defined(TARGET32_BIT))
      getrlimit64(RLIMIT_NOFILE, &qmaster_rlimits);
#else
      getrlimit(RLIMIT_NOFILE, &qmaster_rlimits);
#endif

      /* save old debug log level and set log level to INFO */
      old_ll = log_state_get_log_level();

      /* enable max connection close mode */
      cl_com_set_max_connection_close_mode(com_handle, CL_ON_MAX_COUNT_CLOSE_AUTOCLOSE_CLIENTS);

      cl_com_get_max_connections(com_handle, &max_connections);

      /* add local host to allowed host list */
      cl_com_add_allowed_host(com_handle,com_handle->local->comp_host);

      /* check dynamic event client count */
      mconf_set_max_dynamic_event_clients(sge_set_max_dynamic_event_clients(mconf_get_max_dynamic_event_clients())); 

      /* log startup info into qmaster messages file */
      log_state_set_log_level(LOG_INFO);
      INFO((SGE_EVENT, MSG_QMASTER_FD_HARD_LIMIT_SETTINGS_U, sge_u32c(qmaster_rlimits.rlim_max)));
      INFO((SGE_EVENT, MSG_QMASTER_FD_SOFT_LIMIT_SETTINGS_U, sge_u32c(qmaster_rlimits.rlim_cur)));
      INFO((SGE_EVENT, MSG_QMASTER_MAX_FILE_DESCRIPTORS_LIMIT_U, sge_u32c(max_connections)));
      INFO((SGE_EVENT, MSG_QMASTER_MAX_EVC_LIMIT_U, sge_u32c(mconf_get_max_dynamic_event_clients())));
      log_state_set_log_level(old_ll);
   }

   cl_commlib_set_connection_param(cl_com_get_handle("qmaster",1), HEARD_FROM_TIMEOUT, mconf_get_max_unheard());

   DEXIT;
   return;
} /* communication_setup() */

/****** qmaster/setup_qmaster/is_qmaster_already_running() *********************
*  NAME
*     is_qmaster_already_running() -- is qmaster already running 
*
*  SYNOPSIS
*     static bool is_qmaster_already_running(void) 
*
*  FUNCTION
*     Check, whether there is running qmaster already.
*
*  INPUTS
*     void - none 
*
*  RESULT
*     true  - running qmaster detected. 
*     false - otherwise
*
*  NOTES
*     MT-NOTE: is_qmaster_already_running() is not MT safe 
*
*  BUGS
*     This function will only work, if the PID found in the qmaster PID file
*     either does belong to a running qmaster or no process at all.
*
*     Of course PID's will be reused. This is, however, not a problem because
*     of the very specifc situation in which this function is called.
*
*******************************************************************************/
static bool is_qmaster_already_running(void)
{
   enum { NULL_SIGNAL = 0 };

   bool res = true;
   char pidfile[SGE_PATH_MAX] = { '\0' };
   pid_t pid = 0;

   DENTER(TOP_LAYER, "is_qmaster_already_running");

   sprintf(pidfile, "%s/%s", bootstrap_get_qmaster_spool_dir(), QMASTER_PID_FILE);

   if ((pid = sge_readpid(pidfile)) == 0)
   {
      DEXIT;
      return false;
   }

   res = (kill(pid, NULL_SIGNAL) == 0) ? true: false;

   DEXIT;
   return res;
} /* is_qmaster_already_running() */

/****** qmaster/setup_qmaster/qmaster_lock_and_shutdown() ***************************
*  NAME
*     qmaster_lock_and_shutdown() -- Acquire qmaster lock file and shutdown 
*
*  SYNOPSIS
*     static void qmaster_lock_and_shutdown(int anExitValue) 
*
*  FUNCTION
*     qmaster exit function. This version MUST NOT be used, if the current
*     working   directory is NOT the spool directory. Other components do rely
*     on finding the lock file in the spool directory.
*
*  INPUTS
*     int anExitValue - exit value 
*
*  RESULT
*     void - none
*
*  EXAMPLE
*     ??? 
*
*  NOTES
*     MT-NOTE: qmaster_lock_and_shutdown() is MT safe 
*
*******************************************************************************/
static void qmaster_lock_and_shutdown(int anExitValue)
{
   DENTER(TOP_LAYER, "qmaster_lock_and_shutdown");
   
   if (anExitValue == 0) {
      if (qmaster_lock(QMASTER_LOCK_FILE) == -1) {
         CRITICAL((SGE_EVENT, MSG_QMASTER_LOCKFILE_ALREADY_EXISTS));
      }
   }
   sge_gdi_shutdown();

   DEXIT;
   return;
} /* qmaster_lock_and_shutdown() */


static int setup_qmaster(void)
{
   lListElem *jep, *ep, *tmpqep;
   static bool first = true;
   lListElem *spooling_context = NULL;
   lList *answer_list = NULL;
   time_t time_start, time_end;
   monitoring_t monitor;

   DENTER(TOP_LAYER, "sge_setup_qmaster");

   if (first)
      first = false;
   else {
      CRITICAL((SGE_EVENT, MSG_SETUP_SETUPMAYBECALLEDONLYATSTARTUP));
      DEXIT;
      return -1;
   }   

   /* register our error function for use in replace_params() */
   config_errfunc = set_error;

   /*
    * Initialize Master lists and hash tables, if necessary 
    */

/** This is part is making the scheduler a
  * lot slower that it was before. This was an enhancement introduced
  * in cvs revision 1.35
  * It might be added again, when hte hashing problem on large job lists
  * with only a view owners is solved.
  */
#if 0
   if (*(object_type_get_master_list(SGE_TYPE_JOB)) == NULL) {
      *(object_type_get_master_list(SGE_TYPE_JOB)) = lCreateList("Master_Job_List", JB_Type);
   }
   cull_hash_new(*(object_type_get_master_list(SGE_TYPE_JOB)), JB_owner, 0);
#endif

   if (!sge_initialize_persistence(&answer_list)) {
      answer_list_output(&answer_list);
      DEXIT;
      return -1;
   } else {
      answer_list_output(&answer_list);
      spooling_context = spool_get_default_context();
   }
   
   if (sge_read_configuration(spooling_context, answer_list) != 0)
   {
      DEXIT;
      return -1;
   }
   
   mconf_set_new_config(true);

   /* get aliased hostname from commd */
   reresolve_me_qualified_hostname();
   DEBUG((SGE_EVENT,"uti_state_get_qualified_hostname() returned \"%s\"\n",uti_state_get_qualified_hostname() ));
   /*
   ** read in all objects and check for correctness
   */
   DPRINTF(("Complex Attributes----------------------\n"));
   spool_read_list(&answer_list, spooling_context, &Master_CEntry_List, SGE_TYPE_CENTRY);
   answer_list_output(&answer_list);

   DPRINTF(("host_list----------------------------\n"));
   spool_read_list(&answer_list, spooling_context, &Master_Exechost_List, SGE_TYPE_EXECHOST);
   spool_read_list(&answer_list, spooling_context, &Master_Adminhost_List, SGE_TYPE_ADMINHOST);
   spool_read_list(&answer_list, spooling_context, &Master_Submithost_List, SGE_TYPE_SUBMITHOST);
   answer_list_output(&answer_list);

   if (!host_list_locate(Master_Exechost_List, SGE_TEMPLATE_NAME)) {
      /* add an exec host "template" */
      if (sge_add_host_of_type(SGE_TEMPLATE_NAME, SGE_EXECHOST_LIST, &monitor))
         ERROR((SGE_EVENT, MSG_CONFIG_ADDINGHOSTTEMPLATETOEXECHOSTLIST));
   }

   /* add host "global" to Master_Exechost_List as an exec host */
   if (!host_list_locate(Master_Exechost_List, SGE_GLOBAL_NAME)) {
      /* add an exec host "global" */
      if (sge_add_host_of_type(SGE_GLOBAL_NAME, SGE_EXECHOST_LIST, &monitor))
         ERROR((SGE_EVENT, MSG_CONFIG_ADDINGHOSTGLOBALTOEXECHOSTLIST));
   }

   /* add qmaster host to Master_Adminhost_List as an administrativ host */
   if (!host_list_locate(Master_Adminhost_List, uti_state_get_qualified_hostname())) {
      if (sge_add_host_of_type(uti_state_get_qualified_hostname(), SGE_ADMINHOST_LIST, &monitor)) {
         DEXIT;
         return -1;
      }
   }

   DPRINTF(("manager_list----------------------------\n"));
   spool_read_list(&answer_list, spooling_context, &Master_Manager_List, SGE_TYPE_MANAGER);
   answer_list_output(&answer_list);
   if (!manop_is_manager("root")) {
      ep = lAddElemStr(&Master_Manager_List, MO_name, "root", MO_Type);

      if (!spool_write_object(&answer_list, spooling_context, ep, "root", SGE_TYPE_MANAGER)) {
         answer_list_output(&answer_list);
         CRITICAL((SGE_EVENT, MSG_CONFIG_CANTWRITEMANAGERLIST)); 
         DEXIT;
         return -1;
      }
   }
   for_each(ep, Master_Manager_List) 
      DPRINTF(("%s\n", lGetString(ep, MO_name)));

   DPRINTF(("host group definitions-----------\n"));
   spool_read_list(&answer_list, spooling_context, hgroup_list_get_master_list(), 
                   SGE_TYPE_HGROUP);
   answer_list_output(&answer_list);

   DPRINTF(("operator_list----------------------------\n"));
   spool_read_list(&answer_list, spooling_context, &Master_Operator_List, SGE_TYPE_OPERATOR);
   answer_list_output(&answer_list);
   if (!manop_is_operator("root")) {
      ep = lAddElemStr(&Master_Operator_List, MO_name, "root", MO_Type);

      if (!spool_write_object(&answer_list, spooling_context, ep, "root", SGE_TYPE_OPERATOR)) {
         answer_list_output(&answer_list);
         CRITICAL((SGE_EVENT, MSG_CONFIG_CANTWRITEOPERATORLIST)); 
         DEXIT;
         return -1;
      }
   }
   for_each(ep, Master_Operator_List) 
      DPRINTF(("%s\n", lGetString(ep, MO_name)));


   DPRINTF(("userset_list------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, &Master_Userset_List, SGE_TYPE_USERSET);
   answer_list_output(&answer_list);

   DPRINTF(("calendar list ------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, &Master_Calendar_List, SGE_TYPE_CALENDAR);
   answer_list_output(&answer_list);

#ifndef __SGE_NO_USERMAPPING__
   DPRINTF(("administrator user mapping-----------\n"));
   spool_read_list(&answer_list, spooling_context, cuser_list_get_master_list(), SGE_TYPE_CUSER);
   answer_list_output(&answer_list);
#endif

   DPRINTF(("cluster_queue_list---------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list(SGE_TYPE_CQUEUE), SGE_TYPE_CQUEUE);
   answer_list_output(&answer_list);
   cqueue_list_set_unknown_state(
            *(object_type_get_master_list(SGE_TYPE_CQUEUE)),
            NULL, false, true);
   
   DPRINTF(("pe_list---------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, &Master_Pe_List, SGE_TYPE_PE);
   answer_list_output(&answer_list);

   DPRINTF(("ckpt_list---------------------------------\n"));
   spool_read_list(&answer_list, spooling_context, &Master_Ckpt_List, SGE_TYPE_CKPT);
   answer_list_output(&answer_list);

   DPRINTF(("job_list-----------------------------------\n"));
   /* measure time needed to read job database */
   time_start = time(0);
   spool_read_list(&answer_list, spooling_context, object_type_get_master_list(SGE_TYPE_JOB), SGE_TYPE_JOB);
   time_end = time(0);
   answer_list_output(&answer_list);

   {
      u_long32 saved_logginglevel = log_state_get_log_level();
      log_state_set_log_level(LOG_INFO);
      INFO((SGE_EVENT, MSG_QMASTER_READ_JDB_WITH_X_ENTR_IN_Y_SECS_UU,
            sge_u32c(lGetNumberOfElem(*(object_type_get_master_list(SGE_TYPE_JOB)))), 
            sge_u32c(time_end - time_start)));
      log_state_set_log_level(saved_logginglevel);
   }

   for_each(jep, *(object_type_get_master_list(SGE_TYPE_JOB))) {
      DPRINTF(("JOB "sge_u32" PRIORITY %d\n", lGetUlong(jep, JB_job_number), 
            (int)lGetUlong(jep, JB_priority) - BASE_PRIORITY));

      /* doing this operation we need the complete job list read in */
      job_suc_pre(jep);

      centry_list_fill_request(lGetList(jep, JB_hard_resource_list), 
                  NULL, Master_CEntry_List, false, true, false);
   }

   if (!bootstrap_get_job_spooling()) {
      lList *answer_list = NULL;
      dstring buffer = DSTRING_INIT;
      char *str = NULL;
      int len = 0;

      INFO((SGE_EVENT, "job spooling is disabled - removing spooled jobs"));
      
      bootstrap_set_job_spooling("true");
      
      for_each(jep, *(object_type_get_master_list(SGE_TYPE_JOB))) {
         u_long32 job_id = lGetUlong(jep, JB_job_number);
         sge_dstring_clear(&buffer);

         if (lGetString(jep, JB_exec_file) != NULL) {
            if ((str = sge_file2string(lGetString(jep, JB_exec_file), &len))) {
               lXchgString(jep, JB_script_ptr, &str);
               FREE(str);
               lSetUlong(jep, JB_script_size, len);

               unlink(lGetString(jep, JB_exec_file));
            }
            else {
               printf("could not read in script file\n");
            }
         }
         spool_delete_object(&answer_list, spool_get_default_context(), 
                             SGE_TYPE_JOB, 
                             job_get_key(job_id, 0, NULL, &buffer));                     
      }
      answer_list_output(&answer_list);
      sge_dstring_free(&buffer);
      bootstrap_set_job_spooling("false");
   }

   /* 
      if the job is in state running 
      we have to register each slot 
      in a queue and in the parallel 
      environment if the job is a 
      parallel one
   */
   debit_all_jobs_from_qs(); 
   debit_all_jobs_from_pes(Master_Pe_List); 

   /* clear suspend on subordinate flag in QU_state */ 
   for_each(tmpqep, *(object_type_get_master_list(SGE_TYPE_CQUEUE))) {
      lList *qinstance_list = lGetList(tmpqep, CQ_qinstances);
      lListElem *qinstance;

      for_each(qinstance, qinstance_list) {
         qinstance_state_set_susp_on_sub(qinstance, false);
      }
   }

   /* 
    * initialize QU_queue_number if the value is 0 
    *
    * Normally this attribute gets a value > 0 during instance creation
    * but due to CR 6286510 (IZ 1665) there might be instances which have
    * the value 0. We correct this here.
    */
   {
      lListElem *cqueue;

      for_each(cqueue, *(object_type_get_master_list(SGE_TYPE_CQUEUE))) {
         lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
         lListElem *qinstance;
   
         for_each(qinstance, qinstance_list) {
            u_long32 qinstance_number = lGetUlong(qinstance, QU_queue_number);

            if (qinstance_number == 0) {
               qinstance_number = sge_get_qinstance_number();
               lSetUlong(qinstance, QU_queue_number, qinstance_number);
            }
         }
      }
   }

   /* 
    * Initialize
    *    - suspend on subordinate state 
    *    - cached QI values.
    */
   for_each(tmpqep, *(object_type_get_master_list(SGE_TYPE_CQUEUE))) {
      cqueue_mod_qinstances(tmpqep, NULL, tmpqep, true, &monitor);
   }
        
   /* calendar */
   {
      lListElem *cep;
      lList *ppList = NULL;

      for_each (cep, Master_Calendar_List) 
      {
         calendar_parse_year(cep, &answer_list);
         calendar_parse_week(cep, &answer_list);
         answer_list_output(&answer_list);

         calendar_update_queue_states(cep, NULL, NULL, &ppList, &monitor);
      }

      lFreeList(&ppList);
   }

   /* rebuild signal resend events */
   rebuild_signal_events();

   DPRINTF(("scheduler config -----------------------------------\n"));
   
   sge_read_sched_configuration(spooling_context, &answer_list);
   answer_list_output(&answer_list);

   /* SGEEE: read user list */
   spool_read_list(&answer_list, spooling_context, &Master_User_List, SGE_TYPE_USER);
   answer_list_output(&answer_list);

   remove_invalid_job_references(1);

   /* SGE: read project list */
   spool_read_list(&answer_list, spooling_context, &Master_Project_List, SGE_TYPE_PROJECT);
   answer_list_output(&answer_list);

   remove_invalid_job_references(0);
   
   /* SGEEE: read share tree */
   spool_read_list(&answer_list, spooling_context, &Master_Sharetree_List, SGE_TYPE_SHARETREE);
   answer_list_output(&answer_list);
   ep = lFirst(Master_Sharetree_List);
   if (ep) {
      lList *alp = NULL;
      lList *found = NULL;
      check_sharetree(&alp, ep, Master_User_List, Master_Project_List, 
            NULL, &found);
      lFreeList(&found);
      lFreeList(&alp); 
   }
   
   /* RU: */
   /* initiate timer for all hosts because they start in 'unknown' state */ 
   if (Master_Exechost_List) {
      lListElem *host               = NULL;
      lListElem *global_host_elem   = NULL;
      lListElem *template_host_elem = NULL;

      /* get "global" element pointer */
      global_host_elem   = host_list_locate(Master_Exechost_List, SGE_GLOBAL_NAME);   

      /* get "template" element pointer */
      template_host_elem = host_list_locate(Master_Exechost_List, SGE_TEMPLATE_NAME);
  
      for_each(host, Master_Exechost_List) {
         if ( (host != global_host_elem ) && (host != template_host_elem ) ) {
            reschedule_add_additional_time(load_report_interval(host));
            reschedule_unknown_trigger(host);
            reschedule_add_additional_time(0); 
         }
      }
   }

   DEXIT;
   return 0;
}

/* get rid of still debited per job usage contained 
   in user or project object if the job is no longer existing */
static int remove_invalid_job_references(
int user 
) {
   lListElem *up, *upu, *next;
   u_long32 jobid;

   DENTER(TOP_LAYER, "remove_invalid_job_references");

   for_each (up, user?Master_User_List:Master_Project_List) {

      int spool_me = 0;
      next = lFirst(lGetList(up, UP_debited_job_usage));
      while ((upu=next)) {
         next = lNext(upu);

         jobid = lGetUlong(upu, UPU_job_number);
         if (!job_list_locate(*(object_type_get_master_list(SGE_TYPE_JOB)), jobid)) {
            lRemoveElem(lGetList(up, UP_debited_job_usage), &upu);
            WARNING((SGE_EVENT, "removing reference to no longer existing job "sge_u32" of %s "SFQ"\n",
                           jobid, user?"user":"project", lGetString(up, UP_name)));
            spool_me = 1;
         }
      }

      if (spool_me) {
         lList *answer_list = NULL;
         spool_write_object(&answer_list, spool_get_default_context(), up, 
                            lGetString(up, UP_name), user ? SGE_TYPE_USER : 
                                                            SGE_TYPE_PROJECT);
         answer_list_output(&answer_list);
      }
   }

   DEXIT;
   return 0;
}

static int debit_all_jobs_from_qs()
{
   lListElem *gdi;
   u_long32 slots;
   const char *queue_name;
   lListElem *next_jep, *jep, *qep, *next_jatep, *jatep;
   int ret = 0;

   DENTER(TOP_LAYER, "debit_all_jobs_from_qs");

   next_jep = lFirst(*(object_type_get_master_list(SGE_TYPE_JOB)));
   while ((jep=next_jep)) {
   
      /* may be we have to delete this job */   
      next_jep = lNext(jep);
      
      next_jatep = lFirst(lGetList(jep, JB_ja_tasks));
      while ((jatep = next_jatep)) {
         next_jatep = lNext(jatep);

         /* don't look at states - we only trust in 
            "granted destin. ident. list" */

         for_each (gdi, lGetList(jatep, JAT_granted_destin_identifier_list)) {

            queue_name = lGetString(gdi, JG_qname);
            slots = lGetUlong(gdi, JG_slots);
            
            if (!(qep = cqueue_list_locate_qinstance(*(object_type_get_master_list(SGE_TYPE_CQUEUE)), queue_name))) {
               ERROR((SGE_EVENT, MSG_CONFIG_CANTFINDQUEUEXREFERENCEDINJOBY_SU,  
                      queue_name, sge_u32c(lGetUlong(jep, JB_job_number))));
               lRemoveElem(lGetList(jep, JB_ja_tasks), &jatep);
            } else {
               /* debit in all layers */
               debit_host_consumable(jep, host_list_locate(Master_Exechost_List,
                                     "global"), Master_CEntry_List, slots);
               debit_host_consumable(jep, host_list_locate(
                        Master_Exechost_List, lGetHost(qep, QU_qhostname)), 
                        Master_CEntry_List, slots);
               qinstance_debit_consumable(qep, jep, Master_CEntry_List, slots);
            }
         }
      }
   }

   DEXIT;
   return ret;
}
