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

#include <fnmatch.h>
#include <string.h>

#include "sge.h"
#include "sgermon.h"
#include "sge_conf.h"
#include "sge_log.h"
#include "sge_gdi.h"
#include "sge_unistd.h"
#include "sge_string.h"
#include "sge_hostname.h"

#include "sge_attr.h"
#include "sge_answer.h"
#include "sge_cqueue.h"
#include "sge_feature.h"
#include "sge_object.h"
#include "sge_qinstance.h"
#include "sge_hgroup.h"
#include "sge_href.h"
#include "sge_qref.h"
#include "sge_edit.h"
#include "sge_cqueue_qconf.h"

#include "msg_common.h"
#include "msg_clients_common.h"

#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_subordinate.h"

static void insert_custom_complex_values_writer(spooling_field *fields);
static int write_QU_consumable_config_list(const lListElem *ep, int nm,
                                           dstring *buffer, lList **alp);

/* Bugfix: Issuezilla #1198
 * In order to remove slots from the complex values list, we use a custom writer
 * for the QU_consumable_config_list field.  This writer is defined in
 * write_QU_consumable_config_list() and tries to mimic the format specified by
 * the cqqconf_sfi spooling instruction.  If either the
 * cqqconf_sub_name_value_space_sfi spooling instruction or the cqqconf_sfi
 * subinstruction field changes, the write_QU_consumable_config_list() function
 * will have to be changed to match. */
static const spool_flatfile_instr cqqconf_sub_name_value_space_sfi = 
{
   NULL,
   false,
   false,
   false,
   false,
   false,
   '\0',
   '=',
   ' ',
   '\0',
   '\0',
   &cqqconf_sub_name_value_space_sfi,
   { NoName, NoName, NoName }
};

static const spool_flatfile_instr cqqconf_sfi = 
{
   NULL,
   true,
   false,
   false,
   true,
   false,
   ' ',
   '\n',
   '\0',
   '\0',
   '\0',
   &cqqconf_sub_name_value_space_sfi,
   { NoName, NoName, NoName }
};

bool
cqueue_add_del_mod_via_gdi(lListElem *this_elem, lList **answer_list,
                           u_long32 gdi_command)
{
   bool ret = true;

   DENTER(TOP_LAYER, "cqueue_add_del_mod_via_gdi");
   if (this_elem != NULL) {
      u_long32 operation = SGE_GDI_GET_OPERATION(gdi_command);
      bool do_verify = (operation == SGE_GDI_MOD) || (operation == SGE_GDI_ADD);

      if (do_verify) {
         ret &= cqueue_verify_attributes(this_elem, answer_list,
                                         this_elem, false);
      }
      if (ret) {
         lList *cqueue_list = NULL;
         lList *gdi_answer_list = NULL;

         cqueue_list = lCreateList("", CQ_Type);
         lAppendElem(cqueue_list, this_elem);

         gdi_answer_list = sge_gdi(SGE_CQUEUE_LIST, gdi_command,
                                   &cqueue_list, NULL, NULL);
         answer_list_replace(answer_list, &gdi_answer_list);
      }
   }
   DEXIT;
   return ret;
}

lListElem *
cqueue_get_via_gdi(lList **answer_list, const char *name) 
{
   lListElem *ret = NULL;

   DENTER(TOP_LAYER, "cqueue_get_via_gdi");
   if (name != NULL) {
      lList *gdi_answer_list = NULL;
      lEnumeration *what = NULL;
      lCondition *where = NULL;
      lList *cqueue_list = NULL;

      what = lWhat("%T(ALL)", CQ_Type);
      where = lWhere("%T(%I==%s)", CQ_Type, CQ_name, name);
      gdi_answer_list = sge_gdi(SGE_CQUEUE_LIST, SGE_GDI_GET, 
                                &cqueue_list, where, what);
      what = lFreeWhat(what);
      where = lFreeWhere(where);

      if (!answer_list_has_error(&gdi_answer_list)) {
         ret = lFirst(cqueue_list);
      } else {
         answer_list_replace(answer_list, &gdi_answer_list);
      }
   } 
   DEXIT;
   return ret;
}

