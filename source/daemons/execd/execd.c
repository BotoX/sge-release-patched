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
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>

#include "sge_unistd.h"
#include "sge.h"
#include "sge_gdi.h"
#include "setup.h"
#include "sge_any_request.h"
#include "sge_all_listsL.h"
#include "sge_host.h"
#include "sge_load_sensor.h"
#include "sge_log.h"
#include "sge_prog.h"
#include "sgermon.h"
#include "commlib.h"
#include "sge_conf.h"
#include "dispatcher.h"
#include "execd_ck_to_do.h"
#include "execd_get_new_conf.h"
#include "execd_job_exec.h"
#include "execd_kill_execd.h"
#include "execd_signal_queue.h"
#include "execd_ticket.h"
#include "job_report_execd.h"
#include "sge_report_execd.h"
#include "load_avg.h"
#include "parse.h"
#include "sge_feature.h"
#include "reaper_execd.h"
#include "setup_execd.h"
#include "shutdown.h"
#include "sig_handlers.h"
#include "startprog.h"
#include "usage.h"
#include "spool/classic/read_write_job.h"
#include "sge_os.h"
#include "sge_stdlib.h"
#include "sge_spool.h"
#include "sge_answer.h"
#include "basis_types.h"
#include "sge_language.h"
#include "sge_job.h"
#include "sge_mt_init.h"
#include "sge_uidgid.h"

#include "msg_common.h"
#include "msg_execd.h"
#include "msg_gdilib.h"

#ifdef COMPILE_DC
#   include "ptf.h"
#   include "sgedefs.h"
#endif

volatile int jobs_to_start = 1;

/* only used when running as SGE execd */
volatile int waiting4osjid = 1;

/* Store the directory the execd runs in when in normal operation.
 * avoid calling getcwd, cause this catches zombies on sun and is a !?GRML call!
 */
char execd_spool_dir[SGE_PATH_MAX];

static void execd_exit_func(int i);
static void execd_register(void);
static void dispatcher_errfunc(const char *err_str);
static void parse_cmdline_execd(char **argv);
static lList *sge_parse_cmdline_execd(char **argv, lList **ppcmdline);
static lList *sge_parse_execd(lList **ppcmdline, lList **ppreflist, u_long32 *help);

/* DISPATCHER TABLE FOR EXECD */
dispatch_entry execd_dispatcher_table[] = {
   { TAG_JOB_EXECUTION, NULL, NULL, 0, execd_job_exec },
   { TAG_SLAVE_ALLOW,   NULL, NULL, 0, execd_job_slave },
   { TAG_CHANGE_TICKET, NULL, NULL, 0, execd_ticket },
   { TAG_ACK_REQUEST,   NULL, NULL, 0, execd_c_ack }, 
   { TAG_SIGJOB,        NULL, NULL, 0, execd_signal_queue },
   { TAG_SIGQUEUE,      NULL, NULL, 0, execd_signal_queue },
   { TAG_KILL_EXECD,    NULL, NULL, 0, execd_kill_execd  },
   { TAG_NEW_FEATURES,  NULL, NULL, 0, execd_new_features },
   { TAG_GET_NEW_CONF,  NULL, NULL, 0, execd_get_new_conf },
   { -1,                NULL, NULL, 0, execd_ck_to_do}
};


/* time execd maximal waits in the dispatch routine */
#define DISPATCH_TIMEOUT_SGE     2

int main(int argc, char *argv[]);

