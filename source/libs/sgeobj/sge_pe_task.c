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

#include "sge_string.h"
#include "sgermon.h"
#include "sge_log.h"
#include "cull_list.h"

#include "sge_object.h"
#include "sge_answer.h"

#include "sge_job.h"
#include "sge_ja_task.h"
#include "sge_pe_task.h"

#include "sge_usageL.h"

#include "msg_sgeobjlib.h"

/****** sgeobj/pe_task/pe_task_sum_past_usage() *******************************
*  NAME
*     pe_task_sum_past_usage() -- sum up pe tasks past usage
*
*  SYNOPSIS
*     lListElem* pe_task_sum_past_usage(lListElem *container, 
*                                       const lListElem *pe_task) 
*
*  FUNCTION
*     The pe task list of a ja task can contain one container 
*     element to hold the usage of finished pe tasks no longer 
*     stored in the task list.
*
*     This function adds the usage of pe_task to the usage of container.
*
*  INPUTS
*     lListElem *container     - container object to hold past usage
*     const lListElem *pe_task - the pe task from which to copy usage
*
*  RESULT
*     lListElem* - the container object
*
*  NOTES
*     JG: TODO: The code summing up usage should better be in a 
*               module libs/gdi/sge_usage.* containing all usage 
*               related functions the same or similar functionality 
*               is needed in many places in Grid Engine.
*
*  SEE ALSO
*     sgeobj/pe_task/pe_task_sum_past_usage_all()
*     sgeobj/pe_task/pe_task_sum_past_usage_list()
*******************************************************************************/
lListElem *
pe_task_sum_past_usage(lListElem *container, const lListElem *pe_task)
{
   lList *container_usage;
   const lList *pe_task_usage;
   const lListElem *usage;
   
   DENTER(TOP_LAYER, "pe_task_sum_past_usage");

   /* invalid input - nothing to do */
   if(container == NULL || pe_task == NULL) {
      DEXIT;
      return NULL;
   }

   /* container and pe_task are the same element - nothing to do */
   if(container == pe_task) {
      DEXIT;
      return container;
   }

   container_usage = lGetList(container, PET_scaled_usage);
   pe_task_usage   = lGetList(pe_task,   PET_scaled_usage);

   /* empty usage in pe task - nothing to do */
   if(pe_task_usage == NULL) {
      DEXIT;
      return container;
   }

   /* container has no usage list yet - create it */
   if(container_usage == NULL) {
      container_usage = lCreateList("usage", UA_Type);
      lSetList(container, PET_scaled_usage, container_usage);
   }

   /* sum up usage */
   for_each(usage, pe_task_usage) {
      const char *name = lGetString(usage, UA_name);
      if (!strcmp(name, USAGE_ATTR_CPU) ||
          !strcmp(name, USAGE_ATTR_IO)  ||
          !strcmp(name, USAGE_ATTR_IOW) ||
          !strcmp(name, USAGE_ATTR_VMEM) ||
          !strcmp(name, USAGE_ATTR_MAXVMEM) ||
          !strcmp(name, USAGE_ATTR_MEM)) {
         lListElem *sum = lGetElemStr(container_usage, UA_name, name);
         if(sum == NULL) {
            lAppendElem(container_usage, lCopyElem(usage));
         } else {
            lSetDouble(sum, UA_value, lGetDouble(sum, UA_value) + lGetDouble(usage, UA_value));
         }
      }   
   }

   DEXIT;
   return container;
}

/****** sgeobj/pe_task/pe_task_sum_past_usage_all() ***************************
*  NAME
*     pe_task_sum_past_usage_all() -- sum up all pe tasks past usage
*
*  SYNOPSIS
*     lListElem* pe_task_sum_past_usage_all(lList *pe_task_list)
*
*  FUNCTION
*     Similar to pe_task_sum_past_usage, but will sum up the usage of 
*     all pe tasks in a task list to the container object in this list.
*
*     If the container object does not yet exist, it will be created.
*
*  INPUTS
*     lList *pe_task_list - the pe task list to process
*
*  RESULT
*     lListElem* - the container object
*
*  SEE ALSO
*     sgeobj/pe_task/pe_task_sum_past_usage()
*     sgeobj/pe_task/pe_task_sum_past_usage_list()
*******************************************************************************/
lListElem *pe_task_sum_past_usage_all(lList *pe_task_list)
{
   lListElem *container = NULL;
   const lListElem *pe_task;

   DENTER(TOP_LAYER, "pe_task_sum_past_usage_all");

   /* no pe task list - nothing to do */
   if(pe_task_list == NULL) {
      DEXIT;
      return NULL;
   }

   /* loop over all pe tasks and sum up usage */
   for_each(pe_task, pe_task_list) {
      if(container == NULL) {
         container = pe_task_sum_past_usage_list(pe_task_list, pe_task);
      } else {
         pe_task_sum_past_usage(container, pe_task);
      }
   }

   DEXIT;
   return container;
}

