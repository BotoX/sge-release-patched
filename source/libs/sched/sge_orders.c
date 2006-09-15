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
#include <unistd.h>
#include <sys/types.h>

#include "sge_orders.h"
#include "sge_all_listsL.h"
#include "sge_feature.h"
#include "sgermon.h"
#include "sge_log.h"
#include "schedd_message.h"
#include "sge_answer.h"
#include "sge_event_client.h"
#include "msg_schedd.h"
#include "msg_common.h"
#include "sge_profiling.h"
#include "sge_order.h"

#ifdef TEST_GDI2
#include "sge_gdi_ctx.h"
#include "sge_event_client2.h"
#endif

/****** sge_orders/sge_add_schedd_info() ***************************************
*  NAME
*     sge_add_schedd_info() -- retrieves the messages and generates an order out 
*                              of it.
*
*  SYNOPSIS
*     lList* sge_add_schedd_info(lList *or_list, int *global_mes_count, int 
*     *job_mes_count) 
*
*  FUNCTION
*     retrieves all messages, puts them into an order package, and frees the
*     orginal messages. It also returns the number of global and job messages.
*
*  INPUTS
*     lList *or_list        - int: the order list to which the message order is added
*     int *global_mes_count - out: global message count
*     int *job_mes_count    - out: job message count
*
*  RESULT
*     lList* - the order list
*
*  NOTES
*     MT-NOTE: sge_add_schedd_info() is not MT safe 
*
*******************************************************************************/
lList *sge_add_schedd_info(lList *or_list, int *global_mes_count, int *job_mes_count) 
{
   lList *jlist;
   lListElem *sme, *ep;

   DENTER(TOP_LAYER, "sge_add_schedd_info");

   sme = schedd_mes_obtain_package(global_mes_count, job_mes_count);

   if (!sme || (lGetNumberOfElem(lGetList(sme, SME_message_list)) < 1 
         && lGetNumberOfElem(lGetList(sme, SME_global_message_list)) < 1)) {
      DEXIT;
      return or_list;
   }
   
   /* create orders list if not existent */
   if (!or_list) {
      or_list = lCreateList("orderlist", OR_Type);
   }   

   /* build order */
   ep=lCreateElem(OR_Type);   
   
   jlist = lCreateList("", SME_Type);
   lAppendElem(jlist, sme);
   lSetList(ep, OR_joker, jlist);
   
   lSetUlong(ep, OR_type, ORT_job_schedd_info);
   lAppendElem(or_list, ep);

   DEXIT;
   return or_list;
}

/*************************************************************
 Create a new order-list or add orders to an existing one.
 or_list==NULL -> create new one.
 or_list!=NULL -> append orders.
 returns updated order-list.

 The granted list contains granted queues.

 TODO SG: add adoc header, and comment on use of ja_task in here.

 is MT safe
 *************************************************************/


