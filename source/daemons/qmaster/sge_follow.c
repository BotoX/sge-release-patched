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
#include <string.h>
#include <pthread.h>

#ifdef SOLARISAMD64
#  include <sys/stream.h>
#endif  

#include "sge_conf.h"
#include "sge.h"
#include "sge_pe.h"
#include "sge_ja_task.h"
#include "sge_any_request.h"
#include "sge_qinstance.h"
#include "sge_qinstance_state.h"
#include "sge_time.h"
#include "sge_log.h"
#include "sge_orderL.h"
#include "sge_order.h"
#include "sge_usageL.h"
#include "sge_schedd_conf.h"
#include "sgermon.h"
#include "commlib.h"
#include "sge_host.h"
#include "sge_signal.h"
#include "sge_job_qmaster.h"
#include "sge_follow.h"
#include "sge_sched.h"
#include "scheduler.h"
#include "sgeee.h"
#include "sge_support.h"
#include "sge_userprj_qmaster.h"
#include "sge_pe_qmaster.h"
#include "sge_qmod_qmaster.h"
#include "sge_subordinate_qmaster.h"
#include "sge_sharetree_qmaster.h"
#include "sge_give_jobs.h"
#include "sge_event_master.h"
#include "sge_queue_event_master.h"
#include "sge_feature.h"
#include "sge_prog.h"
#include "config.h"
#include "mail.h"
#include "sge_str.h"
#include "sge_messageL.h"
#include "sge_string.h"
#include "sge_security.h"
#include "sge_range.h"
#include "sge_job.h"
#include "sge_hostname.h"
#include "sge_answer.h"
#include "sge_userprj.h"
#include "sge_userset.h"
#include "sge_sharetree.h"
#include "sge_todo.h"
#include "sge_cqueue.h"

#include "sge_persistence_qmaster.h"
#include "spool/sge_spooling.h"

#include "msg_common.h"
#include "msg_evmlib.h"
#include "msg_qmaster.h"
#include "sge_mtutil.h"

typedef struct {
   pthread_mutex_t last_update_mutex; /* gards the last_update access */
   u_long32 last_update;               /* used to store the last time, when the usage was stored */
   order_pos_t *cull_order_pos;        /* stores cull positions in the job, ja-task, and order structure */
} sge_follow_t;

static sge_follow_t Follow_Control = {PTHREAD_MUTEX_INITIALIZER, 0, NULL};

/**********************************************************************
 Gets an order and executes it.

 Return 0 if everything is fine or 

 -1 if the scheduler has sent an inconsistent order list but we think
    the next event delivery will correct this

 -2 if the scheduler has sent an inconsistent order list and we don't think
    the next event delivery will correct this

 -3 if delivery to an execd failed
 
lList **topp,   ticket orders ptr ptr 

 **********************************************************************/ 
