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
#include <sys/stat.h>
#include <fnmatch.h>

#include "sge_unistd.h"
#include "sgermon.h"
#include "symbols.h"
#include "sge.h"
#include "sge_time.h"
#include "sge_log.h"
#include "sge_all_listsL.h"
#include "sge_host.h"
#include "sge_sched.h"
#include "cull_sort.h"
#include "usage.h"
#include "sge_feature.h"
#include "parse.h"
#include "sge_prog.h"
#include "sge_parse_num_par.h"
#include "sge_string.h"
#include "show_job.h"
#include "sge_dstring.h"
#include "qstat_printing.h"
#include "sge_range.h"
#include "sig_handlers.h"
#include "msg_clients_common.h"
#include "sge_job.h"
#include "get_path.h"
#include "sge_var.h"
#include "sge_answer.h"
#include "sge_qinstance.h"
#include "sge_qinstance_state.h"
#include "sge_qinstance_type.h"
#include "sge_urgency.h"
#include "sge_pe.h"
#include "sge_ulong.h"

static int sge_print_job(lListElem *job, lListElem *jatep, lListElem *qep, int print_jobid, 
                         char *master, dstring *task_str, u_long32 full_listing, int
                         slots, int slot, lList *ehl, lList *cl, const lList *pe_list, 
                         char *intend, u_long32 group_opt, int slots_per_line, 
                         int queue_name_length);

static int sge_print_subtask(lListElem *job, lListElem *ja_task, lListElem *task, int print_hdr, int indent);

static int sge_print_jobs_not_enrolled(lListElem *job, lListElem *qep,
                           int print_jobid, char *master, u_long32 full_listing,
                           int slots, int slot, lList *exechost_list,
                           lList *centry_list, const lList *pe_list, char *indent,
                           u_long32 sge_ext, u_long32 group_opt, int queue_name_length);

static void sge_printf_header(u_long32 full_listing, u_long32 sge_ext);

static char hashes[] = "##############################################################################################################";

int 
sge_print_queue(lListElem *q, lList *exechost_list, lList *centry_list,
                u_long32 full_listing, lList *qresource_list, 
                u_long32 explain_bits, int longest_queue_length) {
   char to_print[80];
   char arch_string[80];
   double load_avg;
   static int first_time = 1;
   int sge_ext;
   char *load_avg_str;
   char load_alarm_reason[MAX_STRING_SIZE];
   char suspend_alarm_reason[MAX_STRING_SIZE];
   dstring queue_name_buffer = DSTRING_INIT;
   const char *queue_name = NULL;
   bool is_load_value;
   bool has_value_from_object; 
   u_long32 interval;

   DENTER(TOP_LAYER, "sge_print_queue");

   if(longest_queue_length<30)
      longest_queue_length=30;

   *load_alarm_reason = 0;
   *suspend_alarm_reason = 0;
   queue_name = qinstance_get_name(q, &queue_name_buffer);

   /* make it possible to display any load value in qstat output */
   if (!(load_avg_str=getenv("SGE_LOAD_AVG")) || !strlen(load_avg_str))
      load_avg_str = LOAD_ATTR_LOAD_AVG;

   if (!(full_listing & QSTAT_DISPLAY_FULL)) {
      DEXIT;
      return 1;
   }

   /* compute the load and check for alarm states */

   is_load_value = sge_get_double_qattr(&load_avg, load_avg_str, q, exechost_list, centry_list, &has_value_from_object);
   if (sge_load_alarm(NULL, q, lGetList(q, QU_load_thresholds), exechost_list, centry_list, NULL, true)) {
      qinstance_state_set_alarm(q, true);
      sge_load_alarm_reason(q, lGetList(q, QU_load_thresholds), exechost_list, 
                            centry_list, load_alarm_reason, 
                            MAX_STRING_SIZE - 1, "load");
   }
   parse_ulong_val(NULL, &interval, TYPE_TIM,
                   lGetString(q, QU_suspend_interval), NULL, 0);
   if (lGetUlong(q, QU_nsuspend) != 0 &&
       interval != 0 &&
       sge_load_alarm(NULL, q, lGetList(q, QU_suspend_thresholds), exechost_list, centry_list, NULL, false)) {
      qinstance_state_set_suspend_alarm(q, true);
      sge_load_alarm_reason(q, lGetList(q, QU_suspend_thresholds), 
                            exechost_list, centry_list, suspend_alarm_reason, 
                            MAX_STRING_SIZE - 1, "suspend");
   }

   if (first_time) {
      char temp[20];
      first_time = 0;
      sprintf(temp, "%%-%d.%ds", longest_queue_length, longest_queue_length);

      printf(temp,MSG_QSTAT_PRT_QUEUENAME); 
      
      printf(" %-5.5s %-9.9s %-8.8s %-13.13s %s\n", 
            MSG_QSTAT_PRT_QTYPE, 
            MSG_QSTAT_PRT_USEDTOT,
            load_avg_str,
            LOAD_ATTR_ARCH,
            MSG_QSTAT_PRT_STATES);
   }

   sge_ext = (full_listing & QSTAT_DISPLAY_EXTENDED);

   printf("----------------------------------------------------------------------------%s", 
      sge_ext?"------------------------------------------------------------------------------------------------------------":"");
   {
      int i;
      for(i=0; i< longest_queue_length - 30; i++)
         printf("-");
      printf("\n");
   }

   {
      char temp[20];
      sprintf(temp, "%%-%d.%ds ", longest_queue_length, longest_queue_length);
      printf(temp, queue_name);
   }
      
   {
      dstring type_string = DSTRING_INIT;

      qinstance_print_qtype_to_dstring(q, &type_string, true);
      printf("%-5.5s ", sge_dstring_get_string(&type_string)); 
      sge_dstring_free(&type_string);
   }

   /* number of used/free slots */
   sprintf(to_print, "%d/%d ", 
      qinstance_slots_used(q),
      (int)lGetUlong(q, QU_job_slots));
   printf("%-9.9s ", to_print);   

   /* load avg */
   if (!is_load_value) {
      if (has_value_from_object) {
         sprintf(to_print, "%2.2f ", load_avg);
      } else {
         sprintf(to_print, "---  ");
      }
   } else {
      sprintf(to_print, "-NA- ");
   }
   
   printf("%-8.8s ", to_print);   

   /* arch */
   if (!sge_get_string_qattr(arch_string, sizeof(arch_string)-1, LOAD_ATTR_ARCH, 
         q, exechost_list, centry_list))
      sprintf(to_print, "%s ", arch_string);
   else
      sprintf(to_print, "-NA- ");
   printf("%-13.13s ", to_print);   

   {
      dstring state_string = DSTRING_INIT;

      qinstance_state_append_to_dstring(q, &state_string);
      printf("%s", sge_dstring_get_string(&state_string)); 
      sge_dstring_free(&state_string);
   }
   printf("\n");

   if((full_listing & QSTAT_DISPLAY_ALARMREASON)) {
      if(*load_alarm_reason) {
	      printf(load_alarm_reason);
      }
      if(*suspend_alarm_reason) {
	      printf(suspend_alarm_reason);
      }
   }

   if ((explain_bits & QI_ALARM) > 0) {
      if(*load_alarm_reason) {
         printf(load_alarm_reason);
      }
   }
   if ((explain_bits & QI_SUSPEND_ALARM) > 0) {
      if(*suspend_alarm_reason) {
         printf(suspend_alarm_reason);
      }
   }
   if (explain_bits != QI_DEFAULT) {
      lList *qim_list = lGetList(q, QU_message_list);
      lListElem *qim = NULL;

      for_each(qim, qim_list) {
         u_long32 type = lGetUlong(qim, QIM_type);

         if ((explain_bits & QI_AMBIGUOUS) == type || 
             (explain_bits & QI_ERROR) == type) {
            const char *message = lGetString(qim, QIM_message);

            printf("\t");
            printf(message);
         }
      }
   }

   /* view (selected) resources of queue in case of -F [attr,attr,..] */ 
   if ((full_listing & QSTAT_DISPLAY_QRESOURCES)) {
      dstring resource_string = DSTRING_INIT;
      lList *rlp;
      lListElem *rep;
      char dom[5];
      u_long32 dominant = 0;
      const char *s;

      rlp = NULL;

      queue_complexes2scheduler(&rlp, q, exechost_list, centry_list);

      for_each (rep , rlp) {

         /* we had a -F request */
         if (qresource_list) {
            lListElem *qres;

            qres = lGetElemStr(qresource_list, CE_name, 
                               lGetString(rep, CE_name));
            if (qres == NULL) {
               qres = lGetElemStr(qresource_list, CE_name,
                               lGetString(rep, CE_shortcut));
            }

            /* if this complex variable wasn't requested with -F, skip it */
            if (qres == NULL) {
               continue ;
            }
         }
         sge_dstring_clear(&resource_string);
         s = sge_get_dominant_stringval(rep, &dominant, &resource_string);
         monitor_dominance(dom, dominant); 
         printf("\t%s:%s=%s\n", dom, lGetString(rep, CE_name), s);
/*
         switch(lGetUlong(rep, CE_valtype)) {
         case TYPE_INT:  
         case TYPE_TIM:  
         case TYPE_MEM:  
         case TYPE_BOO:  
         case TYPE_DOUBLE:  
            printf("\t%s:%s=%s\n", dom, lGetString(rep, CE_name), s);
            break;
         }
*/         
      }

      lFreeList(rlp);
      sge_dstring_free(&resource_string);

   }

   DEXIT;
   return 1;
}

