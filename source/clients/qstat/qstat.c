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
#include <fnmatch.h>
#include <ctype.h>

#include "sgermon.h"
#include "symbols.h"
#include "sge.h"
#include "sge_gdi.h"
#include "sge_time.h"
#include "sge_log.h"
#include "sge_stdlib.h"
#include "sge_all_listsL.h"
#include "commlib.h"
#include "sge_host.h"
#include "sig_handlers.h"
#include "sge_sched.h"
#include "cull_sort.h"
#include "usage.h"
#include "sge_dstring.h"
#include "sge_feature.h"
#include "parse.h"
#include "sge_prog.h"
#include "sge_parse_num_par.h"
#include "sge_string.h"
#include "show_job.h"
#include "qstat_printing.h"
#include "sge_range.h"
#include "sge_schedd_text.h"
#include "qm_name.h"
#include "load_correction.h"
#include "msg_common.h"
#include "msg_clients_common.h"
#include "msg_qstat.h"
#include "sge_conf.h" 
#include "sgeee.h" 
#include "sge_support.h"
#include "sge_unistd.h"
#include "sge_answer.h"
#include "sge_pe.h"
#include "sge_str.h"
#include "sge_ckpt.h"
#include "sge_qinstance.h"
#include "sge_qinstance_state.h"
#include "sge_centry.h"
#include "sge_schedd_conf.h"
#include "sge_cqueue.h"
#include "sge_cqueue_qstat.h"
#include "sge_qref.h"
#include "qstat_xml.h"
#include "qstat_cmdline.h"
#include "qstat_filter.h"

#include "cull/cull_xml.h"

#include "sge_mt_init.h"
#include "read_defaults.h"
#include "setup_path.h"

#define FORMAT_I_20 "%I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I "
#define FORMAT_I_10 "%I %I %I %I %I %I %I %I %I %I "
#define FORMAT_I_5 "%I %I %I %I %I "
#define FORMAT_I_2 "%I %I "
#define FORMAT_I_1 "%I "

static void 
get_all_lists(lList **queue_l, lList **job_l, lList **centry_l, 
              lList **exechost_l, lList **sc_l, lList **pe_l, 
              lList **ckpt_l, lList **acl_l, lList **zombie_l, 
              lList **hgrp_l, lList *qrl, lList *perl, lList *ul, lList **project_list, 
              u_long32 full_listing);

static lList *
sge_parse_qstat(lList **ppcmdline, lList **pplresource, lList **pplqresource, 
                lList **pplqueueref, lList **ppluser, lList **pplqueue_user, 
                lList **pplpe, u_long32 *pfull, u_long32 *explain_bits, 
                char **hostname, u_long32 *job_info, 
                u_long32 *group_opt, u_long32 *queue_states, lList **ppljid, u_long32 *isXML);

static int qstat_show_job(lList *jid, u_long32 isXML);
static int qstat_show_job_info(u_long32 isXML);

lList *centry_list;
lList *exechost_list = NULL;

/************************************************************************/

static int qselect_mode = 0;
/*
** Additional options for debugging output
** only available, if env MORE_INFO is set.
*/
static u_long32 global_showjobs = 0;
static u_long32 global_showqueues = 0;

extern char **environ;

int main(int argc, char *argv[]);