bool
cqueue_hgroup_get_via_gdi(lList **answer_list, const lList *qref_list,
                          lList **hgrp_list, lList **cq_list)
{
   bool ret = true;

   DENTER(TOP_LAYER, "cqueue_hgroup_get_via_gdi");
   if (hgrp_list != NULL && cq_list != NULL) {
      state_gdi_multi state = STATE_GDI_MULTI_INIT;
      lList *multi_answer_list = NULL;
      lCondition *cqueue_where = NULL;
      lListElem *qref = NULL;
      bool fetch_all_hgroup = false;
      bool fetch_all_qi = false;
      bool fetch_all_nqi = false;
      int hgrp_id = 0; 
      int cq_id = 0;

      for_each(qref, qref_list) {
         dstring cqueue_name = DSTRING_INIT;
         dstring host_domain = DSTRING_INIT;
         const char *name = lGetString(qref, QR_name);
         bool has_hostname, has_domain;
         lCondition *add_cqueue_where = NULL;
         const char* cqueue_name_str = NULL;
         
         cqueue_name_split(name, &cqueue_name, &host_domain,
                           &has_hostname, &has_domain);

         fetch_all_hgroup = fetch_all_hgroup || has_domain;
         fetch_all_qi = fetch_all_qi || (has_domain || has_hostname);
         fetch_all_nqi = fetch_all_nqi || (!has_domain && !has_hostname);

         cqueue_name_str = sge_dstring_get_string(&cqueue_name);
         
         if (cqueue_name_str == NULL) {
            char buffer[MAX_STRING_SIZE];
            
            sprintf (buffer, MSG_CQUEUE_NAMENOTCORRECT_SS, name, name);
            answer_list_add (answer_list, buffer, STATUS_ESYNTAX,
                             ANSWER_QUALITY_ERROR);
            ret = false;
            break;
         }
         
         add_cqueue_where = lWhere("%T(%I p= %s)", CQ_Type, CQ_name, 
                                   sge_dstring_get_string(&cqueue_name));
         if (cqueue_where == NULL) {
            cqueue_where = add_cqueue_where;
         } else {
            cqueue_where = lOrWhere(cqueue_where, add_cqueue_where);   
         }
         sge_dstring_free(&cqueue_name);
         sge_dstring_free(&host_domain);
      }
      if (ret && fetch_all_hgroup) { 
         lEnumeration *what = lWhat("%T(ALL)", HGRP_Type);
        
         hgrp_id = sge_gdi_multi(answer_list, SGE_GDI_RECORD, SGE_HGROUP_LIST, 
                                 SGE_GDI_GET, NULL, NULL, what, NULL, &state, true);
         what = lFreeWhat(what);
      }  
      if (ret) {
         lEnumeration *what; 
         
         what = enumeration_create_reduced_cq(fetch_all_qi, fetch_all_nqi);
         cq_id = sge_gdi_multi(answer_list, SGE_GDI_SEND, SGE_CQUEUE_LIST,
                               SGE_GDI_GET, NULL, cqueue_where, what,
                               &multi_answer_list, &state, true);
         what = lFreeWhat(what);
      }
      if (ret && fetch_all_hgroup) {
         lList *local_answer_list = NULL;
         
         local_answer_list = sge_gdi_extract_answer(SGE_GDI_GET, 
                      SGE_HGROUP_LIST, hgrp_id, multi_answer_list, hgrp_list);
         if (local_answer_list != NULL) {
            lListElem *answer = lFirst(local_answer_list);

            if (lGetUlong(answer, AN_status) != STATUS_OK) {
               lDechainElem(local_answer_list, answer);
               answer_list_add_elem(answer_list, answer);
               ret = false;
            }
         } 
         lFreeList(local_answer_list);
      }  
      if (ret) {
         lList *local_answer_list = NULL;
         
         local_answer_list = sge_gdi_extract_answer(SGE_GDI_GET, 
                      SGE_CQUEUE_LIST, cq_id, multi_answer_list, cq_list);
         if (local_answer_list != NULL) {
            lListElem *answer = lFirst(local_answer_list);

            if (lGetUlong(answer, AN_status) != STATUS_OK) {
               lDechainElem(local_answer_list, answer);
               answer_list_add_elem(answer_list, answer);
               ret = false;
            }
         } 
         local_answer_list = lFreeList(local_answer_list);
      }
      multi_answer_list = lFreeList(multi_answer_list);
      cqueue_where = lFreeWhere(cqueue_where);
   }
   DEXIT;
   return ret;
}

