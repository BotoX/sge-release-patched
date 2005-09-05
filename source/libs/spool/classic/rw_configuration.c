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
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "sgermon.h"
#include "sge_gdi_request.h"
#include "rw_configuration.h"
#include "sge_log.h"
#include "sge_stdio.h"
#include "sge_feature.h" 
#include "sge_unistd.h"
#include "sge_spool.h"
#include "sge_answer.h"
#include "sge_range.h"
#include "sge_conf.h"
#include "msg_common.h"

/*-----------------------------------------------------------------------*
 * write_configuration
 *  write configuration in human readable (and editable) form to file or 
 *  fileptr. If fname == NULL use fpout for writing.
 * if conf_list = NULL use config for writing
 *-----------------------------------------------------------------------*/
int write_configuration(
int spool,
lList **alpp,
const char *fname,
const lListElem *epc,
FILE *fpout,
u_long32 flags 
) {
   FILE *fp;
   lListElem *ep = NULL;
   lList *cfl;
   dstring ds;
   char buffer[256];
   
   DENTER(TOP_LAYER, "write_configuration");

   sge_dstring_init(&ds, buffer, sizeof(buffer));
   if (fname) {
      if (!(fp = fopen(fname, "w"))) {
         ERROR((SGE_EVENT, MSG_FILE_NOOPEN_SS, fname, 
                strerror(errno)));
         if (!alpp) {
            SGE_EXIT(1);
         } else {
            answer_list_add(alpp, SGE_EVENT, 
                            STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DEXIT;
            return -1;  
         }
      }
   }
   else
      fp = fpout;

   if (spool && sge_spoolmsg_write(fp, COMMENT_CHAR,
             feature_get_product_name(FS_VERSION, &ds)) < 0) {
      goto FPRINTF_ERROR;
   } 

   if (flags & FLG_CONF_SPOOL) {
      FPRINTF((fp, "%-25s " sge_u32"\n", "conf_version", 
            lGetUlong(epc, CONF_version)));
      DPRINTF(("writing conf %s version " sge_u32 "\n", fname, 
               lGetUlong(epc, CONF_version)));
   }

   cfl = lGetList(epc, CONF_entries);

   for_each(ep, cfl) {
      if (strcmp( lGetString(ep, CF_name), REPRIORITIZE) == 0)
         continue;

      FPRINTF((fp, "%-25s %s\n", lGetString(ep, CF_name), 
               lGetString(ep, CF_value)));
   }

   if (fname) {
      fclose(fp);
   }

   DEXIT;
   return 0;

FPRINTF_ERROR:
   DEXIT;
   return -1;
}

/*---------------------------------------------------------
 * read_configuration()
 * read configuration from file
 *
 * returns a CONF_Type list.
 *
 * fname: File which is read
 * conf_name: Internal name of configuration
 *---------------------------------------------------------*/
lListElem *read_configuration(
const char *fname,
const char *conf_name,
u_long32 flags 
) {
   FILE *fp;
   lListElem *ep=NULL, *epc;
   char buf[512], *value, *name;
   lList *lp;
   int first_line = 1;
   u_long32 conf_version = 0L;
   lList *alp = NULL;
   lList *rlp = NULL;

   DENTER(TOP_LAYER, "read_configuration");

   if (!(fp = fopen(fname, "r"))) {
      WARNING((SGE_EVENT, MSG_CONFIG_CONF_ERROROPENINGSPOOLFILE_SS, fname, strerror(errno)));
      DEXIT;
      return NULL;
   }
   
   lp = NULL;
   while (fgets(buf, sizeof(buf), fp)) {
      char *lasts = NULL;
      /* set chrptr to the first non blank character
       * If line is empty continue with next line   
       * sge_strtok() led to a error here because of recursive use
       * in parse_qconf.c
       */
      if(!(name = strtok_r(buf, " \t\n", &lasts)))
         continue;

      /* allow commentaries */
      if (name[0] == '#')
         continue;

      if ((flags & FLG_CONF_SPOOL) && first_line) {
         first_line = 0;
         if (!strcmp(name, "conf_version")) {
            if (!(value = strtok_r(NULL, " \t\n", &lasts))) {
               WARNING((SGE_EVENT, MSG_CONFIG_CONF_NOVALUEFORCONFIGATTRIB_S, name));
               fclose(fp);
               DEXIT;
               return NULL;
            }
            sscanf(value, sge_u32, &conf_version);
            DPRINTF(("read conf %s version " sge_u32"\n", conf_name, conf_version));
            continue;
         } else {
            WARNING((SGE_EVENT, MSG_CONFIG_CONF_VERSIONNOTFOUNDONREADINGSPOOLFILE));
         }   
      }

      ep = lAddElemStr(&lp, CF_name, name, CF_Type);
      if (!ep) {
         WARNING((SGE_EVENT, MSG_CONFIG_CONF_ERRORSTORINGCONFIGVALUE_S, name));
         lFreeList(&lp);
         fclose(fp);
         DEXIT;
         return NULL;
      }

      /* validate input */
      if (!strcmp(name, "gid_range")) {
         if ((value= strtok_r(NULL, " \t\n", &lasts))) {
            if (!strcmp(value, "none") ||
                !strcmp(value, "NONE")) {
               lSetString(ep, CF_value, value);
            } else {
               range_list_parse_from_string(&rlp, &alp, value, 
                                            false, false, INF_NOT_ALLOWED);
               if (rlp == NULL) {
                  WARNING((SGE_EVENT, MSG_CONFIG_CONF_INCORRECTVALUEFORCONFIGATTRIB_SS, 
                           name, value));
                  lFreeList(&alp);
                  lFreeList(&lp);
                  fclose(fp);
                  DEXIT;
                  return NULL;
               } else {
                  lListElem *rep;

                  for_each (rep, rlp) {
                     u_long32 min;

                     min = lGetUlong(rep, RN_min);
                     if (min < GID_RANGE_NOT_ALLOWED_ID) {
                        WARNING((SGE_EVENT, MSG_CONFIG_CONF_GIDRANGELESSTHANNOTALLOWED_I, GID_RANGE_NOT_ALLOWED_ID));
                        lFreeList(&alp);
                        lFreeList(&lp);
                        fclose(fp);
                        DEXIT;
                        return (NULL);
                     }                  
                  }
                  lFreeList(&alp);
                  lFreeList(&rlp);
                  lSetString(ep, CF_value, value);
               }
            }
         }
      } 
      else if (!strcmp(name, "admin_user")) {
         value = strtok_r(NULL, " \t\n", &lasts);
         while (value[0] && isspace((int) value[0]))
            value++;
         if (value) {
            lSetString(ep, CF_value, value);
         } else {
            WARNING((SGE_EVENT, MSG_CONFIG_CONF_NOVALUEFORCONFIGATTRIB_S, name));
            lFreeList(&lp);
            fclose(fp);
            DEXIT;
            return NULL;
         }
      } 
      else if (!strcmp(name, "user_lists") || 
         !strcmp(name, "xuser_lists") || 
         !strcmp(name, "projects") || 
         !strcmp(name, "xprojects") || 
         !strcmp(name, "prolog") || 
         !strcmp(name, "epilog") || 
         !strcmp(name, "starter_method") || 
         !strcmp(name, "suspend_method") || 
         !strcmp(name, "resume_method") || 
         !strcmp(name, "terminate_method") || 
         !strcmp(name, "qmaster_params") || 
         !strcmp(name, "execd_params") || 
         !strcmp(name, "reporting_params") || 
         !strcmp(name, "qlogin_command") ||
         !strcmp(name, "rlogin_command") ||
         !strcmp(name, "rsh_command") ||
         !strcmp(name, "qlogin_daemon") ||
         !strcmp(name, "rlogin_daemon") ||
         !strcmp(name, "rsh_daemon")) {
         if (!(value = strtok_r(NULL, "\t\n", &lasts))) {
            /* return line if value is empty */
            WARNING((SGE_EVENT, MSG_CONFIG_CONF_NOVALUEFORCONFIGATTRIB_S, name));
            lFreeList(&lp);
            fclose(fp);
            DEXIT;
            return NULL;
         }
         /* skip leading delimitors */
         while (value[0] && isspace((int) value[0]))
            value++;

         lSetString(ep, CF_value, value);
      } else {
         if (!(value = strtok_r(NULL, " \t\n", &lasts))) {
            WARNING((SGE_EVENT, MSG_CONFIG_CONF_NOVALUEFORCONFIGATTRIB_S, name));
            lFreeList(&lp);
            fclose(fp);
            DEXIT;
            return NULL;
         }
           
         lSetString(ep, CF_value, value);

         if (strtok_r(NULL, " \t\n", &lasts)) {
            /* Allow only one value per line */
            WARNING((SGE_EVENT, MSG_CONFIG_CONF_ONLYSINGLEVALUEFORCONFIGATTRIB_S, name));
            fclose(fp);
            DEXIT;
            return NULL;
         }
      }
   }

   fclose(fp);

   epc = lCreateElem(CONF_Type);
   lSetHost(epc, CONF_hname, conf_name);
   lSetUlong(epc, CONF_version, conf_version);
   lSetList(epc, CONF_entries, lp);

   DEXIT;
   return epc;
}