/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/
int main(
int argc,
char **argv 
) {
   lListElem *tmp, *jep, *jatep, *qep, *up, *aep, *cqueue;
   lList *queue_list = NULL, *job_list = NULL, *resource_list = NULL;
   lList *qresource_list = NULL, *queueref_list = NULL, *user_list = NULL;
   lList *acl_list = NULL, *project_list = NULL, *zombie_list = NULL, *hgrp_list = NULL; 
   lList *jid_list = NULL, *queue_user_list = NULL, *peref_list = NULL;
   lList *pe_list = NULL, *ckpt_list = NULL, *ref_list = NULL; 
   lList *alp = NULL, *pcmdline = NULL;
   bool a_cqueue_is_selected = true;
   u_long32 full_listing = QSTAT_DISPLAY_ALL, job_info = 0;
   u_long32 explain_bits = QI_DEFAULT;
   u_long32 group_opt = 0;
   u_long32 queue_states = U_LONG32_MAX;
   lSortOrder *so = NULL;
   int nqueues;
   char *hostname = NULL;
   bool first_time = true;
   u_long32 isXML = false;
   lList *XML_out = NULL;
   int longest_queue_length=30;

   DENTER_MAIN(TOP_LAYER, "qstat");

   sge_mt_init();

   log_state_set_log_gui(1);

   sge_gdi_param(SET_MEWHO, QSTAT, NULL);
   if (sge_gdi_setup(prognames[QSTAT], &alp)!=AE_OK) {
      answer_list_output(&alp);
      SGE_EXIT(1);
   }

   sge_setup_sig_handlers(QSTAT);

   if (!strcmp(sge_basename(*argv++, '/'), "qselect")) {
      qselect_mode = 1;
   }


   {
      dstring file = DSTRING_INIT;
      const char *user = uti_state_get_user_name();
      const char *cell_root = path_state_get_cell_root();

      if (qselect_mode == 0) { /* the .sge_qstat file should only be used in the qstat mode */
         get_root_file_path(&file, cell_root, SGE_COMMON_DEF_QSTAT_FILE);
         switch_list_qstat_parse_from_file(&pcmdline, &alp, qselect_mode, 
                                           sge_dstring_get_string(&file));
         if (get_user_home_file_path(&file, SGE_HOME_DEF_QSTAT_FILE, user,
                                     &alp)) {
            switch_list_qstat_parse_from_file(&pcmdline, &alp, qselect_mode, 
                                           sge_dstring_get_string(&file));
         }
      }                                  
      switch_list_qstat_parse_from_cmdline(&pcmdline, &alp, qselect_mode, argv);
      sge_dstring_free(&file);
   }
 
   if(alp) {
      /*
      ** high level parsing error! sow answer list
      */
      for_each(aep, alp) {
         fprintf(stderr, "%s\n", lGetString(aep, AN_text));
      }
      lFreeList(&alp);
      lFreeList(&pcmdline);
      SGE_EXIT(1);
   }

   alp = sge_parse_qstat(&pcmdline, 
      &resource_list,   /* -l resource_request           */
      &qresource_list,  /* -F qresource_request          */
      &queueref_list,   /* -q queue_list                 */
      &user_list,       /* -u user_list - selects jobs   */
      &queue_user_list, /* -U user_list - selects queues */
      &peref_list,      /* -pe pe_list                   */
      &full_listing,    /* -ext                          */
      &explain_bits,    /* -explain                      */
      &hostname,
      &job_info,        /* -j ..                         */
      &group_opt,       /* -g ..                         */
      &queue_states,    /* -qs ...                       */
      &jid_list, &isXML);       /* .. jid_list                   */

   if(alp) {
      /*
      ** low level parsing error! show answer list
      */
      for_each(aep, alp) {
         fprintf(stderr, "%s\n", lGetString(aep, AN_text));
      }
      lFreeList(&alp);
      lFreeList(&pcmdline);
      lFreeList(&ref_list);
      SGE_EXIT(1);
   }

   /* if -j, then only print job info and leave */
   if (job_info) {
      int ret = 0;

      if(lGetNumberOfElem(jid_list) > 0) {
         ret = qstat_show_job(jid_list, isXML);
      } else {
         ret = qstat_show_job_info(isXML);
      }
      DEXIT;
      SGE_EXIT(ret);
   }


   sge_stopwatch_start(0);

   {
      lList *schedd_config = NULL;
      lList *answer_list = NULL;

      str_list_transform_user_list(&user_list, &answer_list);

      get_all_lists(
         &queue_list, 
         (qselect_mode || ((group_opt & GROUP_CQ_SUMMARY) != 0 )) ? NULL : 
                                                                 &job_list,
         &centry_list, 
         &exechost_list,
         &schedd_config,
         &pe_list,
         &ckpt_list,
         &acl_list,
         &zombie_list, 
         &hgrp_list,
         queueref_list,
         peref_list,
         user_list,
         &project_list,
         full_listing); 

      if (!sconf_set_config(&schedd_config, &answer_list)){
         answer_list_output(&answer_list);
         lFreeList(&schedd_config);
         DEXIT;
         SGE_EXIT(-1);
      }
   }

   centry_list_init_double(centry_list);

   sge_stopwatch_log(0, "Time for getting all lists");
   
   if (getenv("MORE_INFO")) {
      if(global_showjobs) {
         lWriteListTo(job_list, stdout);
         lFreeList(&job_list);
         lFreeList(&queue_list);
         SGE_EXIT(0);
         return 0;
      }

      if(global_showqueues) {
         lWriteListTo(queue_list, stdout);
         lFreeList(&job_list);
         lFreeList(&queue_list);
         SGE_EXIT(0);
         return 0;
      }
   }

   /* 
    *
    * step 1: we tag the queues selected by the user
    *
    *         -q, -U, -pe and -l must be fulfilled by 
    *         the queue if they are given
    *
    */

   DPRINTF(("------- selecting queues -----------\n"));
   /* all queues are selected */
   cqueue_list_set_tag(queue_list, TAG_SHOW_IT, true);

   /* unseclect all queues not selected by a -q (if exist) */
   if (lGetNumberOfElem(queueref_list)>0) {
      if ((nqueues=select_by_qref_list(queue_list, hgrp_list, queueref_list))<0) {
         fprintf(stderr, MSG_QSTAT_NOQUEUESREMAININGAFTERXQUEUESELECTION_S,"-q");
         fprintf(stderr, "\n");
         SGE_EXIT(1);
      }
      if (nqueues==0) {
         fprintf(stderr, MSG_QSTAT_NOQUEUESREMAININGAFTERXQUEUESELECTION_S,"-q");
         fprintf(stderr, "\n");
         SGE_EXIT(1);
      }
   }

   /* unselect all queues not selected by -qs */
   select_by_queue_state(queue_states, exechost_list, queue_list, centry_list);
  
   /* unseclect all queues not selected by a -U (if exist) */
   if (lGetNumberOfElem(queue_user_list)>0) {
      if ((nqueues=select_by_queue_user_list(exechost_list, queue_list, queue_user_list, acl_list, project_list))<0) {
         SGE_EXIT(1);
      }

      if (nqueues==0) {
         fprintf(stderr, MSG_QSTAT_NOQUEUESREMAININGAFTERXQUEUESELECTION_S,"-U");
         fprintf(stderr, "\n");
         SGE_EXIT(1);
      }
   }

   /* unseclect all queues not selected by a -pe (if exist) */
   if (lGetNumberOfElem(peref_list)>0) {
      if ((nqueues=select_by_pe_list(queue_list, peref_list, pe_list))<0) {
         SGE_EXIT(1);
      }

      if (nqueues==0) {
         fprintf(stderr, MSG_QSTAT_NOQUEUESREMAININGAFTERXQUEUESELECTION_S,"-pe");
         fprintf(stderr, "\n");
         SGE_EXIT(1);
      }
   }
   /* unseclect all queues not selected by a -l (if exist) */
   if (lGetNumberOfElem(resource_list)) {
      if (select_by_resource_list(resource_list, exechost_list, queue_list, centry_list, 1)<0) {
         SGE_EXIT(1);
      }
   }   

   a_cqueue_is_selected = is_cqueue_selected(queue_list);

   if (!a_cqueue_is_selected && qselect_mode) {
      fprintf(stderr, "%s\n", MSG_QSTAT_NOQUEUESREMAININGAFTERSELECTION);
      SGE_EXIT(1);
   }

   if (qselect_mode) {
      for_each(cqueue, queue_list) {
         lList *qinstance_list = lGetList(cqueue, CQ_qinstances);

         for_each(qep, qinstance_list) {
            if ((lGetUlong(qep, QU_tag) & TAG_SHOW_IT)!=0) {
               dstring qinstance_name = DSTRING_INIT;

               qinstance_get_name(qep, &qinstance_name);
               printf("%s\n", sge_dstring_get_string(&qinstance_name));
               sge_dstring_free(&qinstance_name);
            }
         }
      }
      SGE_EXIT(0);
   }

   if (!a_cqueue_is_selected && 
      (full_listing&(QSTAT_DISPLAY_QRESOURCES|QSTAT_DISPLAY_FULL))) {
      SGE_EXIT(0);
   }
   /* 
    *
    * step 2: we tag the jobs 
    *
    */

   /* 
   ** all jobs are selected 
   */
   for_each (jep, job_list) {
      for_each (jatep, lGetList(jep, JB_ja_tasks)) {
         if (!(lGetUlong(jatep, JAT_status) & JFINISHED))
            lSetUlong(jatep, JAT_suitable, TAG_SHOW_IT);
      }
   }

   /*
   ** tag only jobs which satisfy the user list
   */
   if (lGetNumberOfElem(user_list)) {
      DPRINTF(("------- selecting jobs -----------\n"));

      /* ok, now we untag the jobs if the user_list was specified */ 
      for_each(up, user_list) 
         for_each (jep, job_list) {
            if (up && lGetString(up, ST_name) && 
                  !fnmatch(lGetString(up, ST_name), 
                              lGetString(jep, JB_owner), 0)) {
               for_each (jatep, lGetList(jep, JB_ja_tasks)) {
                  lSetUlong(jatep, JAT_suitable, 
                     lGetUlong(jatep, JAT_suitable)|TAG_SHOW_IT|TAG_SELECT_IT);
               }
            }
         }
   }


   if (lGetNumberOfElem(peref_list) || lGetNumberOfElem(queueref_list) || 
       lGetNumberOfElem(resource_list) || lGetNumberOfElem(queue_user_list)) {
      /*
      ** unselect all pending jobs that fit in none of the selected queues
      ** that way the pending jobs are really pending jobs for the queues 
      ** printed
      */

      sconf_set_qs_state(QS_STATE_EMPTY);
      for_each(jep, job_list) {
         int ret, show_job;

         show_job = 0;

         for_each(cqueue, queue_list) {
            const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);

            for_each(qep, qinstance_list) {
               lListElem *host = NULL;

               if (!(lGetUlong(qep, QU_tag) & TAG_SHOW_IT)) {
                  continue;
               }
               
               host = host_list_locate(exechost_list, lGetHost(qep, QU_qhostname));
               
               if (host != NULL) {
                  ret = sge_select_queue(lGetList(jep, JB_hard_resource_list), qep, 
                                         host, exechost_list, centry_list, true, 1, queue_user_list, acl_list, jep);

                  if (ret==1) {
                     show_job = 1;
                     break;
                  }
               }
               /* we should have an error message here, even so it should not happen, that
                 we have queue instances without a host, but.... */
            }
         }   

         for_each (jatep, lGetList(jep, JB_ja_tasks)) {
            if (!show_job && !(lGetUlong(jatep, JAT_status) == JRUNNING || (lGetUlong(jatep, JAT_status) == JTRANSFERING))) {
               DPRINTF(("show task "sge_u32"."sge_u32"\n",
                       lGetUlong(jep, JB_job_number),
                       lGetUlong(jatep, JAT_task_number)));
               lSetUlong(jatep, JAT_suitable, lGetUlong(jatep, JAT_suitable) & ~TAG_SHOW_IT);
            }
         }
         if (!show_job) {
            lSetList(jep, JB_ja_n_h_ids, NULL);
            lSetList(jep, JB_ja_u_h_ids, NULL);
            lSetList(jep, JB_ja_o_h_ids, NULL);
            lSetList(jep, JB_ja_s_h_ids, NULL);
         }
      }
      sconf_set_qs_state(QS_STATE_FULL);
   }

   /*
    * step 2.5: reconstruct queue stata structure
    */
   if ((group_opt & GROUP_CQ_SUMMARY) != 0) {
      lPSortList(queue_list, "%I+ ", CQ_name);
   } else {
      lList *tmp_queue_list = NULL;
      lListElem *cqueue = NULL;

      tmp_queue_list = lCreateList("", QU_Type);

      for_each(cqueue, queue_list) {
         lList *qinstances = NULL;

         lXchgList(cqueue, CQ_qinstances, &qinstances);
         lAddList(tmp_queue_list, &qinstances);
      }
      
      lFreeList(&queue_list);
      queue_list = tmp_queue_list;
      tmp_queue_list = NULL;

      lPSortList(queue_list, "%I+ %I+ %I+", QU_seq_no, QU_qname, QU_qhostname);

   }

   /* 
    *
    * step 3: iterate over the queues (and print them)
    *
    *         tag the jobs in these queues with TAG_FOUND_IT
    *         print these jobs if necessary
    *
    */
   {
      u_long32 name;
      char *env;
      if ((group_opt & GROUP_CQ_SUMMARY) == 0) { 
         name = QU_full_name;
      }
      else {
         name = CQ_name;
      }
      if((env = getenv("SGE_LONG_QNAMES")) != NULL){
         longest_queue_length = atoi(env);
         if (longest_queue_length == -1) {
            for_each(qep, queue_list) {
               int length;
               const char *queue_name =lGetString(qep, name);
               if( (length = strlen(queue_name)) > longest_queue_length){
                  longest_queue_length = length;
               }
            }
         }
         else {
            if (longest_queue_length < 10) {
               longest_queue_length = 10;
            }
         }
      }
   }