bool
cqueue_hgroup_get_all_via_gdi(lList **answer_list,
                              lList **hgrp_list, lList **cq_list)
{
   bool ret = true;

   DENTER(TOP_LAYER, "cqueue_hgroup_get_all_via_gdi");
   if (hgrp_list != NULL && cq_list != NULL) {
      state_gdi_multi state = STATE_GDI_MULTI_INIT;
      lEnumeration *hgrp_what = NULL; 
      lEnumeration *cqueue_what = NULL;
      int hgrp_id = 0; 
      int cq_id = 0;
      lList *local_answer_list = NULL;
      lList *multi_answer_list = NULL;

      /* HGRP */
      hgrp_what = lWhat("%T(ALL)", HGRP_Type);
      hgrp_id = sge_gdi_multi(answer_list, SGE_GDI_RECORD, SGE_HGROUP_LIST,
                              SGE_GDI_GET, NULL, NULL, hgrp_what, NULL, &state, true);
      hgrp_what = lFreeWhat(hgrp_what);

      /* CQ */
      cqueue_what = lWhat("%T(ALL)", CQ_Type);
      cq_id = sge_gdi_multi(answer_list, SGE_GDI_SEND, SGE_CQUEUE_LIST,
                            SGE_GDI_GET, NULL, NULL, cqueue_what,
                            &multi_answer_list, &state, true);
      cqueue_what = lFreeWhat(cqueue_what);

      /* HGRP */
      local_answer_list = sge_gdi_extract_answer(SGE_GDI_GET,
                      SGE_HGROUP_LIST, hgrp_id, multi_answer_list, hgrp_list);
      if (local_answer_list != NULL) {
         lListElem *answer = lFirst(local_answer_list);

         if (lGetUlong(answer, AN_status) != STATUS_OK) {
            lDechainElem(local_answer_list, answer);
            answer_list_add_elem(answer_list, answer);
            ret = false;
         }
      }
      local_answer_list = lFreeList(local_answer_list);
      
      /* CQ */   
      local_answer_list = sge_gdi_extract_answer(SGE_GDI_GET, 
                   SGE_CQUEUE_LIST, cq_id, multi_answer_list, cq_list);
      if (local_answer_list != NULL) {
         lListElem *answer = lFirst(local_answer_list);

         if (lGetUlong(answer, AN_status) != STATUS_OK) {
            lDechainElem(local_answer_list, answer);
            answer_list_add_elem(answer_list, answer);
            ret = false;
         }
      } 
      local_answer_list = lFreeList(local_answer_list);
      multi_answer_list = lFreeList(multi_answer_list);
   }
   DEXIT;
   return ret;
}

