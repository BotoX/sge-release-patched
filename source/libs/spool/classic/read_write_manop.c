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
#include <errno.h>
#include <string.h>

#include "sge_unistd.h"
#include "sge.h"
#include "sgermon.h"
#include "sge_gdi.h"
#include "sge_log.h"
#include "read_write_manop.h"
#include "cull.h"
#include "sge_stdio.h"
#include "msg_common.h" 
#include "msg_spoollib_classic.h"
#include "sge_feature.h"
#include "sge_manop.h"
#include "sge_spool.h"
#include "sgeobj/sge_object.h"

/* ------------------------------------------------------------

   read_manop()

   reads either manager or operator list from disk

*/
int read_manop(
int target 
) {
   lListElem *ep;
   lList **lpp;
   stringT filename;
   char str[256];
   FILE *fp;
   SGE_STRUCT_STAT st;
   lDescr *descr = NULL;
   int key;

   DENTER(TOP_LAYER, "read_manop");

   switch (target) {
   case SGE_MANAGER_LIST:
      lpp = object_type_get_master_list(SGE_TYPE_MANAGER);      
      descr = UM_Type;
      key = UM_name;
      strcpy(filename, MAN_FILE);
      break;
      
   case SGE_OPERATOR_LIST:
      lpp = object_type_get_master_list(SGE_TYPE_OPERATOR);      
      descr = UO_Type;
      key = UO_name;
      strcpy(filename, OP_FILE);
      break;

   default:
      DEXIT;
      return 1;
   }

   /* if no such file exists. ok return without error */
   if (SGE_STAT(filename, &st) && errno==ENOENT) {
      DEXIT;
      return 0;
   }

   fp = fopen(filename, "r");
   if (!fp) {
      ERROR((SGE_EVENT, MSG_FILE_ERROROPENINGX_S, filename));
      DEXIT;
      return 1;
   }
   
   lFreeList(lpp);
   *lpp = lCreateList("man list", descr);

   while (fscanf(fp, "%[^\n]\n", str) == 1) {
      ep = lCreateElem(descr);
      if (str[0] != COMMENT_CHAR) {
         lSetString(ep, key, str);
         lAppendElem(*lpp, ep);
      } else {
         lFreeElem(&ep);
      }
   }

   FCLOSE(fp);

   DEXIT;
   return 0;
FCLOSE_ERROR:
   ERROR((SGE_EVENT, MSG_FILE_ERRORCLOSEINGX_S, filename));
   DEXIT;
   return 1;
}



/* ------------------------------------------------------------

   write_manop()

   writes either manager or operator list to disk

   spool:
      1 write for spooling
      0 write only user controlled fields
 
*/
 
int write_manop(
int spool,
int target 
) {
   FILE *fp;
   lListElem *ep;
   lList *lp;
   char filename[255], real_filename[255];
   dstring ds;
   char buffer[256];
   int key;

   DENTER(TOP_LAYER, "write_manop");

   sge_dstring_init(&ds, buffer, sizeof(buffer));
   switch (target) {
   case SGE_MANAGER_LIST:
      lp = *object_type_get_master_list(SGE_TYPE_MANAGER);      
      key = UM_name;
      strcpy(filename, ".");
      strcat(filename, MAN_FILE);
      strcpy(real_filename, MAN_FILE);
      break;
      
   case SGE_OPERATOR_LIST:
      lp = *object_type_get_master_list(SGE_TYPE_OPERATOR);      
      key = UO_name;
      strcpy(filename, ".");
      strcat(filename, OP_FILE);
      strcpy(real_filename, OP_FILE);
      break;

   default:
      DEXIT;
      return 1;
   }

   fp = fopen(filename, "w");
   if (!fp) {
      ERROR((SGE_EVENT, MSG_ERRORWRITINGFILE_SS, filename, strerror(errno)));
      DEXIT;
      return 1;
   }

   if (spool && sge_spoolmsg_write(fp, COMMENT_CHAR,
             feature_get_product_name(FS_VERSION, &ds)) < 0) {
      goto FPRINTF_ERROR;
   }  

   for_each(ep, lp) {
      FPRINTF((fp, "%s\n", lGetString(ep, key)));
   }

   FCLOSE(fp);

   if (rename(filename, real_filename) == -1) {
      DEXIT;
      return 1;
   } else {
      strcpy(filename, real_filename);
   }     

   DEXIT;
   return 0;

FPRINTF_ERROR:
FCLOSE_ERROR:
   DEXIT;
   return 1;  
}


