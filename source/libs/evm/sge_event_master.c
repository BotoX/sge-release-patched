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

#include "sge_event_master.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "sge_event_master.h"
#include "sge.h"
#include "cull.h"
#include "sge_feature.h"
#include "sge_time.h"
#include "sge_host.h"
#include "sge_event.h"
#include "sge_all_listsL.h"
#include "sge_prog.h"
#include "sgermon.h"
#include "sge_log.h"
#include "sge_conf.h"
#include "sge_answer.h"
#include "sge_qinstance.h"
#include "sge_report.h"
#include "sge_ckpt.h"
#include "sge_pe.h"
#include "sge_userprj.h"
#include "sge_job.h"
#include "sge_hostname.h"
#include "sge_userset.h"
#include "sge_manop.h"
#include "sge_calendar.h"
#include "sge_sharetree.h"
#include "sge_hgroup.h"
#include "sge_cuser.h"
#include "sge_centry.h"
#include "sge_cqueue.h"
#include "sge_object.h"
#include "sge_mtutil.h"
#include "setup_qmaster.h"
#include "configuration_qmaster.h"
#include "cl_errors.h"
#include "cl_commlib.h"
#include "sge_profiling.h"
#include "msg_common.h"
#include "msg_sgeobjlib.h"
#include "msg_evmlib.h"

#include "sge_lock.h"

/****** transaction handling implementation ************
 *
 * Well, one cannot realy the transaction implementation
 * transaction handling. First of all, there is no role
 * back. Second, there is only one transaction in the 
 * whole system, one no way to distinguish between the 
 * events added by the thread, which opend the transaction, 
 * and other threads just adding events.
 *
 * We need the current implementation for the scheduler
 * protocol. All gdi requets have to be atomic from the
 * scheduler point of view. Therefore we have a second
 * (the transaction) list to add events, and put them
 * into the send queue, when the gdi request has been
 * handled.
 * 
 * Important variables:
 *  pthread_mutex_t  transaction_mutex;  
 *  lList            *transaction_events;
 *  pthread_mutex_t  t_add_event_mutex;
 *  bool             is_transaction;   
 *
 * related methods:
 *  sge_set_commit_required()
 *  sge_is_commit_required()
 *  sge_commit() 
 *
 *******************************************************/


/****** subscription_t definition **********************
 *
 * This is a subscription entry in a two dimensional 
 * definition. The first dimension is for the event
 * clients and changes when the event client subscription
 * changes. The second dimension is of fixed size and 
 * contains one element for each posible event.
 *
 *******************************************************/
typedef struct {
      bool         subscription; /* true -> the event is subscribed           */
      bool         blocked;      /* true -> no events will be accepted before */
                                 /*   the total update is issued                */
      bool         flush;        /* true -> flush is set                      */
      u_long32     flush_time;   /* seconds how much the event can be delayed */
      lCondition   *where;       /* where filter                              */
      lDescr       *descr;       /* target list descriptor                    */
      lEnumeration *what;        /* limits the target element                 */
} subscription_t;


/****** event_master_control_t definition ********************
 *
 * This struct contains all the control information needed
 * to have the event master running. It contains the references
 * to all lists, mutexes, and booleans.
 *
 ************************************************************/
typedef struct {
   pthread_mutex_t  transaction_mutex;    /* a mutix ensuring that only one transaction is running at a time */
   lList            *transaction_events;  /* a list storing all events happening, while a transaction is open, a
                                             transaction is not thread specific*/
   pthread_mutex_t  t_add_event_mutex;    /* garding the transaction_events list and the boolean is_transaction */
   bool             is_transaction;       /* identifies, if a transaction is open, or not */
   pthread_mutex_t  mutex;                 /* used for mutual exclusion. only use in public functions   */
   pthread_cond_t   lockfield_cond_var;    /* used for waiting                                          */
   pthread_cond_t   waitfield_cond_var;    /* used for waiting                                          */
   pthread_cond_t   cond_var;              /* used for waiting                                          */
   pthread_mutex_t  cond_mutex;            /* used for mutual exclusion. only use in internal functions */
   pthread_mutex_t  ack_mutex;             /* gards the ack list                                        */
   pthread_mutex_t  send_mutex;            /* gards the event send list                                 */
   
   u_long32         max_event_clients;     /* contains the max number of custom event clients, the      */
                                           /* scheduler is not accounted for. protected by mutex.       */
   u_long32         len_event_clients;     /* contains the length of the custom event clients array     */
                                           /* protected by mutex.                                       */
   
   bool             delivery_signaled;     /* signals that an event delivery has been signaled          */
                                           /* protected by cond_mutex.                                  */
/*   bool             is_flush;  */            /* ensures that events are delivered right away. protected   */     
                                           /* cond_mutex.                                               */
   
   bool             is_prepare_shutdown;   /* is set, when the qmaster is going down. Do not accept     */
                                           /* new event clients, when this is set to false. protected   */
                                           /* by mutex.                                                 */
   bool             is_exit;               /* true -> exit event master, and the thread should end      */
                                           /* protected by mutex                                        */
   bool             lock_all;              /* true -> all event client locks are occupied.              */
                                           /* protected by mutex                                        */
   lList*           clients;               /* list of event master clients                              */     
                                           /* protected by mutex                                        */
   lList*           ack_events;            /* list of events to ack                                     */     
                                           /* protected by ack_mutex                                    */
   lList*           send_events;           /* list of events to send                                    */     
                                           /* protected by send_mutex                                   */
   /* A client locked via the lockfield is only locked for content.  Any changes
    * to the clients list, such as adding and removing clients, requires the
    * locking of the mutex. */
   bitfield         *lockfield;            /* bitfield of locks for event clients                       */     
                                           /* protected by mutex                                        */
   /* In order to allow the locking of individual clients, I have added an array
    * of pointers to the client entries in the clients list.  This was needed
    * because otherwise looping through the registered clients requires the
    * locking of the entire list.  (Without the array, the id, which is the key
    * for the lockfield, is part of the content of the client, which has to be
    * locked to be accessed, which is what we're trying to double in the first
    * place!) */
   /* For simplicity's sake, rather than completely replacing the clients list
    * with an array, I have made the array an overlay for the list.  This allows
    * all the old code to function while permitting new code to use the array
    * for faster access.  Specifically, the lSelect in add_list_event_direct()
    * continues to work without modification. */
   lListElem**      clients_array;         /* array of pointers to event clients                        */
                                           /* protected by lockfield/mutex                              */
   /* In order better optimize for the normal case, i.e. few event clients, I
    * have added this indices array.  It contains the indices for the non-NULL
    * entries in the clients_array.  It gets rebuilt by the event thread
    * whenever an event client is added or removed.  Now, instead of having to
    * search through 109 (99 dynamic + 10 static) entries to find the 2 or 3
    * that aren't NULL, *every* time through the loop, the event thread only has
    * to search for the non-NULL entries when something changes. */
   int*             clients_indices;       /* array of currently active entries in the clients_array    */
                                           /* unprotected -- only used by event thread                  */
   bool             indices_dirty;         /* whether the clients_indices array needs to be updated     */
                                           /* protected by mutex                                        */
} event_master_control_t;


/*
 * contains the number of event clients that have subscribt a specific event
 */
typedef struct {
   pthread_mutex_t  subscribed_events_mutex;
   int   subscribed_events[sgeE_EVENTSIZE];
}subscribed_control_t;

/****** Eventclient/Server/-Event_Client_Server_Defines ************************
*  NAME
*     Defines -- Constants used in the module
*
*  SYNOPSIS
*     #define EVENT_DELIVERY_INTERVAL_S 1 
*     #define EVENT_DELIVERY_INTERVAL_N 0 
*     #define EVENT_ACK_MIN_TIMEOUT 600
*     #define EVENT_ACK_MAX_TIMEOUT 1200
*
*  FUNCTION
*     EVENT_DELIVERY_INTERVAL_S is the event delivery interval. It is set in seconds.
*     EVENT_DELIVERY_INTERVAL_N same thing but in nano seconds.
*
*     EVENT_ACK_MIN/MAX_TIMEOUT is the minimum/maximum timeout value for an event
*     client sending the acknowledge for the delivery of events.
*     The real timeout value depends on the event delivery interval for the 
*     event client (10 * event delivery interval).
*
*
*******************************************************************************/
#define EVENT_DELIVERY_INTERVAL_S 1
#define EVENT_DELIVERY_INTERVAL_N 0
#define EVENT_ACK_MIN_TIMEOUT 600
#define EVENT_ACK_MAX_TIMEOUT 1200

/********************************************************************
 *
 * The next three array are important for lists, which can be
 * subscripted by the event client and which contain a sub-list
 * that can be subscripted by it self again (such as the: job list
 * with the JAT_Task sub list, or the cluster queue list with the
 * queue instances sub-list)
 * All lists, which follow the same structure have to be defined
 * in the special construct.
 *
 * EVENT_LIST:
 *  Contains all events for the main list, which delivers also
 *  the sub-list. 
 *
 * FIELD_LIST:
 *  Contains all attributes in the main list, which contain the
 *  sub-list in question.
 *
 * SOURCE_LIST:
 *  Contains the sub-scription events for the sub-list, which als
 *  contains the filter for the sub-list.
 *
 *
 * This construct and its functions are limited to one sub-scribable
 * sub-list per main list. If multiple sub-lists can be subsribed, the
 * construct has to be exetended.
 *
 *
 * SEE ALSO:
 *     evm/sge_event_master/list_select()
 *     evm/sge_event_master/elem_select() 
 *  and
 *     evm/sge_event_master/add_list_event
 *     evm/sge_event_master/add_event
 *
 *********************************************************************/
#define LIST_MAX 3

const int EVENT_LIST[LIST_MAX][6] = {
   {sgeE_JOB_LIST, sgeE_JOB_ADD, sgeE_JOB_DEL, sgeE_JOB_MOD, sgeE_JOB_MOD_SCHED_PRIORITY, -1},
   {sgeE_CQUEUE_LIST, sgeE_CQUEUE_ADD, sgeE_CQUEUE_DEL, sgeE_CQUEUE_MOD, -1, -1},
   {sgeE_JATASK_ADD, sgeE_JATASK_DEL, sgeE_JATASK_MOD, -1, -1, -1 }
};

const int FIELD_LIST[LIST_MAX][3] = {
   {JB_ja_tasks, JB_ja_template, -1},
   {CQ_qinstances, -1, -1},
   {JAT_task_list, -1, -1}
};

const int SOURCE_LIST[LIST_MAX][3] = {
   {sgeE_JATASK_MOD, sgeE_JATASK_ADD, -1},
   {sgeE_QINSTANCE_ADD, sgeE_QINSTANCE_MOD, -1},
   {sgeE_PETASK_ADD, -1, -1}
};

/******************************************************
 *
 * The next two array are needed for blocking events
 * for a given client, when a total update is pending
 * for it. 
 *
 * The total_udpate_events array defines all list events,
 * which are used during a total update.
 *
 * The block_events contain all events, which aer blocked
 * during a total update.
 *
 *  SEE ALSO:
 *   blockEvents
 *   total_update 
 *   add_list_event_direct
 *
 ******************************************************/

#ifndef __SGE_NO_USERMAPPING__
#define total_update_eventsMAX 20
#else
#define total_update_eventsMAX 19                                    
#endif   

   const int total_update_events[total_update_eventsMAX+1] = { sgeE_ADMINHOST_LIST,
                                       sgeE_CALENDAR_LIST,
                                       sgeE_CKPT_LIST,
                                       sgeE_CENTRY_LIST,
                                       sgeE_CONFIG_LIST,
                                       sgeE_EXECHOST_LIST,
                                       sgeE_JOB_LIST,
                                       sgeE_JOB_SCHEDD_INFO_LIST,
                                       sgeE_MANAGER_LIST,
                                       sgeE_OPERATOR_LIST,
                                       sgeE_PE_LIST,
                                       sgeE_CQUEUE_LIST,
                                       sgeE_SCHED_CONF,
                                       sgeE_SUBMITHOST_LIST,
                                       sgeE_USERSET_LIST,
                                       sgeE_NEW_SHARETREE,
                                       sgeE_PROJECT_LIST,
                                       sgeE_USER_LIST,
                                       sgeE_HGROUP_LIST,

#ifndef __SGE_NO_USERMAPPING__
                                       sgeE_CUSER_LIST,
#endif
                                       -1};

   const int block_events[total_update_eventsMAX][9] = { {sgeE_ADMINHOST_ADD, sgeE_ADMINHOST_DEL, sgeE_ADMINHOST_MOD, -1, -1, -1, -1, -1, -1}, 
                                      {sgeE_CALENDAR_ADD,  sgeE_CALENDAR_DEL,  sgeE_CALENDAR_MOD,  -1, -1, -1, -1, -1, -1},
                                      {sgeE_CKPT_ADD,      sgeE_CKPT_DEL,      sgeE_CKPT_MOD,      -1, -1, -1, -1, -1, -1},
                                      {sgeE_CENTRY_ADD,    sgeE_CENTRY_DEL,    sgeE_CENTRY_MOD,    -1, -1, -1, -1, -1, -1}, 
                                      {sgeE_CONFIG_ADD,    sgeE_CONFIG_DEL,    sgeE_CONFIG_MOD,    -1, -1, -1, -1, -1, -1}, 
                                      {sgeE_EXECHOST_ADD,  sgeE_EXECHOST_DEL,  sgeE_EXECHOST_MOD,  -1, -1, -1, -1, -1, -1},
                                      {sgeE_JOB_ADD, sgeE_JOB_DEL, sgeE_JOB_MOD, sgeE_JOB_MOD_SCHED_PRIORITY, sgeE_JOB_USAGE, sgeE_JOB_FINAL_USAGE, sgeE_JOB_FINISH, -1, -1}, 
                                      {sgeE_JOB_SCHEDD_INFO_ADD, sgeE_JOB_SCHEDD_INFO_DEL, sgeE_JOB_SCHEDD_INFO_MOD, -1, -1, -1, -1, -1, -1},
                                      {sgeE_MANAGER_ADD,         sgeE_MANAGER_DEL,         sgeE_MANAGER_MOD,         -1, -1, -1, -1, -1, -1}, 
                                      {sgeE_OPERATOR_ADD,        sgeE_OPERATOR_DEL,        sgeE_OPERATOR_MOD,        -1, -1, -1, -1, -1, -1},
                                      {sgeE_PE_ADD,              sgeE_PE_DEL,              sgeE_PE_MOD,              -1, -1, -1, -1, -1, -1},
                                      {sgeE_CQUEUE_ADD,          sgeE_CQUEUE_DEL,          sgeE_CQUEUE_MOD, sgeE_QINSTANCE_ADD, sgeE_QINSTANCE_DEL, sgeE_QINSTANCE_MOD, sgeE_QINSTANCE_SOS, sgeE_QINSTANCE_USOS, -1}, 
                                      {-1, -1, -1, -1, -1, -1, -1, -1},
                                      {sgeE_SUBMITHOST_ADD,      sgeE_SUBMITHOST_DEL,      sgeE_SUBMITHOST_MOD,      -1, -1, -1, -1, -1, -1},
                                      {sgeE_USERSET_ADD,         sgeE_USERSET_DEL,         sgeE_USERSET_MOD,         -1, -1, -1, -1, -1, -1},
                                      {-1, -1, -1, -1, -1, -1, -1, -1}, 
                                      {sgeE_PROJECT_ADD,         sgeE_PROJECT_DEL,         sgeE_PROJECT_MOD,         -1, -1, -1, -1, -1, -1},
                                      {sgeE_USER_ADD,            sgeE_USER_DEL,            sgeE_USER_MOD,            -1, -1, -1, -1, -1, -1},
                                      {sgeE_HGROUP_ADD,          sgeE_HGROUP_DEL,          sgeE_HGROUP_MOD,          -1, -1, -1, -1, -1, -1}

#ifndef __SGE_NO_USERMAPPING__
                                     ,{sgeE_CUSER_ADD,           sgeE_CUSER_DEL,           sgeE_CUSER_MOD,           -1, -1, -1, -1, -1, -1}
#endif
                                 };


/********************************************************************
 *
 * Some events have to be delivered even so they have no date left
 * after filtering for them. These are for example all update list
 * events. 
 * The ensure, that is is done as fast as posible, we define an 
 * array of the size of the number of events, we have a init function
 * which sets the events which will be updated. To add a new event
 * ones has only to update that function.
 * Events which do not contain any data are not affected. They are
 * allways delivered.
 *
 * SEE ALSO:
 * - Array:
 *    SEND_EVENTS
 *
 * - Init function:
 *    evm/sge_event_master/sge_init_send_events()
 *
 *******************************************************************/
static bool SEND_EVENTS[sgeE_EVENTSIZE]; 

static subscribed_control_t Subscribed_Control;

static event_master_control_t Master_Control = {PTHREAD_MUTEX_INITIALIZER,
                                                NULL,
                                                PTHREAD_MUTEX_INITIALIZER,
                                                false,
                                                PTHREAD_MUTEX_INITIALIZER,
                                                PTHREAD_COND_INITIALIZER, 
                                                PTHREAD_COND_INITIALIZER, 
                                                PTHREAD_COND_INITIALIZER, 
                                                PTHREAD_MUTEX_INITIALIZER,
                                                PTHREAD_MUTEX_INITIALIZER,
                                                PTHREAD_MUTEX_INITIALIZER, 0,
                                                0, false, false, /*false,*/ false,
                                                false, NULL, NULL, NULL, NULL,
                                                NULL, NULL, false};
static pthread_once_t         Event_Master_Once = PTHREAD_ONCE_INIT;
static pthread_t              Event_Thread;