/*-------------------------------------------------------------------------*/
int main(
int argc,
char **argv 
) {
   int i, dispatch_timeout;
   char err_str[1024];
   int max_enroll_tries;
   int my_pid;
   static char tmp_err_file_name[SGE_PATH_MAX];

   DENTER_MAIN(TOP_LAYER, "execd");


#ifdef __SGE_COMPILE_WITH_GETTEXT__  
   /* init language output for gettext() , it will use the right language */
   sge_init_language_func((gettext_func_type)        gettext,
                         (setlocale_func_type)      setlocale,
                         (bindtextdomain_func_type) bindtextdomain,
                         (textdomain_func_type)     textdomain);
   sge_init_language(NULL,NULL);   
#endif /* __SGE_COMPILE_WITH_GETTEXT__  */

   sge_mt_init();

   /* This needs a better solution */
   umask(022);
      
   /* Initialize path for temporary logging until we chdir to spool */
   my_pid = getpid();
   sprintf(tmp_err_file_name,"%s."U32CFormat"",TMP_ERR_FILE_EXECD,u32c(my_pid));
   log_state_set_log_file(tmp_err_file_name);

   /* exit func for SGE_EXIT() */
   in_main_loop = 0;
   uti_state_set_exit_func(execd_exit_func);
   sge_setup_sig_handlers(EXECD);


   if (sge_setup(EXECD, NULL) != 0) {
      /* sge_setup has already printed the error message */
      SGE_EXIT(1);
   }

   if ((i=sge_occupy_first_three())>=0) {
      CRITICAL((SGE_EVENT, MSG_FILE_REDIRECTFD_I, i));
      SGE_EXIT(1);
   }     
   lInit(nmv);

   parse_cmdline_execd(argv);   
   

   /* exit if we can't get communication handle (bind port) */
   max_enroll_tries = 30;
   while ( cl_com_get_handle((char*)prognames[EXECD],1) == NULL) {
      prepare_enroll(prognames[EXECD]);
      max_enroll_tries--;
      if ( max_enroll_tries <= 0 || shut_me_down ) {
         /* exit after 30 seconds */
         CRITICAL((SGE_EVENT, MSG_COM_ERROR));
         SGE_EXIT(1);
      }
      if (  cl_com_get_handle((char*)prognames[EXECD],1) == NULL) {
        /* sleep when prepare_enroll() failed */
        sleep(1);
      }
   }

   /* daemonizes if qmaster is unreachable */   
   sge_setup_sge_execd(tmp_err_file_name);

   if (!getenv("SGE_ND"))
      daemonize_execd();


   /* are we using qidle or not */
   sge_ls_qidle(use_qidle);
   sge_ls_gnu_ls(1);
   
   DPRINTF(("use_qidle: %d\n", use_qidle));

   /* test load sensor (internal or external) */
   lFreeList(sge_build_load_report());

   execd_register();

   sge_write_pid(EXECD_PID_FILE);

   /* at this point we are sure we are the only sge_execd */
   /* first we have to report any reaped children that might exist */

   starting_up();
   
   /*
   ** log a warning message if execd hasn't been started by root
   */
   if (sge_is_start_user_root()) {
      WARNING((SGE_EVENT, MSG_SWITCH_USER_NOT_ROOT));
   }   

#ifdef COMPILE_DC
   if (ptf_init()) {
      CRITICAL((SGE_EVENT, MSG_EXECD_NOSTARTPTF));
      SGE_EXIT(1);
   }
   INFO((SGE_EVENT, MSG_EXECD_STARTPDCANDPTF));
#endif

   Master_Job_List = lCreateList("Master_Job_List", JB_Type);
   job_list_read_from_disk(&Master_Job_List, "Master_Job_List",
                           0, SPOOL_WITHIN_EXECD, 
                          job_initialize_job);
   
   /* clean up jobs hanging around (look in active_dir) */
   clean_up_old_jobs(1);
   sge_send_all_reports(0, NUM_REP_REPORT_JOB, execd_report_sources);

   dispatch_timeout = DISPATCH_TIMEOUT_SGE;
      
   in_main_loop = 1;

   /***** MAIN LOOP *****/
   while (shut_me_down != 1) {
      /* use auto acknowlege feature of dispatcher for the following
         inbound messages */
      static int tagarray[] = { TAG_SIGJOB, TAG_SIGQUEUE, TAG_NONE };
 
      i = dispatch(execd_dispatcher_table, 
                   sizeof(execd_dispatcher_table)/sizeof(dispatch_entry),
                   tagarray, dispatch_timeout, err_str, dispatcher_errfunc, 1);

      if (sigpipe_received) {
          sigpipe_received = 0;
          INFO((SGE_EVENT, "SIGPIPE received\n"));
      }

      if (i) {             

         if ( strcmp(cl_get_error_text(i), CL_RETVAL_UNDEFINED_STR) != 0 ) {
            if (i != CL_RETVAL_OK) {
               WARNING((SGE_EVENT, MSG_COM_RECEIVEREQUEST_S, cl_get_error_text(i)));
               if (i == CL_RETVAL_CONNECTION_NOT_FOUND ||
                   i == CL_RETVAL_UNKNOWN) {
                  execd_register(); /* reregister at qmaster */
                }
            }
         } else {
            WARNING((SGE_EVENT, MSG_COM_RECEIVEREQUEST_S, err_str ));
         }
      }
   }

   Master_Job_List = lFreeList(Master_Job_List);   
   
   sge_shutdown();
   DEXIT;
   return 0;
}