/****** sgeobj/pe_task/pe_task_sum_past_usage_list() **************************
*  NAME
*     pe_task_sum_past_usage_list() -- sum up pe tasks past usage
*
*  SYNOPSIS
*     lListElem* 
*     pe_task_sum_past_usage_list(lList *pe_task_list, 
*                                 const lListElem *pe_task) 
*
*  FUNCTION
*     Similar to pe_task_sum_past_usage.
*     The container is retrieved from pe_task_list, if it does not 
*     yet exist it is created and inserted into pe_task_list as 
*     first element.
*
*  INPUTS
*     lList *pe_task_list      - list containing the container object
*     const lListElem *pe_task - the pe task from which to copy usage
*
*  RESULT
*     lListElem* - the container object
*
*  SEE ALSO
*     sgeobj/pe_task/pe_task_sum_past_usage()
*     sgeobj/pe_task/pe_task_sum_past_usage_all()
******************************************************************************/
lListElem *
pe_task_sum_past_usage_list(lList *pe_task_list, const lListElem *pe_task)
{
   lListElem *container;

   DENTER(TOP_LAYER, "pe_task_sum_past_usage_list");

   /* no pe task list - nothing to do */
   if(pe_task_list == NULL) {
      DEXIT;
      return NULL;
   }

   /* get container - if it does not yet exist, create it as first element in pe task list */
   container = lGetElemStr(pe_task_list, PET_id, PE_TASK_PAST_USAGE_CONTAINER);
   if(container == NULL) {
      container = lCreateElem(PET_Type);
      lSetString(container, PET_id, PE_TASK_PAST_USAGE_CONTAINER);
      lInsertElem(pe_task_list, NULL, container);
   }

   /* sum up usage */
   pe_task_sum_past_usage(container, pe_task);

   DEXIT;
   return container;
}

/****** sge_pe_task/pe_task_verify_request() ***********************************
*  NAME
*     pe_task_verify_request() -- verify a pe task request object
*
*  SYNOPSIS
*     bool 
*     pe_task_verify_request(const lListElem *petr, lList **answer_list) 
*
*  FUNCTION
*     Verifies structure and contents of a pe task request.
*     A pe task request is sent to sge_execd by qrsh -inherit.
*
*  INPUTS
*     const lListElem *petr - the object to verify
*     lList **answer_list   - answer list to pass back error messages
*
*  RESULT
*     bool - true on success,
*            false on error with error message in answer_list
*
*  NOTES
*     MT-NOTE: pe_task_verify_request() is MT safe 
*
*  BUGS
*     The function is far from being complete.
*     Currently, only the CULL structure is verified, not the contents.
*
*  SEE ALSO
*     sge_object/object_verify_cull()
*******************************************************************************/
bool 
pe_task_verify_request(const lListElem *petr, lList **answer_list) {
   bool ret = true;

   DENTER(TOP_LAYER, "pe_task_verify_request");

   if (petr == NULL) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, "NULL pointer argument");
      ret = false;
   }

   if (ret) {
      if (!object_verify_cull(petr, PETR_Type)) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, "corrupted cull structure or reduced element");
         ret = false;
      }
   }

   /* 
    * A pe task request entering execd must have some additional properties: 
    *    - PETR_jobid > 0
    *    - PETR_jataskid ??
    *    - PETR_queuename ??
    *    - PETR_owner != NULL
    *    - PETR_cwd ??
    *    - verify PETR_path_aliases, if they are != NULL
    *    - verify PETR_environment, if it is != NULL
    *    - PETR_submission_time > 0
    */

   DRETURN(ret);
}