/*    TODO                                            */    
/*    is correct_capacities needed here ???           */    
   correct_capacities(exechost_list, centry_list);
   if ((group_opt & GROUP_CQ_SUMMARY) != 0) {
      for_each (cqueue, queue_list) {
         if (lGetUlong(cqueue, CQ_tag) != TAG_DEFAULT) {
            double load = 0.0;
            u_long32 used, total;
            u_long32 temp_disabled, available, manual_intervention;
            u_long32 suspend_manual, suspend_threshold, suspend_on_subordinate;
            u_long32 suspend_calendar, unknown, load_alarm;
            u_long32 disabled_manual, disabled_calendar, ambiguous;
            u_long32 orphaned, error;
            bool is_load_available;
            bool show_states = (full_listing & QSTAT_DISPLAY_EXTENDED) ? true : false;

            cqueue_calculate_summary(cqueue,
                                     exechost_list,
                                     centry_list,
                                     &load,
                                     &is_load_available,
                                     &used,
                                     &total,
                                     &suspend_manual,
                                     &suspend_threshold,
                                     &suspend_on_subordinate,
                                     &suspend_calendar,
                                     &unknown,
                                     &load_alarm,
                                     &disabled_manual,
                                     &disabled_calendar,
                                     &ambiguous,
                                     &orphaned,
                                     &error,
                                     &available,
                                     &temp_disabled,
                                     &manual_intervention);
            
            if (!isXML) {
               if (first_time) {
                  char queue_def[50];
                  char fields[] = "%7s %6s %6s %6s %6s %6s ";
                  sprintf(queue_def, "%%-%d.%ds %s ", longest_queue_length, longest_queue_length, fields);                         
                  printf( queue_def,
                         "CLUSTER QUEUE", "CQLOAD", 
                         "USED", "AVAIL", "TOTAL", "aoACDS", "cdsuE");
                  if (show_states) {
                     printf("%5s %5s %5s %5s %5s %5s %5s %5s %5s %5s %5s", 
                            "s", "A", "S", "C", "u", "a", "d", "D", "c", "o", "E");
                  }
                  printf("\n");

                  printf("--------------------");
                  printf("--------------------");
                  printf("--------------------");
                  printf("-------------------");
                  if (show_states) {
                     printf("--------------------");
                     printf("--------------------");
                     printf("--------------------");
                     printf("------");
                  }
                  {
                     int i;
                     for(i=0; i< longest_queue_length - 36; i++)
                        printf("-");
                  }
                  printf("\n");
                  first_time = false;
               }
               {
                  char queue_def[50];
                  sprintf(queue_def, "%%-%d.%ds ", longest_queue_length, longest_queue_length);

                  printf(queue_def, lGetString(cqueue, CQ_name));
               }
               if (is_load_available) {
                  printf("%7.2f ", load);
               } else {
                  printf("%7s ", "-NA-");
               }
               printf("%6d ", (int)used);
               printf("%6d ", (int)available);
               printf("%6d ", (int)total);
               printf("%6d ", (int)temp_disabled);
               printf("%6d ", (int)manual_intervention);
               if (show_states) {
                  printf("%5d ", (int)suspend_manual);
                  printf("%5d ", (int)suspend_threshold);
                  printf("%5d ", (int)suspend_on_subordinate);
                  printf("%5d ", (int)suspend_calendar);
                  printf("%5d ", (int)unknown);
                  printf("%5d ", (int)load_alarm);
                  printf("%5d ", (int)disabled_manual);
                  printf("%5d ", (int)disabled_calendar);
                  printf("%5d ", (int)ambiguous);
                  printf("%5d ", (int)orphaned);
                  printf("%5d ", (int)error);
               }
               printf("\n");
            }
            else {
               lListElem *elem = NULL;
               lList *attributeList = NULL;

               elem = lCreateElem(XMLE_Type);
               attributeList = lCreateList("attributes", XMLE_Type);
               lSetList(elem, XMLE_List, attributeList);
             
               xml_append_Attr_S(attributeList, "name", lGetString(cqueue, CQ_name));
               if (is_load_available) {
                  xml_append_Attr_D(attributeList, "load", load);
               }
               xml_append_Attr_I(attributeList, "used", (int)used);
               xml_append_Attr_I(attributeList, "available", (int)available);
               xml_append_Attr_I(attributeList, "total", (int)total);
               xml_append_Attr_I(attributeList, "temp_disabled", (int)temp_disabled);
               xml_append_Attr_I(attributeList, "manual_intervention", (int)manual_intervention);
               if (show_states) {
                  xml_append_Attr_I(attributeList, "suspend_manual", (int)suspend_manual);
                  xml_append_Attr_I(attributeList, "suspend_threshold", (int)suspend_threshold);
                  xml_append_Attr_I(attributeList, "suspend_on_subordinate", (int)suspend_on_subordinate);
                  xml_append_Attr_I(attributeList, "suspend_calendar", (int)suspend_calendar);
                  xml_append_Attr_I(attributeList, "unknown", (int)unknown);
                  xml_append_Attr_I(attributeList, "load_alarm", (int)load_alarm);
                  xml_append_Attr_I(attributeList, "disabled_manual", (int)disabled_manual);
                  xml_append_Attr_I(attributeList, "disabled_calendar", (int)disabled_calendar);
                  xml_append_Attr_I(attributeList, "ambiguous", (int)ambiguous);
                  xml_append_Attr_I(attributeList, "orphaned", (int)orphaned);
                  xml_append_Attr_I(attributeList, "error", (int)error);
               }
               if (elem) {
                  if (XML_out == NULL){
                     XML_out = lCreateList("cluster_queue_summary", XMLE_Type);
                  }
                  lAppendElem(XML_out, elem); 
               } 
            }
         }

      } 

   } else {
      for_each(qep, queue_list) {
         lList *xml_job_list = NULL; 
         lListElem *elem = NULL;
         /* here we have the queue */
         if (lGetUlong(qep, QU_tag) & TAG_SHOW_IT) {
            if ((full_listing & QSTAT_DISPLAY_NOEMPTYQ) && 
                     !qinstance_slots_used(qep)) {
               continue;
            } else {
               if (isXML) {
                  if ((elem = xml_print_queue(qep, exechost_list, centry_list,
                                  full_listing, qresource_list, explain_bits)) != NULL) {

                     if (XML_out == NULL){
                        XML_out = lCreateList("Queue-List", XMLE_Type);
                     }
                     
                     lAppendElem(XML_out, elem); 
                  }
               } else{
                  sge_print_queue(qep, exechost_list, centry_list,
                                  full_listing, qresource_list, 
                                  explain_bits, longest_queue_length);
               }
            }


            if (shut_me_down) {
               SGE_EXIT(1);
            }

            if (isXML){
               xml_print_jobs_queue(qep, job_list, pe_list, user_list,
                                 exechost_list, centry_list,
                                 1, full_listing, group_opt, elem?(&xml_job_list):(&XML_out));

               if (elem && xml_job_list){
                  lSetList(elem, XMLE_List, xml_job_list);
               }
            }
            else {
               sge_print_jobs_queue(qep, job_list, pe_list, user_list,
                                 exechost_list, centry_list,
                                 1, full_listing, "", group_opt, longest_queue_length);
            }
         }
      }
      if (XML_out != NULL) {
         lListElem *xmlElem = lCreateElem(XMLE_Type);
         lListElem *attrElem = lCreateElem(XMLA_Type);
         lList *tempXML = lCreateList("queue_info", XMLE_Type);
         
         lSetString(attrElem, XMLA_Name, "queue_info");
         lSetObject(xmlElem, XMLE_Element, attrElem);
         lSetBool(xmlElem, XMLE_Print, true);
         lSetList(xmlElem, XMLE_List, XML_out);
         lAppendElem(tempXML, xmlElem);
         XML_out = tempXML;
      }
   }
    

   /*
    *
    * step 3.5: remove all jobs that we found till now 
    *           sort other jobs for printing them in order
    *
    */
   DTRACE;
   jep = lFirst(job_list);
   while (jep) {
      lList *task_list;
      lListElem *jatep, *tmp_jatep;

      tmp = lNext(jep);
      task_list = lGetList(jep, JB_ja_tasks);
      jatep = lFirst(task_list);
      while (jatep) {
         tmp_jatep = lNext(jatep);
         if ((lGetUlong(jatep, JAT_suitable) & TAG_FOUND_IT)) {
            lRemoveElem(task_list, &jatep);
         }
         jatep = tmp_jatep;
      }
      jep = tmp;
   }

   if (lGetNumberOfElem(job_list)>0 ) {
         sgeee_sort_jobs(&job_list);
   }

   if ((group_opt & GROUP_CQ_SUMMARY) == 0) {
      if (isXML) {
         lList *tempXML = NULL;
         xml_qstat_jobs(job_list, zombie_list, pe_list, user_list, exechost_list, centry_list, so,
                        full_listing, group_opt, &tempXML /*&XML_out*/); 
         if (tempXML != NULL) {
            lListElem *xmlElem = lCreateElem(XMLE_Type);
            lListElem *attrElem = lCreateElem(XMLA_Type);
            
            lSetString(attrElem, XMLA_Name, "job_info");
            lSetObject(xmlElem, XMLE_Element, attrElem);
            lSetBool(xmlElem, XMLE_Print, true);
            lSetList(xmlElem, XMLE_List, tempXML);
            if (XML_out == NULL){
               XML_out = lCreateList("job_info", XMLE_Type);
            }
            lAppendElem(XML_out, xmlElem);
         }
      }
      else {
         /* 
          *
          * step 4: iterate over jobs that are pending;
          *         tag them with TAG_FOUND_IT
          *
          *         print the jobs that run in these queues 
          *
          */
         sge_print_jobs_pending(job_list, pe_list, user_list, exechost_list,
                                centry_list, so, full_listing, group_opt, longest_queue_length);

         /* 
          *
          * step 5:  in case of SGE look for finished jobs and view them as
          *          finished  a non SGE-qstat will show them as error jobs
          *
          */
         sge_print_jobs_finished(job_list, pe_list, user_list, exechost_list,
                                 centry_list, full_listing, group_opt, longest_queue_length);

         /*
          *
          * step 6:  look for jobs not found. This should not happen, cause each
          *          job is running in a queue, or pending. But if there is
          *          s.th. wrong we have
          *          to ensure to print this job just to give hints whats wrong
          *
          */
         sge_print_jobs_error(job_list, pe_list, user_list, exechost_list,
                              centry_list, full_listing, group_opt, longest_queue_length);

         /*
          *
          * step 7:  print recently finished jobs ('zombies')
          *
          */
         sge_print_jobs_zombie(zombie_list, pe_list, user_list, exechost_list,
                               centry_list,  full_listing, group_opt, longest_queue_length);
      }
   }

   if (XML_out != NULL) {
      lListElem *xml_elem = NULL;
    
      xml_elem = xml_getHead("job_info", XML_out, NULL);
      XML_out = NULL;   
      lWriteElemXMLTo(xml_elem, stdout);
  
      lFreeElem(&xml_elem);
    
   }
   
   lFreeList(&zombie_list);
   lFreeList(&job_list);
   lFreeList(&queue_list);
   SGE_EXIT(0);
   return 0;
}

