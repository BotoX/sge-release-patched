
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

#include <string.h>

#include "sge.h"
#include "sgermon.h"
#include "sge_conf.h"
#include "sge_log.h"
#include "sge_gdi.h"
#include "sge_unistd.h"

#include "sge_answer.h"
#include "sge_centry_qconf.h"
#include "sge_centry.h"
#include "sge_object.h"
#include "sge_io.h"
#include "sge_edit.h"
#include "sge_prog.h"

#include "msg_common.h"
#include "msg_clients_common.h"

#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"

#ifdef TEST_GDI2
#include "sge_gdi_ctx.h"
#endif

static bool 
centry_provide_modify_context(void *context, lListElem **this_elem, lList **answer_list);
static bool 
centry_list_provide_modify_context(void *context, lList **this_list, lList **answer_list);




bool 
centry_add_del_mod_via_gdi(void *context, lListElem *this_elem, lList **answer_list,
                           u_long32 gdi_command)
{
   bool ret = false;
#ifdef TEST_GDI2
   sge_gdi_ctx_class_t *ctx = (sge_gdi_ctx_class_t *)context;
#endif   

   DENTER(TOP_LAYER, "centry_add_del_mod_via_gdi");
   if (this_elem != NULL) {
      lList *centry_list = NULL;
      lList *gdi_answer_list = NULL;

      centry_list = lCreateList("", CE_Type);
      lAppendElem(centry_list, this_elem);
#ifdef TEST_GDI2
      gdi_answer_list = ctx->gdi(ctx, SGE_CENTRY_LIST, gdi_command,
                                &centry_list, NULL, NULL);
#else
      gdi_answer_list = sge_gdi(SGE_CENTRY_LIST, gdi_command,
                                &centry_list, NULL, NULL);
#endif                                
      answer_list_replace(answer_list, &gdi_answer_list);
   }

   DRETURN(ret);
}

lListElem *
centry_get_via_gdi(void *context, lList **answer_list, const char *name) 
{
   lListElem *ret = NULL;
#ifdef TEST_GDI2
   sge_gdi_ctx_class_t *ctx = (sge_gdi_ctx_class_t *)context;
#endif   

   DENTER(TOP_LAYER, "centry_get_via_gdi");
   if (name != NULL) {
      lList *gdi_answer_list = NULL;
      lEnumeration *what = NULL;
      lCondition *where = NULL;
      lList *centry_list = NULL;

      what = lWhat("%T(ALL)", CE_Type);
      where = lWhere("%T(%I==%s)", CE_Type, CE_name, name);
#ifdef TEST_GDI2      
      gdi_answer_list = ctx->gdi(ctx, SGE_CENTRY_LIST, SGE_GDI_GET, 
                                &centry_list, where, what);
#else
      gdi_answer_list = sge_gdi(SGE_CENTRY_LIST, SGE_GDI_GET, 
                                &centry_list, where, what);
#endif                                
      lFreeWhat(&what);
      lFreeWhere(&where);

      if (!answer_list_has_error(&gdi_answer_list)) {
         ret = lFirst(centry_list);
      } else {
         answer_list_replace(answer_list, &gdi_answer_list);
      }
   } 

   DRETURN(ret);
}