/****** sge_orders/sge_create_orders() *****************************************
*  NAME
*     sge_create_orders() -- Create a new order-list or add orders to an existing one
*
*  SYNOPSIS
*     lList* sge_create_orders(lList *or_list, u_long32 type, lListElem *job, 
*     lListElem *ja_task, lList *granted, bool update_execd) 
*
*  FUNCTION
*     - If the or_list is NULL, a new one will be generated
*
*     - in case of a clear_pri order, teh ja_task is improtant. If NULL is put
*       in for ja_task, only the pendin tasks of the spedified job are set to NULL.
*       If a ja_task is put in, all tasks of the job are set to NULL
*
*  INPUTS
*     lList *or_list     - the order list
*     u_long32 type      - order type
*     lListElem *job     - job
*     lListElem *ja_task - ja_task ref or NULL(there is only one case, where it can be NULL)
*     lList *granted     - granted queue list
*     bool update_execd  - should the execd get new ticket values?
*
*  RESULT
*     lList* - returns the orderlist
*
*  NOTES
*     MT-NOTE: sge_create_orders() is MT safe 
*
*  SEE ALSO
*     ???/???
*******************************************************************************/
lList 
*sge_create_orders(lList *or_list, u_long32 type, lListElem *job, lListElem *ja_task,
                   lList *granted , bool update_execd) 
{
   lList *ql = NULL;
   lListElem *gel, *ep, *ep2;
   u_long32 qslots;
  
   DENTER(TOP_LAYER, "sge_create_orders");
   
   if (!job) {
      lFreeList(&or_list);
      DEXIT;
      return or_list;
   }

   /* create orders list if not existent */
   if (!or_list) {
      or_list = lCreateList("orderlist", OR_Type);
   }   

   /* build sublist of granted */
   if (update_execd) {
      for_each(gel, granted) {
         qslots = lGetUlong(gel, JG_slots);
         if (qslots) { /* ignore Qs with slots==0 */
            ep2=lCreateElem(OQ_Type);

            lSetUlong(ep2, OQ_slots, qslots);
            lSetString(ep2, OQ_dest_queue, lGetString(gel, JG_qname));
            lSetUlong(ep2, OQ_dest_version, lGetUlong(gel, JG_qversion));
            lSetDouble(ep2, OQ_ticket, lGetDouble(gel, JG_ticket));
            lSetDouble(ep2, OQ_oticket, lGetDouble(gel, JG_oticket));
            lSetDouble(ep2, OQ_fticket, lGetDouble(gel, JG_fticket));
            lSetDouble(ep2, OQ_sticket, lGetDouble(gel, JG_sticket));
            if (!ql)
               ql=lCreateList("orderlist",OQ_Type);
            lAppendElem(ql, ep2);
         }
      }
   }

   /* build order */
   ep=lCreateElem(OR_Type);

   if(ja_task != NULL) {
      lSetDouble(ep, OR_ticket,    lGetDouble(ja_task, JAT_tix));
      lSetDouble(ep, OR_ntix,      lGetDouble(ja_task, JAT_ntix));
      lSetDouble(ep, OR_prio,      lGetDouble(ja_task, JAT_prio));
   }
   
   if (type == ORT_tickets || type == ORT_ptickets) {
      
      static order_pos_t *order_pos = NULL;
      
      const lDescr tixDesc[] = {
                            {JAT_task_number, lUlongT},
                            {JAT_tix, lDoubleT},
                            {JAT_oticket, lDoubleT}, 
                            {JAT_fticket, lDoubleT },
                            {JAT_sticket, lDoubleT },
                            {JAT_share, lDoubleT},
                            {JAT_prio, lDoubleT},
                            {JAT_ntix, lDoubleT},
                            {JAT_granted_destin_identifier_list, lListT},
                            {NoName, lEndT}
                           };
      const lDescr tix2Desc[] = {
                             {JAT_task_number, lUlongT},
                             {JAT_tix, lDoubleT},
                             {JAT_oticket, lDoubleT}, 
                             {JAT_fticket, lDoubleT },
                             {JAT_sticket, lDoubleT },
                             {JAT_share, lDoubleT},
                             {JAT_prio, lDoubleT},
                             {JAT_ntix, lDoubleT},
                             {NoName, lEndT}
                            };
      const lDescr jobDesc[] = {
                                 {JB_nppri, lDoubleT },
                                 {JB_nurg, lDoubleT },
                                 {JB_urg, lDoubleT },
                                 {JB_rrcontr, lDoubleT },
                                 {JB_dlcontr, lDoubleT },
                                 {JB_wtcontr, lDoubleT },
                                 {JB_ja_tasks, lListT},
                                 {NoName, lEndT}
                               };
      ja_task_pos_t *ja_pos;
      ja_task_pos_t *order_ja_pos;   
      job_pos_t   *job_pos;
      job_pos_t   *order_job_pos;
      lListElem *jep = lCreateElem(jobDesc);
      lList *jlist = lCreateList("", jobDesc);
     
      if (order_pos == NULL) {
         lListElem *tempElem = lCreateElem(tix2Desc);

         sge_create_cull_order_pos(&order_pos, job, ja_task, jep, tempElem);    
         
         lFreeElem(&tempElem);
      }

      ja_pos = &(order_pos->ja_task);
      order_ja_pos = &(order_pos->order_ja_task);
      job_pos = &(order_pos->job);
      order_job_pos = &(order_pos->order_job);
     

      /* Create a reduced task list with only the required fields */
      {           
         lList *tlist = NULL;         
         lListElem *tempElem = NULL;

         if (update_execd){
            tlist = lCreateList("", tixDesc);
            tempElem = lCreateElem(tixDesc); 
            lSetList(tempElem, JAT_granted_destin_identifier_list, 
                     lCopyList("", lGetList(ja_task, JAT_granted_destin_identifier_list)));
         }
         else {
            tlist = lCreateList("", tix2Desc);
            tempElem = lCreateElem(tix2Desc);
         }

         lAppendElem(tlist, tempElem);
         
         lSetPosDouble(tempElem, order_ja_pos->JAT_tix_pos,     lGetPosDouble(ja_task,ja_pos->JAT_tix_pos));
         lSetPosDouble(tempElem, order_ja_pos->JAT_oticket_pos, lGetPosDouble(ja_task,ja_pos->JAT_oticket_pos));
         lSetPosDouble(tempElem, order_ja_pos->JAT_fticket_pos, lGetPosDouble(ja_task,ja_pos->JAT_fticket_pos));
         lSetPosDouble(tempElem, order_ja_pos->JAT_sticket_pos, lGetPosDouble(ja_task,ja_pos->JAT_sticket_pos));
         lSetPosDouble(tempElem, order_ja_pos->JAT_share_pos,   lGetPosDouble(ja_task,ja_pos->JAT_share_pos));
         lSetPosDouble(tempElem, order_ja_pos->JAT_prio_pos,    lGetPosDouble(ja_task,ja_pos->JAT_prio_pos));
         lSetPosDouble(tempElem, order_ja_pos->JAT_ntix_pos,    lGetPosDouble(ja_task,ja_pos->JAT_ntix_pos));            
      
         lSetList(jep, JB_ja_tasks, tlist);
      }

      /* Create a reduced job list with only the required fields */
      lAppendElem(jlist, jep);
      
      lSetPosDouble(jep, order_job_pos->JB_nppri_pos,   lGetPosDouble(job, job_pos->JB_nppri_pos));
      lSetPosDouble(jep, order_job_pos->JB_nurg_pos,    lGetPosDouble(job, job_pos->JB_nurg_pos));
      lSetPosDouble(jep, order_job_pos->JB_urg_pos,     lGetPosDouble(job, job_pos->JB_urg_pos));
      lSetPosDouble(jep, order_job_pos->JB_rrcontr_pos, lGetPosDouble(job, job_pos->JB_rrcontr_pos));
      lSetPosDouble(jep, order_job_pos->JB_dlcontr_pos, lGetPosDouble(job, job_pos->JB_dlcontr_pos));
      lSetPosDouble(jep, order_job_pos->JB_wtcontr_pos, lGetPosDouble(job, job_pos->JB_wtcontr_pos));

      lSetList(ep, OR_joker, jlist);
   }

   lSetUlong(ep, OR_type, type);
   lSetUlong(ep, OR_job_number, lGetUlong(job, JB_job_number));
   lSetUlong(ep, OR_job_version, lGetUlong(job, JB_version));
   lSetList(ep, OR_queuelist, ql);

   if (ja_task != NULL) {
      const char *s = NULL;

      lSetUlong(ep, OR_ja_task_number, lGetUlong(ja_task, JAT_task_number));
      s = lGetString(ja_task, JAT_granted_pe);
      if (s != NULL) {
         lSetString(ep, OR_pe, s);
      }   
   }

   lAppendElem(or_list, ep);

   DEXIT;
   return or_list; 
}