static void       event_master_once_init(void);
static void       init_send_events(void); 
static void*      event_deliver_thread(void*);
static bool       should_exit(void);
static int        get_number_of_subscriptions(u_long32 event_type);
static void       send_events(lListElem *report, lList *report_list);
static void       flush_events(lListElem*, int);
static void       total_update(lListElem*);
static void       build_subscription(lListElem*);
static void       remove_event_client(lListElem *client, int aClientID);
static void       check_send_new_subscribed_list(const subscription_t*, const subscription_t*, lListElem*, ev_event);
static int        eventclient_subscribed(const lListElem *, ev_event, const char*);
static int        purge_event_list(lList* aList, ev_event anEvent); 
static bool       add_list_event_for_client(u_long32, u_long32, ev_event, u_long32, u_long32, const char*, const char*, const char*, lList*, bool);
static void       add_list_event_direct(lListElem *event_client,
                                        lListElem *event, bool copy_event);
static void       total_update_event(lListElem*, ev_event);
static bool       list_select(subscription_t*, int, lList**, lList*, const lCondition*, const lEnumeration*, const lDescr*);
static lListElem* elem_select(subscription_t*, lListElem*, const int[], const lCondition*, const lEnumeration*, const lDescr*, int);    
static lListElem* eventclient_list_locate_by_adress(const char*, const char*, u_long32);
static const lDescr* getDescriptorL(subscription_t*, const lList*, int);
static void       lock_all_clients(void);
static void       unlock_all_clients(void);
static bool       lock_client(u_long32 id, bool wait);
static void       lock_client_protected(u_long32 id);
static void       unlock_client(u_long32 id);
static lListElem* get_event_client(u_long32 id);
static void       set_event_client(u_long32 id, lListElem *client);
static u_long32   assign_new_dynamic_id (void);
static void       process_acks(void);
static void       process_sends(void);
static void       set_flush (void);
#if 0
static bool       should_flush_event_client (u_long32 id, ev_event type,
                                             bool has_lock);
#endif

static void blockEvents(lListElem *event_client, ev_event ev_type, bool isBlock);

/****** Eventclient/Server/sge_add_event_client() ******************************
*  NAME
*     sge_add_event_client() -- register a new event client
*
*  SYNOPSIS
*     #include "sge_event_master.h"
*
*     int 
*     sge_add_event_client(lListElem *clio, lList **alpp, lList **eclpp, 
*     char *ruser, char *rhost) 
*
*  FUNCTION
*     Registeres a new event client. 
*     If it requested a dynamic id, a new id is created and assigned.
*     If it is a special client (with fixed id) and an event client
*     with this id already exists, the old instance is deleted and the
*     new one registered.
*     If the registration succees, the event client is sent all data
*     (sgeE*_LIST events) according to its subscription.
*
*  INPUTS
*     lListElem *clio - the event client object used as registration data
*     lList **alpp    - answer list pointer for answer to event client
*     lList **eclpp   - list pointer to return new event client object
*     char *ruser     - user that tries to register an event client
*     char *rhost     - host on which the event client runs
*
*  RESULT
*     int - AN_status value. STATUS_OK on success, else error code
*
*  NOTES
*     MT-NOTE: sge_add_event_client() is NOT MT safe.
*
*******************************************************************************/
int sge_add_event_client(lListElem *clio, lList **alpp, lList **eclpp, char *ruser, char *rhost)
{
   lListElem *ep=NULL;
   u_long32 now;
   u_long32 id;
   u_long32 ed_time;
   const char *name;
   lList *subscription;
   const char *host;
   const char *commproc;
   u_long32 commproc_id;

   DENTER(TOP_LAYER,"sge_add_event_client");

   pthread_once(&Event_Master_Once, event_master_once_init);

   id = lGetUlong(clio, EV_id);
   name = lGetString(clio, EV_name);
   subscription = lGetList(clio, EV_subscribed);
   ed_time = lGetUlong(clio, EV_d_time);
   host = lGetHost(clio, EV_host);
   commproc = lGetString(clio, EV_commproc);
   commproc_id = lGetUlong(clio, EV_commid);

   if (name == NULL) {
      name = "unnamed";
      lSetString(clio, EV_name, name);
   }
   
   /* EV_ID_ANY is 0, therefor the compare is always true (Irix complained) */
   if (/*(id < EV_ID_ANY) ||*/ (id >= EV_ID_FIRST_DYNAMIC)) { /* invalid request */
      ERROR((SGE_EVENT, MSG_EVE_ILLEGALIDREGISTERED_U, u32c(id)));
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);

      DEXIT;
      return STATUS_ESEMANTIC;
   }

   if (subscription == NULL) {
      ERROR((SGE_EVENT, MSG_EVE_INVALIDSUBSCRIPTION));
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);

      DEXIT;
      return STATUS_ESEMANTIC;
   }

   /* This also aquires the mutex. */
   /* I have to lock all here because I have to search for the client by
    * address, which means I need to have control over all the clients. */
   lock_all_clients ();

   if (Master_Control.is_prepare_shutdown) {
      unlock_all_clients ();
      ERROR((SGE_EVENT, MSG_EVE_QMASTERISGOINGDOWN));
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);      
      DEXIT;
      return STATUS_ESEMANTIC;
   }
   
   if (id == EV_ID_ANY) {   /* qmaster shall give id dynamically */
      /* Try to find an event client with the very same commd 
         address triplet. If it exists the "new" event client must 
         be the result of a reconnect after a timeout that happend at 
         client side. We delete the old event client. */
      ep = eventclient_list_locate_by_adress(host, commproc, commproc_id);

      if (ep != NULL) {
         ERROR((SGE_EVENT, MSG_EVE_CLIENTREREGISTERED_SSSU, name, host, 
                commproc, u32c(commproc_id)));

         /* Reuse the old EV_id for simplicity's sake */
         id = lGetUlong (ep, EV_id);
         DPRINTF (("Reusing old id: %d\n", id));
      
         /* delete old event client entry */
         set_event_client (id, NULL);         
         lRemoveElem(Master_Control.clients, ep);
      }
      /* Otherwise, pick the first free id from the clients array. */
      else {
         id = assign_new_dynamic_id ();
         
         if (id == 0) {
            unlock_all_clients ();
            ERROR((SGE_EVENT, MSG_TO_MANY_DYNAMIC_EC_U, u32c( Master_Control.max_event_clients)));
            answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);

            DEXIT;
            return STATUS_ESEMANTIC;
         } 

         Master_Control.indices_dirty = true;
         INFO((SGE_EVENT, MSG_EVE_REG_SUU, name, u32c(id), u32c(ed_time)));         
      }

      /* Set the new id for this client. */
      lSetUlong(clio, EV_id, id);
   }
   
   /* special event clients: we allow only one instance */
   /* if it already exists, delete the old one and register the new one */
   if (id > EV_ID_ANY && id < EV_ID_FIRST_DYNAMIC) {
      if (!manop_is_manager(ruser)) {
         unlock_all_clients ();
         ERROR((SGE_EVENT, MSG_WRONG_USER_FORFIXEDID ));
         answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);

         DEXIT;
         return STATUS_ESEMANTIC;
      }

      if ((ep = get_event_client (id)) != NULL) {
         /* we already have this special client */
         ERROR((SGE_EVENT, MSG_EVE_CLIENTREREGISTERED_SSSU, name, host, 
                commproc, u32c(commproc_id)));         

         /* delete old event client entry */
         set_event_client (id, NULL);
         lRemoveElem(Master_Control.clients, ep);
      } else {
         INFO((SGE_EVENT, MSG_EVE_REG_SUU, name, u32c(id), u32c(ed_time)));
         Master_Control.indices_dirty = true;
      }   
   }

   ep=lCopyElem(clio);
   lSetBool(clio, EV_changed, false);

   lAppendElem(Master_Control.clients, ep);
   set_event_client (id, ep);
   
   /* Release the locks on all clients except the one on which we're working. */
   /* This works because lock_all uses a special bit to indicate a global lock,
    * rather than setting the lock for every client. */
   lock_client_protected (id);
   unlock_all_clients ();
   
   lSetUlong(ep, EV_next_number, 1);

   /* register this contact */
   now = sge_get_gmt();
   lSetUlong(ep, EV_last_send_time, 0);
   lSetUlong(ep, EV_next_send_time, now + lGetUlong(ep, EV_d_time));
   lSetUlong(ep, EV_last_heard_from, now);
   lSetUlong(ep, EV_state, EV_connected);

   /* return new event client object to event client */
   if (eclpp != NULL) {
      lListElem *ret_el = lCopyElem(ep);
      if (*eclpp == NULL) {
         *eclpp = lCreateListHash("new event client", EV_Type, true);
      }
      lSetBool(ret_el, EV_changed, false);
      lAppendElem(*eclpp, ret_el);
   }

   /* Start with no pending events. */
   
   build_subscription(ep);

   /* build events for total update */
   total_update(ep);

   /* flush initial list events */
   flush_events(ep, 0);

   unlock_client (id);
   
   INFO((SGE_EVENT, MSG_SGETEXT_ADDEDTOLIST_SSSS,
         ruser, rhost, name, MSG_EVE_EVENTCLIENT));
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

   DEXIT; 
   return STATUS_OK;
} /* sge_add_event_client() */

/****** Eventclient/Server/sge_mod_event_client() ******************************
*  NAME
*     sge_mod_event_client() -- modify event client
*
*  SYNOPSIS
*     #include "sge_event_master.h"
*
*     int 
*     sge_mod_event_client(lListElem *clio, lList **alpp, lList **eclpp, 
*     char *ruser, char *rhost) 
*
*  FUNCTION
*     An event client object is modified.
*     It is possible to modify the event delivery time and
*     the subscription.
*     If the subscription is changed, and new sgeE*_LIST events are subscribed,
*     these lists are sent to the event client.
*
*  INPUTS
*     lListElem *clio - object containing the data to change
*     lList **alpp    - answer list pointer
*     lList **eclpp   - list pointer to return changed object
*     char *ruser     - user that triggered the modify action
*     char *rhost     - host that triggered the modify action
*
*  RESULT
*     int - AN_status code. STATUS_OK on success, else error code
*
*  NOTES
*     MT-NOTE: sge_mod_event_client() is NOT MT safe.
*
*******************************************************************************/
int sge_mod_event_client(lListElem *clio, lList **alpp, lList **eclpp, char *ruser, char *rhost) 
{
   lListElem *event_client=NULL;
   u_long32 id;
   u_long32 busy;
   u_long32 ev_d_time;

   DENTER(TOP_LAYER,"sge_mod_event_client");

   pthread_once(&Event_Master_Once, event_master_once_init);

   /* try to find event_client */
   id = lGetUlong(clio, EV_id);
   
   lock_client (id, true);

   event_client = get_event_client (id);

   if (event_client == NULL) {
      unlock_client (id);

      ERROR((SGE_EVENT, MSG_EVE_UNKNOWNEVCLIENT_US, u32c(id), "modify"));
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);

      DEXIT;
      return STATUS_EEXIST;
   }

   /* these parameters can be changed */
   busy         = lGetUlong(clio, EV_busy);
   ev_d_time    = lGetUlong(clio, EV_d_time);

   /* check for validity */
   if (ev_d_time < 1) {
      unlock_client (id);

      ERROR((SGE_EVENT, MSG_EVE_INVALIDINTERVAL_U, u32c(ev_d_time)));
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);

      DEXIT;
      return STATUS_ESEMANTIC;
   }

   if (lGetList(clio, EV_subscribed) == NULL) {
      unlock_client (id);

      ERROR((SGE_EVENT, MSG_EVE_INVALIDSUBSCRIPTION));
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);

      DEXIT;
      return STATUS_ESEMANTIC;
   }

   /* event delivery interval changed. 
    * We have to update the next delivery time to 
    * next_delivery_time - old_interval + new_interval 
    */
   if (ev_d_time != lGetUlong(event_client, EV_d_time)) {
      lSetUlong(event_client, EV_next_send_time, 
                lGetUlong(event_client, EV_next_send_time) - 
                lGetUlong(event_client, EV_d_time) + ev_d_time);
      lSetUlong(event_client, EV_d_time, ev_d_time);
   }

   /* subscription changed */
   /*old_subscription = lGetString(event_client, EV_subscription);*/
   if (lGetBool(clio, EV_changed)) {
      subscription_t *new_sub = NULL; 
      subscription_t *old_sub = NULL; 

      build_subscription(clio);
      new_sub = lGetRef(clio, EV_sub_array);
      old_sub = lGetRef(event_client, EV_sub_array);
 
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_ADMINHOST_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_CALENDAR_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_CKPT_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_CENTRY_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_CONFIG_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_EXECHOST_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_JOB_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_JOB_SCHEDD_INFO_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_MANAGER_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_OPERATOR_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_NEW_SHARETREE);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_PE_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_PROJECT_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_CQUEUE_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_SUBMITHOST_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_USER_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_USERSET_LIST);
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_HGROUP_LIST);

#ifndef __SGE_NO_USERMAPPING__
      check_send_new_subscribed_list(old_sub, new_sub, event_client, sgeE_CUSER_LIST);
#endif      

      lSetList(event_client, EV_subscribed, lCopyList("", lGetList(clio, EV_subscribed)));
      lSetRef(event_client, EV_sub_array, new_sub);
      lSetRef(clio, EV_sub_array, NULL);
      if (old_sub){
         int i;
         for (i=0; i<sgeE_EVENTSIZE; i++){ 
            if (old_sub[i].where)
               lFreeWhere(old_sub[i].where);
            if (old_sub[i].what)
               lFreeWhat(old_sub[i].what);
            if (old_sub[i].descr){
               cull_hash_free_descr(old_sub[i].descr);
               free(old_sub[i].descr);
            }
         } 
         FREE(old_sub);

      }
   }

   /* busy state changed */
   if (busy != lGetUlong(event_client, EV_busy)) {
      lSetUlong(event_client, EV_busy, busy);
   }

   lSetUlong(event_client, EV_state, EV_connected);

   set_flush();

   DEBUG((SGE_EVENT, MSG_SGETEXT_MODIFIEDINLIST_SSSS,
         ruser, rhost, lGetString(event_client, EV_name), MSG_EVE_EVENTCLIENT));
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

   /* return modified event client object to event client */
   if (eclpp != NULL) {
      if (*eclpp == NULL) {
         *eclpp = lCreateListHash("modified event client", EV_Type, true);
      }

      lAppendElem(*eclpp, lCopyElem(event_client));
   }

   unlock_client (id);

   DEXIT; 
   return STATUS_OK;
} /* sge_mod_event_client() */

/****** evm/sge_event_master/sge_remove_event_client() *************************
*  NAME
*     sge_remove_event_client() -- remove event client 
*
*  SYNOPSIS
*     void sge_remove_event_client(u_long32 aClientID) 
*
*  FUNCTION
*     Remove event client. Fetch event client from event client list. Purge
*     event subscription array. Remove event client from event client list.
*
*  INPUTS
*     u_long32 aClientID - event client id 
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: sge_remove_event_client() is NOT MT safe. 
*
*******************************************************************************/
void sge_remove_event_client(u_long32 aClientID) {
   lListElem *client;

   DENTER(TOP_LAYER, "sge_remove_event_client");

   pthread_once(&Event_Master_Once, event_master_once_init);

   lock_client (aClientID, true);
   client = get_event_client (aClientID);
   
   /* If we didn't find the client in the array, try the list as a backup.  The
    * result should always be the same, but we do it anyway, just in case. */
   if (client == NULL) {
      /* Only lock in the case where we need to search the list. */
      /* Since we already own the lock for this client, all we need is for the
       * state of the list to hold still long enough for us to find the client.
       * Thus it is OK that we just lock the mutex instead of locking all
       * clients. */
      sge_mutex_lock("event_master_mutex", SGE_FUNC, __LINE__,
                     &Master_Control.mutex);
      
      client = lGetElemUlong (Master_Control.clients, EV_id, aClientID);
      
      /* Correct the problem. */
      Master_Control.clients_array[aClientID] = client;

      sge_mutex_unlock("event_master_mutex", SGE_FUNC, __LINE__,
                       &Master_Control.mutex);
      
      ERROR ((SGE_EVENT, MSG_ARRAY_OUT_OF_SYNC_U, u32c(aClientID)));
   }
   
   if (client == NULL) {
      ERROR((SGE_EVENT, MSG_EVE_UNKNOWNEVCLIENT_US, u32c(aClientID), "remove"));
      unlock_client (aClientID);
      
      return;
   }
   lSetUlong(client, EV_state, EV_terminated);

   /* We have to remove the entry from the list before we release the client
    * lock.  Otherwise there will be a gap where the array will have a NULL
    * entry and the list will not. */
   unlock_client (aClientID);

   DEXIT;
   return;
} /* sge_remove_event_client() */