static bool 
centry_provide_modify_context(void *context, lListElem **this_elem, lList **answer_list)
{
   bool ret = false;
   int status = 0;
   lList *alp = NULL;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;
   
#ifdef TEST_GDI2
   sge_gdi_ctx_class_t *ctx = (sge_gdi_ctx_class_t*)context;
   uid_t uid = ctx->get_uid(ctx);
   gid_t gid = ctx->get_gid(ctx);
#else
   uid_t uid = uti_state_get_uid();
   gid_t gid = uti_state_get_gid();
#endif   

   DENTER(TOP_LAYER, "centry_provide_modify_context");

   if (this_elem != NULL && *this_elem) {
      const char *filename = NULL;

      filename = spool_flatfile_write_object(&alp, *this_elem, false,
                                             CE_fields, &qconf_ce_sfi, SP_DEST_TMP,
                                             SP_FORM_ASCII, filename, false);
      
      if (answer_list_output(&alp)) {
         unlink(filename);
         FREE(filename);
         DRETURN(false);
      }
 
      status = sge_edit(filename, uid, gid);
      if (status >= 0) {
         lListElem *centry;

         fields_out[0] = NoName;
         centry = spool_flatfile_read_object(&alp, CE_Type, NULL,
                                             CE_fields, fields_out, true, &qconf_ce_sfi,
                                             SP_FORM_ASCII, NULL, filename);
            
         if (answer_list_output(&alp)) {
            lFreeElem(&centry);
         }

         if (centry != NULL) {
            missing_field = spool_get_unprocessed_field(CE_fields, fields_out, &alp);
         }

         if (missing_field != NoName) {
            lFreeElem(&centry);
            answer_list_output (&alp);
         }      

         if (centry != NULL) {
            lFreeElem(this_elem);
            *this_elem = centry; 
            ret = true;
         } else {
            answer_list_add(answer_list, MSG_FILE_ERRORREADINGINFILE,
                            STATUS_ERROR1, ANSWER_QUALITY_ERROR);
         }
      } else {
         answer_list_add(answer_list, MSG_PARSE_EDITFAILED,
                         STATUS_ERROR1, ANSWER_QUALITY_ERROR);
      }
      unlink(filename);
      FREE(filename);
   } 
   
   lFreeList(&alp);

   DRETURN(ret);
}

bool 
centry_add(void *context, lList **answer_list, const char *name) 
{
   bool ret = true;

   DENTER(TOP_LAYER, "centry_add");
   if (name != NULL) {
      lListElem *centry = centry_create(answer_list, name);

      if (centry == NULL) {
         ret = false;
      }
      if (ret) {
         ret &= centry_provide_modify_context(context, &centry, answer_list);
      }
      if (ret) {
         ret &= centry_add_del_mod_via_gdi(context, centry, answer_list, SGE_GDI_ADD); 
      } 
   }  
  
   DRETURN(ret); 
}

bool 
centry_add_from_file(void *context, lList **answer_list, const char *filename) 
{
   bool ret = true;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;

   DENTER(TOP_LAYER, "centry_add_from_file");
   if (filename != NULL) {
      lListElem *centry;

      fields_out[0] = NoName;
      centry = spool_flatfile_read_object(answer_list, CE_Type, NULL,
                                          CE_fields, fields_out, true, &qconf_ce_sfi,
                                          SP_FORM_ASCII, NULL, filename);
            
      if (answer_list_output (answer_list)) {
         lFreeElem(&centry);
      }

      if (centry != NULL) {
         missing_field = spool_get_unprocessed_field(CE_fields, fields_out, answer_list);
      }

      if (missing_field != NoName) {
         lFreeElem(&centry);
         answer_list_output (answer_list);
      }      

      if (centry == NULL) {
         ret = false;
      }
      if (ret) {
         ret &= centry_add_del_mod_via_gdi(context, centry, answer_list, SGE_GDI_ADD); 
      } 
   }  
   
   DRETURN(ret); 
}

bool 
centry_modify(void *context, lList **answer_list, const char *name)
{
   bool ret = true;

   DENTER(TOP_LAYER, "centry_modify");
   if (name != NULL) {
      lListElem *centry = centry_get_via_gdi(context, answer_list, name);

      if (centry == NULL) {
         answer_list_add_sprintf(answer_list, STATUS_ERROR1, 
                                 ANSWER_QUALITY_ERROR, MSG_CENTRY_DOESNOTEXIST_S, name);
         ret = false;
      }
      if (ret) {
         ret &= centry_provide_modify_context(context, &centry, answer_list);
      }
      if (ret) {
         ret &= centry_add_del_mod_via_gdi(context, centry, answer_list, SGE_GDI_MOD);
      }
      if (centry) {
         lFreeElem(&centry);
      }
   }

   DRETURN(ret);
}