/*************************************************************
  
 *************************************************************/
int 
sge_send_orders2master(void *evc_context, lList **orders) 
{
   int ret = STATUS_OK;
   lList *alp = NULL;
   lList *malp = NULL;

   int set_busy, order_id = 0;
   state_gdi_multi state = STATE_GDI_MULTI_INIT;
#ifdef TEST_GDI2
   sge_evc_class_t *evc = (sge_evc_class_t *)evc_context;
   sge_gdi_ctx_class_t *ctx = evc->get_gdi_ctx(evc);
#else
   lListElem *event_client = ec_get_event_client();
#endif   

   DENTER(TOP_LAYER, "sge_send_orders2master");

   /* do we have to set event client to "not busy"? */
#ifdef TEST_GDI2
   set_busy = (evc->ec_get_busy_handling(evc) == EV_BUSY_UNTIL_RELEASED);
#else
   set_busy = (ec_get_busy_handling(event_client) == EV_BUSY_UNTIL_RELEASED);
#endif   

   if (*orders != NULL) {
      DPRINTF(("SENDING %d ORDERS TO QMASTER\n", lGetNumberOfElem(*orders)));
#ifdef TEST_GDI2
      if(set_busy) {
         order_id = ctx->gdi_multi(ctx, &alp, SGE_GDI_RECORD, SGE_ORDER_LIST, SGE_GDI_ADD,
                                  orders, NULL, NULL, NULL, &state, false);
      } else {
         order_id = ctx->gdi_multi(ctx, &alp, SGE_GDI_SEND, SGE_ORDER_LIST, SGE_GDI_ADD,
                                  orders, NULL, NULL, &malp, &state, false);
      }
#else
      if(set_busy) {
         order_id = sge_gdi_multi(&alp, SGE_GDI_RECORD, SGE_ORDER_LIST, SGE_GDI_ADD,
                                  orders, NULL, NULL, NULL, &state, false);
      } else {
         order_id = sge_gdi_multi(&alp, SGE_GDI_SEND, SGE_ORDER_LIST, SGE_GDI_ADD,
                                  orders, NULL, NULL, &malp, &state, false);
      }
#endif      

      if (alp != NULL) {
         ret = answer_list_handle_request_answer_list(&alp, stderr);
         DEXIT;
         return ret;
      }
   }   

   /* if necessary, set busy state to "not busy" */
   if(set_busy) {
      DPRINTF(("RESETTING BUSY STATE OF EVENT CLIENT\n"));
#ifdef TEST_GDI2
      evc->ec_set_busy(evc, 0);
      evc->ec_commit_multi(evc, &malp, &state);
#else
      ec_set_busy(event_client, 0);
      ec_commit_multi(event_client, &malp, &state);
#endif      
   }

   /* check result of orders */
   if(order_id > 0) {
      sge_gdi_extract_answer(&alp, SGE_GDI_ADD, SGE_ORDER_LIST, order_id, malp, NULL);

      ret = answer_list_handle_request_answer_list(&alp, stderr);
   }

   lFreeList(&malp);
   DEXIT;
   return ret;
}



