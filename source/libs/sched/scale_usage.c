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

#include "cull.h"
#include "sge_usageL.h"
#include "sge_ja_task.h"
#include "sge_host.h"
#include "sge_sched.h"
#include "sgermon.h"

/* #define TRACE_INCOMING_USAGE */

/* if the scaled usage list does not yet exist, it is created and returned */
/* JG: TODO: why is it in libsched? It is only used by qmaster. 
 *           it might be moved to a module handling usage objects in libs/gdi
 */
lList *scale_usage(
lList *scaling,     /* HS_Type */
lList *prev_usage,  /* HS_Type */
lList *scaled_usage /* UA_Type */
) {
   lListElem *sep, *ep, *prev;

   DENTER(TOP_LAYER, "scale_usage");

#ifndef TRACE_INCOMING_USAGE 
   if (!scaling) {
      DRETURN(NULL);
   }
#endif

   if (scaled_usage == NULL) {
      scaled_usage = lCreateList("usage", UA_Type);
   }

   for_each (ep, scaled_usage) {
#ifdef TRACE_INCOMING_USAGE
      DPRINTF(("%s = %f\n", 
         lGetString(ep, UA_name), 
         lGetDouble(ep, UA_value))); 
#endif
      if ((sep=lGetElemStr(scaling, HS_name, lGetString(ep, UA_name)))) {
         lSetDouble(ep, UA_value, lGetDouble(ep, UA_value) * lGetDouble(sep, HS_value));
      }
   }

   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_CPU)) != NULL) {
      if ((ep=lGetElemStr(scaled_usage, UA_name, USAGE_ATTR_CPU))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_IO)) != NULL) {
      if ((ep=lGetElemStr(scaled_usage, UA_name, USAGE_ATTR_IO))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_IOW)) != NULL) {
      if ((ep=lGetElemStr(scaled_usage, UA_name, USAGE_ATTR_IOW))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_VMEM)) != NULL) {
      if ((ep=lGetElemStr(scaled_usage, UA_name, USAGE_ATTR_VMEM))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_MAXVMEM)) != NULL) {
      if ((ep=lGetElemStr(scaled_usage, UA_name, USAGE_ATTR_MAXVMEM))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }
   if ((prev = lGetElemStr(prev_usage, UA_name, USAGE_ATTR_MEM)) != NULL) {
      if ((ep=lGetElemStr(scaled_usage, UA_name, USAGE_ATTR_MEM))) {
         lAddDouble(ep, UA_value, lGetDouble(prev, UA_value));
      } else {
         lAppendElem(scaled_usage, lCopyElem(prev));
      }
   }

   DRETURN(scaled_usage);
}