/****** sge_event_master/sge_set_max_dynamic_event_clients() *******************
*  NAME
*     sge_set_max_dynamic_event_clients() -- set max number of dyn. event clients
*
*  SYNOPSIS
*     void sge_set_max_dynamic_event_clients(u_long32 max) 
*
*  FUNCTION
*     Sets max number of dynamic event clients. If the new value is larger than
*     the maximum number of used file descriptors for communication this value
*     is set to the max. number of file descriptors minus some reserved file
*     descriptors. (10 for static event clients, 9 for execd, 10 for file 
*     descriptors used by application (to write files, etc.) ).
*
*     At least one dynamic event client is allowed.
*
*  INPUTS
*     u_long32 max - number of dynamic event clients
*
*  NOTES
*     MT-NOTE: sge_set_max_dynamic_event_clients() is MT safe 
*
*******************************************************************************/
u_long32 sge_set_max_dynamic_event_clients(u_long32 new_value){
   u_long32 max = new_value;
   u_long32 max_allowed_value = 0;
   cl_com_handle_t* handle = NULL;

   DENTER(TOP_LAYER, "sge_set_max_dynamic_event_clients");

   pthread_once(&Event_Master_Once, event_master_once_init);

   /* Don't need to lock all clients because we're not changing the clients. */
   sge_mutex_lock("event_master_mutex", SGE_FUNC, __LINE__, &Master_Control.mutex);

   /* Set the max event clients if it changed. */
   if (max != Master_Control.max_event_clients) {


      /* check max. file descriptors of qmaster communication handle */
      handle = cl_com_get_handle("qmaster",1);
      if (handle != NULL) {
         int max_file_handles = 0;
         cl_com_get_max_connections(handle,&max_file_handles);
         if ( max_file_handles >= 19 ) {
            max_allowed_value = max_file_handles - 19;
         } else {
            max_allowed_value = 1;
         }

         if ( max > max_allowed_value ) {
            max = max_allowed_value;
            WARNING((SGE_EVENT, MSG_CONF_NR_DYNAMIC_EVENT_CLIENT_EXCEEDS_MAX_FILEDESCR_U, u32c(max) ));
         }
      }

      /* If the max is bigger than our array, resize the array and lockfield. */
      if (max > Master_Control.len_event_clients - (EV_ID_FIRST_DYNAMIC - 1)) {
         lListElem **newp = NULL;
         int *newi = NULL;
         bitfield *newb = NULL;
         int old_length = 0;

         old_length = Master_Control.len_event_clients;
         Master_Control.len_event_clients = max + EV_ID_FIRST_DYNAMIC - 1;

         DPRINTF (("Enlarging client array from %d to %d\n", old_length,
                   Master_Control.len_event_clients));

         newp = (lListElem **)malloc (sizeof (lListElem *) *
                                      Master_Control.len_event_clients);

         /* Copy all entries from the old list to the new.  The new list is
          * guaranteed to be longer than the old list. */
         memcpy (newp, Master_Control.clients_array,
                 old_length * sizeof (lListElem **));

         /* Zero the rest of the entries. */
         memset (&newp[old_length], '\0',
                 (Master_Control.len_event_clients - old_length) *
                                                         sizeof (lListElem **));

         FREE (Master_Control.clients_array);
         Master_Control.clients_array = newp;

         /* No need to copy the indices.  We can just mark them as dirty, and the
          * event thread will rebuild them later. */
         newi = (int *)malloc (sizeof (int) *
                                        (Master_Control.len_event_clients + 1));
         memset (newi, '\0',
                 (Master_Control.len_event_clients + 1) * sizeof (int));
         FREE (Master_Control.clients_indices);
         Master_Control.clients_indices = newi;
         Master_Control.indices_dirty = true;

         newb = sge_bitfield_new (Master_Control.len_event_clients);
         sge_bitfield_bitwise_copy (Master_Control.lockfield, newb);
         sge_bitfield_free (Master_Control.lockfield);
         Master_Control.lockfield = newb;
      } /* if */

      /* If the new max is lower than the old max, then lowering the maximum 
       * prevents new event clients in the upper range, but allows the old ones
       * there to drain off naturally. */
      Master_Control.max_event_clients = max;
      INFO((SGE_EVENT, MSG_SET_MAXDYNEVENTCLIENT_U, u32c(max)));
   } /* if */
   sge_mutex_unlock("event_master_mutex", SGE_FUNC, __LINE__, &Master_Control.mutex);
   
   DEXIT;
   return max;
}


/****** sge_event_master/sge_get_max_dynamic_event_clients() *******************
*  NAME
*     sge_get_max_dynamic_event_clients() -- get max dynamic event clients nr
*
*  SYNOPSIS
*     u_long32 sge_get_max_dynamic_event_clients(u_long32 max) 
*
*  FUNCTION
*     Returns the actual value of max. dynamic event clients allowed
*
*  RESULT
*     u_long32 - max value
*
*  NOTES
*     MT-NOTE: sge_get_max_dynamic_event_clients() is MT save
*
*******************************************************************************/
u_long32 sge_get_max_dynamic_event_clients(void){
   u_long32 actual_value = 0;
   DENTER(TOP_LAYER, "sge_get_max_dynamic_event_clients");

   pthread_once(&Event_Master_Once, event_master_once_init);

   sge_mutex_lock("event_master_mutex", SGE_FUNC, __LINE__, &Master_Control.mutex);
   actual_value = Master_Control.max_event_clients;
   sge_mutex_unlock("event_master_mutex", SGE_FUNC, __LINE__, &Master_Control.mutex);

   DEXIT;
   return actual_value;
}


/****** evm/sge_event_master/sge_select_event_clients() ************************
*  NAME
*     sge_select_event_clients() -- select event clients 
*
*  SYNOPSIS
*     lList* sge_select_event_clients(const char *aNewList, const lCondition 
*     *aCond, const lEnumeration *anEnum) 
*
*  FUNCTION
*     Select event clients.  
*
*  INPUTS
*     const char *aNewList       - name of the result list returned. 
*     const lCondition *aCond    - where condition 
*     const lEnumeration *anEnum - what enumeration
*
*  RESULT
*     lList* - list with elements of type 'EV_Type'.
*
*  NOTES
*     MT-NOTE: sge_select_event_clients() is MT safe 
*     MT-NOTE:
*     MT-NOTE: The elements contained in the result list are copies of the
*     MT-NOTE: respective event client list elements.
*
*******************************************************************************/
lList* sge_select_event_clients(const char *aNewList, const lCondition *aCond, const lEnumeration *anEnum)
{
   lList *lst = NULL;

   DENTER(TOP_LAYER, "sge_select_event_clients");

   pthread_once(&Event_Master_Once, event_master_once_init);

   /* We have to lock everything because we need to access every client. */
   /* This also aquires the mutex. */
   lock_all_clients ();

   if (Master_Control.clients != NULL) {
      lst = lSelect(aNewList, (const lList*)Master_Control.clients, aCond, anEnum);
   }

   unlock_all_clients ();

   DEXIT;
   return lst;
} /* sge_select_event_clients() */

/****** evm/sge_event_master/sge_shutdown_event_client() ***********************
*  NAME
*     sge_shutdown_event_client() -- shutdown an event client 
*
*  SYNOPSIS
*     int sge_shutdown_event_client(u_long32 aClientID, const char* anUser, 
*     uid_t anUID) 
*
*  FUNCTION
*     Shutdown an event client. Send the event client denoted by 'aClientID' 
*     a shutdown event.
*
*     Shutting down an event client is only permitted if 'anUser' does have
*     manager privileges OR is the owner of event client 'aClientID'.
*
*  INPUTS
*     u_long32 aClientID - event client ID 
*     const char* anUser - user which did request this operation 
*     uid_t anUID        - user id of request user
*     lList **alpp       - answer list for info and errors
*
*  RESULT
*     EPERM - operation not permitted  
*     ESRCH - client with given client id is unknown
*     0     - otherwise
*
*  NOTES
*     MT-NOTE: sge_shutdown_event_client() is not MT safe. 
*
*******************************************************************************/
int sge_shutdown_event_client(u_long32 aClientID, const char* anUser,
                              uid_t anUID, lList **alpp)
{
   lListElem *client = NULL;
   int ret = 0;
   DENTER(TOP_LAYER, "sge_shutdown_event_client");

   pthread_once(&Event_Master_Once, event_master_once_init);

   if (aClientID <= EV_ID_ANY) {
      SGE_ADD_MSG_ID(sprintf(SGE_EVENT, 
                        MSG_EVE_UNKNOWNEVCLIENT_US, u32c(aClientID), "shutdown"));
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DEXIT;
      return EINVAL;
   }
   
   lock_client (aClientID, true);
   client = get_event_client (aClientID);

   if (client != NULL) {
      if (!manop_is_manager(anUser) && (anUID != lGetUlong(client, EV_uid))) {
         answer_list_add(alpp, MSG_COM_NOSHUTDOWNPERMS, STATUS_DENIED,
                         ANSWER_QUALITY_ERROR);
         DEXIT;
         return EPERM;
      }

      add_list_event_for_client(aClientID, 0, sgeE_SHUTDOWN, 0, 0, NULL, NULL,
                                NULL, NULL, true);

      /* Print out a message about the event. */
      if (aClientID == EV_ID_SCHEDD) {
         SGE_ADD_MSG_ID(sprintf (SGE_EVENT, MSG_COM_KILLED_SCHEDULER_S,
                                 lGetHost(client, EV_host)));
      }
      else {
         SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_COM_SHUTDOWNNOTIFICATION_SUS,
                        lGetString(client, EV_name),
                        (unsigned long)lGetUlong(client, EV_id),
                        lGetHost(client, EV_host)));
      }

      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   }
   else {
      SGE_ADD_MSG_ID(sprintf(SGE_EVENT, 
                        MSG_EVE_UNKNOWNEVCLIENT_US, u32c(aClientID), "shutdown"));
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = EINVAL;
   }

   unlock_client (aClientID);

   DEXIT;
   return ret;
} /* sge_shutdown_event_client */

/****** evm/sge_event_master/sge_shutdown_dynamic_event_clients() **************
*  NAME
*     sge_shutdown_dynamic_event_clients() -- shutdown all dynamic event clients
*
*  SYNOPSIS
*     int sge_shutdown_dynamic_event_clients(const char *anUser) 
*
*  FUNCTION
*     Shutdown all dynamic event clients. Each dynamic event client known will
*     be send a shutdown event.
*
*     An event client is a dynamic event client if it's client id is greater
*     than or equal to 'EV_ID_FIRST_DYNAMIC'. 
*
*     Shutting down all dynamic event clients is only permitted if 'anUser' does
*     have manager privileges.
*
*  INPUTS
*     const char *anUser - user which did request this operation 
*     lList **alpp       - answer list for info and errors
*
*  RESULT
*     EPERM - operation not permitted 
*     0     - otherwise
*
*  NOTES
*     MT-NOTES: sge_shutdown_dynamic_event_clients() is NOT MT safe. 
*
*******************************************************************************/
int sge_shutdown_dynamic_event_clients(const char *anUser, lList **alpp)
{
   lListElem *client; 
   int id = 0;
   int count = 0;

   DENTER(TOP_LAYER, "sge_shutdown_dynamic_event_clients");

   pthread_once(&Event_Master_Once, event_master_once_init);

   if (!manop_is_manager(anUser)) {
      answer_list_add(alpp, MSG_COM_NOSHUTDOWNPERMS, STATUS_DENIED,
                      ANSWER_QUALITY_ERROR);
      DEXIT;
      return EPERM;
   }

   while (Master_Control.clients_indices[count] != 0) {
      id = (u_long32)Master_Control.clients_indices[count++];

      /* Ignore clients with static ids. */
      if (id < EV_ID_FIRST_DYNAMIC) {
         continue;
      }
      
      /* Keep looking until we find a non-NULL client.  Should (almost) never
       * be an issue since the indices get rebuilt whenever anything changes. */
      /* This check causes us to lock and unlock every live client and possibly
       * some dead ones, but it's still much cheaper than any of the previous
       * incarnations of the event master locking schemes. */
      lock_client ((u_long32)id, true);
      if ((client = get_event_client ((u_long32)id)) == NULL) {
         unlock_client (id);
         continue;
      }
      unlock_client (id);
      
      /* It's not really critical that we maintain atomicity here since the
       * event is going to be put in a list for processing later.  We just don't
       * want to waste lots of time adding events for clients that we know don't
       * exist. */
      sge_add_event_for_client(id, 0, sgeE_SHUTDOWN, 0, 0, NULL, NULL, NULL,
                               NULL);

      SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_COM_SHUTDOWNNOTIFICATION_SUS,
                     lGetString(client, EV_name),
                     (unsigned long)id, lGetHost(client, EV_host)));
      answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
      
   }
   
   DEXIT;
   return 0;
} /* sge_shutdown_dynamic_event_clients() */

/****** Eventclient/Server/sge_add_event() *************************************
*  NAME
*     sge_add_event() -- add an object as event
*
*  SYNOPSIS
*     #include "sge_event_master.h"
*
*     void 
*     sge_add_event( u_long32 timestamp, ev_event type,
*                   u_long32 intkey, u_long32 intkey2, const char *strkey, 
*                   const char *strkey2, lListElem *element) 
*
*  FUNCTION
*     Adds an object to the list of events to deliver. Called, if an event 
*     occurs to that object, e.g. it was added to Grid Engine, modified or 
*     deleted.
*  
*     Internally, a list with that single object is created and passed to 
*     sge_add_list_event().
*
*  INPUTS
*     u_long32 timestamp      - time stamp in gmt for the even; if 0 is passed,
*                               sge_add_list_event will insert the actual time
*     ev_event type           - the event id
*     u_long32 intkey         - additional data
*     u_long32 intkey2        - additional data
*     const char *strkey      - additional data
*     const char *strkey2     - additional data
*     const char *session     - events session key
*     lListElem *element      - the object to deliver as event
*
*  NOTES
*     MT-NOTE: sge_add_event() is NOT MT safe.
*
*******************************************************************************/
bool sge_add_event(u_long32 timestamp, ev_event type, u_long32 intkey,
   u_long32 intkey2, const char *strkey, const char *strkey2, 
   const char *session, lListElem *element) 
{
   lList *lp = NULL;
   lList *temp_sub_lp = NULL;
   int sub_list_elem = 0;
   
   pthread_once(&Event_Master_Once, event_master_once_init);

   if (get_number_of_subscriptions(type) <= 0) {
      return true; /* no event client has this event subscribed */
   }

   if (element != NULL) {
      /* ignore the sublist in case of the following events. We have
         extra events to handle the sub-lists */
      if (type == sgeE_JOB_MOD) {
         sub_list_elem = JB_ja_tasks;
         lXchgList(element, sub_list_elem, &temp_sub_lp);
      }
      else if(type == sgeE_CQUEUE_MOD) {
         sub_list_elem = CQ_qinstances;
         lXchgList(element, sub_list_elem, &temp_sub_lp);
      }
      else if(type == sgeE_JATASK_MOD) {
         sub_list_elem = JAT_task_list;
         lXchgList(element, sub_list_elem, &temp_sub_lp);
      }
      
      lp = lCreateListHash("Events", lGetElemDescr(element), false);       
      lAppendElem (lp, lCopyElemHash(element, false));

      /* restore the original event object */
      if (temp_sub_lp != NULL) {
         lXchgList(element, sub_list_elem, &temp_sub_lp);
      }
   }
   
   return add_list_event_for_client (EV_ID_ANY, timestamp, type, intkey, intkey2,
                                     strkey, strkey2, session, lp, false);
}

/****** sge_event_master/sge_add_event_for_client() ****************************
*  NAME
*     sge_add_event_for_client() -- add an event for a given object
*
*  SYNOPSIS
*     void sge_add_event_for_client(u_long32 aClientID, u_long32 aTimestamp, 
*     ev_event anID, u_long32 anIntKey1, u_long32 anIntKey2, const char 
*     *aStrKey1, const char *aStrKey2, const char *aSeesion, lListElem 
*     *anObject) 
*
*  FUNCTION
*     Add an event for a given event client.
*
*  INPUTS
*     u_long32 aClientID   - event client id 
*     u_long32 aTimestamp  - event delivery time, 0 -> deliver now 
*     ev_event type        - event id 
*     u_long32 anIntKey1   - 1st numeric key 
*     u_long32 anIntKey2   - 2nd numeric key 
*     const char *aStrKey1 - 1st alphanumeric key 
*     const char *aStrKey2 - 2nd alphanumeric key 
*     const char *aSession - event session 
*     lListElem *element   - object to be delivered with the event 
*
*  NOTES
*     MT-NOTE: sge_add_event_for_client() is MT safe 
*
*******************************************************************************/
bool sge_add_event_for_client(u_long32 aClientID, u_long32 aTimestamp, ev_event type, u_long32 anIntKey1, 
                              u_long32 anIntKey2, const char *aStrKey1, const char *aStrKey2, 
                              const char *aSession, lListElem *element)
{
   lList *lp = NULL;
   lList *temp_sub_lp = NULL;
   int sub_list_elem =0; 

   DENTER (TOP_LAYER, "sge_add_event_for_client");
   
   pthread_once(&Event_Master_Once, event_master_once_init);

   if (get_number_of_subscriptions(type) <= 0) {
      return true; /* no event client has this event subscribed */
   }

   /* This doesn't check whether the id is too large, which is a possibility.
    * The problem is that to do the check, I'd have to grab the lock, which
    * is not worth it.  Instead I will just rely on the calling methods not
    * doing anything stupid.  The worst that can happen is that an event gets
    * created for a client that doesn't exist, which only wastes resources. */
   if (aClientID <= EV_ID_ANY) {
      ERROR((SGE_EVENT, MSG_EVE_UNKNOWNEVCLIENT_US, u32c(aClientID), "add an event"));
      DEXIT;
      return false;
   }
   
   if (element != NULL) {
      /* ignore the sublist in case of the following events. We have
         extra events to handle the sub-lists */
      if (type == sgeE_JOB_MOD) {
         sub_list_elem = JB_ja_tasks;
         lXchgList(element, sub_list_elem, &temp_sub_lp);
      }
      else if(type == sgeE_CQUEUE_MOD) {
         sub_list_elem = CQ_qinstances;
         lXchgList(element, sub_list_elem, &temp_sub_lp);
      }
      else if(type == sgeE_JATASK_MOD) {
         sub_list_elem = JAT_task_list;
         lXchgList(element, sub_list_elem, &temp_sub_lp);
      }
      
      lp = lCreateListHash("Events", lGetElemDescr(element), false);       
      lAppendElem (lp, lCopyElemHash(element, false));

      /* restore the original event object */
      if (temp_sub_lp != NULL) {
         lXchgList(element, sub_list_elem, &temp_sub_lp);
      }      
   }
   
   DEXIT;
   return add_list_event_for_client (aClientID, aTimestamp, type, anIntKey1, anIntKey2,
                                     aStrKey1, aStrKey2, aSession, lp, false);
}