const char* sge_get_dominant_stringval(lListElem *rep, u_long32 *dominant_p, dstring *resource_string_p)
{ 
   const char *s = NULL;
   
   u_long32 type = lGetUlong(rep, CE_valtype);
   switch (type) {
   case TYPE_HOST:   
   case TYPE_STR:   
   case TYPE_CSTR:  
   case TYPE_RESTR:
      if (!(lGetUlong(rep, CE_pj_dominant)&DOMINANT_TYPE_VALUE)) {
         *dominant_p = lGetUlong(rep, CE_pj_dominant);
         s = lGetString(rep, CE_pj_stringval);
      } else {
         *dominant_p = lGetUlong(rep, CE_dominant);
         s = lGetString(rep, CE_stringval);
      }
      break;
   case TYPE_TIM: 

      if (!(lGetUlong(rep, CE_pj_dominant)&DOMINANT_TYPE_VALUE)) {
         double val = lGetDouble(rep, CE_pj_doubleval);

         *dominant_p = lGetUlong(rep, CE_pj_dominant);
         double_print_time_to_dstring(val, resource_string_p);
         s = sge_dstring_get_string(resource_string_p);
      } else {
         double val = lGetDouble(rep, CE_doubleval);

         *dominant_p = lGetUlong(rep, CE_dominant);
         double_print_time_to_dstring(val, resource_string_p);
         s = sge_dstring_get_string(resource_string_p);
      }
      break;
   case TYPE_MEM:

      if (!(lGetUlong(rep, CE_pj_dominant)&DOMINANT_TYPE_VALUE)) {
         double val = lGetDouble(rep, CE_pj_doubleval);

         *dominant_p = lGetUlong(rep, CE_pj_dominant);
         double_print_memory_to_dstring(val, resource_string_p);
         s = sge_dstring_get_string(resource_string_p);
      } else {
         double val = lGetDouble(rep, CE_doubleval);

         *dominant_p = lGetUlong(rep, CE_dominant);
         double_print_memory_to_dstring(val, resource_string_p);
         s = sge_dstring_get_string(resource_string_p);
      }
      break;
   case TYPE_INT:   

      if (!(lGetUlong(rep, CE_pj_dominant)&DOMINANT_TYPE_VALUE)) {
         double val = lGetDouble(rep, CE_pj_doubleval);

         *dominant_p = lGetUlong(rep, CE_pj_dominant);
         double_print_int_to_dstring(val, resource_string_p);
         s = sge_dstring_get_string(resource_string_p);
      } else {
         double val = lGetDouble(rep, CE_doubleval);

         *dominant_p = lGetUlong(rep, CE_dominant);
         double_print_int_to_dstring(val, resource_string_p);
         s = sge_dstring_get_string(resource_string_p);
      }
      break;
   default:   

      if (!(lGetUlong(rep, CE_pj_dominant)&DOMINANT_TYPE_VALUE)) {
         double val = lGetDouble(rep, CE_pj_doubleval);

         *dominant_p = lGetUlong(rep, CE_pj_dominant);
         double_print_to_dstring(val, resource_string_p);
         s = sge_dstring_get_string(resource_string_p);
      } else {
         double val = lGetDouble(rep, CE_doubleval);

         *dominant_p = lGetUlong(rep, CE_dominant);
         double_print_to_dstring(val, resource_string_p);
         s = sge_dstring_get_string(resource_string_p);
      }
      break;
   }
   return s;
}

static int sge_print_subtask(
lListElem *job,
lListElem *ja_task,
lListElem *pe_task,  /* NULL, if master task shall be printed */
int print_hdr,
int indent 
) {
   char task_state_string[8];
   u_long32 tstate, tstatus;
   int task_running;
   const char *str;
   lListElem *ep;
   lList *usage_list;
   lList *scaled_usage_list;

   DENTER(TOP_LAYER, "sge_print_subtask");

   /* is sub-task logically running */
   if(pe_task == NULL) {
      tstatus = lGetUlong(ja_task, JAT_status);
      usage_list = lGetList(ja_task, JAT_usage_list);
      scaled_usage_list = lGetList(ja_task, JAT_scaled_usage_list);
   } else {
      tstatus = lGetUlong(pe_task, PET_status);
      usage_list = lGetList(pe_task, PET_usage);
      scaled_usage_list = lGetList(pe_task, PET_scaled_usage);
   }

   task_running = (tstatus==JRUNNING || tstatus==JTRANSFERING);

   if (print_hdr) {
      printf(QSTAT_INDENT "Sub-tasks:           %-12.12s %5.5s %s %-4.4s %-6.6s\n", 
             "task-ID",
             "state",
             USAGE_ATTR_CPU "        " USAGE_ATTR_MEM "     " USAGE_ATTR_IO "     ",
             "stat",
             "failed");
   }

   if(pe_task == NULL) {
      str = "";
   } else {
      str = lGetString(pe_task, PET_id);
   }
   printf("   %s%-12s ", indent?QSTAT_INDENT2:"", str);

   /* move status info into state info */
   tstate = lGetUlong(ja_task, JAT_state);
   if (tstatus==JRUNNING) {
      tstate |= JRUNNING;
      tstate &= ~JTRANSFERING;
   } else if (tstatus==JTRANSFERING) {
      tstate |= JTRANSFERING;
      tstate &= ~JRUNNING;
   } else if (tstatus==JFINISHED) {
      tstate |= JEXITING;
      tstate &= ~(JRUNNING|JTRANSFERING);
   }

   if (lGetList(job, JB_jid_predecessor_list) || lGetUlong(ja_task, JAT_hold)) {
      tstate |= JHELD;
   }

   if (lGetUlong(ja_task, JAT_job_restarted)) {
      tstate &= ~JWAITING;
      tstate |= JMIGRATING;
   }

   /* write states into string */ 
   job_get_state_string(task_state_string, tstate);
   printf("%-5.5s ", task_state_string); 

   {
      lListElem *up;

      /* scaled cpu usage */
      if (!(up = lGetElemStr(scaled_usage_list, UA_name, USAGE_ATTR_CPU))) {
         printf("%-10.10s ", task_running?"NA":""); 
      } else {
         dstring resource_string = DSTRING_INIT;

         double_print_time_to_dstring(lGetDouble(up, UA_value), 
                                      &resource_string);
         printf("%s ", sge_dstring_get_string(&resource_string));
         sge_dstring_free(&resource_string);
      }

      /* scaled mem usage */
      if (!(up = lGetElemStr(scaled_usage_list, UA_name, USAGE_ATTR_MEM))) 
         printf("%-7.7s ", task_running?"NA":""); 
      else
         printf("%-5.5f ", lGetDouble(up, UA_value)); 
  
      /* scaled io usage */
      if (!(up = lGetElemStr(scaled_usage_list, UA_name, USAGE_ATTR_IO))) 
         printf("%-7.7s ", task_running?"NA":""); 
      else
         printf("%-5.5f ", lGetDouble(up, UA_value)); 
   }

   if (tstatus==JFINISHED) {
      ep=lGetElemStr(usage_list, UA_name, "exit_status");

      printf("%-4d", ep ? (int)lGetDouble(ep, UA_value) : 0);
   }

   putchar('\n');

   DEXIT;
   return 0;
}

