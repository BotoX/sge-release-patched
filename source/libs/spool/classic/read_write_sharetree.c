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
/*
   This is the module for handling the SGE sharetree
   We save the sharetree to <spool>/qmaster/sharetree
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "sge_unistd.h"
#include "sgermon.h"
#include "sge_usageL.h"
#include "sge_sharetree.h"
#include "cull_parse_util.h"
#include "sge_log.h"
#include "sge_dstring.h"
#include "scheduler.h"
#include "sge_answer.h"
#include "sgeee.h"                                                            
#include "sge_support.h"                                                
#include "msg_common.h"
#include "sge_feature.h"
#include "sge_stdio.h"
#include "sge_spool.h"

static lListElem *search_nodeSN(lListElem *ep, u_long32 id);


/************************************************************************
  write_sharetree

  write a sharetree element human readable to file or fp. 
  If fname == NULL use fpout for writing.
  if spool!=0 store all information else store user relevant information
  sharetree file looks like this:

  id=1             when spooled 
  name=trulla
  type=1
  version=178      when spooled and root_node == true 
  shares=22
  childnodes=2,3   when spooled

  If the file contains a whole tree repetitions follow. "childnodes" is used
  as delimiter.

 ************************************************************************/
int write_sharetree(
lList **alpp,
const lListElem *ep,
char *fname,
FILE *fpout,
int spool,
int recurse,        /* if =1 recursive write */
int root_node 
) {
   FILE *fp; 
   int print_elements[] = { STN_id, 0 };
   const char *delis[] = {":", ",", NULL};
   lListElem *cep;
   int i;
   dstring ds;
   char buffer[256];

   DENTER(TOP_LAYER, "write_sharetree");

   sge_dstring_init(&ds, buffer, sizeof(buffer));

   if (!ep) {
      if (!alpp) {
         ERROR((SGE_EVENT, MSG_OBJ_NOSTREEELEM));
         SGE_EXIT(1);
      } 
      else {
         answer_list_add(alpp, MSG_OBJ_NOSTREEELEM, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DEXIT;
         return -1;
      }
   }

   if (fname) {
      if (!(fp = fopen(fname, "w"))) {
         ERROR((SGE_EVENT, MSG_FILE_NOOPEN_SS, fname, strerror(errno)));
         if (!alpp) {
            SGE_EXIT(1);
         } 
         else {
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DEXIT;
            return -1;
         }
      }
   }
   else
      fp = fpout;

   if (spool && root_node && sge_spoolmsg_write(fp, COMMENT_CHAR,
             feature_get_product_name(FS_VERSION, &ds)) < 0) {
      goto FPRINTF_ERROR;
   }  

   if (recurse) 
      FPRINTF((fp, "id=" sge_u32 "\n", lGetUlong(ep, STN_id)));
  
   if (root_node) {
      id_sharetree((lListElem *)ep, 0);   /* JG: TODO: spooling changes object! */
      if (spool)
         FPRINTF((fp, "version=" sge_u32 "\n", lGetUlong(ep, STN_version)));
   }

   FPRINTF((fp, "name=%s\n", lGetString(ep, STN_name)));
   FPRINTF((fp, "type=" sge_u32 "\n", lGetUlong(ep, STN_type)));
   FPRINTF((fp, "shares=" sge_u32 "\n", lGetUlong(ep, STN_shares)));
   if (recurse) {
      int fret;

      FPRINTF((fp, "childnodes="));
      fret = uni_print_list(fp, NULL, 0, lGetList(ep, STN_children), print_elements, 
                     delis, 0);
      if (fret < 0) {
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DEXIT;
         return -1;
      }
      FPRINTF((fp, "\n"));
   }

   /* if this is a recursive call proceed with the children */
   if (recurse) {
      for_each(cep, lGetList(ep, STN_children)) {     
         if ((i = write_sharetree(alpp, cep, NULL, fp, spool, recurse, 0))) {
            DEXIT;
            return i;
         }
      }
   }

   if (fname) {
      FCLOSE(fp);
   }

   DEXIT;
   return 0;
FPRINTF_ERROR:
   if (fname) {
      FCLOSE(fp); 
   }
   answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
FCLOSE_ERROR:
   DEXIT;
   return -1;
}

/***************************************************
 Read sharetree

 Return 
   NULL if EOF
   NULL and errstr set if error
 ***************************************************/
lListElem *read_sharetree(
char *fname,
FILE *fp,
int spool,      /* from spooled file (may contain additional fields */
char *errstr,
int recurse,
lListElem *rootelem     /* in case of a recursive call this is the root elem
                           of the tree we already read in */
) {
   int print_elements[] = { STN_id, 0 };
   int complete=0;      /* set to 1 when the node is completly read */
   lListElem *ep=NULL, *unspecified;
   lList *child_list;
   static char buf[10000];
   char *name, *val;
   u_long32 id;
   int i, line=0;

   DENTER(TOP_LAYER, "read_sharetree");

   errstr[0] = '\0';

   if (!fp && !(fp = fopen(fname, "r"))) {
      sprintf(errstr, MSG_FILE_NOOPEN_SS, fname, strerror(errno));
      DEXIT;
      return NULL;
   }

   while (!complete && fgets(buf, sizeof(buf), fp)) {
      char *lasts = NULL;
      line++;

      if (buf[0] == '\0' || buf[0] == '#' || buf[0] == '\n')
         continue;

      name = strtok_r(buf, "=\n", &lasts);
      val = strtok_r(NULL, "\n", &lasts);

      if (!strcmp(name, "name")) {
         if (!ep && recurse) {
            sprintf(errstr, MSG_STREE_UNEXPECTEDNAMEFIELD);
            DEXIT;
            return NULL;
         }
         else if (!ep) {
            ep = lCreateElem(STN_Type);
            lSetString(ep, STN_name, val);
            lSetUlong(ep, STN_type, 0);
            lSetUlong(ep, STN_shares, 0);
         }
         else if ((lGetString(ep, STN_name))) {
            sprintf(errstr, MSG_STREE_NAMETWICE_I, line);
            DEXIT;
            return NULL;
         }
         else {
            lSetString(ep, STN_name, val);
         }
      } else if (!strcmp(name, "id")) {
         id = strtol(val, NULL, 10);
         /* if there is a rootelem given we search the tree for our id.
            Our element is already created. */
         if (rootelem) {
            ep = search_nodeSN(rootelem, id);
            if (!ep) {
               sprintf(errstr, MSG_STREE_NOFATHERNODE_U, sge_u32c(id));
               DEXIT;
               return NULL;
            }
         }
         else
            ep = lCreateElem(STN_Type);
         lSetUlong(ep, STN_id, id);
         lSetString(ep, STN_name, NULL);
         lSetUlong(ep, STN_type, 0);
         lSetUlong(ep, STN_shares, 0);
         lSetList(ep, STN_children, NULL); 

      } else if (!strcmp(name, "type")) {
         if (!ep) {
            sprintf(errstr, MSG_STREE_UNEXPECTEDTYPEFIELD);
            DEXIT;
            return NULL;
         }
         if ((lGetUlong(ep, STN_type))) {
            sprintf(errstr, MSG_STREE_TYPETWICE_I, line);
            DEXIT;
            return NULL;
         }
         lSetUlong(ep, STN_type, strtol(val, NULL, 10));
      } else if (!strcmp(name, "version")) {
         if (!rootelem && !spool && !ep) {
            sprintf(errstr, MSG_STREE_UNEXPECTEDVERSIONFIELD);
            DEXIT;
            return NULL;
         }
         if ((lGetUlong(ep, STN_version))) {
            sprintf(errstr, MSG_STREE_TYPETWICE_I, line);
            DEXIT;
            return NULL;
         }
         lSetUlong(ep, STN_version, strtol(val, NULL, 10));
      } else if (!strcmp(name, "shares")) {
         if (!ep) {
            sprintf(errstr, MSG_STREE_UNEXPECTEDSHARESFIELD);
            DEXIT;
            return NULL;
         }
         if ((lGetUlong(ep, STN_shares))) {
            sprintf(errstr, MSG_STREE_SHARESTWICE_I, line);
            DEXIT;
            return NULL;
         }
         lSetUlong(ep, STN_shares, strtol(val, NULL, 10));
      } else if (!strcmp(name, "childnodes")) {
         if (!ep) {
            sprintf(errstr, MSG_STREE_UNEXPECTEDCHILDNODEFIELD);
            DEXIT;
            return NULL;
         }
         if ((lGetList(ep, STN_children))) {
            sprintf(errstr, MSG_STREE_CHILDNODETWICE_I, line);
            DEXIT;
            return NULL;
         }
         complete = 1;
         child_list = NULL;
         i = cull_parse_simple_list(val, &child_list, "childnode list",
                                    STN_Type, print_elements);
         if (i) {
            lFreeElem(&ep);
            strcpy(errstr, MSG_STREE_NOPARSECHILDNODES);
            DEXIT;
            return NULL;
         }
         lSetList(ep, STN_children, child_list);
         
         if (recurse) {
            if (!read_sharetree(NULL, fp, spool, errstr, recurse,
                                rootelem?rootelem:ep) &&
                errstr[0]) {
               if (!rootelem) { /* we are at the root level */
                  lFreeElem(&ep);
               }
               DEXIT;
               return NULL;
            }
         }
      }   
      else {
         /* unknown line */
         sprintf(errstr, MSG_STREE_NOPARSELINE_I, line);
         DEXIT;
         return NULL;
      }
   }

   DTRACE;

   /* check for a reference to a node which is not specified */
   if (!rootelem && (unspecified = sge_search_unspecified_node(ep))) {
      sprintf(errstr, MSG_STREE_NOVALIDNODEREF_U, sge_u32c(lGetUlong(unspecified, STN_id)));

      lFreeElem(&ep);
      DEXIT;
      return NULL;
   }

   if (!rootelem && line<=1) {
      strcpy(errstr, MSG_FILE_FILEEMPTY);
   }

   DEXIT;
   return ep;
}

/********************************************************
 Search a share tree node with a given id in a share tree
 ********************************************************/
static lListElem *search_nodeSN(
lListElem *ep,  /* root of the tree */
u_long32 id 
) {
   lListElem *cep, *fep;

   DENTER(TOP_LAYER, "search_nodeSN");

   if (!ep) {
      DEXIT;
      return NULL;
   }

   if (lGetUlong(ep, STN_id) == id) {
      DEXIT;
      return ep;
   }

   for_each(cep, lGetList(ep, STN_children)) {
      if ((fep = search_nodeSN(cep, id))) {
         DEXIT;
         return fep;
      }
   }
      
   DEXIT;
   return NULL;
}