/****** Eventclient/Server/sge_add_list_event() ********************************
*  NAME
*     sge_add_list_event() -- add a list as event
*
*  SYNOPSIS
*     #include "sge_event_master.h"
*
*     void 
*     sge_add_list_event( u_long32 timestamp,
*                        ev_event type, 
*                        u_long32 intkey, u_long32 intkey2, const char *strkey,
*                        const char *session, lList *list) 
*
*  FUNCTION
*     Adds a list of objects to the list of events to deliver, e.g. the 
*     sgeE*_LIST events.
*
*  INPUTS
*     u_long32 timestamp      - time stamp in gmt for the even; if 0 is passed,
*                               sge_add_list_event will insert the actual time
*     ev_event type           - the event id
*     u_long32 intkey         - additional data
*     u_long32 intkey2        - additional data
*     const char *strkey      - additional data
*     const char *session     - events session key
*     lList *list             - the list to deliver as event
*
*  NOTES
*     MT-NOTE: sge_add_list_event() is MT safe.
*
*******************************************************************************/
bool sge_add_list_event(u_long32 timestamp, ev_event type, 
                        u_long32 intkey, u_long32 intkey2, const char *strkey, 
                        const char *strkey2, const char *session, lList *list) 
{
   bool ret;
   lList *lp = NULL;
   lList *temp_sub_lp = NULL;
   int sub_list_elem = 0;
   lListElem *element = NULL;   
   
   pthread_once(&Event_Master_Once, event_master_once_init);

   if (get_number_of_subscriptions(type) <= 0) {
      return true; /* no event client has this event subscribed */
   }
  
   if (list != NULL) {
      lp = lCreateListHash("Events", lGetListDescr(list), false); 
      if (lp == NULL) {
         return false;
      }
      for_each(element, list) {
      
         /* ignore the sublist in case of the following events. We have
            extra events to handle the sub-lists */
         if (type == sgeE_JOB_MOD) {
            sub_list_elem = JB_ja_tasks;
            lXchgList(element, sub_list_elem, &temp_sub_lp);
         }
         else if(type == sgeE_CQUEUE_MOD) {
            sub_list_elem = CQ_qinstances;
            lXchgList(element, sub_list_elem, &temp_sub_lp);
         }
         else if(type == sgeE_JATASK_MOD) {
            sub_list_elem = JAT_task_list;
            lXchgList(element, sub_list_elem, &temp_sub_lp);
         }
         
         lAppendElem(lp, lCopyElemHash(element, false));

         /* restore the original event object */
         if (temp_sub_lp != NULL) {
            lXchgList(element, sub_list_elem, &temp_sub_lp);
         }
      }
   }
   
   ret = add_list_event_for_client (EV_ID_ANY, timestamp, type, intkey, intkey2,
                                    strkey, strkey2, session, lp, false);
   return ret;
}

/****** Eventclient/Server/add_list_event_for_client() *************************
*  NAME
*     add_list_event_for_client() -- add a list as event
*
*  SYNOPSIS
*     #include "sge_event_master.h"
*
*     void 
*     add_list_event_for_client(u_long32 aClientID, u_long32 timestamp,
*                               ev_event type, u_long32 intkey,
*                               u_long32 intkey2, const char *strkey,
*                               const char *session, lList *list) 
*
*  FUNCTION
*     Adds a list of objects to the list of events to deliver, e.g. the 
*     sgeE*_LIST events, to a specific client.  No checking is done to make
*     sure that the client id is valid.  That is the responsibility of the
*     calling function.
*
*  INPUTS
*     u_long32 aClientID      - the id of the recipient
*     u_long32 timestamp      - time stamp in gmt for the even; if 0 is passed,
*                               sge_add_list_event will insert the actual time
*     ev_event type           - the event id
*     u_long32 intkey         - additional data
*     u_long32 intkey2        - additional data
*     const char *strkey      - additional data
*     const char *session     - events session key
*     lList *list             - the list to deliver as event
*
*  RESULTS
*     Whether the event was added successfully.
*
*  NOTES
*     MT-NOTE: add_list_event_for_client() is MT safe.
*
*******************************************************************************/
static bool add_list_event_for_client(u_long32 aClientID, u_long32 timestamp,
                                      ev_event type, u_long32 intkey,
                                      u_long32 intkey2, const char *strkey,
                                      const char *strkey2, const char *session,
                                      lList *list, bool has_lock)
{
   lListElem *evp = lCreateElem (EV_Type); /* event envelope for the global event list */
   lList *etlp = lCreateListHash("Event_List", ET_Type, false);
   lListElem *etp = lCreateElem (ET_Type); /* actual event object */
   lListElem *client = NULL;
   bool res = true;
   bool is_add_direct = true;
   
   DENTER(TOP_LAYER, "add_list_event_for_client");
   
   pthread_once(&Event_Master_Once, event_master_once_init);
   
   if (timestamp == 0) {
      timestamp = sge_get_gmt();
   } /* event envelope */

   lSetUlong (evp, EV_id, aClientID);
   lSetString (evp, EV_session, session);
   lSetList (evp, EV_events, etlp);

   lAppendElem (etlp, etp);

   lSetUlong (etp, ET_type, type);
   lSetUlong(etp, ET_timestamp, timestamp);
   lSetUlong (etp, ET_intkey, intkey);
   lSetUlong (etp, ET_intkey2, intkey2);
   lSetString (etp, ET_strkey, strkey);
   lSetString (etp, ET_strkey2, strkey2);
   lSetList (etp, ET_new_version, list);   

   sge_mutex_lock("event_master_t_add_event_mutex", SGE_FUNC, __LINE__, &Master_Control.t_add_event_mutex);
   if (Master_Control.is_transaction) {
      is_add_direct = false; 
      if (Master_Control.transaction_events == NULL) {
         Master_Control.transaction_events = lCreateListHash ("Event_Queue", EV_Type, false);
      }

      /* If the setspecific worked, add the event. */
      if (Master_Control.transaction_events != NULL) {
         lAppendElem (Master_Control.transaction_events , evp);
         res = true;
      }
   } 
   sge_mutex_unlock("event_master_t_add_event_mutex", SGE_FUNC, __LINE__, &Master_Control.t_add_event_mutex);
   
   if (is_add_direct) {
      sge_mutex_lock("event_master_send_mutex", SGE_FUNC, __LINE__, &Master_Control.send_mutex);

      lAppendElem (Master_Control.send_events, evp);

      sge_mutex_unlock("event_master_send_mutex", SGE_FUNC, __LINE__, &Master_Control.send_mutex);

      if (aClientID != EV_ID_ANY) {
         /* If we don't already hold the lock, grab it. */
         if (!has_lock) {
            /* If grabbing the lock fails, it's because this id is bad. */
            if (!lock_client (aClientID, true)) {
               res = false;
            }
         }

         /* If we got the lock OK or already had the lock, see if we need to
          * flush. */
         if (res) {
            client = get_event_client (aClientID);

            /* If the client doesn't exist, return false. */
            if (client == NULL) {
               res = false;
            }
            /* Otherwise, flush the client if required. */
#if 0
            else if (should_flush_event_client (aClientID, type, true)) {
               set_flush ();
            }
#endif            

            /* If we had to aquire the lock, release it now. */
            if (!has_lock) {
               unlock_client (aClientID);
            }
         }
      }
      set_flush();
   }
   
   DEXIT;
   return res;
} /* end add_list_event_for_client*/

static void process_sends ()
{
   lListElem *event_client = NULL;
   lListElem *send = NULL;
   lListElem *event = NULL;
   lList *event_list = NULL;
   u_long32 ec_id = 0;
   const char *session = NULL;
   ev_event type = sgeE_ALL_EVENTS;
   int count = 0;
   lList *temp_send_events = NULL;
   lList *new_send_events = lCreateListHash("Events_To_Send", EV_Type, false);
   
   DENTER(TOP_LAYER, "process_sends");

   sge_mutex_lock("event_master_send_mutex", SGE_FUNC, __LINE__, &Master_Control.send_mutex);
      temp_send_events = Master_Control.send_events;
      Master_Control.send_events = new_send_events;
      new_send_events = NULL;
   sge_mutex_unlock("event_master_send_mutex", SGE_FUNC, __LINE__, &Master_Control.send_mutex);

   while ((send = lFirst (temp_send_events)) != NULL) {
      send = lDechainElem (temp_send_events, send);
      
      ec_id = lGetUlong (send, EV_id);
      session = lGetString (send, EV_session);
      event_list = lGetList (send, EV_events);
      
      if (ec_id == EV_ID_ANY) {
         DPRINTF (("Processing event for all clients\n"));

         event = lFirst (event_list);

         while (event != NULL) {
            event = lDechainElem (event_list, event);
            type = lGetUlong (event, ET_type);
            count = 0;

            while (Master_Control.clients_indices[count] != 0) {
               ec_id = (u_long32)Master_Control.clients_indices[count++];

               DPRINTF (("Preparing event for client %ld\n", ec_id));

               lock_client(ec_id, true);
               
               event_client = get_event_client (ec_id);
               
               /* Keep going until we get a non-NULL client. */
               /*This shouldn't ever be NULL. The indices prevent it. */
               if (event_client == NULL) {
                  unlock_client(ec_id);
                  continue; /* while */
               }

               if (eventclient_subscribed(event_client, type, session)) {
                  add_list_event_direct (event_client, event, true);
               }
               
               unlock_client(ec_id);

            } /* while */

            event = lFreeElem(event);
            event = lFirst (event_list);
         } /* while */
      } else {
         DPRINTF (("Processing event for client %d.\n", ec_id));

         lock_client(ec_id, true);

         event_client = get_event_client (ec_id);

         /* Skip bad client ids.  Events with bad client ids will be freed
          * when send is freed since we don't dechain them. */
         if (event_client != NULL) {
            event = lFirst (event_list);

            while (event != NULL) {
               event = lDechainElem (event_list, event);
               type = lGetUlong (event, ET_type);

               /* This has to come after the client is locked. */
               if ((event_client = get_event_client (ec_id)) == NULL) {
                  unlock_client(ec_id);
                  ERROR((SGE_EVENT, MSG_EVE_UNKNOWNEVCLIENT_US, u32c(ec_id), "send events"));
               }
               else if (eventclient_subscribed(event_client, type, session)) {
                  add_list_event_direct (event_client, event, false);

                  /* We can't free the event when we're done because it now belongs
                   * to send_events(). */
               } /* else if */
               event = lFirst (event_list);
            } /* while */
         } /* if */
            
         unlock_client(ec_id);
      } /* else */

      send = lFreeElem (send);
   } /* while */

   temp_send_events = lFreeList(temp_send_events);                   
   DEXIT;
   return;
} /* process_sends() */

/****** Eventclient/Server/sge_handle_event_ack() ******************************
*  NAME
*     sge_handle_event_ack() -- acknowledge event delivery
*
*  SYNOPSIS
*     #include "sge_event_master.h"
*
*     void 
*     sge_handle_event_ack(u_long32 aClientID, ev_event anEvent) 
*
*  FUNCTION
*     After the server sent events to an event client, it has to acknowledge
*     their receipt. 
*     Acknowledged events are deleted from the list of events to deliver, 
*     otherwise they will be resent after the next event delivery interval.
*     If the handling of a busy state of the event client is enabled and set to 
*     EV_BUSY_UNTIL_ACK, the event client will be set to "not busy".
*
*  INPUTS
*     u_long32 aClientID - event client sending acknowledge
*     ev_event anEvent   - serial number of the last event to acknowledge
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: sge_handle_event_ack() is MT safe.
*
*
*******************************************************************************/
void sge_handle_event_ack(u_long32 aClientID, ev_event anEvent)
{
   lListElem *evp = lCreateElem (EV_Type);
   lList *etlp = lCreateListHash ("Event_List", ET_Type, false);
   lListElem *etp = lCreateElem (ET_Type);
   
   DENTER(TOP_LAYER, "sge_handle_event_ack");
   
   pthread_once(&Event_Master_Once, event_master_once_init);

   /* This doesn't check whether the id is too large, which is a possibility.
    * The problem is that to double the check, I'd have to grab the lock, which
    * is not worth it.  Instead I will just rely on the calling methods not
    * doing anything stupid.  The worst that can happen is that an ack gets
    * created for a client that doesn't exist, which only wastes resources. */
   if (aClientID <= EV_ID_ANY) {
      ERROR((SGE_EVENT, MSG_EVE_UNKNOWNEVCLIENT_US, u32c(aClientID), "add acknowledgements"));
      DEXIT;
      return;
   }
   
   lSetUlong (etp, ET_number, (u_long32)anEvent);
   lAppendElem (etlp, etp);
   lSetUlong (evp, EV_id, aClientID);
   lSetList (evp, EV_events, etlp);
   
   sge_mutex_lock("event_master_ack_mutex", SGE_FUNC, __LINE__, &Master_Control.ack_mutex);
   
   lAppendElem (Master_Control.ack_events, evp);
   
   sge_mutex_unlock("event_master_ack_mutex", SGE_FUNC, __LINE__, &Master_Control.ack_mutex);
  
   set_flush();
  
   DEXIT;
}

static void process_acks(void)
{
   lList *list = NULL;
   lListElem *client = NULL;
   lListElem *ack = NULL;
   u_long32 ec_id = 0;
   lList *temp_ack_list = NULL;
   lList *new_ack_list = lCreateListHash("Events_To_ACK", EV_Type, false);

   DENTER(TOP_LAYER, "process_acks");

   sge_mutex_lock("event_master_ack_mutex", SGE_FUNC, __LINE__, &Master_Control.ack_mutex);
      temp_ack_list = Master_Control.ack_events;
      Master_Control.ack_events = new_ack_list;
      new_ack_list = NULL;
   sge_mutex_unlock("event_master_ack_mutex", SGE_FUNC, __LINE__, &Master_Control.ack_mutex);

   while ((ack = lFirst(temp_ack_list)) != NULL) {
      ack = lDechainElem (temp_ack_list, ack);
   
      ec_id = lGetUlong (ack, EV_id);
      
      lock_client(ec_id, true);

      /* Either way, if we're here, the client is locked. */
      client = get_event_client (ec_id);
      
      if (client == NULL) {
         unlock_client(ec_id);
         ERROR((SGE_EVENT, MSG_EVE_UNKNOWNEVCLIENT_US, u32c(ec_id), "process acknowledgements"));
      }
      else {
         int res = 0;
         
         list = lGetList(client, EV_events);

         res = purge_event_list(list, (ev_event)lGetUlong (lFirst (lGetList
                                                 (ack, EV_events)), ET_number));

         DPRINTF(("%s: purged %d acknowleded events\n", SGE_FUNC, res));

         lSetUlong(client, EV_last_heard_from, sge_get_gmt()); /* note time of ack */

         switch (lGetUlong(client, EV_busy_handling))
         {
         case EV_BUSY_UNTIL_ACK:
         case EV_THROTTLE_FLUSH:
            lSetUlong(client, EV_busy, 0); /* clear busy state */
            break;
         default:
            break;
         } /* switch */
         
         unlock_client(ec_id);
      } /* else */

      ack = lFreeElem (ack);
   } /* while */

   temp_ack_list = lFreeList(temp_ack_list);
   
   DEXIT;
} /* sge_handle_event_ack() */

/****** evm/sge_event_master/sge_deliver_events_immediately() ******************
*  NAME
*     sge_deliver_events_immediately() -- deliver events immediately 
*
*  SYNOPSIS
*     void sge_deliver_events_immediately(u_long32 aClientID) 
*
*  FUNCTION
*     Deliver all events for the event client denoted by 'aClientID'
*     immediately.
*
*  INPUTS
*     u_long32 aClientID - event client id 
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: sge_deliver_events_immediately() is NOT MT safe. 
*
*******************************************************************************/
void sge_deliver_events_immediately(u_long32 aClientID)
{
   lListElem *client = NULL;

   DENTER(TOP_LAYER, "sge_event_immediate_delivery");

   pthread_once(&Event_Master_Once, event_master_once_init);

   lock_client(aClientID, true);

   if ((client = get_event_client (aClientID)) == NULL) {
      ERROR((SGE_EVENT, MSG_EVE_UNKNOWNEVCLIENT_US, u32c(aClientID), "deliver events immediately"));
   }
   else {
      flush_events(client, 0);
      sge_mutex_lock("event_master_cond_mutex", SGE_FUNC, __LINE__, &Master_Control.cond_mutex);
/*      Master_Control.is_flush = true;*/
      Master_Control.delivery_signaled = true;
      pthread_cond_signal(&Master_Control.cond_var);
      sge_mutex_unlock("event_master_cond_mutex", SGE_FUNC, __LINE__, &Master_Control.cond_mutex);
   }   

   unlock_client(aClientID);

   DEXIT;
   return;
} /* sge_deliver_event_immediately() */


/****** evm/sge_event_master/sge_resync_schedd() *******************************
*  NAME
*     sge_resync_schedd() -- resync schedd 
*
*  SYNOPSIS
*     int sge_resync_schedd(void) 
*
*  FUNCTION
*     Does a total update (send all lists) to schedd and outputs an error
*     message.
*
*  INPUTS
*     void - none 
*
*  RESULT
*     0 - resync successful
*    -1 - otherwise
*
*  NOTES
*     MT-NOTE: sge_resync_schedd() in NOT MT safe. 
*
*******************************************************************************/
int sge_resync_schedd(void)
{
   lListElem *client;
   int ret = -1;
   DENTER(TOP_LAYER, "sge_sync_schedd");

   pthread_once(&Event_Master_Once, event_master_once_init);

   lock_client(EV_ID_SCHEDD, true);

   if ((client = get_event_client (EV_ID_SCHEDD)) != NULL) {
      ERROR((SGE_EVENT, MSG_EVE_REINITEVENTCLIENT_S,
             lGetString(client, EV_name)));
      
      total_update(client);
      
      ret = 0;
   }
   else {
      ERROR((SGE_EVENT, MSG_EVE_UNKNOWNEVCLIENT_US, u32c(EV_ID_SCHEDD),
             "resyncronize"));
      ret = -1;
   }
   
   unlock_client(EV_ID_SCHEDD);

   DEXIT;
   return ret;
} /* sge_resync_schedd() */