/*-------------------------------------------------------------------------*/
/* print jobs per queue                                                    */
/*-------------------------------------------------------------------------*/
void sge_print_jobs_queue(
lListElem *qep,
lList *job_list,
const lList *pe_list,
lList *user_list,
lList *ehl,
lList *centry_list,
int print_jobs_of_queue,
u_long32 full_listing,
char *indent,
u_long32 group_opt, 
int queue_name_length 
) {
   int first = 1;
   lListElem *jlep;
   lListElem *jatep;
   lListElem *gdilep;
   u_long32 job_tag;
   u_long32 jid = 0, old_jid;
   u_long32 jataskid = 0, old_jataskid;
   dstring queue_name_buffer = DSTRING_INIT;
   const char *qnm;
   dstring dyn_task_str = DSTRING_INIT;

   DENTER(TOP_LAYER, "sge_print_jobs_queue");

   qnm = qinstance_get_name(qep, &queue_name_buffer);

   for_each(jlep, job_list) {
      int master, i;

      for_each(jatep, lGetList(jlep, JB_ja_tasks)) {
         if (shut_me_down) {
            SGE_EXIT(1);
         }
            
         for_each (gdilep, lGetList(jatep, JAT_granted_destin_identifier_list)) {

            if(!strcmp(lGetString(gdilep, JG_qname), qnm)) {
               int slot_adjust = 0;
               int lines_to_print;
               int slots_per_line, slots_in_queue = lGetUlong(gdilep, JG_slots); 

               if (qinstance_state_is_manual_suspended(qep) ||
                   qinstance_state_is_susp_on_sub(qep) ||
                   qinstance_state_is_cal_suspended(qep)) {
                  u_long32 jstate;

                  jstate = lGetUlong(jatep, JAT_state);
                  jstate &= ~JRUNNING;                 /* unset bit JRUNNING */
                  jstate |= JSUSPENDED_ON_SUBORDINATE; /* set bit JSUSPENDED_ON_SUBORDINATE */
                  lSetUlong(jatep, JAT_state, jstate);
               }
               job_tag = lGetUlong(jatep, JAT_suitable);
               job_tag |= TAG_FOUND_IT;
               lSetUlong(jatep, JAT_suitable, job_tag);

               master = !strcmp(qnm, 
                     lGetString(lFirst(lGetList(jatep, JAT_granted_destin_identifier_list)), JG_qname));

               if (master) {
                  const char *pe_name;
                  lListElem *pe;
                  if (((pe_name=lGetString(jatep, JAT_granted_pe))) &&
                      ((pe=pe_list_locate(pe_list, pe_name))) &&
                      !lGetBool(pe, PE_job_is_first_task))

                      slot_adjust = 1;
               }

               /* job distribution view ? */
               if (!(group_opt & GROUP_NO_PETASK_GROUPS)) {
                  /* no - condensed ouput format */
                  if (!master && !(full_listing & QSTAT_DISPLAY_FULL)) {
                     /* skip all slave outputs except in full display mode */
                     continue;
                  }

                  /* print only on line per job for this queue */
                  lines_to_print = 1;

                  /* always only show the number of job slots represented by the line */
                  if ((full_listing & QSTAT_DISPLAY_FULL))
                     slots_per_line = slots_in_queue;
                  else
                     slots_per_line = sge_granted_slots(lGetList(jatep, JAT_granted_destin_identifier_list));
               } else {
                  /* yes */
                  lines_to_print = (int)slots_in_queue+slot_adjust;
                  slots_per_line = 1;
               }

               for (i=0; i<lines_to_print ;i++) {
                  int already_printed = 0;

                  if (!lGetNumberOfElem(user_list) || 
                     (lGetNumberOfElem(user_list) && (lGetUlong(jatep, JAT_suitable)&TAG_SELECT_IT))) {
                     if (print_jobs_of_queue && (job_tag & TAG_SHOW_IT)) {
                        int different, print_jobid;

                        old_jid = jid;
                        jid = lGetUlong(jlep, JB_job_number);
                        old_jataskid = jataskid;
                        jataskid = lGetUlong(jatep, JAT_task_number);
                        sge_dstring_sprintf(&dyn_task_str, u32, jataskid);
                        different = (jid != old_jid) || (jataskid != old_jataskid);

                        if (different) 
                           print_jobid = 1;
                        else {
                           if (!(full_listing & QSTAT_DISPLAY_RUNNING))
                              print_jobid = master && (i==0);
                           else 
                              print_jobid = 0;
                        }

                        if (!already_printed && (full_listing & QSTAT_DISPLAY_RUNNING) &&
                              (lGetUlong(jatep, JAT_state) & JRUNNING)) {
                           sge_print_job(jlep, jatep, qep, print_jobid,
                              (master && different && (i==0))?"MASTER":"SLAVE", &dyn_task_str, full_listing,
                              slots_in_queue+slot_adjust, i, ehl, centry_list, pe_list, indent, 
                              group_opt, slots_per_line, queue_name_length);   
                           already_printed = 1;
                        }
                        if (!already_printed && (full_listing & QSTAT_DISPLAY_SUSPENDED) &&
                           ((lGetUlong(jatep, JAT_state)&JSUSPENDED) ||
                           (lGetUlong(jatep, JAT_state)&JSUSPENDED_ON_THRESHOLD) ||
                            (lGetUlong(jatep, JAT_state)&JSUSPENDED_ON_SUBORDINATE))) {
                           sge_print_job(jlep, jatep, qep, print_jobid,
                              (master && different && (i==0))?"MASTER":"SLAVE", &dyn_task_str, full_listing,
                              slots_in_queue+slot_adjust, i, ehl, centry_list, pe_list, indent, 
                              group_opt, slots_per_line, queue_name_length);   
                           already_printed = 1;
                        }

                        if (!already_printed && (full_listing & QSTAT_DISPLAY_USERHOLD) &&
                            (lGetUlong(jatep, JAT_hold)&MINUS_H_TGT_USER)) {
                           sge_print_job(jlep, jatep, qep, print_jobid,
                              (master && different && (i==0))?"MASTER":"SLAVE", &dyn_task_str, full_listing,
                              slots_in_queue+slot_adjust, i, ehl, centry_list, pe_list, indent, 
                              group_opt, slots_per_line, queue_name_length);
                           already_printed = 1;
                        }

                        if (!already_printed && (full_listing & QSTAT_DISPLAY_OPERATORHOLD) &&
                            (lGetUlong(jatep, JAT_hold)&MINUS_H_TGT_OPERATOR))  {
                           sge_print_job(jlep, jatep, qep, print_jobid,
                              (master && different && (i==0))?"MASTER":"SLAVE", &dyn_task_str, full_listing,
                              slots_in_queue+slot_adjust, i, ehl, centry_list, pe_list, indent, 
                              group_opt, slots_per_line, queue_name_length);
                           already_printed = 1;
                        }
                            
                        if (!already_printed && (full_listing & QSTAT_DISPLAY_SYSTEMHOLD) &&
                            (lGetUlong(jatep, JAT_hold)&MINUS_H_TGT_SYSTEM)) {
                           sge_print_job(jlep, jatep, qep, print_jobid,
                              (master && different && (i==0))?"MASTER":"SLAVE", &dyn_task_str, full_listing,
                              slots_in_queue+slot_adjust, i, ehl, centry_list, pe_list, indent, 
                              group_opt, slots_per_line, queue_name_length);
                           already_printed = 1;
                        }
                     }
                  }
               }
               first = 0;
            }
         }
      }
   }
   sge_dstring_free(&queue_name_buffer);
   sge_dstring_free(&dyn_task_str);
   DEXIT;
}

