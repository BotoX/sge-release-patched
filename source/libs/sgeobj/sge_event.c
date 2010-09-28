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

#include "sge_eventL.h"
#include "cull.h"
#include "sge_job.h"
#include "sge_sharetree.h"
#include "sge_event.h"

#include "msg_sgeobjlib.h"

/* documentation see libs/evc/sge_event_client.c */
const char *event_text(const lListElem *event, dstring *buffer) 
{
   u_long32 type, intkey, number, intkey2;
   int n=0;
   const char *strkey, *strkey2;
   lList *lp;

   number = lGetUlong(event, ET_number);
   type = lGetUlong(event, ET_type);
   intkey = lGetUlong(event, ET_intkey);
   intkey2 = lGetUlong(event, ET_intkey2);
   strkey = lGetString(event, ET_strkey);
   strkey2 = lGetString(event, ET_strkey2);
   if ((lp=lGetList(event, ET_new_version))) {
      n = lGetNumberOfElem(lp);
   }

   switch (type) {

   /* -------------------- */
   case sgeE_ADMINHOST_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADMINHOSTLISTXELEMENTS_II, (int)number, n);
      break;
   case sgeE_ADMINHOST_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDADMINHOSTX_IS, (int)number, strkey);
      break;
   case sgeE_ADMINHOST_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELADMINHOSTX_IS, (int)number, strkey);
      break;
   case sgeE_ADMINHOST_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODADMINHOSTX_IS, (int)number, strkey);
      break;

   /* -------------------- */
   case sgeE_CALENDAR_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_CALENDARLISTXELEMENTS_II, (int)number, n);
      break;
   case sgeE_CALENDAR_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDCALENDARX_IS, (int)number, strkey);
      break;
   case sgeE_CALENDAR_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELCALENDARX_IS, (int)number, strkey);
      break;
   case sgeE_CALENDAR_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODCALENDARX_IS, (int)number, strkey);
      break;

   /* -------------------- */
   case sgeE_CKPT_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_CKPTLISTXELEMENTS_II, (int)number, n);
      break;
   case sgeE_CKPT_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDCKPT_IS, (int)number, strkey);
      break;
   case sgeE_CKPT_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELCKPT_IS, (int)number, strkey);
      break;
   case sgeE_CKPT_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODCKPT_IS, (int)number, strkey);
      break;

   /* -------------------- */
   case sgeE_CONFIG_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_CONFIGLISTXELEMENTS_II, (int)number, n);
      break;
   case sgeE_CONFIG_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDCONFIGX_IS, (int)number, strkey);
      break;
   case sgeE_CONFIG_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELCONFIGX_IS, (int)number, strkey);
      break;
   case sgeE_CONFIG_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODCONFIGX_IS, (int)number, strkey);
      break;

   /* -------------------- */
   case sgeE_EXECHOST_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_EXECHOSTLISTXELEMENTS_II, (int)number, n);
      break;
   case sgeE_EXECHOST_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDEXECHOSTX_IS, (int)number, strkey);
      break;
   case sgeE_EXECHOST_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELEXECHOSTX_IS, (int)number, strkey);
      break;
   case sgeE_EXECHOST_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODEXECHOSTX_IS, (int)number, strkey);
      break;

   /* -------------------- */
   case sgeE_GLOBAL_CONFIG:
      sge_dstring_sprintf(buffer, MSG_EVENT_GLOBAL_CONFIG_I, (int)number);
      break;

   /* -------------------- */
   case sgeE_JATASK_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDJATASK_US, u32c(number), job_get_id_string(intkey, intkey2, strkey));
      break;
   case sgeE_JATASK_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELJATASK_US, u32c(number), job_get_id_string(intkey, intkey2, strkey));
      break;
   case sgeE_JATASK_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODJATASK_US, u32c(number), job_get_id_string(intkey, intkey2, strkey));
      break;

   /* -------------------- */
   case sgeE_PETASK_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDPETASK_US, u32c(number), job_get_id_string(intkey, intkey2, strkey));
      break;
   case sgeE_PETASK_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELPETASK_US, u32c(number), job_get_id_string(intkey, intkey2, strkey));
      break;
#if 0      
   /* JG: we'll have it soon ;-) */
   case sgeE_PETASK_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODPETASK_UUUS, u32c(number), job_get_id_string(intkey, intkey2, strkey));
      break;