/****** evm/sge_event_master/sge_event_shutdown() ******************************
*  NAME
*     sge_event_shutdown() -- shutdown event delivery 
*
*  SYNOPSIS
*     void sge_event_shutdown(void) 
*
*  FUNCTION
*     Shutdown event delivery. 
*
*  INPUTS
*     void - none 
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: sge_event_shutdown() is NOT MT safe. 
*
*******************************************************************************/
void sge_event_shutdown(void)
{
   DENTER(TOP_LAYER, "sge_event_shutdown");

   pthread_once(&Event_Master_Once, event_master_once_init);

   sge_mutex_lock("event_master_mutex", SGE_FUNC, __LINE__, &Master_Control.mutex);

   Master_Control.is_exit = true;

   sge_mutex_unlock("event_master_mutex", SGE_FUNC, __LINE__, &Master_Control.mutex);

   DPRINTF(("%s: wait for send thread termination\n", SGE_FUNC));

   pthread_join(Event_Thread, NULL);

   DEXIT;
   return;
} /* sge_event_shutdown() */

/****** evm/sge_event_master/event_master_once_init() **************************
*  NAME
*     event_master_once_init() -- one-time event master initialization
*
*  SYNOPSIS
*     static void event_master_once_init(void) 
*
*  FUNCTION
*     Initialize the event master control structure. Initialize permanent
*     event array. Create and kick off event send thread.
*
*  INPUTS
*     void - none 
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: event_master_once_init() is MT safe 
*     MT-NOTE:
*     MT-NOTE: This function must only be used as a one-time initialization
*     MT-NOTE: function in conjunction with 'pthread_once()'.
*
*******************************************************************************/
static void event_master_once_init(void)
{
   pthread_attr_t attr;

   DENTER(TOP_LAYER, "event_master_once_init");

   Master_Control.len_event_clients = Master_Control.max_event_clients +
                                      EV_ID_FIRST_DYNAMIC - 1;
   Master_Control.clients = lCreateListHash("EV_Clients", EV_Type, true);
   Master_Control.ack_events = lCreateListHash("Events_To_ACK", EV_Type, false);
   Master_Control.send_events = lCreateListHash("Events_To_Send", EV_Type, false);
   Master_Control.lockfield = sge_bitfield_new (Master_Control.len_event_clients);
   Master_Control.clients_array = (lListElem **)malloc (sizeof (lListElem *) *
                                              Master_Control.len_event_clients);
   memset (Master_Control.clients_array, 0,
                       Master_Control.len_event_clients * sizeof (lListElem *));
   Master_Control.clients_indices = (int *)malloc (sizeof (int) *
                                        (Master_Control.len_event_clients + 1));
   memset (Master_Control.clients_indices, 0,
           (Master_Control.len_event_clients + 1) * sizeof (int));

   init_send_events();

   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
   pthread_create(&Event_Thread, &attr, event_deliver_thread, NULL);

/* init the event subscription counter */
   {
      int i;
      pthread_mutex_init(&Subscribed_Control.subscribed_events_mutex, NULL);

      for (i = 0; i < sgeE_EVENTSIZE; i++) {
         Subscribed_Control.subscribed_events[i] = 0;
      }
   }


   DEXIT;
   return;
} /* event_master_one_init() */

/****** evm/sge_event_master/init_send_events() ********************************
*  NAME
*     init_send_events() -- sets the events, that should allways be delivered 
*
*  SYNOPSIS
*     void init_send_events() 
*
*  FUNCTION
*     sets the events, that should allways be delivered 
*
*  NOTES
*     MT-NOTE: init_send_events() is not MT safe 
*
*******************************************************************************/
static void init_send_events(void)
{
   DENTER(TOP_LAYER, "init_send_events");

   memset(SEND_EVENTS, false, sizeof(bool) * sgeE_EVENTSIZE);

   SEND_EVENTS[sgeE_ADMINHOST_LIST] = true;
   SEND_EVENTS[sgeE_CALENDAR_LIST] = true;
   SEND_EVENTS[sgeE_CKPT_LIST] = true;
   SEND_EVENTS[sgeE_CENTRY_LIST] = true;
   SEND_EVENTS[sgeE_CONFIG_LIST] = true;
   SEND_EVENTS[sgeE_EXECHOST_LIST] = true;
   SEND_EVENTS[sgeE_JOB_LIST] = true;
   SEND_EVENTS[sgeE_JOB_SCHEDD_INFO_LIST] = true;
   SEND_EVENTS[sgeE_MANAGER_LIST] = true;
   SEND_EVENTS[sgeE_OPERATOR_LIST] = true;
   SEND_EVENTS[sgeE_PE_LIST] = true;
   SEND_EVENTS[sgeE_PROJECT_LIST] = true;
   SEND_EVENTS[sgeE_QMASTER_GOES_DOWN] = true;
   SEND_EVENTS[sgeE_CQUEUE_LIST] = true;
   SEND_EVENTS[sgeE_SUBMITHOST_LIST] = true;
   SEND_EVENTS[sgeE_USER_LIST] = true;
   SEND_EVENTS[sgeE_USERSET_LIST] = true;
   SEND_EVENTS[sgeE_HGROUP_LIST] = true;
#ifndef __SGE_NO_USERMAPPING__      
   SEND_EVENTS[sgeE_CUSER_LIST] = true;
#endif      

   DEXIT;
   return;
} /* init_send_events() */

/****** evm/sge_event_master/event_deliver_thread() *************************************
*  NAME
*     event_deliver_thread() -- send events due 
*
*  SYNOPSIS
*     static void* event_deliver_thread(void *anArg) 
*
*  FUNCTION
*     Event send thread. Do common thread initialization. Send events until
*     shutdown is requestet. 
*
*  INPUTS
*     void *anArg - none 
*
*  RESULT
*     void* - none 
*
*  EXAMPLE
*     ??? 
*
*  NOTES
*     MT-NOTE: event_deliver_thread() is a thread function. Do NOT use this function
*     MT-NOTE: in any other way!
*
*******************************************************************************/
static void* event_deliver_thread(void *anArg)
{
   lListElem *report = NULL; 
   lList *report_list = NULL;
   struct timespec ts;
   time_t next_prof_output = 0;

   DENTER(TOP_LAYER, "event_deliver_thread");

   sge_qmaster_thread_init(true);

   set_thread_name(pthread_self(),"Deliver Thread");

   report_list = lCreateListHash("report list", REP_Type, false);
   report = lCreateElem(REP_Type);
   lSetUlong(report, REP_type, NUM_REP_REPORT_EVENTS);
   lSetHost(report, REP_host, uti_state_get_qualified_hostname());
   lAppendElem(report_list, report);

   while (!should_exit()) {

     if (thread_prof_active_by_id(pthread_self())) {
         prof_start(SGE_PROF_CUSTOM1, NULL);
         prof_start(SGE_PROF_GDI_REQUEST, NULL);
         prof_set_level_name(SGE_PROF_CUSTOM1, "Deliver Thread", NULL); 
      } else {
           prof_stop(SGE_PROF_CUSTOM1, NULL);
           prof_stop(SGE_PROF_GDI_REQUEST, NULL);
        }

      PROF_START_MEASUREMENT(SGE_PROF_CUSTOM1);

      /* update thread alive time */
      sge_update_thread_alive_time(SGE_MASTER_EVENT_DELIVER_THREAD);
      sge_mutex_lock("event_master_cond_mutex", SGE_FUNC, __LINE__, &Master_Control.cond_mutex);
      /*
       * did a new event arrive which has a flush time of 0 seconds?
       */
      if (!Master_Control.delivery_signaled) {
         u_long32 current_time = sge_get_gmt();
  
         /*
          * since the ack for a delivered event might take some time, we want to sleep
          * before delivering new events. If we would change this into a while loop,
          * the client would never sleep, because it does take some time before a event
          * is acknowledged. An event is removed, after it gets acknowledged.
          */
         /* This block will sleep until an event is delivered, someone shuts the
          * qmaster down, or the given time interval passes.  This is not an
          * issue with clients and timeouts because the delivery interval will
          * likely never be more than 1 sec.  We ignore the nanoseconds
          * component of the interval in the loop comparison because we can't
          * represent nanoseconds in a u_long32 that contains seconds.  At worst
          * this shortcut will occasionally cause this block to finish early
          * due to a well timed spurrious wakeup. */
         do { 
            ts.tv_sec = current_time + EVENT_DELIVERY_INTERVAL_S;
            ts.tv_nsec = EVENT_DELIVERY_INTERVAL_N;
            pthread_cond_timedwait(&Master_Control.cond_var,
                                   &Master_Control.cond_mutex, &ts);
         } while (!Master_Control.delivery_signaled && !should_exit() &&
                  ((sge_get_gmt() - current_time) < EVENT_DELIVERY_INTERVAL_S));

      }
      Master_Control.delivery_signaled = false;

      

      sge_mutex_unlock("event_master_cond_mutex", SGE_FUNC, __LINE__, &Master_Control.cond_mutex);

      /* If the client array has changed, rebuild the indices. */
      sge_mutex_lock("event_master_mutex", SGE_FUNC, __LINE__, &Master_Control.mutex);
      
      if (Master_Control.indices_dirty) {
         lListElem *ep = NULL;
         int count = 0;

         DPRINTF (("Rebuilding indices\n"));
         
         /* For a large number of event clients, this loop would be faster as
          * a for loop that walks through the clients_array. */
         for_each (ep, Master_Control.clients) {
            Master_Control.clients_indices[count++] = (int)lGetUlong (ep, EV_id);
         }
         
         Master_Control.clients_indices[count] = 0;
         Master_Control.indices_dirty = false;
      }
      
      sge_mutex_unlock("event_master_mutex", SGE_FUNC, __LINE__, &Master_Control.mutex);
      
      process_acks ();
      process_sends ();
      send_events(report, report_list);

      PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM1);

      if (prof_is_active(SGE_PROF_ALL)) {
        time_t now = sge_get_gmt();

         if (now > next_prof_output) {
            prof_output_info(SGE_PROF_ALL, false, "profiling summary:\n");
            prof_reset(SGE_PROF_ALL,NULL);
            next_prof_output = now + 60;
         }
      }
   }

   report_list = lFreeList(report_list);
   report = NULL;
   
   DEXIT;
   return NULL;
}

/****** evm/sge_event_master/should_exit() *************************************
*  NAME
*     should_exit() -- should thread exit? 
*
*  SYNOPSIS
*     static bool should_exit(void) 
*
*  FUNCTION
*     Determine if send thread should exit. Does return value of the Master
*     Control struct 'exit' flag. 
*
*  INPUTS
*     void - none 
*
*  RESULT
*     true   - exit
*     false  - continue 
*
*  NOTES
*     MT-NOTE: should_exit() is MT safe 
*
*******************************************************************************/
static bool should_exit(void)
{
   bool res = false;

   DENTER(TOP_LAYER, "should_exit");

   sge_mutex_lock("event_master_mutex", SGE_FUNC, __LINE__,
                  &Master_Control.mutex);
   res = Master_Control.is_exit;
   sge_mutex_unlock("event_master_mutex", SGE_FUNC, __LINE__,
                    &Master_Control.mutex);

   DEXIT;
   return res;
} /* should_exit() */



/****** sge_event_master/remove_event_client() *********************************
*  NAME
*     remove_event_client() -- removes an event client
*
*  SYNOPSIS
*     static void remove_event_client(lListElem *client, int aClientID) 
*
*  FUNCTION
*     removes a event client, marks the index as dirty and frees the memory
*
*  INPUTS
*     lListElem *client - event client to remove
*     int aClientID     - event client id to remove
*
*  NOTES
*     MT-NOTE: remove_event_client() is not MT safe
*     - it locks the event master mutex to modify the event client list
*     - it assums that the event client is locked before this method is called
*
*******************************************************************************/
static void remove_event_client(lListElem *client, int aClientID) {
   subscription_t *old_sub = NULL;
   int i;

   DENTER(TOP_LAYER, "remove_event_client");

   old_sub = lGetRef(client, EV_sub_array);

   INFO((SGE_EVENT, MSG_EVE_UNREG_SU, lGetString(client, EV_name),
         u32c(lGetUlong(client, EV_id))));

   if (old_sub) {

      sge_mutex_lock("event_master_subscribed_events_mutex", "lock_client", __LINE__, 
                  &Subscribed_Control.subscribed_events_mutex);  
      
      for (i=0; i<sgeE_EVENTSIZE; i++){ 
         if (old_sub[i].subscription == EV_SUBSCRIBED){
            Subscribed_Control.subscribed_events[i]--;
         }
         if (old_sub[i].where) {
            lFreeWhere(old_sub[i].where);
         }   
         if (old_sub[i].what) {
            lFreeWhat(old_sub[i].what);
         }   
         if (old_sub[i].descr){
            cull_hash_free_descr(old_sub[i].descr);
            free(old_sub[i].descr);
         }
      } 
      
      sge_mutex_unlock("event_master_subscribed_events_mutex", "lock_client", __LINE__, 
                    &Subscribed_Control.subscribed_events_mutex);
      
      free(old_sub);
      lSetRef(client, EV_sub_array, NULL);
   }

   set_event_client (aClientID, NULL);
   sge_mutex_lock("event_master_mutex", SGE_FUNC, __LINE__, &Master_Control.mutex);
   lRemoveElem(Master_Control.clients, client);
   Master_Control.indices_dirty = true;
   sge_mutex_unlock("event_master_mutex", SGE_FUNC, __LINE__, &Master_Control.mutex);

   DEXIT;
   return;
}

/****** evm/sge_event_master/get_number_of_subscriptions() ******************
*  NAME
*     get_number_of_subscriptions() -- returns the number of event clients, 
*                                        that have a given event subscribed
*
*  SYNOPSIS
*     static int get_number_of_subscriptions(u_long32 event_type)
*
*  FUNCTION
*     returns the number of events clients subscribing a given event.
*
*  INPUTS
*     u_long32 event_type - event number
*
*  RESULT
*    int - number of event clients 
*
*  NOTES
*     MT-NOTE: get_number_of_subscriptions() is MT safe 
*
*******************************************************************************/

static int get_number_of_subscriptions(u_long32 event_type) {
   int ret = 0;

   sge_mutex_lock("event_master_subscribed_events_mutex", "lock_client", __LINE__, 
                  &Subscribed_Control.subscribed_events_mutex);  
   
   ret = Subscribed_Control.subscribed_events[event_type];

   sge_mutex_unlock("event_master_subscribed_events_mutex", "lock_client", __LINE__, 
                    &Subscribed_Control.subscribed_events_mutex);
   
   return ret;
}


/****** evm/sge_event_master/send_events() *************************************
*  NAME
*     send_events() -- send events to event clients 
*
*  SYNOPSIS
*     static void send_events(void) 
*
*  FUNCTION
*     Loop over all event clients and send events due. If an event client did
*     time out, it will be removed from the list of registered event clients.
*
*     Events will be delivered only, if the so called 'busy handling' of a 
*     client does allow it. Events will be delivered as a report (REP_Type)
*     with a report list of type ET_Type. 
*
*  INPUTS
*     lListElem *report  - a report, has to be part of the report list. All
*                          fields have to be init, except for REP_list element.   
*     lList *report_list - a pre-init report list
*
*  RESULT
*     void - none 
*
*  NOTES
*     MT-NOTE: send_events() is MT safe 
*     MT-NOTE:
*     MT-NOTE: After all events for all clients have been sent. This function
*     MT-NOTE: will wait on the condition variable 'Master_Control.cond_var'
*
*******************************************************************************/
static void send_events(lListElem *report, lList *report_list) {
   u_long32 timeout, busy_handling;
   lListElem *event_client;
   const char *host;
   const char *commproc;
   int ret, id; 
   int deliver_interval;
   time_t now = time(NULL);
   u_long32 ec_id = 0;
   int count = 0;

   DENTER(TOP_LAYER, "send_events");

   while (Master_Control.clients_indices[count] != 0) {
      ec_id = (u_long32)Master_Control.clients_indices[count++];

      if (!lock_client(ec_id, false)) {

         /* If we can't get access to this client right now, just leave it to
          * be handled in the next run. */
         DPRINTF (("Skipping event client %d because it's locked.\n", ec_id));

         continue; /* while */
      } /* if */
      
      event_client = get_event_client (ec_id);

      /* Keep looking for a non-NULL client. */
      if (event_client == NULL) {
         unlock_client(ec_id);
         continue; /* while */
      }
     
      /* is the event client in state terminated? remove it*/
      if (lGetUlong(event_client, EV_state) == EV_terminated) {
         remove_event_client(event_client, ec_id);
         unlock_client(ec_id);
         /* we removed a client, continue with the next one */
         continue;
      }
     
      /* extract address of event client */
      host = lGetHost(event_client, EV_host);
      commproc = lGetString(event_client, EV_commproc);
      id = lGetUlong(event_client, EV_commid);

      deliver_interval = lGetUlong(event_client, EV_d_time);
      busy_handling = lGetUlong(event_client, EV_busy_handling);

      /* somone turned the clock back */
      if (lGetUlong(event_client, EV_last_heard_from) > now) {
         lSetUlong(event_client, EV_last_heard_from, now);
         lSetUlong(event_client, EV_next_send_time, now + deliver_interval);
      }
      else if (lGetUlong(event_client, EV_last_send_time)  > now) {
         lSetUlong(event_client, EV_last_send_time, now);
      }
  
      /* if set, use qmaster_params SCHEDULER_TIMEOUT */
      if (scheduler_timeout > 0) {
          timeout = scheduler_timeout;
      }
      else {
         /* is the ack timeout expired ? */
         timeout = 10*deliver_interval;
         
         if (timeout < EVENT_ACK_MIN_TIMEOUT) {
            timeout = EVENT_ACK_MIN_TIMEOUT;
         }
         else if (timeout > EVENT_ACK_MAX_TIMEOUT) {
            timeout = EVENT_ACK_MAX_TIMEOUT;
         }
      }

      if (now > (lGetUlong(event_client, EV_last_heard_from) + timeout)) {
         ERROR((SGE_EVENT, MSG_COM_ACKTIMEOUT4EV_ISIS, 
               (int) timeout, commproc, (int) id, host));
         remove_event_client(event_client, ec_id);      
         unlock_client(ec_id);
         continue; /* while */
      }

      /* do we have to deliver events ? */
      if ((now >= lGetUlong(event_client, EV_next_send_time)) 
           && (busy_handling == EV_THROTTLE_FLUSH 
               || !lGetUlong(event_client, EV_busy))
         ) {
         lList *lp = NULL;
         
         /* put only pointer in report - dont copy */
         lXchgList(event_client, EV_events, &lp);
         lXchgList(report, REP_list, &lp);

         unlock_client(ec_id);
        
         ret = report_list_send(report_list, host, commproc, id, 0, NULL);
         
         lock_client(ec_id, true);      

         event_client = get_event_client (ec_id);

         /* Keep looking for a non-NULL client. */
         if (event_client != NULL) {
            /* on failure retry is triggered automatically */
            if (ret == CL_RETVAL_OK) {
               now = sge_get_gmt();

               /*printf("send events %d to host: %s id: %d: now: %d\n", numevents, host, id, sge_get_gmt()); */           
               switch (busy_handling) {
               case EV_THROTTLE_FLUSH:
                  /* increase busy counter */
                  lSetUlong(event_client, EV_busy, lGetUlong(event_client, EV_busy)+1); 
                  break;
               case EV_BUSY_UNTIL_RELEASED:
               case EV_BUSY_UNTIL_ACK:
                  lSetUlong(event_client, EV_busy, 1);
                  break;
               default: 
                  /* EV_BUSY_NO_HANDLING */
                  break;
               }

               lSetUlong(event_client, EV_last_send_time, now);
            }

            /* We reset this time even if the report list send failed because we
             * want to give failed clients a break before trying them again. */
            lSetUlong (event_client, EV_next_send_time, now + deliver_interval);

            /* don't delete sent events - deletion is triggerd by ack's */
            lXchgList(report, REP_list, &lp);
            lXchgList(event_client, EV_events, &lp);

            /* Because we let go of the lock while we were sending the report, we
             * have to make certain that no one added events to this client while
             * we were busy.  If there are events, just add them to the end of our
             * previous list if it exists.  If not, we just keep the new list. */
            if (lp != NULL) {
               lList *olp = lGetList (event_client, EV_events);

               if (olp != NULL) {
                  /* This frees lp. */
                  lAddList (olp, lp);
                  lp = NULL;
               }
               else {
                  lSetList (event_client, EV_events, lp);
               }
            }
         }
      } /* if */
      
      unlock_client(ec_id);      
   } /* while */
   
   DEXIT;
   return;
} /* send_events() */
 