/*-------------------------------------------------------------------------*/
/* pending jobs matching the queues                                        */
/*-------------------------------------------------------------------------*/
void sge_print_jobs_pending(
lList *job_list,
const lList *pe_list,
lList *user_list,
lList *ehl,
lList *centry_list,
lSortOrder *so,
u_long32 full_listing,
u_long32 group_opt,
int queue_name_length
) {
   lListElem *nxt, *jep, *jatep, *nxt_jatep;
   int sge_ext;
   dstring dyn_task_str = DSTRING_INIT;
   lList* ja_task_list = NULL;
   int FoundTasks;

   DENTER(TOP_LAYER, "sge_print_jobs_pending");

   sge_ext = (full_listing & QSTAT_DISPLAY_EXTENDED);

   nxt = lFirst(job_list);
   while ((jep=nxt)) {
      nxt = lNext(jep);
      sge_dstring_free(&dyn_task_str);
      nxt_jatep = lFirst(lGetList(jep, JB_ja_tasks));
      FoundTasks = 0;

      while((jatep = nxt_jatep)) { 
         if (shut_me_down) {
            SGE_EXIT(1);
         }   
         nxt_jatep = lNext(jatep);
         if (!(((full_listing & QSTAT_DISPLAY_OPERATORHOLD) && (lGetUlong(jatep, JAT_hold)&MINUS_H_TGT_OPERATOR))  
               ||
             ((full_listing & QSTAT_DISPLAY_USERHOLD) && (lGetUlong(jatep, JAT_hold)&MINUS_H_TGT_USER)) 
               ||
             ((full_listing & QSTAT_DISPLAY_SYSTEMHOLD) && (lGetUlong(jatep, JAT_hold)&MINUS_H_TGT_SYSTEM)) 
               ||
             ((full_listing & QSTAT_DISPLAY_JOBHOLD) && lGetList(jep, JB_jid_predecessor_list))
               ||
             ((full_listing & QSTAT_DISPLAY_STARTTIMEHOLD) && lGetUlong(jep, JB_execution_time))
               ||
             !(full_listing & QSTAT_DISPLAY_HOLD))
            ) {
            break;
         }

         if (!(lGetUlong(jatep, JAT_suitable) & TAG_FOUND_IT) && 
            VALID(JQUEUED, lGetUlong(jatep, JAT_state)) &&
            !VALID(JFINISHED, lGetUlong(jatep, JAT_status))) {
            lSetUlong(jatep, JAT_suitable, 
            lGetUlong(jatep, JAT_suitable)|TAG_FOUND_IT);

            if ((!lGetNumberOfElem(user_list) || 
               (lGetNumberOfElem(user_list) && 
               (lGetUlong(jatep, JAT_suitable)&TAG_SELECT_IT))) &&
               (lGetUlong(jatep, JAT_suitable)&TAG_SHOW_IT)) {

               sge_printf_header((full_listing & QSTAT_DISPLAY_FULL) |
                                 (full_listing & QSTAT_DISPLAY_PENDING), 
                                 sge_ext);

               if ((full_listing & QSTAT_DISPLAY_PENDING) && 
                   (group_opt & GROUP_NO_TASK_GROUPS) > 0) {
                  sge_dstring_sprintf(&dyn_task_str, u32, 
                                    lGetUlong(jatep, JAT_task_number));
                  sge_print_job(jep, jatep, NULL, 1, NULL,
                                &dyn_task_str, full_listing, 0, 0, ehl, centry_list, 
                                pe_list, "", group_opt, 0, queue_name_length);
               } else {
                  if (!ja_task_list) {
                     ja_task_list = lCreateList("", lGetElemDescr(jatep));
                  }
                  lAppendElem(ja_task_list, lCopyElem(jatep));
                  FoundTasks = 1;
               }
            }
         }
      }
      if ((full_listing & QSTAT_DISPLAY_PENDING)  && 
          (group_opt & GROUP_NO_TASK_GROUPS) == 0 && 
          FoundTasks && 
          ja_task_list) {
         lList *task_group = NULL;

         while ((task_group = ja_task_list_split_group(&ja_task_list))) {
            ja_task_list_print_to_string(task_group, &dyn_task_str);

            sge_print_job(jep, lFirst(task_group), NULL, 1, NULL, 
                          &dyn_task_str, full_listing, 0, 0, ehl, 
                          centry_list, pe_list, "", group_opt, 0, queue_name_length);
            task_group = lFreeList(task_group);
            sge_dstring_free(&dyn_task_str);
         }
         ja_task_list = lFreeList(ja_task_list);
      }
      if (jep != nxt && full_listing & QSTAT_DISPLAY_PENDING) {

         sge_print_jobs_not_enrolled(jep, NULL, 1, NULL, full_listing,
                                     0, 0, ehl, centry_list, pe_list, "", sge_ext, 
                                     group_opt, queue_name_length);
      }
   }
   sge_dstring_free(&dyn_task_str);
   DEXIT;
}

static void sge_printf_header(u_long32 full_listing, u_long32 sge_ext)
{
   static int first_pending = 1;
   static int first_zombie = 1;

   if ((full_listing & QSTAT_DISPLAY_PENDING) && 
       (full_listing & QSTAT_DISPLAY_FULL)) {
      if (first_pending) {
         first_pending = 0;
         printf("\n############################################################################%s\n",
            sge_ext?hashes:"");
         printf(MSG_QSTAT_PRT_PEDINGJOBS);
         printf("############################################################################%s\n",
            sge_ext?hashes:"");
      }
   } 
   if ((full_listing & QSTAT_DISPLAY_ZOMBIES) &&
       (full_listing & QSTAT_DISPLAY_FULL)) {
      if (first_zombie) {
         first_zombie = 0;
         printf("\n############################################################################%s\n", sge_ext?hashes:"");
         printf(MSG_QSTAT_PRT_FINISHEDJOBS);
         printf("############################################################################%s\n", sge_ext?hashes:""); 
      }
   }
}