bool 
cqueue_provide_modify_context(lListElem **this_elem, lList **answer_list,
                              bool ignore_unchanged_message)
{
   bool ret = false;
   int status = 0;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;
   
   DENTER(TOP_LAYER, "cqueue_provide_modify_context");
   if (this_elem != NULL && *this_elem) {
      char *filename = NULL;      
      filename = (char *)spool_flatfile_write_object(answer_list, *this_elem,
                                                     false, CQ_fields,
                                                     &cqqconf_sfi, SP_DEST_TMP,
                                                     SP_FORM_ASCII, filename,
                                                     false);
      
      if (answer_list_output(answer_list)) {
         DEXIT;
         SGE_EXIT(1);
      }
 
      status = sge_edit(filename);
      if (status >= 0) {
         lListElem *cqueue;

         fields_out[0] = NoName;
         cqueue = spool_flatfile_read_object(answer_list, CQ_Type, NULL,
                                             CQ_fields, fields_out, false, &cqqconf_sfi,
                                             SP_FORM_ASCII, NULL, filename);
      
         if (answer_list_output(answer_list)) {
            cqueue = lFreeElem (cqueue);
         }

         if (cqueue != NULL) {
            missing_field = spool_get_unprocessed_field (CQ_fields, fields_out, answer_list);
         }

         if (missing_field != NoName) {
            cqueue = lFreeElem (cqueue);
            answer_list_output (answer_list);
         }

         if (cqueue != NULL) {
            if (object_has_differences(*this_elem, answer_list, 
                                       cqueue, false) ||
                ignore_unchanged_message) {
               *this_elem = lFreeElem(*this_elem);
               *this_elem = cqueue; 
               ret = true;
            } else {
               answer_list_add(answer_list, MSG_FILE_NOTCHANGED,
                               STATUS_ERROR1, ANSWER_QUALITY_ERROR);
            }
         } else {
            answer_list_add(answer_list, MSG_FILE_ERRORREADINGINFILE,
                            STATUS_ERROR1, ANSWER_QUALITY_ERROR);
         }
      } else {
         answer_list_add(answer_list, MSG_PARSE_EDITFAILED,
                         STATUS_ERROR1, ANSWER_QUALITY_ERROR);
      }

      unlink(filename);
   } 
   DEXIT;
   return ret;
}

bool 
cqueue_add(lList **answer_list, const char *name) 
{
   bool ret = true;

   DENTER(TOP_LAYER, "cqueue_add");
   if (name != NULL) {
      lListElem *cqueue = cqueue_create(answer_list, name);

      if (cqueue == NULL) {
         ret = false;
      }
      if (ret) {
         ret &= cqueue_set_template_attributes(cqueue, answer_list);
      }
      if (ret) {
         ret &= cqueue_provide_modify_context(&cqueue, answer_list, true);
      }
      if (ret) {
         ret &= cqueue_add_del_mod_via_gdi(cqueue, answer_list, 
                                           SGE_GDI_ADD | SGE_GDI_SET_ALL); 
      } 
   }  
  
   DEXIT;
   return ret; 
}

bool 
cqueue_add_from_file(lList **answer_list, const char *filename) 
{
   bool ret = true;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;

   DENTER(TOP_LAYER, "cqueue_add_from_file");
   if (filename != NULL) {
      lListElem *cqueue;

      fields_out[0] = NoName;
      cqueue = spool_flatfile_read_object(answer_list, CQ_Type, NULL,
                                          CQ_fields, fields_out, false, &cqqconf_sfi,
                                          SP_FORM_ASCII, NULL, filename);
            
      if (answer_list_output(answer_list)) {
         cqueue = lFreeElem (cqueue);
      }

      if (cqueue != NULL) {
         missing_field = spool_get_unprocessed_field (CQ_fields, fields_out, answer_list);
      }

      if (missing_field != NoName) {
         cqueue = lFreeElem(cqueue);
         answer_list_output(answer_list);
      }

      if (cqueue == NULL) {
         ret = false;
      }
      if (ret) {
         ret &= cqueue_add_del_mod_via_gdi(cqueue, answer_list, 
                                           SGE_GDI_ADD | SGE_GDI_SET_ALL); 
      } 
   }  
  
   DEXIT;
   return ret; 
}

bool 
cqueue_modify(lList **answer_list, const char *name)
{
   bool ret = true;

   DENTER(TOP_LAYER, "cqueue_modify");
   if (name != NULL) {
      lListElem *cqueue = cqueue_get_via_gdi(answer_list, name);

      if (cqueue == NULL) {
         sprintf(SGE_EVENT, MSG_CQUEUE_DOESNOTEXIST_S, name);
         answer_list_add(answer_list, SGE_EVENT,
                         STATUS_ERROR1, ANSWER_QUALITY_ERROR);
         ret = false;
      }
      if (ret) {
         ret &= cqueue_provide_modify_context(&cqueue, answer_list, false);
      }
      if (ret) {
         ret &= cqueue_add_del_mod_via_gdi(cqueue, answer_list, 
                                           SGE_GDI_MOD | SGE_GDI_SET_ALL);
      }
      if (cqueue) {
         cqueue = lFreeElem(cqueue);
      }
   }

   DEXIT;
   return ret;
}