static void flush_events(lListElem *event_client, int interval)
{
   u_long32 next_send, flush_delay;
   int now = time(NULL);
   DENTER(TOP_LAYER, "flush_events");

   SGE_ASSERT(event_client != NULL);

   next_send = lGetUlong(event_client, EV_next_send_time);
   next_send = MIN(next_send, now + interval);

   /* never send out two event packages in the very same second */
   flush_delay = 1;

   if (lGetUlong(event_client, EV_busy_handling) == EV_THROTTLE_FLUSH) {
      u_long32 busy_counter = lGetUlong(event_client, EV_busy);
      u_long32 ed_time = lGetUlong(event_client, EV_d_time);
      u_long32 flush_delay_rate = MAX(lGetUlong(event_client, EV_flush_delay), 1);
      if (busy_counter >= flush_delay_rate) {
         /* busy counters larger than flush delay cause events being 
            sent out in regular event delivery interval for alive protocol 
            purposes with event client */
         flush_delay = MAX(flush_delay, ed_time);
      } else {
         /* for smaller busy counters event delivery interval is scaled 
            down with the busy counter */
         flush_delay = MAX(flush_delay, ed_time * busy_counter / flush_delay_rate);
      }
   }

   next_send = MAX(next_send, lGetUlong(event_client, EV_last_send_time) + flush_delay);

   lSetUlong(event_client, EV_next_send_time, next_send);
   
   DPRINTF(("%s: %s %d\tNOW: %d NEXT FLUSH: %d (%s,%s,%d)\n", 
         SGE_FUNC, lGetString(event_client, EV_name), lGetUlong(event_client, EV_id), 
         now, next_send, 
         lGetHost(event_client, EV_host), 
         lGetString(event_client, EV_commproc),
         lGetUlong(event_client, EV_commid))); 

   DEXIT;
   return;
} /* flush_events() */

/****** Eventclient/Server/total_update() **************************************
*  NAME
*     total_update() -- send all data to eventclient
*
*  SYNOPSIS
*     static void 
*     total_update(lListElem *event_client) 
*
*  FUNCTION
*     Sends all complete lists it subscribed to an eventclient.
*     If the event client receives a complete list instead of single events,
*     it should completely update it's database.
*
*  INPUTS
*     lListElem *event_client - the event client to update
*
*  NOTES
*     MT-NOTE: total_update() is MT safe, IF the function is invoked with
*     MT-NOTE: 'LOCK_EVENT_CLIENT_LST' locked! This is in accordance with
*     MT-NOTE: the acquire/release protocol as defined by the Grid Engine
*     MT-NOTE: Locking API.
*     MT-NOTE: the method also locks the global lock. One has to make sure,
*     MT-NOTE: that no calling method has that lock already.
*
*  SEE ALSO
*     libs/lck/sge_lock.h
*     libs/lck/sge_lock.c
*
*******************************************************************************/
static void total_update(lListElem *event_client)
{
   DENTER(TOP_LAYER, "total_update");

   SGE_ASSERT (event_client != NULL);
  
   blockEvents(event_client, sgeE_ALL_EVENTS, true);
   
   SGE_LOCK(LOCK_GLOBAL, LOCK_READ);
   
   total_update_event(event_client, sgeE_ADMINHOST_LIST);
   total_update_event(event_client, sgeE_CALENDAR_LIST);
   total_update_event(event_client, sgeE_CKPT_LIST);
   total_update_event(event_client, sgeE_CENTRY_LIST);
   total_update_event(event_client, sgeE_CONFIG_LIST);
   total_update_event(event_client, sgeE_EXECHOST_LIST);
   total_update_event(event_client, sgeE_JOB_LIST);
   total_update_event(event_client, sgeE_JOB_SCHEDD_INFO_LIST);
   total_update_event(event_client, sgeE_MANAGER_LIST);
   total_update_event(event_client, sgeE_OPERATOR_LIST);
   total_update_event(event_client, sgeE_PE_LIST);
   total_update_event(event_client, sgeE_CQUEUE_LIST);
   total_update_event(event_client, sgeE_SCHED_CONF);
   total_update_event(event_client, sgeE_SUBMITHOST_LIST);
   total_update_event(event_client, sgeE_USERSET_LIST);

   total_update_event(event_client, sgeE_NEW_SHARETREE);
   total_update_event(event_client, sgeE_PROJECT_LIST);
   total_update_event(event_client, sgeE_USER_LIST);

   total_update_event(event_client, sgeE_HGROUP_LIST);

#ifndef __SGE_NO_USERMAPPING__
   total_update_event(event_client, sgeE_CUSER_LIST);
#endif

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);
   
   DEXIT;
   return;
} /* total_update() */


/****** evm/sge_event_master/build_subscription() ******************************
*  NAME
*     build_subscription() -- generates an array out of the cull registration
*                                 structure
*
*  SYNOPSIS
*     static void build_subscription(lListElem *event_el) 
*
*  FUNCTION
*      generates an array out of the cull registration
*      structure. The array contains all event elements and each of them 
*      has an identifier, if it is subscribed or not. Before that is done, it is
*      tested, the EV_changed flag is set. If not, the function simply returns.
*
*
*  INPUTS
*     lListElem *event_el - the event element, which event structure will be transformed 
*
*******************************************************************************/
static void build_subscription(lListElem *event_el)
{
   lList *subscription = lGetList(event_el, EV_subscribed);
   lListElem *sub_el = NULL;
   subscription_t *sub_array = NULL; 
   subscription_t *old_sub_array = NULL;
   int i = 0;

   DENTER(TOP_LAYER, "build_subscription");


   if (!lGetBool(event_el, EV_changed)) {
      DEXIT;
      return;
   }

   DPRINTF(("rebuild event mask for a client\n"));

   sub_array = (subscription_t *) malloc(sizeof(subscription_t) * sgeE_EVENTSIZE);
   
   memset(sub_array, 0, sizeof(subscription_t) * sgeE_EVENTSIZE); 

   for (i=0; i<sgeE_EVENTSIZE; i++) {
      sub_array[i].subscription = EV_NOT_SUBSCRIBED;
      sub_array[i].blocked = false;
   }
  
   sge_mutex_lock("event_master_subscribed_events_mutex", "lock_client", __LINE__, 
                  &Subscribed_Control.subscribed_events_mutex);
   
   for_each(sub_el, subscription){
      const lListElem *temp = NULL;
      u_long32 event = lGetUlong(sub_el, EVS_id);   
      
      sub_array[event].subscription = EV_SUBSCRIBED; 
      Subscribed_Control.subscribed_events[event]++;
      sub_array[event].flush = lGetBool(sub_el, EVS_flush);
      sub_array[event].flush_time = lGetUlong(sub_el, EVS_interval);
     
      if ((temp = lGetObject(sub_el, EVS_where)))
         sub_array[event].where = lWhereFromElem(temp);

      if ((temp = lGetObject(sub_el, EVS_what))) {      
         sub_array[event].what = lWhatFromElem(temp);
      }         
   }
   
   old_sub_array = lGetRef(event_el, EV_sub_array);
   
   if (old_sub_array) {
      int i;
      for (i=0; i<sgeE_EVENTSIZE; i++){ 
         if (sub_array[i].subscription == EV_SUBSCRIBED){
            Subscribed_Control.subscribed_events[i]--;
         }
         if (old_sub_array[i].where) {
            lFreeWhere(old_sub_array[i].where);
         }   
         if (old_sub_array[i].what) {
            lFreeWhat(old_sub_array[i].what);
         }   
         if (old_sub_array[i].descr){
            cull_hash_free_descr(old_sub_array[i].descr);
            free(old_sub_array[i].descr);
         }
      }
      free(old_sub_array);
   }
  
   sge_mutex_unlock("event_master_subscribed_events_mutex", "lock_client", __LINE__, 
                    &Subscribed_Control.subscribed_events_mutex);

   
   lSetRef(event_el, EV_sub_array, sub_array);
   lSetBool(event_el, EV_changed, false);

   DEXIT;
} /* build_subscription() */

/****** Eventclient/Server/check_send_new_subscribed_list() ********************
*  NAME
*     check_send_new_subscribed_list() -- check suscription for new list events
*
*  SYNOPSIS
*     static void 
*     check_send_new_subscribed_list(const subscription_t *old_subscription, 
*                                    const subscription_t *new_subscription, 
*                                    lListElem *event_client, 
*                                    ev_event event) 
*
*  FUNCTION
*     Checks, if sgeE*_LIST events have been added to the subscription of a
*     certain event client. If yes, send these lists to the event client.
*
*  INPUTS
*     const subscription_t *old_subscription - former subscription
*     const subscription_t *new_subscription - new subscription
*     lListElem *event_client      - the event client object
*     ev_event event               - the event to check
*
*  SEE ALSO
*     Eventclient/Server/total_update_event()
*
*******************************************************************************/
static void 
check_send_new_subscribed_list(const subscription_t *old_subscription, 
                               const subscription_t *new_subscription, 
                               lListElem *event_client, ev_event event)
{
   if ((new_subscription[event].subscription & EV_SUBSCRIBED) && 
       (old_subscription[event].subscription == EV_NOT_SUBSCRIBED)) {
      total_update_event(event_client, event);
   }   
}

/****** Eventclient/Server/eventclient_subscribed() ************************
*  NAME
*     eventclient_subscribed() -- has event client subscribed an event?
*
*  SYNOPSIS
*     #include "sge_event_master.h"
*
*     int 
*     eventclient_subscribed(const lListElem *event_client, ev_event event) 
*
*  FUNCTION
*     Checks if the given event client has a certain event subscribed.
*     For event clients that use session filtering additional conditions
*     must be fulfilled otherwise the event counts not as subscribed.
*
*  INPUTS
*     const lListElem *event_client - event client to check
*     ev_event event                - event to check
*     const char *session           - session key of this event
*
*  RESULT
*     int - 0 = not subscribed, 1 = subscribed
*
*  SEE ALSO
*     Eventclient/-Session filtering
*******************************************************************************/
static int eventclient_subscribed(const lListElem *event_client, ev_event event, 
                                  const char *session)
{
   const subscription_t *subscription = NULL;
   const char *ec_session;

   DENTER(TOP_LAYER, "eventclient_subscribed");

   SGE_ASSERT (event_client != NULL);
   
   if (event_client == NULL) {
      DEXIT;
      return 0;
   }

   subscription = lGetRef(event_client, EV_sub_array);
   ec_session = lGetString(event_client, EV_session);

   if (subscription == NULL) {
      DPRINTF (("No subscription!\n"));
      DEXIT;
      return 0;
   }

   if (ec_session) {
      if (session) {
         /* events that belong to a specific session are not subscribed 
            in case the event client is not interested in that session */
         if (strcmp(session, ec_session)) {
            DPRINTF (("Event session does not match client session\n"));
            DEXIT;
            return 0;
         }
      } else {
         /* events that do not belong to a specific session are not 
            subscribed if the event client is interested in events of a 
            specific session. 
            The only exception are list events, because list events do not 
            belong to a specific session. These events require filtering on 
            a more fine grained level */
         if (!IS_TOTAL_UPDATE_EVENT(event)) {
            DEXIT;
            return 0;
         }
      }
   }
   if (subscription[event].subscription == EV_SUBSCRIBED &&
       subscription[event].blocked == false) {
      DEXIT;
      return 1;
   }

   DEXIT;
   return 0;
}

/****** evm/sge_event_master/purge_event_list() ********************************
*  NAME
*     purge_event_list() -- purge event list
*
*  SYNOPSIS
*     static int purge_event_list(lList* aList, ev_event anEvent) 
*
*  FUNCTION
*     Remove all events from 'aList' which do have an event id less than or
*     equal to 'anEvent'.
*
*  INPUTS
*     lList* aList     - event list
*     ev_event anEvent - event
*
*  RESULT
*     int - number of events purged.
*
*  NOTES
*     MT-NOTE: purge_event_list() is NOT MT safe. 
*     MT-NOTE: 
*     MT-NOTE: Do not call this function without having 'aList' locked!
*
*  BUGS
*     BUGBUG-AD: If 'anEvent' == 0, not events will be purged. However zero is
*     BUGBUG-AD: also the id of 'sgeE_ALL_EVENTS'. Is this behaviour correct?
*
*******************************************************************************/
static int purge_event_list(lList* aList, ev_event anEvent) 
{
   int purged = 0, pos = 0;
   lListElem *ev = NULL;

   DENTER(TOP_LAYER, "purge_event_list");

   if (0 == anEvent) {
      DEXIT;
      return 0;
   }

   pos = lGetPosInDescr(ET_Type, ET_number);

   ev = lFirst(aList);

   while (ev)
   {
      lListElem *tmp = ev;

      ev = lNext(ev); /* fetch next event, before the old one will be deleted */

      if (lGetPosUlong(tmp, pos) > anEvent) {
         break;
      }

      lRemoveElem(aList, tmp);
      purged++;
   }

   DEXIT;
   return purged;
} /* remove_events_from_client() */

static void add_list_event_direct(lListElem *event_client, lListElem *event, 
                                  bool copy_event)
{
   lList *lp = NULL;
   lList *clp = NULL;
   lListElem *ep = NULL;
   ev_event type = lGetUlong(event, ET_type);
   bool flush = false;
   subscription_t *subscription = NULL;
   char buffer[1024];
   dstring buffer_wrapper;
   u_long32 i = 0;
   const lCondition *selection = NULL;
   const lEnumeration *fields = NULL;
   const lDescr *descr = NULL;
   
   DENTER(TOP_LAYER, "add_list_event_direct"); 

   SGE_ASSERT (event_client != NULL);
  
   if (lGetUlong(event_client, EV_state) != EV_connected) {
      /* the event client is not connected anymore, so we are not
         adding new events to it*/
      return;
   }

   /* if the total updates blocked the client, we have to unblock this list */
   blockEvents(event_client, type, false); 

   sge_dstring_init(&buffer_wrapper, buffer, sizeof(buffer));

   /* Pull out payload for selecting */
   lXchgList (event, ET_new_version, &lp);

   /* If the list is NULL, no need to bother with any of this.  Plus, if we
    * did do this part with a NULL list, the check for a clp with no
    * elements would kick us out. */
   if (lp != NULL) {
      subscription = lGetRef(event_client, EV_sub_array);
      selection = subscription[type].where;
      fields = subscription[type].what;

#if 0
      DPRINTF(("deliver event: %d with where filter=%s and what filter=%s\n", 
               type, selection?"true":"false", fields?"true":"false")); 
#endif               

      if (fields) {
         descr = getDescriptorL(subscription, lp, type);

         DPRINTF (("Reducing event data\n"));
         
         if (!list_select(subscription, type, &clp, lp, selection, fields,
                          descr)){
            clp = lSelectD("updating list", lp, selection, descr, fields, false);
         }

         /* no elements in the event list, no need for an event */
         if (!SEND_EVENTS[type] && lGetNumberOfElem(clp) == 0) {
            if (clp != NULL) {
               clp = lFreeList(clp);
            }
            
            /* we are not making a copy, so we have to restore the old element */
            lXchgList (event, ET_new_version, &lp);
               
            if (!copy_event) {
               event = lFreeElem(event);
            }

            DPRINTF (("Skipping event because it has no content for this client.\n"));
            return;
         }

         /* If we're not making a copy, we have to free the original list.  If
          * we are making a copy, freeing the list is the responsibility of the
          * calling function. */
         if (!copy_event) {
            lp = lFreeList (lp);
         }
      } /* if */
      /* If there's no what clause, and we want a copy, we copy the list */
      else if (copy_event) {
         DPRINTF (("Copying event data\n"));
         clp = lCopyListHash(lGetListName (lp), lp, false);
      }
      /* If there's no what clause, and we don't want to copy, we just reuse
       * the original list. */
      else {
         clp = lp;
         /* Make sure lp is clear for the next part. */
         lp = NULL;
      }
   } /* if */

   /* If we're making a copy, copy the event and swap the orignial list
    * back into the original event */
   if (copy_event) {
      DPRINTF (("Copying event\n"));
      ep = lCopyElemHash(event, false);
      
      lXchgList (event, ET_new_version, &lp);
      
   }
   /* If we're not making a copy, reuse the original event. */
   else {
      ep = event;
   }

   /* Swap the new list into the working event. */
   lXchgList (ep, ET_new_version, &clp);
   
   /* fill in event number and increment 
      EV_next_number of event recipient */
   i = lGetUlong(event_client, EV_next_number);
   lSetUlong(event_client, EV_next_number, (i + 1));
   lSetUlong(ep, ET_number, i);
  
   /* build a new event list if not exists */
   lp = lGetList(event_client, EV_events);
   
   if (lp == NULL) {
      lp=lCreateListHash("Events", ET_Type, false);
      lSetList(event_client, EV_events, lp);
   }

   /* chain in new event */
   lAppendElem(lp, ep);

   DPRINTF(("%d %s\n", lGetUlong(event_client, EV_id), 
            event_text(ep, &buffer_wrapper)));

   /* check if event clients wants flushing */
   subscription = lGetRef(event_client, EV_sub_array);
      
   if (type == sgeE_QMASTER_GOES_DOWN) {
      Master_Control.is_prepare_shutdown = true;
      lSetUlong(event_client, EV_busy, 0); /* can't be too busy for shutdown */
      flush_events(event_client, 0);
      flush = true;
   }
   else if (type == sgeE_SHUTDOWN) {
      flush_events(event_client, 0);
      flush = true;
      /* the event client should be shutdown, so we do not add any events to it, after
         the shutdown event */
      lSetUlong(event_client, EV_state, EV_closing);   
   }
   else if (subscription[type].flush) {
      DPRINTF(("flushing event client\n"));
      flush_events(event_client, subscription[type].flush_time);
      flush = ( subscription[type].flush_time == 0);
   }

   DEXIT;
}