/****
 **** get_all_lists (static)
 ****
 **** Gets copies of queue-, job-, complex-, exechost- , pe- and 
 **** sc_config-list from qmaster.
 **** The lists are stored in the .._l pointerpointer-parameters.
 **** WARNING: Lists previously stored in this pointers are not destroyed!!
 ****/
static void get_all_lists(
lList **queue_l,
lList **job_l,
lList **centry_l,
lList **exechost_l,
lList **sc_l,
lList **pe_l,
lList **ckpt_l,
lList **acl_l,
lList **zombie_l,
lList **hgrp_l,
lList *queueref_list,
lList *peref_list,
lList *user_list,
lList **project_l,
u_long32 show 
) {
   lCondition *where= NULL, *nw = NULL, *qw = NULL, *pw = NULL; 
   lCondition *zw = NULL, *gc_where = NULL;
   lEnumeration *q_all, *pe_all, *ckpt_all, *acl_all, *ce_all, *up_all;
   lEnumeration *eh_all, *sc_what, *gc_what, *hgrp_what;
   lList *alp = NULL;
   lListElem *aep = NULL;
   lListElem *ep = NULL;
   lList *conf_l = NULL;
   lList *mal = NULL;
   int q_id, j_id = 0, pe_id = 0, ckpt_id = 0, acl_id = 0, z_id = 0, up_id = 0;
   int ce_id, eh_id, sc_id, gc_id, hgrp_id = 0;
   int show_zombies = (show & QSTAT_DISPLAY_ZOMBIES) ? 1 : 0;
   state_gdi_multi state = STATE_GDI_MULTI_INIT;

   DENTER(TOP_LAYER, "get_all_lists");

   q_all = lWhat("%T(ALL)", CQ_Type);
   q_id = sge_gdi_multi(&alp, SGE_GDI_RECORD, SGE_CQUEUE_LIST, SGE_GDI_GET,
                        NULL, NULL, q_all, NULL, &state, true);
   lFreeWhat(&q_all);
   lFreeWhere(&qw);
 
   if (alp) {
      printf("%s\n", lGetString(lFirst(alp), AN_text));
      SGE_EXIT(1);
   }

   /* 
   ** jobs
   */
   if (job_l) {
      lCondition *where = qstat_get_JB_Type_selection(user_list, show);
      lEnumeration *what = qstat_get_JB_Type_filter();

      j_id = sge_gdi_multi(&alp, SGE_GDI_RECORD, SGE_JOB_LIST, SGE_GDI_GET,
                           NULL, where, what, NULL, &state, true);

      lFreeWhere(&where);

      if (alp) {
         printf("%s\n", lGetString(lFirst(alp), AN_text));
         SGE_EXIT(1);
      }
   }

   /* 
   ** job zombies
   */
   if (zombie_l && show_zombies) {
      for_each(ep, user_list) {
         nw = lWhere("%T(%I p= %s)", JB_Type, JB_owner, lGetString(ep, ST_name));
         if (!zw)
            zw = nw;
         else
            zw = lOrWhere(zw, nw);
      }

      z_id = sge_gdi_multi(&alp, SGE_GDI_RECORD, SGE_ZOMBIE_LIST, SGE_GDI_GET, 
                           NULL, zw, qstat_get_JB_Type_filter(), NULL, &state, true);
      lFreeWhere(&zw);

      if (alp) {
         printf("%s\n", lGetString(lFirst(alp), AN_text));
         SGE_EXIT(1);
      }
   }

   /*
   ** complexes
   */
   ce_all = lWhat("%T(ALL)", CE_Type);
   ce_id = sge_gdi_multi(&alp, SGE_GDI_RECORD, SGE_CENTRY_LIST, SGE_GDI_GET, 
                        NULL, NULL, ce_all, NULL, &state, true);
   lFreeWhat(&ce_all);

   if (alp) {
      printf("%s\n", lGetString(lFirst(alp), AN_text));
      SGE_EXIT(1);
   }

   /*
   ** exechosts
   */
   where = lWhere("%T(%I!=%s)", EH_Type, EH_name, SGE_TEMPLATE_NAME);
   eh_all = lWhat("%T(ALL)", EH_Type);
   eh_id = sge_gdi_multi(&alp, SGE_GDI_RECORD, SGE_EXECHOST_LIST, SGE_GDI_GET,
                        NULL, where, eh_all, NULL, &state, true);
   lFreeWhat(&eh_all);
   lFreeWhere(&where);

   if (alp) {
      printf("%s\n", lGetString(lFirst(alp), AN_text));
      SGE_EXIT(1);
   }

   /* 
   ** pe list 
   */ 
   if (pe_l) {   
      pe_all = lWhat("%T(%I%I%I%I%I)", PE_Type, PE_name, PE_slots, PE_job_is_first_task, PE_control_slaves, PE_urgency_slots);
      pe_id = sge_gdi_multi(&alp, SGE_GDI_RECORD, SGE_PE_LIST, SGE_GDI_GET,
                           NULL, pw, pe_all, NULL, &state, true);
      lFreeWhat(&pe_all);
      lFreeWhere(&pw);

      if (alp) {
         printf("%s\n", lGetString(lFirst(alp), AN_text));
         SGE_EXIT(1);
      }
   }

  /* 
   ** ckpt list 
   */ 
   if (ckpt_l) {
      ckpt_all = lWhat("%T(%I)", CK_Type, CK_name);
      ckpt_id = sge_gdi_multi(&alp, SGE_GDI_RECORD, SGE_CKPT_LIST, SGE_GDI_GET,
                           NULL, NULL, ckpt_all, NULL, &state, true);
      lFreeWhat(&ckpt_all);

      if (alp) {
         printf("%s\n", lGetString(lFirst(alp), AN_text));
         SGE_EXIT(1);
      }
   }

   /* 
   ** acl list 
   */ 
   if (acl_l) {
      acl_all = lWhat("%T(ALL)", US_Type);
      acl_id = sge_gdi_multi(&alp, SGE_GDI_RECORD, SGE_USERSET_LIST, SGE_GDI_GET, 
                           NULL, NULL, acl_all, NULL, &state, true);
      lFreeWhat(&acl_all);

      if (alp) {
         printf("%s\n", lGetString(lFirst(alp), AN_text));
         SGE_EXIT(1);
      }
   }

   /* 
   ** project list 
   */ 
   if (project_l) {
      up_all = lWhat("%T(ALL)", UP_Type);
      up_id = sge_gdi_multi(&alp, SGE_GDI_RECORD, SGE_PROJECT_LIST, SGE_GDI_GET, 
                           NULL, NULL, up_all, NULL, &state, true);
      lFreeWhat(&up_all);

      if (alp) {
         printf("%s\n", lGetString(lFirst(alp), AN_text));
         SGE_EXIT(1);
      }
   }

   /*
   ** scheduler configuration
   */

   /* might be enough, but I am not sure */
   /*sc_what = lWhat("%T(%I %I)", SC_Type, SC_user_sort, SC_job_load_adjustments);*/ 
   sc_what = lWhat("%T(ALL)", SC_Type);

   sc_id = sge_gdi_multi(&alp, SGE_GDI_RECORD, SGE_SC_LIST, SGE_GDI_GET, 
                        NULL, NULL, sc_what, NULL, &state, true);
   lFreeWhat(&sc_what);

   if (alp) {
      printf("%s\n", lGetString(lFirst(alp), AN_text));
      SGE_EXIT(1);
   }

   /*
   ** hgroup 
   */
   hgrp_what = lWhat("%T(ALL)", HGRP_Type);
   hgrp_id = sge_gdi_multi(&alp, SGE_GDI_RECORD, SGE_HGROUP_LIST, SGE_GDI_GET, 
                        NULL, NULL, hgrp_what, NULL, &state, true);
   lFreeWhat(&hgrp_what);

   if (alp) {
      printf("%s\n", lGetString(lFirst(alp), AN_text));
      SGE_EXIT(1);
   }

   /*
   ** global cluster configuration
   */
   gc_where = lWhere("%T(%I c= %s)", CONF_Type, CONF_hname, SGE_GLOBAL_NAME);
   gc_what = lWhat("%T(ALL)", CONF_Type);
   gc_id = sge_gdi_multi(&alp, SGE_GDI_SEND, SGE_CONFIG_LIST, SGE_GDI_GET,
                        NULL, gc_where, gc_what, &mal, &state, true);
   lFreeWhat(&gc_what);
   lFreeWhere(&gc_where);

   if (alp) {
      printf("%s\n", lGetString(lFirst(alp), AN_text));
      SGE_EXIT(1);
   }

   /*
   ** handle results
   */
   /* --- queue */
   alp = sge_gdi_extract_answer(SGE_GDI_GET, SGE_CQUEUE_LIST, q_id, 
                                 mal, queue_l);
   if (!alp) {
      printf("%s\n", MSG_GDI_QUEUESGEGDIFAILED);
      SGE_EXIT(1);
   }
   if (lGetUlong(aep=lFirst(alp), AN_status) != STATUS_OK) {
      printf("%s\n", lGetString(aep, AN_text));
      SGE_EXIT(1);
   }
   lFreeList(&alp);

   /* --- job */
   if (job_l) {
      alp = sge_gdi_extract_answer(SGE_GDI_GET, SGE_JOB_LIST, j_id, mal, job_l);

#if 0 /* EB: debug */
      {
         lListElem *elem = NULL;

         for_each(elem, *job_l) {
            lListElem *task = lFirst(lGetList(elem, JB_ja_tasks));

            fprintf(stderr, "jid="sge_u32" ", lGetUlong(elem, JB_job_number));
            if (task) {
               dstring string = DSTRING_INIT;

               fprintf(stderr, "state=%s status=%s job_restarted="sge_u32"\n", sge_dstring_ulong_to_binstring(&string, lGetUlong(task, JAT_state)), sge_dstring_ulong_to_binstring(&string, lGetUlong(task, JAT_status)), lGetUlong(task, JAT_job_restarted));
               sge_dstring_free(&string);
            } else {
               fprintf(stderr, "\n");
            }
         }
      }
#endif
      if (!alp) {
         printf("%s\n", MSG_GDI_JOBSGEGDIFAILED);
         SGE_EXIT(1);
      }
      if (lGetUlong(aep=lFirst(alp), AN_status) != STATUS_OK) {
         printf("%s\n", lGetString(aep, AN_text));
         SGE_EXIT(1);
      }
      lFreeList(&alp);

      /*
       * debug output to perform testsuite tests
       */
      if (sge_getenv("_SGE_TEST_QSTAT_JOB_STATES") != NULL) {
         fprintf(stderr, "_SGE_TEST_QSTAT_JOB_STATES: jobs_received=%d\n", 
                 lGetNumberOfElem(*job_l));
      }
   }

   /* --- job zombies */
   if (zombie_l && show_zombies) {
      alp = sge_gdi_extract_answer(SGE_GDI_GET, SGE_ZOMBIE_LIST, z_id, mal,
         zombie_l);
      if (!alp) {
         printf("%s\n", MSG_GDI_JOBZOMBIESSGEGDIFAILED);
         SGE_EXIT(1);
      }
      if (lGetUlong(aep=lFirst(alp), AN_status) != STATUS_OK) {
         printf("%s\n", lGetString(aep, AN_text));
         SGE_EXIT(1);
      }
      lFreeList(&alp);
   }

   /* --- complex */
   alp = sge_gdi_extract_answer(SGE_GDI_GET, SGE_CENTRY_LIST, ce_id,
                                mal, centry_l);
   if (!alp) {
      printf("%s\n", MSG_GDI_COMPLEXSGEGDIFAILED);
      SGE_EXIT(1);
   }
   if (lGetUlong(aep=lFirst(alp), AN_status) != STATUS_OK) {
      printf("%s\n", lGetString(aep, AN_text));
      SGE_EXIT(1);
   }
   lFreeList(&alp);

   /* --- exec host */
   alp = sge_gdi_extract_answer(SGE_GDI_GET, SGE_EXECHOST_LIST, eh_id, 
                                 mal, exechost_l);
   if (!alp) {
      printf("%s\n", MSG_GDI_EXECHOSTSGEGDIFAILED);
      SGE_EXIT(1);
   }
   if (lGetUlong(aep=lFirst(alp), AN_status) != STATUS_OK) {
      printf("%s\n", lGetString(aep, AN_text));
      SGE_EXIT(1);
   }
   lFreeList(&alp);

   /* --- pe */
   if (pe_l) {
      alp = sge_gdi_extract_answer(SGE_GDI_GET, SGE_PE_LIST, pe_id, 
                                    mal, pe_l);
      if (!alp) {
         printf("%s\n", MSG_GDI_PESGEGDIFAILED);
         SGE_EXIT(1);
      }
      if (lGetUlong(aep=lFirst(alp), AN_status) != STATUS_OK) {
         printf("%s\n", lGetString(aep, AN_text));
         SGE_EXIT(1);
      }
      lFreeList(&alp);
   }

   /* --- ckpt */
   if (ckpt_l) {
      alp = sge_gdi_extract_answer(SGE_GDI_GET, SGE_CKPT_LIST, ckpt_id, 
                                    mal, ckpt_l);
      if (!alp) {
         printf("%s\n", MSG_GDI_CKPTSGEGDIFAILED);
         SGE_EXIT(1);
      }
      if (lGetUlong(aep=lFirst(alp), AN_status) != STATUS_OK) {
         printf("%s\n", lGetString(aep, AN_text));
         SGE_EXIT(1);
      }
      lFreeList(&alp);
   }

   /* --- acl */
   if (acl_l) {
      alp = sge_gdi_extract_answer(SGE_GDI_GET, SGE_USERSET_LIST, acl_id, 
                                    mal, acl_l);
      if (!alp) {
         printf("%s\n", MSG_GDI_USERSETSGEGDIFAILED);
         SGE_EXIT(1);
      }
      if (lGetUlong(aep=lFirst(alp), AN_status) != STATUS_OK) {
         printf("%s\n", lGetString(aep, AN_text));
         SGE_EXIT(1);
      }
      lFreeList(&alp);
   }

   /* --- project */
   if (project_l) {
      alp = sge_gdi_extract_answer(SGE_GDI_GET, SGE_PROJECT_LIST, up_id, 
                                    mal, project_l);
      if (!alp) {
         printf("%s\n", MSG_GDI_PROJECTSGEGDIFAILED);
         SGE_EXIT(1);
      }
      if (lGetUlong(aep=lFirst(alp), AN_status) != STATUS_OK) {
         printf("%s\n", lGetString(aep, AN_text));
         SGE_EXIT(1);
      }
      lFreeList(&alp);
   }


   /* --- scheduler configuration */
   alp = sge_gdi_extract_answer(SGE_GDI_GET, SGE_SC_LIST, sc_id, mal, sc_l);
   if (!alp) {
      printf("%s\n", MSG_GDI_SCHEDDCONFIGSGEGDIFAILED);
      SGE_EXIT(1);
   }
   if (lGetUlong(aep=lFirst(alp), AN_status) != STATUS_OK) {
      printf("%s\n", lGetString(aep, AN_text));
      SGE_EXIT(1);
   }

   lFreeList(&alp);

   /* --- hgrp */
   alp = sge_gdi_extract_answer(SGE_GDI_GET, SGE_HGROUP_LIST, hgrp_id, mal, 
                                hgrp_l);
   if (!alp) {
      printf("%s\n", MSG_GDI_HGRPCONFIGGDIFAILED);
      SGE_EXIT(1);
   }
   if (lGetUlong(aep=lFirst(alp), AN_status) != STATUS_OK) {
      printf("%s\n", lGetString(aep, AN_text));
      SGE_EXIT(1);
   }
   lFreeList(&alp);

   /* -- apply global configuration for sge_hostcmp() scheme */
   alp = sge_gdi_extract_answer(SGE_GDI_GET, SGE_CONFIG_LIST, gc_id, mal, &conf_l);
   if (!alp) {
      printf("%s\n", MSG_GDI_GLOBALCONFIGGDIFAILED);
      SGE_EXIT(1);
   }
   if (lGetUlong(aep=lFirst(alp), AN_status) != STATUS_OK) {
      printf("%s\n", lGetString(aep, AN_text));
      SGE_EXIT(1);
   }
   if (lFirst(conf_l)) {
      lListElem *local = NULL;
      merge_configuration(lFirst(conf_l), local, NULL);
   }
   lFreeList(&alp);
   lFreeList(&conf_l);
   lFreeList(&mal);

   DEXIT;
   return;
}

