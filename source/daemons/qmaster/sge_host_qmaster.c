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

#include "sge.h"
#include "sge_conf.h"
#include "symbols.h"
#include "sge_prog.h"
#include "sge_time.h"
#include "sge_feature.h"
#include "sge_idL.h"
#include "sge_ja_task.h"
#include "commlib.h"
#include "sge_host.h"
#include "sge_manop.h"
#include "sge_host_qmaster.h"
#include "sge_gdi_request.h"
#include "sge_answer.h"
#include "sge_utility.h"
#include "sge_event_master.h"
#include "sge_queue_event_master.h"
#include "sge_job_schedd.h"
#include "sge_c_gdi.h"
#include "mail.h"
#include "sgermon.h"
#include "sge_log.h"
#include "sge_parse_num_par.h"
#include "configuration_qmaster.h"
#include "sge_qmod_qmaster.h"
#include "sge_cqueue_qmaster.h"
#include "sort_hosts.h"
#include "sge_userset_qmaster.h"
#include "sge_userprj_qmaster.h"
#include "sge_complex_schedd.h"
#include "reschedule.h"
#include "sge_string.h"
#include "sge_security.h"
#include "sge_unistd.h"
#include "sge_hostname.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sge_qinstance_qmaster.h"
#include "sge_job.h"
#include "sge_report.h"
#include "sge_userprj.h"
#include "sge_userset.h"
#include "sge_utility_qmaster.h"
#include "qmaster_to_execd.h"
#include "sge_centry.h"
#include "sge_href.h"
#include "sge_cqueue.h"
#include "sge_str.h"
#include "sge_load.h"
#include "sge_lock.h"
#include "configuration_qmaster.h"

#include "sge_persistence_qmaster.h"
#include "sge_reporting_qmaster.h"
#include "sge_advance_reservation_qmaster.h"
#include "sge_bootstrap.h"
#include "spool/sge_spooling.h"
#include "sched/sge_resource_utilization.h"
#include "sched/sge_serf.h"
#include "sched/debit.h"
#include "sched/valid_queue_user.h"

#include "msg_common.h"
#include "msg_qmaster.h"
#include "sgeobj/msg_sgeobjlib.h"

static void master_kill_execds(sge_gdi_ctx_class_t *ctx, sge_gdi_request *request, sge_gdi_request *answer);
static void host_trash_nonstatic_load_values(lListElem *host);
static void notify(sge_gdi_ctx_class_t *ctx, lListElem *lel, sge_gdi_request *answer, int kill_jobs, int force);
static int verify_scaling_list(lList **alpp, lListElem *host); 
static void host_update_categories(const lListElem *new_hep, const lListElem *old_hep);
static int attr_mod_threshold(sge_gdi_ctx_class_t *ctx, lList **alpp, lListElem *qep, lListElem *new_ep,
                              int sub_command, char *attr_name, char *object_name);

/****** qmaster/host/host_trash_nonstatic_load_values() ***********************
*  NAME
*     host_trash_nonstatic_load_values() -- Trash old load values 
*
*  SYNOPSIS
*     static void host_trash_nonstatic_load_values(lListElem *host) 
*
*  FUNCTION
*     Trash old load values in "host" element 
*
*  INPUTS
*     lListElem *host - EH_Type element 
*
*  RESULT
*     void - None
*******************************************************************************/
static void host_trash_nonstatic_load_values(lListElem *host) 
{
   lList *load_attr_list;
   lListElem *next_load_attr;
   lListElem *load_attr;

   load_attr_list = lGetList(host, EH_load_list);
   next_load_attr = lFirst(load_attr_list);
   while ((load_attr = next_load_attr)) {
      const char *load_attr_name = lGetString(load_attr, HL_name);

      next_load_attr = lNext(load_attr);
      if (!sge_is_static_load_value(load_attr_name)) {
         lRemoveElem(load_attr_list, &load_attr);
      }
   }
   if (lGetNumberOfElem(load_attr_list) == 0) {
      lSetList(host, EH_load_list, NULL);
   }
}

/* ------------------------------------------ 

   adds the host to host_list with type 

   -1 error 
   0 ok
   1 host does exist

 */
int sge_add_host_of_type(
sge_gdi_ctx_class_t *ctx,
const char *hostname,
u_long32 target, 
monitoring_t *monitor 
) {
   int ret;
   int dataType;
   int pos;
   lListElem *ep;
   gdi_object_t *object;
   lList *ppList = NULL;
   const char *username = ctx->get_username(ctx);
   const char *qualified_hostname = ctx->get_qualified_hostname(ctx);

   DENTER(TOP_LAYER, "sge_add_host_of_type");

   if (!hostname) {
      DEXIT;
      return -1;
   }
  
   object = get_gdi_object(target);
   
   /* prepare template */
   ep = lCreateElem(object->type);
   pos = lGetPosInDescr(object->type,object->key_nm);
   dataType = lGetPosType(object->type , pos);
   switch (dataType) {
      case lStringT:
         lSetString(ep, object->key_nm, hostname);
         break;
      case lHostT:
         lSetHost(ep, object->key_nm, hostname);
         break;
      default:
         DPRINTF(("sge_add_host_of_type: unexpected datatype\n"));
   }
   ret = sge_gdi_add_mod_generic(ctx, NULL, ep, 1, object, username, 
      qualified_hostname, 0, &ppList, monitor);
   lFreeElem(&ep);
   lFreeList(&ppList);

   DEXIT;
   return (ret == STATUS_OK) ? 0 : -1;
}

bool
host_list_add_missing_href(sge_gdi_ctx_class_t *ctx,
                           lList *this_list, 
                           lList **answer_list, 
                           const lList *href_list, 
                           monitoring_t *monitor)
{
   bool ret = true;
   lListElem *href = NULL;

   DENTER(TOP_LAYER, "host_list_add_missing_href");
   for_each(href, href_list) {
      const char *hostname = lGetHost(href, HR_name);
      lListElem *host = host_list_locate(this_list, hostname);

      if (host == NULL) {
         ret &= (sge_add_host_of_type(ctx, hostname, SGE_EXECHOST_LIST, monitor) == 0);
      }
   }
   DEXIT;
   return ret;
}