int 
sge_follow_order(lListElem *ep, lList **alpp, char *ruser, char *rhost,
                 lList **topp) 
{
   int allowed;
   u_long32 job_number, task_number;
   const char *or_pe, *q_name=NULL;
   u_long32 or_type;
   lList *acl, *xacl;
   lListElem *jep, *qep, *master_qep, *oep, *hep, *master_host = NULL, *jatp = NULL;
   u_long32 state;
   u_long32 pe_slots = 0, q_slots, q_version, task_id_range = 0;
   lListElem *pe = NULL;

   DENTER(TOP_LAYER, "sge_follow_order");

   or_type=lGetUlong(ep, OR_type);
   or_pe=lGetString(ep, OR_pe);

   DPRINTF(("-----------------------------------------------------------------------\n"));

   switch(or_type) {

   /* ----------------------------------------------------------------------- 
    * START JOB
    * ----------------------------------------------------------------------- */
   case ORT_start_job: {
      char opt_sge[256] = ""; 
      lList *gdil = NULL;

      DPRINTF(("ORDER ORT_start_job\n"));

      job_number=lGetUlong(ep, OR_job_number);
      if(!job_number) {
         ERROR((SGE_EVENT, MSG_JOB_NOJOBID));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DEXIT;
         return -2;
      }
      task_number=lGetUlong(ep, OR_ja_task_number);
      if (!task_number) { 
         ERROR((SGE_EVENT, MSG_JOB_NOORDERTASK_US, u32c(job_number), "ORT_start_job"));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DEXIT;
         return -2;
      }
      jep = job_list_locate(Master_Job_List, job_number);
      if(!jep) {
         WARNING((SGE_EVENT, MSG_JOB_FINDJOB_U, u32c(job_number)));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);

         DEXIT;
         return -1;
      }

      DPRINTF(("ORDER to start Job %ld Task %ld\n", (long) job_number, 
               (long) task_number));      

      if (lGetUlong(jep, JB_version) != lGetUlong(ep, OR_job_version)) {
         ERROR((SGE_EVENT, MSG_ORD_OLDVERSION_UUU, 
               u32c(lGetUlong(ep, OR_job_version)), u32c(job_number), 
               u32c(task_number)));
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DEXIT;
         return -1;
      }

      /* search and enroll task */
      jatp = job_search_task(jep, NULL, task_number);
      if(jatp == NULL) {
         if (range_list_is_id_within(lGetList(jep, JB_ja_n_h_ids), task_number)) {
            lList *answer_list = NULL;
           
            /* 
             * CR - TODO:
             *
             * job_create_task() should check if it should create the task and
             * not the user by first checking the function range_list_is_id_within()
             *
             */

            jatp = job_create_task(jep, NULL, task_number);
            /* spooling of the JATASK will be done in sge_commit_job */
            sge_add_event(0, sgeE_JATASK_ADD, job_number, task_number, 
                          NULL, NULL, lGetString(jep, JB_session), jatp);
            sge_event_spool(&answer_list, 0, sgeE_JOB_MOD,
                            job_number, 0, NULL, NULL, 
                            lGetString(jep, JB_session),
                            jep, NULL, NULL, true, true);
         } else {
            INFO((SGE_EVENT, MSG_JOB_IGNORE_DELETED_TASK_UU,
                  u32c(job_number), u32c(task_number)));
            DEXIT;
            return 0;
         }
      }
      if (!jatp) {
         WARNING((SGE_EVENT, MSG_JOB_FINDJOBTASK_UU, u32c(task_number), 
                  u32c(job_number)));
         /* try to repair schedd data */
         sge_add_event( 0, sgeE_JATASK_DEL, job_number, task_number, 
                       NULL, NULL, lGetString(jep, JB_session), NULL);
         DEXIT;
         return -1;
      }

      if (lGetUlong(jatp, JAT_status) != JIDLE) {
         ERROR((SGE_EVENT, MSG_ORD_TWICE_UU, u32c(job_number), 
                u32c(task_number)));
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DEXIT;
         return -1;
      }

      cqueue_list_set_tag(*(object_type_get_master_list(SGE_TYPE_CQUEUE)), 
                          0, true);

      if (lGetString(jep, JB_pe)) {
         pe = pe_list_locate(Master_Pe_List, or_pe);
         if (!pe) {
            ERROR((SGE_EVENT, MSG_OBJ_UNABLE2FINDPE_S, or_pe));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DEXIT;
            return -2;
         }
         task_id_range = 0;
         lSetString(jatp, JAT_granted_pe, or_pe);  /* free me on error! */
      }

      master_qep = NULL;
       
      /* fill number of tickets into job */
      lSetDouble(jatp, JAT_tix,                   lGetDouble(ep, OR_ticket));
      lSetDouble(jatp, JAT_ntix,                  lGetDouble(ep, OR_ntix));
      lSetDouble(jatp, JAT_prio,                  lGetDouble(ep, OR_prio));

      sprintf(opt_sge, MSG_ORD_INITIALTICKETS_U, u32c((u_long32)lGetDouble(ep, OR_ticket)));

      if ((oep = lFirst(lGetList(ep, OR_queuelist)))) {
         lSetDouble(jatp, JAT_oticket, lGetDouble(oep, OQ_oticket));
         lSetDouble(jatp, JAT_fticket, lGetDouble(oep, OQ_fticket));
         lSetDouble(jatp, JAT_sticket, lGetDouble(oep, OQ_sticket));
      }

      for_each(oep, lGetList(ep, OR_queuelist)) {
         lListElem *gdil_ep;

         q_name    = lGetString(oep, OQ_dest_queue);
         q_version = lGetUlong(oep, OQ_dest_version);
         q_slots   = lGetUlong(oep, OQ_slots);

         /* ---------------------- 
          *  find and check queue 
          */
         if(!q_name) {
            ERROR((SGE_EVENT, MSG_OBJ_NOQNAME));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            lFreeList(gdil);
            lSetString(jatp, JAT_granted_pe, NULL);
            DEXIT;
            return -2;
         }

         DPRINTF(("ORDER: start %d slots of job \"%d\" on"
                  " queue \"%s\" v%d%s\n", 
               q_slots, job_number, q_name, (int)q_version, (opt_sge ) ));

         qep = cqueue_list_locate_qinstance(*(object_type_get_master_list(SGE_TYPE_CQUEUE)), q_name);
         if (!qep) {
            ERROR((SGE_EVENT, MSG_QUEUE_UNABLE2FINDQ_S, q_name));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            lFreeList(gdil);
            lSetString(jatp, JAT_granted_pe, NULL);
            DEXIT;
            return -2;
         }

         /* the first queue is the master queue */
         if (master_qep == NULL) {
            master_qep = qep;
         }   

         /* check queue version */
         if (q_version != lGetUlong(qep, QU_version)) {
            ERROR((SGE_EVENT, MSG_ORD_QVERSION_UUS,
                  u32c(q_version), u32c(lGetUlong(qep, QU_version)), q_name));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);

            /* try to repair schedd data */
            qinstance_add_event(qep, sgeE_QINSTANCE_MOD);

            lFreeList(gdil);
            lSetString(jatp, JAT_granted_pe, NULL);
            DEXIT;
            return -1;
         }
         
         DPRINTF(("Queue version: %d\n", q_version));

         /* ensure that the jobs owner has access to this queue */
         /* job structure ?                                     */
         acl = lCopyList("acl", lGetList(qep, QU_acl));
         xacl = lCopyList("xacl", lGetList(qep, QU_xacl));
         allowed = sge_has_access_(lGetString(jep, JB_owner), lGetString(jep, JB_group), 
                                    acl, xacl, Master_Userset_List);
         lFreeList(acl); 
         lFreeList(xacl); 
         if (!allowed) {
            ERROR((SGE_EVENT, MSG_JOB_JOBACCESSQ_US, u32c(job_number), q_name));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            lFreeList(gdil);
            lSetString(jatp, JAT_granted_pe, NULL);
            DEXIT;
            return -1;
         }

         /* ensure that this queue has enough free slots */
         if (lGetUlong(qep, QU_job_slots) - qinstance_slots_used(qep) < q_slots) {
            ERROR((SGE_EVENT, MSG_JOB_FREESLOTS_USUU, u32c(q_slots), q_name, 
                  u32c(job_number), u32c(task_number)));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            lFreeList(gdil);
            lSetString(jatp, JAT_granted_pe, NULL);
            DEXIT;
            return -1;
         }  
            
         if (qinstance_state_is_error(qep)) {
            ERROR((SGE_EVENT, MSG_JOB_QMARKEDERROR_S, q_name));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            lFreeList(gdil);
            lSetString(jatp, JAT_granted_pe, NULL);
            DEXIT;
            return -1;
         }  
         if (qinstance_state_is_cal_suspended(qep)) {
            ERROR((SGE_EVENT, MSG_JOB_QSUSPCAL_S, q_name));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            lFreeList(gdil);
            lSetString(jatp, JAT_granted_pe, NULL);
            DEXIT;
            return -1;
         }  
         if (qinstance_state_is_cal_disabled(qep)) {
            ERROR((SGE_EVENT, MSG_JOB_QDISABLECAL_S, q_name));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            lFreeList(gdil);
            lSetString(jatp, JAT_granted_pe, NULL);
            DEXIT;
            return -1;
         }  

         /* set tagged field in queue; this is needed for building */
         /* up job_list in the queue after successful job sending */
         lSetUlong(qep, QU_tag, q_slots); 

         /* ---------------------- 
          *  find and check host 
          */
         if (!(hep=host_list_locate(Master_Exechost_List, 
                     lGetHost(qep, QU_qhostname)))) {
            ERROR((SGE_EVENT, MSG_JOB_UNABLE2FINDHOST_S, lGetHost(qep, QU_qhostname)));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            lFreeList(gdil);
            lSetString(jatp, JAT_granted_pe, NULL);
            DEXIT;
            return -2;
         }
         if (hep) {
            lListElem *ruep;
            lList *rulp;

            rulp = lGetList(hep, EH_reschedule_unknown_list);
            if (rulp) {
               for_each(ruep, rulp) {
                  if (lGetUlong(jep, JB_job_number) == lGetUlong(ruep, RU_job_number)
                      && lGetUlong(jatp, JAT_task_number) == lGetUlong(ruep, RU_task_number)) {
                     ERROR((SGE_EVENT, MSG_JOB_UNABLE2STARTJOB_US, u32c(lGetUlong(ruep, RU_job_number)),
                        lGetHost(qep, QU_qhostname)));
                     answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
                     lFreeList(gdil);
                     lSetString(jatp, JAT_granted_pe, NULL);
                     DEXIT;
                     return -1;
                  }
               }
            }  
         }

         /* the master host is the host of the master queue */
         if (master_qep && !master_host) 
            master_host = hep;

         /* ------------------------------------------------ 
          *  build up granted_destin_identifier_list (gdil)
          */
         gdil_ep = lAddElemStr(&gdil, JG_qname, q_name, JG_Type); /* free me on error! */
         lSetHost(gdil_ep, JG_qhostname, lGetHost(qep, QU_qhostname));
         lSetUlong(gdil_ep, JG_slots, q_slots);

         /* ------------------------------------------------
          *  tag each gdil entry of slave exec host
          *  in case of sge controlled slaves 
          *  this triggers our retry for delivery of slave jobs
          *  and gets untagged when ack has arrived 
          */
         if (pe && lGetBool(pe, PE_control_slaves)) {
      
            lSetDouble(gdil_ep, JG_ticket, lGetDouble(oep, OQ_ticket));
            lSetDouble(gdil_ep, JG_oticket, lGetDouble(oep, OQ_oticket));
            lSetDouble(gdil_ep, JG_fticket, lGetDouble(oep, OQ_fticket));
            lSetDouble(gdil_ep, JG_sticket, lGetDouble(oep, OQ_sticket));

            if (sge_hostcmp(lGetHost(master_host, EH_name), lGetHost(hep, EH_name))) {
               lListElem *first_at_host;

               /* ensure each host gets tagged only one time 
                  we tag the first entry for a host in the existing gdil */
               first_at_host = lGetElemHost(gdil, JG_qhostname, lGetHost(hep, EH_name));
               if (!first_at_host) {
                  ERROR((SGE_EVENT, MSG_JOB_HOSTNAMERESOLVE_US, 
                           u32c(lGetUlong(jep, JB_job_number)), 
                           lGetHost(hep, EH_name)  ));
               } else {
                  lSetUlong(first_at_host, JG_tag_slave_job, 1);   
               }
            } else  {
               DPRINTF(("master host %s\n", lGetHost(master_host, EH_name)));
            }   
         }
         /* in case of a pe job update free_slots on the pe */
         if (pe) { 
            pe_slots += q_slots;
         }   
      }
         
      /* fill in master_queue */
      lSetString(jatp, JAT_master_queue, lGetString(master_qep, QU_full_name));
      lSetList(jatp, JAT_granted_destin_identifier_list, gdil);

      if (sge_give_job(jep, jatp, master_qep, pe, master_host)) {

         /* setting of queues in state unheard is done by sge_give_job() */
         sge_commit_job(jep, jatp, NULL, COMMIT_ST_DELIVERY_FAILED, COMMIT_DEFAULT);
         /* This was sge_commit_job(jep, COMMIT_ST_RESCHEDULED). It raised problems if a job
            could not be delivered. The jobslotsfree had been increased even if
            they where not decreased bevore. */

         ERROR((SGE_EVENT, MSG_JOB_JOBDELIVER_UU, 
                u32c(lGetUlong(jep, JB_job_number)), u32c(lGetUlong(jatp, JAT_task_number))));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DEXIT;
         return -3;
      }

      /* job is now sent and goes into transfering state */
      sge_commit_job(jep, jatp, NULL, COMMIT_ST_SENT, COMMIT_DEFAULT);   /* mode==0 -> really accept when execd acks */

      /* set timeout for job resend */
      trigger_job_resend(sge_get_gmt(), master_host, job_number, task_number);

      if (pe) {
         pe_debit_slots(pe, pe_slots, job_number);
         /* this info is not spooled */
         sge_add_event( 0, sgeE_PE_MOD, 0, 0, 
                       lGetString(jatp, JAT_granted_pe), NULL, NULL, pe);
         lListElem_clear_changed_info(pe);
      }

      DPRINTF(("successfully handed off job \"" u32 "\" to queue \"%s\"\n",
               lGetUlong(jep, JB_job_number), lGetString(jatp, JAT_master_queue)));

      /* now after successfully (we hope) sent the job to execd 
         suspend all subordinated queues that need suspension */
      {
         lList *master_list = *(object_type_get_master_list(SGE_TYPE_CQUEUE));
         lList *gdil = lGetList(jatp, JAT_granted_destin_identifier_list);

         cqueue_list_x_on_subordinate_gdil(master_list, true, gdil);
      }
   }    
      break;

 /* ----------------------------------------------------------------------- 
    * SET PRIORITY VALUES TO NULL
    *
    * modifications performed on the job are not spooled 
    * ----------------------------------------------------------------------- */
   case ORT_clear_pri_info:

      DPRINTF(("ORDER ORT_ptickets\n"));
      {
         ja_task_pos_t *ja_pos = NULL;
         job_pos_t   *job_pos = NULL;
         lListElem *next_ja_task = NULL;

         job_number = lGetUlong(ep, OR_job_number);
         if(job_number == 0) {
            ERROR((SGE_EVENT, MSG_JOB_NOJOBID));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DEXIT;
            return -2;
         }
         
         task_number = lGetUlong(ep, OR_ja_task_number);

         DPRINTF(("ORDER : job("u32")->pri/tickets reset"));

         jep = job_list_locate(Master_Job_List, job_number);
         if(jep == NULL) {
            WARNING((SGE_EVENT, MSG_JOB_UNABLE2FINDJOBORD_U, u32c(job_number)));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DEXIT;
            return 0; /* it's ok - job has exited - forget about him */
         }
       
         next_ja_task = lFirst(lGetList(jep, JB_ja_tasks));
         
         /* we have to iterate over the ja-tasks and the template */
         jatp = job_get_ja_task_template_pending(jep, 0);
         if (jatp == NULL) {
            ERROR((SGE_EVENT, MSG_JOB_FINDJOBTASK_UU,  
                  u32c(0), u32c(job_number)));
            DEXIT;
            return -2;
         }
      
         sge_mutex_lock("follow_last_update_mutex", SGE_FUNC, __LINE__, &Follow_Control.last_update_mutex);
         
         if (Follow_Control.cull_order_pos != NULL) { /* do we have the positions cached? */
            ja_pos =        &(Follow_Control.cull_order_pos->ja_task);
            job_pos =       &(Follow_Control.cull_order_pos->job);
            
            while(jatp != NULL) {
               lSetPosDouble(jatp, ja_pos->JAT_tix_pos, 0);
               lSetPosDouble(jatp, ja_pos->JAT_oticket_pos, 0);
               lSetPosDouble(jatp, ja_pos->JAT_fticket_pos, 0);
               lSetPosDouble(jatp, ja_pos->JAT_sticket_pos, 0);
               lSetPosDouble(jatp, ja_pos->JAT_share_pos,   0);
               lSetPosDouble(jatp, ja_pos->JAT_prio_pos,    0);
               lSetPosDouble(jatp, ja_pos->JAT_ntix_pos,    0);
               if (task_number != 0) { /* if task_number == 0, we only change the */
                  jatp = next_ja_task; /* pending tickets, otherwise all */
                  next_ja_task = lNext(next_ja_task);
               }
               else {
                  jatp = NULL;
               }
            }

            lSetPosDouble(jep, job_pos->JB_nppri_pos,   0);
            lSetPosDouble(jep, job_pos->JB_nurg_pos,    0);
            lSetPosDouble(jep, job_pos->JB_urg_pos,     0);
            lSetPosDouble(jep, job_pos->JB_rrcontr_pos, 0);
            lSetPosDouble(jep, job_pos->JB_dlcontr_pos, 0);
            lSetPosDouble(jep, job_pos->JB_wtcontr_pos, 0);
         }
         else {   /* we do not have the positions cached.... */
            while(jatp != NULL) {
               lSetDouble(jatp, JAT_tix,     0);
               lSetDouble(jatp, JAT_oticket, 0);
               lSetDouble(jatp, JAT_fticket, 0);
               lSetDouble(jatp, JAT_sticket, 0);
               lSetDouble(jatp, JAT_share,   0);
               lSetDouble(jatp, JAT_prio,    0);
               lSetDouble(jatp, JAT_ntix,    0);
               if (task_number != 0) {   /* if task_number == 0, we only change the */
                  jatp = next_ja_task;   /* pending tickets, otherwise all */
                  next_ja_task = lNext(next_ja_task);
               }
               else {
                  jatp = NULL;
               }
            }

            lSetDouble(jep, JB_nppri,   0);
            lSetDouble(jep, JB_nurg,    0);
            lSetDouble(jep, JB_urg,     0);
            lSetDouble(jep, JB_rrcontr, 0);
            lSetDouble(jep, JB_dlcontr, 0);
            lSetDouble(jep, JB_wtcontr, 0);
            
         }
         
         sge_mutex_unlock("follow_last_update_mutex", SGE_FUNC, __LINE__, &Follow_Control.last_update_mutex);         
         
      } /* just ignore them being not in SGEEE mode */
      break;
      
   /* ----------------------------------------------------------------------- 
    * CHANGE TICKETS OF PENDING JOBS
    *
    * Modify the tickets of pending jobs for the sole purpose of being
    * able to display and sort the pending jobs list based on the
    * expected execution order.
    *
    * modifications performed on the job are not spooled 
    * ----------------------------------------------------------------------- */
   case ORT_ptickets:

      DPRINTF(("ORDER ORT_ptickets\n"));
      {
         ja_task_pos_t *ja_pos;
         ja_task_pos_t *order_ja_pos;   
         job_pos_t   *job_pos;
         job_pos_t   *order_job_pos;
         lListElem *joker;

         job_number=lGetUlong(ep, OR_job_number);
         if(!job_number) {
            ERROR((SGE_EVENT, MSG_JOB_NOJOBID));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DEXIT;
            return -2;
         }

         DPRINTF(("ORDER : job("u32")->ticket = "u32"\n", 
            job_number, (u_long32)lGetDouble(ep, OR_ticket)));

         jep = job_list_locate(Master_Job_List, job_number);
         if(!jep) {
            WARNING((SGE_EVENT, MSG_JOB_UNABLE2FINDJOBORD_U, u32c(job_number)));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DEXIT;
            return 0; /* it's ok - job has exited - forget about him */
         }
         task_number=lGetUlong(ep, OR_ja_task_number);
         if (!task_number) {
            ERROR((SGE_EVENT, MSG_JOB_NOORDERTASK_US, u32c(job_number), "ORT_ptickets"));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DEXIT;
            return -2;
         }
         jatp = job_search_task(jep, NULL, task_number);
         if (!jatp) {
            jatp = job_get_ja_task_template_pending(jep, task_number);
         }
         if (!jatp) {
            ERROR((SGE_EVENT, MSG_JOB_FINDJOBTASK_UU,  
                  u32c(task_number), u32c(job_number)));
            sge_add_event( 0, sgeE_JATASK_DEL, job_number, task_number, 
                          NULL, NULL, lGetString(jep, JB_session), NULL);
            DEXIT;
            return -2;
         }
      
         sge_mutex_lock("follow_last_update_mutex", SGE_FUNC, __LINE__, &Follow_Control.last_update_mutex);
         
         if (Follow_Control.cull_order_pos == NULL) {
            lListElem *joker_task;
            
            joker=lFirst(lGetList(ep, OR_joker));
            joker_task = lFirst(lGetList(joker, JB_ja_tasks));
            
            sge_create_cull_order_pos(&(Follow_Control.cull_order_pos), jep, jatp, joker, joker_task);
         }
        
         ja_pos =        &(Follow_Control.cull_order_pos->ja_task);
         order_ja_pos =  &(Follow_Control.cull_order_pos->order_ja_task);
         job_pos =       &(Follow_Control.cull_order_pos->job);
         order_job_pos = &(Follow_Control.cull_order_pos->order_job);
         
         if (lGetPosUlong(jatp, ja_pos->JAT_status_pos) == JFINISHED) {

            WARNING((SGE_EVENT, MSG_JOB_CHANGEPTICKETS_UU, 
                     u32c(lGetUlong(jep, JB_job_number)), 
                     u32c(lGetUlong(jatp, JAT_task_number))));
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            sge_mutex_unlock("follow_last_update_mutex", SGE_FUNC, __LINE__, &Follow_Control.last_update_mutex);
            DEXIT;
            return 0;
         }

         /* modify jobs ticket amount */
         lSetPosDouble(jatp, ja_pos->JAT_tix_pos, lGetDouble(ep, OR_ticket));

         /* check several fields to be updated */
         if ((joker=lFirst(lGetList(ep, OR_joker)))) {
            lListElem *joker_task;

            joker_task = lFirst(lGetList(joker, JB_ja_tasks));

            lSetPosDouble(jatp, ja_pos->JAT_oticket_pos, lGetPosDouble(joker_task, order_ja_pos->JAT_oticket_pos));
            lSetPosDouble(jatp, ja_pos->JAT_fticket_pos, lGetPosDouble(joker_task, order_ja_pos->JAT_fticket_pos));
            lSetPosDouble(jatp, ja_pos->JAT_sticket_pos, lGetPosDouble(joker_task, order_ja_pos->JAT_sticket_pos));
            lSetPosDouble(jatp, ja_pos->JAT_share_pos,   lGetPosDouble(joker_task, order_ja_pos->JAT_share_pos));
            lSetPosDouble(jatp, ja_pos->JAT_prio_pos,    lGetPosDouble(joker_task, order_ja_pos->JAT_prio_pos));
            lSetPosDouble(jatp, ja_pos->JAT_ntix_pos,    lGetPosDouble(joker_task, order_ja_pos->JAT_ntix_pos));

            lSetPosDouble(jep, job_pos->JB_nppri_pos,   lGetPosDouble(joker, order_job_pos->JB_nppri_pos));
            lSetPosDouble(jep, job_pos->JB_nurg_pos,    lGetPosDouble(joker, order_job_pos->JB_nurg_pos));
            lSetPosDouble(jep, job_pos->JB_urg_pos,     lGetPosDouble(joker, order_job_pos->JB_urg_pos));
            lSetPosDouble(jep, job_pos->JB_rrcontr_pos, lGetPosDouble(joker, order_job_pos->JB_rrcontr_pos));
            lSetPosDouble(jep, job_pos->JB_dlcontr_pos, lGetPosDouble(joker, order_job_pos->JB_dlcontr_pos));
            lSetPosDouble(jep, job_pos->JB_wtcontr_pos, lGetPosDouble(joker, order_job_pos->JB_wtcontr_pos));
         }
         
         sge_mutex_unlock("follow_last_update_mutex", SGE_FUNC, __LINE__, &Follow_Control.last_update_mutex);         
         
#if 0
         DPRINTF(("PRIORITY: "u32"."u32" %f/%f tix/ntix %f npri %f/%f urg/nurg %f prio\n",
            lGetUlong(jep, JB_job_number),
            lGetUlong(jatp, JAT_task_number),
            lGetDouble(jatp, JAT_tix),
            lGetDouble(jatp, JAT_ntix),
            lGetDouble(jep, JB_nppri),
            lGetDouble(jep, JB_urg),
            lGetDouble(jep, JB_nurg),
            lGetDouble(jatp, JAT_prio)));
#endif

      } /* just ignore them being not in SGEEE mode */
      break;


   /* ----------------------------------------------------------------------- 
    * CHANGE TICKETS OF RUNNING/TRANSFERING JOBS
    *
    * Our aim is to collect all ticket orders of an gdi request
    * and to send ONE packet to each exec host. Here we just
    * check the orders for consistency. They get forwarded to 
    * execd having checked all orders.  
    * 
    * modifications performed on the job are not spooled 
    * ----------------------------------------------------------------------- */
   case ORT_tickets:

      DPRINTF(("ORDER ORT_tickets\n"));
      {

         lList *oeql;
         lListElem *joker;
         int skip_ticket_order = 0;

         job_number=lGetUlong(ep, OR_job_number);
         if(!job_number) {
            ERROR((SGE_EVENT, MSG_JOB_NOJOBID));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DEXIT;
            return -2;
         }

         DPRINTF(("ORDER: job("u32")->ticket = "u32"\n", 
            job_number, (u_long32)lGetDouble(ep, OR_ticket)));

         jep = job_list_locate(Master_Job_List, job_number);
         if(!jep) {
            WARNING((SGE_EVENT, MSG_JOB_UNABLE2FINDJOBORD_U, u32c(job_number)));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DEXIT;
            return 0; /* it's ok - job has exited - forget about him */
         }
         task_number=lGetUlong(ep, OR_ja_task_number);
         if (!task_number) {
            ERROR((SGE_EVENT, MSG_JOB_NOORDERTASK_US, u32c(job_number), "ORT_tickets"));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DEXIT;
            return -2;
         }
         jatp = job_search_task(jep, NULL, task_number);
         if (!jatp) {
            ERROR((SGE_EVENT, MSG_JOB_FINDJOBTASK_UU,  
                  u32c(task_number), u32c(job_number)));
            sge_add_event( 0, sgeE_JATASK_DEL, job_number, task_number, 
                          NULL, NULL, lGetString(jep, JB_session), NULL);
            DEXIT;
            return -2;
         }

         if (lGetUlong(jatp, JAT_status) != JRUNNING && 
             lGetUlong(jatp, JAT_status) != JTRANSFERING) {

            if (lGetUlong(jatp, JAT_status) != JFINISHED) {
               WARNING((SGE_EVENT, MSG_JOB_CHANGETICKETS_UUU, 
                        u32c(lGetUlong(jep, JB_job_number)), 
                        u32c(lGetUlong(jatp, JAT_task_number)),
                        u32c(lGetUlong(jatp, JAT_status))
                        ));
               answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
               DEXIT;
               return 0;
            }
            skip_ticket_order = 1;
         }

         if (!skip_ticket_order) {
            bool destribute_tickets = false;
            /* modify jobs ticket amount and spool job */
            lSetDouble(jatp, JAT_tix, lGetDouble(ep, OR_ticket));
            DPRINTF(("TICKETS: "u32"."u32" "u32" tickets\n",
               lGetUlong(jep, JB_job_number),
               lGetUlong(jatp, JAT_task_number),
               (u_long32)lGetDouble(jatp, JAT_tix)));

            /* check several fields to be updated */
            if ((joker=lFirst(lGetList(ep, OR_joker)))) {
               lListElem *joker_task;
               ja_task_pos_t *ja_pos;
               ja_task_pos_t *order_ja_pos;   
               job_pos_t   *job_pos;
               job_pos_t   *order_job_pos;
         
               joker_task = lFirst(lGetList(joker, JB_ja_tasks));
               destribute_tickets = (lGetPosViaElem(joker_task, JAT_granted_destin_identifier_list) > -1);

               sge_mutex_lock("follow_last_update_mutex", SGE_FUNC, __LINE__, &Follow_Control.last_update_mutex);
         
               if (Follow_Control.cull_order_pos == NULL) {
                  lListElem *joker_task;
                  
                  joker=lFirst(lGetList(ep, OR_joker));
                  joker_task = lFirst(lGetList(joker, JB_ja_tasks));
                  
                  sge_create_cull_order_pos(&(Follow_Control.cull_order_pos), jep, jatp, joker, joker_task);
               }
              
               ja_pos =        &(Follow_Control.cull_order_pos->ja_task);
               order_ja_pos =  &(Follow_Control.cull_order_pos->order_ja_task);
               job_pos =       &(Follow_Control.cull_order_pos->job);
               order_job_pos = &(Follow_Control.cull_order_pos->order_job);
               
               lSetPosDouble(jatp, ja_pos->JAT_oticket_pos, lGetPosDouble(joker_task, order_ja_pos->JAT_oticket_pos));
               lSetPosDouble(jatp, ja_pos->JAT_fticket_pos, lGetPosDouble(joker_task, order_ja_pos->JAT_fticket_pos));
               lSetPosDouble(jatp, ja_pos->JAT_sticket_pos, lGetPosDouble(joker_task, order_ja_pos->JAT_sticket_pos));
               lSetPosDouble(jatp, ja_pos->JAT_share_pos,   lGetPosDouble(joker_task, order_ja_pos->JAT_share_pos));
               lSetPosDouble(jatp, ja_pos->JAT_prio_pos,    lGetPosDouble(joker_task, order_ja_pos->JAT_prio_pos));
               lSetPosDouble(jatp, ja_pos->JAT_ntix_pos,    lGetPosDouble(joker_task, order_ja_pos->JAT_ntix_pos));

               lSetPosDouble(jep, job_pos->JB_nppri_pos,   lGetPosDouble(joker, order_job_pos->JB_nppri_pos));
               lSetPosDouble(jep, job_pos->JB_nurg_pos,    lGetPosDouble(joker, order_job_pos->JB_nurg_pos));
               lSetPosDouble(jep, job_pos->JB_urg_pos,     lGetPosDouble(joker, order_job_pos->JB_urg_pos));
               lSetPosDouble(jep, job_pos->JB_rrcontr_pos, lGetPosDouble(joker, order_job_pos->JB_rrcontr_pos));
               lSetPosDouble(jep, job_pos->JB_dlcontr_pos, lGetPosDouble(joker, order_job_pos->JB_dlcontr_pos));
               lSetPosDouble(jep, job_pos->JB_wtcontr_pos, lGetPosDouble(joker, order_job_pos->JB_wtcontr_pos));               
               sge_mutex_unlock("follow_last_update_mutex", SGE_FUNC, __LINE__, &Follow_Control.last_update_mutex); 
            }

            /* tickets should only be further destributed in the scheduler reprioritize_interval. Only in
               those intervales does the ticket order structure contain a JAT_granted_destin_identifier_list.
               We use that as an identifier to go on, or not. */
            if (destribute_tickets) {
               /* add a copy of this order to the ticket orders list */
               if (!*topp) 
                  *topp = lCreateList("ticket orders", OR_Type);

               /* If a ticket order has a queuelist, then this is a parallel job
                  with controlled sub-tasks. We generate a ticket order for
                  each host in the queuelist containing the total tickets for
                  all job slots being used on the host */
               if ((oeql=lCopyList(NULL, lGetList(ep, OR_queuelist)))) {
                  const char *oep_qname=NULL, *oep_hname=NULL;
                  lListElem *oep_qep=NULL;
                  /* set granted slot tickets */
                  for_each(oep, oeql) {
                     lListElem *gdil_ep;
                     if ((gdil_ep=lGetSubStr(jatp, JG_qname, lGetString(oep, OQ_dest_queue),
                          JAT_granted_destin_identifier_list))) {
                        lSetDouble(gdil_ep, JG_ticket, lGetDouble(oep, OQ_ticket));
                        lSetDouble(gdil_ep, JG_oticket, lGetDouble(oep, OQ_oticket));
                        lSetDouble(gdil_ep, JG_fticket, lGetDouble(oep, OQ_fticket));
                        lSetDouble(gdil_ep, JG_sticket, lGetDouble(oep, OQ_sticket));
                     }
                  }

                  while((oep=lFirst(oeql))) {          
                     if (((oep_qname=lGetString(oep, OQ_dest_queue))) &&
                         ((oep_qep = cqueue_list_locate_qinstance(*(object_type_get_master_list(SGE_TYPE_CQUEUE)),
                                                       oep_qname))) &&
                         ((oep_hname=lGetHost(oep_qep, QU_qhostname)))) {

                        const char *curr_oep_qname=NULL, *curr_oep_hname=NULL;
                        lListElem *curr_oep, *next_oep, *curr_oep_qep=NULL;
                        double job_tickets_on_host = lGetDouble(oep, OQ_ticket);
                        lListElem *newep;

                        for(curr_oep=lNext(oep); curr_oep; curr_oep=next_oep) {
                           next_oep = lNext(curr_oep);
                           if (((curr_oep_qname=lGetString(curr_oep, OQ_dest_queue))) &&
                               ((curr_oep_qep = cqueue_list_locate_qinstance(*(object_type_get_master_list(SGE_TYPE_CQUEUE)), curr_oep_qname))) &&
                               ((curr_oep_hname=lGetHost(curr_oep_qep, QU_qhostname))) &&
                               !sge_hostcmp(oep_hname, curr_oep_hname)) {     /* CR SPEEDUP CANDIDATE */
                              job_tickets_on_host += lGetDouble(curr_oep, OQ_ticket);
                              lRemoveElem(oeql, curr_oep);
                           }
                        }
                        newep = lCopyElem(ep);
                        lSetDouble(newep, OR_ticket, job_tickets_on_host);
                        lAppendElem(*topp, newep);

                     } else
                        ERROR((SGE_EVENT, MSG_ORD_UNABLE2FINDHOST_S, oep_qname ? oep_qname : MSG_OBJ_UNKNOWN));

                     lRemoveElem(oeql, oep);
                  }

                  lFreeList(oeql);

               } 
               else if (lGetPosViaElem(jatp, JAT_granted_destin_identifier_list) !=-1 )
                     lAppendElem(*topp, lCopyElem(ep));
            }
         }
      } /* just ignore them being not in sge mode */
      break;

   /* ----------------------------------------------------------------------- 
    * REMOVE JOBS THAT ARE WAITING FOR SCHEDD'S PERMISSION TO GET DELETED
    *
    * Using this order schedd can only remove jobs in the 
    * "dead but not buried" state 
    * 
    * ----------------------------------------------------------------------- */
   case ORT_remove_job:
   /* ----------------------------------------------------------------------- 
    * REMOVE IMMEDIATE JOBS THAT COULD NOT GET SCHEDULED IN THIS PASS
    *
    * Using this order schedd can only remove idle immediate jobs 
    * (former ORT_remove_interactive_job)
    * ----------------------------------------------------------------------- */
   case ORT_remove_immediate_job:

      job_number=lGetUlong(ep, OR_job_number);
      if(!job_number) {
         ERROR((SGE_EVENT, MSG_JOB_NOJOBID));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DEXIT;
         return -2;
      }
      task_number=lGetUlong(ep, OR_ja_task_number);
      if (!task_number) {
         ERROR((SGE_EVENT, MSG_JOB_NOORDERTASK_US, u32c(job_number),
            (or_type==ORT_remove_immediate_job)?"ORT_remove_immediate_job":"ORT_remove_job"));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DEXIT;
         return -2;
      }
      DPRINTF(("ORDER: remove %sjob "u32"."u32"\n", 
         or_type==ORT_remove_immediate_job?"immediate ":"" ,
         job_number, task_number));
      jep = job_list_locate(Master_Job_List, job_number);
      if(!jep) {
         ERROR((SGE_EVENT, MSG_JOB_FINDJOB_U, u32c(job_number)));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         /* try to repair schedd data - session is unknown here */
         sge_add_event( 0, sgeE_JOB_DEL, job_number, task_number, 
                       NULL, NULL, NULL, NULL);
         DEXIT;
         return -1;
      }
      jatp = job_search_task(jep, NULL, task_number);

      /* if ja task doesn't exist yet, create it */
      if (jatp == NULL) {
         jatp = job_create_task(jep, NULL, task_number);

         /* new jatask has to be spooled and event sent */
         if (jatp != NULL) {
            sge_event_spool(alpp, 0, sgeE_JATASK_ADD, 
                            job_number, task_number, NULL, NULL, 
                            lGetString(jep, JB_session),
                            jep, jatp, NULL, true, true);

         } else {
            ERROR((SGE_EVENT, MSG_JOB_FINDJOBTASK_UU, u32c(task_number), 
                   u32c(job_number)));
            DEXIT;
            return -1;
         }
      }

      if (or_type==ORT_remove_job) {

         if (lGetUlong(jatp, JAT_status) != JFINISHED) {
            ERROR((SGE_EVENT, MSG_JOB_REMOVENOTFINISHED_U, u32c(lGetUlong(jep, JB_job_number))));
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DEXIT;
            return -1;
         }
    
         /* remove it */
         sge_commit_job(jep, jatp, NULL, COMMIT_ST_DEBITED_EE, COMMIT_DEFAULT);
      } else {
         if (!JOB_TYPE_IS_IMMEDIATE(lGetUlong(jep, JB_type))) {
            if(lGetString(jep, JB_script_file))
               ERROR((SGE_EVENT, MSG_JOB_REMOVENONINTERACT_U, u32c(lGetUlong(jep, JB_job_number))));
            else
               ERROR((SGE_EVENT, MSG_JOB_REMOVENONIMMEDIATE_U,  u32c(lGetUlong(jep, JB_job_number))));
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DEXIT;
            return -1;
         }
         if (lGetUlong(jatp, JAT_status) != JIDLE) {
            ERROR((SGE_EVENT, MSG_JOB_REMOVENOTIDLEIA_U, u32c(lGetUlong(jep, JB_job_number))));
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DEXIT;
            return -1;
         }
         INFO((SGE_EVENT, MSG_JOB_NOFREERESOURCEIA_UU, 
               u32c(lGetUlong(jep, JB_job_number)), 
               u32c(lGetUlong(jatp, JAT_task_number)),
               lGetString(jep, JB_owner)));

         /* remove it */
         sge_commit_job(jep, jatp, NULL, COMMIT_ST_NO_RESOURCES, COMMIT_DEFAULT | COMMIT_NEVER_RAN);
      }
      break;

   /* ----------------------------------------------------------------------- 
    * REPLACE A USER/PROJECT'S 
    * 
    * - UP_usage   
    * - UP_usage_time_stamp   
    * - UP_long_term_usage  
    * - UP_debited_job_usage  
    *
    * Using this order schedd can debit usage on users/projects
    * both orders are handled identically except target list
    * ----------------------------------------------------------------------- */
   case ORT_update_project_usage:
   case ORT_update_user_usage:
      DPRINTF(("ORDER: ORT_update_project_usage/ORT_update_user_usage\n"));
      {
         lListElem *up_order, *up, *ju, *up_ju, *next;
         int pos;
         const char *up_name;
         lList *tlp;
         u_long32 now = sge_get_gmt();
         bool is_spool = false;

         
         sge_mutex_lock("follow_last_update_mutex", SGE_FUNC, __LINE__, &Follow_Control.last_update_mutex);
         if (now < Follow_Control.last_update) {
            Follow_Control.last_update = now;
         }   

         if (now >= (Follow_Control.last_update + spool_time)) {
            is_spool = true; 
            Follow_Control.last_update = now;
         }
         sge_mutex_unlock("follow_last_update_mutex", SGE_FUNC, __LINE__, &Follow_Control.last_update_mutex);

         DPRINTF(("ORDER: update %d users/prjs\n", 
            lGetNumberOfElem(lGetList(ep, OR_joker))));

         for_each (up_order, lGetList(ep, OR_joker)) {
            if ((pos=lGetPosViaElem(up_order, UP_name))<0 || 
                  !(up_name = lGetString(up_order, UP_name))) {
               continue;
            }   

            DPRINTF(("%s %s usage updating with %d jobs\n",
               or_type==ORT_update_project_usage?"project":"user",
               up_name, lGetNumberOfElem(lGetList(up_order, 
               UP_debited_job_usage))));

            if (!(up=userprj_list_locate( 
                  or_type==ORT_update_project_usage ? Master_Project_List : Master_User_List, up_name ))) {
               /* order contains reference to unknown user/prj object */
               continue;
            }   

            if ((pos=lGetPosViaElem(up_order, UP_version))>=0 &&
                (lGetPosUlong(up_order, pos) != lGetUlong(up, UP_version))) {
               /* order contains update for outdated user/project usage */
               ERROR((SGE_EVENT, MSG_ORD_USRPRJVERSION_UUS, u32c(lGetPosUlong(up_order, pos)),
                      u32c(lGetUlong(up, UP_version)), up_name));
               /* Note: Should we apply the debited job usage in this case? */
               continue;
            }

            lSetUlong(up, UP_version, lGetUlong(up, UP_version)+1);

            if ((pos=lGetPosViaElem(up_order, UP_project))>=0) {
               lSwapList(up_order, UP_project, up, UP_project);
            }

            if ((pos=lGetPosViaElem(up_order, UP_usage_time_stamp))>=0)
               lSetUlong(up, UP_usage_time_stamp, lGetPosUlong(up_order, pos));

            if ((pos=lGetPosViaElem(up_order, UP_usage))>=0) {
               lSwapList(up_order, UP_usage, up, UP_usage);
            }

            if ((pos=lGetPosViaElem(up_order, UP_long_term_usage))>=0) {
               lSwapList(up_order, UP_long_term_usage, up, UP_long_term_usage);
            }

            /* update old usage in up for each job appearing in
               UP_debited_job_usage of 'up_order' */
            next = lFirst(lGetList(up_order, UP_debited_job_usage));
            while ((ju = next)) {
               next = lNext(ju);

               job_number = lGetUlong(ju, UPU_job_number);
              
               /* seek for existing debited usage of this job */
               if ((up_ju=lGetSubUlong(up, UPU_job_number, job_number,
                                       UP_debited_job_usage))) { 
                  
                  /* if passed old usage list is NULL, delete existing usage */
                  if (lGetList(ju, UPU_old_usage_list) == NULL) {

                     lRemoveElem(lGetList(up_order, UP_debited_job_usage), ju);
                     lRemoveElem(lGetList(up, UP_debited_job_usage), up_ju);

                  } else {

                     /* still exists - replace old usage with new one */
                     DPRINTF(("updating debited usage for job "u32"\n", job_number));
                     lSwapList(ju, UPU_old_usage_list, up_ju, UPU_old_usage_list);
                  }

               } else {
                  /* unchain ju element and chain it into our user/prj object */
                  DPRINTF(("adding debited usage for job "u32"\n", job_number));
                  lDechainElem(lGetList(up_order, UP_debited_job_usage), ju);

                  if (lGetList(ju, UPU_old_usage_list) != NULL) {
                     /* unchain ju element and chain it into our user/prj object */
                     if (!(tlp=lGetList(up, UP_debited_job_usage))) {
                        tlp = lCreateList(up_name, UPU_Type);
                        lSetList(up, UP_debited_job_usage, tlp);
                     }
                     lInsertElem(tlp, NULL, ju);
                  } else {
                     /* do not chain in empty empty usage records */
                     lFreeElem(ju);
                  }
               }
            }

            /* spool and send event */
            
            {
               lList *answer_list = NULL;
               sge_event_spool(&answer_list, now,
                               or_type == ORT_update_user_usage ? 
                                          sgeE_USER_MOD:sgeE_PROJECT_MOD,
                               0, 0, up_name, NULL, NULL,
                               up, NULL, NULL, true, is_spool);
               answer_list_output(&answer_list);
            }
         }
      } /* just ignore them being not in sge mode */
      break;

   /* ----------------------------------------------------------------------- 
    * FILL IN SEVERAL SCHEDULING VALUES INTO QMASTERS SHARE TREE 
    * TO BE DISPLAYED BY QMON AND OTHER CLIENTS
    * ----------------------------------------------------------------------- */
   case ORT_share_tree:
      DPRINTF(("ORDER: ORT_share_tree\n"));
      if ( !sge_init_node_fields(lFirst(Master_Sharetree_List)) &&
	        update_sharetree(alpp, Master_Sharetree_List, lGetList(ep, OR_joker))) {
         /* alpp gets filled by update_sharetree */
         DPRINTF(("ORDER: ORT_share_tree failed\n" ));
         DEXIT;
         return -1;
      } /* just ignore it being not in sge mode */
       
      break;

   /* ----------------------------------------------------------------------- 
    * UPDATE FIELDS IN SCHEDULING CONFIGURATION 
    * ----------------------------------------------------------------------- */
   case ORT_sched_conf:
      {
         lListElem *joker;
         int pos;

         DPRINTF(("ORDER: ORT_sched_conf\n" ));
         joker = lFirst(lGetList(ep, OR_joker));

         if (sconf_is() && joker != NULL) {
            if ((pos=lGetPosViaElem(joker, SC_weight_tickets_override)) > -1) {
               sconf_set_weight_tickets_override( lGetPosUlong(joker, pos));
            }   
         }
         /* no need to spool sched conf */
      }
       
      break;

   case ORT_suspend_on_threshold:
      {
         lListElem *queueep;
         u_long32 jobid;

         DPRINTF(("ORDER: ORT_suspend_on_threshold\n"));

         jobid = lGetUlong(ep, OR_job_number);
         task_number = lGetUlong(ep, OR_ja_task_number);

         if (!(jep = job_list_locate(Master_Job_List, jobid))
            || !(jatp = job_search_task(jep, NULL, task_number))
            || !lGetList(jatp, JAT_granted_destin_identifier_list)) {
            /* don't panic - it is probably an exiting job */
            WARNING((SGE_EVENT, MSG_JOB_SUSPOTNOTRUN_UU, u32c(jobid), u32c(task_number)));
         } else {
            const char *qnm = lGetString(lFirst(lGetList(jatp, JAT_granted_destin_identifier_list)), JG_qname);
            queueep = cqueue_list_locate_qinstance(*(object_type_get_master_list(SGE_TYPE_CQUEUE)), qnm);
            if (!queueep) {
               ERROR((SGE_EVENT, MSG_JOB_UNABLE2FINDMQ_SU,
                     qnm, u32c(jobid)));
               answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
               DEXIT;
               return -1;
            }
            
            INFO((SGE_EVENT, MSG_JOB_SUSPTQ_UUS, u32c(jobid), u32c(task_number), qnm));

            if (!ISSET(lGetUlong(jatp, JAT_state), JSUSPENDED)) {
               sge_signal_queue(SGE_SIGSTOP, queueep, jep, jatp);
               state = lGetUlong(jatp, JAT_state);
               CLEARBIT(JRUNNING, state);
               lSetUlong(jatp, JAT_state, state);
            } 
            state = lGetUlong(jatp, JAT_state);
            SETBIT(JSUSPENDED_ON_THRESHOLD, state);
            lSetUlong(jatp, JAT_state, state);

            {
               lList *answer_list = NULL;
               const char *session = lGetString (jep, JB_session);
               sge_event_spool(&answer_list, 0, sgeE_JATASK_MOD,
                               jobid, task_number, NULL, NULL, session,
                               jep, jatp, NULL, true, true);
               answer_list_output(&answer_list);
            }

            /* update queues time stamp in schedd */
            lSetUlong(queueep, QU_last_suspend_threshold_ckeck, sge_get_gmt());
            qinstance_add_event(queueep, sgeE_QINSTANCE_MOD);
         }
      }
      break;

   case ORT_unsuspend_on_threshold:
      {
         lListElem *queueep;
         u_long32 jobid;

         DPRINTF(("ORDER: ORT_unsuspend_on_threshold\n"));

         jobid = lGetUlong(ep, OR_job_number);
         task_number = lGetUlong(ep, OR_ja_task_number);

         if (!(jep = job_list_locate(Master_Job_List, jobid))
            || !(jatp = job_search_task(jep, NULL,task_number))
            || !lGetList(jatp, JAT_granted_destin_identifier_list)) {
            /* don't panic - it is probably an exiting job */  
            WARNING((SGE_EVENT, MSG_JOB_UNSUSPOTNOTRUN_UU, u32c(jobid), u32c(task_number)));
         } 
         else {
            const char *qnm = lGetString(lFirst(lGetList(jatp, JAT_granted_destin_identifier_list)), JG_qname);
            queueep = cqueue_list_locate_qinstance(*(object_type_get_master_list(SGE_TYPE_CQUEUE)), qnm);
            if (!queueep) {
               ERROR((SGE_EVENT, MSG_JOB_UNABLE2FINDMQ_SU, qnm, u32c(jobid)));
               answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
               DEXIT;
               return -1;
            }

            INFO((SGE_EVENT, MSG_JOB_UNSUSPOT_UUS, u32c(jobid), u32c(task_number), qnm));
      
            if (!ISSET(lGetUlong(jatp, JAT_state), JSUSPENDED)) {
               sge_signal_queue(SGE_SIGCONT, queueep, jep, jatp);
               state = lGetUlong(jatp, JAT_state);
               SETBIT(JRUNNING, state);
               lSetUlong(jatp, JAT_state, state);
            }
            state = lGetUlong(jatp, JAT_state);
            CLEARBIT(JSUSPENDED_ON_THRESHOLD, state);
            lSetUlong(jatp, JAT_state, state);
            {
               lList *answer_list = NULL;
               const char *session = lGetString (jep, JB_session);
               sge_event_spool(&answer_list, 0, sgeE_JATASK_MOD,
                               jobid, task_number, NULL, NULL, session,
                               jep, jatp, NULL, true, true);
               answer_list_output(&answer_list);
            }
            /* update queues time stamp in schedd */
            lSetUlong(queueep, QU_last_suspend_threshold_ckeck, sge_get_gmt());
            qinstance_add_event(queueep, sgeE_QINSTANCE_MOD);
         }
      }
      break;

   case ORT_job_schedd_info:
      {
         lList *sub_order_list = NULL;
         lListElem *sme  = NULL; /* SME_Type */

         DPRINTF(("ORDER: ORT_job_schedd_info\n"));
         sub_order_list = lGetList(ep, OR_joker);
         
         if (sub_order_list != NULL) {
            sme = lFirst(sub_order_list);
            lDechainElem(sub_order_list, sme);
         }

         if (sme != NULL) {
            if (Master_Job_Schedd_Info_List != NULL) {
               Master_Job_Schedd_Info_List = lFreeList(Master_Job_Schedd_Info_List);
            }

            Master_Job_Schedd_Info_List = lCreateList("schedd info", SME_Type);
            lAppendElem(Master_Job_Schedd_Info_List, sme);

            /* this information is not spooled (but might be usefull in a db) */
            sge_add_event( 0, sgeE_JOB_SCHEDD_INFO_MOD, 0, 0, 
                       NULL, NULL, NULL, sme);
         }              
      }
      break;

   default:
      break;
   }

  DEXIT;
  return STATUS_OK;
}