static int sge_print_jobs_not_enrolled(lListElem *job, lListElem *qep,
                           int print_jobid, char *master, u_long32 full_listing,
                           int slots, int slot, lList *exechost_list,
                           lList *centry_list, const lList *pe_list, char *indent,
                           u_long32 sge_ext, u_long32 group_opt, int queue_name_length)
{
   lList *range_list[8];         /* RN_Type */
   u_long32 hold_state[8];
   int i;
   dstring ja_task_id_string = DSTRING_INIT;
 
   DENTER(TOP_LAYER, "sge_print_jobs_not_enrolled");

   job_create_hold_id_lists(job, range_list, hold_state); 
   for (i = 0; i <= 7; i++) {
      lList *answer_list = NULL;
      u_long32 first_id;
      int show = 0;

      if (((full_listing & QSTAT_DISPLAY_USERHOLD) && (hold_state[i] & MINUS_H_TGT_USER)) ||
          ((full_listing & QSTAT_DISPLAY_OPERATORHOLD) && (hold_state[i] & MINUS_H_TGT_OPERATOR)) ||
          ((full_listing & QSTAT_DISPLAY_SYSTEMHOLD) && (hold_state[i] & MINUS_H_TGT_SYSTEM)) ||
          ((full_listing & QSTAT_DISPLAY_STARTTIMEHOLD) && (lGetUlong(job, JB_execution_time) > 0)) ||
          ((full_listing & QSTAT_DISPLAY_JOBHOLD) && (lGetList(job, JB_jid_predecessor_list) != 0)) ||
          (!(full_listing & QSTAT_DISPLAY_HOLD))
         ) {
         show = 1;
      }
      if (range_list[i] != NULL && show) { 
         if ((group_opt & GROUP_NO_TASK_GROUPS) == 0) {
            range_list_print_to_string(range_list[i], &ja_task_id_string, 0);
            first_id = range_list_get_first_id(range_list[i], &answer_list);
            if (answer_list_has_error(&answer_list) != 1) {
               lListElem *ja_task = job_get_ja_task_template_hold(job, 
                                                      first_id, hold_state[i]);
               lList *n_h_ids = NULL;
               lList *u_h_ids = NULL;
               lList *o_h_ids = NULL;
               lList *s_h_ids = NULL;

               sge_printf_header(full_listing, sge_ext);
               lXchgList(job, JB_ja_n_h_ids, &n_h_ids);
               lXchgList(job, JB_ja_u_h_ids, &u_h_ids);
               lXchgList(job, JB_ja_o_h_ids, &o_h_ids);
               lXchgList(job, JB_ja_s_h_ids, &s_h_ids);
               sge_print_job(job, ja_task, qep, print_jobid, master,
                             &ja_task_id_string, full_listing, slots, slot,
                             exechost_list, centry_list, pe_list, indent, group_opt, 0, queue_name_length);
               lXchgList(job, JB_ja_n_h_ids, &n_h_ids);
               lXchgList(job, JB_ja_u_h_ids, &u_h_ids);
               lXchgList(job, JB_ja_o_h_ids, &o_h_ids);
               lXchgList(job, JB_ja_s_h_ids, &s_h_ids);
            }
            sge_dstring_free(&ja_task_id_string);
         } else {
            lListElem *range; /* RN_Type */
            
            for_each(range, range_list[i]) {
               u_long32 start, end, step;
               range_get_all_ids(range, &start, &end, &step);
               for (; start <= end; start += step) { 
                  lListElem *ja_task = job_get_ja_task_template_hold(job,
                                                          start, hold_state[i]);
                  sge_dstring_sprintf(&ja_task_id_string, u32, start);
                  sge_print_job(job, ja_task, NULL, 1, NULL,
                                &ja_task_id_string, full_listing, 0, 0, 
                                exechost_list, centry_list, pe_list, indent, group_opt, 0, queue_name_length);
               }
            }
         }
      }
   }
   job_destroy_hold_id_lists(job, range_list); 
   sge_dstring_free(&ja_task_id_string);
   DEXIT;
   return STATUS_OK;
}                 

/*-------------------------------------------------------------------------*/
/* print the finished jobs in case of SGE                                  */
/*-------------------------------------------------------------------------*/
void sge_print_jobs_finished(
lList *job_list,
const lList *pe_list,
lList *user_list,
lList *ehl,
lList *centry_list,
u_long32 full_listing, 
u_long32 group_opt,
int longest_queue_length) {
   int sge_ext;
   int first = 1;
   lListElem *jep, *jatep;
   dstring dyn_task_str = DSTRING_INIT;

   DENTER(TOP_LAYER, "sge_print_jobs_finished");

   sge_ext = (full_listing & QSTAT_DISPLAY_EXTENDED);

   {
      for_each (jep, job_list) {
         for_each (jatep, lGetList(jep, JB_ja_tasks)) {
            if (shut_me_down) {
               SGE_EXIT(1);
            }   
            if (lGetUlong(jatep, JAT_status) == JFINISHED) {
               lSetUlong(jatep, JAT_suitable, lGetUlong(jatep, JAT_suitable)|TAG_FOUND_IT);

               if (!getenv("MORE_INFO"))
                  continue;

               if (!lGetNumberOfElem(user_list) || (lGetNumberOfElem(user_list) && 
                     (lGetUlong(jatep, JAT_suitable)&TAG_SELECT_IT))) {
                  if (first) {
                     first = 0;
                     printf("\n################################################################################%s\n", sge_ext?hashes:"");
                     printf(MSG_QSTAT_PRT_JOBSWAITINGFORACCOUNTING);
                     printf(  "################################################################################%s\n", sge_ext?hashes:"");
                  }
                  sge_dstring_sprintf(&dyn_task_str, u32, 
                                    lGetUlong(jatep, JAT_task_number));
                  sge_print_job(jep, jatep, NULL, 1, NULL, &dyn_task_str, 
                                full_listing, 0, 0, ehl, centry_list, pe_list, "", group_opt, 0, longest_queue_length);   
               }
            }
         }
      }
   }
   sge_dstring_free(&dyn_task_str);
   DEXIT;
}
  
/*-------------------------------------------------------------------------*/
/* print the jobs in error                                                 */
/*-------------------------------------------------------------------------*/
void sge_print_jobs_error(
lList *job_list,
const lList *pe_list,
lList *user_list,
lList *ehl,
lList *centry_list,
u_long32 full_listing,
u_long32 group_opt,
int longest_queue_length) {
   int first = 1;
   lListElem *jep, *jatep;
   int sge_ext;
   dstring dyn_task_str = DSTRING_INIT;

   DENTER(TOP_LAYER, "sge_print_jobs_error");

   sge_ext = (full_listing & QSTAT_DISPLAY_EXTENDED);

   for_each (jep, job_list) {
      for_each (jatep, lGetList(jep, JB_ja_tasks)) {
         if (!(lGetUlong(jatep, JAT_suitable) & TAG_FOUND_IT) && lGetUlong(jatep, JAT_status) == JERROR) {
            lSetUlong(jatep, JAT_suitable, lGetUlong(jatep, JAT_suitable)|TAG_FOUND_IT);

            if (!lGetNumberOfElem(user_list) || (lGetNumberOfElem(user_list) && 
                  (lGetUlong(jatep, JAT_suitable)&TAG_SELECT_IT))) {
               if (first) {
                  first = 0;
                  printf("\n################################################################################%s\n", sge_ext?hashes:"");
                  printf(MSG_QSTAT_PRT_ERRORJOBS);
                  printf("################################################################################%s\n", sge_ext?hashes:"");
               }
               sge_dstring_sprintf(&dyn_task_str, "u32", lGetUlong(jatep, JAT_task_number));
               sge_print_job(jep, jatep, NULL, 1, NULL, &dyn_task_str, full_listing, 0, 0, ehl, centry_list, pe_list, "", group_opt, 0, longest_queue_length);
            }
         }
      }
   }
   sge_dstring_free(&dyn_task_str);
   DEXIT;
}

/*-------------------------------------------------------------------------*/
/* print the zombie jobs                                                   */
/*-------------------------------------------------------------------------*/
void sge_print_jobs_zombie(
lList *zombie_list,
const lList *pe_list,
lList *user_list,
lList *ehl,
lList *centry_list,
u_long32 full_listing,
u_long32 group_opt,
int longest_queue_length
) {
   int sge_ext;
   lListElem *jep;
   dstring dyn_task_str = DSTRING_INIT; 

   DENTER(TOP_LAYER, "sge_print_jobs_zombie");
   
   if (! (full_listing & QSTAT_DISPLAY_ZOMBIES)) {
      DEXIT;
      return;
   }

   sge_ext = (full_listing & QSTAT_DISPLAY_EXTENDED);

   for_each (jep, zombie_list) { 
      lList *z_ids = NULL;
      z_ids = lGetList(jep, JB_ja_z_ids);
      if (z_ids != NULL) {
         lListElem *ja_task = NULL;
         u_long32 first_task_id = range_list_get_first_id(z_ids, NULL);

         sge_printf_header(full_listing & 
                           (QSTAT_DISPLAY_ZOMBIES | QSTAT_DISPLAY_FULL), 
                           sge_ext);
         ja_task = job_get_ja_task_template_pending(jep, first_task_id);
         range_list_print_to_string(z_ids, &dyn_task_str, 0);
         sge_print_job(jep, ja_task, NULL, 1, NULL, &dyn_task_str, 
                       full_listing, 0, 0, ehl, centry_list, pe_list, "", group_opt, 0, longest_queue_length);
         sge_dstring_free(&dyn_task_str);
      }
   }
   DEXIT;
}