bool 
cqueue_modify_from_file(lList **answer_list, const char *filename)
{
   bool ret = true;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;

   DENTER(TOP_LAYER, "cqueue_modify_from_file");
   if (filename != NULL) {
      lListElem *cqueue;

      fields_out[0] = NoName;
      cqueue = spool_flatfile_read_object(answer_list, CQ_Type, NULL,
                                          CQ_fields, fields_out, false, &cqqconf_sfi,
                                          SP_FORM_ASCII, NULL, filename);
            
      if (answer_list_output(answer_list)) {
         cqueue = lFreeElem (cqueue);
      }

      if (cqueue != NULL) {
         missing_field = spool_get_unprocessed_field (CQ_fields, fields_out, answer_list);
      }

      if (missing_field != NoName) {
         cqueue = lFreeElem(cqueue);
         answer_list_output(answer_list);
      }      

      if (cqueue == NULL) {
         sprintf(SGE_EVENT, MSG_CQUEUE_FILENOTCORRECT_S, filename);
         answer_list_add(answer_list, SGE_EVENT,
                         STATUS_ERROR1, ANSWER_QUALITY_ERROR);
         ret = false;
      }
      if (ret) {
         ret &= cqueue_add_del_mod_via_gdi(cqueue, answer_list, 
                                           SGE_GDI_MOD | SGE_GDI_SET_ALL);
      }
      if (cqueue != NULL) {
         cqueue = lFreeElem(cqueue);
      }
   }

   DEXIT;
   return ret;
}

bool 
cqueue_delete(lList **answer_list, const char *name)
{
   bool ret = true;

   DENTER(TOP_LAYER, "cqueue_delete");
   if (name != NULL) {
      lListElem *cqueue = cqueue_create(answer_list, name); 
   
      if (cqueue != NULL) {
         ret &= cqueue_add_del_mod_via_gdi(cqueue, answer_list, SGE_GDI_DEL); 
      }
   }
   DEXIT;
   return ret;
}