/*
 * MT-NOTE: distribute_ticket_orders() is NOT MT safe
 */
int distribute_ticket_orders(
lList *ticket_orders 
) {
   u_long32 jobid, jataskid; 
   lList *to_send;
   const char *host_name;
   const char *master_host_name;
   lListElem *jep, *other_jep, *ep, *ep2, *next, *other, *jatask = NULL, *other_jatask;
   sge_pack_buffer pb;
   u_long32 dummymid = 0;
   int n;
   u_long32 now;
   int cl_err = CL_RETVAL_OK;
   unsigned long last_heard_from;

   DENTER(TOP_LAYER, "distribute_ticket_orders");

   now = sge_get_gmt();

   while ((ep=lFirst(ticket_orders))) {     /* CR SPEEDUP CANDIDATE */
      lListElem *hep;
      lListElem *destin_elem;
   
      jobid = lGetUlong(ep, OR_job_number);
      jataskid = lGetUlong(ep, OR_ja_task_number);

      DPRINTF(("Job: %ld, Task: %ld", jobid, jataskid));

      /* seek job element */
      if (!(jep = job_list_locate(Master_Job_List, jobid)) || 
          !(jatask = job_search_task(jep, NULL, jataskid))) { 
         ERROR((SGE_EVENT, MSG_JOB_MISSINGJOBTASK_UU, u32c(jobid), u32c(jataskid)));
         lRemoveElem(ticket_orders, ep);
      }
 
      /* seek master queue */
      destin_elem = lFirst(lGetList(jatask, JAT_granted_destin_identifier_list));
      
      if (destin_elem == NULL) { /* the job has finished, while we are processing the events. No need to assign the */
         lRemoveElem(ticket_orders, ep);
         continue;               /* the running tickets, since there is nothing running anymore and we do not have */
      }                          /* the information, where the job was running */

      master_host_name = lGetHost(destin_elem, JG_qhostname);

      /* put this one in 'to_send' */ 
      to_send = lCreateList("to send", lGetElemDescr(ep));
      lDechainElem(ticket_orders, ep);
      lAppendElem(to_send, ep);

      /* 
         now seek all other ticket orders 
         for jobs residing on this host 
         and add them to 'to_send'
      */ 
      next = lFirst(ticket_orders);
      while ((other=next)) {      /* CR SPEEDUP CANDIDATE */
         next = lNext(other);

         other_jep = job_list_locate(Master_Job_List, lGetUlong(other, OR_job_number)); 
         other_jatask = job_search_task(other_jep, NULL, lGetUlong(other, OR_ja_task_number));
         if (!other_jep || !other_jatask) {
            ERROR((SGE_EVENT, MSG_JOB_MISSINGJOBTASK_UU, u32c(jobid), u32c(jataskid)));
            lRemoveElem(ticket_orders, other);
         }
         else { 
            destin_elem = lFirst(lGetList(jatask, JAT_granted_destin_identifier_list));

            if (destin_elem == NULL) { /* the job has finished, while we are processing the events. No need to assign the */
               lRemoveElem(ticket_orders, other);
               continue;               /* the running tickets, since there is nothing running anymore and we do not have */
            }                          /* the information, where the job was running */
      
            host_name = lGetHost(destin_elem, JG_qhostname);

            if (!sge_hostcmp(host_name, master_host_name)) {
               /* add it */
               lDechainElem(ticket_orders, other);
               lAppendElem(to_send, other);
            } 
         }
      }
 
      hep = host_list_locate(Master_Exechost_List, master_host_name);
      n = lGetNumberOfElem(to_send);

      if (hep) {
         cl_commlib_get_last_message_time((cl_com_get_handle((char*)uti_state_get_sge_formal_prog_name() ,0)),
                                        (char*)master_host_name, (char*)prognames[EXECD],1, &last_heard_from);
      }
      if (  hep &&  last_heard_from + 10 * conf.load_report_time > now) {

         /* should have now all ticket orders for 'host' in 'to_send' */ 
         if (init_packbuffer(&pb, sizeof(u_long32)*3*n, 0)==PACK_SUCCESS) {
            for_each (ep2, to_send) {
               packint(&pb, lGetUlong(ep2, OR_job_number));
               packint(&pb, lGetUlong(ep2, OR_ja_task_number));
               packdouble(&pb, lGetDouble(ep2, OR_ticket));
            }
            cl_err = gdi_send_message_pb(0, prognames[EXECD], 1, master_host_name, 
                                         TAG_CHANGE_TICKET, &pb, &dummymid);
            clear_packbuffer(&pb);
            DPRINTF(("%s %d ticket changings to execd@%s\n", 
                     (cl_err==CL_RETVAL_OK)?"failed sending":"sent", n, master_host_name));
         }
      } else {
         DPRINTF(("     skipped sending of %d ticket changings to "
               "execd@%s because %s\n", n, master_host_name, 
               !hep?"no such host registered":"suppose host is down"));
      }

      to_send = lFreeList(to_send);
   }

   DEXIT;
   return cl_err;
}