/****** Eventclient/Server/total_update_event() *******************************
*  NAME
*     total_update_event() -- create a total update event
*
*  SYNOPSIS
*     static void 
*     total_update_event(lListElem *event_client, ev_event type) 
*
*  FUNCTION
*     Creates an event delivering a certain list of objects for an event client.
*     For event clients that have subscribed a session list filtering can be done
*     here.
*
*  INPUTS
*     lListElem *event_client - event client to receive the list
*     ev_event type           - event describing the list to update
*
*******************************************************************************/
static void total_update_event(lListElem *event_client, ev_event type) 
{
   lList *lp = NULL; /* lp should be set, if we have to make a copy */
   lList *copy_lp = NULL; /* copy_lp should be used for a copy of the org. list */
   char buffer[1024];
   dstring buffer_wrapper;
   const char *session = NULL;
   u_long32 id;

   DENTER(TOP_LAYER, "total_update_event");
   
   SGE_ASSERT (event_client != NULL);
   
   sge_dstring_init(&buffer_wrapper, buffer, sizeof(buffer));
   session = lGetString(event_client, EV_session);
   id = lGetUlong(event_client, EV_id);

   /* This test bothers me.  Technically, the GDI thread should just drop the
    * event in the send queue and forget about it.  However, doing this test
    * here could prevent the queuing of events that will later be determined
    * to be useless. */
   if (eventclient_subscribed(event_client, type, NULL)) {
      
      unlock_client (id); 
   
      switch (type) {
         case sgeE_ADMINHOST_LIST:
            lp = Master_Adminhost_List;
            break;
         case sgeE_CALENDAR_LIST:
            lp = Master_Calendar_List;
            break;
         case sgeE_CKPT_LIST:
            lp = Master_Ckpt_List;
            break;
         case sgeE_CENTRY_LIST:
            lp = Master_CEntry_List;
            break;
         case sgeE_CONFIG_LIST: 
            /* sge_get_configuration() returns a copy already, we do not need to make 
               one later */
            copy_lp = sge_get_configuration();
            break;
         case sgeE_EXECHOST_LIST:
            lp = Master_Exechost_List;
            break;
         case sgeE_JOB_LIST:
            lp = Master_Job_List;
            break;
         case sgeE_JOB_SCHEDD_INFO_LIST:
            lp = Master_Job_Schedd_Info_List;
            break;
         case sgeE_MANAGER_LIST:
            lp = Master_Manager_List;
            break;
         case sgeE_NEW_SHARETREE:
            lp = Master_Sharetree_List;
            break;
         case sgeE_OPERATOR_LIST:
            lp = Master_Operator_List;
            break;
         case sgeE_PE_LIST:
            lp = Master_Pe_List;
            break;
         case sgeE_PROJECT_LIST:
            lp = Master_Project_List;
            break;
         case sgeE_CQUEUE_LIST:
            lp = *(object_type_get_master_list(SGE_TYPE_CQUEUE));
            break;
         case sgeE_SCHED_CONF:
            lp = *sconf_get_config_list();
            break;
         case sgeE_SUBMITHOST_LIST:
            lp = Master_Submithost_List;
            break;
         case sgeE_USER_LIST:
            lp = Master_User_List;
            break;
         case sgeE_USERSET_LIST:
            lp = Master_Userset_List;
            break;
         case sgeE_HGROUP_LIST:
            lp = Master_HGroup_List;
            break;
#ifndef __SGE_NO_USERMAPPING__
         case sgeE_CUSER_LIST:
            lp = Master_Cuser_List;
            break;
#endif
         default:
            WARNING((SGE_EVENT, MSG_EVE_TOTALUPDATENOTHANDLINGEVENT_I, type));
            DEXIT;
            return;
      } /* switch */

      if (lp != NULL) {
         copy_lp = lCopyListHash(lGetListName(lp), lp, false);
      }
      /* 'send_events()' will free the copy of 'lp' */
      add_list_event_for_client (id, 0, type, 0, 0, NULL, NULL, NULL, 
                                 copy_lp, false);

      lock_client (id, true);
   } /* if */

   DEXIT;
   return;
} /* total_update_event() */


/****** evm/sge_event_master/list_select() *************************************
*  NAME
*     list_select() -- makes a reduced job list dublication 
*
*  SYNOPSIS
*     static bool list_select(subscription_t *subscription, int type, lList 
*     **reduced_lp, lList *lp, const lCondition *selection, const lEnumeration 
*     *fields, const lDescr *descr) 
*
*  FUNCTION
*     Only works on job events. All others are ignored. The job events
*     need some special handling and this is done in this function. The
*     JAT_Type list can be subscribed by its self and it is also part
*     of the JB_Type. If a JAT_Type filter is set, this function also
*     filters the JAT_Type lists in the JB_Type lists.
*
*  INPUTS
*     subscription_t *subscription - subscription array 
*     int type                     - event type 
*     lList **reduced_lp           - target list (has to be an empty list) 
*     lList *lp                    - source list (will be modified) 
*     const lCondition *selection  - where filter 
*     const lEnumeration *fields   - what filter 
*     const lDescr *descr          - reduced descriptor 
*
*  RESULT
*     static bool - true, if it was a job event 
*
*******************************************************************************/
static bool list_select(subscription_t *subscription, int type,
                        lList **reduced_lp, lList *lp,
                        const lCondition *selection, const lEnumeration *fields,
                        const lDescr *descr)
{
   bool ret = false;
   int entry_counter;
   int event_counter;

   DENTER(TOP_LAYER, "list_select");
   
   for (entry_counter = 0; entry_counter < LIST_MAX; entry_counter++) {
      event_counter = -1;
      while (EVENT_LIST[entry_counter][++event_counter] != -1){
         if (type == EVENT_LIST[entry_counter][event_counter]) {
            int sub_type = -1;
            int i=-1;

            while (SOURCE_LIST[entry_counter][++i] != -1) {
               if (subscription[SOURCE_LIST[entry_counter][i]].what){
                  sub_type = SOURCE_LIST[entry_counter][i];
                  break;
               }
            }
  
            if (sub_type != -1) {
               lListElem *element = NULL;
               lListElem *reduced_el = NULL;
               
               ret = true;
               *reduced_lp = lCreateListHash("update", descr, false);        
               
               for_each(element, lp) {
                  reduced_el = elem_select(subscription, element, 
                               FIELD_LIST[entry_counter], selection, 
                               fields, descr, sub_type);
               
                  lAppendElem(*reduced_lp, reduced_el);
               }
            }
            else {
               DPRINTF(("no sub type filter specified\n"));
            }
            goto end;
         } /* end if */
      } /* end while */
   } /* end for */
end:   
   DEXIT;
   return ret;        
}

/****** sge_event_master/elem_select() ******************************************
*  NAME
*     elem_select() -- makes a reduced copy of an element with reducing sublists
*                      as well
*
*  SYNOPSIS
*     static lListElem *elem_select(subscription_t *subscription, lListElem *element, 
*                              const int ids[], const lCondition *selection, 
*                              const lEnumeration *fields, const lDescr *dp, int sub_type)
*
*  FUNCTION
*     The function will apply the given filters for the element. Before the element
*     is reduced, all attribute sub lists named in "ids" will be removed from the list and
*     reduced. The reduced sub lists will be added the the reduced element and the original
*     element will be restored. The sub-lists will only be reduced, if the reduced element
*     still contains their attributes.
*
*  INPUTS
*     subscription_t *subscription - subscription array 
*     lListElem *element           - the element to reduce
*     const int ids[]              - attribute with sublists to be reduced as well
*     const lCondition *selection  - where filter 
*     const lEnumeration *fields   - what filter 
*     const lDescr *descr          - reduced descriptor 
*     int sub_type                 - list type of the sublists.
*
*  RESULT
*     bool - the reduced element, or NULL if something went wrong
*
*  NOTE:
*  MT-NOTE: works only on the variables -> thread save
*
*******************************************************************************/
static lListElem *elem_select(subscription_t *subscription, lListElem *element, 
                              const int ids[], const lCondition *selection, 
                              const lEnumeration *fields, const lDescr *dp, int sub_type) {
   const lCondition *sub_selection = NULL; 
   const lEnumeration *sub_fields = NULL;
   const lDescr *sub_descr = NULL;
   lList **sub_list;
   lListElem *el = NULL;
   int counter;
 
   DENTER(TOP_LAYER, "elem_select");
 
   if (!element) {
      DEXIT;
      return NULL;
   }
 
   if (sub_type <= sgeE_ALL_EVENTS || sub_type >= sgeE_EVENTSIZE){
      
      /* TODO: SG: add error message */
      DPRINTF(("wrong event sub type\n"));
      DEXIT;
      return NULL;
   }
   
   /* get the filters for the sub lists */
   if (sub_type>=0){
      sub_selection = subscription[sub_type].where;
      sub_fields = subscription[sub_type].what; 
   }
  
   if (sub_fields){ /* do we have a sub list filter, otherwise ... */
      int ids_size = 0;   

      /* allocate memory to store the sub-lists, which should be handeled special */
      while (ids[ids_size] != -1)
         ids_size++;
      sub_list = malloc(ids_size * sizeof(lList*));
      memset(sub_list, 0 , ids_size * sizeof(lList*));
      
      /* remove the sub-lists from the main element */
      for(counter = 0; counter < ids_size; counter ++){
         lXchgList(element, ids[counter], &(sub_list[counter]));
      } 
      
      /* get descriptor for reduced sub-lists */
      if (!sub_descr){
         for(counter = 0; counter < ids_size; counter ++){
            if (sub_list[counter]){
               sub_descr = getDescriptorL(subscription, sub_list[counter], sub_type);
               break;
            }
         } 
      }

      /* copy the main list */
      if (!fields) {
         /* there might be no filter for the main element, but for the sub-lists */
         el = lCopyElemHash(element, false);
      }
      else if (!dp) {
         /* for some reason, we did not get a descriptor for the target element */
         el = lSelectElem(element, selection, fields, false);
      }
      else {
         el = lSelectElemD(element, selection, dp, fields, false);
      }

      /* if we have a new reduced main element */
      if (el) {
         /* copy the sub-lists, if they are still part of the reduced main element */
         for(counter = 0; counter < ids_size; counter ++) {
            if (sub_list[counter] && (lGetPosViaElem(el, ids[counter]) != -1)) {
               lSetList(el, ids[counter],
                        lSelectD("", sub_list[counter], sub_selection,
                                 sub_descr, sub_fields, false));
            }            
         } 
      }
      
      /* restore the old sub_list */
      for(counter = 0; counter < ids_size; counter ++){
         lXchgList(element, ids[counter], &(sub_list[counter]));
      } 

      FREE(sub_list);
   }
   /* .... do a simple select */
   else {
      DPRINTF(("no sub filter specified\n"));
      el = lSelectElemD(element, selection, dp, fields, false);
   }   

   DEXIT;
   return el;
}

/****** Eventclient/Server/eventclient_list_locate() **************************
*  NAME
*     eventclient_list_locate_by_adress() -- search event client by adress
*
*  SYNOPSIS
*     #include "sge_event_master.h"
*
*     lListElem *
*     eventclient_list_locate_by_adress(const char *host, 
*                     const char *commproc, u_long32 id) 
*
*  FUNCTION
*     Searches the event client list for an event client with the
*     specified commlib adress.
*     Returns a pointer to the event client object or
*     NULL, if no such event client is registered.
*
*  INPUTS
*     const char *host     - hostname of the event client to search
*     const char *commproc - commproc of the event client to search
*     u_long32 id          - id of the event client to search
*
*  RESULT
*     lListElem* - event client object or NULL.
*
*  NOTES
*
*******************************************************************************/
static lListElem *
eventclient_list_locate_by_adress(const char *host, const char *commproc,
                                  u_long32 id)
{
   lListElem *ep;

   DENTER(TOP_LAYER, "eventclient_list_locate_by_adress");

   for_each (ep, Master_Control.clients) {
      if (lGetUlong(ep, EV_commid) == id &&
          !sge_hostcmp(lGetHost(ep, EV_host), host) &&
          !strcmp(lGetString(ep, EV_commproc), commproc)) {
         break;
      }
   }

   DEXIT;
   return ep;
}

/****** sge_event_master/getDescriptorL() **************************************
*  NAME
*     getDescriptorL() -- returns a reduced desciptor 
*
*  SYNOPSIS
*     static const lDescr* getDescriptorL(subscription_t *subscription, const 
*     lList* list, int type) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     subscription_t *subscription - subscription array 
*     const lList* list            - source list 
*     int type                     - event type 
*
*  RESULT
*     static const lDescr* - reduced descriptor or NULL, if no what exists 
*
*  NOTE
*   MT-NOTE: thread save, works only on the submitted variables.
*******************************************************************************/
static const lDescr* getDescriptorL(subscription_t *subscription,
                                    const lList* list, int type){
   const lDescr *dp = NULL;
   
   if (subscription[type].what) {
      if (!(dp = subscription[type].descr)) {
         subscription[type].descr = lGetReducedDescr(lGetListDescr(list),
                                                     subscription[type].what );
         dp = subscription[type].descr; 
      }
   }
   
   return dp;
}

/****** sge_event_master/lock_client_protected() *******************************
*  NAME
*     lock_client_protected() -- locks the client without aquiring the mutex
*
*  SYNOPSIS
*     static void lock_client_protected(u_long32 id)
*
*  FUNCTION
*     Locks the client with the given id without trying to lock the mutex first.
*     The calling thread is assumed to hold the Master_Control.mutex.
*
*  INPUTS
*     u_long32 id - the client id
*
*  NOTE
*     MT-NOTE: NOT thread safe.  Requires caller to hold Master_Control.mutex.
*******************************************************************************/
static void lock_client_protected(u_long32 id)
{
   DENTER(TOP_LAYER, "lock_client_protected");
/* DEBUG */
DPRINTF (("lock_client_protected(%ld)\n", id));

/* DEBUG */
#if 1
if (id < 1) {
   abort();
}
#endif
   
   if ((id < 1) ||
       (id > Master_Control.max_event_clients + EV_ID_FIRST_DYNAMIC - 1)) {
      return;
   }
   
   sge_bitfield_set (Master_Control.lockfield, (int)id -1);
   DEXIT;
}

/****** sge_event_master/lock_client() *****************************************
*  NAME
*     lock_client() -- locks the client
*
*  SYNOPSIS
*     static bool lock_client(u_long32 id, bool wait)
*
*  FUNCTION
*     Locks the client with the given id.
*
*  INPUTS
*     u_long32 id - the client id
*     bool wait   - whether to block while locking the client if the client is
*                   already locked by another thread
*
*  RESULT
*     True if the client is now locked, false if not.  False can only be
*     returned if wait is false and the client is already locked.
*
*  NOTE
*     MT-NOTE: thread safe.
*******************************************************************************/
static bool lock_client(u_long32 id, bool wait)
{
   DENTER(TOP_LAYER, "lock_client");
   
/* DEBUG */
#if 0 
DPRINTF (("lock_client(%ld, %d)\n", id, wait));

if (id < 1) {
   DPRINTF(("Client ids less than 1 are not allowed!\n"));
   abort();
}
#endif
   
   if ((id < 1) ||
       (id > Master_Control.max_event_clients + EV_ID_FIRST_DYNAMIC - 1)) {
      return false;
   }
   
   sge_mutex_lock("event_master_mutex", "lock_client", __LINE__, &Master_Control.mutex);
   
   /* To keep from degenerating into a spin lock when all clients are locked,
    * we sleep.  We just reuse the same cond var that lock all uses.  We will
    * wake up at the same time as the lock all, but our guardian will make sure
    * that we will go right back to sleep.  This is perhaps not the world's most
    * scalable solution, but for the number of threads that are likely to be
    * mucking around in the event master simultaneously, it is good enough. */
   /* We block on a lock_all even if wait if false because otherwise my loops
    * turns into spinlocks when lock_all is set. */
   while (Master_Control.lock_all ||
          (wait &&
           sge_bitfield_get (Master_Control.lockfield, (int)id - 1) )) {
      if (Master_Control.lock_all) {
         /* This uses the lockfield cond var because it will be much less trafficy
          * than the waitfield cond var in this case. */
         pthread_cond_wait (&Master_Control.lockfield_cond_var,
                            &Master_Control.mutex);
      }
      else {
         /* We will be awakened when a lock is freed.  Since there will only be
          * a couple of threads, the odds are good that it will be the lock we
          * want. */
         pthread_cond_wait (&Master_Control.waitfield_cond_var,
                            &Master_Control.mutex);
      }
   }

   /* If wait is true, this client is guaranteed to be unlocked at thi
    * point. */
   if (sge_bitfield_get (Master_Control.lockfield, (int)id - 1)) {

      sge_mutex_unlock("event_master_mutex", "lock_client", __LINE__, &Master_Control.mutex);
      DEXIT;
      return false;
   }
   
   sge_bitfield_set (Master_Control.lockfield, (int)id - 1);   
   sge_mutex_unlock("event_master_mutex", "lock_client", __LINE__, &Master_Control.mutex);
   
   DEXIT;
   return true;
}

