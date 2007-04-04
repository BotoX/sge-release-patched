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

#include "sgermon.h"
#include "sge_log.h"
#include "cull.h"
#include "valid_queue_user.h"
#include "sge_string.h"
#include "sge_answer.h"
#include "sge_qinstance.h"
#include "sge_userset.h"

#include "msg_schedd.h"

static int sge_contained_in_access_list_(const char *user, const char *group, 
                                         lList *acl, const lList *acl_list);

/* - -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -

   sge_has_access - determines if a user/group has access to a queue

   returns
      1 for true
      0 for false

*/
int sge_has_access(const char *user, const char *group, lListElem *q,
                   const lList *acl_list) 
{
   return sge_has_access_(user, group, 
         lGetList(q, QU_acl), lGetList(q, QU_xacl), acl_list);
}

/* needed to test also sge_queue_type structures without converting the
** whole queue
*/
int sge_has_access_(const char *user, const char *group, lList *q_acl,
                    lList *q_xacl, const lList *acl_list) 
{
   int ret;

   DENTER(TOP_LAYER, "sge_has_access_");

   ret = sge_contained_in_access_list_(user, group, q_xacl, acl_list);
   if (ret < 0 || ret == 1) { /* also deny when an xacl entry was not found in acl_list */
      DRETURN(0);
   }

   if (!q_acl) {  /* no entry in allowance list - ok everyone */
       DRETURN(1);
   }

   ret = sge_contained_in_access_list_(user, group, q_acl, acl_list);
   if (ret < 0) {
      DRETURN(0);
   }
   if (ret) {
      /* we're explicitly allowed to access */
      DRETURN(1);
   }

   /* there is an allowance list but the owner isn't there -> denied */
   DRETURN(0);
} 


/* sge_contained_in_access_list_() returns 
   1  yes it is contained in the acl
   0  no 
   -1 got NULL for user/group 

   user, group: may be NULL
*/
static int sge_contained_in_access_list_(const char *user, const char *group,
                                         lList *acl, const lList *acl_list) 
{
   lListElem *acl_search, *acl_found;

   DENTER(TOP_LAYER,"sge_contained_in_access_list_");

   for_each (acl_search, acl) {
      if ((acl_found=lGetElemStr(acl_list, US_name,
            lGetString(acl_search,US_name)))) {
         /* ok - there is such an access list */
         if (sge_contained_in_access_list(user, group, acl_found, NULL)) {
            DRETURN(1);
         } 
      } else {
      	DPRINTF(("cannot find userset list entry \"%s\"\n", 
		         lGetString(acl_search,US_name)));
      }
   }
   DRETURN(0);
}

