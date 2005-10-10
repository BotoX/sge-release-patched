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
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>

#ifdef WIN32NATIVE
#  include "win32nativetypes.h"
#endif /* WIN32NATIVE */

#include "sge_log.h"
#include "sge_gdi.h"
#include "sge_gdi_request.h"
#include "setup.h"
#include "gdi_setup.h"
#include "sge_any_request.h"
#include "sge_gdiP.h"
#include "commlib.h"
#include "cull_list.h"
#include "sge_prog.h"
#include "sge_all_listsL.h"
#include "sig_handlers.h"
#include "sgermon.h"
#include "sge_unistd.h"
#include "qm_name.h"
#include "sge_security.h"
#include "sge_answer.h"
#include "sge_uidgid.h"
#include "setup_path.h"
#include "sge_feature.h"
#include "sge_bootstrap.h"

#include "uti/sge_profiling.h"

#include "gdi/msg_gdilib.h"

static void default_exit_func(int i);
static void gdi_init_mt(void);

struct gdi_state_t {
   /* gdi request base */
   u_long32 request_id;     /* incremented with each GDI request to have a unique request ID
                               it is ensured that the request ID is also contained in answer */
   int      daemon_first;
   int      first_time;
   int      commd_state;

   int      made_setup;
   int      isalive;

   char     cached_master_name[CL_MAXHOSTLEN];

   gdi_send_t *async_gdi; /* used to store a async GDI request.*/
};

static pthread_key_t   gdi_state_key;

static pthread_once_t gdi_once_control = { PTHREAD_ONCE_INIT };

static void gdi_state_destroy(void* state) {
   free(state);
}

static void gdi_init_mt(void) {
   pthread_key_create(&gdi_state_key, &gdi_state_destroy);
} 
 
/* per process initialization */
void gdi_once_init(void) {

   /* uti */
   uidgid_mt_init();

   bootstrap_mt_init();
   feature_mt_init();
   sge_prof_setup();


   /* gdi */
   gdi_init_mt();
   path_mt_init();
}

static void gdi_state_init(struct gdi_state_t* state) {
   state->request_id = 0;
   state->daemon_first = 1;
   state->first_time = 1;
   state->commd_state = COMMD_UNKNOWN;

   state->made_setup = 0;
   state->isalive = 0;
   strcpy(state->cached_master_name, "");

   state->async_gdi = NULL;
}

/****** gid/gdi_setup/gdi_mt_init() ************************************************
*  NAME
*     gdi_mt_init() -- Initialize GDI state for multi threading use.
*
*  SYNOPSIS
*     void gdi_mt_init(void) 
*
*  FUNCTION
*     Set up GDI. This function must be called at least once before any of the
*     GDI functions is used. This function is idempotent, i.e. it is safe to
*     call it multiple times.
*
*     Thread local storage for the GDI state information is reserved. 
*
*  INPUTS
*     void - NONE 
*
*  RESULT
*     void - NONE
*
*  NOTES
*     MT-NOTE: gdi_mt_init() is MT safe 
*
*******************************************************************************/
void gdi_mt_init(void)
{
   pthread_once(&gdi_once_control, gdi_once_init);
}

/****** libs/gdi/gdi_state_get_????() ************************************
*  NAME
*     gdi_state_get_????() - read access to gdilib global variables
*
*  FUNCTION
*     Provides access to either global variable or per thread global
*     variable.
*
******************************************************************************/


u_long32 gdi_state_get_request_id(void)
{
   GET_SPECIFIC(struct gdi_state_t, gdi_state, gdi_state_init, gdi_state_key, "gdi_state_get_request_id");
   return gdi_state->request_id;
}

u_long32 gdi_state_get_next_request_id(void)
{
   GET_SPECIFIC(struct gdi_state_t, gdi_state, gdi_state_init, gdi_state_key, "gdi_state_get_request_id");
   gdi_state->request_id++;
   return gdi_state->request_id;
}


int gdi_state_get_daemon_first(void)
{
   GET_SPECIFIC(struct gdi_state_t, gdi_state, gdi_state_init, gdi_state_key, "gdi_state_get_daemon_first");
   return gdi_state->daemon_first;
}

int gdi_state_get_first_time(void)
{
   GET_SPECIFIC(struct gdi_state_t, gdi_state, gdi_state_init, gdi_state_key, "gdi_state_get_first_time");
   return gdi_state->first_time;
}

int gdi_state_get_commd_state(void)
{
   GET_SPECIFIC(struct gdi_state_t, gdi_state, gdi_state_init, gdi_state_key, "gdi_state_get_commd_state");
   return gdi_state->commd_state;
}

int gdi_state_get_made_setup(void)
{
   GET_SPECIFIC(struct gdi_state_t, gdi_state, gdi_state_init, gdi_state_key, "gdi_state_get_made_setup");
   return gdi_state->made_setup;
}

int gdi_state_get_isalive(void)
{
   GET_SPECIFIC(struct gdi_state_t, gdi_state, gdi_state_init, gdi_state_key, "gdi_state_get_isalive");
   return gdi_state->isalive;
}