bool 
centry_modify_from_file(void *context, lList **answer_list, const char *filename)
{
   bool ret = true;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;

   DENTER(TOP_LAYER, "centry_modify_from_file");
   if (filename != NULL) {
      lListElem *centry;

      fields_out[0] = NoName;
      centry = spool_flatfile_read_object(answer_list, CE_Type, NULL,
                                          CE_fields, fields_out, true, &qconf_ce_sfi,
                                          SP_FORM_ASCII, NULL, filename);
            
      if (answer_list_output(answer_list)) {
         lFreeElem(&centry);
      }

      if (centry != NULL) {
         missing_field = spool_get_unprocessed_field(CE_fields, fields_out, answer_list);
      }

      if (missing_field != NoName) {
         lFreeElem(&centry);
         answer_list_output(answer_list);
      }      

      if (centry == NULL) {
         answer_list_add_sprintf(answer_list, STATUS_ERROR1,
                                 ANSWER_QUALITY_ERROR, MSG_CENTRY_FILENOTCORRECT_S, filename);
         ret = false;
      }
      if (ret) {
         ret &= centry_add_del_mod_via_gdi(context, centry, answer_list, SGE_GDI_MOD);
      }
      if (centry) {
         lFreeElem(&centry);
      }
   }
   
   DRETURN(ret);
}

bool 
centry_delete(void *context, lList **answer_list, const char *name)
{
   bool ret = true;

   DENTER(TOP_LAYER, "centry_delete");
   if (name != NULL) {
      lListElem *centry = centry_create(answer_list, name); 
   
      if (centry != NULL) {
         ret &= centry_add_del_mod_via_gdi(context, centry, answer_list, SGE_GDI_DEL); 
      }
   }

   DRETURN(ret);
}

bool 
centry_show(void *context, lList **answer_list, const char *name)
{
   bool ret = true;

   DENTER(TOP_LAYER, "centry_show");
   if (name != NULL) {
      lListElem *centry = centry_get_via_gdi(context, answer_list, name);
   
      if (centry != NULL) {
         const char *filename;
         filename = spool_flatfile_write_object(answer_list, centry, false, CE_fields,
                                     &qconf_ce_sfi, SP_DEST_STDOUT, SP_FORM_ASCII,
                                     NULL, false);
         FREE(filename);
         lFreeElem(&centry);
         if (answer_list_has_error(answer_list)) {
            DRETURN(false);
         }
      } else {
         answer_list_add_sprintf(answer_list, STATUS_ERROR1, 
                                 ANSWER_QUALITY_ERROR, MSG_CENTRY_DOESNOTEXIST_S, name); 
         ret = false;
      }
   }
   DRETURN(ret);
}

