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
#include <sys/types.h>

#include "sge.h"
#include "sge_log.h"
#include "sgermon.h"
#include "sge_pe.h"
#include "sge_event_master.h"
#include "sge_userset.h"
#include "sge_userset_qmaster.h"
#include "sge_feature.h"
#include "sge_conf.h"
#include "valid_queue_user.h"
#include "sge_unistd.h"
#include "sge_answer.h"
#include "sge_qinstance.h"
#include "sge_job.h"
#include "sge_userprj.h"
#include "sge_host.h"
#include "sge_utility.h"
#include "sge_cqueue.h"
#include "sge_attrL.h"
#include "sgeobj/sge_resource_quota.h"
#include "sge_resource_quota_qmaster.h"

#include "sge_persistence_qmaster.h"
#include "spool/sge_spooling.h"

#include "msg_common.h"
#include "msg_qmaster.h"

static void sge_change_queue_version_acl(sge_gdi_ctx_class_t *ctx, const char *acl_name);
static lList* do_depts_conflict(lListElem *new, lListElem *old);
static int verify_userset_deletion(lList **alpp, const char *userset_name);
static int dept_is_valid_defaultdepartment(lListElem *dept, lList **answer_list);
static int acl_is_valid_acl(lListElem *acl, lList **answer_list);

/*********************************************************************
   sge_add_userset() - Master code
   
   adds an userset list to the global userset_list
 *********************************************************************/