char *gdi_state_get_cached_master_name(void)
{
   GET_SPECIFIC(struct gdi_state_t, gdi_state, gdi_state_init, gdi_state_key, "gdi_state_get_cached_master_name");
   return gdi_state->cached_master_name;
}

gdi_send_t*
gdi_state_get_last_gdi_request(void) {
   GET_SPECIFIC(struct gdi_state_t, gdi_state, gdi_state_init, gdi_state_key, 
                "gdi_state_get_last_gdi_request");

   return gdi_state->async_gdi;
}

void 
gdi_state_clear_last_gdi_request(void) 
{
   GET_SPECIFIC(struct gdi_state_t, gdi_state, gdi_state_init, gdi_state_key, 
                "gdi_state_clear_last_gdi_request");
   gdi_free_request(&(gdi_state->async_gdi));
}

void
gdi_free_request(gdi_send_t **async_gdi) 
{
   (*async_gdi)->out.first = free_gdi_request((*async_gdi)->out.first);
   FREE(*async_gdi);
}

bool
gdi_set_request(const char* rhost, const char* commproc, u_short id, 
                state_gdi_multi *out, u_long32 gdi_request_mid) 
{
   gdi_send_t *async_gdi = NULL;
 
   async_gdi = (gdi_send_t*) malloc(sizeof(gdi_send_t));
   if (async_gdi == NULL) {
      return false;
   }
 
   strncpy(async_gdi->rhost, rhost, CL_MAXHOSTLEN);
   strncpy(async_gdi->commproc, commproc, CL_MAXHOSTLEN);
   async_gdi->id = id;
   async_gdi->gdi_request_mid = gdi_request_mid;

   /* copy gdi request and set the past in one to NULL */
   async_gdi->out.first = out->first;
   out->first = NULL;
   async_gdi->out.last = out->last;
   out->last = NULL;
   async_gdi->out.sequence_id = out->sequence_id;
   out->sequence_id = 0;
 
   { /* set thread specific value */
      GET_SPECIFIC(struct gdi_state_t, gdi_state, gdi_state_init, gdi_state_key, 
                   "gdi_set_request");
      if (gdi_state->async_gdi != NULL) {
         gdi_free_request(&(gdi_state->async_gdi));
      }
      gdi_state->async_gdi = async_gdi;
   }
  
   return true;
}

/****** libs/gdi/gdi_state_set_????() ************************************
*  NAME
*     gdi_state_set_????() - write access to gdilib global variables
*
*  FUNCTION
*     Provides access to either global variable or per thread global
*     variable.
*
******************************************************************************/

void gdi_state_set_request_id(u_long32 id)
{
   GET_SPECIFIC(struct gdi_state_t, gdi_state, gdi_state_init, gdi_state_key, "gdi_state_set_request_id");
   gdi_state->request_id = id;
}

void gdi_state_set_daemon_first(int i)
{
   GET_SPECIFIC(struct gdi_state_t, gdi_state, gdi_state_init, gdi_state_key, "gdi_state_set_daemon_first");
   gdi_state->daemon_first = i;
}

void gdi_state_set_first_time(int i)
{
   GET_SPECIFIC(struct gdi_state_t, gdi_state, gdi_state_init, gdi_state_key, "gdi_state_set_first_time");
   gdi_state->first_time = i;
}

void gdi_state_set_commd_state(int i)
{
   GET_SPECIFIC(struct gdi_state_t, gdi_state, gdi_state_init, gdi_state_key, "gdi_state_set_commd_state");
   gdi_state->commd_state = i;
}

void gdi_state_set_made_setup(int i)
{
   GET_SPECIFIC(struct gdi_state_t, gdi_state, gdi_state_init, gdi_state_key, "gdi_state_set_made_setup");
   gdi_state->made_setup = i;
}

void gdi_state_set_isalive(int i)
{
   GET_SPECIFIC(struct gdi_state_t, gdi_state, gdi_state_init, gdi_state_key, "gdi_state_set_isalive");
   gdi_state->isalive = i;
}



/****** gdi/setup/sge_gdi_setup() *********************************************
*  NAME
*     sge_gdi_setup() -- setup GDI 
*
*  SYNOPSIS
*     int sge_gdi_setup(char* programname) 
*
*  FUNCTION
*     This function initializes the Gridengine Database Interface (GDI)
*     and their underlaying communication mechanisms 
*
*  INPUTS
*     char* programname - Name of thye program without path information 
*  
*  OUTPUT
*     lList **alpp - If the GDI setup fails and alpp is non-NULL an answer 
*                    list is returned containing diagnosis information about
*                    the problem during setup.  If NULL, all errors are printed
*                    to stdout.
*
*  RESULT
*     AE_OK            - everything is fine
*     AE_QMASTER_DOWN  - the master is down 
*     AE_ALREADY_SETUP - sge_gdi_setup() was already called
*
*  NOTES
*     MT-NOTES: sge_gdi_setup() is MT safe
*     MT-NOTES: In a mulit threaded program each thread must call 
*     MT-NOTES: before sge_gdi() and other calls can be used
******************************************************************************/
int sge_gdi_setup(const char *programname, lList **alpp)
{
   bool alpp_was_null = true;
   int last_enroll_error = CL_RETVAL_OK;
   DENTER(TOP_LAYER, "sge_gdi_setup");

   lInit(nmv);

   if (alpp != NULL && *alpp != NULL) {
     alpp_was_null = false;
   }

   /* initialize libraries */
   pthread_once(&gdi_once_control, gdi_once_init);
   if (gdi_state_get_made_setup()) {
      if (alpp_was_null) {
         SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_GDI_GDI_ALREADY_SETUP));
      }
      else {
         answer_list_add_sprintf(alpp, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                 MSG_GDI_GDI_ALREADY_SETUP);
      }
      
      DEXIT;
      return AE_ALREADY_SETUP;
   }