bool 
cqueue_show(lList **answer_list, const lList *qref_pattern_list)
{
   bool ret = true;

   DENTER(TOP_LAYER, "cqueue_show");
   if (qref_pattern_list != NULL) {
      lList *hgroup_list = NULL;
      lList *cqueue_list = NULL;
      lListElem *qref_pattern;
      bool local_ret;

      local_ret = cqueue_hgroup_get_via_gdi(answer_list, qref_pattern_list,
                                            &hgroup_list, &cqueue_list);
      if (local_ret) {

         for_each(qref_pattern, qref_pattern_list) {
            dstring cqueue_name = DSTRING_INIT;
            dstring host_domain = DSTRING_INIT;
            const char *cq_pattern = NULL;
            const char *name = NULL;
            bool has_hostname;
            bool has_domain;
            lList *qref_list = NULL;
            lListElem *qref = NULL;
            bool found_something = false;

            name = lGetString(qref_pattern, QR_name);
            cqueue_name_split(name, &cqueue_name, &host_domain,
                              &has_hostname, &has_domain);
            cq_pattern = sge_dstring_get_string(&cqueue_name);
            cqueue_list_find_all_matching_references(cqueue_list, NULL,
                                                     cq_pattern, &qref_list);
            if (has_domain) {
               const char *d_pattern = sge_dstring_get_string(&host_domain);
               lList *href_list = NULL;
               bool is_first = true;

               hgroup_list_find_matching_and_resolve(hgroup_list, NULL,
                                                     d_pattern, &href_list);
               for_each(qref, qref_list) {
                  const char *cqueue_name = lGetString(qref, QR_name);
                  const lListElem *cqueue = NULL;

                  cqueue = lGetElemStr(cqueue_list, CQ_name, cqueue_name);
                  if (cqueue != NULL) {
                     const lList *qinstance_list = NULL;
                     const lListElem *href = NULL;

                     qinstance_list = lGetList(cqueue, CQ_qinstances);
                     for_each(href, href_list) {
                        const char *hostname = lGetHost(href, HR_name);
                        const lListElem *qinstance;

                        qinstance = lGetElemHost(qinstance_list,
                                                 QU_qhostname, hostname);

                        if (qinstance != NULL) {
                           spooling_field *fields = sge_build_QU_field_list
                                                                  (true, false);

                           /* Bugfix: Issuezilla #1198
                            * In order to prevent the slots from being printed
                            * in the complex values list, we have to insert a
                            * custom writer for the complex_values field.  We do
                            * that with this function. */
                           insert_custom_complex_values_writer (fields);

                           if (is_first) {
                              is_first = false; 
                           } else {
                              fprintf(stdout, "\n");
                           }
                           
                           spool_flatfile_write_object(answer_list, qinstance,
                                                       false, fields,
                                                       &cqqconf_sfi,
                                                       SP_DEST_STDOUT,
                                                       SP_FORM_ASCII, NULL,
                                                       false);
                           FREE (fields);
                           
                           if (answer_list_output(answer_list)) {
                              DEXIT;
                              SGE_EXIT(1);
                           }

                           found_something = true;
                        }
                     }
                  }
               }

               href_list = lFreeList(href_list);
               qref_list = lFreeList(qref_list);
            } else if (has_hostname) {
               const char *h_pattern = sge_dstring_get_string(&host_domain);
               bool is_first = true;

               for_each(qref, qref_list) {
                  const char *cqueue_name = NULL;
                  const lListElem *cqueue = NULL;

                  cqueue_name = lGetString(qref, QR_name);
                  cqueue = lGetElemStr(cqueue_list, CQ_name, cqueue_name);
                  if (cqueue != NULL) {
                     const lList *qinstance_list = NULL;
                     const lListElem *qinstance = NULL;

                     qinstance_list = lGetList(cqueue, CQ_qinstances);
                     for_each(qinstance, qinstance_list) {
                        const char *hostname = NULL;

                        hostname = lGetHost(qinstance, QU_qhostname);
                        if (!fnmatch(h_pattern, hostname, 0) ||
                            !sge_hostcmp(h_pattern, hostname)) {
                           spooling_field *fields = sge_build_QU_field_list
                                                                  (true, false);                          

                           /* Bugfix: Issuezilla #1198
                            * In order to prevent the slots from being printed
                            * in the complex values list, we have to insert a
                            * custom writer for the complex_values field.  We do
                            * that with this function. */
                           insert_custom_complex_values_writer (fields);

                           if (is_first) {
                              is_first = false; 
                           } else {
                              fprintf(stdout, "\n");
                           }

                           spool_flatfile_write_object(answer_list, qinstance,
                                                       false, fields,
                                                       &cqqconf_sfi,
                                                       SP_DEST_STDOUT,
                                                       SP_FORM_ASCII, NULL,
                                                       false);
                           FREE (fields);
                           
                           if (answer_list_output(answer_list)) {
                              DEXIT;
                              SGE_EXIT(1);
                           }

                           found_something = true;
                        }
                     }
                  }
               }
            } else {
               bool is_first = true;

               for_each(qref, qref_list) {
                  const char *cqueue_name = lGetString(qref, QR_name);
                  lListElem *cqueue = NULL;

                  cqueue = lGetElemStr(cqueue_list, CQ_name, cqueue_name);
                  if (cqueue != NULL) {
                     if (is_first) {
                        is_first = false; 
                     } else {
                        fprintf(stdout, "\n");
                     }

                     spool_flatfile_write_object(answer_list, cqueue, false,
                                                 CQ_fields, &cqqconf_sfi,
                                                 SP_DEST_STDOUT, SP_FORM_ASCII, 
                                                 NULL, false);
                           
                     if (answer_list_output(answer_list)) {
                        DEXIT;
                        SGE_EXIT(1);
                     }

                     found_something = true;
                  }
               }
            }
            if (!found_something) {
               sprintf(SGE_EVENT, MSG_CQUEUE_NOQMATCHING_S, name);
               answer_list_add(answer_list, SGE_EVENT,
                               STATUS_ERROR1, ANSWER_QUALITY_ERROR);
               ret = false;
            }
            sge_dstring_free(&host_domain);
            sge_dstring_free(&cqueue_name);
         }
      } 
   } else {
      lListElem *cqueue = cqueue_create(answer_list, "template");

      DTRACE;
      ret &= cqueue_set_template_attributes(cqueue, answer_list);
      spool_flatfile_write_object(answer_list, cqueue, false, CQ_fields,
                                  &cqqconf_sfi, SP_DEST_STDOUT, SP_FORM_ASCII,
                                  NULL, false);
                           
      if (answer_list_output(answer_list)) {
         DEXIT;
         SGE_EXIT(1);
      }
   }
   DEXIT;
   return ret;
}