#endif

   /* -------------------- */
   case sgeE_JOB_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_JOBLISTXELEMENTS_UI, u32c(number), n);
      break;
   case sgeE_JOB_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDJOB_US, u32c(number), job_get_id_string(intkey, intkey2, strkey));
      break;
   case sgeE_JOB_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELJOB_US, u32c(number), job_get_id_string(intkey, intkey2, strkey));
      break;
   case sgeE_JOB_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODJOB_US, u32c(number), job_get_id_string(intkey, intkey2, strkey));
      break;
   case sgeE_JOB_MOD_SCHED_PRIORITY:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODSCHEDDPRIOOFJOBXTOY_USI, 
            u32c(number), 
            job_get_id_string(intkey, intkey2, strkey),
            ((int)lGetUlong(lFirst(lp), JB_priority))-BASE_PRIORITY);
      break;
   case sgeE_JOB_USAGE:
      sge_dstring_sprintf(buffer, MSG_EVENT_JOBXUSAGE_US, 
         u32c(number), job_get_id_string(intkey, intkey2, strkey));
      break;
   case sgeE_JOB_FINAL_USAGE:
      sge_dstring_sprintf(buffer, MSG_EVENT_JOBXFINALUSAGE_US, 
         u32c(number), job_get_id_string(intkey, intkey2, strkey));
      break;

   case sgeE_JOB_FINISH:
      sge_dstring_sprintf(buffer, MSG_EVENT_JOBXFINISH_US, 
         u32c(number), job_get_id_string(intkey, intkey2, strkey));
      break;

   /* -------------------- */
   case sgeE_JOB_SCHEDD_INFO_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_JOB_SCHEDD_INFOLISTXELEMENTS_II, (int)number, n);
      break;
   case sgeE_JOB_SCHEDD_INFO_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDJOB_SCHEDD_INFO_III, (int)number, (int)intkey, (int)intkey2);
      break;
   case sgeE_JOB_SCHEDD_INFO_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELJOB_SCHEDD_INFO_III, (int)number, (int)intkey, (int)intkey2);
      break;
   case sgeE_JOB_SCHEDD_INFO_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODJOB_SCHEDD_INFO_III, (int)number, (int)intkey, (int)intkey2);
      break;

   /* -------------------- */
   case sgeE_MANAGER_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_MANAGERLISTXELEMENTS_II, (int)number, n);
      break;
   case sgeE_MANAGER_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDMANAGERX_IS, (int)number, strkey);
      break;
   case sgeE_MANAGER_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELMANAGERX_IS, (int)number, strkey);
      break;
   case sgeE_MANAGER_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODMANAGERX_IS, (int)number, strkey);
      break;

   /* -------------------- */
   case sgeE_OPERATOR_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OPERATORLISTXELEMENTS_II, (int)number, n);
      break;
   case sgeE_OPERATOR_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOPERATORX_IS, (int)number, strkey);
      break;
   case sgeE_OPERATOR_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOPERATORX_IS, (int)number, strkey);
      break;
   case sgeE_OPERATOR_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOPERATORX_IS, (int)number, strkey);
      break;

   /* -------------------- */
   case sgeE_NEW_SHARETREE:
      sge_dstring_sprintf(buffer, MSG_EVENT_SHARETREEXNODESYLEAFS_III, (int)number, 
         lGetNumberOfNodes(NULL, lp, STN_children),
         lGetNumberOfLeafs(NULL, lp, STN_children));
      break;

   /* -------------------- */
   case sgeE_PE_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_PELISTXELEMENTS_II, (int)number, n);
      break;
   case sgeE_PE_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDPEX_IS, (int)number, strkey);
      break;
   case sgeE_PE_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELPEX_IS, (int)number, strkey);
      break;
   case sgeE_PE_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODPEX_IS , (int)number, strkey);
      break;

   /* -------------------- */
   case sgeE_PROJECT_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_PROJECTLISTXELEMENTS_II, (int)number, n);
      break;
   case sgeE_PROJECT_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDPROJECTX_IS, (int)number, strkey);
      break;
   case sgeE_PROJECT_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELPROJECTX_IS , (int)number, strkey);
      break;
   case sgeE_PROJECT_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODPROJECTX_IS, (int)number, strkey);
      break;

   /* -------------------- */
   case sgeE_QMASTER_GOES_DOWN:
      sge_dstring_sprintf(buffer, MSG_EVENT_QMASTERGOESDOWN_I, (int)number);
      break;

   /* -------------------- */
   case sgeE_CQUEUE_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_CQUEUELISTXELEMENTS_II, (int)number, n);
      break;
   case sgeE_CQUEUE_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDCQUEUEX_IS, (int)number, strkey);
      break;
   case sgeE_CQUEUE_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELCQUEUEX_IS, (int)number, strkey);
      break;
   case sgeE_CQUEUE_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODCQUEUEX_IS, (int)number, strkey);
      break;

   /* -------------------- */
   case sgeE_QINSTANCE_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDQINSTANCE_ISS, (int)number, strkey, strkey2);
      break;
   case sgeE_QINSTANCE_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELQINSTANCE_ISS, (int)number, strkey, strkey2);
      break;
   case sgeE_QINSTANCE_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODQINSTANCE_ISS, (int)number, strkey, strkey2);
      break;
   case sgeE_QINSTANCE_USOS:
      sge_dstring_sprintf(buffer, MSG_EVENT_UNSUSPENDQUEUEXONSUBORDINATE_IS, (int)number, strkey);
      break;
   case sgeE_QINSTANCE_SOS:
      sge_dstring_sprintf(buffer, MSG_EVENT_SUSPENDQUEUEXONSUBORDINATE_IS , (int)number, strkey);
      break;

   /* -------------------- */
   case sgeE_SCHED_CONF:
      sge_dstring_sprintf(buffer, MSG_EVENT_SCHEDULERCONFIG_I , (int)number);
      break;

   /* -------------------- */
   case sgeE_SCHEDDMONITOR:
      sge_dstring_sprintf(buffer, MSG_EVENT_TRIGGERSCHEDULERMONITORING_I, (int)number);
      break;

   /* -------------------- */
   case sgeE_SHUTDOWN:
      sge_dstring_sprintf(buffer, MSG_EVENT_SHUTDOWN_I, (int)number);
      break;

   /* -------------------- */
   case sgeE_SUBMITHOST_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_SUBMITHOSTLISTXELEMENTS_II, (int)number, n);
      break;
   case sgeE_SUBMITHOST_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDSUBMITHOSTX_IS, (int)number, strkey);
      break;
   case sgeE_SUBMITHOST_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELSUBMITHOSTX_IS, (int)number, strkey);
      break;
   case sgeE_SUBMITHOST_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODSUBMITHOSTX_IS, (int)number, strkey);
      break;

   /* -------------------- */
   case sgeE_USER_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_USERLISTXELEMENTS_II , (int)number, n);
      break;
   case sgeE_USER_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDUSERX_IS, (int)number, strkey);
      break;
   case sgeE_USER_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELUSERX_IS, (int)number, strkey);
      break;
   case sgeE_USER_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODUSERX_IS, (int)number, strkey);
      break;

   /* -------------------- */
   case sgeE_USERSET_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_USERSETLISTXELEMENTS_II , (int)number, n);
      break;
   case sgeE_USERSET_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDUSERSETX_IS , (int)number, strkey);
      break;
   case sgeE_USERSET_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELUSERSETX_IS, (int)number, strkey);
      break;
   case sgeE_USERSET_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODUSERSETX_IS, (int)number, strkey);
      break;

   /* -------------------- */
   case sgeE_HGROUP_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_HGROUPLISTXELEMENTS_II , (int)number, n);
      break;
   case sgeE_HGROUP_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDHGROUPX_IS , (int)number, strkey);
      break;
   case sgeE_HGROUP_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELHGROUPX_IS, (int)number, strkey);
      break;
   case sgeE_HGROUP_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODHGROUPX_IS, (int)number, strkey);
      break;

   /* -------------------- */
   case sgeE_CENTRY_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_CENTRYLISTXELEMENTS_II , (int)number, n);
      break;
   case sgeE_CENTRY_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDCENTRYX_IS , (int)number, strkey);
      break;
   case sgeE_CENTRY_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELCENTRYX_IS, (int)number, strkey);
      break;
   case sgeE_CENTRY_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODCENTRYX_IS, (int)number, strkey);
      break;

   /* -------------------- */
   default:
      sge_dstring_sprintf(buffer, MSG_EVENT_NOTKNOWN_I, (int)number);
      break;
   }

   return sge_dstring_get_string(buffer);
}