#ifdef __SGE_COMPILE_WITH_GETTEXT__  
   /* init language output for gettext() , it will use the right language */
   sge_init_language_func((gettext_func_type)        gettext,
                         (setlocale_func_type)      setlocale,
                         (bindtextdomain_func_type) bindtextdomain,
                         (textdomain_func_type)     textdomain);
   sge_init_language(NULL,NULL);   
#endif /* __SGE_COMPILE_WITH_GETTEXT__  */

   if (sge_setup(uti_state_get_mewho(), alpp)) {
      if (alpp_was_null) {
         /* This frees the list, so no worries. */
         answer_list_output (alpp);
      }
      
      DEXIT;
      return AE_QMASTER_DOWN;
   }

   prepare_enroll(programname, &last_enroll_error);

   /* ensure gdi default exit func is used if no-one has been specified */
   if (!uti_state_get_exit_func())
      uti_state_set_exit_func(default_exit_func);   

   gdi_state_set_made_setup(1);

   /* check if master is alive */
   if (gdi_state_get_isalive()) {
      DEXIT;  /* TODO: shall we rework the gdi function return values ? CR */
      if (check_isalive(sge_get_master(0)) != CL_RETVAL_OK) {
         return AE_QMASTER_DOWN;
      } else {
         return AE_OK;
      }
   }

   DEXIT;
   return AE_OK;
}


/****** gdi/setup/sge_gdi_param() *********************************************
*  NAME
*     sge_gdi_param() -- add some additional params for sge_gdi_setup() 
*
*  SYNOPSIS
*     int sge_gdi_param(int param, int intval, char* strval) 
*
*  FUNCTION
*     This function makes it possible to pass additional parameters to
*     sge_gdi_setup(). It has to be called before sge_gdi_setup() 
*
*  INPUTS
*     int param    - constant identifying the parameter 
*                       SET_MEWHO - 
*                          intval will be the program id
*                       SET_LEAVE - 
*                          strval is a pointer to an exit function
*                       SET_EXIT_ON_ERROR - 
*                          intval is 0 or 1
*                       SET_ISALIVE - 0 or 1 -  
*                          do check whether master is alive
*     int intval   - integer value or 0 
*     char* strval - string value or NULL 
*
*  RESULT
*     AE_OK            - parameter was set successfully
*     AE_ALREADY_SETUP - sge_gdi_setup() was called beforie 
*                        sge_gdi_param() 
*     AE_UNKNOWN_PARAM - param is an unknown constant
*
*  NOTES
*     MT-NOTES: sge_gdi_setup() is MT safe
*     MT-NOTES: In a mulit threaded program each thread must call 
*     MT-NOTES: sge_gdi_param() to have these settings in effect
******************************************************************************/
int sge_gdi_param(int param, int intval, char *strval) 
{
   DENTER(TOP_LAYER, "sge_gdi_param");

/* initialize libraries */
   pthread_once(&gdi_once_control, gdi_once_init);
   if (gdi_state_get_made_setup()) {
      DEXIT;
      return AE_ALREADY_SETUP;
   }

   switch (param) {
   case SET_MEWHO:
      uti_state_set_mewho(intval);
      break;
   case SET_ISALIVE:
      gdi_state_set_isalive(intval);
      break;
   case SET_LEAVE:
      uti_state_set_exit_func((sge_exit_func_t) strval);
      break;
   case SET_EXIT_ON_ERROR:
      uti_state_set_exit_on_error(intval ? true : false);
      break;
   default:
      DEXIT;
      return AE_UNKNOWN_PARAM;
   }

   DEXIT;
   return AE_OK;
}

static void default_exit_func(int i) 
{
   sge_security_exit(i); 
   cl_com_cleanup_commlib();
}

/****** gdi/setup/sge_gdi_shutdown() ******************************************
*  NAME
*     sge_gdi_shutdown() -- gdi shutdown.
*
*  SYNOPSIS
*     int sge_gdi_shutdown()
*
*  FUNCTION
*     This function has to be called before quitting the program. It 
*     cancels registration at commd.
*
*  NOTES
*     MT-NOTES: sge_gdi_setup() is MT safe
******************************************************************************/  
int sge_gdi_shutdown()
{
   DENTER(TOP_LAYER, "sge_gdi_shutdown");

   /* initialize libraries */
   pthread_once(&gdi_once_control, gdi_once_init);
   default_exit_func(0);

   DEXIT;
   return 0;
}