bool 
cqueue_list_sick(lList **answer_list)
{
   bool ret = true;
   lList *hgroup_list = NULL;
   lList *cqueue_list = NULL;
   bool local_ret;

   DENTER(TOP_LAYER, "cqueue_sick");

   local_ret = cqueue_hgroup_get_all_via_gdi(answer_list, 
                                             &hgroup_list, &cqueue_list);
   if (local_ret) {
      dstring ds = DSTRING_INIT;
      lListElem *cqueue = NULL;

      for_each(cqueue, cqueue_list) {
         cqueue_sick(cqueue, answer_list, hgroup_list, &ds);
      }
      if (sge_dstring_get_string(&ds)) 
         printf(sge_dstring_get_string(&ds));
      sge_dstring_free(&ds);
   }
   DEXIT;
   return ret;
}

bool
cqueue_sick(lListElem *cqueue, lList **answer_list, 
            lList *master_hgroup_list, dstring *ds)
{
   bool ret = true;

   DENTER(TOP_LAYER, "cqueue_sick");

   /*
    * Warn about:
    *    - unused setting for attributes
    *    - hgroup settings were not all hosts are contained in hostlist
    */
   {
      const char *cqueue_name = lGetString(cqueue, CQ_name);
      lList *used_hosts = NULL;
      lList *used_groups = NULL;
      lList **answer_list = NULL;
      int index;
      
      /*
       * resolve href list of cqueue
       */
      href_list_find_all_references(lGetList(cqueue, CQ_hostlist), answer_list,
                                    master_hgroup_list, &used_hosts,
                                    &used_groups);

      index = 0;
      while (cqueue_attribute_array[index].cqueue_attr != NoName) {
         /*
          * Skip geee attributes in ge mode
          */
         lList *attr_list = lGetList(cqueue,
                                 cqueue_attribute_array[index].cqueue_attr);
         lListElem *next_attr = lFirst(attr_list);
         lListElem *attr = NULL;

         /*
          * Test each attribute setting if it is really used in the
          * current configuration
          */
         while((attr = next_attr) != NULL) { 
            const char *name = lGetHost(attr, 
                                   cqueue_attribute_array[index].href_attr);

            next_attr = lNext(attr);
            if (is_hgroup_name(name)) {
               if (strcmp(name, HOSTREF_DEFAULT)) {
                  lListElem *hgroup = NULL;
                  lList *used_hgroup_hosts = NULL;
                  lList *used_hgroup_groups = NULL;
                  lList *add_hosts = NULL;
                  lList *equity_hosts = NULL;

                  hgroup = hgroup_list_locate(master_hgroup_list, name);

                  /*
                   * hgroup specific setting:
                   *    make sure each host of hgroup is part of 
                   *    resolved list
                   */
                  hgroup_find_all_references(hgroup, answer_list, 
                                             master_hgroup_list, 
                                             &used_hgroup_hosts,
                                             &used_hgroup_groups);
                  href_list_compare(used_hgroup_hosts, answer_list,
                                    used_hosts, &add_hosts, NULL,
                                    &equity_hosts, NULL);

                  if (lGetNumberOfElem(add_hosts)) {
                     DTRACE;
                     sge_dstring_sprintf_append(ds, 
                             MSG_CQUEUE_DEFOVERWRITTEN_SSSSS,
                             cqueue_attribute_array[index].name,
                             name, cqueue_name, name, cqueue_name);
                  }

                  add_hosts = lFreeList(add_hosts);
                  equity_hosts = lFreeList(equity_hosts); 
                  used_hgroup_hosts = lFreeList(used_hgroup_hosts);
                  used_hgroup_groups = lFreeList(used_hgroup_groups);
               } else {
                  DTRACE;
               }
            } else {
               /*
                * host specific setting:
                *    make sure the host is contained in resolved list 
                */ 
               if (!href_list_has_member(used_hosts, name)) {
                  DTRACE;
                  sge_dstring_sprintf_append(ds, 
                             MSG_CQUEUE_UNUSEDATTRSETTING_SS,
                             cqueue_attribute_array[index].name,
                             name, cqueue_name);
               } else {
                  DTRACE;
               }
            }
         }
            
         index++;
      }
      used_hosts = lFreeList(used_hosts);
      used_groups = lFreeList(used_groups);
   }
   
   DEXIT;
   return ret;
}

