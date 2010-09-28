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

#include "commlib.h"
#include "cull.h"
#include "sgermon.h"
#include "sge_manop_qmaster.h"
#include "sge_event_master.h"
#include "sge_log.h"
#include "sge_uidgid.h"
#include "sge_answer.h"
#include "sge_manop.h"

#include "sge_persistence_qmaster.h"
#include "spool/sge_spooling.h"

#include "msg_common.h"
#include "msg_qmaster.h"

/* ------------------------------------------------------------

   sge_add_manop() - adds an manop list to the global manager/operator
                     list

   if the invoking process is the qmaster 
   the added manop list is spooled in the MANAGER_FILE/OPERATOR_FILE

*/
int sge_add_manop(
lListElem *ep,
lList **alpp,
char *ruser,
char *rhost,
u_long32 target  /* may be SGE_MANAGER_LIST or SGE_OPERATOR_LIST */
) {
   const char *manop_name;
   const char *object_name;
   lList **lpp = NULL;
   lListElem *added;
   int pos;

   DENTER(TOP_LAYER, "sge_add_manop");

   if ( !ep || !ruser || !rhost ) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   switch (target) {
   case SGE_MANAGER_LIST:
      lpp = &Master_Manager_List;
      object_name = MSG_OBJ_MANAGER;
      break;
   case SGE_OPERATOR_LIST:
      lpp = &Master_Operator_List;
      object_name = MSG_OBJ_OPERATOR;
      break;
   default :
      DPRINTF(("unknown target passed to %s\n", SGE_FUNC));
      DEXIT;
      return STATUS_EUNKNOWN;
   }
DTRACE;
   /* ep is no acl element, if ep has no MO_name */
   if ((pos = lGetPosViaElem(ep, MO_name)) < 0) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_MISSINGCULLFIELD_SS,
            lNm2Str(MO_name), SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   manop_name = lGetPosString(ep, pos);
   if (!manop_name) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   if (lGetElemStr(*lpp, MO_name, manop_name)) {
      ERROR((SGE_EVENT, MSG_SGETEXT_ALREADYEXISTS_SS, object_name, manop_name));
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EEXIST;
   }

   /* update in interal lists */
   added = lAddElemStr(lpp, MO_name, manop_name, MO_Type);

   /* update on file */
   if(!sge_event_spool(alpp, 0, target == SGE_MANAGER_LIST ? sgeE_MANAGER_ADD : 
                                                             sgeE_OPERATOR_ADD,
                       0, 0, manop_name, NULL, NULL,
                       added, NULL, NULL, true, true)) {
      ERROR((SGE_EVENT, MSG_SGETEXT_CANTSPOOL_SS, object_name, manop_name));
      answer_list_add(alpp, SGE_EVENT, STATUS_EDISK, ANSWER_QUALITY_ERROR);
   
      /* remove element from list */
      lFreeElem(lDechainElem(*lpp, added));

      DEXIT;
      return STATUS_EDISK;
   }

   INFO((SGE_EVENT, MSG_SGETEXT_ADDEDTOLIST_SSSS,
            ruser, rhost, manop_name, object_name));
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DEXIT;
   return STATUS_OK;
}

/* ------------------------------------------------------------

   sge_del_manop() - deletes an access list from the global acl_list

*/
int sge_del_manop(
lListElem *ep,
lList **alpp,
char *ruser,
char *rhost,
u_long32 target  /* may be SGE_MANAGER_LIST or SGE_OPERATOR_LIST */
) {
   lListElem *found;
   int pos;
   const char *manop_name;
   const char *object_name;
   lList **lpp = NULL;

   DENTER(TOP_LAYER, "sge_del_manop");

   if ( !ep || !ruser || !rhost ) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   switch (target) {
   case SGE_MANAGER_LIST:
      lpp = &Master_Manager_List;
      object_name = MSG_OBJ_MANAGER;
      break;
   case SGE_OPERATOR_LIST:
      lpp = &Master_Operator_List;
      object_name = MSG_OBJ_OPERATOR;
      break;
   default :
      DPRINTF(("unknown target passed to %s\n", SGE_FUNC));
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   /* ep is no manop element, if ep has no MO_name */
   if ((pos = lGetPosViaElem(ep, MO_name)) < 0) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_MISSINGCULLFIELD_SS,
            lNm2Str(MO_name), SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   manop_name = lGetPosString(ep, pos);
   if (!manop_name) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, SGE_FUNC));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EUNKNOWN;
   }

   /* prevent removing of root from man/op-list */
   if (!strcmp(manop_name, "root")) {
      ERROR((SGE_EVENT, MSG_SGETEXT_MAY_NOT_REMOVE_USER_FROM_LIST_SS, "root", object_name));  
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EEXIST;
   }

   found = lGetElemStr(*lpp, MO_name, manop_name);
   if (!found) {
      ERROR((SGE_EVENT, MSG_SGETEXT_DOESNOTEXIST_SS, object_name, manop_name));
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DEXIT;
      return STATUS_EEXIST;
   }
   
   lDechainElem(*lpp, found);

   /* update on file */
   if (!sge_event_spool(alpp, 0, target == SGE_MANAGER_LIST ? 
                                 sgeE_MANAGER_DEL : sgeE_OPERATOR_DEL,
                           0, 0, manop_name, NULL, NULL,
                           NULL, NULL, NULL, true, true)) {
      ERROR((SGE_EVENT, MSG_SGETEXT_CANTSPOOL_SS, object_name, manop_name));
      answer_list_add(alpp, SGE_EVENT, STATUS_EDISK, ANSWER_QUALITY_ERROR);
   
      /* chain in again */
      lAppendElem(*lpp, found);

      DEXIT;
      return STATUS_EDISK;
   }
   lFreeElem(found);

   INFO((SGE_EVENT, MSG_SGETEXT_REMOVEDFROMLIST_SSSS,
            ruser, rhost, manop_name, object_name));
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DEXIT;
   return STATUS_OK;
}