bool
centry_list_show(void *context, lList **answer_list) 
{
   bool ret = true;
   lList *centry_list = NULL;

   DENTER(TOP_LAYER, "centry_list_show");
   centry_list = centry_list_get_via_gdi(context, answer_list);
   if (centry_list != NULL) {
      const char *filename;

      spool_flatfile_align_list(answer_list, (const lList *)centry_list,
                                CE_fields, 3);
      filename = spool_flatfile_write_list(answer_list, centry_list, CE_fields,
                                           &qconf_ce_list_sfi, SP_DEST_STDOUT, SP_FORM_ASCII, 
                                           NULL, false);
     
      FREE(filename);
      lFreeList(&centry_list);

      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   DRETURN(ret);
}

lList *
centry_list_get_via_gdi(void *context, lList **answer_list)
{
   lList *ret = NULL;
   lList *gdi_answer_list = NULL;
   lEnumeration *what = NULL;
#ifdef TEST_GDI2
   sge_gdi_ctx_class_t *ctx = (sge_gdi_ctx_class_t *)context;
#endif

   DENTER(TOP_LAYER, "centry_list_get_via_gdi");
   what = lWhat("%T(ALL)", CE_Type);
#ifdef TEST_GDI2   
   gdi_answer_list = ctx->gdi(ctx, SGE_CENTRY_LIST, SGE_GDI_GET,
                             &ret, NULL, what);
#else
   gdi_answer_list = sge_gdi(SGE_CENTRY_LIST, SGE_GDI_GET,
                             &ret, NULL, what);
#endif                             
   lFreeWhat(&what);

   if (answer_list_has_error(&gdi_answer_list)) {
      answer_list_replace(answer_list, &gdi_answer_list);
   }

   centry_list_sort(ret);

   lFreeList(&gdi_answer_list);

   DRETURN(ret);
}

bool
centry_list_add_del_mod_via_gdi(void *context, lList **this_list, lList **answer_list,
                                lList **old_list) 
{
   bool ret = true;
#ifdef TEST_GDI2
   sge_gdi_ctx_class_t *ctx = (sge_gdi_ctx_class_t *)context;
#endif

   DENTER(TOP_LAYER, "centry_list_add_del_mod_via_gdi");
   if (!this_list || !old_list) {
      SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_INAVLID_PARAMETER_IN_S, SGE_FUNC));
      answer_list_add(answer_list, SGE_EVENT, 
                      STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   
   if (*this_list != NULL) {
      lList *modify_list = NULL;
      lList *add_list = NULL;
      lListElem *centry_elem = NULL;
      lListElem *next_centry_elem = NULL;
      bool cont = true;

      /* check for duplicate names */
      next_centry_elem = lFirst(*this_list);
      while ((centry_elem = next_centry_elem)) {
         lListElem *cmp_elem = lFirst(*this_list);
            
         while((centry_elem != cmp_elem)){
            const char *name1 = NULL;
            const char *name2 = NULL;
            const char *shortcut1 = NULL;
            const char *shortcut2 = NULL;

            /* Bugfix: Issuezilla 1161
             * Previously it was assumed that name and shortcut would never be
             * NULL.  In the course of testing for duplicate names, each name
             * would potentially be accessed twice.  Now, in order to check for
             * NULL without making a mess, I access each name exactly once.  In
             * some cases, that may be 50% more than the previous code, but in
             * what I expect is the usual case, it will be 50% less. */
            name1 = lGetString(centry_elem, CE_name);
            name2 = lGetString(cmp_elem, CE_name);
            shortcut1 = lGetString(centry_elem, CE_shortcut);
            shortcut2 = lGetString(cmp_elem, CE_shortcut);
            
            if (name1 == NULL) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                       ANSWER_QUALITY_ERROR,
                                       MSG_CENTRY_NULL_NAME);
               DRETURN(false);
            }                  
            else if (name2 == NULL) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                       ANSWER_QUALITY_ERROR,
                                       MSG_CENTRY_NULL_NAME);
               DRETURN(false);
            }                  
            else if (shortcut1 == NULL) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                       ANSWER_QUALITY_ERROR,
                                       MSG_CENTRY_NULL_SHORTCUT_S,
                                       name1);
               DRETURN(false);
            }                  
            else if (shortcut2 == NULL) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                       ANSWER_QUALITY_ERROR,
                                       MSG_CENTRY_NULL_SHORTCUT_S,
                                       name2);
               DRETURN(false);
            }                  
            else if ((strcmp (name1, name2) == 0) ||
                     (strcmp(name1, shortcut2) == 0) ||
                     (strcmp(shortcut1, name2) == 0) ||
                     (strcmp(shortcut1, shortcut2) == 0)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                       ANSWER_QUALITY_ERROR,
                                       MSG_ANSWER_COMPLEXXALREADYEXISTS_SS,
                                       name1, shortcut1);
               cont = false;
            } 
            
            cmp_elem = lNext(cmp_elem);
         }
         
         if (!centry_elem_validate(centry_elem, NULL, answer_list)){
            cont = false;
         }

         next_centry_elem = lNext(centry_elem);
      }
     
      if (!cont) {
         DRETURN(false);
      }

      modify_list = lCreateList("", CE_Type);
      add_list = lCreateList("", CE_Type);
      next_centry_elem = lFirst(*this_list);
      while ((centry_elem = next_centry_elem)) {
         const char *name = lGetString(centry_elem, CE_name);
         lListElem *tmp_elem = centry_list_locate(*old_list, name);

         next_centry_elem = lNext(centry_elem);
         if (tmp_elem != NULL) {
            lDechainElem(*this_list, centry_elem);
            if (object_has_differences(centry_elem, NULL, tmp_elem, false)) {
               lAppendElem(modify_list, centry_elem);
            } else {
               lFreeElem(&centry_elem);
            }
            lRemoveElem(*old_list, &tmp_elem);
         } else {
            lDechainElem(*this_list, centry_elem);
            lAppendElem(add_list, centry_elem);
         }
      }
      lFreeList(this_list);
      
      {
         lList *gdi_answer_list = NULL;
         lList *mal_answer_list = NULL;
         state_gdi_multi state = STATE_GDI_MULTI_INIT;
         int del_id = -1;
         int mod_id = -1;
         int add_id = -1; 
         int number_req = 0;
         bool do_del = false;
         bool do_mod = false;
         bool do_add = false;

         /*
          * How many requests are summarized to one multi request? 
          */
         if (lGetNumberOfElem(*old_list) > 0) {
            number_req++;
            do_del = true;
         }
         if (lGetNumberOfElem(modify_list) > 0) {
            number_req++;
            do_mod = true;
         }
         if (lGetNumberOfElem(add_list) > 0) {
            number_req++;
            do_add = true;
         }

         /*
          * Do the multi request
          */
         if (ret && do_del) {
            int mode = (--number_req > 0) ? SGE_GDI_RECORD : SGE_GDI_SEND;
#ifdef TEST_GDI2
            del_id = ctx->gdi_multi(ctx, &gdi_answer_list, mode, 
                                   SGE_CENTRY_LIST, SGE_GDI_DEL, old_list,
                                   NULL, NULL, &mal_answer_list, &state, false);
#else
            del_id = sge_gdi_multi(&gdi_answer_list, mode, 
                                   SGE_CENTRY_LIST, SGE_GDI_DEL, old_list,
                                   NULL, NULL, &mal_answer_list, &state, false);
#endif                                   
            if (answer_list_has_error(&gdi_answer_list)) {
               answer_list_append_list(answer_list, &gdi_answer_list);
               DTRACE;
               ret = false;
            }
            lFreeList(old_list);
         }
         if (ret && do_mod) {
            int mode = (--number_req > 0) ? SGE_GDI_RECORD : SGE_GDI_SEND;
#ifdef TEST_GDI2
            mod_id = ctx->gdi_multi(ctx, &gdi_answer_list, mode, 
                                   SGE_CENTRY_LIST, SGE_GDI_MOD, &modify_list,
                                   NULL, NULL, &mal_answer_list, &state, false);
#else
            mod_id = sge_gdi_multi(&gdi_answer_list, mode, 
                                   SGE_CENTRY_LIST, SGE_GDI_MOD, &modify_list,
                                   NULL, NULL, &mal_answer_list, &state, false);
#endif
            if (answer_list_has_error(&gdi_answer_list)) {
               answer_list_append_list(answer_list, &gdi_answer_list);
               DTRACE;
               ret = false;
            }
            lFreeList(&modify_list);
         }
         if (ret && do_add) {
            int mode = (--number_req > 0) ? SGE_GDI_RECORD : SGE_GDI_SEND;
#ifdef TEST_GDI2
            add_id = ctx->gdi_multi(ctx, &gdi_answer_list, mode, 
                                   SGE_CENTRY_LIST, SGE_GDI_ADD, &add_list,
                                   NULL, NULL, &mal_answer_list, &state, false);
#else
            add_id = sge_gdi_multi(&gdi_answer_list, mode, 
                                   SGE_CENTRY_LIST, SGE_GDI_ADD, &add_list,
                                   NULL, NULL, &mal_answer_list, &state, false);
#endif
            if (answer_list_has_error(&gdi_answer_list)) {
               answer_list_append_list(answer_list, &gdi_answer_list);
               DTRACE;
               ret = false;
            }
            lFreeList(&add_list);
         }

         /*
          * Verify that the parts of the multi request are successfull
          */
         if (do_del && ret) {
            sge_gdi_extract_answer(&gdi_answer_list, SGE_GDI_DEL, 
                                                     SGE_CENTRY_LIST, del_id,
                                                     mal_answer_list, NULL);
            answer_list_append_list(answer_list, &gdi_answer_list);
         }
         if (do_mod && ret) {
            sge_gdi_extract_answer(&gdi_answer_list, SGE_GDI_MOD, 
                                                     SGE_CENTRY_LIST, mod_id,
                                                     mal_answer_list, NULL);
            answer_list_append_list(answer_list, &gdi_answer_list);
         }
         if (do_add && ret) {
            sge_gdi_extract_answer(&gdi_answer_list, SGE_GDI_ADD, 
                                                     SGE_CENTRY_LIST, add_id,
                                                     mal_answer_list, NULL);
            answer_list_append_list(answer_list, &gdi_answer_list);
         }

         /*
          * Provide an overall summary for the callee
          */
         if (lGetNumberOfElem(*answer_list) == 0) {
            answer_list_add_sprintf(answer_list, STATUS_OK, 
                                    ANSWER_QUALITY_INFO, 
                                    MSG_CENTRY_NOTCHANGED);
         }

         lFreeList(&gdi_answer_list);
         lFreeList(&mal_answer_list);
      }

      lFreeList(&add_list);
      lFreeList(&modify_list);
   }

   DRETURN(ret);    
}