/****
 **** sge_parse_qstat (static)
 ****
 **** 'stage 2' parsing of qstat-options. Gets the options from
 **** ppcmdline, sets the full and empry_qs flags and puts the
 **** queue/res/user-arguments into the lists.
 ****/
static lList *sge_parse_qstat(
lList **ppcmdline,
lList **pplresource,
lList **pplqresource,
lList **pplqueueref,
lList **ppluser,
lList **pplqueue_user,
lList **pplpe,
u_long32 *pfull,
u_long32 *explain_bits,
char **hostname,
u_long32 *job_info,
u_long32 *group_opt,
u_long32 *queue_states,
lList **ppljid, 
u_long32 *isXML
) {
   stringT str;
   lList *alp = NULL;
   u_long32 helpflag;
   int usageshowed = 0;
   char *argstr;
   u_long32 full = 0;
   lList *plstringopt = NULL; 


   DENTER(TOP_LAYER, "sge_parse_qstat");

   qstat_filter_add_core_attributes();

   /* Loop over all options. Only valid options can be in the
      ppcmdline list. 
   */
   while(lGetNumberOfElem(*ppcmdline))
   {
      if(parse_flag(ppcmdline, "-help",  &alp, &helpflag)) {
         usageshowed = qstat_usage(qselect_mode, stdout, NULL);
         DEXIT;
         SGE_EXIT(0);
         break;
      }

      while (parse_string(ppcmdline, "-j", &alp, &argstr)) {
         *job_info = 1;
         if (argstr) {
            if (*ppljid) {
               lFreeList(ppljid);
            }
            str_list_parse_from_string(ppljid, argstr, ",");
            FREE(argstr);
         }
         continue;
      }

      while (parse_flag(ppcmdline, "-xml", &alp, isXML)){
         qstat_filter_add_xml_attributes();
         continue;
      }
      
      /*
      ** Two additional flags only if MORE_INFO is set:
      ** -dj   dump jobs:  displays full global_job_list 
      ** -dq   dump queue: displays full global_queue_list
      */
      if (getenv("MORE_INFO")) {
         while (parse_flag(ppcmdline, "-dj", &alp, &global_showjobs))
            ;
         
         while (parse_flag(ppcmdline, "-dq", &alp, &global_showqueues))
            ;
      }

      while (parse_flag(ppcmdline, "-ne", &alp, &full)) {
         if (full) {
            (*pfull) |= QSTAT_DISPLAY_NOEMPTYQ;
            full = 0;
         }
         continue;
      }


      while (parse_flag(ppcmdline, "-f", &alp, &full)) {
         if(full) {
            (*pfull) |= QSTAT_DISPLAY_FULL;
            full = 0;
         }
         continue;
      }

      while (parse_string(ppcmdline, "-s", &alp, &argstr)) {
         if (argstr != NULL) {
            static char noflag = '$';
            static char* flags[] = {
               "hu", "hs", "ho", "hj", "ha", "h", "p", "r", "s", "z", "a", NULL
            };
            static u_long32 bits[] = {
               (QSTAT_DISPLAY_USERHOLD|QSTAT_DISPLAY_PENDING), 
               (QSTAT_DISPLAY_SYSTEMHOLD|QSTAT_DISPLAY_PENDING), 
               (QSTAT_DISPLAY_OPERATORHOLD|QSTAT_DISPLAY_PENDING), 
               (QSTAT_DISPLAY_JOBHOLD|QSTAT_DISPLAY_PENDING), 
               (QSTAT_DISPLAY_STARTTIMEHOLD|QSTAT_DISPLAY_PENDING), 
               (QSTAT_DISPLAY_HOLD|QSTAT_DISPLAY_PENDING), 
               QSTAT_DISPLAY_PENDING,
               QSTAT_DISPLAY_RUNNING, 
               QSTAT_DISPLAY_SUSPENDED, 
               QSTAT_DISPLAY_ZOMBIES,
               (QSTAT_DISPLAY_PENDING|QSTAT_DISPLAY_RUNNING|
                QSTAT_DISPLAY_SUSPENDED),
               0 
            };
            int i, j;
            char *s_switch;
            u_long32 rm_bits = 0;
            
            /* initialize bitmask */
            for (j=0; flags[j]; j++) 
               rm_bits |= bits[j];
            (*pfull) &= ~rm_bits;
            
            /* 
             * search each 'flag' in argstr
             * if we find the whole string we will set the corresponding 
             * bits in '*pfull'
             */
            for (i=0, s_switch=flags[i]; s_switch != NULL; i++, s_switch=flags[i]) {
               for (j=0; argstr[j]; j++) { 
                  if ((argstr[j] == flags[i][0] && argstr[j] != noflag)) {
                     if ((strlen(flags[i]) == 2) && argstr[j+1] && (argstr[j+1] == flags[i][1])) {
                        argstr[j] = noflag;
                        argstr[j+1] = noflag;
                        (*pfull) |= bits[i];
                        break;
                     } else if ((strlen(flags[i]) == 1)){
                        argstr[j] = noflag;
                        (*pfull) |= bits[i];
                        break;
                     }
                  }
               }
            }

            /* search for invalid options */
            for (j=0; argstr[j]; j++) {
               if (argstr[j] != noflag) {
                  sprintf(str, "%s\n", MSG_OPTIONS_WRONGARGUMENTTOSOPT);
                  if (!usageshowed)
                     qstat_usage(qselect_mode, stderr, NULL);
                  answer_list_add(&alp, str, 
                                  STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
                  DEXIT;
                  return alp;
               }
            }

            FREE(argstr);
         }
         continue;
      }

      while (parse_string(ppcmdline, "-explain", &alp, &argstr)) {
         u_long32 filter = QI_AMBIGUOUS | QI_ALARM | QI_SUSPEND_ALARM | QI_ERROR;
         *explain_bits = qinstance_state_from_string(argstr, &alp, filter);
         (*pfull) |= QSTAT_DISPLAY_FULL;
         FREE(argstr);
         continue;
      }
       
      while (parse_string(ppcmdline, "-F", &alp, &argstr)) {
         (*pfull) |= QSTAT_DISPLAY_QRESOURCES|QSTAT_DISPLAY_FULL;
         if (argstr) {
            if (*pplqresource) {
               lFreeList(pplqresource);
            }
            *pplqresource = centry_list_parse_from_string(*pplqresource, argstr, false);
            FREE(argstr);
         }
         continue;
      }

      while (parse_flag(ppcmdline, "-ext", &alp, &full)) {
         qstat_filter_add_ext_attributes();
         if (full) {
            (*pfull) |= QSTAT_DISPLAY_EXTENDED;
            full = 0;
         }
         continue;
      }

      if (!qselect_mode ) {
         while (parse_flag(ppcmdline, "-urg", &alp, &full)) {
            qstat_filter_add_urg_attributes(); 
            if (full) {
               (*pfull) |= QSTAT_DISPLAY_URGENCY;
               full = 0;
            }
            continue;
         }
      }

      if (!qselect_mode ) {
         while (parse_flag(ppcmdline, "-pri", &alp, &full)) {
            qstat_filter_add_pri_attributes();
            while (full) {
               (*pfull) |= QSTAT_DISPLAY_PRIORITY;
               full = 0;
            }
            continue;
         }
      }

      while (parse_flag(ppcmdline, "-r", &alp, &full)) {
         qstat_filter_add_r_attributes();
         while (full) {
            (*pfull) |= QSTAT_DISPLAY_RESOURCES;
            full = 0;
         }
         continue;
      }

      while (parse_flag(ppcmdline, "-t", &alp, &full)) {
         qstat_filter_add_t_attributes();
         if (full) {
            (*pfull) |= QSTAT_DISPLAY_TASKS;
            *group_opt |= GROUP_NO_PETASK_GROUPS;
            full = 0;
         }
         continue;
      }

      while (parse_string(ppcmdline, "-qs", &alp, &argstr)) {
         u_long32 filter = 0xFFFFFFFF;
         *queue_states = qinstance_state_from_string(argstr, &alp, filter);
         FREE(argstr);
         continue;
      }

      while (parse_string(ppcmdline, "-l", &alp, &argstr)) {
         qstat_filter_add_l_attributes();
         *pplresource = centry_list_parse_from_string(*pplresource, argstr, false);
         FREE(argstr);
         continue;
      }

      while (parse_multi_stringlist(ppcmdline, "-u", &alp, ppluser, ST_Type, ST_name)) {
         continue;
      }
      
      while (parse_multi_stringlist(ppcmdline, "-U", &alp, pplqueue_user, ST_Type, ST_name)) {
         qstat_filter_add_U_attributes();
         continue;
      }   
      
      while (parse_multi_stringlist(ppcmdline, "-pe", &alp, pplpe, ST_Type, ST_name)) {
         qstat_filter_add_pe_attributes();
         continue;
      }   
      
      while (parse_multi_stringlist(ppcmdline, "-q", &alp, pplqueueref, QR_Type, QR_name)) {
         qstat_filter_add_q_attributes();
         continue;
      }

      while (parse_multi_stringlist(ppcmdline, "-g", &alp, &plstringopt, ST_Type, ST_name)) {
         *group_opt |= parse_group_options(plstringopt, &alp);
         lFreeList(&plstringopt);    
         continue;
      }
   }

   if (lGetNumberOfElem(*ppcmdline)) {
     sprintf(str, "%s\n", MSG_PARSE_TOOMANYOPTIONS);
     if (!usageshowed)
        qstat_usage(qselect_mode, stderr, NULL);
     answer_list_add(&alp, str, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
     DEXIT;
     return alp;
   }

   DEXIT;
   return alp;
}

/*
** qstat_show_job
** displays information about a given job
** to be extended
**
** returns 0 on success, non-zero on failure
*/
static int qstat_show_job(
lList *jid_list,
u_long32 isXML
) {
   lListElem *j_elem = 0;
   lList* jlp = NULL;
   lList* ilp = NULL;
   lListElem* aep = NULL;
   lCondition *where = NULL, *newcp = NULL;
   lEnumeration* what = NULL;
   lList* alp = NULL;
   bool schedd_info = true;
   bool jobs_exist = true;
   lListElem* mes;

   DENTER(TOP_LAYER, "qstat_show_job");

   /* get job scheduling information */
   what = lWhat("%T(ALL)", SME_Type);
   alp = sge_gdi(SGE_JOB_SCHEDD_INFO, SGE_GDI_GET, &ilp, NULL, what);
   lFreeWhat(&what);

   if (!isXML){
      for_each(aep, alp) {
         if (lGetUlong(aep, AN_status) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            schedd_info = false;
         }
      }
      lFreeList(&alp);
   }

   /* build 'where' for all jobs */
   where = NULL;
   for_each(j_elem, jid_list) {
      const char *job_name = lGetString(j_elem, ST_name);

      if (isdigit(job_name[0])){
         u_long32 jid = atol(lGetString(j_elem, ST_name));
         newcp = lWhere("%T(%I==%u)", JB_Type, JB_job_number, jid);
      }
      else {
         newcp = lWhere("%T(%I p= %s)", JB_Type, JB_job_name, job_name);
      }
      if (newcp){ 
         if (!where)
            where = newcp;
         else
            where = lOrWhere(where, newcp);
      }   
   }
   what = lWhat("%T(ALL)", JB_Type);
   /* get job list */
   alp = sge_gdi(SGE_JOB_LIST, SGE_GDI_GET, &jlp, where, what);
   lFreeWhere(&where);
   lFreeWhat(&what);

   if (isXML){
      xml_qstat_show_job(&jlp, &ilp,  &alp, &jid_list);
   
      lFreeList(&jlp);
      lFreeList(&alp);
      lFreeList(&jid_list);
      DEXIT;
      return 0;
   }

   for_each(aep, alp) {
      if (lGetUlong(aep, AN_status) != STATUS_OK) {
         fprintf(stderr, "%s\n", lGetString(aep, AN_text));
         jobs_exist = false;
      }
   }
   lFreeList(&alp);
   if(!jobs_exist) {

      DEXIT;
      return 1;
   }

   /* does jlp contain all information we requested? */
   if (lGetNumberOfElem(jlp) == 0) {
      lListElem *elem1, *elem2;
      int first_time = 1;

      for_each(elem1, jlp) {
         char buffer[256];
 
         sprintf(buffer, sge_U32CFormat, sge_u32c(lGetUlong(elem1, JB_job_number)));   
         elem2 = lGetElemStr(jid_list, ST_name, buffer);     
         
         if (elem2) {
            lDechainElem(jid_list, elem2);
            lFreeElem(&elem2);
         }    
      }
      fprintf(stderr, "%s\n", MSG_QSTAT_FOLLOWINGDONOTEXIST);
      for_each(elem1, jid_list) {
         if (!first_time) {
            fprintf(stderr, ", "); 
         }
         first_time = 0;
         fprintf(stderr, "%s", lGetString(elem1, ST_name));
      }
      fprintf(stderr, "\n");
      DEXIT;
      SGE_EXIT(1);
   }

   /* print scheduler job information and global scheduler info */
   for_each (j_elem, jlp) {
      u_long32 jid = lGetUlong(j_elem, JB_job_number);
      lListElem *sme;

      printf("==============================================================\n");
      /* print job information */
      cull_show_job(j_elem, 0);
      
      /* print scheduling information */
      if (schedd_info && (sme = lFirst(ilp))) {
         int first_run = 1;

         if (sme) {
            /* global schduling info */
            for_each (mes, lGetList(sme, SME_global_message_list)) {
               if (first_run) {
                  printf("%s:            ",MSG_SCHEDD_SCHEDULINGINFO);
                  first_run = 0;
               }
               else
                  printf("%s", "                            ");
               printf("%s\n", lGetString(mes, MES_message));
            }

            /* job scheduling info */
            for_each(mes, lGetList(sme, SME_message_list)) {
               lListElem *mes_jid;

               for_each(mes_jid, lGetList(mes, MES_job_number_list)) {
                  if (lGetUlong(mes_jid, ULNG) == jid) {
                     if (first_run) {
                        printf("%s:            ",MSG_SCHEDD_SCHEDULINGINFO);
                        first_run = 0;
                     } else
                        printf("%s", "                            ");
                     printf("%s\n", lGetString(mes, MES_message));
                  }
               }
            }
         }
      }
   }

   lFreeList(&ilp);
   lFreeList(&jlp);
   DEXIT;
   return 0;
}

static int qstat_show_job_info(u_long32 isXML)
{
   lList *ilp = NULL, *mlp = NULL;
   lListElem* aep = NULL;
   lEnumeration* what = NULL;
   lList* alp = NULL;
   bool schedd_info = true;
   lListElem* mes;
   int initialized = 0;
   u_long32 last_jid = 0;
   u_long32 last_mid = 0;
   char text[256], ltext[256];
   int ids_per_line = 0;
   int first_run = 1;
   int first_row = 1;
   lListElem *sme;
   lListElem *jid_ulng = NULL; 

   DENTER(TOP_LAYER, "qstat_show_job_info");

   /* get job scheduling information */
   what = lWhat("%T(ALL)", SME_Type);
   alp = sge_gdi(SGE_JOB_SCHEDD_INFO, SGE_GDI_GET, &ilp, NULL, what);
   lFreeWhat(&what);
   if (isXML){
      xml_qstat_show_job_info(&ilp, &alp);
   }
   else {
      for_each(aep, alp) {
         if (lGetUlong(aep, AN_status) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            schedd_info = false;
         }
      }
      lFreeList(&alp);
      if (!schedd_info) {
         DEXIT;
         return 1;
      }

      sme = lFirst(ilp);
      if (sme) {
         /* print global schduling info */
         first_run = 1;
         for_each (mes, lGetList(sme, SME_global_message_list)) {
            if (first_run) {
               printf("%s:            ",MSG_SCHEDD_SCHEDULINGINFO);
               first_run = 0;
            }
            else
               printf("%s", "                            ");
            printf("%s\n", lGetString(mes, MES_message));
         }
         if (!first_run)
            printf("\n");

         first_run = 1;

         mlp = lGetList(sme, SME_message_list);
         lPSortList (mlp, "I+", MES_message_number);

         /* 
          * Remove all jids which have more than one entry for a MES_message_number
          * After this step the MES_messages are not correct anymore
          * We do not need this messages for the summary output
          */
         {
            lListElem *flt_msg, *flt_nxt_msg;
            lList *new_list;
            lListElem *ref_msg, *ref_jid;

            new_list = lCreateList("filtered message list", MES_Type);

            flt_nxt_msg = lFirst(mlp);
            while ((flt_msg = flt_nxt_msg)) {
               lListElem *flt_jid, * flt_nxt_jid;
               int found_msg, found_jid;

               flt_nxt_msg = lNext(flt_msg);
               found_msg = 0;
               for_each(ref_msg, new_list) {
                  if (lGetUlong(ref_msg, MES_message_number) == 
                      lGetUlong(flt_msg, MES_message_number)) {
                 
                  flt_nxt_jid = lFirst(lGetList(flt_msg, MES_job_number_list));
                  while ((flt_jid = flt_nxt_jid)) {
                     flt_nxt_jid = lNext(flt_jid);
                    
                     found_jid = 0; 
                     for_each(ref_jid, lGetList(ref_msg, MES_job_number_list)) {
                        if (lGetUlong(ref_jid, ULNG) == 
                            lGetUlong(flt_jid, ULNG)) {
                           lRemoveElem(lGetList(flt_msg, MES_job_number_list), &flt_jid);
                           found_jid = 1;
                           break;
                        }
                     }
                     if (!found_jid) { 
                        lDechainElem(lGetList(flt_msg, MES_job_number_list), flt_jid);
                        lAppendElem(lGetList(ref_msg, MES_job_number_list), flt_jid);
                     } 
                  }
                  found_msg = 1;
               }
            }
            if (!found_msg) {
               lDechainElem(mlp, flt_msg);
               lAppendElem(new_list, flt_msg);
            }
         }
         lSetList(sme, SME_message_list, new_list);
         mlp = new_list;
      }

      text[0]=0;
      for_each(mes, mlp) {
         lPSortList (lGetList(mes, MES_job_number_list), "I+", ULNG);

         for_each(jid_ulng, lGetList(mes, MES_job_number_list)) {
            u_long32 mid;
            u_long32 jid = 0;
            int skip = 0;
            int header = 0;

            mid = lGetUlong(mes, MES_message_number);
            jid = lGetUlong(jid_ulng, ULNG);

            if (initialized) {
               if (last_mid == mid && last_jid == jid)
                  skip = 1;
               else if (last_mid != mid)
                     header = 1;
               }
               else {
                  initialized = 1;
                  header = 1;
            }

               if (strlen(text) >= MAX_LINE_LEN || ids_per_line >= MAX_IDS_PER_LINE || header) {
                  printf("%s", text);
                  text[0] = 0;
                  ids_per_line = 0;
                  first_row = 0;
               }

               if (header) {
                  if (!first_run)
                     printf("\n\n");
                  else
                     first_run = 0;
                  printf("%s\n", sge_schedd_text(mid+SCHEDD_INFO_OFFSET));
                  first_row = 1;
               }

               if (!skip) {
                  if (ids_per_line == 0)
                     if (first_row)
                        strcat(text, "\t");
                     else
                        strcat(text, ",\n\t");
                  else
                     strcat(text, ",\t");
                  sprintf(ltext, sge_u32, jid);
                  strcat(text, ltext);
                  ids_per_line++;
               }

               last_jid = jid;
               last_mid = mid;
            }
         }
         if (text[0] != 0)
            printf("%s\n", text);
      }
   }

   lFreeList(&ilp);
   
   DEXIT;
   return 0;
}

