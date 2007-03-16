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
#include <errno.h>
#include <limits.h>
#include <fnmatch.h>
#include <ctype.h>


#include "sge_advance_reservation.h"

#include "uti/sge_stdlib.h"
#include "uti/sge_stdio.h"

#include "rmon/sgermon.h"
#include "sgeobj/sge_answer.h"
#include "uti/sge_time.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "sge_time.h"
#include "sge_log.h"
#include "sge.h"
#include "symbols.h"
#include "sge_utility.h"
#include "sge_object.h"
#include "sge_centry.h"
#include "sge_qref.h"
#include "sge_pe.h"
#include "msg_qmaster.h"
#include "sge_range.h"
#include "sge_rangeL.h"


/****** sge_advance_reservation/ar_list_locate() *******************************
*  NAME
*     ar_list_locate() -- locate a advance reservation by id
*
*  SYNOPSIS
*     lListElem* ar_list_locate(lList *ar_list, u_long32 ar_id) 
*
*  FUNCTION
*     This function returns a ar object with the selected id from the
*     given list.
*
*  INPUTS
*     lList *ar_list - list to be searched in
*     u_long32 ar_id - id of interest
*
*  RESULT
*     lListElem* - if found the reference to the ar object, else NULL
*
*  NOTES
*     MT-NOTE: ar_list_locate() is MT safe 
*******************************************************************************/
lListElem *ar_list_locate(lList *ar_list, u_long32 ar_id)
{
   lListElem *ep = NULL;

   DENTER(TOP_LAYER, "ar_list_locate");

   ep = lGetElemUlong(ar_list, AR_id, ar_id);

   DRETURN(ep);
}

/****** sge_advance_reservation/ar_validate() **********************************
*  NAME
*     ar_validate() -- validate a advance reservation
*
*  SYNOPSIS
*     bool ar_validate(lListElem *ar, lList **alpp, bool in_master)
*
*  FUNCTION
*     Ensures a new ar has valid start and end times
*
*  INPUTS
*     lListElem *ar   - the ar to check
*     lList **alpp - answer list pointer
*     bool in_master      - are we in qmaster?
*
*  RESULT
*     bool - true if OK, else false
*
*  NOTES
*     MT-NOTE: ar_validate() is MT safe
*******************************************************************************/
bool ar_validate(lListElem *ar, lList **alpp, bool in_master)
{
   u_long32 start_time;
   u_long32 end_time;
   u_long32 duration;
   u_long32 now = sge_get_gmt();
   object_description *object_base = object_type_get_object_description();
   
   DENTER(TOP_LAYER, "ar_validate");

   /*   AR_start_time, SGE_ULONG        */
   if ((start_time = lGetUlong(ar, AR_start_time)) == 0) {
      answer_list_add_sprintf(alpp, STATUS_EEXIST, ANSWER_QUALITY_ERROR ,
                              MSG_AR_MISSING_VALUE_S, "start time");
      goto ERROR;
   }

   /*   AR_end_time, SGE_ULONG        */
   if ((end_time = lGetUlong(ar, AR_end_time)) == 0) {
      answer_list_add_sprintf(alpp, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                              MSG_AR_MISSING_VALUE_S, "end time");
      goto ERROR;
   }

   /*   AR_duration, SGE_ULONG */
   if ((duration =  lGetUlong(ar, AR_duration)) == 0) {
      answer_list_add_sprintf(alpp, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                              MSG_AR_MISSING_VALUE_S, "duration");
      goto ERROR;
   }

   if ((end_time - start_time) != duration) {
      answer_list_add_sprintf(alpp, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                              MSG_AR_START_END_DURATION_INVALID);
      goto ERROR;
   }

   if (start_time > end_time) {
      answer_list_add_sprintf(alpp, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                              MSG_AR_START_LATER_THAN_END);
      goto ERROR;
   }
   if (start_time < now) {
      answer_list_add_sprintf(alpp, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                              MSG_AR_START_IN_PAST);
      goto ERROR;
   }
   /*   AR_owner, SGE_STRING */
 
   
   if (in_master) {
      /*    AR_name, SGE_STRING */
      if (object_verify_name(ar, alpp, AR_name, SGE_OBJ_AR)) {
         goto ERROR;
      }
      /*   AR_account, SGE_STRING */
      if (!lGetString(ar, AR_account)) {
         lSetString(ar, AR_account, DEFAULT_ACCOUNT);
      } else {
         if (verify_str_key(alpp, lGetString(ar, AR_account), MAX_VERIFY_STRING,
         "account string", QSUB_TABLE) != STATUS_OK) {
            goto ERROR;
         }
      }
      /*   AR_verify, SGE_ULONG              just verify the reservation or final case */
      /*   AR_error_handling, SGE_ULONG      how to deal with soft and hard exceptions */
      /*   AR_state, SGE_ULONG               state of the AR */
      /*   AR_checkpoint_name, SGE_STRING    Named checkpoint */
      /*   AR_resource_list, SGE_LIST */
      {
         lList *master_centry_list = *object_base[SGE_TYPE_CENTRY].list;
         
         if (centry_list_fill_request(lGetList(ar, AR_resource_list),
         alpp, master_centry_list, false, true,
         false)) {
            goto ERROR;
         }
         if (compress_ressources(alpp, lGetList(ar, AR_resource_list), SGE_OBJ_AR)) {
            goto ERROR;
         }
         
         if (!centry_list_is_correct(lGetList(ar, AR_resource_list), alpp)) {
            goto ERROR;
         }
      }
      /*   AR_queue_list, SGE_LIST */
      if (!qref_list_is_valid(lGetList(ar, AR_queue_list), alpp)) {
         goto ERROR;
      }
      /*   AR_mail_options, SGE_ULONG   */
      /*   -PJ- TBD: check for enum */
      /*   AR_mail_list, SGE_LIST */
      /*   -PJ- TBD: check list */
      
      
      /*   AR_pe, SGE_STRING,  AR_pe_range, SGE_LIST */
      {
         const char *pe_name = NULL;
         lList *pe_range = NULL;
         int ret;
         
         pe_name = lGetString(ar, AR_pe);
         if (pe_name) {
            const lListElem *pep;
            pep = pe_list_find_matching(*object_base[SGE_TYPE_PE].list, pe_name);
            if (!pep) {
               ERROR((SGE_EVENT, MSG_JOB_PEUNKNOWN_S, pe_name));
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               goto ERROR;
            }
            /* check pe_range */
            pe_range = lGetList(ar, AR_pe_range);
            if (object_verify_pe_range(alpp, pe_name, pe_range, SGE_OBJ_AR)!=STATUS_OK) {
               goto ERROR;
            }
         }
         /*   AR_acl_list, SGE_LIST */
         if ((ret=userset_list_validate_acl_list(lGetList(ar, AR_acl_list), alpp)) != STATUS_OK) {
            goto ERROR;
         }
         
         /*   AR_xacl_list, SGE_LIST */
         if ((ret=userset_list_validate_acl_list(lGetList(ar, AR_xacl_list), alpp)) != STATUS_OK) {
            goto ERROR;
         }
      }
      
   /*   AR_type, SGE_ULONG     */
   }
   DRETURN(true);

ERROR:
   DRETURN(false);
}