/* ------------------------------------------------------------

   sge_del_host - deletes a host from the host_list

   if the invoking process is the qmaster the host list is
   spooled to disk

*/
int sge_del_host(
sge_gdi_ctx_class_t *ctx,
lListElem *hep,
lList **alpp,
char *ruser,
char *rhost,
u_long32 target,
const lList* master_hGroup_List
) {
   int pos;
   lListElem *ep;
   const char *host;
   char unique[CL_MAXHOSTLEN];
   lList **host_list = NULL;
   int nm = 0;
   char *name = NULL;
   int ret;
   const char *qualified_hostname = ctx->get_qualified_hostname(ctx);

   DENTER(TOP_LAYER, "sge_del_host");

   if ( !hep || !ruser || !rhost ) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   switch ( target ) {
   case SGE_EXECHOST_LIST:
      host_list = object_type_get_master_list(SGE_TYPE_EXECHOST);
      nm = EH_name;
      name = "execution host";
      break;
   case SGE_ADMINHOST_LIST:
      host_list = object_type_get_master_list(SGE_TYPE_ADMINHOST);
      nm = AH_name;
      name = "administrative host";
      break;
   case SGE_SUBMITHOST_LIST:
      host_list = object_type_get_master_list(SGE_TYPE_SUBMITHOST);
      nm = SH_name;
      name = "submit host";
      break;
   default:
     DEXIT;
     return STATUS_EUNKNOWN;
   }
   /* ep is no host element, if ep has no nm */
   if ((pos = lGetPosViaElem(hep, nm, SGE_NO_ABORT)) < 0) {
      ERROR((SGE_EVENT, MSG_SGETEXT_MISSINGCULLFIELD_SS,
            lNm2Str(nm), SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   host = lGetPosHost(hep, pos);
   if (!host) {
      ERROR((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   ret = sge_resolve_hostname(host, unique, EH_name);
   if (ret  != CL_RETVAL_OK) {
      /* 
       * Due to CR 6319231, IZ 1760 this is allowed 
       */
      ;
   }

   /* check if host is in host list */
   if ((ep=host_list_locate(*host_list, unique))==NULL) {
      /* may be host was not the unique hostname.
         Get the unique hostname and try to find it again. */
      if (getuniquehostname(host, unique, 0)!=CL_RETVAL_OK)
      {
         ERROR((SGE_EVENT, MSG_SGETEXT_CANTRESOLVEHOST_S, host));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DEXIT;
         return STATUS_EUNKNOWN;
      }
      /* again check if host is in host list. This time use the unique
         hostname */
      if ((ep=host_list_locate(*host_list, unique))==NULL) {
         ERROR((SGE_EVENT, MSG_SGETEXT_DOESNOTEXIST_SS, name, host));
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DEXIT;
         return STATUS_EEXIST;
      }
   }

   /* 
      check if someone tries to delete 
      the qmaster host from admin host list
   */
   if (target==SGE_ADMINHOST_LIST && 
         !sge_hostcmp(unique, qualified_hostname)) {
      ERROR((SGE_EVENT, MSG_SGETEXT_CANTDELADMINQMASTER_S, 
          qualified_hostname));
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EEXIST;
   }

   if (target == SGE_EXECHOST_LIST && 
       host_is_referenced(hep, alpp, 
                          *(object_type_get_master_list(SGE_TYPE_CQUEUE)),
                          master_hGroup_List)) {
      answer_list_log(alpp, false);                    
      DEXIT;
      return STATUS_ESEMANTIC;
   }

   if (target==SGE_EXECHOST_LIST && !strcasecmp(unique, "global")) {
      ERROR((SGE_EVENT, MSG_OBJ_DELGLOBALHOST));
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_ESEMANTIC;
   }

   /* remove host file and send event */
   switch(target) {
      case SGE_ADMINHOST_LIST:
         {
            lList *answer_list = NULL;
            sge_event_spool(ctx, &answer_list, 0, sgeE_ADMINHOST_DEL, 
                            0, 0, lGetHost(ep, nm), NULL, NULL,
                            NULL, NULL, NULL, true, true);
            answer_list_output(&answer_list);
         }
         break;
      case SGE_EXECHOST_LIST:
         {
            lList *answer_list = NULL;
            sge_event_spool(ctx, &answer_list, 0, sgeE_EXECHOST_DEL, 
                            0, 0, lGetHost(ep, nm), NULL, NULL,
                            NULL, NULL, NULL, true, true);
            answer_list_output(&answer_list);
         }
	 host_update_categories(NULL, ep);

         break;
      case SGE_SUBMITHOST_LIST:
         {
            lList *answer_list = NULL;
            sge_event_spool(ctx, &answer_list, 0, sgeE_SUBMITHOST_DEL, 
                            0, 0, lGetHost(ep, nm), NULL, NULL,
                            NULL, NULL, NULL, true, true);
            answer_list_output(&answer_list);
         }
         break;
   }

   /* delete found host element */
   lRemoveElem(*host_list, &ep);

   INFO((SGE_EVENT, MSG_SGETEXT_REMOVEDFROMLIST_SSSS, 
         ruser, rhost, unique, name));
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DEXIT;
   return STATUS_OK;
}

/* ------------------------------------------------------------ */

int host_mod(
sge_gdi_ctx_class_t *ctx,
lList **alpp,
lListElem *new_host,
lListElem *ep,
int add,
const char *ruser,
const char *rhost,
gdi_object_t *object,
int sub_command, monitoring_t *monitor
) {
   const char *host;
   int nm;
   int pos;
   int dataType;
   object_description *object_base = object_type_get_object_description();

   DENTER(TOP_LAYER, "host_mod");

   nm = object->key_nm;

   /* ---- [EAS]H_name */
   if (add) {
      if (attr_mod_str(alpp, ep, new_host, nm, object->object_name)) {
         goto ERROR;
      }
   }
   pos = lGetPosViaElem(new_host, nm, SGE_NO_ABORT);
   dataType = lGetPosType(lGetElemDescr(new_host),pos);
   if (dataType == lHostT) {
      host = lGetHost(new_host, nm);
   } else {
      host = lGetString(new_host, nm);
   }
   if (nm == EH_name) {
      bool acl_changed = false;

      /* ---- EH_scaling_list */
      if (lGetPosViaElem(ep, EH_scaling_list, SGE_NO_ABORT)>=0) {
         attr_mod_sub_list(alpp, new_host, EH_scaling_list, HS_name, ep,
                           sub_command, SGE_ATTR_LOAD_SCALING, SGE_OBJ_EXECHOST, 0);
         if (verify_scaling_list(alpp, new_host)!=STATUS_OK) {
            goto ERROR;
         }   
      }

      /* ---- EH_consumable_config_list */
      if (attr_mod_threshold(ctx, alpp, ep, new_host,
                             sub_command, SGE_ATTR_COMPLEX_VALUES, 
                             SGE_OBJ_EXECHOST)) { 
         goto ERROR;
      }

      /* ---- EH_acl */
      if (lGetPosViaElem(ep, EH_acl, SGE_NO_ABORT)>=0) {
         DPRINTF(("got new EH_acl\n"));
         acl_changed = true;
         /* check acl list */
         if (userset_list_validate_acl_list(lGetList(ep, EH_acl), alpp) != STATUS_OK) {
            goto ERROR;
         }   
         attr_mod_sub_list(alpp, new_host, EH_acl, US_name, ep,
                           sub_command, SGE_ATTR_USER_LISTS, SGE_OBJ_EXECHOST, 0);
      }

      /* ---- EH_xacl */
      if (lGetPosViaElem(ep, EH_xacl, SGE_NO_ABORT)>=0) {
         DPRINTF(("got new EH_xacl\n"));
         acl_changed = true;
         /* check xacl list */
         if (userset_list_validate_acl_list(lGetList(ep, EH_xacl), alpp) != STATUS_OK) {
            goto ERROR;
         }   
         attr_mod_sub_list(alpp, new_host, EH_xacl, US_name, ep,
                           sub_command, SGE_ATTR_XUSER_LISTS, 
                           SGE_OBJ_EXECHOST, 0);
      }


      /* ---- EH_prj */
      if (lGetPosViaElem(ep, EH_prj, SGE_NO_ABORT)>=0) {
         DPRINTF(("got new EH_prj\n"));
         /* check prj list */
         if (verify_userprj_list(alpp, lGetList(ep, EH_prj),
                  *object_base[SGE_TYPE_PROJECT].list, "projects",
                  object->object_name, host)!=STATUS_OK) {
            goto ERROR;
         }   
         attr_mod_sub_list(alpp, new_host, EH_prj, UP_name, ep,
                           sub_command, SGE_ATTR_PROJECTS, 
                           SGE_OBJ_EXECHOST, 0);    
      }

      /* ---- EH_xprj */
      if (lGetPosViaElem(ep, EH_xprj, SGE_NO_ABORT)>=0) {
         DPRINTF(("got new EH_xprj\n"));
         /* check xprj list */
         if (verify_userprj_list(alpp, lGetList(ep, EH_xprj), 
                  *object_base[SGE_TYPE_PROJECT].list, "xprojects",
                  object->object_name, host)!=STATUS_OK) {
            goto ERROR;
         }   
         attr_mod_sub_list(alpp, new_host, EH_xprj, UP_name, ep,
                           sub_command, SGE_ATTR_XPROJECTS, 
                           SGE_OBJ_EXECHOST, 0);   
      }

      /* ---- EH_usage_scaling_list */
      if (lGetPosViaElem(ep, EH_usage_scaling_list, SGE_NO_ABORT)>=0) {
         attr_mod_sub_list(alpp, new_host, EH_usage_scaling_list, HS_name, ep,
         sub_command, SGE_ATTR_USAGE_SCALING, SGE_OBJ_EXECHOST, 0); 
      }

      if (lGetPosViaElem(ep, EH_report_variables, SGE_NO_ABORT)>=0) {
         const lListElem *var;

         attr_mod_sub_list(alpp, new_host, EH_report_variables, STU_name, ep,
                           sub_command, "report_variables", 
                           SGE_OBJ_EXECHOST, 0);
     
         /* check if all report_variables are valid complex variables */
         for_each(var, lGetList(ep, EH_report_variables)) {
            const char *name = lGetString(var, STU_name);
            if (centry_list_locate(*object_base[SGE_TYPE_CENTRY].list, name) == NULL) {
               ERROR((SGE_EVENT, MSG_SGETEXT_UNKNOWN_RESOURCE_S, name));
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               goto ERROR;
            }
         }
      }

      if (acl_changed == true) {
         lListElem *ar;
         lList *master_userset_list = *(object_type_get_master_list(SGE_TYPE_USERSET));

         for_each(ar, *(object_type_get_master_list(SGE_TYPE_AR))) {
            if (lGetElemStr(lGetList(ar, AR_granted_slots), JG_qhostname, host)) {
               if (!sge_ar_have_users_access(NULL, ar, host, lGetList(ep, EH_acl),
                                             lGetList(ep, EH_xacl),
                                             master_userset_list)) {
                  ERROR((SGE_EVENT, MSG_PARSE_MOD3_REJECTED_DUE_TO_AR_SU, 
                         SGE_ATTR_USER_LISTS, sge_u32c(lGetUlong(ar, AR_id))));
                  answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                  goto ERROR;
               }
            }
         }
      }
   }

   DRETURN(0);
ERROR:
   DRETURN(STATUS_EUNKNOWN);
}

int host_spool(sge_gdi_ctx_class_t *ctx, lList **alpp, lListElem *ep, gdi_object_t *object)
{
   int pos;
   int dataType;
   const char *key;
   sge_object_type host_type = SGE_TYPE_ADMINHOST;

   bool dbret;
   lList *answer_list = NULL;
   bool job_spooling = ctx->get_job_spooling(ctx);

   DENTER(TOP_LAYER, "host_spool");

   pos = lGetPosViaElem(ep, object->key_nm, SGE_NO_ABORT );
   dataType = lGetPosType(lGetElemDescr(ep),pos);
   if (dataType == lHostT ) { 
      key = lGetHost(ep, object->key_nm);
   } else {
      key = lGetString(ep, object->key_nm);
   }
     
   switch (object->key_nm) {
      case AH_name:
         host_type = SGE_TYPE_ADMINHOST;
         break;
      case EH_name:
         host_type = SGE_TYPE_EXECHOST;
         break;
      case SH_name:
         host_type = SGE_TYPE_SUBMITHOST;
         break;
   }
     
   dbret = spool_write_object(alpp, spool_get_default_context(), ep, key, host_type, job_spooling);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, 
                              MSG_PERSISTENCE_WRITE_FAILED_S,
                              key);
   }

   DEXIT;
   return dbret ? 0 : 1;
}

int host_success(sge_gdi_ctx_class_t *ctx, lListElem *ep, lListElem *old_ep, gdi_object_t *object, lList **ppList, monitoring_t *monitor) 
{
   DENTER(TOP_LAYER, "host_success");

   switch(object->key_nm) {
      case EH_name:
      {
         const char *host = lGetHost(ep, EH_name);
         int global_host = !strcmp(SGE_GLOBAL_NAME, host);

         sge_change_queue_version_exechost(ctx, host);

         if (global_host) {
            host_list_merge(*object_type_get_master_list(SGE_TYPE_EXECHOST));
         } else {
            const lListElem *global_ep = NULL;

            global_ep = lGetElemHost(*object_type_get_master_list(SGE_TYPE_EXECHOST), EH_name, 
                                     SGE_GLOBAL_NAME);
            host_merge(ep, global_ep);
         }

         host_update_categories(ep, old_ep);
         sge_add_event( 0, old_ep?sgeE_EXECHOST_MOD:sgeE_EXECHOST_ADD, 
                       0, 0, host, NULL, NULL, ep);
         lListElem_clear_changed_info(ep);
      }
      break;

      case AH_name:
         sge_add_event( 0, old_ep?sgeE_ADMINHOST_MOD:sgeE_ADMINHOST_ADD, 
                       0, 0, lGetHost(ep, AH_name), NULL, NULL, ep);
         lListElem_clear_changed_info(ep);
      break;

      case SH_name:
         sge_add_event( 0, old_ep?sgeE_SUBMITHOST_MOD:sgeE_SUBMITHOST_ADD, 
                       0, 0, lGetHost(ep, SH_name), NULL, NULL, ep);
         lListElem_clear_changed_info(ep);
      break;
   }

   DRETURN(0);
}

/* ------------------------------------------------------------ */

void sge_mark_unheard(
lListElem *hep,
const char *target    /* prognames[QSTD|EXECD] */
) {
   const char *host;

   DENTER(TOP_LAYER, "sge_mark_unheard");

   host = lGetHost(hep, EH_name);

   if (target) {
      if (cl_com_remove_known_endpoint_from_name((char*)host,(char*)target,1) == CL_RETVAL_OK) {
         DEBUG((SGE_EVENT, "set %s/%s/%d to unheard\n", host, target, 1 ));
      }
   } else {
      if (cl_com_remove_known_endpoint_from_name((char*)host,(char*)prognames[EXECD],1) == CL_RETVAL_OK) {
         DEBUG((SGE_EVENT, "set %s/%s/%d to unheard\n", host,(char*)prognames[EXECD], 1 ));
      }
   }

   host_trash_nonstatic_load_values(hep);
   cqueue_list_set_unknown_state(
         *(object_type_get_master_list(SGE_TYPE_CQUEUE)),
         host, true, true);

   DRETURN_VOID;
}

/* ----------------------------------------

   updates global and host specific load values
   using the load report list lp
*/
void sge_update_load_values(
sge_gdi_ctx_class_t *ctx,
char *rhost,
lList *lp 
) {
   u_long32 now;
   const char *report_host = NULL;
   lListElem *ep, **hepp = NULL;
   lListElem *lep;
   lListElem *global_ep = NULL, *host_ep = NULL;
   bool added_non_static = false, statics_changed = false;

   DENTER(TOP_LAYER, "sge_update_load_values");

   /* JG: TODO: this time should better come with the report.
    *           it is the time when the reported values were valid.
    */
   now = sge_get_gmt();

   /* loop over all received load values */
   for_each(ep, lp) {
      const char *name, *value, *host;
      u_long32 global, is_static;

      /* get name, value and other info */
      name = lGetString(ep, LR_name);
      value = lGetString(ep, LR_value);
      host  = lGetHost(ep, LR_host);
      global = lGetUlong(ep, LR_global);
      is_static = lGetUlong(ep, LR_static);

      /* erroneous load report */
      if (!name || !value || !host) {
         continue;
      }   

      /* handle global or exec host? */
      if(global) {
         hepp = &global_ep;
      } else {
         hepp = &host_ep;
      }

      /* update load value list of rhost */
      if (*hepp == NULL) {
         *hepp = host_list_locate(*object_type_get_master_list(SGE_TYPE_EXECHOST), host);
         if (!*hepp) {
            if (!global) {
               report_host = lGetHost(ep, LR_host); /* this is our error indicator */
            }
            DPRINTF(("got load value for UNKNOWN host "SFQ"\n", 
                      lGetHost(ep, LR_host)));
            continue;
         }
      } 

      /* replace old load value or add a new one */
      lep = lGetSubStr(*hepp, HL_name, name, EH_load_list);  

      if (lep == NULL) {
         lep = lAddSubStr(*hepp, HL_name, name, EH_load_list, HL_Type);
         DPRINTF(("%s: adding load value: "SFQ" = "SFQ"\n", 
               lGetHost(ep, LR_host), name, value));

         if (is_static) {
            statics_changed = true;
         } else {
            if (!global) {
               added_non_static = true; /* triggers clearing of unknown state */
            }
         }
      } else {
         if (is_static) {
            const char *oldval = lGetString(lep, HL_value);
            if (sge_strnullcmp(oldval, value) != 0) {
               statics_changed = true;

               DPRINTF(("%s: updating STATIC lv: "SFQ" = "SFQ" oldval: "SFQ"\n", 
                       lGetHost(ep, LR_host), name, value, oldval));
            }
         }
      }
      /* copy value */
      lSetString(lep, HL_value, value); 
      lSetUlong(lep, HL_last_update, now);
   }

   /* output error from previous host, if any */
   if (report_host) {
      INFO((SGE_EVENT, MSG_CANT_ASSOCIATE_LOAD_SS, rhost, report_host));
   }

   /* if non static load values arrived, this indicates that 
   ** host is known 
   */
   if (added_non_static) {
      const char* tmp_hostname;

      tmp_hostname = lGetHost(host_ep, EH_name);
      cqueue_list_set_unknown_state(
         *(object_type_get_master_list(SGE_TYPE_CQUEUE)),
         tmp_hostname, true, false);
   }

   if (global_ep) {
      lList *answer_list = NULL;
      sge_event_spool(ctx, &answer_list, 0, sgeE_EXECHOST_MOD, 
                      0, 0, SGE_GLOBAL_NAME, NULL, NULL,
                      global_ep, NULL, NULL, true, false);
      answer_list_output(&answer_list);
      reporting_create_host_record(&answer_list, global_ep, now);
      answer_list_output(&answer_list);
   }

   /*
   ** if static load values (eg arch) have changed
   ** then spool
   */
   if (host_ep) {
      lList *answer_list = NULL;
      sge_event_spool(ctx, &answer_list, 0, sgeE_EXECHOST_MOD, 
                      0, 0, lGetHost(host_ep, EH_name), NULL, NULL,
                      host_ep, NULL, NULL, true, statics_changed);
      answer_list_output(&answer_list);
      reporting_create_host_record(&answer_list, host_ep, now);
      answer_list_output(&answer_list);
   }

   DEXIT;
   return;
}

/* ----------------------------------------

   trash old load values 
   
*/
void sge_load_value_cleanup_handler(sge_gdi_ctx_class_t *ctx, te_event_t anEvent, monitoring_t *monitor)
{
   lListElem *hep, *ep, *nextep; 
   lList *h_list;
   const char *host;
   int host_unheard;
   u_short id = 1;
   u_long32 timeout; 
   int nstatics, nbefore;
   lListElem *global_host_elem   = NULL;
   lListElem *template_host_elem = NULL;
   time_t now = time(NULL);
   unsigned long last_heard_from;
   lList *master_exechost_list = *object_type_get_master_list(SGE_TYPE_EXECHOST);
   const void *iterator = NULL;

   DENTER(TOP_LAYER, "sge_load_value_cleanup_handler");

   MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), monitor);

   /* get "global" element pointer */
   global_host_elem   = host_list_locate(master_exechost_list, SGE_GLOBAL_NAME);    
   /* get "template" element pointer */
   template_host_elem = host_list_locate(master_exechost_list, SGE_TEMPLATE_NAME); 
   /* take each host including the "global" host */
   for_each(hep, master_exechost_list) {   
      if (hep == template_host_elem) {
         continue;
      }   

      host = lGetHost(hep, EH_name);

      /* do not trash load values of simulated hosts */
      if(mconf_get_simulate_hosts()) {
         const lListElem *simhost = lGetSubStr(hep, CE_name, "simhost", EH_consumable_config_list);
         if(simhost != NULL) {
            const char *real_host = lGetString(simhost, CE_stringval);
            if(real_host != NULL && sge_hostcmp(real_host, host) != 0) {
               DPRINTF(("skip trashing load values for host %s simulated by %s\n", host, real_host));
               continue;
            }
         }
      }

      timeout = MAX(load_report_interval(hep)*3, mconf_get_max_unheard()); 
      if ( hep != global_host_elem) {
         cl_commlib_get_last_message_time((cl_com_get_handle(prognames[QMASTER],0)),
                                        (char*)host, (char*)prognames[EXECD], id, &last_heard_from);
      }
      if ( (hep != global_host_elem )  && (now > last_heard_from + timeout)) {
         host_unheard = 1;
#if 0
         DPRINTF(("id = %d, comproc = %s, host = %s, timeout = "sge_u32", "
               "now = "sge_u32", last_heard = "sge_u32"\n", id, prognames[EXECD], host, 
               sge_u32c(timeout), sge_u32c(now), 
               sge_u32c(last_heard_from(prognames[EXECD], &id, host))));
#endif
      } else {
         host_unheard = 0;
      }

      /* take each load value */
      nstatics = 0;
      nbefore = lGetNumberOfElem(lGetList(hep, EH_load_list));     

      ep=lFirst(lGetList(hep, EH_load_list));
      while (ep) {
         nextep=lNext(ep);

         if (sge_is_static_load_value(lGetString(ep, HL_name))) {
            nstatics++;
         } else {
            if ((lGetUlong(ep, HL_last_update)+timeout<now || host_unheard)) {
               DPRINTF(("host %s: trashing load value "SFQ": %s\n", 
                        host, 
                        lGetString(ep, HL_name),
                        (lGetUlong(ep, HL_last_update)+timeout < now)? "HL_last_update":"last_heard_from"));
              lDelSubStr(hep, HL_name, lGetString(ep, HL_name), EH_load_list);
            }  
         }
         ep = nextep;
      }
      h_list   = lGetList(hep, EH_load_list);
      if ( (nstatics == lGetNumberOfElem(h_list)) &&
           (nbefore   > lGetNumberOfElem(h_list))    ) {
         lListElem *cqueue;

         /* load reports were trashed and only the 
            static load values remained: 
            set all queues residing at this host in unknown state */
         for_each(cqueue, *(object_type_get_master_list(SGE_TYPE_CQUEUE))) {
            lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
            lListElem *qinstance;

            qinstance = lGetElemHostFirst(qinstance_list, QU_qhostname, 
                                          host, &iterator);
            while (qinstance != NULL) {
               sge_qmaster_qinstance_state_set_unknown(qinstance, true);
               qinstance_add_event(qinstance, sgeE_QINSTANCE_MOD);

               DPRINTF(("%s: trashed all (%d) non-static load values -> unknown\n", 
                        lGetString(qinstance, QU_qname), 
                        nbefore - lGetNumberOfElem(lGetList(hep, EH_load_list))));

               /* initiate timer for this host because they turn into 'unknown' state */
               reschedule_unknown_trigger(hep); 
               qinstance = lGetElemHostNext(qinstance_list, QU_qhostname, 
                                            host, &iterator); 
            }
         }
      } 
   }

   mconf_set_new_config(false);

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);

   DRETURN_VOID;
}

u_long32 load_report_interval(lListElem *hep)
{
   u_long32 timeout; 
   const char *host;

   DENTER(TOP_LAYER, "load_report_interval");

   host = lGetHost(hep, EH_name);

   /* cache load report interval in exec host to prevent host name resolving each epoch */
   if (mconf_is_new_config() || !lGetUlong(hep, EH_load_report_interval))
   {
      lListElem *conf_entry = NULL;
      
      if ((conf_entry = sge_get_configuration_entry_by_name(host, "load_report_time")) != NULL)
      {
         if (parse_ulong_val(NULL, &timeout, TYPE_TIM, lGetString(conf_entry, CF_value), NULL, 0) == 0)
         {
            ERROR((SGE_EVENT, MSG_OBJ_LOADREPORTIVAL_SS, host, lGetString(conf_entry, CF_value)));
            timeout = 120;
         }
         
         lFreeElem(&conf_entry);
      }
   
      DPRINTF(("%s: load value timeout for host %s is "sge_u32"\n", SGE_FUNC, host, timeout));
      
      lSetUlong(hep, EH_load_report_interval, timeout);
   }
   else
   { 
      timeout = lGetUlong(hep, EH_load_report_interval);
   }
    
   DEXIT; 
   return timeout;
}


u_long32 sge_get_max_unheard_value(void) 
{
   lListElem *conf_entry = NULL;
   u_long32 max_unheard_secs = 0;
   
   DENTER(TOP_LAYER, "sge_get_max_unheard_value");

   if ((conf_entry = sge_get_configuration_entry_by_name(SGE_GLOBAL_NAME, "max_unheard")) != NULL)
   {
      if (parse_ulong_val(NULL, &max_unheard_secs, TYPE_TIM, lGetString(conf_entry, CF_value), NULL, 0) == 0)
      {
         ERROR((SGE_EVENT, MSG_OBJ_MAXUNHEARDVALUE_SS, SGE_GLOBAL_NAME, lGetString(conf_entry, CF_value)));
         max_unheard_secs = 300;
      }
      
      lFreeElem(&conf_entry);
   }
      
   DPRINTF(("%s: max unheard value is "sge_u32"\n", SGE_FUNC, max_unheard_secs)); 
   
   DEXIT; 
   return max_unheard_secs;
}

void sge_change_queue_version_exechost(sge_gdi_ctx_class_t *ctx, const char *exechost_name) 
{
   lListElem *cqueue = NULL; 
   bool change_all = (strcasecmp(exechost_name, SGE_GLOBAL_NAME) == 0) ? true : false;

   DENTER(TOP_LAYER, "sge_change_queue_version_exechost");

   for_each(cqueue, *(object_type_get_master_list(SGE_TYPE_CQUEUE))) {
      lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
      lListElem *qinstance = NULL;
      lListElem *next_qinstance = NULL;
      const void *iterator = NULL;

      if (change_all) {
         next_qinstance = lFirst(qinstance_list);
      } else {
         next_qinstance = lGetElemHostFirst(qinstance_list, QU_qhostname, 
                                            exechost_name, &iterator);
      }
      while ((qinstance = next_qinstance)) {
         const char *name = NULL;
         lList *answer_list = NULL;

         if (change_all) {
            next_qinstance = lNext(qinstance);
            name = SGE_GLOBAL_NAME;
         } else {
            next_qinstance = lGetElemHostNext(qinstance_list, QU_qhostname, 
                                              exechost_name, &iterator); 
            name = exechost_name;
         }
         DPRINTF((SFQ" has changed. Increasing qversion of"SFQ"\n",
                  name, lGetString(qinstance, QU_full_name)));
         qinstance_increase_qversion(qinstance);
         sge_event_spool(ctx, &answer_list, 0, sgeE_QINSTANCE_MOD, 
                         0, 0, lGetString(qinstance, QU_qname), 
                         lGetHost(qinstance, QU_qhostname), NULL,
                         qinstance, NULL, NULL, true, false);
         answer_list_output(&answer_list);
      }
   }

   DEXIT;
   return;
}

/****
 **** sge_gdi_kill_exechost
 ****
 **** prepares the killing of an exechost (or all).
 **** Actutally only the permission is checked here
 **** and master_kill_execds is called to do the work.
 ****/
void sge_gdi_kill_exechost(sge_gdi_ctx_class_t *ctx, char *host, sge_gdi_request *request, sge_gdi_request *answer,
                           uid_t uid, gid_t gid, char *user, char *group)
{

   DENTER(GDI_LAYER, "sge_gdi_kill_exechost");

   if (!manop_is_manager(user)) {
      ERROR((SGE_EVENT, MSG_OBJ_SHUTDOWNPERMS)); 
      answer_list_add(&(answer->alp), SGE_EVENT, STATUS_ENOMGR, 
                      ANSWER_QUALITY_ERROR);
      DEXIT;
      return;
   }

   master_kill_execds(ctx, request, answer);
   DEXIT;
}

/******************************************************************
   We have to tell execd(s) to terminate. 

   request->lp is a lList of Type ID_Type.
      If the first ID_str is NULL, we have to kill all execd's.
      Otherwise, every ID_str describes an execd to kill.
      If ID_force is 1, we have to kill jobs, too.
      If ID_force is 0, we don't kill jobs.
 *******************************************************************/
static void master_kill_execds(
sge_gdi_ctx_class_t *ctx,
sge_gdi_request *request,
sge_gdi_request *answer 
) {
   int kill_jobs;
   lListElem *lel, *rep;
   char host[CL_MAXHOSTLEN];
   const char *hostname;

   DENTER(TOP_LAYER, "master_kill_execds");

   if (lGetString(lFirst(request->lp), ID_str) == NULL) {
      /* this means, we have to kill every execd. */

      kill_jobs = lGetUlong(lFirst(request->lp), ID_force)?1:0;
      /* walk over exechost list and send every exechosts execd a
         notification */
      for_each(lel, *object_type_get_master_list(SGE_TYPE_EXECHOST)) {  
         hostname = lGetHost(lel, EH_name);
         if (strcmp(hostname, SGE_TEMPLATE_NAME) && strcmp(hostname, SGE_GLOBAL_NAME )) {
            notify(ctx, lel, answer, kill_jobs, 0); 

            /* RU: */
            /* initiate timer for this host which turns into unknown state */
            reschedule_unknown_trigger(lel);
         }
      }
      if(lGetNumberOfElem(answer->alp) == 0) {
         /* no exechosts have been killed */
         DPRINTF((MSG_SGETEXT_NOEXECHOSTS));
         INFO((SGE_EVENT, MSG_SGETEXT_NOEXECHOSTS));
         answer_list_add(&(answer->alp), SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
      }
   } else {
      /* only specified exechosts should be killed. */
      
      /* walk over list with execd's to kill */
      for_each(rep, request->lp) {
         if ((getuniquehostname(lGetString(rep, ID_str), host, 0)) != CL_RETVAL_OK)
         {
            WARNING((SGE_EVENT, MSG_SGETEXT_CANTRESOLVEHOST_S, lGetString(rep, ID_str)));
            answer_list_add(&(answer->alp), SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
         } else {
            if ((lel = host_list_locate(*object_type_get_master_list(SGE_TYPE_EXECHOST), host))) {
               kill_jobs = lGetUlong(rep, ID_force)?1:0;
               /*
               ** if a host name is given, then a kill is forced
               ** this means that even if the host is unheard we try
               ** to kill it
               */
               notify(ctx, lel, answer, kill_jobs, 1);
               /* RU: */
               /* initiate timer for this host which turns into unknown state */ 
               reschedule_unknown_trigger(lel);
            } else {
               WARNING((SGE_EVENT, MSG_SGETEXT_ISNOEXECHOST_S, host));
               answer_list_add(&(answer->alp), SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING);
            }
         }
      }
   }
   DEXIT;
}


/********************************************************************
 Notify execd on a host to shutdown
 ********************************************************************/
static void notify(
sge_gdi_ctx_class_t *ctx,
lListElem *lel,
sge_gdi_request *answer,
int kill_jobs,
int force 
) {
   const char *hostname;
   u_long execd_alive;
   const char *action_str;
   u_long32 state;
   lListElem *jep;
   lList *mail_users, *gdil;
   int mail_options;
   char sge_mail_subj[1024];
   char sge_mail_body[1024];
   unsigned long last_heard_from;
   bool job_spooling = ctx->get_job_spooling(ctx);

   DENTER(TOP_LAYER, "notify");

   action_str = kill_jobs ? MSG_NOTIFY_SHUTDOWNANDKILL:MSG_NOTIFY_SHUTDOWN;

   hostname = lGetHost(lel, EH_name);

   cl_commlib_get_last_message_time((cl_com_get_handle(prognames[QMASTER], 0)),
                                        (char*)hostname, (char*)prognames[EXECD],1, &last_heard_from);
   execd_alive = last_heard_from;

   if (!force && !execd_alive) {
      WARNING((SGE_EVENT, MSG_OBJ_NOEXECDONHOST_S, hostname));
      answer_list_add(&(answer->alp), SGE_EVENT, STATUS_ESEMANTIC, 
                     ANSWER_QUALITY_WARNING);
   }
   if (execd_alive || force) {
      if (host_notify_about_kill(ctx, lel, kill_jobs)) {
         INFO((SGE_EVENT, MSG_COM_NONOTIFICATION_SSS, action_str, 
               (execd_alive ? "" : MSG_OBJ_UNKNOWN), hostname));
      } else {
         INFO((SGE_EVENT, MSG_COM_NOTIFICATION_SSS, action_str, 
               (execd_alive ? "" : MSG_OBJ_UNKNOWN), hostname));
      }
      DPRINTF((SGE_EVENT));
      answer_list_add(&(answer->alp), SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }

   if(kill_jobs) {
      /* mark killed jobs as deleted */
      for_each(jep, *(object_type_get_master_list(SGE_TYPE_JOB))) {   
         lListElem *jatep;
         mail_users = NULL;
         mail_options = 0;

         for_each(jatep, lGetList(jep, JB_ja_tasks)) {
            gdil = lGetList(jatep, JAT_granted_destin_identifier_list);
            if(gdil) {
               if(!(sge_hostcmp(lGetHost(lFirst(gdil), JG_qhostname), hostname))) {
                  /*   send mail to users if requested                  */
                  if (mail_users == NULL) {
                     mail_users = lGetList(jep, JB_mail_list);
                  }
                  if (mail_options == 0) {
                     mail_options = lGetUlong(jep, JB_mail_options);
                  }
                  
                  if (VALID(MAIL_AT_ABORT, mail_options) && !(lGetUlong(jatep, JAT_state) & JDELETED)) {
                     lUlong jep_JB_job_number;
                     const char* jep_JB_job_name;

                     jep_JB_job_number = lGetUlong(jep, JB_job_number);
                     jep_JB_job_name   = lGetString(jep, JB_job_name);

                     sprintf(sge_mail_subj, MSG_MAIL_JOBKILLEDSUBJ_US, 
                             sge_u32c(jep_JB_job_number), 
                             jep_JB_job_name);
                     sprintf(sge_mail_body, MSG_MAIL_JOBKILLEDBODY_USS, 
                             sge_u32c(jep_JB_job_number), 
                             jep_JB_job_name, 
                             hostname);
                     cull_mail(QMASTER, mail_users, sge_mail_subj, sge_mail_body, "job abortion");
                  }
    
                  /* this job has the killed exechost as master host */
                  state = lGetUlong(jatep, JAT_state);
                  SETBIT(JDELETED, state);
                  lSetUlong(jatep, JAT_state, state);
                  /* spool job */
                  {
                     lList *answer_list = NULL;
                     dstring buffer = DSTRING_INIT;
                     spool_write_object(&answer_list, 
                                        spool_get_default_context(), jep,
                                        job_get_key(lGetUlong(jep, JB_job_number), 
                                            lGetUlong(jatep, JAT_task_number), 
                                            NULL, &buffer), 
                                        SGE_TYPE_JOB,
                                        job_spooling);
                     lListElem_clear_changed_info(jatep);
                     /* JG: TODO: don't we have to send an event? */
                     answer_list_output(&answer_list);
                     sge_dstring_free(&buffer);
                  }
               }
            }
         }
      }
   }

   sge_mark_unheard(lel, NULL); /* for both execd */

   DEXIT;
   return;
}


/****
 **** sge_execd_startedup
 ****
 **** gdi call for old request starting_up.
 ****/
int sge_execd_startedup(
sge_gdi_ctx_class_t *ctx,
lListElem *host,
lList **alpp,
char *ruser,
char *rhost,
u_long32 target, monitoring_t *monitor) {
   lListElem *hep, *cqueue;
   dstring ds;
   char buffer[256];

   DENTER(TOP_LAYER, "sge_execd_startedup");

   sge_dstring_init(&ds, buffer, sizeof(buffer));

   if( !host || !ruser || !rhost) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }
   
   hep = host_list_locate(*object_type_get_master_list(SGE_TYPE_EXECHOST), rhost);
   if(!hep) {
      if (sge_add_host_of_type(ctx, rhost, SGE_EXECHOST_LIST, monitor) < 0) {
         ERROR((SGE_EVENT, MSG_OBJ_INVALIDHOST_S, rhost));
         answer_list_add(alpp, SGE_EVENT, STATUS_DENIED, ANSWER_QUALITY_ERROR);
         DEXIT;
         return STATUS_DENIED;
      } 
   }

   hep = host_list_locate(*object_type_get_master_list(SGE_TYPE_EXECHOST), rhost);
   if(!hep) {
      ERROR((SGE_EVENT, MSG_OBJ_NOADDHOST_S, rhost));
      answer_list_add(alpp, SGE_EVENT, STATUS_DENIED, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_DENIED;
   }
   lSetUlong(hep, EH_featureset_id, lGetUlong(host, EH_featureset_id));
   lSetUlong(hep, EH_report_seqno, 0);

   /*
    * reinit state of all qinstances at this host according to initial_state
    */
   for_each (cqueue, *(object_type_get_master_list(SGE_TYPE_CQUEUE))) {
      lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
      lListElem *qinstance = NULL;
      const void *iterator = NULL;

      qinstance = lGetElemHostFirst(qinstance_list, QU_qhostname, 
                                    rhost, &iterator);
      while (qinstance != NULL) {
         bool state_changed = sge_qmaster_qinstance_set_initial_state(qinstance);

         if (state_changed) {
            lList *answer_list = NULL;

            qinstance_increase_qversion(qinstance);
            sge_event_spool(ctx, &answer_list, 0, sgeE_QINSTANCE_MOD, 
                            0, 0, lGetString(qinstance, QU_qname), 
                            lGetHost(qinstance, QU_qhostname), NULL,
                            qinstance, NULL, NULL, true, true);
            answer_list_output(&answer_list); 
         }
         qinstance = lGetElemHostNext(qinstance_list, QU_qhostname,
                                      rhost, &iterator);
      }
   }
   
   DPRINTF(("=====>STARTING_UP: %s %s on >%s< is starting up\n", 
      feature_get_product_name(FS_SHORT_VERSION, &ds), "execd", rhost));

   /*
   ** loop over pseudo hosts and set EH_startup flag
   */
   lSetUlong(hep, EH_startup, 1);
   sge_add_event( 0, sgeE_EXECHOST_MOD, 0, 0, rhost, NULL, NULL, hep);
   lListElem_clear_changed_info(hep);

   INFO((SGE_EVENT, MSG_LOG_REGISTER_SS, "execd", rhost));
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_ERROR);

   DEXIT;
   return STATUS_OK;
}


static int verify_scaling_list(lList **answer_list, lListElem *host) 
{
   bool ret = true;
   lListElem *hs_elem = NULL;
   lList *master_centry_list = *object_type_get_master_list(SGE_TYPE_CENTRY);

   DENTER(TOP_LAYER, "verify_scaling_list");
   for_each (hs_elem, lGetList(host, EH_scaling_list)) {
      const char *name = lGetString(hs_elem, HS_name);
      lListElem *centry = centry_list_locate(master_centry_list, name);
   
      if (centry == NULL) {
         const char *hname = lGetHost(host, EH_name);

         ERROR((SGE_EVENT, MSG_OBJ_NOSCALING4HOST_SS, name, hname));
         answer_list_add(answer_list, SGE_EVENT, 
                         STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = false;
         break;
      }
   }
   DEXIT;
   return ret ? STATUS_OK : STATUS_EUNKNOWN;
}

/****** sge_host_qmaster/host_diff_sublist() ***********************************
*  NAME
*     host_diff_sublist() -- Diff exechost sublists
*
*  SYNOPSIS
*     static void host_diff_sublist(const lListElem *new, const lListElem *old,
*     int snm1, int snm2, int key_nm, const lDescr *dp, lList **new_sublist,
*     lList **old_sublist)
*
*  FUNCTION
*     Makes a diff userset/project sublists of an exec host.
*
*  INPUTS
*     const lListElem *new - New exec host (EH_Type)
*     const lListElem *old - Pld exec host (EH_Type)
*     int snm1             - First exec host sublist field
*     int snm2             - Second exec host sublist field
*     int key_nm           - Field with key in sublist
*     const lDescr *dp     - Type for outgoing sublist arguments
*     lList **new_sublist  - List of new references
*     lList **old_sublist  - List of old references
*
*  NOTES
*     MT-NOTE: host_diff_sublist() is MT safe
*******************************************************************************/
static void host_diff_sublist(const lListElem *new, const lListElem *old,
      int snm1, int snm2, int key_nm, const lDescr *dp,
      lList **new_sublist, lList **old_sublist)
{
   const lListElem *ep;
   const char *p;

   /* collect 'old' entries in 'old_sublist' */
   if (old && old_sublist) {
      for_each (ep, lGetList(old, snm1)) {
         p = lGetString(ep, key_nm);
         if (!lGetElemStr(*old_sublist, key_nm, p))
            lAddElemStr(old_sublist, key_nm, p, dp);
      }
      for_each (ep, lGetList(old, snm2)) {
         p = lGetString(ep, key_nm);
         if (!lGetElemStr(*old_sublist, key_nm, p))
            lAddElemStr(old_sublist, key_nm, p, dp);
      }
   }

   /* collect 'new' entries in 'new_sublist' */
   if (new && new_sublist) {
      for_each (ep, lGetList(new, snm1)) {
         p = lGetString(ep, key_nm);
         if (!lGetElemStr(*new_sublist, key_nm, p))
            lAddElemStr(new_sublist, key_nm, p, dp);
      }
      for_each (ep, lGetList(new, snm2)) {
         p = lGetString(ep, key_nm);
         if (!lGetElemStr(*new_sublist, key_nm, p))
            lAddElemStr(new_sublist, key_nm, p, dp);
      }
   }

   return;
}


/****** sge_host_qmaster/host_diff_projects() **********************************
*  NAME
*     host_diff_projects() -- Diff old/new exec host projects
*
*  SYNOPSIS
*     void host_diff_projects(const lListElem *new, const lListElem *old, lList
*     **new_prj, lList **old_prj)
*
*  FUNCTION
*     A diff new/old is made regarding exec host projects/xprojects.
*     Project references are returned in new_prj/old_prj.
*
*  INPUTS
*     const lListElem *new - New exec host (EH_Type)
*     const lListElem *old - Old exec host (EH_Type)
*     lList **new_prj      - New project references (US_Type)
*     lList **old_prj      - Old project references (US_Type)
*
*  NOTES
*     MT-NOTE: host_diff_projects() is not MT safe
*******************************************************************************/
void host_diff_projects(const lListElem *new,
         const lListElem *old, lList **new_prj, lList **old_prj)
{
   host_diff_sublist(new, old, EH_prj, EH_xprj,
         UP_name, UP_Type, new_prj, old_prj);
   lDiffListStr(UP_name, new_prj, old_prj);
}

/****** sge_host_qmaster/host_diff_usersets() **********************************
*  NAME
*     host_diff_usersets() -- Diff old/new exec host usersets
*
*  SYNOPSIS
*     void host_diff_usersets(const lListElem *new, const lListElem *old, lList
*     **new_acl, lList **old_acl)
*
*  FUNCTION
*     A diff new/old is made regarding exec host acl/xacl.
*     Userset references are returned in new_acl/old_acl.
*
*  INPUTS
*     const lListElem *new - New exec host (EH_Type)
*     const lListElem *old - Old exec host (EH_Type)
*     lList **new_acl      - New userset references (US_Type)
*     lList **old_acl      - Old userset references (US_Type)
*
*  NOTES
*     MT-NOTE: host_diff_usersets() is not MT safe
*******************************************************************************/
void host_diff_usersets(const lListElem *new,
      const lListElem *old, lList **new_acl, lList **old_acl)
{
   host_diff_sublist(new, old, EH_acl, EH_xacl,
         US_name, US_Type, new_acl, old_acl);
   lDiffListStr(US_name, new_acl, old_acl);
}



/****** sge_host_qmaster/host_update_categories() ******************************
*  NAME
*     host_update_categories() --  Update categories wrts userset/project
*
*  SYNOPSIS
*     static void host_update_categories(const lListElem *new_hep, const
*     lListElem *old_hep)
*
*  FUNCTION
*     The userset/project information wrts categories is updated based
*     on new/old exec host configuration and events are sent upon
*     changes.
*
*
*  INPUTS
*     const lListElem *new_hep - New exec host (EH_Type)
*     const lListElem *old_hep - Old exec host (EH_Type)
*
*  NOTES
*     MT-NOTE: host_update_categories() is not MT safe
*******************************************************************************/
static void host_update_categories(const lListElem *new_hep, const lListElem *old_hep)
{
   lList *old = NULL, *new = NULL;

   host_diff_projects(new_hep, old_hep, &new, &old);
   project_update_categories(new, old);
   lFreeList(&old);
   lFreeList(&new);

   host_diff_usersets(new_hep, old_hep, &new, &old);
   userset_update_categories(new, old);
   lFreeList(&old);
   lFreeList(&new);
}

/****** sge_utility_qmaster/attr_mod_threshold() *******************************
*  NAME
*     attr_mod_threshold() -- modify the threshold configuration sublist 
*
*  SYNOPSIS
*     int attr_mod_threshold(lList **alpp, lListElem *qep, lListElem *new_ep, 
*     int sub_command, char *attr_name, char 
*     *object_name) 
*
*  FUNCTION
*   Validation tries to find each element of the qep element in the threshold identified by nm.
*   Elements which already existst here are copied into sublist of new_ep.   
*
*  INPUTS
*     sge_gdi_ctx_class_t *ctx  - gdi context
*     lList **alpp              - The answer list 
*     lListElem *qep            - The source object element 
*     lListElem *new_ep         - The target object element 
*     int sub_command           - The add, modify, remove command 
*     const char *attr_name     - The attribute name 
*     const char *object_name   - The target object name
*
*  RESULT
*     int - 0 if success
*
*  NOTES
*     MT-NOTE: attr_mod_threshold() is MT safe 
*
*******************************************************************************/
static int attr_mod_threshold(sge_gdi_ctx_class_t *ctx, lList **alpp, lListElem *qep, lListElem *new_ep,
                              int sub_command, char *attr_name, char *object_name) {
   int ret;

   DENTER(TOP_LAYER, "attr_mod_threshold");

   /* ---- attribute EH_consumable_config_list */
   if (lGetPosViaElem(qep, EH_consumable_config_list, SGE_NO_ABORT)>=0) {
      lListElem *tmp_elem = NULL;

      DPRINTF(("got new %s\n", attr_name));

      if (ensure_attrib_available(alpp, qep, EH_consumable_config_list)) {
         DRETURN(STATUS_EUNKNOWN);
      }

      tmp_elem = lCopyElem(new_ep);

      /* the attr_mod_sub_list return boolean and there is stored in the int value, attention true=1 */
      ret = attr_mod_sub_list(alpp, tmp_elem, EH_consumable_config_list, CE_name, qep,
                              sub_command, attr_name, object_name, 0);
      if (!ret) {
         lFreeElem(&tmp_elem);
         DRETURN(STATUS_EUNKNOWN);
      }

      /* the centry_list_fill_request returns 0 if success */
      ret = centry_list_fill_request(lGetList(tmp_elem, EH_consumable_config_list), alpp,
                                     *centry_list_get_master_list(), true, false, false);
      if (ret) {
         lFreeElem(&tmp_elem);
         DRETURN(STATUS_EUNKNOWN);
      }
      {
         lListElem *jep = NULL;
         lListElem *ar_ep = NULL;
         const char *host = lGetHost(tmp_elem, EH_name);
         int slots;
         int global_host = !strcmp(SGE_GLOBAL_NAME, host);
         lList *master_centry_list = *object_type_get_master_list(SGE_TYPE_CENTRY);

         lSetList(tmp_elem, EH_resource_utilization, NULL);
         debit_host_consumable(NULL, tmp_elem, master_centry_list, 0);
         for_each (jep, *(object_type_get_master_list(SGE_TYPE_JOB))) {
            lListElem *jatep = NULL;

            slots = 0;
            for_each (jatep, lGetList(jep, JB_ja_tasks)) {
               slots += nslots_granted(lGetList(jatep, JAT_granted_destin_identifier_list),
                  global_host?NULL:host);
            }
            if (slots)
               debit_host_consumable(jep, tmp_elem, master_centry_list, slots);
         }

         for_each(ar_ep, *object_type_get_master_list(SGE_TYPE_AR)) {
            lListElem *gdil_ep = lGetSubHost(ar_ep, JG_qhostname, host, AR_granted_slots);

            if (gdil_ep != NULL) {
               lListElem *dummy_job = lCreateElem(JB_Type);

               lSetList(dummy_job, JB_hard_resource_list, lCopyList("", lGetList(ar_ep, AR_resource_list)));

               rc_add_job_utilization(dummy_job, 0, SCHEDULING_RECORD_ENTRY_TYPE_RESERVING,
                                      tmp_elem, master_centry_list, lGetUlong(gdil_ep, JG_slots),
                                      EH_consumable_config_list, EH_resource_utilization, host,
                                      lGetUlong(ar_ep, AR_start_time), lGetUlong(ar_ep, AR_duration),
                                      HOST_TAG, false);
               lFreeElem(&dummy_job);
            }
         }
      }

      if (ar_list_has_reservation_due_to_host_complex_attr(*object_type_get_master_list(SGE_TYPE_AR), alpp,
                                                           tmp_elem, *object_type_get_master_list(SGE_TYPE_CENTRY))) {
         lFreeElem(&tmp_elem);
         DRETURN(STATUS_EUNKNOWN);
      }

      /* copy back the consumable config and resource utilization lists to new exec host object */
      {
         lList *t = NULL;
         lXchgList(tmp_elem, EH_consumable_config_list, &t);
         lXchgList(new_ep, EH_consumable_config_list, &t);
         lXchgList(tmp_elem, EH_consumable_config_list, &t);

         t = NULL;
         lXchgList(tmp_elem, EH_resource_utilization, &t);
         lXchgList(new_ep, EH_resource_utilization, &t);
         lXchgList(tmp_elem, EH_resource_utilization, &t);
      }

      lFreeElem(&tmp_elem);
   }

   DRETURN(0);
}