#define OPTI_PRINT8(value) \
   if (value > 99999999 ) \
      printf("%8.3g ", value); \
   else  \
      printf("%8.0f ", value)

/* regular output */
static char jhul1[] = "---------------------------------------------------------------------------------------------";
/* -g t */
static char jhul2[] = "-";
/* -ext */
static char jhul3[] = "-------------------------------------------------------------------------------";
/* -t */
static char jhul4[] = "-----------------------------------------------------";
/* -urg */
static char jhul5[] = "----------------------------------------------------------------";
/* -pri */
static char jhul6[] = "-----------------------------------";

static int sge_print_job(
lListElem *job,
lListElem *jatep,
lListElem *qep,
int print_jobid,
char *master,
dstring *dyn_task_str,
u_long32 full_listing,
int slots,
int slot,
lList *exechost_list,
lList *centry_list,
const lList *pe_list,
char *indent,
u_long32 group_opt,
int slots_per_line,  /* number of slots to be printed in slots column 
                       when 0 is passed the number of requested slots printed */
int queue_name_length 
) {
   char state_string[8];
   static int first_time = 1;
   u_long32 jstate;
   int sge_urg, sge_pri, sge_ext, sge_time;
   lList *ql = NULL;
   lListElem *qrep, *gdil_ep=NULL;
   int running;
   const char *queue_name;
   int tsk_ext;
   u_long tickets,otickets,stickets,ftickets;
   int is_zombie_job;
   dstring ds;
   char buffer[128];
   dstring queue_name_buffer = DSTRING_INIT;

   DENTER(TOP_LAYER, "sge_print_job");

   sge_dstring_init(&ds, buffer, sizeof(buffer));

   is_zombie_job = job_is_zombie_job(job);

   if (qep != NULL) {
      queue_name = qinstance_get_name(qep, &queue_name_buffer);
   } else {
      queue_name = NULL; 
   }

   sge_ext = ((full_listing & QSTAT_DISPLAY_EXTENDED) == QSTAT_DISPLAY_EXTENDED);
   tsk_ext = (full_listing & QSTAT_DISPLAY_TASKS);
   sge_urg = (full_listing & QSTAT_DISPLAY_URGENCY);
   sge_pri = (full_listing & QSTAT_DISPLAY_PRIORITY);
   sge_time = (!sge_ext | tsk_ext | sge_urg | sge_pri);


   if (first_time) {
      first_time = 0;
      if (!(full_listing & QSTAT_DISPLAY_FULL)) {
         int line_length = queue_name_length-10+1;
         char * seperator = malloc(line_length);		   
         const char *part1 = "%s%-7.7s %s %s%s%s%s%s %-10.10s %-12.12s %s%-5.5s %s%s%s%s%s%s%s%s%s%-";
		   const char *part3 = ".";
		   const char *part5 = "s %s %s%s%s%s%s%s";
		   char *part6 = malloc(strlen(part1) + strlen(part3) + strlen(part5) + 20);
         {
            int i;
            for(i=0; i<line_length; i++){
               seperator[i] = '-';
            }
         }
         seperator[line_length-1] = '\0';
		   sprintf(part6, "%s%d%s%d%s", part1, queue_name_length, part3, queue_name_length, part5);
      
         printf(part6,
                  indent,
                  "job-ID",
                  "prior ",
               (sge_pri||sge_urg)?" nurg   ":"",
               sge_pri?" npprior":"",
               (sge_pri||sge_ext)?" ntckts ":"",
               sge_urg?" urg      rrcontr  wtcontr  dlcontr ":"",
               sge_pri?"  ppri":"",
                  "name",
                  "user",
               sge_ext?"project          department ":"",
                  "state",
               sge_time?"submit/start at     ":"",
               sge_urg ? " deadline           " : "",
               sge_ext ? USAGE_ATTR_CPU "        " USAGE_ATTR_MEM "     " USAGE_ATTR_IO "      " : "",
               sge_ext?"tckts ":"",
               sge_ext?"ovrts ":"",
               sge_ext?"otckt ":"",
               sge_ext?"ftckt ":"",
               sge_ext?"stckt ":"",
               sge_ext?"share ":"",
                  "queue",
               (group_opt & GROUP_NO_PETASK_GROUPS)?"master":"slots",
                  "ja-task-ID ", 
               tsk_ext?"task-ID ":"",
               tsk_ext?"state ":"",
               tsk_ext?USAGE_ATTR_CPU "        " USAGE_ATTR_MEM "     " USAGE_ATTR_IO "      " : "",
               tsk_ext?"stat ":"",
               tsk_ext?"failed ":"" );

         printf("\n%s%s%s%s%s%s%s%s\n", indent, 
               jhul1, 
               seperator,
               (group_opt & GROUP_NO_PETASK_GROUPS)?jhul2:"",
               sge_ext ? jhul3 : "", 
               tsk_ext ? jhul4 : "",
               sge_urg ? jhul5 : "",
               sge_pri ? jhul6 : "");
               
         FREE(part6);
         FREE(seperator);               
      }
   }
      
   printf("%s", indent);

   /* job number / ja task id */
   if (print_jobid)
      printf("%7d ", (int)lGetUlong(job, JB_job_number)); 
   else
      printf("        "); 

   /* per job priority information */
   {

      if (print_jobid)
         printf("%7.5f ", lGetDouble(jatep, JAT_prio)); /* nprio 0.0 - 1.0 */
      else 
         printf("        ");

      if (sge_pri || sge_urg) {
         if (print_jobid)
            printf("%7.5f ", lGetDouble(job, JB_nurg)); /* nurg 0.0 - 1.0 */
         else
            printf("        ");
      }

      if (sge_pri) {
         if (print_jobid)
            printf("%7.5f ", lGetDouble(job, JB_nppri)); /* nppri 0.0 - 1.0 */
         else
            printf("        ");
      }

      if (sge_pri || sge_ext) {
         if (print_jobid)
            printf("%7.5f ", lGetDouble(jatep, JAT_ntix)); /* ntix 0.0 - 1.0 */
         else
            printf("        ");
      }

      if (sge_urg) {
         if (print_jobid) {
            OPTI_PRINT8(lGetDouble(job, JB_urg));
            OPTI_PRINT8(lGetDouble(job, JB_rrcontr));
            OPTI_PRINT8(lGetDouble(job, JB_wtcontr));
            OPTI_PRINT8(lGetDouble(job, JB_dlcontr));
         } else {
            printf("         "
                   "         "
                   "         "
                   "         ");
         }
      } 

      if (sge_pri) {
         if (print_jobid) {
            printf("%5d ", ((int)lGetUlong(job, JB_priority))-BASE_PRIORITY); 
         } else {
            printf("         "
                   "         ");
         }
      }

   } 

   if (print_jobid) {
      /* job name */
      printf("%-10.10s ", lGetString(job, JB_job_name)); 

      /* job owner */
      printf("%-12.12s ", lGetString(job, JB_owner)); 
   } else {
      printf("           "); 
      printf("             "); 
   }

   if (sge_ext) {
      const char *s;

      if (print_jobid) {
         /* job project */
         printf("%-16.16s ", (s=lGetString(job, JB_project))?s:"NA"); 
         /* job department */
         printf("%-10.10s ", (s=lGetString(job, JB_department))?s:"NA"); 
      } else {
         printf("                 "); 
         printf("           "); 
      }
   }

   /* move status info into state info */
   jstate = lGetUlong(jatep, JAT_state);
   if (lGetUlong(jatep, JAT_status)==JTRANSFERING) {
      jstate |= JTRANSFERING;
      jstate &= ~JRUNNING;
   }

   if (lGetList(job, JB_jid_predecessor_list) || lGetUlong(jatep, JAT_hold)) {
      jstate |= JHELD;
   }

   if (lGetUlong(jatep, JAT_job_restarted)) {
      jstate &= ~JWAITING;
      jstate |= JMIGRATING;
   }

   if (print_jobid) {
      /* write states into string */ 
      job_get_state_string(state_string, jstate);
      printf("%-5.5s ", state_string); 
   } else {
      printf("      "); 
   }

   if (sge_time) {
      if (print_jobid) {
         /* start/submit time */
         if (!lGetUlong(jatep, JAT_start_time) ) {
            printf("%s ", sge_ctime(lGetUlong(job, JB_submission_time), &ds));
         }   
         else {
#if 0
            /* AH: intermediate change to monitor JAT_stop_initiate_time 
             * must be removed before 6.0 if really needed a better possiblity 
             * for monitoring must be found (TODO)
             */
            if (getenv("JAT_stop_initiate_time") && (lGetUlong(jatep, JAT_state) & JDELETED))
               printf("%s!", sge_ctime(lGetUlong(jatep, JAT_stop_initiate_time), &ds));
            else
#endif
               printf("%s ", sge_ctime(lGetUlong(jatep, JAT_start_time), &ds));
         }
      } else {
         printf("                    "); 
      }
   }

   /* is job logically running */
   running = lGetUlong(jatep, JAT_status)==JRUNNING || 
      lGetUlong(jatep, JAT_status)==JTRANSFERING;

   /* deadline time */
   if (sge_urg) {
      if (print_jobid) { 
         if (!lGetUlong(job, JB_deadline) )
            printf("                    ");
         else
            printf("%s ", sge_ctime(lGetUlong(job, JB_deadline), &ds));
      } else {
         printf("                    "); 
      }
   }

   if (sge_ext) {
      lListElem *up, *pe, *task;
      lList *job_usage_list;
      const char *pe_name;
      
      if (!master || !strcmp(master, "MASTER"))
         job_usage_list = lCopyList(NULL, lGetList(jatep, JAT_scaled_usage_list));
      else
         job_usage_list = lCreateList("", UA_Type);

      /* sum pe-task usage based on queue slots */
      if (job_usage_list) {
         int subtask_ndx=1;
         for_each(task, lGetList(jatep, JAT_task_list)) {
            lListElem *dst, *src, *ep;
            const char *qname;

            if (!slots ||
                (queue_name && 
                 ((ep=lFirst(lGetList(task, PET_granted_destin_identifier_list)))) &&
                 ((qname=lGetString(ep, JG_qname))) &&
                 !strcmp(qname, queue_name) && ((subtask_ndx++%slots)==slot))) {
               for_each(src, lGetList(task, PET_scaled_usage)) {
                  if ((dst=lGetElemStr(job_usage_list, UA_name, lGetString(src, UA_name))))
                     lSetDouble(dst, UA_value, lGetDouble(dst, UA_value) + lGetDouble(src, UA_value));
                  else
                     lAppendElem(job_usage_list, lCopyElem(src));
               }
            }
         }
      }


      /* scaled cpu usage */
      if (!(up = lGetElemStr(job_usage_list, UA_name, USAGE_ATTR_CPU))) 
         printf("%-10.10s ", running?"NA":""); 
      else {
         int secs, minutes, hours, days;

         secs = lGetDouble(up, UA_value);

         days    = secs/(60*60*24);
         secs   -= days*(60*60*24);

         hours   = secs/(60*60);
         secs   -= hours*(60*60);

         minutes = secs/60;
         secs   -= minutes*60;
      
         printf("%d:%2.2d:%2.2d:%2.2d ", days, hours, minutes, secs); 
      } 
      /* scaled mem usage */
      if (!(up = lGetElemStr(job_usage_list, UA_name, USAGE_ATTR_MEM))) 
         printf("%-7.7s ", running?"NA":""); 
      else
         printf("%-5.5f ", lGetDouble(up, UA_value)); 
  
      /* scaled io usage */
      if (!(up = lGetElemStr(job_usage_list, UA_name, USAGE_ATTR_IO))) 
         printf("%-7.7s ", running?"NA":""); 
      else
         printf("%-5.5f ", lGetDouble(up, UA_value)); 

      lFreeList(job_usage_list);

      /* get tickets for job/slot */
      /* braces needed to suppress compiler warnings */
      if ((pe_name=lGetString(jatep, JAT_granted_pe)) &&
           (pe=pe_list_locate(pe_list, pe_name)) &&
           lGetBool(pe, PE_control_slaves)
         && slots && (gdil_ep=lGetSubStr(jatep, JG_qname, queue_name,
               JAT_granted_destin_identifier_list))) {
         if (slot == 0) {
            tickets = lGetDouble(gdil_ep, JG_ticket);
            otickets = lGetDouble(gdil_ep, JG_oticket);
            ftickets = lGetDouble(gdil_ep, JG_fticket);
            stickets = lGetDouble(gdil_ep, JG_sticket);
         }
         else {
            if (slots) {
               tickets = lGetDouble(gdil_ep, JG_ticket) / slots;
               otickets = lGetDouble(gdil_ep, JG_oticket) / slots;
               ftickets = lGetDouble(gdil_ep, JG_fticket) / slots;
               stickets = lGetDouble(gdil_ep, JG_sticket) / slots;
            } 
            else {
               tickets = otickets = ftickets = stickets = 0;
            }
         }
      }
      else {
         tickets = lGetDouble(jatep, JAT_tix);
         otickets = lGetDouble(jatep, JAT_oticket);
         ftickets = lGetDouble(jatep, JAT_fticket);
         stickets = lGetDouble(jatep, JAT_sticket);
      }


      /* report jobs dynamic scheduling attributes */
      /* only scheduled have these attribute */
      /* Pending jobs can also have tickets */
      if (is_zombie_job) {
         printf("   NA ");
         printf("   NA ");
         printf("   NA ");
         printf("   NA ");
         printf("   NA ");
         printf("   NA ");
      } else {
         if (sge_ext || lGetList(jatep, JAT_granted_destin_identifier_list)) {
            printf("%5d ", (int)tickets),
            printf("%5d ", (int)lGetUlong(job, JB_override_tickets)); 
            printf("%5d ", (int)otickets);
            printf("%5d ", (int)ftickets);
            printf("%5d ", (int)stickets);
            printf("%-5.2f ", lGetDouble(jatep, JAT_share)); 
         } else {
            printf("      "); 
            printf("      "); 
            printf("      "); 
            printf("      "); 
            printf("      "); 
            printf("      "); 
            printf("      "); 
         }
      }
   }

   /* if not full listing we need the queue's name in each line */
   if (!(full_listing & QSTAT_DISPLAY_FULL)) {
      char temp[20];
	   sprintf(temp,"%%-%d.%ds ", queue_name_length, queue_name_length);
      printf(temp, queue_name?queue_name:"");
   }

   if ((group_opt & GROUP_NO_PETASK_GROUPS)) {
      /* MASTER/SLAVE information needed only to show parallel job distribution */
      if (master)
         printf("%-7.6s", master);
      else
         printf("       ");
   } else {
      /* job slots requested/granted */
      if (!slots_per_line)
         slots_per_line = sge_job_slot_request(job, pe_list);
      printf("%5d ", slots_per_line);
   }

   if (sge_dstring_get_string(dyn_task_str) && job_is_array(job))
      printf("%s", sge_dstring_get_string(dyn_task_str)); 
   else
      printf("       ");

   if (tsk_ext) {
      lList *task_list = lGetList(jatep, JAT_task_list);
      lListElem *task, *ep;
      const char *qname;
      int indent=0;
      int subtask_ndx=1;
      int num_spaces = sizeof(jhul1)-1 + (sge_ext?sizeof(jhul2)-1:0) - 
            ((full_listing & QSTAT_DISPLAY_FULL)?11:0);

      /* print master sub-task belonging to this queue */
      if (!slot && task_list && queue_name &&
          ((ep=lFirst(lGetList(jatep, JAT_granted_destin_identifier_list)))) &&
          ((qname=lGetString(ep, JG_qname))) &&
          !strcmp(qname, queue_name)) {
         if (indent++)
            printf("%*s", num_spaces, " ");
         sge_print_subtask(job, jatep, NULL, 0, 0);
         /* subtask_ndx++; */
      }
         
      /* print sub-tasks belonging to this queue */
      for_each(task, task_list) {
         if (!slots || (queue_name && 
              ((ep=lFirst(lGetList(task, PET_granted_destin_identifier_list)))) &&
              ((qname=lGetString(ep, JG_qname))) &&
              !strcmp(qname, queue_name) && ((subtask_ndx++%slots)==slot))) {
            if (indent++)
               printf("%*s", num_spaces, " ");
            sge_print_subtask(job, jatep, task, 0, 0);
         }
      }

      if (!indent)
         putchar('\n');

   } 
   else {
      /* print a new line */
      putchar('\n');
   }

   /* print additional job info if requested */
   if (print_jobid && (full_listing & QSTAT_DISPLAY_RESOURCES)) {
         printf(QSTAT_INDENT "Full jobname:     %s\n", lGetString(job, JB_job_name));

         if(queue_name)
            printf(QSTAT_INDENT "Master queue:     %s\n", queue_name);

         if (lGetString(job, JB_pe)) {
            dstring range_string = DSTRING_INIT;

            range_list_print_to_string(lGetList(job, JB_pe_range), 
                                       &range_string, 1);
            printf(QSTAT_INDENT "Requested PE:     %s %s\n", 
                   lGetString(job, JB_pe), sge_dstring_get_string(&range_string)); 
            sge_dstring_free(&range_string);
         }
         if (lGetString(jatep, JAT_granted_pe)) {
            lListElem *gdil_ep;
            u_long32 pe_slots = 0;
            for_each (gdil_ep, lGetList(jatep, JAT_granted_destin_identifier_list))
               pe_slots += lGetUlong(gdil_ep, JG_slots);
            printf(QSTAT_INDENT "Granted PE:       %s "u32"\n", 
               lGetString(jatep, JAT_granted_pe), pe_slots); 
         }
         if (lGetString(job, JB_checkpoint_name)) 
            printf(QSTAT_INDENT "Checkpoint Env.:  %s\n", 
               lGetString(job, JB_checkpoint_name)); 

         sge_show_ce_type_list_line_by_line(QSTAT_INDENT "Hard Resources:   ",
               QSTAT_INDENT2, lGetList(job, JB_hard_resource_list), true, centry_list,
               sge_job_slot_request(job, pe_list)); 

         /* display default requests if necessary */
         {
            lList *attributes = NULL;
            lListElem *ce;
            const char *name;
            lListElem *hep;

            queue_complexes2scheduler(&attributes, qep, exechost_list, centry_list);
            for_each (ce, attributes) {
               double dval;

               name = lGetString(ce, CE_name);
               if (!lGetBool(ce, CE_consumable) || !strcmp(name, "slots") || 
                   job_get_request(job, name))
                  continue;

               parse_ulong_val(&dval, NULL, lGetUlong(ce, CE_valtype), lGetString(ce, CE_default), NULL, 0); 
               if (dval == 0.0)
                  continue;

               /* For pending jobs (no queue/no exec host) we may print default request only
                  if the consumable is specified in the global host. For running we print it
                  if the resource is managed at this node/queue */
               if ((qep && lGetSubStr(qep, CE_name, name, QU_consumable_config_list)) ||
                   (qep && (hep=host_list_locate(exechost_list, lGetHost(qep, QU_qhostname))) &&
                    lGetSubStr(hep, CE_name, name, EH_consumable_config_list)) ||
                     ((hep=host_list_locate(exechost_list, SGE_GLOBAL_NAME)) &&
                         lGetSubStr(hep, CE_name, name, EH_consumable_config_list)))

               printf("%s%s=%s (default)\n", QSTAT_INDENT, name, lGetString(ce, CE_default));      
            }
            lFreeList(attributes);
         }

         sge_show_ce_type_list_line_by_line(QSTAT_INDENT "Soft Resources:   ",
               QSTAT_INDENT2, lGetList(job, JB_soft_resource_list), false, NULL, 0); 

         ql = lGetList(job, JB_hard_queue_list);
         if (ql) {
            printf(QSTAT_INDENT "Hard requested queues: ");
            for_each(qrep, ql) {
               printf("%s", lGetString(qrep, QR_name));
               printf("%s", lNext(qrep)?", ":"\n");
            }
         }

         ql = lGetList(job, JB_soft_queue_list);
         if (ql) {
            printf(QSTAT_INDENT "Soft requested queues: ");
            for_each(qrep, ql) {
               printf("%s", lGetString(qrep, QR_name));
               printf("%s", lNext(qrep)?", ":"\n");
            }
         }
         ql = lGetList(job, JB_master_hard_queue_list);
         if (ql){
            printf(QSTAT_INDENT "Master task hard requested queues: ");
            for_each(qrep, ql) {
               printf("%s", lGetString(qrep, QR_name));
               printf("%s", lNext(qrep)?", ":"\n");
            }
         }
         ql = lGetList(job, JB_jid_request_list );
         if (ql) {
            printf(QSTAT_INDENT "Predecessor Jobs (request): ");
            for_each(qrep, ql) {
               printf("%s", lGetString(qrep, JRE_job_name));
               printf("%s", lNext(qrep)?", ":"\n");
            }
         }
         ql = lGetList(job, JB_jid_predecessor_list);
         if (ql) {
            printf(QSTAT_INDENT "Predecessor Jobs: ");
            for_each(qrep, ql) {
               printf(u32, lGetUlong(qrep, JRE_job_number));
               printf("%s", lNext(qrep)?", ":"\n");
            }
         }
   }

#undef QSTAT_INDENT
#undef QSTAT_INDENT2

   sge_dstring_free(&queue_name_buffer);

   DEXIT;
   return 1;
}