/*--------------------------------------------------------------------
 * build a ORT_remove_job order for each finished job 
 *--------------------------------------------------------------------*/
lList *create_delete_job_orders(
lList *finished_jobs, 
lList *order_list  
) {
   lListElem *job, *ja_task;

   DENTER(TOP_LAYER, "create_delete_job_orders");

   for_each(job, finished_jobs) {
      for_each(ja_task, lGetList(job, JB_ja_tasks)) {
         DPRINTF(("DELETE JOB "sge_u32"."sge_u32"\n", lGetUlong(job, JB_job_number),
            lGetUlong(ja_task, JAT_task_number)));
         order_list = sge_create_orders(order_list, ORT_remove_job, job, 
            ja_task, NULL, true);
      }
   }

   DEXIT;
   return order_list;
}



/****** sge_orders/sge_join_orders() ******************************************
*  NAME
*     sge_join_orders() -- generates one order list from the order structure 
*
*  SYNOPSIS
*     lLlist* sge_join_orders(order_t orders) 
*
*  FUNCTION
*      generates one order list from the order structure, and cleans the
*      the order structure. The orders, which have been send already, are
*      removed.
*
*  INPUTS
*     order_t orders - the order strucutre
*
*  RESULT
*     lLlist* - a order list
*
*  NOTES
*     MT-NOTE: sge_join_orders() is not  safe 
*
*******************************************************************************/
lList *sge_join_orders(order_t *orders){
      lList *orderlist=NULL;
   
      orderlist = orders->configOrderList;
      orders->configOrderList = NULL;
  
      
      if (orderlist == NULL) {
         orderlist = orders->jobStartOrderList;
      }
      else {
         lAddList(orderlist, &(orders->jobStartOrderList));
      }   
      orders->jobStartOrderList = NULL; 
    
      
      if (orderlist == NULL) {
         orderlist = orders->pendingOrderList;
      }
      else {
         lAddList(orderlist, &(orders->pendingOrderList));
      }
      orders->pendingOrderList= NULL;

      
      /* they have been send earlier, so we can remove them */
      lFreeList(&(orders->sentOrderList));

      return orderlist;
}


/****** sge_orders/sge_GetNumberOfOrders() *************************************
*  NAME
*     sge_GetNumberOfOrders() -- returns the number of orders generated
*
*  SYNOPSIS
*     int sge_GetNumberOfOrders(order_t *orders) 
*
*  FUNCTION
*     returns the number of orders generated
*
*  INPUTS
*     order_t *orders - a structure of orders
*
*  RESULT
*     int - number of orders in the structure
*
*  NOTES
*     MT-NOTE: sge_GetNumberOfOrders() is  MT safe 
*
*******************************************************************************/
int sge_GetNumberOfOrders(order_t *orders) {
   int count = 0;

   count += lGetNumberOfElem(orders->configOrderList);
   count += lGetNumberOfElem(orders->pendingOrderList);
   count += lGetNumberOfElem(orders->jobStartOrderList);
   count += lGetNumberOfElem(orders->sentOrderList);

   return count;
}


