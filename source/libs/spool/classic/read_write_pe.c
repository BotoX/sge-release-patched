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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "sge.h"
#include "sge_pe.h"
#include "sge_answer.h"
#include "read_write_pe.h"
#include "sge_string.h"
#include "sge_log.h"
#include "config.h"
#include "read_object.h"
#include "sgermon.h"
#include "sge_stdio.h"
#include "msg_common.h"
#include "spool/classic/msg_spoollib_classic.h"
#include "sge_feature.h"
#include "sge_spool.h"
#include "sge_io.h"
#include "sge_userset.h"
#include "sge_resource_utilization.h"
#include "sge_qinstance.h"
#include "sge_qref.h"

/****
 **** cull_read_in_pe
 ****/
lListElem *cull_read_in_pe(
const char *dirname,
const char *filename,
int spool,
int type,
int *tag,
int fields[] 
) {
   lListElem *ep;
   struct read_object_args args = { PE_Type, "pe", read_pe_work };
   int intern_tag = 0;

   DENTER(TOP_LAYER, "cull_read_in_pe");

   ep = read_object(dirname, filename, spool, 0, 0,&args, tag?tag:&intern_tag, fields);
  
   DEXIT;
   return ep;
}


/* ------------------------------------------------------------

   read_pe_work()

   spool:
      1 write for spooling
      0 write only user controlled fields

*/
int read_pe_work(
lList **alpp,
lList **clpp,
int fields[],
lListElem *ep,
int spool,
int flag,
int *tag,
int parsing_type 
) {
   DENTER(TOP_LAYER, "read_pe_work");

   /* --------- PE_name */
   if (!set_conf_string(alpp, clpp, fields, "pe_name", ep, PE_name)) {
      DEXIT;
      return -1;
   }

   /* --------- PE_slots */
   if (!set_conf_ulong(alpp, clpp, fields, "slots", ep, PE_slots)) {
      DEXIT;
      return -1;
   }

   /* --------- PE_user_list */
   if (!set_conf_list(alpp, clpp, fields, "user_lists", ep, PE_user_list, 
                        US_Type, US_name)) {
      DEXIT;
      return -1;
   }
   
   /* --------- PE_xuser_list */
   if (!set_conf_list(alpp, clpp, fields, "xuser_lists", ep, PE_xuser_list, 
                        US_Type, US_name)) {
      DEXIT;
      return -1;
   }

   /* --------- PE_start_proc_args */
   if (!set_conf_string(alpp, clpp, fields, "start_proc_args", ep, 
            PE_start_proc_args)) {
      DEXIT;
      return -1;
   }
   NULL_OUT_NONE(ep, PE_start_proc_args);

   /* --------- PE_stop_proc_args */
   if (!set_conf_string(alpp, clpp, fields, "stop_proc_args", ep, 
       PE_stop_proc_args)) {
      DEXIT;
      return -1;
   }
   NULL_OUT_NONE(ep, PE_stop_proc_args);

   /* --------- PE_allocation_rule */
   if (!set_conf_string(alpp, clpp, fields, "allocation_rule", ep, 
            PE_allocation_rule)) {
      DEXIT;
      return -1;
   }

   /* --------- PE_control_slaves */
   set_conf_bool(NULL, clpp, fields, "control_slaves", ep, PE_control_slaves);

   /* --------- PE_job_is_first_task */
   set_conf_bool(NULL, clpp, fields, "job_is_first_task", ep, 
                 PE_job_is_first_task);

   /* --------- PE_urgency_slots */
   if (!set_conf_string(alpp, clpp, fields, "urgency_slots", ep, 
            PE_urgency_slots)) {
      DEXIT;
      return -1;
   }

   /* initialize number of used PE slots with 0 */
   if (pe_set_slots_used(ep, 0)) {
      answer_list_add(alpp, MSG_GDI_OUTOFMEMORY, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DEXIT;
      return -1;
   }

   DEXIT;
   return 0;
}

/* ------------------------------------------------------------

   returns tmpfile name in case of creating a tempfile

   spool:
      1 write for spooling
      0 write only user controlled fields

   how:
      0 use stdout
      1 write into tmpfile
      2 write into spoolfile

*/         

char *write_pe(
int spool,
int how,
const lListElem *ep 
) {
   FILE *fp;
   lListElem *sep;
   const char *s; 
   char filename[SGE_PATH_MAX], real_filename[SGE_PATH_MAX];
   dstring ds;
   char buffer[256];

   DENTER(TOP_LAYER, "write_pe");

   sge_dstring_init(&ds, buffer, sizeof(buffer));
   switch (how) {
   case 0:
      fp = stdout;
      break;
   case 1:
   case 2:
      if (how==1) {
         if (!sge_tmpnam(filename)) {
            CRITICAL((SGE_EVENT, MSG_TMPNAM_GENERATINGTMPNAM));
            DEXIT;
            return NULL;
         }
      } else {
         sprintf(filename, "%s/.%s", PE_DIR, lGetString(ep, PE_name));
         sprintf(real_filename, "%s/%s", PE_DIR, lGetString(ep, PE_name));
      }

      fp = fopen(filename, "w");
      if (!fp) {
         CRITICAL((SGE_EVENT, MSG_ERRORWRITINGFILE_SS, filename, strerror(errno)));
         DEXIT;
         return NULL;
      }
      break;
   default:
      DEXIT;
      return NULL;
   }

   if (spool && sge_spoolmsg_write(fp, COMMENT_CHAR, 
         feature_get_product_name(FS_VERSION, &ds)) < 0) {
      goto FPRINTF_ERROR;
   }

   /* --------- PE_name */
   FPRINTF((fp, "pe_name           %s\n", lGetString(ep, PE_name)));

   /* --------- PE_slots */
   FPRINTF((fp, "slots             %d\n", (int)lGetUlong(ep, PE_slots)));

   /* --------- PE_user_list */
   FPRINTF((fp, "user_lists        "));
   sep = lFirst(lGetList(ep, PE_user_list));
   if (sep) {
      do {
         FPRINTF((fp, "%s", lGetString(sep, US_name)));
         sep = lNext(sep);
         if (sep) 
            FPRINTF((fp, " "));
      } while (sep);
      FPRINTF((fp, "\n"));
   }
   else
      FPRINTF((fp, "NONE\n"));

   /* --------- PE_xuser_list */
   FPRINTF((fp, "xuser_lists       "));
   sep = lFirst(lGetList(ep, PE_xuser_list));
   if (sep) {
      do {
         FPRINTF((fp, "%s", lGetString(sep, US_name)));
         sep = lNext(sep);
         if (sep) 
            FPRINTF((fp, " "));
      } while (sep);
      FPRINTF((fp, "\n"));
   }
   else
      FPRINTF((fp, "NONE\n"));

   /* --------- PE_start_proc_args */
   FPRINTF((fp, "start_proc_args   %s\n", 
         (s=lGetString(ep, PE_start_proc_args))?s:"NONE"));

   /* --------- PE_stop_proc_args */
   FPRINTF((fp, "stop_proc_args    %s\n", 
         (s=lGetString(ep, PE_stop_proc_args))?s:"NONE"));

   /* --------- PE_allocation_rule */
   FPRINTF((fp, "allocation_rule   %s\n", lGetString(ep, PE_allocation_rule)));

   /* --------- PE_control_slaves */
   FPRINTF((fp, "control_slaves    %s\n", lGetBool(ep, PE_control_slaves) ? 
               "TRUE" : "FALSE"));

   /* --------- PE_job_is_first_task */
   FPRINTF((fp, "job_is_first_task %s\n", lGetBool(ep, PE_job_is_first_task) ? 
               "TRUE" : "FALSE"));


   /* --------- PE_urgency_slots */
   FPRINTF((fp, "urgency_slots     %s\n", lGetString(ep, PE_urgency_slots)));


   /* --------- internal fields ----------------------------------- */

   /* --------- PE_resource_utilization */
   /* 
      the number of free slots is evaluated dynamically to 
      prevent useless disk spooling 
      output only in case of qconf -sp 
   */
   if (!spool && how == 0 && getenv("MORE_INFO")) {
      int n;

      n = lGetUlong(ep, PE_slots) - pe_get_slots_used(ep);
      FPRINTF((fp, "free_slots       %d\n", n));
   }

   if (how!=0) {
      fclose(fp);
   }

   if (how == 2) {
      if (rename(filename, real_filename) == -1) {
         DEXIT;
         return NULL;
      } else {
         strcpy(filename, real_filename);
      }
   }   
   DEXIT;
   return how==1?sge_strdup(NULL, filename):filename;

FPRINTF_ERROR:
   DEXIT;
   return NULL;
}