/* print display bitmask */ 
void qstat_display_bitmask_to_str(u_long32 bitmask, dstring *string)
{
   bool found = false;
   int i;
   static const struct qstat_display_bitmask_s {
      u_long32 mask;
      char *name;
   } bitmasks[] = {
      { QSTAT_DISPLAY_USERHOLD,      "USERHOLD" },
      { QSTAT_DISPLAY_SYSTEMHOLD,    "SYSTEMHOLD" },
      { QSTAT_DISPLAY_OPERATORHOLD,  "OPERATORHOLD" },
      { QSTAT_DISPLAY_JOBHOLD,       "JOBHOLD" },
      { QSTAT_DISPLAY_STARTTIMEHOLD, "STARTTIMEHOLD" },
      { QSTAT_DISPLAY_HOLD,          "HOLD" },
      { QSTAT_DISPLAY_PENDING,       "PENDING" },
      { QSTAT_DISPLAY_RUNNING,       "RUNNING" },
      { QSTAT_DISPLAY_SUSPENDED,     "SUSPENDED" },
      { QSTAT_DISPLAY_ZOMBIES,       "ZOMBIES" },
      { QSTAT_DISPLAY_FINISHED,      "FINISHED" },
      { 0,                           NULL }
   };

   sge_dstring_clear(string);

   for (i=0; bitmasks[i].mask !=0 ; i++) {
      if ((bitmask & bitmasks[i].mask)) {
         if (found)
            sge_dstring_append(string, " ");
         sge_dstring_append(string, bitmasks[i].name);
         found = true;
      }
   }

   if (!found)
      sge_dstring_copy_string(string, "<NONE>");

   return;
}