/*-------------------------------------------------------------
 * Function installed to be called just before exit() is called.
 * clean up
 *-------------------------------------------------------------*/
static void execd_exit_func(
int i 
) {
   DENTER(TOP_LAYER, "execd_exit_func");

   sge_gdi_shutdown();  /* tell commd we're going */

   /* trigger load sensors shutdown */
   sge_ls_stop(0);

#ifdef COMPILE_DC
   ptf_stop();
#endif

   DEXIT;
}

/*-------------------------------------------------------------
 * dispatcher_errfunc
 *
 * function called by dispatcher on non terminal errors 
 *-------------------------------------------------------------*/
static void dispatcher_errfunc(
const char *err_str 
) {
   DENTER(TOP_LAYER, "dispatcher_errfunc");
   ERROR((SGE_EVENT, "%s", err_str));
   DEXIT;
}

/*-------------------------------------------------------------
 * execd_register
 *
 * Function for registering the execd at qmaster
 *-------------------------------------------------------------*/
static void execd_register()
{
   lList *hlp = NULL, *alp = NULL; 
   lListElem *hep;
   int had_problems = 0; /* to ensure single logging */
   int last_commlib_error = CL_RETVAL_OK;

   DENTER(TOP_LAYER, "execd_register");

   hlp = lCreateList("exechost starting", EH_Type);
   hep = lCreateElem(EH_Type);
   lSetUlong(hep, EH_featureset_id, feature_get_active_featureset_id());
   lAppendElem(hlp, hep);

   while (!shut_me_down) {
      lListElem *aep;
      DPRINTF(("*****Checking In With qmaster*****\n"));

      if (had_problems != 0) {
         int i;
         cl_com_handle_t* handle = NULL;
         int commlib_error = CL_RETVAL_OK;
         /*  trigger communication
          *  =====================
          *  cl_commlib_trigger() will block 1 second , when there are no messages to read/write 
          */
         for (i = 0; i< 10 ; i++) {
            
            handle = cl_com_get_handle((char*)prognames[EXECD],1);
            if ( handle == NULL) {
               DPRINTF(("preparing reenroll"));
               prepare_enroll(prognames[EXECD]);
               handle = cl_com_get_handle((char*)prognames[EXECD],1);
            }

            if ( cl_commlib_trigger(handle) != CL_RETVAL_OK) {
               sleep(1);
            }

            commlib_error = sge_get_communication_error();
            if ( commlib_error != CL_RETVAL_OK && commlib_error != last_commlib_error ) {
               last_commlib_error = commlib_error;
               ERROR((SGE_EVENT, MSG_GDI_CANT_GET_COM_HANDLE_SSUUS, 
                                 uti_state_get_qualified_hostname(),
                                 (char*) prognames[uti_state_get_mewho()],
                                 u32c(handle->local->comp_id), 
                                 u32c(handle->service_port),
                                 cl_get_error_text(commlib_error)));
            }
         }
      }

      alp = sge_gdi(SGE_EXECHOST_LIST, SGE_GDI_ADD, &hlp, NULL, NULL);
      aep = lFirst(alp);
      if (!alp || (lGetUlong(aep, AN_status) != STATUS_OK)) {
         if ( had_problems == 0) {
            WARNING((SGE_EVENT, MSG_COM_CANTREGISTER_S, aep?lGetString(aep, AN_text):MSG_COM_ERROR));
            had_problems = 1;
         }
         alp = lFreeList(alp);
         continue;
      }
      break;
   }
   
   hlp = lFreeList(hlp);
   alp = lFreeList(alp);

   DEXIT;
   return;
}