int sge_add_userset(
sge_gdi_ctx_class_t *ctx,
lListElem *ep,
lList **alpp,
lList **userset_list,
char *ruser,
char *rhost 
) {
   const char *userset_name;
   int pos, ret;
   lListElem *found;

   DENTER(TOP_LAYER, "sge_add_userset");

   if ( !ep || !ruser || !rhost ) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   /* ep is no acl element, if ep has no US_name */
   if ((pos = lGetPosViaElem(ep, US_name, SGE_NO_ABORT)) < 0) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_MISSINGCULLFIELD_SS,
            lNm2Str(US_name), SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   userset_name = lGetPosString(ep, pos);
   if (!userset_name) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   /* Name has to be a valid filename without pathchanges, because we use it
      for storing user/project to disk */
   if (verify_str_key(
         alpp, userset_name, MAX_VERIFY_STRING, MSG_OBJ_USERSET,KEY_TABLE) != STATUS_OK) {
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   /* search for userset with this name */
   found = userset_list_locate(*userset_list, userset_name);

   /* no double entries */
   if (found) {
      ERROR((SGE_EVENT, MSG_SGETEXT_ALREADYEXISTS_SS, MSG_OBJ_USERSET, userset_name));
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EEXIST;
   }

   /* ensure userset is at least an ACL (qconf -au ) */
   if (!lGetUlong(ep, US_type)) 
      lSetUlong(ep, US_type, US_ACL);

   /* interpret user/group names */
   ret=userset_validate_entries(ep, alpp, 0);
   if ( ret != STATUS_OK ) {
      DEXIT;
      return ret;
   }

   /*
   ** check for users defined in more than one userset if they
   ** are used as departments
   */
   ret = sge_verify_department_entries(*userset_list, ep, alpp);
   if (ret!=STATUS_OK) {
      DEXIT;
      return ret;
   }

   {
      dstring ds = DSTRING_INIT;
      lListElem *rqs;

      sge_dstring_sprintf(&ds, "@%s", userset_name);

      for_each(rqs, *(object_type_get_master_list(SGE_TYPE_RQS))) {
         if (scope_is_referenced_rqs(rqs, RQR_filter_users, sge_dstring_get_string(&ds))) {
            lSetBool(ep, US_consider_with_categories, true);
            break;
         }
      }
      sge_dstring_free(&ds);
   }

   if (!sge_event_spool(ctx, alpp, 0, sgeE_USERSET_ADD,
                        0, 0, userset_name, NULL, NULL,
                        ep, NULL, NULL, true, true)) {
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   /* update in interal lists */
   if (!*userset_list)
      *userset_list = lCreateList("global userset list", US_Type);
   lAppendElem(*userset_list, lCopyElem(ep));

   INFO((SGE_EVENT, MSG_SGETEXT_ADDEDTOLIST_SSSS,
            ruser, rhost, userset_name, MSG_OBJ_USERSET));
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DEXIT;
   return STATUS_OK;
}

/******************************************************************
   sge_del_userset() - Qmaster code

   deletes an userset list from the global userset_list
 ******************************************************************/
int sge_del_userset(
sge_gdi_ctx_class_t *ctx,
lListElem *ep,
lList **alpp,
lList **userset_list,
char *ruser,
char *rhost 
) {
   lListElem *found;
   int pos, ret;
   const char *userset_name;

   DENTER(TOP_LAYER, "sge_del_userset");

   if ( !ep || !ruser || !rhost ) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   /* ep is no userset element, if ep has no US_name */
   if ((pos = lGetPosViaElem(ep, US_name, SGE_NO_ABORT)) < 0) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_MISSINGCULLFIELD_SS,
            lNm2Str(US_name), SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   userset_name = lGetPosString(ep, pos);
   if (!userset_name) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   /* search for userset with this name and remove it from the list */
   if (!(found = userset_list_locate(*userset_list, userset_name))) {
      ERROR((SGE_EVENT, MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_USERSET, userset_name));
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EEXIST;
   }

   /* ensure there are no (x)acl lists in 
      a queue/pe/project/.. refering to this userset */   
   if ((ret=verify_userset_deletion(alpp, userset_name))!=STATUS_OK) {
      /* answerlist gets filled by verify_userset_deletion() in case of errors */
      DEXIT;
      return ret;
   }

   lRemoveElem(*userset_list, &found);

   sge_event_spool(ctx, alpp, 0, sgeE_USERSET_DEL, 
                   0, 0, userset_name, NULL, NULL,
                   NULL, NULL, NULL, true, true);

   INFO((SGE_EVENT, MSG_SGETEXT_REMOVEDFROMLIST_SSSS,
            ruser, rhost, userset_name, MSG_OBJ_USERSET));
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DEXIT;
   return STATUS_OK;
}

/**************************************************************
   sge_mod_userset() - Qmaster code

   modifies an userset in the global list
 **************************************************************/
int sge_mod_userset(
sge_gdi_ctx_class_t *ctx,
lListElem *ep,
lList **alpp,
lList **userset_list,
char *ruser,
char *rhost 
) {
   const char *userset_name;
   int pos, ret;
   lListElem *found;

   DENTER(TOP_LAYER, "sge_mod_userset");

   if ( !ep || !ruser || !rhost ) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   /* ep is no userset element, if ep has no US_name */
   if ((pos = lGetPosViaElem(ep, US_name, SGE_NO_ABORT)) < 0) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_MISSINGCULLFIELD_SS,
            lNm2Str(US_name), SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   userset_name = lGetPosString(ep, pos);
   if (!userset_name) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   /* no double entries */
   found = userset_list_locate(*userset_list, userset_name);
   if (!found) {
      ERROR((SGE_EVENT, MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_USERSET, userset_name));
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EEXIST;
   }

   /* interpret user/group names */
   ret=userset_validate_entries(ep, alpp, 0);
   if (ret!=STATUS_OK) {
      DEXIT;
      return ret;
   }

   /* make sure acl is valid */
   ret = acl_is_valid_acl(ep, alpp);
   if (ret != STATUS_OK) {
      DEXIT;
      return ret;
   }

   /*
   ** check for users defined in more than one userset if they
   ** are used as departments
   */
   ret = sge_verify_department_entries(*userset_list, ep, alpp);
   if (ret!=STATUS_OK) {
      DEXIT;
      return ret;
   }

   /* delete old userset */
   lRemoveElem(*userset_list, &found);

   /* insert modified userset */
   lAppendElem(*userset_list, lCopyElem(ep));

   {
      dstring ds = DSTRING_INIT;
      lListElem *rqs;

      sge_dstring_sprintf(&ds, "@%s", userset_name);

      for_each(rqs, *(object_type_get_master_list(SGE_TYPE_RQS))) {
         if (scope_is_referenced_rqs(rqs, RQR_filter_users, sge_dstring_get_string(&ds))) {
            lSetBool(ep, US_consider_with_categories, true);
            break;
         }
      }
      sge_dstring_free(&ds);
   }

   /* update on file */
   if (!sge_event_spool(ctx, alpp, 0, sgeE_USERSET_MOD,
                        0, 0, userset_name, NULL, NULL,
                        ep, NULL, NULL, true, true)) {
      DEXIT;
      return STATUS_EDISK;
   }
   /* change queue versions */
   sge_change_queue_version_acl(ctx, userset_name);

   INFO((SGE_EVENT, MSG_SGETEXT_MODIFIEDINLIST_SSSS,
            ruser, rhost, userset_name, MSG_OBJ_USERSET));
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DEXIT;
   return STATUS_OK;
}


/*********************************************************************
   Qmaster code

   increase version of queues using given userset as access list (acl or xacl)

   having changed a complex we have to increase the versions
   of all queues containing this complex;
 **********************************************************************/
static void 
sge_change_queue_version_acl(sge_gdi_ctx_class_t *ctx, const char *acl_name) 
{
   lListElem *cqueue = NULL;

   DENTER(TOP_LAYER, "sge_change_queue_version_acl");

   for_each(cqueue, *(object_type_get_master_list(SGE_TYPE_CQUEUE))) {
      lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
      lListElem *qinstance = NULL;

      for_each(qinstance, qinstance_list) {
         lList *acl_list = lGetList(qinstance, QU_acl);
         lList *xacl_list = lGetList(qinstance, QU_xacl);
         lListElem *acl = lGetElemStr(acl_list, US_name, acl_name);
         lListElem *xacl = lGetElemStr(xacl_list, US_name, acl_name);
         bool is_used = ((acl != NULL) || (xacl != NULL)) ? true : false;

         if (is_used) {
            lList *answer_list = NULL;

            DPRINTF(("increasing version of queue "SFQ" because acl "SFQ
                     " changed\n", lGetString(qinstance, QU_full_name), 
                     acl_name));
            qinstance_increase_qversion(qinstance);
            sge_event_spool(ctx, &answer_list, 0, sgeE_QINSTANCE_MOD, 
                            0, 0, lGetString(qinstance, QU_qname), 
                            lGetHost(qinstance, QU_qhostname), NULL,
                            qinstance, NULL, NULL, true, true);
            answer_list_output(&answer_list);
         }
      }
   }

   DEXIT;
   return;
}

/******************************************************
   sge_verify_department_entries()
      resolves user set/department

   userset_list
      the current master user list (US_Type)
   new_userset
      the new userset element
   alpp
      may be NULL
      is used to build up an answer
      element in case of error

   returns
      STATUS_OK         - on success
      STATUS_ESEMANTIC  - on error
 ******************************************************/
int sge_verify_department_entries(
lList *userset_list,
lListElem *new_userset,
lList **alpp 
) {
   lListElem *up;
   lList *depts;
   lList *answers = NULL;
   lCondition *where = NULL;
   lEnumeration *what = NULL;

   DENTER(TOP_LAYER, "sge_verify_department_entries");

   /*
    * make tests for the defaultdepartment
    */
   if (!strcmp(lGetString(new_userset, US_name), DEFAULT_DEPARTMENT)) {
      if (!dept_is_valid_defaultdepartment(new_userset, alpp)) {
         DEXIT;
         return STATUS_ESEMANTIC;
      }
   }

   if (!(lGetUlong(new_userset, US_type) & US_DEPT)) {
      DEXIT;
      return STATUS_OK;
   }
   
   /*
   ** get the department usersets and only those that have a different
   ** name than the new one.
   */
   what = lWhat("%T(ALL)", US_Type);
   where = lWhere("%T(%I m= %u && %I != %s)", US_Type, US_type, US_DEPT,
                     US_name, lGetString(new_userset, US_name));
   depts = lSelect("Departments", userset_list, where, what);
   lFreeWhere(&where);
   lFreeWhat(&what);

   if (!depts) {
      DEXIT;
      return STATUS_OK;
   }

   /*
   ** Loop over departments and check if a user in the new
   ** element is already contained in another department list.
   ** This requires expanding the group entries.
   */
   for_each(up, depts) {
      answers = do_depts_conflict(new_userset, up); 
      if (answers)
         break;
   }
   
   lFreeList(&depts);
   
   if (answers) {
      *alpp = answers;
      DEXIT;
      return STATUS_ESEMANTIC;
   }

   DEXIT;
   return STATUS_OK;
}

/****** qmaster/dept/dept_is_valid_defaultdepartment() ************************
*  NAME
*     dept_is_valid_defaultdepartment() -- is defaultdepartment correct 
*
*  SYNOPSIS
*     static int dept_is_valid_defaultdepartment(lListElem *dept, 
*                                                lList **answer_list) 
*
*  FUNCTION
*     Check if the given defaultdepartment "dept" is valid. 
*
*  INPUTS
*     lListElem *dept     - US_Type defaultdepartment 
*     lList **answer_list - AN_Type answer list 
*
*  RESULT
*     static int - 0 or 1
*******************************************************************************/
static int dept_is_valid_defaultdepartment(lListElem *dept,
                                           lList **answer_list)
{
   int ret = 1;
   DENTER(TOP_LAYER, "dept_is_valid_defaultdepartment");

   if (dept != NULL) {
      /* test 'type' */
      if (!(lGetUlong(dept, US_type) & US_DEPT)) {
         ERROR((SGE_EVENT, MSG_QMASTER_DEPTFORDEFDEPARTMENT));
         answer_list_add(answer_list, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         ret = 0;
      }
      /* test user list */
      if (lGetNumberOfElem(lGetList(dept, US_entries)) > 0 ) {
         ERROR((SGE_EVENT, MSG_QMASTER_AUTODEFDEPARTMENT));
         answer_list_add(answer_list, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         ret = 0;
      }
   }
   DEXIT;
   return ret;
}

/****** qmaster/acl/acl_is_valid_acl() ****************************************
*  NAME
*     acl_is_valid_acl() -- is the acl correct 
*
*  SYNOPSIS
*     static int acl_is_valid_acl(lListElem *acl, lList **answer_list) 
*
*  FUNCTION
*     Check if the given "acl" is correct 
*
*  INPUTS
*     lListElem *acl      - US_Type acl 
*     lList **answer_list - AN_Type list 
*
*  RESULT
*     static int - 0 or 1
*******************************************************************************/
static int acl_is_valid_acl(lListElem *acl,
                            lList **answer_list)
{
   int ret = 1;
   DENTER(TOP_LAYER, "acl_is_valid_acl");

   if (acl != NULL) {
      if (!(lGetUlong(acl, US_type) & US_DEPT)) {
         if (lGetUlong(acl, US_fshare) > 0) {
            ERROR((SGE_EVENT, MSG_QMASTER_ACLNOSHARE));
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            ret = 0;
         }
         if (lGetUlong(acl, US_oticket) > 0) {
            ERROR((SGE_EVENT, MSG_QMASTER_ACLNOTICKET));
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            ret = 0;
         }
      }
   }
   DEXIT;
   return ret;
}

static lList* do_depts_conflict(
lListElem *new,
lListElem *old 
) {
   lList *new_users = NULL;
   lList *old_users = NULL; 
   lListElem *np;
   lList *alp = NULL;
   const char *nname;
   
   DENTER(TOP_LAYER, "do_depts_conflict");
   
   new_users = lGetList(new, US_entries);
   old_users = lGetList(old, US_entries);

   if (!old_users || !new_users) {
      DEXIT;
      return NULL;
   }
   
   /*
   ** groups are encoded with the first letter @, e.g. @sge
   */
   for_each(np, new_users) {
      nname = lGetString(np, UE_name);
      if (nname && nname[0] == '@') { 
         if (sge_contained_in_access_list(NULL, &nname[1], old, &alp)) {
            DEXIT;
            return alp;
         }
      }   
      else {
         if (sge_contained_in_access_list(nname, NULL, old, &alp)) {
            DEXIT;
            return alp;
         }
      }   
   }      

   DEXIT;
   return NULL;
}

/* 

   return
      0   no matching department found
      1   set department
*/
int set_department(
lList **alpp,
lListElem *job,
lList *userset_list 
) {
   lListElem *dep;
   const char *owner, *group; 

   DENTER(TOP_LAYER, "set_department");

   /* first try to find a department searching the user name directly
      in a department */
   owner = lGetString(job, JB_owner);
   for_each (dep, userset_list) {
      /* use only departments */
      if (!(lGetUlong(dep, US_type) & US_DEPT)) 
         continue;

      if (sge_contained_in_access_list(owner, NULL, dep, NULL)) {
         lSetString(job, JB_department, lGetString(dep, US_name));   
         DPRINTF(("user %s got department "SFQ"\n", 
            owner, lGetString(dep, US_name)));

         DEXIT;
         return 1;
      }
   }

   /* the user does not appear in any department - now try to find
      our group in the department */
   group = lGetString(job, JB_group);
   for_each (dep, userset_list) {
      /* use only departments */ 
      if (!(lGetUlong(dep, US_type) & US_DEPT))
         continue;

      if (sge_contained_in_access_list(NULL, group, dep, NULL)) {
         lSetString(job, JB_department, lGetString(dep, US_name));   
         DPRINTF(("user %s got department \"%s\"\n", owner, lGetString(dep, US_name)));

         DEXIT;
         return 1;
      }
   }   
   

   /*
   ** attach default department if present
   ** if job has no department we reach this
   */
   if(lGetElemStr(userset_list, US_name, DEFAULT_DEPARTMENT)) {
      lSetString(job, JB_department, DEFAULT_DEPARTMENT);
      DPRINTF(("user %s got department "SFQ"\n", owner, DEFAULT_DEPARTMENT));
      DEXIT;
      return 1;
   }
   
   ERROR((SGE_EVENT, MSG_SGETEXT_NO_DEPARTMENT4USER_SS, owner, group));
   answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);

   DEXIT;
   return 0;
}


static int verify_userset_deletion(
lList **alpp,
const char *userset_name 
) {
   int ret = STATUS_OK;
   lListElem *ep = NULL;
   lListElem *cqueue = NULL;
   lList* user_lists = NULL;
   object_description *object_base = object_type_get_object_description();

   DENTER(TOP_LAYER, "verify_userset_deletion");

   for_each (cqueue, *object_base[SGE_TYPE_CQUEUE].list) {
      lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
      lListElem *qinstance;

      for_each(qinstance, qinstance_list) {
         if (lGetElemStr(lGetList(qinstance, QU_acl), US_name, userset_name)) {
            ERROR((SGE_EVENT, MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, 
                   userset_name, MSG_OBJ_USERLIST, MSG_OBJ_QUEUE, 
                   lGetString(qinstance, QU_qname)));
            answer_list_add(alpp, SGE_EVENT, 
                            STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = STATUS_EUNKNOWN;
         }
         if (lGetElemStr(lGetList(qinstance, QU_xacl), US_name, userset_name)) {
            ERROR((SGE_EVENT, MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, 
                   userset_name, MSG_OBJ_XUSERLIST, MSG_OBJ_QUEUE, 
                   lGetString(qinstance, QU_qname)));
            answer_list_add(alpp, SGE_EVENT, 
                            STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = STATUS_EUNKNOWN;
         }
      }
   }

   for_each (ep, *object_base[SGE_TYPE_PE].list) {
      if (lGetElemStr(lGetList(ep, PE_user_list), US_name, userset_name)) {
         ERROR((SGE_EVENT, MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, 
               MSG_OBJ_USERLIST, MSG_OBJ_PE, lGetString(ep, PE_name)));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
      if (lGetElemStr(lGetList(ep, PE_xuser_list), US_name, userset_name)) {
         ERROR((SGE_EVENT, MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, 
               MSG_OBJ_XUSERLIST, MSG_OBJ_PE, lGetString(ep, PE_name)));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
   }

   for_each (ep, *object_base[SGE_TYPE_PROJECT].list) {
      if (lGetElemStr(lGetList(ep, UP_acl), US_name, userset_name)) {
         ERROR((SGE_EVENT, MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, 
               MSG_OBJ_USERLIST, MSG_OBJ_PRJ, lGetString(ep, UP_name)));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
      if (lGetElemStr(lGetList(ep, UP_xacl), US_name, userset_name)) {
         ERROR((SGE_EVENT, MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, 
               MSG_OBJ_XUSERLIST, MSG_OBJ_PRJ, lGetString(ep, UP_name)));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
   }

   /* hosts */
   for_each (ep, *object_base[SGE_TYPE_EXECHOST].list) {
      if (lGetElemStr(lGetList(ep, EH_acl), US_name, userset_name)) {
         ERROR((SGE_EVENT, MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name,
               MSG_OBJ_USERLIST, MSG_OBJ_EH, lGetHost(ep, EH_name)));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DEXIT;
         return STATUS_EEXIST;
      }
      if (lGetElemStr(lGetList(ep, EH_xacl), US_name, userset_name)) {
         ERROR((SGE_EVENT, MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name,
               MSG_OBJ_XUSERLIST, MSG_OBJ_EH, lGetHost(ep, EH_name)));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DEXIT;
         return STATUS_EEXIST;
      }
   }

   /* global configuration */
   user_lists = mconf_get_user_lists();
   if (lGetElemStr(user_lists, US_name, userset_name)) {
      ERROR((SGE_EVENT, MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, 
            MSG_OBJ_USERLIST, MSG_OBJ_CONF, MSG_OBJ_GLOBAL));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = STATUS_EUNKNOWN;
   }
   lFreeList(&user_lists);

   user_lists = mconf_get_xuser_lists();
   if (lGetElemStr(user_lists, US_name, userset_name)) {
      ERROR((SGE_EVENT, MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, 
            MSG_OBJ_XUSERLIST, MSG_OBJ_CONF, MSG_OBJ_GLOBAL));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = STATUS_EUNKNOWN;
   }
   lFreeList(&user_lists);

   DEXIT;
   return ret;
}

/****** sge_userset_qmaster/userset_still_used() *******************************
*  NAME
*     userset_still_used() -- True, if userset still used 
*
*  SYNOPSIS
*     static bool userset_still_used(const char *u)
*
*  FUNCTION
*     Returns true, if userset is still used as ACL with host_conf(5),
*     queue_conf(5), or sge_pe(5).
* 
*     Use of usersets as ACLs in sge_conf(5) play no role here, 
*     since such ACLs are checked in qmaster and thus are not 
*     relevant for the scheduling algorithm.
*
*  INPUTS
*     const char *p - the userset
*
*  RESULT
*     static bool - True, if userset still used
*
*  NOTES
*     MT-NOTE: userset_still_used() is not MT safe
*******************************************************************************/
static bool userset_still_used(const char *u)
{
   const lListElem *qc, *cq, *hep, *rqs;
   dstring ds = DSTRING_INIT;

   sge_dstring_sprintf(&ds, "@%s", u);

   for_each (rqs, *object_type_get_master_list(SGE_TYPE_RQS)) {
      if (scope_is_referenced_rqs(rqs, RQR_filter_users, sge_dstring_get_string(&ds))) {
         sge_dstring_free(&ds);
         return true;
      }
   }
   sge_dstring_free(&ds);

   for_each (hep, *object_type_get_master_list(SGE_TYPE_PE)) 
      if (lGetSubStr(hep, US_name, u, PE_user_list) ||
          lGetSubStr(hep, US_name, u, PE_xuser_list))
         return true;

   for_each (hep, *object_type_get_master_list(SGE_TYPE_EXECHOST))
      if (lGetSubStr(hep, US_name, u, EH_acl) ||
          lGetSubStr(hep, US_name, u, EH_xacl))
         return true;

   for_each (cq, *object_type_get_master_list(SGE_TYPE_CQUEUE)) {
      for_each (qc, lGetList(cq, CQ_acl))
         if (lGetSubStr(qc, US_name, u, AUSRLIST_value))
            return true;
      for_each (qc, lGetList(cq, CQ_xacl))
         if (lGetSubStr(qc, US_name, u, AUSRLIST_value))
            return true;
   }

   return false;
}


/****** sge_userset_qmaster/userset_update_categories() ************************
*  NAME
*     userset_update_categories() -- Update all usersets wrts categories
*
*  SYNOPSIS
*     void userset_update_categories(const lList *added, const lList *removed)
*
*  FUNCTION
*     Each added/removed userset is verified whether it is used first
*     time/still as ACL for host_conf(5)/queue_conf(5)/sge_pe(5). If
*     so an event is sent.
*
*  INPUTS
*     const lList *added   - List of added userset references (US_Type)
*     const lList *removed - List of removed userset references (US_Type)
*
*  NOTES
*     MT-NOTE: userset_update_categories() is not MT safe
*******************************************************************************/
void userset_update_categories(const lList *added, const lList *removed)
{
   const lListElem *ep;
   const char *u;
   lListElem *acl;

   DENTER(TOP_LAYER, "userset_update_categories");

   for_each (ep, added) {
      u = lGetString(ep, US_name);
      DPRINTF(("added userset: \"%s\"\n", u));
      acl = lGetElemStr(*object_type_get_master_list(SGE_TYPE_USERSET), US_name, u);
      if (acl && lGetBool(acl, US_consider_with_categories)==false) {
         lSetBool(acl, US_consider_with_categories, true);
         sge_add_event(0, sgeE_USERSET_MOD, 0, 0, u, NULL, NULL, acl);
      }
   }

   for_each (ep, removed) {
      u = lGetString(ep, US_name);
      DPRINTF(("removed userset: \"%s\"\n", u));
      acl = lGetElemStr(*object_type_get_master_list(SGE_TYPE_USERSET), US_name, u);


      if (acl && !userset_still_used(u)) {
         lSetBool(acl, US_consider_with_categories, false);
         sge_add_event(0, sgeE_USERSET_MOD, 0, 0, u, NULL, NULL, acl);
      }
   }

   DEXIT;
   return;
}