bool 
centry_list_modify(void *context, lList **answer_list)
{
   bool ret = true;

   DENTER(TOP_LAYER, "centry_list_modify");
   if (ret) {
      lList *centry_list = centry_list_get_via_gdi(context, answer_list);
      lList *old_centry_list = lCopyList("", centry_list);

      ret &= centry_list_provide_modify_context(context, &centry_list, answer_list);
      if (ret) {
         ret &= centry_list_add_del_mod_via_gdi(context, &centry_list, answer_list, &old_centry_list);  
      }
      
      lFreeList(&centry_list);
      lFreeList(&old_centry_list);
   }
   DRETURN(ret);
}

bool 
centry_list_modify_from_file(void *context, lList **answer_list, const char *filename)
{
   bool ret = true;

   DENTER(TOP_LAYER, "centry_list_modify_from_file");
   if (ret) {
      lList *old_centry_list = NULL; 
      lList *centry_list = NULL; 
      
      centry_list = spool_flatfile_read_list(answer_list, CE_Type, CE_fields,
                                             NULL, true, &qconf_ce_list_sfi,
                                             SP_FORM_ASCII, NULL, filename);
            
      if (answer_list_output (answer_list)) {
         lFreeList(&centry_list);
      }

      old_centry_list = centry_list_get_via_gdi(context, answer_list); 

      if (centry_list == NULL) {
         answer_list_add_sprintf(answer_list, STATUS_ERROR1, 
                                 ANSWER_QUALITY_ERROR, 
                                 MSG_CENTRY_FILENOTCORRECT_S, filename);
         ret = false;
      } 
      if (ret) { 
         ret &= centry_list_add_del_mod_via_gdi(context, &centry_list, answer_list, 
                                                &old_centry_list);  
      }

      lFreeList(&centry_list);
      lFreeList(&old_centry_list);
   }
   DRETURN(ret);
}