/*---------------------------------------------------------------------
 * parse_cmdline_execd
 *---------------------------------------------------------------------*/
static void parse_cmdline_execd(
char **argv
) {
   lList *ref_list = NULL, *alp = NULL, *pcmdline = NULL;
   lListElem *aep;
   u_long32 help = 0;

   DENTER(TOP_LAYER, "parse_cmdline_execd");
            
   alp = sge_parse_cmdline_execd(argv+1, &pcmdline);
   if(alp) {
      /* 
      ** high level parsing error! show answer list
      */
      for_each(aep, alp) {
         fprintf(stderr, "%s", lGetString(aep, AN_text));
      }
      lFreeList(alp);
      lFreeList(pcmdline);
      SGE_EXIT(1);
   }

   alp = sge_parse_execd(&pcmdline, &ref_list, &help);
   lFreeList(pcmdline);
   lFreeList(ref_list);

   if(alp) {
      /*
      ** low level parsing error! show answer list
      */
      for_each(aep, alp) {
         fprintf(stderr, "%s", lGetString(aep, AN_text));
      }
      lFreeList(alp);
      SGE_EXIT(1);
   }
   lFreeList(alp);

   if(help) {
      /*
      ** user wanted only help. we can exit!
      */
      SGE_EXIT(0);
   }
   DEXIT;
}


/*-------------------------------------------------------------
 * sge_parse_cmdline_execd
 *
 *-------------------------------------------------------------*/ 
static lList *sge_parse_cmdline_execd(
char **argv,
lList **ppcmdline 
) {
char **sp;
char **rp;
stringT str;
lList *alp = NULL;

   DENTER(TOP_LAYER, "sge_parse_cmdline_execd");

   rp = argv;
   while(*(sp=rp)) {
      /* -help */
      if ((rp = parse_noopt(sp, "-help", NULL, ppcmdline, &alp)) != sp)
         continue;

      /* -nostart-commd */
      if ((rp = parse_noopt(sp, "-nostart-commd", NULL, ppcmdline, &alp)) != sp)
         continue;

      /* -lj */
      if ((rp = parse_until_next_opt(sp, "-lj", NULL, ppcmdline, &alp)) != sp)
         continue;

      /* oops */
      sprintf(str, MSG_PARSE_INVALIDARG_S, *sp);
      sge_usage(stderr);
      answer_list_add(&alp, str, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DEXIT;
      return alp;
   }
   DEXIT;
   return alp;
}


/*-------------------------------------------------------------
 * sge_parse_execd
 *
 *-------------------------------------------------------------*/
static lList *sge_parse_execd(lList **ppcmdline, lList **ppreflist, 
                              u_long32 *help) 
{
   stringT str;
   lList *alp = NULL;
   int usageshowed = 0;
   u_long32 flag;

   DENTER(TOP_LAYER, "sge_parse_execd");

   /* Loop over all options. Only valid options can be in the
      ppcmdline list.
   */
   while(lGetNumberOfElem(*ppcmdline))
   {
      flag = 0;
      /* -help */
      if(parse_flag(ppcmdline, "-help", &alp, help)) {
         usageshowed = 1;
         sge_usage(stdout);
         break;
      }

      /* -nostart-commd */
      if(parse_flag(ppcmdline, "-nostart-commd", &alp, &flag)) {
         start_commd = false;
         continue;
      }
   }
   if(lGetNumberOfElem(*ppcmdline)) {
      sprintf(str, MSG_PARSE_TOOMANYARGS);
      if(!usageshowed)
         sge_usage(stderr);
      answer_list_add(&alp, str, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DEXIT;
      return alp;
   }
   DEXIT;
   return alp;
}