/****** sge_event_master/unlock_client() ***************************************
*  NAME
*     unlock_client() -- unlocks the client
*
*  SYNOPSIS
*     static void unlock_client(u_long32 id)
*
*  FUNCTION
*     Unlocks the client with the given id.
*
*  INPUTS
*     u_long32 id - the client id
*
*  NOTE
*     MT-NOTE: thread safe.
*******************************************************************************/
static void unlock_client(u_long32 id)
{
   DENTER(TOP_LAYER, "unlock_client");

   if ((id < 1) ||
       (id > Master_Control.max_event_clients + EV_ID_FIRST_DYNAMIC - 1)) {
      return;
   }
   
   sge_mutex_lock("event_master_mutex", "unlock_client", __LINE__,
                  &Master_Control.mutex);
   
   sge_bitfield_clear (Master_Control.lockfield, (int)id - 1);
   
   if (Master_Control.lock_all &&
       (!sge_bitfield_changed (Master_Control.lockfield))) {
      /* This will wake up everyone who wants a lock.  Only the lock all will
       * get it.  All the others will just go back to sleep. */
      pthread_cond_broadcast (&Master_Control.lockfield_cond_var);
   }
   
   /* This will wake up everyone who wants a lock.  Only the thread waiting
    * for this lock will get it.  All the others will just go back to
    * sleep. */
   pthread_cond_broadcast (&Master_Control.waitfield_cond_var);
   
   sge_mutex_unlock("event_master_mutex", "unlock_client", __LINE__, &Master_Control.mutex);
   DEXIT;
}

/****** sge_event_master/lock_all_clients() ************************************
*  NAME
*     lock_all_clients() -- locks all clients
*
*  SYNOPSIS
*     static void lock_all_clients(void)
*
*  FUNCTION
*     Locks all clients by grabbing a global lock.  When this method returns,
*     the calling thread will hold the Master_Control.mutex.  If any client is
*     locked when this method is called, the calling thread will block until all
*     client locks are free.  Once this method has been called, all subsequent
*     calls to lock_client() will block until this method has returned, even if
*     this method is still waiting to acquire the global lock.
*
*  NOTE
*     MT-NOTE: thread safe.
*******************************************************************************/
static void lock_all_clients(void)
{
   DENTER(TOP_LAYER, "lock_all_clients");

   sge_mutex_lock("event_master_mutex", "lock_all_clients", __LINE__, &Master_Control.mutex);

   /* Block for other lock_alls. */
   while (Master_Control.lock_all) {
      pthread_cond_wait (&Master_Control.lockfield_cond_var,
                         &Master_Control.mutex);
   }
   
   /* We don't have to worry about contention for this variable because it is
    * protected by the mutex. */
   Master_Control.lock_all = true;
   
   /* If there are locks in use, sleep while the drain off. */
   while (sge_bitfield_changed (Master_Control.lockfield)) {
      /* We will be awakened when all locks are free. */
      pthread_cond_wait (&Master_Control.lockfield_cond_var, &Master_Control.mutex);
   }
   
   DEXIT;
}

/****** sge_event_master/unlock_all_clients() **********************************
*  NAME
*     unlock_all_clients() -- unlocks all clients
*
*  SYNOPSIS
*     static void unlock_all_clients(void)
*
*  FUNCTION
*     Unlocks all clients.  Any blocked calls to lock_client() will be awakened.
*
*  NOTE
*     MT-NOTE: thread safe.
*******************************************************************************/
static void unlock_all_clients(void)
{
   DENTER(TOP_LAYER, "unlock_all_clients");

   Master_Control.lock_all = false;

   /* Always signal when releasing this lock so that anyone else who wants a
    * lock knows that they are available again. */
   pthread_cond_broadcast (&Master_Control.lockfield_cond_var);
   
   sge_mutex_unlock("event_master_mutex", "unlock_all_clients", __LINE__, &Master_Control.mutex);
   
   DEXIT;
}

/****** sge_event_master/get_event_client() ************************************
*  NAME
*     get_event_client() -- gets the event client from the list
*
*  SYNOPSIS
*     static lListElem *get_event_client(u_long32 id)
*
*  FUNCTION
*     Returns the event client with the given id, or NULL if no such event
*     client exists.
*
*  INPUTS
*     u_long32 id - the client id
*
*  RESULT
*     The event client with the given id, or NULL if no such event client
*     exists.
*
*  NOTE
*     MT-NOTE: NOT thread safe.  Requires caller to hold Master_Control.mutex.
*******************************************************************************/
static lListElem *get_event_client(u_long32 id)
{
/* DEBUG */
#if 0
DENTER(TOP_LAYER, "get_event_client");
DPRINTF (("get_event_client(%ld)\n", id));
#endif

/* DEBUG */
#if 1
if (id < 1) {
   abort();
}
#endif
   
/* DEBUG */
#if 0
DPRINTF (("value: %x\n", Master_Control.clients_array[(int)id - 1]));
DEXIT;
#endif

   if ((id < 1) ||
       (id > Master_Control.max_event_clients + EV_ID_FIRST_DYNAMIC - 1)) {
      return NULL;
   }

   return Master_Control.clients_array[(int)id - 1];
}

/****** sge_event_master/set_event_client() ************************************
*  NAME
*     set_event_client() -- sets the event client in the list
*
*  SYNOPSIS
*     static void set_event_client(u_long32 id, lListElem *client)
*
*  FUNCTION
*     Sets the value of the event client with the given id.
*
*  INPUTS
*     u_long32 id       - the client id
*     lListElem *client - the value for the event client
*
*  NOTE
*     MT-NOTE: NOT thread safe.  Requires caller to hold Master_Control.mutex.
*******************************************************************************/
static void set_event_client(u_long32 id, lListElem *client)
{
/* DEBUG */
#if 0
DENTER(TOP_LAYER, "set_event_client");
DPRINTF (("set_event_client(%ld, %x)\n", id, client));
#endif

/* DEBUG */
#if 1
if (id < 1) {
   abort();
}
#endif
   
   if ((id < 1) ||
       (id > Master_Control.max_event_clients + EV_ID_FIRST_DYNAMIC - 1)) {
      return;
   }
   
   
   Master_Control.clients_array[(int)id - 1] = client;
#if 0
DEXIT;
#endif
}

/****** sge_event_master/assign_new_dynamic_id() *******************************
*  NAME
*     assign_new_dynamic_id() -- gets a new dynamic id
*
*  SYNOPSIS
*     static u_long32 assign_new_dynamic_id (void)
*
*  FUNCTION
*     Returns the next available dynamic event client id.  The id returned will
*     be between EV_ID_FIRST_DYNAMIC and Master_Control.max_event_clients +
*     EV_ID_FIRST_DYNAMIC.
*
*  RESULTS
*     The next available dynamic event client id.
*
*  NOTE
*     MT-NOTE: NOT thread safe.  Requires caller to hold Master_Control.mutex.
*******************************************************************************/
static u_long32 assign_new_dynamic_id (void)
{
   int count = 0;
   
   DENTER(TOP_LAYER, "assign_new_dynamic_id");
/* DEBUG */
#if 1
DPRINTF (("assign_new_dynamic_id()\n"));
#endif
   /* Only look as far as the current max dynamic client, even though the
    * array may be longer. */
   for (count = EV_ID_FIRST_DYNAMIC - 1;
        count < Master_Control.max_event_clients + EV_ID_FIRST_DYNAMIC - 1;
        count++) {
      if (Master_Control.clients_array[count] == NULL) {
/* DEBUG */
#if 1
DPRINTF (("value: %d\n", count + 1));
#endif
         /* The id is offset from the array by 1 */
         DEXIT;
         return (u_long32)(count + 1);
      }
   }
   
/* DEBUG */
#if 1
DPRINTF (("value: 0\n"));
#endif
   DEXIT;
   /* If there are no free ids, return 0 */
   return (u_long32)0;
}

/****** sge_event_master/sge_commit() ****************************************
*  NAME
*     sge_commit() -- Commit the queued events
*
*  SYNOPSIS
*     bool sge_commit (void)
*
*  FUNCTION
*     Sends any events that this thread currently has queued and clears the
*     queue.
*
*  RESULTS
*     Whether the call succeeded.  It will return false only if
*     sge_is_commit_required() returns false.
*
*  NOTE
*     MT-NOTE: sge_commit is thread safe.
*******************************************************************************/
bool sge_commit (void)
{
   bool ret = false;

   DENTER (TOP_LAYER, "sge_commit");
   
   pthread_once(&Event_Master_Once, event_master_once_init);

   sge_mutex_lock("event_master_t_add_event_mutex", SGE_FUNC, __LINE__, &Master_Control.t_add_event_mutex);
   
   if (Master_Control.is_transaction) {
   
   /* we do not need to evaluate the flushing, we just trigger, when ever there is one event. */
#if 0   
      bool flush = false; 
#endif     
      Master_Control.is_transaction = false;
      
      /* We don't have to worry about holding a lock while we work on this list
       * because the list is thread specific. */

      if (Master_Control.transaction_events != NULL) {
#if 0      
         lListElem *ep = lFirst (Master_Control.transaction_events);

         /* Do this math here before we destroy qlp, and while we're not holding
          * the lock. */
         while (!flush && (ep != NULL)) {
            /* Because we're in control of how events are added, we know that
             * event EV_events list will contain exactly one ET_Type.  If this
             * changes in the future, this line will need to become a loop. */
            /* We do this without first getting a lock because this method may
             * not need to aquire the lock.  By letting this method get the lock
             * if it's needed, we may save some locking and unlocking. */
            flush |= should_flush_event_client(lGetUlong (ep, EV_id),
                                               lGetUlong (lFirst (lGetList (ep, EV_events)), ET_type), 
                                               false);
            
            ep = lNext(ep);
         }
#endif
         sge_mutex_lock("event_master_send_mutex", SGE_FUNC, __LINE__, &Master_Control.send_mutex);
         lAppendList (Master_Control.send_events, Master_Control.transaction_events);
         sge_mutex_unlock("event_master_send_mutex", SGE_FUNC, __LINE__, &Master_Control.send_mutex);
#if 0         
         if (flush) {
            set_flush();
         }
#endif         

         set_flush();
      }
      ret = true;
   }
   sge_mutex_unlock("event_master_t_add_event_mutex", SGE_FUNC, __LINE__, &Master_Control.t_add_event_mutex);
  
   if (ret){
      sge_mutex_unlock("event_master_transaction_mutex", SGE_FUNC, __LINE__, &Master_Control.transaction_mutex);
   }
   
   DEXIT;
   return ret;
  
}

/****** sge_event_master/blockEvents() ****************************************
*  NAME
*     blockEvents() -- blocks or unblocks events
*
*  SYNOPSIS
*     void blockEvents(lListElem *event_client, int ev_type, bool isBlock) 
*
*  FUNCTION
*     In case that global update events have to be send, this function
*     blocks all events for that list, or unblocks it.
*     
*  INPUT
*    lListElem *event_client : event client to modify
*    ev_event ev_type : the event_lists to unblock or -1
*    bool isBlock : true: block events, false: unblock
*
*  NOTE
*     MT-NOTE: sge_commit is thread safe.
*******************************************************************************/
static void blockEvents(lListElem *event_client, ev_event ev_type, bool isBlock) { 
                                       
   subscription_t *sub_array = lGetRef(event_client, EV_sub_array); 

   if (sub_array != NULL) {
      int i = -1;
      if (ev_type == sgeE_ALL_EVENTS) { /* block all subscribed events, for which are list events subscribed */
         while (total_update_events[++i] != -1) {
            if (sub_array[total_update_events[i]].subscription == EV_SUBSCRIBED) {
                int y = -1;
                while (block_events[i][++y] != -1) {
                  sub_array[block_events[i][y]].blocked = isBlock;
                }  
            }
         }
      }
      else {
         while (total_update_events[++i] != -1) {
            if (total_update_events[i] == ev_type) {
                int y = -1;
                while (block_events[i][++y] != -1) {
                  sub_array[block_events[i][y]].blocked = isBlock;
                }  
                  
               break;
            }
         }
      }
   }
}

/****** sge_event_master/set_flush() *******************************************
*  NAME
*     set_flush() -- Flush all events
*
*  SYNOPSIS
*     void set_flush (void)
*
*  FUNCTION
*     Flushes all pending events
*
*  NOTE
*     MT-NOTE: set_flush is thread safe.
*******************************************************************************/
static void set_flush (void)
{
   DENTER (TOP_LAYER, "set_flush");
   
   sge_mutex_lock("event_master_cond_mutex", SGE_FUNC, __LINE__, &Master_Control.cond_mutex);
   if (!Master_Control.delivery_signaled) { 
      Master_Control.delivery_signaled = true;
      pthread_cond_signal(&Master_Control.cond_var);
   }
   sge_mutex_unlock("event_master_cond_mutex", SGE_FUNC, __LINE__, &Master_Control.cond_mutex);
   
   DEXIT;
}

/****** sge_event_master/should_flush_event_client() ***************************
*  NAME
*     should_flush_event_client() -- Tests whether an event needs to be flushed
*
*  SYNOPSIS
*     static bool should_flush_event_client(lListElem *ec, ev_event type)
*
*  FUNCTION
*     Tests whether a given event type for a given event client should cause a
*     flush.  Will return false if the event client doesn't exist.
*
*  INPUTS
*     lListElem *ec  - the event client who will receive the event
*     ev_event type  - the event of the event being received
*
*  RESULTS
*     Whether the events requires a flush
*
*  NOTE
*     MT-NOTE: should_flush_event_client is NOT thread safe.  It expects the
*              caller to hold Master_Control.mutex.
*******************************************************************************/
#if 0
static bool should_flush_event_client(u_long32 id, ev_event type, bool has_lock)
{
   bool flush = false;
   subscription_t *subscription = NULL;
   
   if ((type == sgeE_QMASTER_GOES_DOWN) || (type == sgeE_SHUTDOWN)) {
      flush = true;
   }
   else if (id > EV_ID_ANY) {
      lListElem *ec = NULL;
      
      if (!has_lock) {
         /* If grabbing the lock fails, it's because this id is bad. */
         if (!lock_client (id, true)) {
            return false;
         }
      }

      ec = get_event_client (id);
      
      if (ec != NULL) {
         subscription = lGetRef(ec, EV_sub_array);

         flush = ((subscription[type].flush) &&
                  (subscription[type].flush_time == 0));
      }
      
      if (!has_lock) {
         unlock_client (id);
      }
   }
   
   return flush;
}
#endif

/****** sge_event_master/sge_set_commit_required() *****************************
*  NAME
*     sge_set_commit_required() -- Require commits (make multipe object changes atomic)
*
*  SYNOPSIS
*     void sge_set_commit_required ()
*
*  FUNCTION
*  Enables transactions on events. Sofar a rollback is not suported. It allows to acumulate
*  events, while multiple objects are modified and events are issued and to submit them as
*  one event package. There can only be one event session open at a time. The transaction_mutex
*  will block multiple calles to this method.
*  The method cannot be called recursivly, and sge_commit has to be called to close the transaction. 
*
*
*  NOTE
*     MT-NOTE: sge_set_commit_required is thread safe.  Transactional event
*     processing is handled for each thread individually.
*******************************************************************************/
void sge_set_commit_required () {

   DENTER(TOP_LAYER,"sge_set_commit_required");
   
   pthread_once(&Event_Master_Once, event_master_once_init);
   sge_mutex_lock("event_master_transaction_mutex", SGE_FUNC, __LINE__, &Master_Control.transaction_mutex);
   sge_mutex_lock("event_master_t_add_event_mutex", SGE_FUNC, __LINE__, &Master_Control.t_add_event_mutex); 
   Master_Control.is_transaction = true;
   sge_mutex_unlock("event_master_t_add_event_mutex", SGE_FUNC, __LINE__, &Master_Control.t_add_event_mutex);   
   
   DEXIT;
}

/****** sge_event_master/sge_is_commit_required() ******************************
*  NAME
*     sge_is_commit_required() -- Whether a commit is required
*
*  SYNOPSIS
*     bool sge_is_commit_required (void)
*
*  FUNCTION
*     Test whether transactional event processing is turned on.  If true, this
*     means that sge_commit() must be called in order to actually send events
*     queued with sge_add_[list_]event[_for_client]().
*
*  NOTE
*     MT-NOTE: sge_is_commit_required is thread safe.  Transactional event
*     processing is handled for each thread individually.
*******************************************************************************/
bool sge_is_commit_required (void) {
   bool ret = false;
  
   DENTER(TOP_LAYER,"sge_is_commit_required");
   
   pthread_once(&Event_Master_Once, event_master_once_init);
   sge_mutex_lock("event_master_t_add_event_mutex", SGE_FUNC, __LINE__, &Master_Control.t_add_event_mutex);   
   ret = Master_Control.is_transaction;
   sge_mutex_unlock("event_master_t_add_event_mutex", SGE_FUNC, __LINE__, &Master_Control.t_add_event_mutex);   

   DEXIT;
   return ret;
}