static bool 
centry_list_provide_modify_context(void *context,
                                   lList **this_list, 
                                   lList **answer_list)
{
   bool ret = false;
   int status = 0;
   
#ifdef TEST_GDI2
   sge_gdi_ctx_class_t *ctx = (sge_gdi_ctx_class_t*)context;
   uid_t uid = ctx->get_uid(ctx);
   gid_t gid = ctx->get_gid(ctx);
#else
   uid_t uid = uti_state_get_uid();
   gid_t gid = uti_state_get_gid();
#endif   

   DENTER(TOP_LAYER, "centry_list_provide_modify_context");
   if (this_list != NULL) {
      const char *filename;

      spool_flatfile_align_list(answer_list, (const lList *)*this_list,
                                CE_fields, 3);
      filename = spool_flatfile_write_list(answer_list, *this_list, CE_fields,
                                           &qconf_ce_list_sfi, SP_DEST_TMP,
                                           SP_FORM_ASCII, NULL, false);
      
      if (answer_list_output(answer_list)) {
         unlink(filename);
         FREE(filename);
         DRETURN(false);
      }

      status = sge_edit(filename, uid, gid);
      if (status >= 0) {
         lList *centry_list;

         centry_list = spool_flatfile_read_list(answer_list, CE_Type, CE_fields,
                                                NULL, true, &qconf_ce_list_sfi,
                                                SP_FORM_ASCII, NULL, filename);
            
         if (answer_list_output (answer_list)) {
            lFreeList(&centry_list);
         }

         if (centry_list != NULL) {
            lFreeList(this_list);
            *this_list = centry_list; 
            ret = true;
         } else {
            answer_list_add(answer_list, MSG_FILE_ERRORREADINGINFILE,
                            STATUS_ERROR1, ANSWER_QUALITY_ERROR);
         }
      } else {
         answer_list_add(answer_list, MSG_PARSE_EDITFAILED,
                         STATUS_ERROR1, ANSWER_QUALITY_ERROR);
      }
      unlink(filename);
      FREE(filename);
   } 
   DRETURN(ret);
}
