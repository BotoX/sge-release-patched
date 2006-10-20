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
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>

#include "sge.h"
#include "sge_log.h"
#include "sgermon.h"
#include "sge_event_master.h"
#include "sge_c_gdi.h"
#include "sge_calendar_qmaster.h"
#include "sge_qmod_qmaster.h"
#include "sge_qinstance_qmaster.h"
#include "sge_time.h"
#include "sge_unistd.h"
#include "sge_answer.h"
#include "sge_cqueue.h"
#include "sge_qinstance.h"
#include "sge_calendar.h"
#include "sge_utility.h"
#include "sge_utility_qmaster.h"
#include "sge_lock.h"
#include "sge_qinstance_state.h"

#include "sge_persistence_qmaster.h"
#include "spool/sge_spooling.h"
#include "sge_bootstrap.h"

#include "msg_common.h"
#include "msg_qmaster.h"

#ifdef TEST_GDI2
#include "sge_gdi_ctx.h"
#endif

int 
calendar_mod(void *context, lList **alpp, lListElem *new_cal, lListElem *cep, int add, 
             const char *ruser, const char *rhost, gdi_object_t *object, 
             int sub_command, monitoring_t *monitor) 
{
   const char *cal_name;

   DENTER(TOP_LAYER, "calendar_mod");

   /* ---- CAL_name cannot get changed - we just ignore it */
   if (add) {
      cal_name = lGetString(cep, CAL_name);
      if (verify_str_key(alpp, cal_name, MAX_VERIFY_STRING, "calendar", KEY_TABLE) != STATUS_OK)
         goto ERROR;
      lSetString(new_cal, CAL_name, cal_name);
   } else {
      cal_name = lGetString(new_cal, CAL_name);
   }

   /* ---- CAL_year_calendar */
   attr_mod_zerostr(cep, new_cal, CAL_year_calendar, "year calendar");
   if (lGetPosViaElem(cep, CAL_year_calendar, SGE_NO_ABORT)>=0) {
      if (!calendar_parse_year(new_cal, alpp)) 
         goto ERROR;
   }

   /* ---- CAL_week_calendar */
   attr_mod_zerostr(cep, new_cal, CAL_week_calendar, "week calendar");
   if (lGetPosViaElem(cep, CAL_week_calendar, SGE_NO_ABORT)>=0) {
      if (!calendar_parse_week(new_cal, alpp))
         goto ERROR;
   }

   DEXIT;
   return 0;

ERROR:
   DEXIT;
   return STATUS_EUNKNOWN;
}

int 
calendar_spool(void *context, lList **alpp, lListElem *cep, gdi_object_t *object) 
{
   lList *answer_list = NULL;
   bool dbret;

#ifdef TEST_GDI2
   sge_gdi_ctx_class_t *ctx = (sge_gdi_ctx_class_t*)context;
   bool job_spooling = ctx->get_job_spooling(ctx);
#else
   bool job_spooling = bootstrap_get_job_spooling();
#endif

   DENTER(TOP_LAYER, "calendar_spool");

   dbret = spool_write_object(&answer_list, spool_get_default_context(), cep,
                              lGetString(cep, CAL_name), SGE_TYPE_CALENDAR,
                              job_spooling);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, 
                              MSG_PERSISTENCE_WRITE_FAILED_S,
                              lGetString(cep, CAL_name));
   }

   DEXIT;
   return dbret ? 0 : 1;
}