/****** insert_custom_complex_values_writer() **********************************
*  NAME
*     insert_custom_complex_values_writer() -- Inserts a custom writer for the
*                                              complex_values field
*
*  SYNOPSIS
*     static void insert_custom_complex_values_writer (spooling_field *fields)
*
*  FUNCTION
*     Inserts a custom writer for the complex_values field of a QU field list
*     which does not write out the "slots" complex value.
*
*  INPUT
*     spooling_field *fields - The QU fields list to be used for spooling
*
*  NOTES
*     MT-NOTES: insert_custom_complex_values_writer() is MT safe
*******************************************************************************/
static void insert_custom_complex_values_writer(spooling_field *fields)
{
   /* First, find the complex_values field. */
   int count = 0;
   
   while ((fields[count].nm != NoName) && (fields[count].nm != QU_consumable_config_list)) {
      count++;
   }
   
   if (fields[count].nm == QU_consumable_config_list) {
      /* Next, insert the custom writer. */
      fields[count].write_func = write_QU_consumable_config_list;
   }
   
   return;
}

/****** write_QU_consumable_config_list() **************************************
*  NAME
*     write_QU_consumable_config_list() -- Writes the complex_values field
*                                          without including slots
*
*  SYNOPSIS
*     static int write_QU_consumable_config_list(const lListElem *ep, int nm,
*                                                dstring *buffer, lList **alp)
*
*  FUNCTION
*     Writes the complex_values field to the buffer, but leaves out the slots
*     entry.
*
*  INPUT
*     const lListElem *ep - The QU element containing the complex_values
*     int              nm - The nm of the field := QU_consumable_config_list
*     dstring     *buffer - The dstring into which to print the field
*     lList         **alp - Answer list for errors
*******************************************************************************/
static int write_QU_consumable_config_list(const lListElem *ep, int nm,
                                           dstring *buffer, lList **alp)
{
   lList *lp = lGetList (ep, nm);
   lListElem *vep = NULL;
   bool first = true;
   
   /* Look through the complex_values list and print everything but slots */
   /* The format we're using to print is intended to replicate what is set forth
    * in the cqqconf_sub_name_value_space_sfi spooling instruction.  If this
    * instruction changes, or if the cqqconf_sfi spooling instruction changes to
    * use a new sub-instruction, this function will have to be changed to
    * to reflect this.  Otherwise, the complex values will be printed in a
    * different format from the other attributes. */
   for_each (vep, lp) {
      const char *name = lGetString (vep, CE_name);

      if (strcmp (name, "slots") != 0) {
         const char *strval = NULL;
         
         /* Print a separating space for all elements after the first */
         if (first) {
            first = false;
         }
         else {
            sge_dstring_append (buffer, " ");
         }
         
         /* Append name=value, where value could be a string of a number */
         sge_dstring_append (buffer, name);
         sge_dstring_append (buffer, "=");
         
         strval = lGetString(vep, CE_stringval);

         if (strval != NULL) {
            sge_dstring_append (buffer, strval);
         }
         else {
            char tmp[MAX_STRING_SIZE];
            
            snprintf(tmp, MAX_STRING_SIZE, "%f", lGetDouble(ep, CE_doubleval));
            sge_dstring_append (buffer, strdup (tmp));
         }
      }
   }
   
   return 1;
}