/****** sge_orders/sge_send_job_start_orders() *********************************
*  NAME
*     sge_send_job_start_orders() -- sends the job start orders to the master
*
*  SYNOPSIS
*     int sge_send_job_start_orders(order_t *orders) 
*
*  FUNCTION
*     If many jobs are dispatched during one scheduling run, this function
*     can submit the already generated job start orders to the master, so it
*     can start the jobs and does not need for the scheduling cycle to finish.
*
*     The config orders are submitted as well (queue suspend or unsuspend, job
*     suspend or unsuspend. These orders are deleted in the function and the
*     job start orders are moved to the send_job_StartOrderList.
*
*  INPUTS
*     order_t *orders - the order structure
*
*  RESULT
*     int - STATUS_OK
*
*  NOTES
*     MT-NOTE: sge_send_job_start_orders() is MT safe 
*
*******************************************************************************/
int sge_send_job_start_orders(void *evc_context, order_t *orders) {
   lList *alp = NULL;
   lList *malp = NULL;
   sge_gdi_request *answer= NULL;
   int config_mode = SGE_GDI_RECORD;
   int start_mode = SGE_GDI_RECORD;
   state_gdi_multi state = STATE_GDI_MULTI_INIT;
#ifdef TEST_GDI2
   sge_evc_class_t *evc = (sge_evc_class_t *)evc_context;
   sge_gdi_ctx_class_t *ctx = evc->get_gdi_ctx(evc);
#endif

   DENTER(TOP_LAYER, "sge_send_orders2master");

#ifdef TEST_GDI2
   if(!ctx->gdi_receive_multi_async(ctx, &answer, &malp, false)) {
      DEXIT;
      return false;
   }
   else {
      /* check for a sucessful send */
      lFreeList(&malp); 
   }
#else   
   if(!gdi_receive_multi_async(&answer, &malp, false)) {
      DEXIT;
      return false;
   }
   else {
      /* check for a sucessful send */
      lFreeList(&malp); 
   }
#endif   
   
   /* figure out, what needs to be recorded, and what needs to be send */
   if (lGetNumberOfElem(orders->pendingOrderList) == 0 ) {
      if (lGetNumberOfElem(orders->jobStartOrderList) == 0) {
         config_mode = SGE_GDI_SEND;
      }
      else {
         start_mode = SGE_GDI_SEND;   
      }
   }
  
    /* send orders */
   if (lGetNumberOfElem(orders->configOrderList) > 0) {
      orders->numberSendOrders += lGetNumberOfElem(orders->configOrderList);
#ifdef TEST_GDI2
      ctx->gdi_multi_sync(ctx, &alp, config_mode, SGE_ORDER_LIST, SGE_GDI_ADD,
                         &orders->configOrderList, NULL, NULL, &malp, &state, false, false);
#else
      sge_gdi_multi_sync(&alp, config_mode, SGE_ORDER_LIST, SGE_GDI_ADD,
                         &orders->configOrderList, NULL, NULL, &malp, &state, false, false);
#endif                         
      if (config_mode == SGE_GDI_SEND) {
         orders->numberSendPackages++;     
      }
   }        
   
   if (lGetNumberOfElem(orders->jobStartOrderList) > 0) {
      orders->numberSendOrders += lGetNumberOfElem(orders->jobStartOrderList);
#ifdef TEST_GDI2      
      ctx->gdi_multi_sync(ctx, &alp, start_mode, SGE_ORDER_LIST, SGE_GDI_ADD,
                         &orders->jobStartOrderList, NULL, NULL, &malp, &state, false, false);
#else
      sge_gdi_multi_sync(&alp, start_mode, SGE_ORDER_LIST, SGE_GDI_ADD,
                         &orders->jobStartOrderList, NULL, NULL, &malp, &state, false, false);
#endif                         
      if (start_mode == SGE_GDI_SEND) {
         orders->numberSendPackages++;     
      }
   }

   if (lGetNumberOfElem(orders->pendingOrderList) > 0) {
      orders->numberSendOrders += lGetNumberOfElem(orders->pendingOrderList);
#ifdef TEST_GDI2      
      ctx->gdi_multi_sync(ctx, &alp, SGE_GDI_SEND, SGE_ORDER_LIST, SGE_GDI_ADD,
                         &orders->pendingOrderList, NULL, NULL, &malp, &state, false, false);
#else
      sge_gdi_multi_sync(&alp, SGE_GDI_SEND, SGE_ORDER_LIST, SGE_GDI_ADD,
                         &orders->pendingOrderList, NULL, NULL, &malp, &state, false, false);
#endif                         
      orders->numberSendPackages++;                      
   }

   lFreeList(&malp);
   lFreeList(&alp);

   DEXIT;
   return true;
}