int 
sge_del_calendar(void *context, lListElem *cep, lList **alpp, char *ruser, char *rhost) 
{
   const char *cal_name;
   lList **master_calendar_list = object_type_get_master_list(SGE_TYPE_CALENDAR);

   DENTER(TOP_LAYER, "sge_del_calendar");

   if ( !cep || !ruser || !rhost ) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   /* ep is no calendar element, if cep has no CAL_name */
   if (lGetPosViaElem(cep, CAL_name, SGE_NO_ABORT)<0) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_MISSINGCULLFIELD_SS,
            lNm2Str(QU_qname), SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }
   cal_name = lGetString(cep, CAL_name);

   if (!calendar_list_locate(*master_calendar_list, cal_name)) {
      ERROR((SGE_EVENT, MSG_SGETEXT_DOESNOTEXIST_SS, 
             MSG_OBJ_CALENDAR, cal_name));
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EEXIST;
   }

   /* prevent deletion of a still referenced calendar */
   {
      lList *local_answer_list = NULL;

      if (calendar_is_referenced(cep, &local_answer_list, 
                            *(object_type_get_master_list(SGE_TYPE_CQUEUE)))) {
         lListElem *answer = lFirst(local_answer_list);

         ERROR((SGE_EVENT, "denied: %s", lGetString(answer, AN_text)));
         answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC,
                         ANSWER_QUALITY_ERROR);
         lFreeList(&local_answer_list);
         DEXIT;
         return STATUS_ESEMANTIC;
      }
   }

   /* remove timer for this calendar */
   te_delete_one_time_event(TYPE_CALENDAR_EVENT, 0, 0, cal_name);

   sge_event_spool(context, 
                   alpp, 0, sgeE_CALENDAR_DEL, 
                   0, 0, cal_name, NULL, NULL,
                   NULL, NULL, NULL, true, true);
   lDelElemStr(master_calendar_list, CAL_name, cal_name);

   INFO((SGE_EVENT, MSG_SGETEXT_REMOVEDFROMLIST_SSSS,
         ruser, rhost, cal_name, MSG_OBJ_CALENDAR));
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DEXIT;
   return STATUS_OK;
}

/****** qmaster/sge_calendar_qmaster/sge_calendar_event_handler() **************
*  NAME
*     sge_calendar_event_handler() -- calendar event handler
*
*  SYNOPSIS
*     void sge_calendar_event_handler(te_event_t anEvent) 
*
*  FUNCTION
*     Handle calendar events. 
*
*  INPUTS
*     te_event_t anEvent - calendar event
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: sge_calendar_event_handler() is not MT safe 
*
*******************************************************************************/
void sge_calendar_event_handler(void *context, te_event_t anEvent, monitoring_t *monitor) 
{
   lListElem *cep;
   const char* cal_name = te_get_alphanumeric_key(anEvent);
   lList *ppList = NULL;

   DENTER(TOP_LAYER, "sge_calendar_event_handler");

   MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), monitor);

   if (!(cep = calendar_list_locate(*object_type_get_master_list(SGE_TYPE_CALENDAR), cal_name)))
   {
      ERROR((SGE_EVENT, MSG_EVE_TE4CAL_S, cal_name));   
      SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);      
      DEXIT;
      return;
   }
      
   calendar_update_queue_states(context, cep, 0, NULL, &ppList, monitor);
   lFreeList(&ppList);

   sge_free((char *)cal_name);

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);

   DEXIT;
   return;
} /* sge_calendar_event_handler() */

int calendar_update_queue_states(void *context, lListElem *cep, lListElem *old_cep, gdi_object_t *object, lList **ppList, monitoring_t *monitor)
{
   const char *cal_name = lGetString(cep, CAL_name);
   lList *state_changes_list = NULL;
   u_long32 state;
   time_t when = 0; 
   DENTER(TOP_LAYER, "calendar_update_queue_states");

   if (lListElem_is_changed(cep)) {
      sge_add_event( 0, old_cep ? sgeE_CALENDAR_MOD : sgeE_CALENDAR_ADD, 
                    0, 0, cal_name, NULL, NULL, cep);
      lListElem_clear_changed_info(cep);
   }

   state = calender_state_changes(cep, &state_changes_list, &when, NULL);
   
   qinstance_change_state_on_calendar_all(context, cal_name, state, state_changes_list, monitor);

   lFreeList(&state_changes_list);

   if (when) {
      te_event_t ev;

      ev = te_new_event(when, TYPE_CALENDAR_EVENT, ONE_TIME_EVENT, 0, 0, cal_name);
      te_add_event(ev);
      te_free_event(&ev);
   }
  
   DEXIT;
   return 0;
}

