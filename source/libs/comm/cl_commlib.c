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
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include "sge_arch.h"


#include "cl_commlib.h"
#include "cl_handle_list.h"
#include "cl_connection_list.h"
#include "cl_app_message_queue.h"
#include "cl_message_list.h"
#include "cl_host_list.h"
#include "cl_endpoint_list.h"
#include "cl_application_error_list.h"
#include "cl_host_alias_list.h"
#include "cl_communication.h"
#include "cl_tcp_framework.h"
#include "cl_ssl_framework.h"
#include "cl_util.h"

#include "msg_commlib.h"

/* enable more commlib logging by definening CL_DO_COMMLIB_DEBUG at compile time */

/* the next switch is used to send ack for messages when they arive
   at commlib layer or when application removes them from commlib */
#if 1
#define CL_DO_SEND_ACK_AT_COMMLIB_LAYER /* send ack when message arives */ 
#endif

static void cl_com_default_application_debug_client_callback(int dc_connected, int debug_level);

static int cl_commlib_check_callback_functions(void);
static int cl_commlib_check_connection_count(cl_com_handle_t* handle);
static int cl_commlib_calculate_statistic(cl_com_handle_t* handle,  cl_bool_t force_update ,int lock_list);
static int cl_commlib_handle_debug_clients(cl_com_handle_t* handle, cl_bool_t lock_list);

static int cl_commlib_handle_connection_read(cl_com_connection_t* connection);
static int cl_commlib_handle_connection_write(cl_com_connection_t* connection);
static int cl_commlib_finish_request_completeness(cl_com_connection_t* connection);

static int cl_commlib_handle_connection_ack_timeouts(cl_com_connection_t* connection);
static int cl_commlib_send_ack_message(cl_com_connection_t* connection, cl_com_message_t* message);
static int cl_commlib_send_ccm_message(cl_com_connection_t* connection);
static int cl_commlib_send_ccrm_message(cl_com_connection_t* connection);
static int cl_commlib_send_sim_message(cl_com_connection_t* connection, unsigned long* mid);
static int cl_commlib_send_sirm_message(cl_com_connection_t* connection,
                                        cl_com_message_t* message,     /* message id of SIM */
                                        unsigned long        starttime,
                                        unsigned long        runtime,
                                        unsigned long        buffered_read_messages,
                                        unsigned long        buffered_write_messages,
                                        unsigned long        connection_count,
                                        unsigned long application_status,
                                        char*                infotext);
static int cl_com_trigger(cl_com_handle_t* handle, int synchron);

/* threads functions */
static void *cl_com_trigger_thread(void *t_conf);
static void *cl_com_handle_service_thread(void *t_conf);
static void *cl_com_handle_write_thread(void *t_conf);
static void *cl_com_handle_read_thread(void *t_conf);



static int cl_commlib_send_message_to_endpoint(cl_com_handle_t*   handle,
                                               cl_com_endpoint_t* endpoint, 
                                               cl_xml_ack_type_t  ack_type, 
                                               cl_byte_t*         data, 
                                               unsigned long      size , 
                                               unsigned long      response_mid,
                                               unsigned long      tag);

/* global lists */

/* cl_com_handle_list
 * ==================
 *
 * Each entry in this list is a service handler connection */
static pthread_mutex_t cl_com_handle_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static cl_raw_list_t* cl_com_handle_list = NULL;


/* cl_com_log_list
 * ===============
 *
 * Each entry in this list can be logged via cl_log_list_flush() 
 *
 * also used for setting cl_commlib_debug_resolvable_hosts and cl_commlib_debug_unresolvable_hosts
 */
static pthread_mutex_t cl_com_log_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static cl_raw_list_t* cl_com_log_list = NULL;
static char* cl_commlib_debug_resolvable_hosts = NULL;
static char* cl_commlib_debug_unresolvable_hosts = NULL;

/* cl_com_host_list
 * ================
 *
 * Each entry in this list is a cached cl_get_hostbyname() 
   or cl_gethostbyaddr() call */
static pthread_mutex_t cl_com_host_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static cl_raw_list_t* cl_com_host_list = NULL;

/* cl_com_endpoint_list
 * ====================
 *
 * Each entry in this list is a known endpoint */
static pthread_mutex_t cl_com_endpoint_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static cl_raw_list_t* cl_com_endpoint_list = NULL;

/* cl_com_thread_list
 * ================
 *
 * Each entry is a thread from cl_thread.c module
 */
static pthread_mutex_t cl_com_thread_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static cl_raw_list_t*  cl_com_thread_list = NULL;

/* cl_com_application_error_list
 * =============================
 *
 * Each entry is an error which has to provided to the application 
 *
 */
static pthread_mutex_t cl_com_application_error_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static cl_raw_list_t*  cl_com_application_error_list = NULL;



/* global flags/variables */

/* cl_com_create_threads
 * =====================
 *
 * If set to 0, no threads are used */
static cl_thread_mode_t       cl_com_create_threads = CL_NO_THREAD;


/* global application function pointer for statistic calculation */
/* see cl_commlib_calculate_statistic() */
static pthread_mutex_t cl_com_application_mutex = PTHREAD_MUTEX_INITIALIZER;
static cl_app_status_func_t   cl_com_application_status_func = NULL;

/* global application function pointer for errors */
/* JG: TODO: we don't need this mutex, just lock cl_com_application_error_list instead. */
static pthread_mutex_t cl_com_error_mutex = PTHREAD_MUTEX_INITIALIZER;
static cl_error_func_t   cl_com_error_status_func = NULL;

/* global application function pointer for getting tag id names */
static pthread_mutex_t cl_com_tag_name_mutex = PTHREAD_MUTEX_INITIALIZER;
static cl_tag_name_func_t   cl_com_tag_name_func = NULL;

/* global application function pointer for debug clients */
static pthread_mutex_t cl_com_debug_client_callback_func_mutex = PTHREAD_MUTEX_INITIALIZER;
static cl_app_debug_client_func_t cl_com_debug_client_callback_func = cl_com_default_application_debug_client_callback;

static pthread_mutex_t  cl_com_ssl_setup_mutex = PTHREAD_MUTEX_INITIALIZER;
static cl_ssl_setup_t*  cl_com_ssl_setup_config = NULL;


static int cl_message_list_append_send(cl_com_connection_t* c, cl_com_message_t* m, int l);
static int cl_message_list_remove_send(cl_com_connection_t* c, cl_com_message_t* m, int l);
static int cl_message_list_append_receive(cl_com_connection_t* c, cl_com_message_t* m, int l);
static int cl_message_list_remove_receive(cl_com_connection_t* c, cl_com_message_t* m, int l);

/*
 * Prevent these functions made inline by compiler. This is
 * necessary for Solaris 10 dtrace pid provider to work.
 */
#ifdef SOLARIS
#pragma no_inline(cl_message_list_append_send, cl_message_list_remove_send, cl_message_list_append_receive, cl_message_list_remove_receive)
#endif

static int cl_message_list_append_send(cl_com_connection_t* c, cl_com_message_t* m, int l)
{
   return cl_message_list_append_message(c->send_message_list, m, l);
}
static int cl_message_list_remove_send(cl_com_connection_t* c, cl_com_message_t* m, int l)
{
   return cl_message_list_remove_message(c->send_message_list, m, l);
}
static int cl_message_list_append_receive(cl_com_connection_t* c, cl_com_message_t* m, int l)
{
   return cl_message_list_append_message(c->received_message_list, m, l);
}
static int cl_message_list_remove_receive(cl_com_connection_t* c, cl_com_message_t* m, int l)
{
   return cl_message_list_remove_message(c->received_message_list, m, l);
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_get_host_list()"
cl_raw_list_t* cl_com_get_host_list(void) {
   cl_raw_list_t* host_list = NULL;
   pthread_mutex_lock(&cl_com_host_list_mutex);
   host_list = cl_com_host_list;
   pthread_mutex_unlock(&cl_com_host_list_mutex);
   return host_list;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_get_endpoint_list()"
cl_raw_list_t* cl_com_get_endpoint_list(void) {
   cl_raw_list_t* endpoint_list = NULL;
   pthread_mutex_lock(&cl_com_endpoint_list_mutex);
   endpoint_list = cl_com_endpoint_list;
   pthread_mutex_unlock(&cl_com_endpoint_list_mutex);
   return endpoint_list;
}



#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_get_log_list()"
cl_raw_list_t* cl_com_get_log_list(void) {
   cl_raw_list_t* log_list = NULL;
   pthread_mutex_lock(&cl_com_log_list_mutex);
   log_list = cl_com_log_list;
   pthread_mutex_unlock(&cl_com_log_list_mutex);
   return log_list;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_set_status_func()"
int cl_com_set_status_func(cl_app_status_func_t status_func) {
   pthread_mutex_lock(&cl_com_application_mutex);
   cl_com_application_status_func = *status_func;
   pthread_mutex_unlock(&cl_com_application_mutex);
   return CL_RETVAL_OK;
}



/* The only errors supported by error func calls are:
    CL_RETVAL_UNKNOWN:             when CRM contains unexpected error
    CL_RETVAL_ACCESS_DENIED:       when CRM says that access to service is denied 
    CL_CRM_CS_ENDPOINT_NOT_UNIQUE: when CRM says that there is already an endpoint with this id connected */
#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_set_error_func()"
int cl_com_set_error_func(cl_error_func_t error_func) {
   pthread_mutex_lock(&cl_com_error_mutex);
   cl_com_error_status_func = *error_func;
   pthread_mutex_unlock(&cl_com_error_mutex);
   return CL_RETVAL_OK;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_set_tag_name_func()"
int cl_com_set_tag_name_func(cl_tag_name_func_t tag_name_func) {
   pthread_mutex_lock(&cl_com_tag_name_mutex);
   cl_com_tag_name_func = *tag_name_func;
   pthread_mutex_unlock(&cl_com_tag_name_mutex);
   return CL_RETVAL_OK;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_setup_callback_functions()"
int cl_com_setup_callback_functions(cl_com_connection_t* connection) {
   if (connection == NULL) {
      return CL_RETVAL_PARAMS;
   }

   /* set global error function from application */
   pthread_mutex_lock(&cl_com_error_mutex);
   connection->error_func = cl_com_error_status_func;
   pthread_mutex_unlock(&cl_com_error_mutex);

   /* set global tag name function from application */
   pthread_mutex_lock(&cl_com_tag_name_mutex);
   connection->tag_name_func = cl_com_tag_name_func;
   pthread_mutex_unlock(&cl_com_tag_name_mutex);

   return CL_RETVAL_OK;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_push_application_error()"
int cl_commlib_push_application_error(cl_log_t cl_err_type, int cl_error, const char* cl_info_text) {
   const char* cl_info = cl_info_text;
   int retval = CL_RETVAL_OK; 

   if (cl_info == NULL) {
      cl_info = MSG_CL_COMMLIB_NO_ADDITIONAL_INFO;
      retval  = CL_RETVAL_PARAMS;
   }

   pthread_mutex_lock(&cl_com_error_mutex);
   if (cl_com_error_status_func != NULL) {
      CL_LOG_STR(CL_LOG_INFO,"add application error id: ", cl_get_error_text(cl_error));
      CL_LOG_STR(CL_LOG_INFO,"add application error: ", cl_info );
      cl_application_error_list_push_error(cl_com_application_error_list, cl_err_type, cl_error, cl_info, 1);
   } else {
      retval = CL_RETVAL_UNKNOWN;
      CL_LOG(CL_LOG_ERROR,"no application error function set" );
      CL_LOG_STR(CL_LOG_ERROR,"ignore application error id: ", cl_get_error_text(cl_error));
      CL_LOG_STR(CL_LOG_ERROR,"ignore application error: ", cl_info );
   }
   pthread_mutex_unlock(&cl_com_error_mutex);
   return retval;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_check_callback_functions()"
static int cl_commlib_check_callback_functions(void) {
   /* 
    * This function will call application callback functions in context
    * of application. This happens when application is calling 
    * cl_commlib_send_message(), cl_commlib_receive_message() and
    * cl_com_cleanup_commlib(), because
    * this is the only chance to be in the application context (thread).
    *
    */
    cl_thread_settings_t* actual_thread = NULL;
    cl_bool_t is_commlib_thread = CL_FALSE;

    switch(cl_com_create_threads) {
       case CL_NO_THREAD:
          break;
       default:
          actual_thread = cl_thread_get_thread_config();
          if (actual_thread != NULL) {
             if (actual_thread->thread_pointer != NULL) {
                is_commlib_thread = CL_TRUE;
             }
          }
          break;
    }

    /* check if caller is application (thread) */
    if ( is_commlib_thread == CL_FALSE ) {
       /* here we are in application context, we can call trigger functions */
       cl_application_error_list_elem_t* elem = NULL;
       pthread_mutex_lock(&cl_com_error_mutex);
       cl_raw_list_lock(cl_com_application_error_list);
       while( (elem = cl_application_error_list_get_first_elem(cl_com_application_error_list)) != NULL ) {
          cl_raw_list_remove_elem(cl_com_application_error_list, elem->raw_elem);

          /* now trigger application error func */
          if (cl_com_error_status_func != NULL) {
             CL_LOG(CL_LOG_INFO,"triggering application error function");
             cl_com_error_status_func(elem);
          } else {
             CL_LOG(CL_LOG_WARNING,"can't trigger application error function: no function set");
          }

          free(elem->cl_info);
          free(elem);
          elem = NULL;
       }
       cl_raw_list_unlock(cl_com_application_error_list);
       pthread_mutex_unlock(&cl_com_error_mutex);
    }
    

    return CL_RETVAL_OK;
}


/****** cl_commlib/cl_com_setup_commlib_complete() *****************************
*  NAME
*     cl_com_setup_commlib_complete() -- check whether commlib setup was called
*
*  SYNOPSIS
*     cl_bool_t cl_com_setup_commlib_complete(void) 
*
*  FUNCTION
*     This function returns CL_TRUE when cl_com_setup_commlib() was called
*     at least one time.
*
*  RESULT
*     cl_bool_t - CL_TRUE:  cl_com_setup_commlib() was done
*                 CL_FALSE: There was no commlib setup till now
*
*  NOTES
*     MT-NOTE: cl_com_setup_commlib_complete() is MT safe 
*
*  SEE ALSO
*     cl_commlib/cl_com_setup_commlib()
*******************************************************************************/
#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_setup_commlib_complete()"
cl_bool_t cl_com_setup_commlib_complete(void) {
   cl_bool_t setup_complete = CL_FALSE;

   pthread_mutex_lock(&cl_com_log_list_mutex);
   if (cl_com_log_list != NULL) {
      setup_complete = CL_TRUE;
   }
   pthread_mutex_unlock(&cl_com_log_list_mutex);
   return setup_complete;
}

/* the cl_com_get_(un)resolveable_hosts functions don't need a mutex,
 * because the memory is malloced in cl_com_setup_commlib()
 * and freed in cl_com_cleanup_commlib()
 */
char* cl_com_get_resolvable_hosts(void) {
   return cl_commlib_debug_resolvable_hosts;
}

char* cl_com_get_unresolvable_hosts(void) {
   return cl_commlib_debug_unresolvable_hosts;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_setup_commlib()"
int cl_com_setup_commlib(cl_thread_mode_t t_mode, cl_log_t debug_level, cl_log_func_t flush_func)
{
   int ret_val = CL_RETVAL_OK;
   cl_thread_settings_t* thread_p = NULL;
   cl_bool_t duplicate_call = CL_FALSE;
   cl_bool_t different_thread_mode = CL_FALSE;
   char* help = NULL;

   /* setup global log list */
   pthread_mutex_lock(&cl_com_log_list_mutex);
   help = getenv("SGE_COMMLIB_DEBUG_RESOLVE");
   if (help != NULL) {
      if (cl_commlib_debug_resolvable_hosts == NULL) {
         cl_commlib_debug_resolvable_hosts = strdup(help);
      }
   }
   help = getenv("SGE_COMMLIB_DEBUG_NO_RESOLVE");
   if (help != NULL) {
      if (cl_commlib_debug_unresolvable_hosts == NULL) {
         cl_commlib_debug_unresolvable_hosts = strdup(help);
      }
   }

   if (cl_com_log_list != NULL) {
      duplicate_call = CL_TRUE;
      if (cl_com_handle_list != NULL) {
         if (cl_raw_list_get_elem_count(cl_com_handle_list) > 0) {
            if (cl_com_create_threads != t_mode) {
               different_thread_mode = CL_TRUE;
            }
         }
      }
   }

   if (cl_com_log_list == NULL) {
#ifdef CL_DO_COMMLIB_DEBUG
      ret_val = cl_log_list_setup(&cl_com_log_list, "main", 0, /* CL_LOG_FLUSHED */ CL_LOG_IMMEDIATE  , NULL ); 
#else
      ret_val = cl_log_list_setup(&cl_com_log_list, "main", 0, /* CL_LOG_FLUSHED */ CL_LOG_IMMEDIATE  , flush_func); 
#endif
      if (cl_com_log_list == NULL) {
         pthread_mutex_unlock(&cl_com_log_list_mutex);
         cl_com_cleanup_commlib();
         return ret_val;
      }
   }
   pthread_mutex_unlock(&cl_com_log_list_mutex);
   cl_log_list_set_log_level(cl_com_log_list, debug_level );

   if (duplicate_call == CL_TRUE) {
      CL_LOG(CL_LOG_WARNING,"duplicate call to cl_com_setup_commlib()");
   }
 
   if (different_thread_mode == CL_TRUE) {
      CL_LOG(CL_LOG_ERROR,"duplicate call to cl_com_setup_commlib() with different thread mode");
      cl_commlib_push_application_error(CL_LOG_ERROR, CL_RETVAL_COMMLIB_SETUP_ALREADY_CALLED, MSG_CL_COMMLIB_CANT_SWITCH_THREAD_MODE_WITH_EXISTING_HANDLES);
   } else {
      cl_com_create_threads = t_mode;
   }

   /* setup global application error list */
   pthread_mutex_lock(&cl_com_application_error_list_mutex);
   if (cl_com_application_error_list == NULL) {
      ret_val = cl_application_error_list_setup(&cl_com_application_error_list, "application errors");
      if (cl_com_application_error_list == NULL) {
         pthread_mutex_unlock(&cl_com_application_error_list_mutex);
         cl_com_cleanup_commlib();
         return ret_val;
      } 
   }
   pthread_mutex_unlock(&cl_com_application_error_list_mutex);

   /* setup ssl framework */
   ret_val = cl_com_ssl_framework_setup();
   if (ret_val != CL_RETVAL_OK) {
      cl_com_cleanup_commlib();
      return ret_val;
   }


   /* setup global cl_com_handle_list */
   pthread_mutex_lock(&cl_com_handle_list_mutex);
   if (cl_com_handle_list == NULL) {
      ret_val = cl_handle_list_setup(&cl_com_handle_list,"handle list");
      if (cl_com_handle_list == NULL) {
         pthread_mutex_unlock(&cl_com_handle_list_mutex);
         cl_com_cleanup_commlib();
         return ret_val;
      }
   }
   pthread_mutex_unlock(&cl_com_handle_list_mutex);

   /* setup global host list */
   pthread_mutex_lock(&cl_com_host_list_mutex);
   if (cl_com_host_list == NULL) {
      ret_val = cl_host_list_setup(&cl_com_host_list, "global_host_cache", CL_SHORT, NULL , NULL, 0 , 0, 0 );
      if (cl_com_host_list == NULL) {
         pthread_mutex_unlock(&cl_com_host_list_mutex);
         cl_com_cleanup_commlib();
         return ret_val;
      }
   }
   pthread_mutex_unlock(&cl_com_host_list_mutex);

   /* setup global endpoint list */
   pthread_mutex_lock(&cl_com_endpoint_list_mutex);
   if (cl_com_endpoint_list == NULL) {
      ret_val = cl_endpoint_list_setup(&cl_com_endpoint_list, "global_endpoint_list" , 0 , 0);
      if (cl_com_endpoint_list == NULL) {
         pthread_mutex_unlock(&cl_com_endpoint_list_mutex);
         cl_com_cleanup_commlib();
         return ret_val;
      }
   }
   pthread_mutex_unlock(&cl_com_endpoint_list_mutex);

   /* setup global thread list */
   pthread_mutex_lock(&cl_com_thread_list_mutex);
   switch(cl_com_create_threads) {
      case CL_NO_THREAD: {
         CL_LOG(CL_LOG_INFO,"no threads enabled");
         break;
      }
      case CL_RW_THREAD: {
         if (cl_com_thread_list == NULL) {
            ret_val = cl_thread_list_setup(&cl_com_thread_list,"global_thread_list");
            if (cl_com_thread_list == NULL) {
               pthread_mutex_unlock(&cl_com_thread_list_mutex);
               CL_LOG(CL_LOG_ERROR,"could not setup thread list");
               cl_com_cleanup_commlib();
               return ret_val;
            }
            CL_LOG(CL_LOG_INFO,"starting trigger thread ...");
            ret_val = cl_thread_list_create_thread(cl_com_thread_list,
                                                   &thread_p,
                                                   cl_com_log_list,
                                                   "trigger_thread", 1, cl_com_trigger_thread, NULL, NULL);
            if (ret_val != CL_RETVAL_OK) {
               pthread_mutex_unlock(&cl_com_thread_list_mutex);
               CL_LOG(CL_LOG_ERROR,"could not start trigger_thread");
               cl_com_cleanup_commlib();
               return ret_val;
            }
         }
         break;
      }
   }
   pthread_mutex_unlock(&cl_com_thread_list_mutex);
 
   CL_LOG(CL_LOG_INFO,"ngc library setup done");
   cl_commlib_check_callback_functions();

   if (different_thread_mode == CL_TRUE) {
      return CL_RETVAL_COMMLIB_SETUP_ALREADY_CALLED;
   }

   return CL_RETVAL_OK;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_cleanup_commlib()"
int cl_com_cleanup_commlib(void) {
   int ret_val = CL_RETVAL_OK;
   cl_thread_settings_t* thread_p = NULL;
   cl_handle_list_elem_t* elem = NULL;

   /* lock handle list mutex */   
   pthread_mutex_lock(&cl_com_handle_list_mutex);

   if (cl_com_handle_list  == NULL) {
      pthread_mutex_unlock(&cl_com_handle_list_mutex);
      /* cleanup already called or cl_com_setup_commlib() was not called */
      return CL_RETVAL_PARAMS;
   }
 
   CL_LOG(CL_LOG_INFO,"cleanup commlib ...");

   cl_commlib_check_callback_functions(); /* flush all callbacks to application */

   /* shutdown all connection handle objects (and threads) */  
   while ( (elem = cl_handle_list_get_first_elem(cl_com_handle_list)) != NULL) {
      cl_commlib_shutdown_handle(elem->handle,CL_FALSE);
   }

   CL_LOG(CL_LOG_INFO,"cleanup thread list ...");
   /* cleanup global thread list */
   pthread_mutex_lock(&cl_com_thread_list_mutex);
   switch(cl_com_create_threads) {
      case CL_NO_THREAD:
         CL_LOG(CL_LOG_INFO,"no threads enabled");
         break;
      case CL_RW_THREAD:
         CL_LOG(CL_LOG_INFO,"shutdown trigger thread ...");
         ret_val = cl_thread_list_delete_thread_by_id(cl_com_thread_list, 1);
         if (ret_val != CL_RETVAL_OK) {
            CL_LOG_STR(CL_LOG_ERROR,"error shutting down trigger thread", cl_get_error_text(ret_val));
         } else {
            CL_LOG(CL_LOG_INFO,"shutdown trigger thread OK");
         }
         break;
   }
   
   while( (thread_p=cl_thread_list_get_first_thread(cl_com_thread_list)) != NULL) {
     CL_LOG(CL_LOG_ERROR,"cleanup of threads did not shutdown all threads ...");
     cl_thread_list_delete_thread(cl_com_thread_list, thread_p);
   }

   cl_thread_list_cleanup(&cl_com_thread_list);
   
   cl_thread_cleanup_global_thread_config_key();
   
   pthread_mutex_unlock(&cl_com_thread_list_mutex);

   CL_LOG(CL_LOG_INFO,"cleanup thread list done");


   CL_LOG(CL_LOG_INFO,"cleanup handle list ...");

   /* cleanup global cl_com_handle_list */
   cl_handle_list_cleanup(&cl_com_handle_list);
   pthread_mutex_unlock(&cl_com_handle_list_mutex);

   
   CL_LOG(CL_LOG_INFO,"cleanup endpoint list ...");
   pthread_mutex_lock(&cl_com_endpoint_list_mutex);
   cl_endpoint_list_cleanup(&cl_com_endpoint_list);
   pthread_mutex_unlock(&cl_com_endpoint_list_mutex);
    
   CL_LOG(CL_LOG_INFO,"cleanup host list ...");
   pthread_mutex_lock(&cl_com_host_list_mutex);
   cl_host_list_cleanup(&cl_com_host_list);
   pthread_mutex_unlock(&cl_com_host_list_mutex);

   CL_LOG(CL_LOG_INFO,"cleanup ssl framework configuration object ...");
   cl_com_ssl_framework_cleanup(); 

   CL_LOG(CL_LOG_INFO,"cleanup application error list ...");
   pthread_mutex_lock(&cl_com_application_error_list_mutex);
   cl_application_error_list_cleanup(&cl_com_application_error_list);
   pthread_mutex_unlock(&cl_com_application_error_list_mutex);

   CL_LOG(CL_LOG_INFO,"cleanup log list ...");
   /* cleanup global cl_com_log_list */
   pthread_mutex_lock(&cl_com_log_list_mutex);
   if (cl_commlib_debug_resolvable_hosts != NULL) {
      free(cl_commlib_debug_resolvable_hosts);
      cl_commlib_debug_resolvable_hosts = NULL;
   }
   if (cl_commlib_debug_unresolvable_hosts != NULL) {
      free(cl_commlib_debug_unresolvable_hosts);
      cl_commlib_debug_unresolvable_hosts = NULL;
   }
   cl_log_list_cleanup(&cl_com_log_list);
   pthread_mutex_unlock(&cl_com_log_list_mutex);

   return CL_RETVAL_OK;
}



/*TODO check this function , check locking */
#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_set_connection_param()"
int cl_commlib_set_connection_param(cl_com_handle_t* handle, int parameter, int value) {
   if (handle != NULL) {
      switch(parameter) {
         case HEARD_FROM_TIMEOUT:
            handle->last_heard_from_timeout = value;
            /* add a few seconds to actual HEARD_FROM_TIMEOUT, because we want to be sure that
               qmaster will find the execd when try to send a job at end of HEARD_FROM_TIMEOUT intervall */
            cl_endpoint_list_set_entry_life_time(cl_com_get_endpoint_list(), value + handle->open_connection_timeout );
            break;
      }
   }
   return CL_RETVAL_OK;
}
/*TODO check this function  , check locking*/
#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_get_connection_param()"
int cl_commlib_get_connection_param(cl_com_handle_t* handle, int parameter, int* value) {
   if (handle != NULL) {
      switch(parameter) {
         case HEARD_FROM_TIMEOUT:
            *value = handle->last_heard_from_timeout;
            break;
      }
   }
   return CL_RETVAL_OK;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_specify_ssl_configuration()"
int cl_com_specify_ssl_configuration(cl_ssl_setup_t* new_config) {
   int ret_val = CL_RETVAL_OK;

   pthread_mutex_lock(&cl_com_ssl_setup_mutex);
   if (cl_com_ssl_setup_config != NULL) {
      CL_LOG(CL_LOG_INFO,"resetting ssl setup configuration");
      cl_com_free_ssl_setup(&cl_com_ssl_setup_config);
   } else {
      CL_LOG(CL_LOG_INFO,"setting ssl setup configuration");
   }
   ret_val = cl_com_dup_ssl_setup(&cl_com_ssl_setup_config, new_config);
   pthread_mutex_unlock(&cl_com_ssl_setup_mutex);

   return ret_val;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_create_handle()"
cl_com_handle_t* cl_com_create_handle(int* commlib_error,
                                      cl_framework_t framework,
                                      cl_xml_connection_type_t data_flow_type,
                                      cl_bool_t service_provider,
                                      int handle_port,
                                      cl_tcp_connect_t tcp_connect_mode,
                                      char* component_name, unsigned long component_id,
                                      int select_sec_timeout, int select_usec_timeout) {
   int thread_start_error = 0;
   cl_com_handle_t* new_handle = NULL;
   int return_value = CL_RETVAL_OK;
   int usec_rest = 0;
   int full_usec_seconds = 0;
   int sec_param = 0;
   char help_buffer[80];
   char* local_hostname = NULL;
   cl_handle_list_elem_t* elem = NULL;
#if defined(IRIX) || (defined(LINUX) && defined(TARGET32_BIT))
   struct rlimit64 application_rlimits;
#else
   struct rlimit application_rlimits;
#endif


   cl_commlib_check_callback_functions();

   if (cl_com_handle_list  == NULL) {
      CL_LOG(CL_LOG_ERROR,"cl_com_setup_commlib() not called");
      cl_commlib_push_application_error(CL_LOG_ERROR, CL_RETVAL_NO_FRAMEWORK_INIT, NULL);
      if (commlib_error) {
         *commlib_error = CL_RETVAL_NO_FRAMEWORK_INIT;
      }
      return NULL;
   }

   if (component_name == NULL ) {
      CL_LOG(CL_LOG_ERROR,"component name is NULL");
      cl_commlib_push_application_error(CL_LOG_ERROR, CL_RETVAL_PARAMS, NULL);
      if (commlib_error) {
         *commlib_error = CL_RETVAL_PARAMS;
      }
      return NULL;
   }

   if ( service_provider == CL_TRUE && component_id == 0) {
      CL_LOG(CL_LOG_ERROR,"service can't use component id 0");
      cl_commlib_push_application_error(CL_LOG_ERROR, CL_RETVAL_PARAMS, NULL);
      if (commlib_error) {
         *commlib_error = CL_RETVAL_PARAMS;
      }
      return NULL;
   }

   /* first lock handle list */   
   cl_raw_list_lock(cl_com_handle_list);

   elem = cl_handle_list_get_first_elem(cl_com_handle_list);
   while ( elem != NULL) {
      cl_com_endpoint_t* local_endpoint = elem->handle->local;
       
      if (local_endpoint->comp_id == component_id) {
         if (strcmp(local_endpoint->comp_name, component_name) == 0) {
            /* we have this handle already in list */
            CL_LOG(CL_LOG_ERROR,"component not unique");
            cl_commlib_push_application_error(CL_LOG_ERROR, CL_RETVAL_LOCAL_ENDPOINT_NOT_UNIQUE, NULL);
            cl_raw_list_unlock(cl_com_handle_list);
            if (commlib_error) {
               *commlib_error = CL_RETVAL_LOCAL_ENDPOINT_NOT_UNIQUE;
            }
            return NULL;
         }
      }
      elem = cl_handle_list_get_next_elem(elem);
   }

   return_value = cl_com_gethostname(&local_hostname, NULL, NULL, NULL);
   if (return_value != CL_RETVAL_OK) {
      CL_LOG(CL_LOG_ERROR,cl_get_error_text(return_value));
      cl_commlib_push_application_error(CL_LOG_ERROR, return_value, NULL);
      cl_raw_list_unlock(cl_com_handle_list);
      if (commlib_error) {
         *commlib_error = return_value;
      }
      return NULL;
   }
   CL_LOG_STR(CL_LOG_INFO,"local host name is",local_hostname );


   new_handle = (cl_com_handle_t*) malloc(sizeof(cl_com_handle_t));
   if (new_handle == NULL) {
      free(local_hostname);
      CL_LOG(CL_LOG_ERROR,"malloc() error");
      cl_commlib_push_application_error(CL_LOG_ERROR, CL_RETVAL_MALLOC, NULL);
      cl_raw_list_unlock(cl_com_handle_list);
      if (commlib_error) {
         *commlib_error = CL_RETVAL_MALLOC;
      }
      return NULL;
   }

   /* setup SSL configuration */
   new_handle->ssl_setup = NULL;
   switch(framework) {
      case CL_CT_UNDEFINED: 
      case CL_CT_TCP: 
         break;
      case CL_CT_SSL: {
         pthread_mutex_lock(&cl_com_ssl_setup_mutex);
         if (cl_com_ssl_setup_config == NULL) {
            CL_LOG(CL_LOG_ERROR,"use cl_com_specify_ssl_configuration() to specify a ssl configuration");
            free(local_hostname);
            free(new_handle);
            cl_raw_list_unlock(cl_com_handle_list);
            if (commlib_error) {
               *commlib_error = CL_RETVAL_NO_FRAMEWORK_INIT;
            }
            pthread_mutex_unlock(&cl_com_ssl_setup_mutex);
            cl_commlib_push_application_error(CL_LOG_ERROR, CL_RETVAL_NO_FRAMEWORK_INIT, NULL);
            return NULL;
         }
        
         if ((return_value = cl_com_dup_ssl_setup(&(new_handle->ssl_setup), cl_com_ssl_setup_config)) != CL_RETVAL_OK) {
            free(local_hostname);
            free(new_handle);
            cl_raw_list_unlock(cl_com_handle_list);
            if (commlib_error) {
               *commlib_error = return_value;
            }
            pthread_mutex_unlock(&cl_com_ssl_setup_mutex);
            cl_commlib_push_application_error(CL_LOG_ERROR, return_value, NULL);
            return NULL;
         }

         pthread_mutex_unlock(&cl_com_ssl_setup_mutex);
         break;
      }
   }
   
   usec_rest = select_usec_timeout % 1000000;           /* usec parameter for select should not be > 1000000 !!!*/
   full_usec_seconds = select_usec_timeout / 1000000;   /* full seconds from timeout_val_usec parameter */
   sec_param = select_sec_timeout + full_usec_seconds;  /* add full seconds from usec parameter to timeout_val_sec parameter */



   new_handle->local = NULL;
   new_handle->tcp_connect_mode = tcp_connect_mode;

   new_handle->debug_client_setup = NULL;
   if ((return_value = cl_com_create_debug_client_setup(&(new_handle->debug_client_setup),CL_DEBUG_CLIENT_OFF, CL_TRUE, 0)) != CL_RETVAL_OK) {
      CL_LOG(CL_LOG_ERROR,"can't setup debug client structure");
      free(local_hostname);
      free(new_handle);
      cl_raw_list_unlock(cl_com_handle_list);
      if (commlib_error) {
         *commlib_error = CL_RETVAL_NO_FRAMEWORK_INIT;
      }
      cl_commlib_push_application_error(CL_LOG_ERROR, CL_RETVAL_NO_FRAMEWORK_INIT, NULL);
      return NULL;
   }  

   new_handle->messages_ready_for_read = 0;
   new_handle->messages_ready_mutex = NULL;
   new_handle->connection_list_mutex = NULL;
   new_handle->connection_list = NULL;
   new_handle->received_message_queue = NULL;
   new_handle->send_message_queue = NULL;
   new_handle->next_free_client_id = 1;
   new_handle->service_handler = NULL;
   new_handle->framework = framework;
   new_handle->data_flow_type = data_flow_type;
   new_handle->service_provider = service_provider;
   if ( new_handle->service_provider == CL_TRUE) {
      /* we are service provider */
      new_handle->connect_port = 0;
      if ( handle_port == 0 ) {
         new_handle->service_port = 0;
         CL_LOG(CL_LOG_WARNING,"no port specified, using next free port");
      } else {
         new_handle->service_port = handle_port;
      }
   } else {
      /* we are client, use port for connect */
      new_handle->connect_port = handle_port;
      new_handle->service_port = 0;
   }
   new_handle->connection_timeout = CL_DEFINE_CLIENT_CONNECTION_LIFETIME;
   new_handle->last_heard_from_timeout = CL_DEFINE_CLIENT_CONNECTION_LIFETIME; /* don't use should be removed */
   new_handle->read_timeout = CL_DEFINE_READ_TIMEOUT;
   new_handle->write_timeout = CL_DEFINE_WRITE_TIMEOUT;
   new_handle->open_connection_timeout = CL_DEFINE_GET_CLIENT_CONNECTION_DATA_TIMEOUT;
   new_handle->close_connection_timeout = CL_DEFINE_DELETE_MESSAGES_TIMEOUT_AFTER_CCRM;
   new_handle->acknowledge_timeout = CL_DEFINE_ACK_TIMEOUT;
   new_handle->select_sec_timeout = sec_param;
   new_handle->select_usec_timeout = usec_rest;
   new_handle->synchron_receive_timeout = CL_DEFINE_SYNCHRON_RECEIVE_TIMEOUT;  /* default from old commlib */
   new_handle->do_shutdown = 0;  /* no shutdown till now */
   new_handle->max_connection_count_reached = CL_FALSE;
   new_handle->max_connection_count_found_connection_to_close = CL_FALSE;
   new_handle->allowed_host_list = NULL;
   
   new_handle->auto_close_mode = CL_CM_AC_DISABLED;
   
#if defined(IRIX) || (defined(LINUX) && defined(TARGET32_BIT))
   getrlimit64(RLIMIT_NOFILE, &application_rlimits);
#else
   getrlimit(RLIMIT_NOFILE, &application_rlimits);
#endif

   new_handle->max_open_connections = (unsigned long) application_rlimits.rlim_cur;

   if (FD_SETSIZE < new_handle->max_open_connections) {
      CL_LOG(CL_LOG_ERROR,"FD_SETSIZE < file descriptor limit");
      new_handle->max_open_connections = FD_SETSIZE - 1;
   }

   if ( new_handle->max_open_connections < 32 ) {
      CL_LOG_INT(CL_LOG_ERROR, "to less file descriptors:", (int)new_handle->max_open_connections );
      free(new_handle);
      free(local_hostname);
      cl_raw_list_unlock(cl_com_handle_list);
      if (commlib_error) {
         *commlib_error = CL_RETVAL_TO_LESS_FILEDESCRIPTORS;
      }
      cl_commlib_push_application_error(CL_LOG_ERROR, CL_RETVAL_TO_LESS_FILEDESCRIPTORS, NULL);
      return NULL;
   }
   CL_LOG_INT(CL_LOG_INFO, "max file descriptors on this system    :", (int)new_handle->max_open_connections);

   new_handle->max_open_connections = new_handle->max_open_connections - 20;

   CL_LOG_INT(CL_LOG_INFO, "max. used descriptors for communication:", (int)new_handle->max_open_connections);
   
   new_handle->max_con_close_mode = CL_ON_MAX_COUNT_OFF;
   new_handle->app_condition = NULL;
   new_handle->read_condition = NULL;
   new_handle->write_condition = NULL;
   new_handle->service_thread = NULL;
   new_handle->read_thread = NULL;
   new_handle->write_thread = NULL;


   new_handle->statistic = (cl_com_handle_statistic_t*)malloc(sizeof(cl_com_handle_statistic_t));
   if (new_handle->statistic == NULL) {
      free(new_handle);
      free(local_hostname);
      cl_raw_list_unlock(cl_com_handle_list);
      if (commlib_error) {
         *commlib_error = CL_RETVAL_MALLOC;
      }
      cl_commlib_push_application_error(CL_LOG_ERROR, CL_RETVAL_MALLOC, NULL);
      return NULL;
   }
   memset(new_handle->statistic, 0, sizeof(cl_com_handle_statistic_t));


   gettimeofday(&(new_handle->statistic->last_update),NULL);
   gettimeofday(&(new_handle->start_time),NULL);

   new_handle->messages_ready_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
   if (new_handle->messages_ready_mutex == NULL) {
      cl_com_free_handle_statistic(&(new_handle->statistic));
      free(new_handle);
      free(local_hostname);
      cl_raw_list_unlock(cl_com_handle_list);
      if (commlib_error) {
         *commlib_error = CL_RETVAL_MALLOC;
      }
      cl_commlib_push_application_error(CL_LOG_ERROR, CL_RETVAL_MALLOC, NULL);
      return NULL;
   }

   new_handle->connection_list_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
   if (new_handle->connection_list_mutex == NULL) {
      free(new_handle->messages_ready_mutex);
      cl_com_free_handle_statistic(&(new_handle->statistic));
      free(new_handle);
      free(local_hostname);
      cl_raw_list_unlock(cl_com_handle_list);
      if (commlib_error) {
         *commlib_error = CL_RETVAL_MALLOC;
      }
      cl_commlib_push_application_error(CL_LOG_ERROR, CL_RETVAL_MALLOC, NULL);
      return NULL;
   }


   if (pthread_mutex_init(new_handle->messages_ready_mutex, NULL) != 0) {
      int mutex_ret_val = pthread_mutex_destroy(new_handle->messages_ready_mutex);
      if (mutex_ret_val != EBUSY) {
         free(new_handle->messages_ready_mutex); 
      }
      free(new_handle->connection_list_mutex);
      cl_com_free_handle_statistic(&(new_handle->statistic));
      free(new_handle);
      free(local_hostname);
      cl_raw_list_unlock(cl_com_handle_list);
      if (commlib_error) {
         *commlib_error = CL_RETVAL_MUTEX_ERROR;
      }
      cl_commlib_push_application_error(CL_LOG_ERROR, CL_RETVAL_MUTEX_ERROR, NULL);
      return NULL;
   } 

   if (pthread_mutex_init(new_handle->connection_list_mutex, NULL) != 0) {
      int mutex_ret_val;
      mutex_ret_val = pthread_mutex_destroy(new_handle->connection_list_mutex);
      if (mutex_ret_val != EBUSY) {
         free(new_handle->connection_list_mutex); 
      }
      mutex_ret_val = pthread_mutex_destroy(new_handle->messages_ready_mutex);
      if (mutex_ret_val != EBUSY) {
         free(new_handle->messages_ready_mutex); 
      }
      cl_com_free_handle_statistic(&(new_handle->statistic));
      free(new_handle);
      free(local_hostname);
      cl_raw_list_unlock(cl_com_handle_list);
      if (commlib_error) {
         *commlib_error = CL_RETVAL_MUTEX_ERROR;
      }
      cl_commlib_push_application_error(CL_LOG_ERROR, CL_RETVAL_MUTEX_ERROR, NULL);
      return NULL;
   }


   new_handle->local = cl_com_create_endpoint(local_hostname, component_name, component_id );
   if (new_handle->local == NULL) {
      int mutex_ret_val;
      mutex_ret_val = pthread_mutex_destroy(new_handle->connection_list_mutex);
      if (mutex_ret_val != EBUSY) {
         free(new_handle->connection_list_mutex); 
      }
      mutex_ret_val = pthread_mutex_destroy(new_handle->messages_ready_mutex);
      if (mutex_ret_val != EBUSY) {
         free(new_handle->messages_ready_mutex); 
      }
      cl_com_free_handle_statistic(&(new_handle->statistic));
      free(new_handle);
      free(local_hostname);
      cl_raw_list_unlock(cl_com_handle_list);
      if (commlib_error) {
         *commlib_error = CL_RETVAL_MALLOC;
      }
      cl_commlib_push_application_error(CL_LOG_ERROR, CL_RETVAL_MALLOC, NULL);
      return NULL;
   }

   if ((return_value=cl_app_message_queue_setup(&(new_handle->send_message_queue), "send message queue", 1)) != CL_RETVAL_OK) {
      int mutex_ret_val;
      cl_app_message_queue_cleanup(&(new_handle->received_message_queue));
      cl_app_message_queue_cleanup(&(new_handle->send_message_queue));
      cl_com_free_endpoint(&(new_handle->local));
      mutex_ret_val = pthread_mutex_destroy(new_handle->connection_list_mutex);
      if (mutex_ret_val != EBUSY) {
         free(new_handle->connection_list_mutex); 
      }
      mutex_ret_val = pthread_mutex_destroy(new_handle->messages_ready_mutex);
      if (mutex_ret_val != EBUSY) {
         free(new_handle->messages_ready_mutex); 
      }
      cl_com_free_handle_statistic(&(new_handle->statistic));
      free(new_handle);
      free(local_hostname);
      cl_raw_list_unlock(cl_com_handle_list);
      if (commlib_error) {
         *commlib_error = return_value;
      }
      cl_commlib_push_application_error(CL_LOG_ERROR, return_value, NULL);
      return NULL;
   } 

   if ((return_value=cl_app_message_queue_setup(&(new_handle->received_message_queue), "received message queue", 1)) != CL_RETVAL_OK) {
      int mutex_ret_val;
      cl_app_message_queue_cleanup(&(new_handle->received_message_queue));
      cl_com_free_endpoint(&(new_handle->local));
      mutex_ret_val = pthread_mutex_destroy(new_handle->connection_list_mutex);
      if (mutex_ret_val != EBUSY) {
         free(new_handle->connection_list_mutex); 
      }
      mutex_ret_val = pthread_mutex_destroy(new_handle->messages_ready_mutex);
      if (mutex_ret_val != EBUSY) {
         free(new_handle->messages_ready_mutex); 
      }
      cl_com_free_handle_statistic(&(new_handle->statistic));
      free(new_handle);
      free(local_hostname);
      cl_raw_list_unlock(cl_com_handle_list);
      if (commlib_error) {
         *commlib_error = return_value;
      }
      cl_commlib_push_application_error(CL_LOG_ERROR, return_value, NULL);
      return NULL;
   } 

   if ((return_value=cl_connection_list_setup(&(new_handle->connection_list), "connection list", 1)) != CL_RETVAL_OK) {
      int mutex_ret_val;
      cl_app_message_queue_cleanup(&(new_handle->send_message_queue));
      cl_app_message_queue_cleanup(&(new_handle->received_message_queue));
      cl_connection_list_cleanup(&(new_handle->connection_list));
      cl_com_free_endpoint(&(new_handle->local));
      mutex_ret_val = pthread_mutex_destroy(new_handle->connection_list_mutex);
      if (mutex_ret_val != EBUSY) {
         free(new_handle->connection_list_mutex); 
      }
      mutex_ret_val = pthread_mutex_destroy(new_handle->messages_ready_mutex);
      if (mutex_ret_val != EBUSY) {
         free(new_handle->messages_ready_mutex); 
      }
      cl_com_free_handle_statistic(&(new_handle->statistic));
      free(new_handle);
      free(local_hostname);
      cl_raw_list_unlock(cl_com_handle_list);
      if (commlib_error) {
         *commlib_error = return_value;
      }
      cl_commlib_push_application_error(CL_LOG_ERROR, return_value, NULL);
      return NULL;
   } 

   if ((return_value=cl_string_list_setup(&(new_handle->allowed_host_list), "allowed host list")) != CL_RETVAL_OK) {
      int mutex_ret_val;

      cl_string_list_cleanup(&(new_handle->allowed_host_list));
      cl_app_message_queue_cleanup(&(new_handle->send_message_queue));
      cl_app_message_queue_cleanup(&(new_handle->received_message_queue));
      cl_connection_list_cleanup(&(new_handle->connection_list));
      cl_com_free_endpoint(&(new_handle->local));
      mutex_ret_val = pthread_mutex_destroy(new_handle->connection_list_mutex);
      if (mutex_ret_val != EBUSY) {
         free(new_handle->connection_list_mutex); 
      }
      mutex_ret_val = pthread_mutex_destroy(new_handle->messages_ready_mutex);
      if (mutex_ret_val != EBUSY) {
         free(new_handle->messages_ready_mutex); 
      }
      cl_com_free_handle_statistic(&(new_handle->statistic));
      free(new_handle);
      free(local_hostname);
      cl_raw_list_unlock(cl_com_handle_list);
      if (commlib_error) {
         *commlib_error = return_value;
      }
      cl_commlib_push_application_error(CL_LOG_ERROR, return_value, NULL);
      return NULL;
   }
 
 


   /* local host name not needed anymore */
   free(local_hostname);
   local_hostname = NULL;


   if (new_handle->service_provider == CL_TRUE) {
      /* create service */
      cl_com_connection_t* new_con = NULL;

      CL_LOG(CL_LOG_INFO,"creating service ...");
      return_value = cl_com_setup_connection(new_handle, &new_con);

      if (return_value != CL_RETVAL_OK) {
         int mutex_ret_val;
         cl_com_close_connection(&new_con);
         cl_string_list_cleanup(&(new_handle->allowed_host_list));
         cl_app_message_queue_cleanup(&(new_handle->send_message_queue));
         cl_app_message_queue_cleanup(&(new_handle->received_message_queue));
         cl_connection_list_cleanup(&(new_handle->connection_list));
         cl_com_free_endpoint(&(new_handle->local));
         mutex_ret_val = pthread_mutex_destroy(new_handle->connection_list_mutex);
         if (mutex_ret_val != EBUSY) {
            free(new_handle->connection_list_mutex); 
         }
         mutex_ret_val = pthread_mutex_destroy(new_handle->messages_ready_mutex);
         if (mutex_ret_val != EBUSY) {
            free(new_handle->messages_ready_mutex); 
         }
         cl_com_free_handle_statistic(&(new_handle->statistic));
         free(new_handle);
         cl_raw_list_unlock(cl_com_handle_list);
         if (commlib_error) {
            *commlib_error = return_value;
         }
         cl_commlib_push_application_error(CL_LOG_ERROR, return_value, NULL);
         return NULL;
      }
      
      /* autoclose is not used for service connection */
      new_con->data_flow_type = CL_CM_CT_STREAM;
      new_con->auto_close_type = CL_CM_AC_UNDEFINED;
      new_con->handler = new_handle;

      return_value = cl_com_connection_request_handler_setup(new_con, new_handle->local);
      if (return_value != CL_RETVAL_OK) {
         int mutex_ret_val;
         cl_com_connection_request_handler_cleanup(new_con);
         cl_com_close_connection(&new_con);
         cl_string_list_cleanup(&(new_handle->allowed_host_list));
         cl_app_message_queue_cleanup(&(new_handle->send_message_queue));
         cl_app_message_queue_cleanup(&(new_handle->received_message_queue));
         cl_connection_list_cleanup(&(new_handle->connection_list));
         cl_com_free_endpoint(&(new_handle->local));
         mutex_ret_val = pthread_mutex_destroy(new_handle->connection_list_mutex);
         if (mutex_ret_val != EBUSY) {
            free(new_handle->connection_list_mutex); 
         }
         mutex_ret_val = pthread_mutex_destroy(new_handle->messages_ready_mutex);
         if (mutex_ret_val != EBUSY) {
            free(new_handle->messages_ready_mutex); 
         }
         cl_com_free_handle_statistic(&(new_handle->statistic));

         cl_com_free_debug_client_setup(&(new_handle->debug_client_setup));

         cl_com_free_ssl_setup(&(new_handle->ssl_setup));

         free(new_handle);
         cl_raw_list_unlock(cl_com_handle_list);
         if (commlib_error) {
            *commlib_error = return_value;
         }
         cl_commlib_push_application_error(CL_LOG_ERROR, return_value, NULL);
         return NULL;
      }

      new_handle->service_handler = new_con;

            
      /* Set handle service port, when we use random port */
      if ( new_handle->service_port == 0) {
         int service_port = 0;
         if (cl_com_connection_get_service_port(new_con,&service_port) == CL_RETVAL_OK) {
            new_handle->service_port = service_port;
         }
      }
   }

   /* create handle thread */
   thread_start_error = 0;
   switch(cl_com_create_threads) {
      case CL_NO_THREAD: {
         CL_LOG(CL_LOG_INFO,"no threads enabled");
         break;
      }
      case CL_RW_THREAD: {
         CL_LOG(CL_LOG_INFO,"creating read condition ...");
         return_value = cl_thread_create_thread_condition(&(new_handle->read_condition));
         if (return_value != CL_RETVAL_OK) {
            CL_LOG(CL_LOG_ERROR,"could not setup read condition");
            thread_start_error = 1;
            break;
         }

         CL_LOG(CL_LOG_INFO,"creating application read condition ...");
         return_value = cl_thread_create_thread_condition(&(new_handle->app_condition));
         if (return_value != CL_RETVAL_OK) {
            CL_LOG(CL_LOG_ERROR,"could not setup application read condition");
            thread_start_error = 1;
            break;
         }

         CL_LOG(CL_LOG_INFO,"creating write condition ...");
         return_value = cl_thread_create_thread_condition(&(new_handle->write_condition));
         if (return_value != CL_RETVAL_OK) {
            CL_LOG(CL_LOG_ERROR,"could not setup write condition");
            thread_start_error = 1;
            break;
         }


         CL_LOG(CL_LOG_INFO,"starting handle service thread ...");
         snprintf(help_buffer,80,"%s_service",new_handle->local->comp_name);
         return_value = cl_thread_list_create_thread(cl_com_thread_list,
                                                     &(new_handle->service_thread),
                                                     cl_com_log_list,
                                                     help_buffer, 2, cl_com_handle_service_thread, NULL, (void*)new_handle);
         if (return_value != CL_RETVAL_OK) {
            CL_LOG(CL_LOG_ERROR,"could not start handle service thread");
            thread_start_error = 1;
            break;
         }

         CL_LOG(CL_LOG_INFO,"starting handle read thread ...");
         snprintf(help_buffer,80,"%s_read",new_handle->local->comp_name);
         return_value = cl_thread_list_create_thread(cl_com_thread_list,
                                                     &(new_handle->read_thread),
                                                     cl_com_log_list,
                                                     help_buffer, 3, cl_com_handle_read_thread, NULL, NULL);
         if (return_value != CL_RETVAL_OK) {
            CL_LOG(CL_LOG_ERROR,"could not start handle read thread");
            thread_start_error = 1;
            break;
         }

         CL_LOG(CL_LOG_INFO,"starting handle write thread ...");
         snprintf(help_buffer,80,"%s_write",new_handle->local->comp_name);
         return_value = cl_thread_list_create_thread(cl_com_thread_list,
                                                     &(new_handle->write_thread),
                                                     cl_com_log_list,
                                                     help_buffer, 2, cl_com_handle_write_thread, NULL, NULL);
         if (return_value != CL_RETVAL_OK) {
            CL_LOG(CL_LOG_ERROR,"could not start handle write thread");
            thread_start_error = 1;
            break;
         }
         break;
      }
   }
   /*
    * Do NOT touch return_value, it contains information about 
    * thread start up errors !
    */
   if (thread_start_error != 0) {
      int mutex_ret_val;
      if (new_handle->service_handler != NULL) {
         cl_com_connection_request_handler_cleanup(new_handle->service_handler);
         cl_com_close_connection(&(new_handle->service_handler));
      }
      cl_connection_list_cleanup(&(new_handle->connection_list));
      cl_app_message_queue_cleanup(&(new_handle->send_message_queue));
      cl_app_message_queue_cleanup(&(new_handle->received_message_queue));
      cl_string_list_cleanup(&(new_handle->allowed_host_list));
      cl_com_free_endpoint(&(new_handle->local));
      mutex_ret_val = pthread_mutex_destroy(new_handle->connection_list_mutex);
      if (mutex_ret_val != EBUSY) {
         free(new_handle->connection_list_mutex); 
      }
      mutex_ret_val = pthread_mutex_destroy(new_handle->messages_ready_mutex);
      if (mutex_ret_val != EBUSY) {
         free(new_handle->messages_ready_mutex); 
      }
      cl_com_free_handle_statistic(&(new_handle->statistic));
      free(new_handle);
      cl_raw_list_unlock(cl_com_handle_list);
      if (commlib_error) {
         *commlib_error = return_value;
      }
      cl_commlib_push_application_error(CL_LOG_ERROR, return_value, NULL);
      return NULL;
   }
   cl_handle_list_append_handle(cl_com_handle_list, new_handle,0);
   cl_raw_list_unlock(cl_com_handle_list);
   CL_LOG(CL_LOG_INFO, "new handle created");
   if (commlib_error) {
      *commlib_error = CL_RETVAL_OK;
   }
   return new_handle;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_shutdown_handle()"
int cl_commlib_shutdown_handle(cl_com_handle_t* handle, cl_bool_t return_for_messages) {
   cl_connection_list_elem_t* elem = NULL;
   cl_thread_settings_t* thread_settings = NULL;
   struct timeval now;
   cl_bool_t connection_list_empty = CL_FALSE;
   cl_bool_t trigger_write = CL_FALSE;
   cl_app_message_queue_elem_t* mq_elem = NULL;
   int mq_return_value = CL_RETVAL_OK;
   int ret_val;


   cl_commlib_check_callback_functions();

   if (handle == NULL) {
      return CL_RETVAL_PARAMS;
   }

   if (cl_com_handle_list  == NULL) {
      CL_LOG(CL_LOG_ERROR,"cl_com_setup_commlib() not called");
      return CL_RETVAL_PARAMS;
   }

   cl_raw_list_lock(cl_com_handle_list);

   CL_LOG(CL_LOG_INFO,"shutting down handle ...");
   if ( handle->do_shutdown == 0) {
      /* wait for connection close response message from heach connection */
      /* get current timeout time */
      gettimeofday(&now,NULL);
      handle->shutdown_timeout = now.tv_sec + handle->acknowledge_timeout + handle->close_connection_timeout;
   }

   /* flush send message queue */
   cl_raw_list_lock(handle->send_message_queue);
   while((mq_elem = cl_app_message_queue_get_first_elem(handle->send_message_queue)) != NULL) {
      CL_LOG(CL_LOG_INFO,"flushing send message queue ...");

      mq_return_value = cl_commlib_send_message_to_endpoint(handle, mq_elem->snd_destination,
                                                            mq_elem->snd_ack_type, mq_elem->snd_data,
                                                            mq_elem->snd_size, mq_elem->snd_response_mid,
                                                            mq_elem->snd_tag); 
      /* remove queue entries */
      cl_raw_list_remove_elem(handle->send_message_queue, mq_elem->raw_elem);
      if (mq_return_value != CL_RETVAL_OK) {
         CL_LOG_STR(CL_LOG_ERROR,"can't send message:", cl_get_error_text(mq_return_value));
         free(mq_elem->snd_data);
      }
      cl_com_free_endpoint(&(mq_elem->snd_destination));
      free(mq_elem);
   }
   cl_raw_list_unlock(handle->send_message_queue);

   if (return_for_messages == CL_TRUE) {
      handle->do_shutdown = 1; /* stop accepting new connections , don't delete any messages */
   } else {
      handle->do_shutdown = 2; /* stop accepting new connections , return not handled response messages in cl_commlib_receive_message() */
   }

   /* wait for empty connection list */
   while( connection_list_empty == CL_FALSE) {
      cl_bool_t have_message_connections = CL_FALSE;
      cl_bool_t ignore_timeout = cl_com_get_ignore_timeouts_flag();
      connection_list_empty = CL_TRUE;
      trigger_write = CL_FALSE;

      cl_raw_list_lock(handle->connection_list);
      elem = cl_connection_list_get_first_elem(handle->connection_list);
      while(elem) {
         connection_list_empty = CL_FALSE;
         if ( elem->connection->data_flow_type == CL_CM_CT_MESSAGE) {
            have_message_connections = CL_TRUE;
            if ( elem->connection->connection_state     == CL_CONNECTED     && 
                 elem->connection->connection_sub_state == CL_COM_WORK      &&   
                 elem->connection->ccm_received         == 0)        {
               cl_commlib_send_ccm_message(elem->connection);
               trigger_write = CL_TRUE;
               elem->connection->connection_sub_state = CL_COM_SENDING_CCM;
            }
            CL_LOG_STR(CL_LOG_INFO,"wait for connection removal, current state is", 
                       cl_com_get_connection_state(elem->connection));
            CL_LOG_STR(CL_LOG_INFO,"wait for connection removal, current sub state is", 
                       cl_com_get_connection_sub_state(elem->connection));
            if ( ignore_timeout == CL_TRUE ) {
               switch( elem->connection->connection_state) {
                  case CL_CLOSING:
                  case CL_ACCEPTING:
                  case CL_CONNECTING:
                  case CL_CONNECTED: {
                     CL_LOG(CL_LOG_INFO,"we are connected, don't ignore timeouts");
                     ignore_timeout = CL_FALSE;
                     break;
                  }
                  
                  case CL_DISCONNECTED:
                  case CL_OPENING: {
                     break;
                  }
               }
            }
         }
         elem = cl_connection_list_get_next_elem(elem);
      }
      cl_raw_list_unlock(handle->connection_list);


      /*
       * If there are still connections, trigger read/write of messages and
       * return for messages (if return_for_messages is set)
       */
      if ( connection_list_empty == CL_FALSE) {
         int return_value = CL_RETVAL_OK;
         /* still waiting for messages */
         switch(cl_com_create_threads) {
            case CL_NO_THREAD: {
               pthread_mutex_lock(handle->messages_ready_mutex);
               if (handle->messages_ready_for_read != 0) {
                  pthread_mutex_unlock(handle->messages_ready_mutex);
                  /* return for messages */
                  if (return_for_messages == CL_TRUE) {
                     cl_raw_list_unlock(cl_com_handle_list);
                     CL_LOG(CL_LOG_INFO,"delivering MESSAGES");
                     return CL_RETVAL_MESSAGE_IN_BUFFER;
                  } else {
                     /* delete messages */
                     cl_com_message_t* message = NULL;
                     cl_com_endpoint_t* sender = NULL;
                     /*
                      * cl_commlib_receive_message() can be called inside of cl_commlib_shutdown_handle()
                      * because cl_commlib_shutdown_handle() is called from application context.
                      *
                      */
                     cl_commlib_receive_message(handle,NULL, NULL, 0, CL_FALSE, 0, &message, &sender);
                     if (message != NULL) {
                        CL_LOG(CL_LOG_WARNING,"deleting message");
                        cl_com_free_message(&message);
                     }
                     if (sender != NULL) {
                        CL_LOG_STR(CL_LOG_WARNING,"deleted message from:", sender->comp_host);
                        cl_com_free_endpoint(&sender);
                     }

                  }
               } else {
                  pthread_mutex_unlock(handle->messages_ready_mutex);
               }
               cl_commlib_trigger(handle, 1);
               break;
            }
            case CL_RW_THREAD: {
               if (trigger_write == CL_TRUE) {
                  cl_thread_trigger_event(handle->write_thread);
               }


               pthread_mutex_lock(handle->messages_ready_mutex);
               if (handle->messages_ready_for_read != 0) {
                  pthread_mutex_unlock(handle->messages_ready_mutex);

                  if ( return_for_messages == CL_TRUE) {
                     /* return for messages */
                     cl_raw_list_unlock(cl_com_handle_list);
                     CL_LOG(CL_LOG_ERROR,"delivering MESSAGES");
                     return CL_RETVAL_MESSAGE_IN_BUFFER;
                  } else {
                     /* delete messages */
                     cl_com_message_t* message = NULL;
                     cl_com_endpoint_t* sender = NULL;
                     cl_commlib_receive_message(handle,NULL, NULL, 0, CL_FALSE, 0, &message, &sender);
                     if (message != NULL) {
                        CL_LOG(CL_LOG_WARNING,"deleting message");
                        cl_com_free_message(&message);
                     }
                     if (sender != NULL) {
                        CL_LOG_STR(CL_LOG_WARNING,"deleted message from:", sender->comp_host);
                        cl_com_free_endpoint(&sender);
                     }
                  }

               } else {
                  pthread_mutex_unlock(handle->messages_ready_mutex);
               }
               
               CL_LOG(CL_LOG_INFO,"APPLICATION WAITING for CCRM");
               return_value = cl_thread_wait_for_thread_condition(handle->read_condition,
                                                   handle->select_sec_timeout,
                                                   handle->select_usec_timeout);
               if (return_value == CL_RETVAL_CONDITION_WAIT_TIMEOUT) {
                  CL_LOG(CL_LOG_WARNING,"APPLICATION GOT CONDITION WAIT TIMEOUT WHILE WAITING FOR CCRM");
               }
               break;
            }
         }

         /* shutdown stream connections when there are no more message connections */
         cl_raw_list_lock(handle->connection_list);
         elem = cl_connection_list_get_first_elem(handle->connection_list);
         while(elem) {
            connection_list_empty = CL_FALSE;
            if ( elem->connection->data_flow_type == CL_CM_CT_STREAM) {
               if (  have_message_connections == CL_FALSE ) {
                  /* close stream connections after message connections */
   
                  if (handle->debug_client_setup->dc_mode != CL_DEBUG_CLIENT_OFF) {
                     /* don't close STREAM connection when there are debug clients
                     and debug data list is not empty */
   
                     cl_raw_list_lock(handle->debug_client_setup->dc_debug_list);
                     if (cl_raw_list_get_elem_count(handle->debug_client_setup->dc_debug_list) == 0) {
                        elem->connection->connection_state = CL_CLOSING;
                        elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                     }
                     cl_raw_list_unlock(handle->debug_client_setup->dc_debug_list);
                  } else {
                     /* debug clients are not connected, just close stream connections */
                     elem->connection->connection_state = CL_CLOSING;
                     elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                  }
               }
            }
            elem = cl_connection_list_get_next_elem(elem);
         }
         cl_raw_list_unlock(handle->connection_list);

         cl_commlib_handle_debug_clients(handle, CL_TRUE);
      }

      /*
       * check timeouts
       */
      gettimeofday(&now,NULL);
      if (handle->shutdown_timeout <= now.tv_sec || ignore_timeout == CL_TRUE  ) {
         CL_LOG(CL_LOG_ERROR,"got timeout while waiting for close response message");
         break;
      }
   }

   /* ok now we shutdown the handle */
   CL_LOG(CL_LOG_INFO,"shutdown of handle");

   /* remove handle from list */
   cl_raw_list_lock(handle->connection_list);
   elem = cl_connection_list_get_first_elem(handle->connection_list);
   if (elem != NULL) {
      CL_LOG(CL_LOG_ERROR,"########## connection list is not empty ##########");
   }
   cl_raw_list_unlock(handle->connection_list);

   ret_val = cl_handle_list_remove_handle(cl_com_handle_list, handle, 0);
   cl_raw_list_unlock(cl_com_handle_list);


   /* only shutdown handle if handle was removed from list */
   if (ret_val == CL_RETVAL_OK) {
      /* delete handle thread */
      switch(cl_com_create_threads) {
         case CL_NO_THREAD: {
            CL_LOG(CL_LOG_INFO,"no threads enabled");
            break;
         }
         case CL_RW_THREAD: {
            CL_LOG(CL_LOG_INFO,"shutdown handle write thread ...");
            thread_settings = handle->write_thread;
            handle->write_thread = NULL; /* set thread pointer to NULL because other threads use this pointer */
            ret_val = cl_thread_list_delete_thread(cl_com_thread_list, thread_settings);
            if (ret_val != CL_RETVAL_OK) {
               CL_LOG_STR(CL_LOG_ERROR,"error shutting down handle write thread", cl_get_error_text(ret_val));
            } else {
               CL_LOG(CL_LOG_INFO,"shutdown handle write thread OK");
            }
   
            CL_LOG(CL_LOG_INFO,"shutdown handle read thread ...");
            thread_settings = handle->read_thread;
            handle->read_thread = NULL; /* set thread pointer to NULL because other threads use this pointer */
            ret_val = cl_thread_list_delete_thread(cl_com_thread_list, thread_settings);
            if (ret_val != CL_RETVAL_OK) {
               CL_LOG_STR(CL_LOG_ERROR,"error shutting down handle read thread", cl_get_error_text(ret_val));
            } else {
               CL_LOG(CL_LOG_INFO,"shutdown handle read thread OK");
            }
   
            CL_LOG(CL_LOG_INFO,"shutdown handle service thread ...");
            thread_settings = handle->service_thread;
            handle->service_thread = NULL; /* set thread pointer to NULL because other threads use this pointer */
            ret_val = cl_thread_list_delete_thread(cl_com_thread_list,thread_settings );
            if (ret_val != CL_RETVAL_OK) {
               CL_LOG_STR(CL_LOG_ERROR,"error shutting down handle service thread", cl_get_error_text(ret_val));
            } else {
               CL_LOG(CL_LOG_INFO,"shutdown handle service thread OK");
            }
   
            /* all threads are down, deleteing condition variables */
            CL_LOG(CL_LOG_INFO,"shutdown handle write condition ...");
            ret_val = cl_thread_delete_thread_condition(&(handle->write_condition));
            if (ret_val != CL_RETVAL_OK) {
               CL_LOG_STR(CL_LOG_ERROR,"error shutting down handle write condition", cl_get_error_text(ret_val));
            } else {
               CL_LOG(CL_LOG_INFO,"shutdown handle write condition OK");
            }
   
            CL_LOG(CL_LOG_INFO,"shutdown handle application read condition ...");
            ret_val = cl_thread_delete_thread_condition(&(handle->app_condition));
            if (ret_val != CL_RETVAL_OK) {
               CL_LOG_STR(CL_LOG_ERROR,"error shutting down handle application read condition", cl_get_error_text(ret_val));
            } else {
               CL_LOG(CL_LOG_INFO,"shutdown handle application read condition OK");
            }

            CL_LOG(CL_LOG_INFO,"shutdown handle read condition ...");
            ret_val = cl_thread_delete_thread_condition(&(handle->read_condition));
            if (ret_val != CL_RETVAL_OK) {
               CL_LOG_STR(CL_LOG_ERROR,"error shutting down handle read condition", cl_get_error_text(ret_val));
            } else {
               CL_LOG(CL_LOG_INFO,"shutdown handle read condition OK");
            }
            break;
         }
      }
   
      /* cleanup all connection list entries */
      cl_raw_list_lock(handle->connection_list);
   
      /* mark each connection to close */
      elem = cl_connection_list_get_first_elem(handle->connection_list);
      while(elem) {
         elem->connection->connection_state = CL_CLOSING;
         elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
         elem = cl_connection_list_get_next_elem(elem);
      }
      cl_raw_list_unlock(handle->connection_list);
   
      cl_connection_list_destroy_connections_to_close(handle->connection_list,1); /* OK */
      
      /* shutdown of service */
      if (handle->service_provider == CL_TRUE) {
         cl_com_connection_request_handler_cleanup(handle->service_handler);
         cl_com_close_connection(&(handle->service_handler));
      }
   
      cl_app_message_queue_cleanup(&(handle->send_message_queue));
      cl_app_message_queue_cleanup(&(handle->received_message_queue));
      cl_connection_list_cleanup(&(handle->connection_list));
      cl_com_free_endpoint(&(handle->local));
   
      cl_string_list_cleanup(&(handle->allowed_host_list));
    
      if (handle->messages_ready_mutex != NULL) {
         ret_val = pthread_mutex_destroy(handle->messages_ready_mutex);
         if (ret_val != EBUSY) {
            free(handle->messages_ready_mutex); 
            handle->messages_ready_mutex = NULL;
         }
      }
      if (handle->connection_list_mutex != NULL) {
         ret_val = pthread_mutex_destroy(handle->connection_list_mutex);
         if (ret_val != EBUSY) {
            free(handle->connection_list_mutex); 
            handle->connection_list_mutex = NULL;
         }
      }
      cl_com_free_handle_statistic(&(handle->statistic));
 
      cl_com_free_debug_client_setup(&(handle->debug_client_setup));

      cl_com_free_ssl_setup(&(handle->ssl_setup));

      free(handle);
      return CL_RETVAL_OK;
   } 
   return ret_val;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_setup_connection()"
int cl_com_setup_connection(cl_com_handle_t* handle, cl_com_connection_t** connection) {
   int ret_val = CL_RETVAL_HANDLE_NOT_FOUND;
   if (handle != NULL) {
      switch(handle->framework) {
         case CL_CT_TCP: {
            ret_val = cl_com_tcp_setup_connection(connection, 
                                                  handle->service_port,
                                                  handle->connect_port,
                                                  handle->data_flow_type,
                                                  handle->auto_close_mode,
                                                  handle->framework,
                                                  CL_CM_DF_BIN,
                                                  handle->tcp_connect_mode);
            break;
         }
         case CL_CT_SSL: {
            ret_val = cl_com_ssl_setup_connection(connection, 
                                                  handle->service_port,
                                                  handle->connect_port,
                                                  handle->data_flow_type,
                                                  handle->auto_close_mode,
                                                  handle->framework,
                                                  CL_CM_DF_BIN,
                                                  handle->tcp_connect_mode,
                                                  handle->ssl_setup);
            break;
         }
         case CL_CT_UNDEFINED: {
            ret_val = CL_RETVAL_UNDEFINED_FRAMEWORK;
            break;
         }
      }
   }
   return ret_val;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_set_auto_close_mode()"
int cl_com_set_auto_close_mode(cl_com_handle_t* handle, cl_xml_connection_autoclose_t  mode ) {
   if (handle == NULL) {
      return CL_RETVAL_PARAMS;
   }
   handle->auto_close_mode = mode;
   switch(mode) {
      case CL_CM_AC_ENABLED: 
          CL_LOG(CL_LOG_INFO,"auto close mode is enabled");
          break;
      case CL_CM_AC_DISABLED:
          CL_LOG(CL_LOG_INFO,"auto close mode is disabled");
          break;
      default:
          CL_LOG(CL_LOG_INFO,"unexpeced auto close mode");
   }
   return CL_RETVAL_OK;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_get_auto_close_mode()"
int cl_com_get_auto_close_mode(cl_com_handle_t* handle, cl_xml_connection_autoclose_t* mode ) {
   if (handle == NULL || mode == NULL ) {
      return CL_RETVAL_PARAMS;
   }
   *mode = handle->auto_close_mode;
   return CL_RETVAL_OK;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_set_max_connection_close_mode()"
int cl_com_set_max_connection_close_mode(cl_com_handle_t* handle, cl_max_count_t mode) {

   if (handle == NULL) {
      return CL_RETVAL_PARAMS;
   }
   handle->max_connection_count_reached = CL_FALSE;
   handle->max_con_close_mode = mode;
   return CL_RETVAL_OK;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_get_max_connection_close_mode()"
int cl_com_get_max_connection_close_mode(cl_com_handle_t* handle, cl_max_count_t* mode) {
   if (handle == NULL|| mode == NULL ) {
      return CL_RETVAL_PARAMS;
   }
   *mode = handle->max_con_close_mode;
   return CL_RETVAL_OK;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_set_max_connections()"
int cl_com_set_max_connections(cl_com_handle_t* handle, unsigned long value) {
   if (handle == NULL) {
      return CL_RETVAL_PARAMS;
   }
   if (value < 1) {
      return CL_RETVAL_PARAMS;
   }
   handle->max_open_connections = value;
   return CL_RETVAL_OK;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_get_max_connections()"
int cl_com_get_max_connections(cl_com_handle_t* handle, unsigned long* value) {
   if (handle == NULL) {
      return CL_RETVAL_PARAMS;
   }  
   if (value == NULL) {
      return CL_RETVAL_PARAMS;
   } 
   *value = handle->max_open_connections;
   return CL_RETVAL_OK;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_get_handle()"
cl_com_handle_t* cl_com_get_handle(const char* component_name, unsigned long component_id) {
   cl_handle_list_elem_t* elem = NULL;
   cl_com_handle_t* ret_handle = NULL;

   if (cl_com_handle_list  == NULL) {
      return NULL;
   }

   if ( component_name == NULL) {
      CL_LOG(CL_LOG_WARNING,"cl_com_get_handle() - parameter error");
      return NULL;
   }

   /* lock list */
   if ( cl_raw_list_lock(cl_com_handle_list) != CL_RETVAL_OK) {
      CL_LOG(CL_LOG_WARNING,"cl_com_get_handle() - lock error");
      return NULL;
   }

   CL_LOG_STR(CL_LOG_INFO,"try to find handle for", component_name);
   if ( component_id != 0) {
      CL_LOG_INT(CL_LOG_INFO,"handle must have id", (int)component_id);
   } else {
      CL_LOG(CL_LOG_INFO,"ignoring component_id");
   }
   elem = cl_handle_list_get_first_elem(cl_com_handle_list);
   while ( elem != NULL) { 
      cl_com_handle_t* handle = elem->handle;

      /* if component id is zero, we just search for the name */
      if (handle->local->comp_id == component_id || component_id == 0) {
         if (strcmp(handle->local->comp_name, component_name) == 0) {
            if ( ret_handle != NULL) {
               CL_LOG(CL_LOG_ERROR,"cl_com_get_handle() - found more than one handle");
            } else {
               ret_handle = handle;
            }
         }
      }
      elem = cl_handle_list_get_next_elem(elem);
   }
   
   /* unlock list */
   if ( cl_raw_list_unlock(cl_com_handle_list ) != CL_RETVAL_OK) {
      CL_LOG(CL_LOG_WARNING,"cl_com_get_handle() - unlock error");
      return NULL;
   }

   if (ret_handle == NULL) {
      CL_LOG(CL_LOG_INFO,"cl_com_get_handle() - handle not found");
   }
   return ret_handle;
}


cl_thread_mode_t cl_commlib_get_thread_state(void) {
   return cl_com_create_threads;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_set_alias_file()"
int cl_com_set_alias_file(const char* alias_file) {
   int ret_val;
   if (alias_file == NULL) {
      return CL_RETVAL_PARAMS;
   }
   
   if (cl_com_host_list != NULL) {
      ret_val = cl_host_list_set_alias_file(cl_com_get_host_list(), alias_file );
   } else {
      ret_val = CL_RETVAL_NO_FRAMEWORK_INIT;
   }
   return ret_val;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_set_alias_file_dirty()"
int cl_com_set_alias_file_dirty(void) {
   int ret_val;
   
   if (cl_com_host_list != NULL) {
      ret_val = cl_host_list_set_alias_file_dirty(cl_com_get_host_list());
   } else {
      ret_val = CL_RETVAL_NO_FRAMEWORK_INIT;
   }
   return ret_val;

}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_append_host_alias()"
int cl_com_append_host_alias(char* local_resolved_name, char* alias_name) {
   int ret_val = CL_RETVAL_OK;
   cl_host_list_data_t* ldata = NULL;
   if (local_resolved_name == NULL || alias_name == NULL) {
      return CL_RETVAL_PARAMS;
   }

   ldata = cl_host_list_get_data(cl_com_get_host_list());
   if (ldata == NULL) {
      return CL_RETVAL_NO_FRAMEWORK_INIT;
   }
   ret_val = cl_host_alias_list_append_host(ldata->host_alias_list, local_resolved_name, alias_name, 1 );
   if (ret_val == CL_RETVAL_OK) {
      CL_LOG(CL_LOG_INFO,"added host alias:");
      CL_LOG_STR(CL_LOG_INFO,"local resolved name:",local_resolved_name );
      CL_LOG_STR(CL_LOG_INFO,"aliased name       :",alias_name);
   }
   return ret_val;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_remove_host_alias()"
int cl_com_remove_host_alias(char* alias_name) {
   int ret_val = CL_RETVAL_OK;
   cl_host_list_data_t* ldata = NULL;
   cl_host_alias_list_elem_t* elem = NULL;
   if ( alias_name == NULL) {
      return CL_RETVAL_PARAMS;
   }

   ldata = cl_host_list_get_data(cl_com_get_host_list());
   if (ldata == NULL) {
      return CL_RETVAL_NO_FRAMEWORK_INIT;
   }

   cl_raw_list_lock(ldata->host_alias_list);
   elem = cl_host_alias_list_get_first_elem(ldata->host_alias_list);
   while(elem) {
      if (strcmp(elem->alias_name,alias_name) == 0) {
         CL_LOG(CL_LOG_INFO,"removing host alias:");
         CL_LOG_STR(CL_LOG_INFO,"local resolved name:",elem->local_resolved_hostname );
         CL_LOG_STR(CL_LOG_INFO,"aliased name       :",elem->alias_name);

         ret_val = cl_host_alias_list_remove_host(ldata->host_alias_list,elem,0);
         cl_raw_list_unlock(ldata->host_alias_list);
         if (ret_val != CL_RETVAL_OK) {
            CL_LOG(CL_LOG_ERROR,"error removing host alias");
         }
         return ret_val;
      }
      elem = cl_host_alias_list_get_next_elem(elem);
   }
   cl_raw_list_unlock(ldata->host_alias_list);
   return CL_RETVAL_UNKNOWN;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_append_known_endpoint_from_name()"
int 
cl_com_append_known_endpoint_from_name(char* unresolved_comp_host, 
                                       char* comp_name, 
                                       unsigned long comp_id, 
                                       int comp_port, 
                                       cl_xml_connection_autoclose_t autoclose,
                                       cl_bool_t is_static) 
{
   int retval = CL_RETVAL_OK;
   int function_return = CL_RETVAL_OK;

   char* resolved_hostname = NULL;
   cl_com_endpoint_t* endpoint = NULL;

   if ( unresolved_comp_host == NULL || comp_name == NULL ) {
      return CL_RETVAL_PARAMS;
   }

   retval = cl_com_cached_gethostbyname(unresolved_comp_host, &resolved_hostname, NULL, NULL, NULL );
   if (retval != CL_RETVAL_OK) {
      CL_LOG_STR(CL_LOG_ERROR,"could not resolve host",unresolved_comp_host);
      return retval;
   }

   endpoint = cl_com_create_endpoint(resolved_hostname,comp_name , comp_id );
   if (endpoint == NULL) {
      free(resolved_hostname); 
      return CL_RETVAL_MALLOC;
   }

   function_return = cl_com_append_known_endpoint(endpoint, comp_port, autoclose , is_static );

   free(resolved_hostname); 
   cl_com_free_endpoint(&endpoint);
   
   return function_return;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_append_known_endpoint()"
int cl_com_append_known_endpoint(cl_com_endpoint_t* endpoint, int service_port, cl_xml_connection_autoclose_t autoclose, cl_bool_t is_static ) {
   if (endpoint == NULL) {
      return CL_RETVAL_PARAMS;
   }
   return cl_endpoint_list_define_endpoint(cl_com_get_endpoint_list(), endpoint, service_port, autoclose, is_static);
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_remove_known_endpoint()"
int cl_com_remove_known_endpoint(cl_com_endpoint_t* endpoint) {
   if (endpoint == NULL) {
      return CL_RETVAL_PARAMS;
   }
   return cl_endpoint_list_undefine_endpoint(cl_com_get_endpoint_list(), endpoint);
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_get_known_endpoint_autoclose_mode()"
int cl_com_get_known_endpoint_autoclose_mode(cl_com_endpoint_t* endpoint, cl_xml_connection_autoclose_t* auto_close_mode ) {
   if (endpoint == NULL || auto_close_mode == NULL) {
      return CL_RETVAL_PARAMS;
   }

   return cl_endpoint_list_get_autoclose_mode(cl_com_get_endpoint_list(), endpoint,  auto_close_mode);

}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_get_known_endpoint_port()"
int cl_com_get_known_endpoint_port(cl_com_endpoint_t* endpoint, int* service_port ) {
   if (endpoint == NULL || service_port == NULL) {
      return CL_RETVAL_PARAMS;
   }

   return cl_endpoint_list_get_service_port(cl_com_get_endpoint_list(), endpoint, service_port);
}



#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_remove_known_endpoint_from_name()"
int cl_com_remove_known_endpoint_from_name(char* unresolved_comp_host, char* comp_name, unsigned long comp_id) {
   int retval = CL_RETVAL_OK;
   int function_return = CL_RETVAL_OK;

   char* resolved_hostname = NULL;
   cl_com_endpoint_t* endpoint = NULL;

   if ( unresolved_comp_host == NULL || comp_name == NULL ) {
      return CL_RETVAL_PARAMS;
   }

   retval = cl_com_cached_gethostbyname(unresolved_comp_host, &resolved_hostname, NULL, NULL, NULL );
   if (retval != CL_RETVAL_OK) {
      CL_LOG_STR(CL_LOG_ERROR,"could not resolve host",unresolved_comp_host);
      return retval;
   }

   endpoint = cl_com_create_endpoint(resolved_hostname,comp_name , comp_id );
   if (endpoint == NULL) {
      free(resolved_hostname); 
      return CL_RETVAL_MALLOC;
   }

   function_return = cl_com_remove_known_endpoint(endpoint);

   free(resolved_hostname); 
   cl_com_free_endpoint(&endpoint);
   
   return function_return;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_get_known_endpoint_autoclose_mode_from_name()"
int cl_com_get_known_endpoint_autoclose_mode_from_name(char* unresolved_comp_host, char* comp_name, unsigned long comp_id, cl_xml_connection_autoclose_t* auto_close_mode ) {
  int retval = CL_RETVAL_OK;
   int function_return = CL_RETVAL_OK;

   char* resolved_hostname = NULL;
   cl_com_endpoint_t* endpoint = NULL;

   if ( unresolved_comp_host == NULL || comp_name == NULL ) {
      return CL_RETVAL_PARAMS;
   }

   retval = cl_com_cached_gethostbyname(unresolved_comp_host, &resolved_hostname, NULL, NULL, NULL );
   if (retval != CL_RETVAL_OK) {
      CL_LOG_STR(CL_LOG_ERROR,"could not resolve host",unresolved_comp_host);
      return retval;
   }

   endpoint = cl_com_create_endpoint(resolved_hostname,comp_name , comp_id );
   if (endpoint == NULL) {
      free(resolved_hostname); 
      return CL_RETVAL_MALLOC;
   }

   function_return = cl_com_get_known_endpoint_autoclose_mode(endpoint, auto_close_mode);

   free(resolved_hostname); 
   cl_com_free_endpoint(&endpoint);
   
   return function_return;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_get_known_service_port_from_name()"
int cl_com_get_known_endpoint_port_from_name(char* unresolved_comp_host, char* comp_name, unsigned long comp_id, int* service_port ) {
   int retval = CL_RETVAL_OK;
   int function_return = CL_RETVAL_OK;

   char* resolved_hostname = NULL;
   cl_com_endpoint_t* endpoint = NULL;

   if ( unresolved_comp_host == NULL || comp_name == NULL ) {
      return CL_RETVAL_PARAMS;
   }

   retval = cl_com_cached_gethostbyname(unresolved_comp_host, &resolved_hostname, NULL, NULL, NULL );
   if (retval != CL_RETVAL_OK) {
      CL_LOG_STR(CL_LOG_ERROR,"could not resolve host",unresolved_comp_host);
      return retval;
   }

   endpoint = cl_com_create_endpoint(resolved_hostname,comp_name , comp_id );
   if (endpoint == NULL) {
      free(resolved_hostname); 
      return CL_RETVAL_MALLOC;
   }

   function_return = cl_com_get_known_endpoint_port(endpoint,service_port);

   free(resolved_hostname); 
   cl_com_free_endpoint(&endpoint);
   
   return function_return;
}



#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_get_service_fd()"
int cl_com_set_handle_fds(cl_com_handle_t* handle, fd_set* file_descriptor_set) {
   int fd = -1;
   int ret_val = CL_RETVAL_UNKNOWN;
   cl_connection_list_elem_t* elem = NULL;
   cl_com_connection_t* connection = NULL;

   if (handle == NULL || file_descriptor_set == NULL) {
      return CL_RETVAL_PARAMS;
   }

   if (handle->service_handler != NULL) {
      cl_com_connection_get_fd(handle->service_handler, &fd);
      if (fd >= 0) {
         FD_SET(fd, file_descriptor_set);
         ret_val = CL_RETVAL_OK;
      }
   }

   cl_raw_list_lock(handle->connection_list);
   elem = cl_connection_list_get_first_elem(handle->connection_list);    
   while(elem) {
      connection = elem->connection;
      fd = -1;
      cl_com_connection_get_fd(connection, &fd);
      if (fd >= 0) {
         FD_SET(fd,file_descriptor_set);
         ret_val = CL_RETVAL_OK;
      }
      elem = cl_connection_list_get_next_elem(elem);
   }
   cl_raw_list_unlock(handle->connection_list);
   
   return ret_val;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_get_service_port()"
int cl_com_get_service_port(cl_com_handle_t* handle, int* port) {
   if (handle == NULL || port == NULL) {
      return CL_RETVAL_PARAMS;
   }
   if (handle->service_provider == CL_FALSE) {
      CL_LOG(CL_LOG_WARNING,"no service running");
      *port = -1;
      return CL_RETVAL_UNKNOWN;
   }

   if (handle->service_handler == NULL) {
      CL_LOG(CL_LOG_ERROR,"no service handler found");
      *port = -1;
      return CL_RETVAL_UNKNOWN;
   }

   return cl_com_connection_get_service_port(handle->service_handler, port);
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_set_synchron_receive_timeout()"
int cl_com_set_synchron_receive_timeout(cl_com_handle_t* handle, int timeout) {
   if (handle == NULL || timeout <= 0 ) {
      CL_LOG(CL_LOG_ERROR,"error setting synchron receive timeout");
      return CL_RETVAL_PARAMS;
   }
   CL_LOG_INT(CL_LOG_INFO,"setting synchron receive timeout to", (int)timeout);
   handle->synchron_receive_timeout = timeout;
   return CL_RETVAL_OK;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_get_connect_port()"
int cl_com_get_connect_port(cl_com_handle_t* handle, int* port) {
   if (handle == NULL || port == NULL) {
      return CL_RETVAL_PARAMS;
   }

   if ( handle->connect_port > 0 ) { 
      *port = handle->connect_port;
      return CL_RETVAL_OK;
   } else {
      return CL_RETVAL_UNKNOWN;
   }
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_add_allowed_host()"
int cl_com_add_allowed_host(cl_com_handle_t* handle, char* hostname) {
   int retval = CL_RETVAL_OK;
   char* resolved_name = NULL;
   if (handle == NULL) {
      CL_LOG(CL_LOG_ERROR,"no handle specified");
      return CL_RETVAL_PARAMS;
   }
   if (hostname == NULL) {
      CL_LOG(CL_LOG_ERROR,"no host specified");
      return CL_RETVAL_PARAMS;
   }
   retval = cl_com_cached_gethostbyname(hostname, &resolved_name, NULL, NULL, NULL );
   if (retval != CL_RETVAL_OK) {
      CL_LOG_STR(CL_LOG_ERROR,"could not resolve host",hostname);
      return retval;
   }
   free(resolved_name);
   resolved_name = NULL;
   retval = cl_string_list_append_string(handle->allowed_host_list, hostname ,1);
   if (retval != CL_RETVAL_OK) {
      CL_LOG_STR(CL_LOG_WARNING,"could not add host to allowed host list:", hostname);
   } else {
      CL_LOG_STR(CL_LOG_INFO,"added host to allowed host list:", hostname);
   }
   return retval;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_remove_allowed_host()"
int cl_com_remove_allowed_host(cl_com_handle_t* handle, char* hostname) {
   if (handle == NULL) {
      CL_LOG(CL_LOG_ERROR,"no handle specified");
      return CL_RETVAL_PARAMS;
   }
   if (hostname == NULL) {
      CL_LOG(CL_LOG_ERROR,"no host specified");
      return CL_RETVAL_PARAMS;
   }
   return cl_string_list_remove_string(handle->allowed_host_list, hostname ,1);
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_trigger()"
int cl_commlib_trigger(cl_com_handle_t* handle, int synchron) {

   cl_commlib_check_callback_functions();
   if (handle == NULL) {
      return CL_RETVAL_PARAMS;
   }
   switch(cl_com_create_threads) {
      case CL_NO_THREAD:
         return cl_com_trigger(handle, synchron);
      case CL_RW_THREAD: {
         int ret_val = CL_RETVAL_OK;
         /* application has nothing to do, wait for next message */
         pthread_mutex_lock(handle->messages_ready_mutex);
         if ((handle->messages_ready_for_read == 0) && (synchron == 1)) {
            CL_LOG(CL_LOG_INFO,"NO MESSAGES to READ, WAITING ...");
            pthread_mutex_unlock(handle->messages_ready_mutex);
            ret_val = cl_thread_wait_for_thread_condition(handle->app_condition ,
                                                         handle->select_sec_timeout,
                                                         handle->select_usec_timeout);
         } else {
            pthread_mutex_unlock(handle->messages_ready_mutex);
         }
         if (ret_val != CL_RETVAL_OK) {
            return ret_val;
         }
         return CL_RETVAL_THREADS_ENABLED;
      }   
   }
   return CL_RETVAL_UNKNOWN;
}

/* this function is used for no thread implementation and must be called 
   permanent to get data into the data structures and lists */
#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_trigger()"
static int cl_com_trigger(cl_com_handle_t* handle, int synchron) {
   cl_connection_list_elem_t* elem = NULL;
   
   struct timeval now;
   int retval = CL_RETVAL_OK;
   cl_com_connection_t* the_handler = NULL;
   char tmp_string[1024];


   if (handle == NULL) {
      CL_LOG(CL_LOG_ERROR,"no handle specified");
      return CL_RETVAL_PARAMS;
   }
 
   /* when threads enabled: this is done by cl_com_trigger_thread() - OK */
   if (cl_commlib_get_thread_state() == CL_NO_THREAD) {
      cl_com_host_list_refresh(cl_com_get_host_list());
      cl_com_endpoint_list_refresh(cl_com_get_endpoint_list());
   }

   /* remove broken connections */
   /* when threads enabled: this is done by cl_com_handle_service_thread() - OK */
   cl_connection_list_destroy_connections_to_close(handle->connection_list,1); /* OK */

   /* calculate statistics each second */
   gettimeofday(&now,NULL);
   if (handle->statistic->last_update.tv_sec != now.tv_sec) {
      cl_commlib_calculate_statistic(handle, CL_FALSE, 1); /* do not force update */
      cl_commlib_handle_debug_clients(handle, CL_TRUE);
   }


   /* check number of connections */
   cl_commlib_check_connection_count(handle);


   /* get service handle for virtual select call */
   if (handle->do_shutdown == 0 && handle->max_connection_count_reached == CL_FALSE) {
      the_handler = handle->service_handler;
   } else {
      the_handler = NULL;
   }
  
   /* do virtual select */
   if (synchron == 1) {
      retval = cl_com_open_connection_request_handler(handle->framework, 
                                             handle->connection_list, 
                                             the_handler,
                                             handle->select_sec_timeout,
                                             handle->select_usec_timeout,
                                             CL_RW_SELECT);
   }
   else {
      retval = cl_com_open_connection_request_handler(handle->framework, 
                                             handle->connection_list, 
                                             the_handler,
                                             0,
                                             0,
                                             CL_RW_SELECT);
         
   }


   /* read / write messages */
   cl_raw_list_lock(handle->connection_list);

   elem = cl_connection_list_get_first_elem(handle->connection_list);     
   
   while(elem) {

      if (elem->connection->connection_state == CL_DISCONNECTED) {
         /* open connection if there are messages to send */
         if ( cl_raw_list_get_elem_count(elem->connection->send_message_list) > 0) {
            CL_LOG(CL_LOG_INFO,"setting connection state to CL_OPENING");
            elem->connection->connection_state = CL_OPENING;
         }
      }

      if (elem->connection->connection_state == CL_OPENING) {
         int return_value;
         /* trigger connect */

         if (elem->connection->data_read_flag == CL_COM_DATA_READY ||
            (elem->connection->fd_ready_for_write == CL_COM_DATA_READY && elem->connection->data_write_flag == CL_COM_DATA_READY) ) {

            return_value = cl_com_open_connection(elem->connection, handle->open_connection_timeout,NULL , NULL, NULL, NULL);

            if (return_value != CL_RETVAL_OK && 
                return_value != CL_RETVAL_UNCOMPLETE_WRITE) {
               CL_LOG_STR(CL_LOG_ERROR,"could not open connection:",cl_get_error_text(return_value));
               elem->connection->connection_state = CL_CLOSING;
               elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
            }
            if (return_value != CL_RETVAL_OK && cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
               CL_LOG(CL_LOG_WARNING,"setting connection state to closing");
               elem->connection->connection_state = CL_CLOSING;
               elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
            } 
         }  else {
            /* check timeouts */
            if ( elem->connection->read_buffer_timeout_time != 0) {
               if ( now.tv_sec >= elem->connection->read_buffer_timeout_time || cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
                  CL_LOG(CL_LOG_ERROR,"read timeout for connection opening");
                  elem->connection->connection_state = CL_CLOSING;
                  elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
               }
            }
            if ( elem->connection->write_buffer_timeout_time != 0) {
               if ( now.tv_sec >= elem->connection->write_buffer_timeout_time || cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
                  CL_LOG(CL_LOG_ERROR,"write timeout for connection opening");
                  elem->connection->connection_state = CL_CLOSING;
                  elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
               }
            }
         }
      }
 
      if (elem->connection->connection_state == CL_ACCEPTING) {
         int return_value;
         if (elem->connection->data_read_flag == CL_COM_DATA_READY ||
            (elem->connection->fd_ready_for_write == CL_COM_DATA_READY && elem->connection->data_write_flag == CL_COM_DATA_READY) ) {

            return_value = cl_com_connection_complete_accept(elem->connection,handle->open_connection_timeout,1);
            if (return_value != CL_RETVAL_OK && 
                return_value != CL_RETVAL_UNCOMPLETE_READ && 
                return_value != CL_RETVAL_UNCOMPLETE_WRITE && 
                return_value != CL_RETVAL_SELECT_ERROR ) {
               CL_LOG_STR(CL_LOG_ERROR,"connection accept error:",cl_get_error_text(return_value));
               elem->connection->connection_state = CL_CLOSING;
               elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
            }
            if (return_value != CL_RETVAL_OK && cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
               elem->connection->connection_state = CL_CLOSING;
               elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
            } 
            if (return_value == CL_RETVAL_OK) {
               elem->connection->connection_state = CL_CONNECTING;
               elem->connection->connection_sub_state = CL_COM_READ_INIT;
               elem->connection->data_read_flag = CL_COM_DATA_NOT_READY;
            }
         } else {
            /* check timeouts */
            if ( elem->connection->read_buffer_timeout_time != 0) {
               if ( now.tv_sec >= elem->connection->read_buffer_timeout_time ) {
                  CL_LOG(CL_LOG_ERROR,"accept timeout for connection");
                  elem->connection->connection_state = CL_CLOSING;
                  elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
               }
            }
            if ( elem->connection->write_buffer_timeout_time != 0) {
               if ( now.tv_sec >= elem->connection->write_buffer_timeout_time ) {
                  CL_LOG(CL_LOG_ERROR,"accept timeout for connection");
                  elem->connection->connection_state = CL_CLOSING;
                  elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
               }
            }
         }
      }

      if (elem->connection->connection_state == CL_CONNECTING) {
         int return_value;
         if (elem->connection->data_read_flag == CL_COM_DATA_READY ||
            (elem->connection->fd_ready_for_write == CL_COM_DATA_READY && elem->connection->data_write_flag == CL_COM_DATA_READY) ) {
            return_value = cl_com_connection_complete_request(elem->connection,handle->open_connection_timeout,1, CL_RW_SELECT);
            if (return_value != CL_RETVAL_OK && 
                return_value != CL_RETVAL_UNCOMPLETE_READ && 
                return_value != CL_RETVAL_UNCOMPLETE_WRITE && 
                return_value != CL_RETVAL_SELECT_ERROR ) {
               CL_LOG_STR(CL_LOG_ERROR,"connection establish error:",cl_get_error_text(return_value));
               elem->connection->connection_state = CL_CLOSING;
               elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
            }
            if (return_value != CL_RETVAL_OK && cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
               elem->connection->connection_state = CL_CLOSING;
               elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
            } 
            if ( elem->connection->connection_state == CL_CONNECTED ) {
               cl_commlib_finish_request_completeness(elem->connection);
               /* connection is now in connect state, do select before next reading */
               elem->connection->data_read_flag = CL_COM_DATA_NOT_READY;
            }
         } else {
            /* check timeouts */
            if ( elem->connection->read_buffer_timeout_time != 0) {
               if ( now.tv_sec >= elem->connection->read_buffer_timeout_time || cl_com_get_ignore_timeouts_flag() == CL_TRUE  ) {
                  CL_LOG(CL_LOG_ERROR,"read timeout for connection completion");
                  elem->connection->connection_state = CL_CLOSING;
                  elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
               }
            }
            if ( elem->connection->write_buffer_timeout_time != 0) {
               if ( now.tv_sec >= elem->connection->write_buffer_timeout_time || cl_com_get_ignore_timeouts_flag() == CL_TRUE ) {
                  CL_LOG(CL_LOG_ERROR,"write timeout for connection completion");
                  elem->connection->connection_state = CL_CLOSING;
                  elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
               }
            }
         }
      }

      if (elem->connection->connection_state == CL_CONNECTED) {
         /* check ack timeouts */
         int return_value = cl_commlib_handle_connection_ack_timeouts(elem->connection); 

         if (elem->connection->data_read_flag == CL_COM_DATA_READY && 
             elem->connection->ccrm_sent      == 0                 && 
             elem->connection->ccrm_received  == 0                    ) {
            return_value = cl_commlib_handle_connection_read(elem->connection);
            if ( return_value != CL_RETVAL_OK && 
                 return_value != CL_RETVAL_UNCOMPLETE_READ && 
                 return_value != CL_RETVAL_SELECT_ERROR ) {
               elem->connection->connection_state = CL_CLOSING;
               elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
               CL_LOG_STR(CL_LOG_ERROR,"read from connection: setting close flag! Reason:", cl_get_error_text(return_value));
               snprintf(tmp_string, 1024, MSG_CL_COMMLIB_CLOSING_SSU,
                        elem->connection->remote->comp_host,
                        elem->connection->remote->comp_name,
                        sge_u32c(elem->connection->remote->comp_id));
               cl_commlib_push_application_error(CL_LOG_ERROR, return_value,tmp_string);
            }
            if (return_value != CL_RETVAL_OK && cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
               elem->connection->connection_state = CL_CLOSING;
               elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
            }
         } else {
            /* check timeouts */
            if ( elem->connection->read_buffer_timeout_time != 0) {
               if ( now.tv_sec >= elem->connection->read_buffer_timeout_time ) {
                  CL_LOG(CL_LOG_ERROR,"connection read timeout");
                  elem->connection->connection_state = CL_CLOSING;
                  elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
               }
            }
            if ( elem->connection->ccrm_received != 0 ) {
               CL_LOG(CL_LOG_WARNING, "will not read from this connection, because ccrm was received!");
            }
         }
      }

      if (elem->connection->connection_state == CL_CONNECTED) {
         int return_value = CL_RETVAL_OK;
         if (elem->connection->fd_ready_for_write == CL_COM_DATA_READY &&
             elem->connection->data_write_flag    == CL_COM_DATA_READY &&
             elem->connection->ccrm_sent == 0 ) {
            return_value = cl_commlib_handle_connection_write(elem->connection);
            if (return_value != CL_RETVAL_OK && 
                return_value != CL_RETVAL_UNCOMPLETE_WRITE && 
                return_value != CL_RETVAL_SELECT_ERROR ) {
               elem->connection->connection_state = CL_CLOSING;
               elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
               CL_LOG_STR(CL_LOG_ERROR,"write to connection: setting close flag! Reason:", cl_get_error_text(return_value));
               snprintf(tmp_string, 1024, MSG_CL_COMMLIB_CLOSING_SSU,
                        elem->connection->remote->comp_host,
                        elem->connection->remote->comp_name,
                        sge_u32c(elem->connection->remote->comp_id));
               cl_commlib_push_application_error(CL_LOG_ERROR, return_value, tmp_string);
            }
            if (return_value != CL_RETVAL_OK && cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
               elem->connection->connection_state = CL_CLOSING;
               elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
            }
         } else {
            /* check timeouts */
            if ( elem->connection->write_buffer_timeout_time != 0) {
               if ( now.tv_sec >= elem->connection->write_buffer_timeout_time ) {
                  CL_LOG(CL_LOG_ERROR,"write timeout for connected endpoint");
                  snprintf(tmp_string, 1024, MSG_CL_COMMLIB_CLOSING_SSU,
                           elem->connection->remote->comp_host,
                           elem->connection->remote->comp_name,
                           sge_u32c(elem->connection->remote->comp_id));
                  cl_commlib_push_application_error(CL_LOG_ERROR, CL_RETVAL_SEND_TIMEOUT, tmp_string);
                  elem->connection->connection_state = CL_CLOSING;
                  elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
               }
            }
         }

         /* send connection close response message (if no message are in buffers) */
         if (elem->connection->ccm_received == 1 ) {
            CL_LOG(CL_LOG_INFO,"received ccm");

            CL_LOG_INT(CL_LOG_INFO,"receive buffer:",(int)cl_raw_list_get_elem_count(elem->connection->received_message_list) );
            CL_LOG_INT(CL_LOG_INFO,"send buffer   :",(int)cl_raw_list_get_elem_count(elem->connection->send_message_list) );

            if (cl_raw_list_get_elem_count(elem->connection->send_message_list) == 0 && 
                cl_raw_list_get_elem_count(elem->connection->received_message_list) == 0) {
               elem->connection->ccm_received = 2;
               elem->connection->connection_sub_state = CL_COM_SENDING_CCRM;
               /* disable #if 1 if you want to test a client, that is frozen! */
               cl_commlib_send_ccrm_message(elem->connection);
               CL_LOG(CL_LOG_INFO,"sending ccrm");
            } else {
               CL_LOG(CL_LOG_WARNING,"can't send ccrm, still messages in buffer");
            }
         }
      }
      elem = cl_connection_list_get_next_elem(elem);
   }

   /* now take list entry and add it at least if there are more connections */
   /* this must be done in order to NOT prefer the first connection in list */
   if ( cl_raw_list_get_elem_count(handle->connection_list) > 1) {
         elem = cl_connection_list_get_first_elem(handle->connection_list);
         cl_raw_list_dechain_elem(handle->connection_list,elem->raw_elem);
         cl_raw_list_append_dechained_elem(handle->connection_list,elem->raw_elem);
   }

   cl_raw_list_unlock(handle->connection_list);


   /* when threads enabled: this is done by cl_com_handle_service_thread() */
   if (handle->service_provider                == CL_TRUE && 
       handle->service_handler->data_read_flag == CL_COM_DATA_READY) {

      /* new connection requests */
      cl_com_connection_t* new_con = NULL;
      cl_com_connection_request_handler(handle->service_handler, &new_con);

      if (new_con != NULL) {
         handle->statistic->new_connections = handle->statistic->new_connections + 1;
         new_con->handler = handle->service_handler->handler;
         CL_LOG(CL_LOG_INFO,"adding new client");
         new_con->read_buffer_timeout_time = now.tv_sec + handle->open_connection_timeout;
         cl_connection_list_append_connection(handle->connection_list, new_con,1);
         new_con = NULL;
      }
   }


   return retval;
}



#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_handle_connection_read()"


/* WARNING: connection_list must be locked by caller */
static int cl_commlib_handle_connection_read(cl_com_connection_t* connection) {
   cl_com_message_t* message = NULL;
   cl_message_list_elem_t* message_list_element = NULL;
   unsigned long size = 0;
   int return_value = CL_RETVAL_OK;
   int connect_port = 0;
   struct timeval now;
   cl_bool_t new_message_for_queue = CL_FALSE;

   if (connection == NULL) {
      return CL_RETVAL_PARAMS;
   }

   if (connection->data_flow_type == CL_CM_CT_STREAM) {
      cl_bool_t is_debug_client = CL_FALSE;

      if ( connection->remote != NULL ) {
         if ( connection->remote->comp_name != NULL) {
            if (strcmp(connection->remote->comp_name, CL_COM_DEBUG_CLIENT_NAME) == 0) {
               is_debug_client = CL_TRUE;
            }
         }
      }

      if (is_debug_client == CL_TRUE) {
         int pos = 0;
         gettimeofday(&now,NULL);
         connection->read_buffer_timeout_time = now.tv_sec + connection->handler->read_timeout;
         return_value = cl_com_read(connection, &(connection->data_read_buffer[connection->data_read_buffer_pos]), connection->data_buffer_size - connection->data_read_buffer_pos , &size);
         connection->read_buffer_timeout_time = 0;

         if (return_value != CL_RETVAL_OK && return_value != CL_RETVAL_UNCOMPLETE_READ) {
            return return_value;
         }

         connection->data_read_buffer_pos += size;


         while( connection->data_read_buffer_pos > 0) {
            int unparsed_string_start = -1;
            for (pos = 0; pos < connection->data_read_buffer_pos; pos++) {
               if ( connection->data_read_buffer[pos] == 0 ) {
                  cl_bool_t changed_mode = CL_FALSE;
               
                  unparsed_string_start = pos + 1;
   
                  /* TODO: implement clean commando syntax parser for debug_clients, etc. */
                  if (strcmp("set tag ALL",(char*)connection->data_read_buffer) == 0) {
                     connection->handler->debug_client_setup->dc_mode = CL_DEBUG_CLIENT_ALL;
                     changed_mode = CL_TRUE;
                  }
                  if (strcmp("set tag MSG",(char*)connection->data_read_buffer) == 0) {
                     connection->handler->debug_client_setup->dc_mode = CL_DEBUG_CLIENT_MSG;
                     changed_mode = CL_TRUE;
                  }
                  if (strcmp("set tag APP",(char*)connection->data_read_buffer) == 0) {
                     connection->handler->debug_client_setup->dc_mode = CL_DEBUG_CLIENT_APP;
                     changed_mode = CL_TRUE;
                  }
                  if (strcmp("set dump OFF",(char*)connection->data_read_buffer) == 0) {
                     connection->handler->debug_client_setup->dc_dump_flag = CL_FALSE;
                  }
                  if (strcmp("set dump ON",(char*)connection->data_read_buffer) == 0) {
                     connection->handler->debug_client_setup->dc_dump_flag = CL_TRUE;
                  }
                  if (strcmp("set debug OFF",(char*)connection->data_read_buffer) == 0) {
                     connection->handler->debug_client_setup->dc_app_log_level = 0;
                     changed_mode = CL_TRUE;
                  }
                  if (strcmp("set debug ERROR",(char*)connection->data_read_buffer) == 0) {
                     connection->handler->debug_client_setup->dc_app_log_level = 1;
                     changed_mode = CL_TRUE;
                  }
                  if (strcmp("set debug WARNING",(char*)connection->data_read_buffer) == 0) {
                     connection->handler->debug_client_setup->dc_app_log_level = 2;
                     changed_mode = CL_TRUE;
                  }
                  if (strcmp("set debug INFO",(char*)connection->data_read_buffer) == 0) {
                     connection->handler->debug_client_setup->dc_app_log_level = 3;
                     changed_mode = CL_TRUE;
                  }
                  if (strcmp("set debug DEBUG",(char*)connection->data_read_buffer) == 0) {
                     connection->handler->debug_client_setup->dc_app_log_level = 4;
                     changed_mode = CL_TRUE;
                  }
                  if (strcmp("set debug DPRINTF",(char*)connection->data_read_buffer) == 0) {
                     connection->handler->debug_client_setup->dc_app_log_level = 5;
                     changed_mode = CL_TRUE;
                  }

                  if (changed_mode == CL_TRUE) {
                     switch(connection->handler->debug_client_setup->dc_mode) {
                        case CL_DEBUG_CLIENT_MSG:
                        case CL_DEBUG_CLIENT_OFF: {
                           pthread_mutex_lock(&cl_com_debug_client_callback_func_mutex);
                           if (cl_com_debug_client_callback_func != NULL) {
                              cl_com_debug_client_callback_func(0, connection->handler->debug_client_setup->dc_app_log_level);
                           }
                           pthread_mutex_unlock(&cl_com_debug_client_callback_func_mutex);
                           break;
                        }
                        case CL_DEBUG_CLIENT_ALL:
                        case CL_DEBUG_CLIENT_APP: {
                           pthread_mutex_lock(&cl_com_debug_client_callback_func_mutex);
                           if (cl_com_debug_client_callback_func != NULL) {
                              cl_com_debug_client_callback_func(1, connection->handler->debug_client_setup->dc_app_log_level);
                           }
                           pthread_mutex_unlock(&cl_com_debug_client_callback_func_mutex);
                           break;
                        }
                     }
                  }
                  break; /* found complete string */
               }
            }
            if (unparsed_string_start != -1) {
               int i = 0;
               /* ok we had a complete string, now remove unused string from buffer */
               for (pos = unparsed_string_start; pos < connection->data_read_buffer_pos; pos++) {
                  connection->data_read_buffer[i++] = connection->data_read_buffer[pos];
               }
               connection->data_read_buffer_pos -= unparsed_string_start;
            } else {
               break; /* following string isn't complete */
            }
         }

         /* Touch endpoint, he is still active */
         if ( cl_com_connection_get_connect_port(connection ,&connect_port) == CL_RETVAL_OK) {
            cl_endpoint_list_define_endpoint(cl_com_get_endpoint_list(),
                                             connection->remote, 
                                             connect_port, 
                                             connection->auto_close_type, CL_FALSE );
         }
         gettimeofday(&connection->last_transfer_time,NULL);   /* set receive time */
         connection->statistic->bytes_received = connection->statistic->bytes_received + size;
         connection->statistic->real_bytes_received = connection->statistic->real_bytes_received + size;
         return return_value;
      } else {
         return_value = cl_com_create_message(&message);
         if (return_value != CL_RETVAL_OK) {
            return return_value;   
         }
         gettimeofday(&now,NULL);
         connection->read_buffer_timeout_time = now.tv_sec + connection->handler->read_timeout;
   
         return_value = cl_com_read(connection, connection->data_read_buffer, connection->data_buffer_size, &size);
   
         connection->read_buffer_timeout_time = 0;
   
         if (return_value != CL_RETVAL_OK && return_value != CL_RETVAL_UNCOMPLETE_READ) {
            cl_com_free_message(&message);
            return return_value;
         }
   
         
         CL_LOG_STR(CL_LOG_INFO,"received stream message from:", connection->receiver->comp_host);
         message->message_state  = CL_MS_READY;         /* set message state */
         message->message_mat    = CL_MIH_MAT_NAK;      /* no acknoledge for stream messages */
         message->message_length = size;                    /* set message size */
         gettimeofday(&message->message_receive_time,NULL);   /* set receive time */
   
   
         /* Touch endpoint, he is still active */
         if ( cl_com_connection_get_connect_port(connection ,&connect_port) == CL_RETVAL_OK) {
            cl_endpoint_list_define_endpoint(cl_com_get_endpoint_list(),
                                             connection->remote, 
                                             connect_port, 
                                             connection->auto_close_type, CL_FALSE );
         }
         
         /* set last transfer time of connection */
         memcpy(&connection->last_transfer_time, &message->message_receive_time ,sizeof (struct timeval));
   
         message->message = (cl_byte_t*) malloc(sizeof(cl_byte_t) * size);
         if (message->message == NULL) {
            cl_com_free_message(&message);
            return CL_RETVAL_MALLOC;   
         }
         memcpy(message->message, connection->data_read_buffer , sizeof(cl_byte_t) * size);
         return_value = cl_message_list_append_receive(connection, message, 1);
         if (return_value == CL_RETVAL_OK) {
            if (connection->handler != NULL) { 
               cl_com_handle_t* handle = connection->handler;
               /* increase counter for ready messages */
               pthread_mutex_lock(handle->messages_ready_mutex);
               handle->messages_ready_for_read = handle->messages_ready_for_read + 1;
               cl_app_message_queue_append(handle->received_message_queue, connection, NULL, CL_MIH_MAT_UNDEFINED, NULL, 0,0,0,1);
               pthread_mutex_unlock(handle->messages_ready_mutex);

               
                  cl_thread_trigger_thread_condition(handle->app_condition,1);

            }
         }
         connection->statistic->bytes_received = connection->statistic->bytes_received + size;
         connection->statistic->real_bytes_received = connection->statistic->real_bytes_received + size;
         return return_value;
      }
   }

   if (connection->data_flow_type == CL_CM_CT_MESSAGE) {

      cl_raw_list_lock(connection->received_message_list);

      message = NULL;
      /* try to find actual message */
      message_list_element = cl_message_list_get_least_elem(connection->received_message_list);
      if (message_list_element != NULL) {
         message = message_list_element->message;
         if (message->message_state == CL_MS_READY || message->message_state == CL_MS_PROTOCOL) {
            message = NULL; /* already read, create new message */
         }
      }
     
      if (message == NULL) {
         /* create a new message */
         return_value = cl_com_create_message(&message);
         if (return_value != CL_RETVAL_OK) {
            cl_raw_list_unlock(connection->received_message_list);
            CL_LOG(CL_LOG_ERROR,"error creating new empty read message");
            return return_value;   
         }
         message->message_state = CL_MS_INIT_RCV;
         return_value = cl_message_list_append_receive(connection, message, 0);
         if (return_value != CL_RETVAL_OK) {
            cl_raw_list_unlock(connection->received_message_list);
            CL_LOG(CL_LOG_ERROR,"error appending new empty read message");
            return return_value;   
         }
      } 
         
      if (message->message_state == CL_MS_INIT_RCV) {

         CL_LOG(CL_LOG_INFO,"CL_MS_INIT_RCV");
         connection->data_read_buffer_pos = 0;
         connection->data_read_buffer_processed = 0;
         connection->read_gmsh_header->dl = 0;
         gettimeofday(&now,NULL);
         connection->read_buffer_timeout_time = now.tv_sec + connection->handler->read_timeout;
         message->message_rcv_pointer = 0;
         message->message_state = CL_MS_RCV_GMSH;
      }

      if (message->message_state == CL_MS_RCV_GMSH) {
         CL_LOG(CL_LOG_INFO,"CL_MS_RCV_GMSH");
         size = 0;
         return_value = cl_com_read_GMSH(connection, &size);
         if (return_value != CL_RETVAL_OK ) {
             /* header is not complete, try later */
             cl_raw_list_unlock(connection->received_message_list);
             CL_LOG_STR(CL_LOG_INFO,"cl_com_read_GMSH returned", cl_get_error_text(return_value));

             /* recalculate timeout when some data was received */
             if (size > 0) {
                gettimeofday(&now,NULL);
                connection->read_buffer_timeout_time = now.tv_sec + connection->handler->read_timeout;
                CL_LOG(CL_LOG_INFO,"recalculate read buffer timeout time (CL_MS_RCV_GMSH)");
             }
             return return_value;
         }
         connection->statistic->real_bytes_received = connection->statistic->real_bytes_received + connection->data_read_buffer_pos;
         connection->data_read_buffer_pos = 0;
         connection->data_read_buffer_processed = 0;
         message->message_state = CL_MS_RCV_MIH;
      }

      if (message->message_state == CL_MS_RCV_MIH) {
         cl_com_MIH_t* mih_message = NULL;
         CL_LOG(CL_LOG_INFO,"CL_MS_RCV_MIH");
         size = connection->read_gmsh_header->dl - (connection->data_read_buffer_pos - connection->data_read_buffer_processed);
         if ( (size + connection->data_read_buffer_pos) >= connection->data_buffer_size ) {
            CL_LOG(CL_LOG_ERROR,"stream buffer to small");
            return CL_RETVAL_STREAM_BUFFER_OVERFLOW;
         }
         if (size > 0) {
            unsigned long data_read = 0;
            return_value = cl_com_read(connection, &(connection->data_read_buffer[(connection->data_read_buffer_pos)]), size, &data_read);

            connection->data_read_buffer_pos = connection->data_read_buffer_pos + data_read;
            if (return_value != CL_RETVAL_OK) {
               cl_raw_list_unlock(connection->received_message_list);
               CL_LOG_STR(CL_LOG_INFO,"cl_com_read returned:", cl_get_error_text(return_value));

               /* recalculate timeout when some data was received */
               if (data_read > 0) {
                  gettimeofday(&now,NULL);
                  connection->read_buffer_timeout_time = now.tv_sec + connection->handler->read_timeout;
                  CL_LOG(CL_LOG_INFO,"recalculate read buffer timeout time (CL_MS_RCV_MIH)");
               }
               return return_value;
            }
         }
         connection->statistic->real_bytes_received = connection->statistic->real_bytes_received + connection->data_read_buffer_pos;
         return_value = cl_xml_parse_MIH(&(connection->data_read_buffer[(connection->data_read_buffer_processed)]),connection->read_gmsh_header->dl, &mih_message);

         if (return_value != CL_RETVAL_OK ) {
             cl_raw_list_unlock(connection->received_message_list);
             CL_LOG_STR(CL_LOG_INFO,"cl_xml_parse_MIH returned", cl_get_error_text(return_value));
             return return_value;
         }

         /* TODO: check version */
         message->message_id          = mih_message->mid;
         message->message_length      = mih_message->dl;
         message->message_df          = mih_message->df;
         message->message_mat         = mih_message->mat;
         message->message_tag         = mih_message->tag;
         message->message_response_id = mih_message->rid;
         cl_com_free_mih_message(&mih_message);
         message->message = (cl_byte_t*) malloc( sizeof(cl_byte_t) * message->message_length );
         if (message->message == NULL) {
            cl_raw_list_unlock(connection->received_message_list);
            return CL_RETVAL_MALLOC;
         }
         message->message_state = CL_MS_RCV;
      }
      
      if (message->message_state == CL_MS_RCV) {
         CL_LOG(CL_LOG_INFO,"CL_MS_RCV");

         size = 0;
         /* is message already complete received ? */
         if (message->message_rcv_pointer < message->message_length) {
            return_value = cl_com_read(connection, 
                                       &(message->message[message->message_rcv_pointer]),
                                       message->message_length - message->message_rcv_pointer,
                                       &size);
            message->message_rcv_pointer = message->message_rcv_pointer + size;
            if (return_value != CL_RETVAL_OK) {
               cl_raw_list_unlock(connection->received_message_list);
               CL_LOG_STR(CL_LOG_INFO,"cl_com_read returned:", cl_get_error_text(return_value));

               /* recalculate timeout when some data was received */
               if (size > 0) {
                  gettimeofday(&now,NULL);
                  connection->read_buffer_timeout_time = now.tv_sec + connection->handler->read_timeout;
                  CL_LOG(CL_LOG_INFO,"recalculate read buffer timeout time (CL_MS_RCV)");
               }

               return return_value;
            }
         }

         CL_LOG_STR_STR_INT(CL_LOG_INFO, "received message from:", connection->receiver->comp_host,
                                                                      connection->receiver->comp_name,
                                                                      (int)connection->receiver->comp_id); 
    
         gettimeofday(&message->message_receive_time,NULL);
         /* set last transfer time of connection */
         memcpy(&connection->last_transfer_time,&message->message_receive_time,sizeof (struct timeval));

 
         connection->read_buffer_timeout_time = 0;

         connection->statistic->bytes_received = connection->statistic->bytes_received + message->message_length;
         connection->statistic->real_bytes_received = connection->statistic->real_bytes_received + message->message_length;

         connection->last_received_message_id = message->message_id;

         if (connection->connection_state == CL_CONNECTED) {
            int connect_port = 0;
            /* Touch endpoint, he is still active */
            if (cl_com_connection_get_connect_port(connection,&connect_port) == CL_RETVAL_OK) {
               return_value = cl_endpoint_list_define_endpoint(cl_com_get_endpoint_list(),
                                                               connection->remote, 
                                                               connect_port, 
                                                               connection->auto_close_type,
                                                               CL_FALSE);
            }
         }

         switch(message->message_df) {
            case CL_MIH_DF_BIN:
               CL_LOG(CL_LOG_INFO,"received binary message");
               message->message_state = CL_MS_READY;
               gettimeofday(&(message->message_remove_time),NULL);
               cl_com_add_debug_message(connection, NULL, message);
               /* we have a new message for the read queue */
               new_message_for_queue = CL_TRUE;
               break;
            case CL_MIH_DF_XML:
               CL_LOG(CL_LOG_INFO,"received XML message");
               message->message_state = CL_MS_READY;
               gettimeofday(&(message->message_remove_time),NULL);
               cl_com_add_debug_message(connection, NULL, message);
               /* we have a new message for the read queue */
               new_message_for_queue = CL_TRUE;
               break;
            default:
               CL_LOG(CL_LOG_INFO,"received protocol message");
               message->message_state = CL_MS_PROTOCOL;
               break;
         }
      }

#ifdef CL_DO_SEND_ACK_AT_COMMLIB_LAYER
      /* send a message acknowledge when message arives at commlib layer */
      if (message->message_state == CL_MS_READY) {
         /* send acknowledge for CL_MIH_MAT_ACK type (application removed message from buffer) */
         if ( message->message_mat == CL_MIH_MAT_ACK) {
            cl_commlib_send_ack_message(connection, message);
         } 
      }
#endif /* CL_DO_SEND_ACK_AT_COMMLIB_LAYER */

      if (message->message_state == CL_MS_PROTOCOL) {
         cl_com_AM_t*            ack_message       = NULL;
         cl_com_SIM_t*           sim_message       = NULL;
         cl_com_SIRM_t*          sirm_message      = NULL;
         cl_com_CCM_t*           ccm_message       = NULL;
         cl_com_CCRM_t*          ccrm_message      = NULL;
         cl_com_message_t*       sent_message      = NULL;
         cl_message_list_elem_t* message_list_elem = NULL;
         unsigned long           run_time = 0;

         switch(message->message_df) {
            case CL_MIH_DF_SIM: {
               return_value = cl_message_list_remove_receive(connection, message, 0);
               cl_raw_list_unlock(connection->received_message_list);
               if (return_value != CL_RETVAL_OK) {
                  return return_value;
               }
               cl_com_add_debug_message(connection, NULL, message);

               return_value = cl_xml_parse_SIM((unsigned char*)message->message, message->message_length, &sim_message);
               if (return_value != CL_RETVAL_OK) {
                  cl_com_free_message(&message);
                  return return_value;
               }
               CL_LOG(CL_LOG_INFO,"received SIM message");

               if (connection->handler != NULL) { 
                  cl_com_handle_t* handle = connection->handler;
                  char* application_info = "not available";
                  
                  /* we force an statistic update */
                  cl_commlib_calculate_statistic(handle,CL_TRUE,0);
                  if ( handle->statistic->application_info != NULL ) {
                     application_info = handle->statistic->application_info;
                  }


                  gettimeofday(&now,NULL);
                  run_time = (unsigned long)now.tv_sec - handle->start_time.tv_sec;


                  cl_commlib_send_sirm_message(                connection,
                                                               message,
                                               (unsigned long) handle->start_time.tv_sec,
                                                               run_time,
                                                               handle->statistic->unread_message_count,
                                                               handle->statistic->unsend_message_count,
                                                               handle->statistic->nr_of_connections, 
                                                               handle->statistic->application_status,
                                                               application_info);
               } else {
                  cl_commlib_send_sirm_message(connection, message, 0, 0 ,0 ,0 , 0, 0, "get status info error");
               }
               cl_com_free_message(&message);
               cl_com_free_sim_message(&sim_message);
               return CL_RETVAL_OK;
            }
            case CL_MIH_DF_SIRM: {
               return_value = cl_message_list_remove_receive(connection, message, 0);
               cl_raw_list_unlock(connection->received_message_list);
               if (return_value != CL_RETVAL_OK) {
                  return return_value;
               }
               cl_com_add_debug_message(connection, NULL, message);

               return_value = cl_xml_parse_SIRM((unsigned char*)message->message, message->message_length, &sirm_message);
               if (return_value != CL_RETVAL_OK) {
                  cl_com_free_message(&message);
                  return return_value;
               }
               CL_LOG_INT(CL_LOG_INFO,"received SIRM message for sent SIM message with id:", (int)sirm_message->mid);

               /* set sirm flag for sent SIM message with ack id from sirm */
               cl_raw_list_lock(connection->send_message_list);
               message_list_elem = cl_message_list_get_first_elem(connection->send_message_list);
               sent_message = NULL;
               while (message_list_elem != NULL) {
                  sent_message = message_list_elem->message;
                  if (sent_message->message_state == CL_MS_PROTOCOL) {
                     if (sent_message->message_id == sirm_message->mid) {
                        break;
                     }
                  } 
                  message_list_elem = cl_message_list_get_next_elem(message_list_elem);
               }
               if (sent_message == NULL) {
                  CL_LOG_INT(CL_LOG_ERROR,"got SIRM message for unexpected message id",(int)sirm_message->mid);
                  cl_com_free_sirm_message(&sirm_message);
               } else {
                  CL_LOG_INT(CL_LOG_INFO,"got SIRM message for SIM message id",(int)sirm_message->mid);
                  sent_message->message_sirm = sirm_message;
               }
               cl_raw_list_unlock(connection->send_message_list);

               cl_com_free_message(&message);
               return CL_RETVAL_OK;
            }
            case CL_MIH_DF_AM: {
               return_value = cl_message_list_remove_receive(connection, message, 0);
               cl_raw_list_unlock(connection->received_message_list);
               if (return_value != CL_RETVAL_OK) {
                  return return_value;
               }
               cl_com_add_debug_message(connection, NULL, message);
               
               return_value = cl_xml_parse_AM((unsigned char*)message->message, message->message_length, &ack_message);
               if (return_value != CL_RETVAL_OK) {
                  cl_com_free_message(&message);
                  return return_value;
               }
               CL_LOG_INT(CL_LOG_INFO,"received ACK message. Id of ACK message:", (int)message->message_id);

               /* set ack flag for sent message with ack id from send_message_list */
               cl_raw_list_lock(connection->send_message_list);
               message_list_elem = cl_message_list_get_first_elem(connection->send_message_list);
               sent_message = NULL;
               while (message_list_elem != NULL) {
                  sent_message = message_list_elem->message;
                  if (sent_message->message_state == CL_MS_PROTOCOL) {
                     if (sent_message->message_id == ack_message->mid) {
                        break;
                     }
                  } 
                  message_list_elem = cl_message_list_get_next_elem(message_list_elem);
               }
               if (sent_message == NULL) {
                  CL_LOG_INT(CL_LOG_ERROR,"got ACK message for unexpected message id",(int)ack_message->mid);
               } else {
                  CL_LOG_INT(CL_LOG_INFO,"got ACK message for message id",(int)ack_message->mid);
                  sent_message->message_ack_flag = 1;
               }
               cl_raw_list_unlock(connection->send_message_list);


               cl_com_free_message(&message);
               cl_com_free_am_message(&ack_message);
               return CL_RETVAL_OK;
            }
            case CL_MIH_DF_CCM: {
               return_value = cl_message_list_remove_receive(connection, message,0);
               cl_raw_list_unlock(connection->received_message_list);
               if (return_value != CL_RETVAL_OK) {
                  return return_value;
               }
               cl_com_add_debug_message(connection, NULL, message);

               return_value = cl_xml_parse_CCM((unsigned char*)message->message, message->message_length, &ccm_message);
               if (return_value != CL_RETVAL_OK) {
                  cl_com_free_message(&message);
                  return return_value;
               }
               if (connection->connection_state  == CL_CONNECTED) {
                  if (connection->connection_sub_state == CL_COM_WORK) {
                     CL_LOG(CL_LOG_INFO,"received connection close message");
                     connection->ccm_received = 1;
                  } else {
                     if ( connection->was_accepted == CL_TRUE) {
                        CL_LOG(CL_LOG_ERROR,"received connection close message from client while sending ccm message - ignoring");
                     } else {
                        CL_LOG(CL_LOG_INFO,"received connection close message");
                        connection->ccm_received = 1;
                     }
                  }
               }
               cl_com_free_message(&message);
               cl_com_free_ccm_message(&ccm_message);
               return CL_RETVAL_OK;
            }
            case CL_MIH_DF_CCRM: {
               return_value = cl_message_list_remove_receive(connection, message, 0);
               cl_raw_list_unlock(connection->received_message_list);
               if (return_value != CL_RETVAL_OK) {
                  return return_value;
               }
               cl_com_add_debug_message(connection, NULL, message);

               return_value = cl_xml_parse_CCRM((unsigned char*)message->message, message->message_length, &ccrm_message);
               if (return_value != CL_RETVAL_OK) {
                  cl_com_free_message(&message);
                  return return_value;
               }
               if (connection->connection_state == CL_CONNECTED) {
                  CL_LOG(CL_LOG_INFO,"received connection close response message");
                  connection->ccrm_received = 1;
                  connection->connection_sub_state = CL_COM_DONE;  /* CLOSE message */
                  connection->data_write_flag = CL_COM_DATA_NOT_READY;
               } 

               cl_com_free_message(&message);
               cl_com_free_ccrm_message(&ccrm_message);
               return CL_RETVAL_OK;
            }
            default: {
               CL_LOG(CL_LOG_ERROR,"reseived unsupported protocol message");
               return_value = cl_message_list_remove_receive(connection, message, 0);
               if (return_value == CL_RETVAL_OK) {
                  cl_com_free_message(&message);
               }
               cl_com_add_debug_message(connection, NULL, message);

               break;
            }
         }
      }

      cl_raw_list_unlock(connection->received_message_list);

      if (new_message_for_queue == CL_TRUE) {
         if (connection->handler != NULL) { 
            /* increase counter for ready messages */
            pthread_mutex_lock(connection->handler->messages_ready_mutex);
            connection->handler->messages_ready_for_read = connection->handler->messages_ready_for_read + 1;
            cl_app_message_queue_append(connection->handler->received_message_queue, connection, NULL, CL_MIH_MAT_UNDEFINED, NULL, 0,0,0,1);
            pthread_mutex_unlock(connection->handler->messages_ready_mutex);


               cl_thread_trigger_thread_condition(connection->handler->app_condition,1);
         }
      }


      if (connection->ccm_received == 1) {
   
          CL_LOG_INT(CL_LOG_WARNING,"receive buffer:",(int)cl_raw_list_get_elem_count(connection->received_message_list) );
          CL_LOG_INT(CL_LOG_WARNING,"send buffer   :",(int)cl_raw_list_get_elem_count(connection->send_message_list) );
          CL_LOG_INT(CL_LOG_WARNING,"ccm_received  :",(int)connection->ccm_received);

          if( cl_raw_list_get_elem_count(connection->send_message_list) == 0 && 
              cl_raw_list_get_elem_count(connection->received_message_list) == 0 ) {
             connection->ccm_received = 2;
             connection->connection_sub_state = CL_COM_SENDING_CCRM;
             /* disable #if 1 if you want to test a client, that is frozen! */
             cl_commlib_send_ccrm_message(connection);
             CL_LOG(CL_LOG_WARNING,"sending ccrm");
          } else {
             CL_LOG(CL_LOG_WARNING,"can't send ccrm, still messages in buffer");
          }
       }
   }
   return return_value;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_handle_connection_ack_timeouts()"

/* This function removes all messages in sent_message_list in protocol state where
   the ack timeout is reached (e.g. ACK messages and SIM messages)*/
static int cl_commlib_handle_connection_ack_timeouts(cl_com_connection_t* connection) {
   cl_com_message_t* message = NULL;
   cl_message_list_elem_t* message_list_elem = NULL;
   cl_message_list_elem_t* next_message_list_elem = NULL;

   int return_value = CL_RETVAL_OK;
   struct timeval now;
   if (connection == NULL) {
      return CL_RETVAL_PARAMS;
   }


   /* stream connections have no ack messages */
   if (connection->data_flow_type == CL_CM_CT_STREAM) {
      return CL_RETVAL_OK;
   }

   if (connection->data_flow_type == CL_CM_CT_MESSAGE) {
      long timeout_time = 0;
      cl_bool_t ignore_timeouts = CL_FALSE;

      /* 
       * This code will send a sim message to the connection in order to test the connection
       * availability(Part 1/2).
       */
      if (connection->check_endpoint_flag == CL_TRUE && connection->check_endpoint_mid == 0) {
         if (connection->ccm_received == 0 &&
             connection->connection_state == CL_CONNECTED &&
             connection->connection_sub_state == CL_COM_WORK) {
            cl_commlib_send_sim_message(connection, &(connection->check_endpoint_mid)); 
            CL_LOG(CL_LOG_INFO,"sending sim to connection to check its availability");
         }
      }

#ifdef CL_DO_COMMLIB_DEBUG
       CL_LOG(CL_LOG_INFO,"checking timeouts for ack messages");
#endif
      /* lock received messages list */
      cl_raw_list_lock(connection->send_message_list);

      /* get current timeout time */
      gettimeofday(&now,NULL);
      ignore_timeouts = cl_com_get_ignore_timeouts_flag();

      message_list_elem = cl_message_list_get_first_elem(connection->send_message_list);
      while(message_list_elem != NULL) {
         message = message_list_elem->message;
         next_message_list_elem = cl_message_list_get_next_elem(message_list_elem);
         if (message->message_state == CL_MS_PROTOCOL) {

            /* 
             * Check out for SIRM's sent to connections when check_endpoint_flag was TRUE 
             * This is Part 2/2 for connection alive tests.
             */
            if (message->message_id == connection->check_endpoint_mid && connection->check_endpoint_mid != 0) {
               if (message->message_sirm != NULL) {
                  CL_LOG(CL_LOG_INFO,"got sirm from checked connection");
                  cl_message_list_remove_send(connection, message, 0);
                  CL_LOG_INT(CL_LOG_INFO,"endpoint runtime:", (int)message->message_sirm->runtime );
                  if (message->message_sirm->info != NULL) {
                     CL_LOG_STR(CL_LOG_INFO,"endpoint info:   ", message->message_sirm->info);
                  }
                  cl_com_free_message(&message);
                  connection->check_endpoint_mid = 0;
                  connection->check_endpoint_flag = CL_FALSE; /* now the next check can be done */
                  message_list_elem = next_message_list_elem;
                  continue;
               }
            } 
            timeout_time = (message->message_send_time).tv_sec + connection->handler->acknowledge_timeout;
            if ( timeout_time <= now.tv_sec ) {
               CL_LOG_INT(CL_LOG_ERROR,"ack timeout for message",(int) message->message_id);
               if (message->message_id == connection->check_endpoint_mid && connection->check_endpoint_mid != 0) {
                  connection->check_endpoint_mid = 0;
                  connection->check_endpoint_flag = CL_FALSE; /* now the next check can be done */
               }
               cl_message_list_remove_send(connection, message, 0);
               cl_com_free_message(&message);
            } else {
               if ( ignore_timeouts == CL_TRUE) {
                  if ( connection->connection_state == CL_CONNECTED &&
                       connection->connection_sub_state == CL_COM_WORK ) {
                     CL_LOG(CL_LOG_INFO,"ignore ack timeout flag is set, but this connection is connected and waiting for ack - continue waiting");
                  } else {
                     CL_LOG(CL_LOG_INFO,"ignore ack timeout flag is set and connection is not connected - ignore timeout");
                     if (message->message_id == connection->check_endpoint_mid && connection->check_endpoint_mid != 0) {
                        connection->check_endpoint_mid = 0;
                        connection->check_endpoint_flag = CL_FALSE; /* now the next check can be done */
                     }
                     cl_message_list_remove_send(connection, message, 0);
                     cl_com_free_message(&message);
                  }
               } 
            }
         }
         message_list_elem = next_message_list_elem;
      }
      /* unlock received messages list */
      cl_raw_list_unlock(connection->send_message_list);
   }
   return return_value;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_check_connection_count()"
static int cl_commlib_check_connection_count(cl_com_handle_t* handle) {
   cl_connection_list_elem_t* elem = NULL;

   if (handle == NULL) {
      return CL_RETVAL_PARAMS;
   }

    /* check max open connections */
   if (handle->max_con_close_mode != CL_ON_MAX_COUNT_OFF) {

      /* first lock the connection list */
      cl_raw_list_lock(handle->connection_list);

      /* check if we exceed the max. connection count */
      if ( cl_raw_list_get_elem_count(handle->connection_list) >= handle->max_open_connections ) {

         /* we have reached max connection count, set flag */
         if ( handle->max_connection_count_reached == CL_FALSE) {
            handle->max_connection_count_reached = CL_TRUE;
            CL_LOG(CL_LOG_ERROR,"max open connection count reached");
         }

         /* still haven't found a connection to close */
         if (  handle->max_connection_count_found_connection_to_close == CL_FALSE ) {
            cl_com_connection_t* oldest_connection = NULL;

            if ( handle->max_con_close_mode == CL_ON_MAX_COUNT_CLOSE_AUTOCLOSE_CLIENTS ) {
               /* try to find the oldest connected connection of type CL_CM_CT_MESSAGE */
               elem = cl_connection_list_get_first_elem(handle->connection_list);
               while(elem) {
                  if (elem->connection->data_flow_type       == CL_CM_CT_MESSAGE                       &&
                      elem->connection->connection_state     == CL_CONNECTED                           &&    
                      elem->connection->connection_sub_state == CL_COM_WORK                            &&
                      elem->connection->auto_close_type      == CL_CM_AC_ENABLED                       && 
                      elem->connection                       != handle->last_receive_message_connection   ) {
                  
                     /* we haven't selected an elem till now, take the first one */
                     if (oldest_connection == NULL) {
                        oldest_connection = elem->connection; 
                     } else {
                        /* check if the actual elem is older than oldest_connection */
                        if (elem->connection->last_transfer_time.tv_sec < oldest_connection->last_transfer_time.tv_sec) {
                           oldest_connection = elem->connection;
                        } else {
                           if (elem->connection->last_transfer_time.tv_sec == oldest_connection->last_transfer_time.tv_sec) {
                              if (elem->connection->last_transfer_time.tv_usec < oldest_connection->last_transfer_time.tv_usec) {
                                 oldest_connection = elem->connection;
                              }
                           }
                        }
                     }
                  }
                  elem = cl_connection_list_get_next_elem(elem);
               }
            }

            /* close connection ( if found ) */
            if ( oldest_connection != NULL ) {
               cl_commlib_send_ccm_message(oldest_connection);
               oldest_connection->connection_sub_state = CL_COM_SENDING_CCM;
               handle->max_connection_count_found_connection_to_close = CL_TRUE;
               CL_LOG_STR(CL_LOG_WARNING,"closing connection to host:", oldest_connection->remote->comp_host );
               CL_LOG_STR(CL_LOG_WARNING,"component name:            ", oldest_connection->remote->comp_name );
               CL_LOG_INT(CL_LOG_WARNING,"component id:              ", (int)oldest_connection->remote->comp_id );
            } else {
               CL_LOG(CL_LOG_WARNING,"can't close any connection");
               handle->max_connection_count_found_connection_to_close = CL_FALSE;
            }
         }

         /* we have found a connection to close, check if closing is still in progress */
         if ( handle->max_connection_count_found_connection_to_close == CL_TRUE ) {
            int is_still_in_list = 0;
            elem = cl_connection_list_get_first_elem(handle->connection_list);
            while(elem) {
               if (elem->connection->data_flow_type       == CL_CM_CT_MESSAGE   &&
                   elem->connection->connection_state     == CL_CONNECTED       &&
                   elem->connection->connection_sub_state != CL_COM_WORK       ) {
                  CL_LOG_STR(CL_LOG_WARNING,"processing close of connection to host:", elem->connection->remote->comp_host );
                  CL_LOG_STR(CL_LOG_WARNING,"component name:            ", elem->connection->remote->comp_name );
                  CL_LOG_INT(CL_LOG_WARNING,"component id:              ", (int)elem->connection->remote->comp_id );
                  is_still_in_list = 1;
                  break;
               }
               elem = cl_connection_list_get_next_elem(elem);
            }
            if ( is_still_in_list == 0) {
               handle->max_connection_count_found_connection_to_close = CL_FALSE;
            } else {
               CL_LOG(CL_LOG_WARNING,"still waiting for closing of connection");
            }
         }
      } else {
         /* we have enough free connections, check if connection count reached flag is set */
         if (handle->max_connection_count_reached == CL_TRUE) {
            handle->max_connection_count_reached = CL_FALSE;
            handle->max_connection_count_found_connection_to_close = CL_FALSE;
            CL_LOG(CL_LOG_ERROR,"new connections enabled again");
         }
      }
      cl_raw_list_unlock(handle->connection_list);
   }
   return CL_RETVAL_OK;
} 
#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_handle_debug_clients()"
static int cl_commlib_handle_debug_clients(cl_com_handle_t* handle, cl_bool_t lock_list) {

   cl_connection_list_elem_t* elem = NULL;
   cl_string_list_elem_t* string_elem = NULL;
   char* log_string = NULL;
   cl_bool_t list_empty = CL_FALSE;
   cl_bool_t flushed_client = CL_FALSE;
   cl_bool_t had_data_to_flush = CL_FALSE;
   

   if (handle == NULL) {
      CL_LOG(CL_LOG_ERROR,"no handle specified");
      return CL_RETVAL_PARAMS;
   }

   if (handle->debug_client_setup->dc_mode == CL_DEBUG_CLIENT_OFF) {
      CL_LOG(CL_LOG_INFO,"debug clients not enabled");
      return CL_RETVAL_DEBUG_CLIENTS_NOT_ENABLED;
   }

   if (handle->debug_client_setup->dc_debug_list == NULL) {
      CL_LOG(CL_LOG_INFO,"debug clients not supported");
      return CL_RETVAL_UNKNOWN;
   }

   if (lock_list == CL_TRUE) {
      cl_raw_list_lock(handle->connection_list);
   }

   cl_raw_list_lock(handle->debug_client_setup->dc_debug_list);
   CL_LOG_INT(CL_LOG_INFO, "elements to flush:", (int)cl_raw_list_get_elem_count(handle->debug_client_setup->dc_debug_list));
   cl_raw_list_unlock(handle->debug_client_setup->dc_debug_list);

   while(list_empty == CL_FALSE) {
      log_string = NULL;
      cl_raw_list_lock(handle->debug_client_setup->dc_debug_list);
      string_elem = cl_string_list_get_first_elem(handle->debug_client_setup->dc_debug_list);
      if (string_elem != NULL) {
         cl_raw_list_remove_elem(handle->debug_client_setup->dc_debug_list, string_elem->raw_elem);
         log_string = string_elem->string;
         had_data_to_flush = CL_TRUE;
         free(string_elem);
      } else {
         list_empty = CL_TRUE;
      }
      cl_raw_list_unlock(handle->debug_client_setup->dc_debug_list);
      
      if (log_string != NULL) {
         elem = cl_connection_list_get_first_elem(handle->connection_list);
         while(elem) {
            cl_com_connection_t* connection = elem->connection;
            if (connection->data_flow_type == CL_CM_CT_STREAM && connection->connection_state == CL_CONNECTED) {
               if (strcmp(connection->remote->comp_name, CL_COM_DEBUG_CLIENT_NAME) == 0) {
                  cl_com_message_t* message = NULL;
                  char* message_text = strdup(log_string);

                  /* flush debug client */
                  if (message_text != NULL) {
                     CL_LOG_STR_STR_INT(CL_LOG_INFO, "flushing debug client:",
                                        connection->remote->comp_host,
                                        connection->remote->comp_name,
                                        (int)connection->remote->comp_id);
                  
                     cl_raw_list_lock(connection->send_message_list);
                     
                     cl_com_setup_message(&message,
                                          connection,
                                          (cl_byte_t*) message_text,
                                          strlen(message_text),
                                          CL_MIH_MAT_NAK,
                                          0,
                                          0);
                     cl_message_list_append_send(connection,message,0);
                     cl_raw_list_unlock(connection->send_message_list);
                     flushed_client = CL_TRUE;
                  }
               }
            }
            elem = cl_connection_list_get_next_elem(elem);
         }
         free(log_string);
         log_string = NULL;
      }
   }
   
   if ( had_data_to_flush == CL_TRUE && flushed_client == CL_FALSE ) {
      /* no connected debug clients, turn off debug message flushing */
      CL_LOG(CL_LOG_ERROR,"disable debug client message creation");
      handle->debug_client_setup->dc_mode = CL_DEBUG_CLIENT_OFF;
      pthread_mutex_lock(&cl_com_debug_client_callback_func_mutex);
      if (cl_com_debug_client_callback_func != NULL) {
         cl_com_debug_client_callback_func(0, handle->debug_client_setup->dc_app_log_level);
      }
      pthread_mutex_unlock(&cl_com_debug_client_callback_func_mutex);
   }

   if (lock_list == CL_TRUE) {
      cl_raw_list_unlock(handle->connection_list);
   }

   if ( flushed_client == CL_TRUE ) {
      switch(cl_com_create_threads) {
         case CL_NO_THREAD:
            CL_LOG(CL_LOG_INFO,"no threads enabled");
            /* we just want to trigger write , no wait for read*/
            cl_commlib_trigger(handle, 1);
            break;
         case CL_RW_THREAD:
            /* we just want to trigger write , no wait for read*/
            CL_LOG(CL_LOG_INFO,"trigger write thread");
            cl_thread_trigger_event(handle->write_thread);
            break;
      }
   }
   return CL_RETVAL_OK;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_get_actual_statistic_data()"
int cl_com_get_actual_statistic_data(cl_com_handle_t* handle, cl_com_handle_statistic_t** statistics) {
   int ret_val = CL_RETVAL_OK;
   if (handle == NULL || statistics == NULL) {
      return CL_RETVAL_PARAMS;
   }
   if (*statistics != NULL) {
      return CL_RETVAL_PARAMS;
   }

   *statistics = (cl_com_handle_statistic_t*)malloc(sizeof(cl_com_handle_statistic_t));
   if (*statistics == NULL) {
      return CL_RETVAL_MALLOC;
   }
   

   cl_raw_list_lock(handle->connection_list);
   if ( (ret_val=cl_commlib_calculate_statistic(handle, CL_TRUE, 0)) == CL_RETVAL_OK) {
      memcpy(*statistics, handle->statistic, sizeof(cl_com_handle_statistic_t));
      (*statistics)->application_info = NULL;
      if (handle->statistic->application_info != NULL) {
         (*statistics)->application_info = strdup(handle->statistic->application_info);
      } else {
         (*statistics)->application_info = strdup("not available");
      }
   }
   cl_raw_list_unlock(handle->connection_list);
   
   if ((*statistics)->application_info == NULL) {
      cl_com_free_handle_statistic(statistics);
      return CL_RETVAL_MALLOC;
   }

   return ret_val;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_set_application_debug_client_callback_func()"
int cl_com_set_application_debug_client_callback_func(cl_app_debug_client_func_t debug_client_callback_func ) {
   pthread_mutex_lock(&cl_com_debug_client_callback_func_mutex);
   cl_com_debug_client_callback_func = *debug_client_callback_func;
   pthread_mutex_unlock(&cl_com_debug_client_callback_func_mutex);
   return CL_RETVAL_OK;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_default_application_debug_client_callback()"
static void cl_com_default_application_debug_client_callback(int dc_connected, int debug_level) {
   if (dc_connected == 1) {
      CL_LOG(CL_LOG_INFO,"a application debug client is connected");
   } else {
      CL_LOG(CL_LOG_INFO,"no application debug client connected");
   }

   CL_LOG_INT(CL_LOG_INFO,"debug level is:", debug_level);
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_application_debug()"
int cl_com_application_debug(cl_com_handle_t* handle, const char* message) {
#define CL_DEBUG_DMT_APP_MESSAGE_FORMAT_STRING "%lu\t%.6f\t%s\n"
   int                          ret_val           = CL_RETVAL_OK;
   double                       time_now          = 0.0;
   char*                        dm_buffer         = NULL;
   unsigned long                dm_buffer_len     = 0;
   cl_com_debug_message_tag_t   debug_message_tag = CL_DMT_APP_MESSAGE;
   struct timeval now;
   unsigned long i;
   int found_last = 0;


   if (handle == NULL || message == NULL) {
      return CL_RETVAL_PARAMS;
   }

   
   /* don't add default case for this switch! */
   switch(handle->debug_client_setup->dc_mode) {
      case CL_DEBUG_CLIENT_OFF:
      case CL_DEBUG_CLIENT_MSG: 
         return CL_RETVAL_DEBUG_CLIENTS_NOT_ENABLED;
      case CL_DEBUG_CLIENT_APP:
      case CL_DEBUG_CLIENT_ALL:
         break;
   }

   gettimeofday(&now,NULL);
   time_now = now.tv_sec + (now.tv_usec / 1000000.0);

   dm_buffer_len += cl_util_get_ulong_number_length((unsigned long)debug_message_tag);
   dm_buffer_len += cl_util_get_double_number_length(time_now);
   dm_buffer_len += strlen(message);
   dm_buffer_len += strlen(CL_DEBUG_DMT_APP_MESSAGE_FORMAT_STRING);
   dm_buffer_len += 1;

   dm_buffer = (char*) malloc(sizeof(char)*dm_buffer_len);
   if (dm_buffer == NULL) {
      return CL_RETVAL_MALLOC;
   } else {
      snprintf(dm_buffer,dm_buffer_len,CL_DEBUG_DMT_APP_MESSAGE_FORMAT_STRING,
                         (unsigned long)debug_message_tag,
                         time_now,
                         message);

      /* 
       * remove all "\n" execpt the last one, because any additional "\n" in application message
       * would break qping message format parsing. 
       */ 
      for(i=dm_buffer_len - 1 ; i > 0 ; i--) {
         if ( dm_buffer[i] == '\n' ) {
            if (found_last == 1) {
               dm_buffer[i] = ' ';
            }
            found_last = 1;
         }
      }
      ret_val = cl_string_list_append_string(handle->debug_client_setup->dc_debug_list, dm_buffer , 1);
      free(dm_buffer);
      dm_buffer = NULL;
   }

   return ret_val;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_calculate_statistic()"
static int cl_commlib_calculate_statistic(cl_com_handle_t* handle, cl_bool_t force_update ,int lock_list) {
   cl_connection_list_elem_t* elem = NULL;
   struct timeval now;
   double handle_time_last = 0.0;
   double handle_time_now = 0.0;
   double handle_time_range = 0.0;
   double con_per_second = 0.0;
   double kbits_sent = 0.0;
   double kbits_received = 0.0;
   double real_kbits_sent = 0.0;
   double real_kbits_received = 0.0;
   double send_pay_load = 0.0;
   double receive_pay_load = 0.0;
   char help[256];

   cl_com_con_statistic_t* con_stat = NULL;
   if (handle == NULL) {
      CL_LOG(CL_LOG_ERROR,"no handle specified");
      return CL_RETVAL_PARAMS;
   }

   if (lock_list != 0) {
      cl_raw_list_lock(handle->connection_list);
   }
   gettimeofday(&now,NULL);

   handle_time_now = now.tv_sec + (now.tv_usec / 1000000.0);
   handle_time_last = handle->statistic->last_update.tv_sec + (handle->statistic->last_update.tv_usec / 1000000.0 );
   handle_time_range = handle_time_now - handle_time_last;

   if ( force_update == CL_FALSE && handle_time_range < 60.0 ) {
      /* only update once per minute */
      CL_LOG_INT(CL_LOG_DEBUG, "skipping statistic update, time till next update:", (int) (60 - (int)handle_time_range));
      if (lock_list != 0) {
         cl_raw_list_unlock(handle->connection_list);
      }
      return CL_RETVAL_OK; 
   }

   CL_LOG(CL_LOG_INFO, "performing statistic update");
   gettimeofday(&(handle->statistic->last_update),NULL);


   /* get application status */
   pthread_mutex_lock(&cl_com_application_mutex);
   handle->statistic->application_status = 99999;
   if (cl_com_application_status_func != NULL) {
      if ( handle->statistic->application_info != NULL ) {
         free(handle->statistic->application_info);
         handle->statistic->application_info = NULL;
      }
      handle->statistic->application_status = cl_com_application_status_func(&(handle->statistic->application_info));
   } 
   pthread_mutex_unlock(&cl_com_application_mutex);

   con_per_second = handle->statistic->new_connections / handle_time_range;
   
   handle->statistic->new_connections = 0;
   handle->statistic->unsend_message_count = 0;
   handle->statistic->unread_message_count = 0;
   handle->statistic->nr_of_connections = cl_raw_list_get_elem_count(handle->connection_list);
   elem = cl_connection_list_get_first_elem(handle->connection_list);
   while(elem) {
      /* elem->connection */
      con_stat = elem->connection->statistic;
      if (con_stat) {
         handle->statistic->bytes_sent = handle->statistic->bytes_sent + con_stat->bytes_sent;
         handle->statistic->real_bytes_sent = handle->statistic->real_bytes_sent + con_stat->real_bytes_sent;
         handle->statistic->bytes_received = handle->statistic->bytes_received + con_stat->bytes_received;
         handle->statistic->real_bytes_received = handle->statistic->real_bytes_received + con_stat->real_bytes_received;
         con_stat->bytes_sent = 0;
         con_stat->bytes_received = 0;
         con_stat->real_bytes_sent = 0;
         con_stat->real_bytes_received = 0;

         handle->statistic->unsend_message_count = handle->statistic->unsend_message_count + cl_raw_list_get_elem_count(elem->connection->send_message_list);
         handle->statistic->unread_message_count = handle->statistic->unread_message_count + cl_raw_list_get_elem_count(elem->connection->received_message_list);
      }
      elem = cl_connection_list_get_next_elem(elem);
   }
   if (handle_time_range > 0.0) {
      kbits_sent          = ( (handle->statistic->bytes_sent          / 1024.0) * 8.0) / handle_time_range;
      kbits_received      = ( (handle->statistic->bytes_received      / 1024.0) * 8.0) / handle_time_range;
      real_kbits_sent     = ( (handle->statistic->real_bytes_sent     / 1024.0) * 8.0) / handle_time_range;
      real_kbits_received = ( (handle->statistic->real_bytes_received / 1024.0) * 8.0) / handle_time_range;
   }

   if (real_kbits_sent > 0.0) {
      send_pay_load = kbits_sent / real_kbits_sent;
   }
   if (real_kbits_received > 0.0) {
      receive_pay_load = kbits_received / real_kbits_received;
   }
   

   snprintf(help,256,"           %.3f", handle_time_range);
   CL_LOG_STR(CL_LOG_INFO,"time_range:",help);

   snprintf(help,256,"  %.3f", con_per_second );
   CL_LOG_STR(CL_LOG_INFO,"new connections/sec:",help);

   snprintf(help,256,"           %.3f", send_pay_load);
   CL_LOG_STR(CL_LOG_INFO,"sent ratio:",help);  
   snprintf(help,256,"          %.3f", kbits_sent);    
   CL_LOG_STR(CL_LOG_INFO,"sent kbit/s:",help);  
   snprintf(help,256,"     %.3f", real_kbits_sent);    
   CL_LOG_STR(CL_LOG_INFO,"real sent kbit/s:",help);


   snprintf(help,256,"        %.3f", receive_pay_load);
   CL_LOG_STR(CL_LOG_INFO,"receive ratio:",help);  
   snprintf(help,256,"      %.3f", kbits_received);   
   CL_LOG_STR(CL_LOG_INFO,"received kbit/s:",help);
   snprintf(help,256," %.3f", real_kbits_received);   
   CL_LOG_STR(CL_LOG_INFO,"real received kbit/s:",help);


   snprintf(help,256,"           %.3f" , (double) handle->statistic->bytes_sent / 1024.0);  
   CL_LOG_STR(CL_LOG_INFO,"sent kbyte:",help);   
   snprintf(help,256,"      %.3f" , (double) handle->statistic->real_bytes_sent / 1024.0);  
   CL_LOG_STR(CL_LOG_INFO,"real sent kbyte:",help);   
 

 
   snprintf(help,256,"       %.3f" , (double)handle->statistic->bytes_received / 1024.0);   
   CL_LOG_STR(CL_LOG_INFO,"received kbyte:",help);
   snprintf(help,256,"  %.3f" , (double)handle->statistic->real_bytes_received / 1024.0);   
   CL_LOG_STR(CL_LOG_INFO,"real received kbyte:",help);



   snprintf(help,256," %ld" , handle->statistic->unsend_message_count);
   CL_LOG_STR(CL_LOG_INFO,"unsend_message_count:",help);

   snprintf(help,256," %ld" , handle->statistic->unread_message_count);
   CL_LOG_STR(CL_LOG_INFO,"unread_message_count:",help);

   snprintf(help,256,"     %ld" , handle->statistic->nr_of_connections);
   CL_LOG_STR(CL_LOG_INFO,"open connections:",help);

   snprintf(help,256,"    %ld" , handle->statistic->application_status);
   CL_LOG_STR(CL_LOG_INFO,"application state:",help);

   if ( handle->statistic->application_info != NULL ) {
      snprintf(help,256,"    %s" , handle->statistic->application_info);
      CL_LOG_STR(CL_LOG_INFO,"application state:",help);
   }

   handle->statistic->bytes_sent = 0;
   handle->statistic->bytes_received = 0;
   handle->statistic->real_bytes_sent = 0;
   handle->statistic->real_bytes_received = 0;
   
   if (lock_list != 0) {
      cl_raw_list_unlock(handle->connection_list);
   }
   return CL_RETVAL_OK;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_finish_request_completeness()"
static int cl_commlib_finish_request_completeness(cl_com_connection_t* connection) {
   if (connection == NULL) {
      return CL_RETVAL_PARAMS;
   }

   /* reset buffer variables (used for STREAM debug_clients) */
   connection->data_write_buffer_pos = 0;
   connection->data_write_buffer_processed = 0;
   connection->data_write_buffer_to_send = 0;

   connection->data_read_buffer_processed = 0;
   connection->data_read_buffer_pos = 0;

   if (connection->was_accepted == CL_TRUE) {
      int connect_port = 0;
      if (cl_com_connection_get_connect_port(connection,&connect_port) == CL_RETVAL_OK) {
         if ( connect_port > 0 ) {
            CL_LOG_STR(CL_LOG_INFO,"comp_host :", connection->remote->comp_host);
            CL_LOG_STR(CL_LOG_INFO,"comp_name :", connection->remote->comp_name);
            CL_LOG_INT(CL_LOG_INFO,"comp_id   :", (int)connection->remote->comp_id);
            CL_LOG_INT(CL_LOG_INFO,"new connected client can be reached at port", (int)connect_port);
            if ( connection->auto_close_type == CL_CM_AC_ENABLED) {
               CL_LOG(CL_LOG_INFO,"new connected client supports auto close");
            }
            cl_com_append_known_endpoint(connection->remote, connect_port, connection->auto_close_type, CL_FALSE);
         } else {
            CL_LOG(CL_LOG_INFO,"client does not provide service port");
         }
      }
   }
   return CL_RETVAL_OK;
}



#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_handle_connection_write()"
static int cl_commlib_handle_connection_write(cl_com_connection_t* connection) {
   cl_com_message_t* message = NULL;
   cl_message_list_elem_t* message_list_elem = NULL;
   cl_message_list_elem_t* next_message_list_elem = NULL;
   int return_value = CL_RETVAL_OK;
   struct timeval now;
   int connect_port = 0;

   if (connection == NULL) {
      return CL_RETVAL_PARAMS;
   }

   if (connection->data_flow_type == CL_CM_CT_STREAM) {

       cl_raw_list_lock(connection->send_message_list);

       message_list_elem = cl_message_list_get_first_elem(connection->send_message_list);
       if (message_list_elem != NULL) {
          message = message_list_elem->message;
       } else {
          connection->data_write_flag = CL_COM_DATA_NOT_READY;
          cl_raw_list_unlock(connection->send_message_list);
          return CL_RETVAL_OK;
       }

       if (message->message_state == CL_MS_INIT_SND) {
          message->message_snd_pointer = 0;    
          message->message_state = CL_MS_SND;
          gettimeofday(&now,NULL);
          connection->write_buffer_timeout_time = now.tv_sec + connection->handler->write_timeout;
       }

       if (message->message_state == CL_MS_SND) {
          unsigned long written = 0;
          return_value = cl_com_write(connection,
                                      &(message->message[message->message_snd_pointer]),
                                      message->message_length - message->message_snd_pointer,
                                      &written);
          message->message_snd_pointer = message->message_snd_pointer + written;
          if (return_value != CL_RETVAL_OK) {
             cl_raw_list_unlock(connection->send_message_list);
             return return_value;
          }
          if (message->message_snd_pointer == message->message_length) {
             message->message_state = CL_MS_READY;
          } else {
             cl_raw_list_unlock(connection->send_message_list);
             return CL_RETVAL_SEND_ERROR;
          }
       }

       if (message->message_state == CL_MS_READY) {
          connection->write_buffer_timeout_time = 0;
          connection->statistic->bytes_sent = connection->statistic->bytes_sent + message->message_length;
          connection->statistic->real_bytes_sent = connection->statistic->real_bytes_sent + message->message_length;
          if ( cl_message_list_remove_send(connection, message, 0) == CL_RETVAL_OK) {
             cl_com_free_message(&message);
          }

          if (cl_message_list_get_first_elem(connection->send_message_list) == NULL) {
             connection->data_write_flag = CL_COM_DATA_NOT_READY;
          } else {
             connection->data_write_flag = CL_COM_DATA_READY;
          }

          cl_raw_list_unlock(connection->send_message_list);

          /* set last transfer time of connection */
          gettimeofday(&connection->last_transfer_time,NULL);

          /* Touch endpoint, he is still active */
          if (cl_com_connection_get_connect_port(connection, &connect_port) == CL_RETVAL_OK) {
             cl_endpoint_list_define_endpoint(cl_com_get_endpoint_list(),
                                              connection->remote,
                                              connect_port,
                                              connection->auto_close_type, CL_FALSE );
          }
       }
    }
  
    if (connection->data_flow_type == CL_CM_CT_MESSAGE) {

       cl_raw_list_lock(connection->send_message_list);
       message = NULL;
#ifdef CL_DO_COMMLIB_DEBUG
       CL_LOG_INT(CL_LOG_INFO,"number of messages in send list:",(int)cl_raw_list_get_elem_count(connection->send_message_list));
#endif

       message = NULL;
       message_list_elem = cl_message_list_get_first_elem(connection->send_message_list);
       while (message_list_elem != NULL) {
          if (message_list_elem->message->message_state == CL_MS_PROTOCOL ||
              message_list_elem->message->message_state == CL_MS_READY) {
             message_list_elem = cl_message_list_get_next_elem(message_list_elem);
             continue;
          } 
          message = message_list_elem->message;
          break;
       } 

       if (message == NULL) {
          connection->data_write_flag = CL_COM_DATA_NOT_READY;
          cl_raw_list_unlock(connection->send_message_list);
          return CL_RETVAL_OK;
       }

       next_message_list_elem = cl_message_list_get_next_elem(message_list_elem);

       if (message->message_state == CL_MS_INIT_SND) {
          unsigned long gmsh_message_size = 0;
          unsigned long mih_message_size = 0;

          mih_message_size = CL_MIH_MESSAGE_SIZE;
          mih_message_size = mih_message_size + strlen(CL_MIH_MESSAGE_VERSION);
          mih_message_size = mih_message_size + cl_util_get_ulong_number_length(message->message_id);
          mih_message_size = mih_message_size + cl_util_get_ulong_number_length(message->message_length);
          mih_message_size = mih_message_size + strlen(cl_com_get_mih_df_string(message->message_df));
          mih_message_size = mih_message_size + strlen(cl_com_get_mih_mat_string(message->message_mat));
          mih_message_size = mih_message_size + cl_util_get_ulong_number_length(message->message_tag);
          mih_message_size = mih_message_size + cl_util_get_ulong_number_length(message->message_response_id);


          if (connection->data_buffer_size < (mih_message_size + 1) ) {
             cl_raw_list_unlock(connection->send_message_list);
             return CL_RETVAL_STREAM_BUFFER_OVERFLOW;
          }

          gmsh_message_size = CL_GMSH_MESSAGE_SIZE + cl_util_get_ulong_number_length(mih_message_size);
          if (connection->data_buffer_size < (gmsh_message_size + 1) ) {
             cl_raw_list_unlock(connection->send_message_list);
             return CL_RETVAL_STREAM_BUFFER_OVERFLOW;
          }
          snprintf((char*)connection->data_write_buffer, connection->data_buffer_size, CL_GMSH_MESSAGE , mih_message_size);


          gettimeofday(&now,NULL);
          connection->write_buffer_timeout_time = now.tv_sec + connection->handler->write_timeout;
          connection->data_write_buffer_pos = 0;
          connection->data_write_buffer_processed = 0;
          connection->data_write_buffer_to_send = gmsh_message_size;
          message->message_snd_pointer = 0;    
          message->message_state = CL_MS_SND_GMSH;
       }

       if (message->message_state == CL_MS_SND_GMSH) {
          unsigned long written = 0;
          return_value = cl_com_write(connection,
                                      &(connection->data_write_buffer[connection->data_write_buffer_pos]),
                                      connection->data_write_buffer_to_send,
                                      &written );
          connection->data_write_buffer_pos = connection->data_write_buffer_pos + written;
          connection->data_write_buffer_to_send = connection->data_write_buffer_to_send - written;
          if (return_value != CL_RETVAL_OK) {
             cl_raw_list_unlock(connection->send_message_list);

             /* recalculate timeout when some data was written */
             if (written > 0) {
                gettimeofday(&now,NULL);
                connection->write_buffer_timeout_time = now.tv_sec + connection->handler->write_timeout;
                CL_LOG(CL_LOG_INFO,"recalculate write buffer timeout time (CL_MS_SND_GMSH)");
             }
             return return_value;
          }
          if (connection->data_write_buffer_to_send > 0) {
             cl_raw_list_unlock(connection->send_message_list);
             return CL_RETVAL_SEND_ERROR;
          }
          connection->statistic->real_bytes_sent = connection->statistic->real_bytes_sent + connection->data_write_buffer_pos ;

          sprintf((char*)connection->data_write_buffer, CL_MIH_MESSAGE ,
                   CL_MIH_MESSAGE_VERSION,
                   message->message_id,
                   message->message_length,
                   cl_com_get_mih_df_string(message->message_df),
                   cl_com_get_mih_mat_string(message->message_mat),
                   message->message_tag ,
                   message->message_response_id);

#ifdef CL_DO_COMMLIB_DEBUG         
          CL_LOG_STR(CL_LOG_DEBUG,"write buffer:",(char*)connection->data_write_buffer);
          CL_LOG_STR(CL_LOG_INFO,"CL_MS_SND_GMSH, MIH: ",(char*)connection->data_write_buffer );
#endif

          connection->data_write_buffer_pos = 0;
          connection->data_write_buffer_processed = 0;
          connection->data_write_buffer_to_send = strlen((char*)connection->data_write_buffer);

          message->message_snd_pointer = 0;    
          message->message_state = CL_MS_SND_MIH;
       }

       if (message->message_state == CL_MS_SND_MIH) {
          unsigned long written = 0;
          return_value = cl_com_write(connection,
                                      &(connection->data_write_buffer[connection->data_write_buffer_pos]),
                                      connection->data_write_buffer_to_send,
                                      &written );
          connection->data_write_buffer_pos = connection->data_write_buffer_pos + written;
          connection->data_write_buffer_to_send = connection->data_write_buffer_to_send - written;
          if (return_value != CL_RETVAL_OK) {
             cl_raw_list_unlock(connection->send_message_list);

             /* recalculate timeout when some data was written */
             if (written > 0) {
                gettimeofday(&now,NULL);
                connection->write_buffer_timeout_time = now.tv_sec + connection->handler->write_timeout;
                CL_LOG(CL_LOG_INFO,"recalculate write buffer timeout time (CL_MS_SND_MIH)");
             }
             return return_value;
          }
          if (connection->data_write_buffer_to_send > 0) {
             cl_raw_list_unlock(connection->send_message_list);
             return CL_RETVAL_SEND_ERROR;
          }
          connection->statistic->real_bytes_sent = connection->statistic->real_bytes_sent + connection->data_write_buffer_pos ;

          message->message_state = CL_MS_SND;
       }
       
       if (message->message_state == CL_MS_SND) {
          unsigned long written = 0;
          return_value = cl_com_write(connection,
                                      &(message->message[message->message_snd_pointer]),
                                      message->message_length - message->message_snd_pointer,
                                      &written );
          message->message_snd_pointer = message->message_snd_pointer + written;
          if (return_value != CL_RETVAL_OK) {
             cl_raw_list_unlock(connection->send_message_list);

             /* recalculate timeout when some data was written */
             if (written > 0) {
                gettimeofday(&now,NULL);
                connection->write_buffer_timeout_time = now.tv_sec + connection->handler->write_timeout;
                CL_LOG(CL_LOG_INFO,"recalculate write buffer timeout time (CL_MS_SND)");
             }
             return return_value;
          }
          if (message->message_snd_pointer == message->message_length) {
             message->message_state = CL_MS_READY;
          } else {
             cl_raw_list_unlock(connection->send_message_list);
             return CL_RETVAL_SEND_ERROR;
          }
       }

       if (message->message_state == CL_MS_READY) {

          CL_LOG_STR_STR_INT(CL_LOG_INFO, "sent message to:      ", connection->receiver->comp_host,
                                                                    connection->receiver->comp_name,
                                                                    (int)connection->receiver->comp_id);

          connection->statistic->bytes_sent = connection->statistic->bytes_sent + message->message_length;
          connection->statistic->real_bytes_sent = connection->statistic->real_bytes_sent + message->message_length  ;

          if (connection->connection_state == CL_CONNECTED) {
             int connect_port = 0;
             /* Touch endpoint, he is still active */
             if (cl_com_connection_get_connect_port(connection, &connect_port) == CL_RETVAL_OK) {
                cl_endpoint_list_define_endpoint(cl_com_get_endpoint_list(),
                                                 connection->remote,
                                                 connect_port,
                                                 connection->auto_close_type, CL_FALSE );
             }

             switch (message->message_df) {
                case CL_MIH_DF_CCM:
                   CL_LOG(CL_LOG_INFO,"sent connection close message");
                   connection->connection_sub_state = CL_COM_WAIT_FOR_CCRM;
                   connection->ccm_sent = 1;
                   break;

                case CL_MIH_DF_CCRM:
                   CL_LOG(CL_LOG_INFO,"sent connection close response message");
                   connection->connection_sub_state = CL_COM_DONE;   /* CLOSE message */
                   connection->data_write_flag = CL_COM_DATA_NOT_READY;
                   connection->ccrm_sent = 1;
 
                   /* This client has been gone forever, remove his port from endpoint list */
                   cl_endpoint_list_undefine_endpoint(cl_com_get_endpoint_list(), connection->remote);
                   break;
           
                case CL_MIH_DF_BIN:
                   CL_LOG(CL_LOG_INFO,"sent binary message");
                   break;
                case CL_MIH_DF_XML:
                   CL_LOG(CL_LOG_INFO,"sent XML message");
                   break;
                case CL_MIH_DF_AM:
                   CL_LOG(CL_LOG_INFO,"sent acknowledge message");
                   break;
                case CL_MIH_DF_SIM:
                   CL_LOG(CL_LOG_INFO,"sent status information message");
                   break;
                case CL_MIH_DF_SIRM:
                   CL_LOG(CL_LOG_INFO,"sent status information response message");
                   break;
     
                default:
                   CL_LOG(CL_LOG_ERROR,"unexpected data format");
             }
          }
          connection->write_buffer_timeout_time = 0;
          gettimeofday(&message->message_send_time,NULL);
          cl_com_add_debug_message(connection, NULL, message);

          /* set last transfer time of connection */
          memcpy(&connection->last_transfer_time,&message->message_send_time,sizeof(struct timeval) );
          switch(message->message_df) {
             case CL_MIH_DF_SIM:
                /* don't remove message */
                message->message_state = CL_MS_PROTOCOL; /* wait for SIRM, do not delete message */
                break;

             /* remove messages which don't want an acknoledge */
             default:
             if (message->message_mat == CL_MIH_MAT_NAK) {
                if (cl_message_list_remove_send(connection, message, 0) == CL_RETVAL_OK) {
                   cl_com_free_message(&message);
#ifdef CL_DO_COMMLIB_DEBUG
                   CL_LOG(CL_LOG_DEBUG,"last sent message removed from send_message_list");
#endif
                }
             } else {
                message->message_state = CL_MS_PROTOCOL;  /* wait for ACK, do not delete message */
             }
          } /* switch */

          /* try to find out if there are more messages to send */
          message = NULL;
          message_list_elem = next_message_list_elem;
          while (message_list_elem != NULL) {
             if (message_list_elem->message->message_state == CL_MS_PROTOCOL ||
                 message_list_elem->message->message_state == CL_MS_READY) {
                message_list_elem = cl_message_list_get_next_elem(message_list_elem);
                continue;
             } 
             message = message_list_elem->message;
             break;
          } 


          if (message == NULL) {
             /* no messages to send, data is not ready */
             connection->data_write_flag = CL_COM_DATA_NOT_READY;
          }
       }
       cl_raw_list_unlock(connection->send_message_list);

       if (connection->ccm_received == 1) {
          CL_LOG(CL_LOG_INFO,"received ccm");
   
          CL_LOG_INT(CL_LOG_WARNING,"receive buffer:",(int)cl_raw_list_get_elem_count(connection->received_message_list) );
          CL_LOG_INT(CL_LOG_WARNING,"send buffer   :",(int)cl_raw_list_get_elem_count(connection->send_message_list) );
          CL_LOG_INT(CL_LOG_WARNING,"ccm_received  :",(int)connection->ccm_received);

          if( cl_raw_list_get_elem_count(connection->send_message_list) == 0 && 
              cl_raw_list_get_elem_count(connection->received_message_list) == 0 ) {
             connection->ccm_received = 2;
             connection->connection_sub_state = CL_COM_SENDING_CCRM;
             /* disable #if 1 if you want to test a client, that is frozen! */
             cl_commlib_send_ccrm_message(connection);
             CL_LOG(CL_LOG_WARNING,"sending ccrm");
          } else {
             CL_LOG(CL_LOG_WARNING,"can't send ccrm, still messages in buffer");
          }
       }
    }
    return return_value;
}


/* receive message from host, component, component id from handle */
#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_receive_message()"
int cl_commlib_receive_message(cl_com_handle_t*      handle,
                               char*                 un_resolved_hostname,
                               char*                 component_name,
                               unsigned long         component_id,
                               cl_bool_t             synchron,
                               unsigned long         response_mid,
                               cl_com_message_t**    message,
                               cl_com_endpoint_t**   sender ) {
   cl_com_connection_t* connection = NULL;
   cl_message_list_elem_t* message_elem = NULL;


   long my_timeout = 0;
   int message_sent = 0;
   struct timeval now;
   int leave_reason = CL_RETVAL_OK;
   int message_match = 1;
   int return_value;

   cl_commlib_check_callback_functions();


   if (message == NULL) {
      return CL_RETVAL_PARAMS;
   }
   if (*message != NULL) {
      return CL_RETVAL_PARAMS;
   }

   if (handle == NULL) {
      return CL_RETVAL_PARAMS;
   }

   if (synchron == CL_TRUE) {
      gettimeofday(&now,NULL);
      my_timeout = now.tv_sec + handle->synchron_receive_timeout;
   }

   if ( un_resolved_hostname != NULL || component_name != NULL || component_id != 0) {
      CL_LOG(CL_LOG_DEBUG,"message filtering not supported");
   }
   do {
      leave_reason = CL_RETVAL_OK;
      /* return if there are no connections in list */

      /* If messages_ready_for_read is > 0 there are messages with state CL_MS_READY in receive
       * message lists for application 
       */
      pthread_mutex_lock(handle->messages_ready_mutex);
      if (handle->messages_ready_for_read != 0) {
         cl_app_message_queue_elem_t* app_mq_elem = NULL;

         /* when accessing a connection from the received_message_queue it is 
          * important to hold the messages_ready_mutex lock, because 
          * cl_connection_list_destroy_connections_to_close() will also lock
          * the mutex to remove all queue entries referencing to a connection 
          * which will be removed. So as long the messages_ready_mutex lock
          * is owned the connection is guilty!
          */


         cl_raw_list_lock(handle->received_message_queue);
         app_mq_elem = cl_app_message_queue_get_first_elem(handle->received_message_queue);
         while (app_mq_elem != NULL) {
            connection = app_mq_elem->rcv_connection;

            /* TODO: filter messages for endpoint, if specified and open connection if necessary  (use cl_commlib_open_connection()) */

            /* try to find complete received message in received message list of this connection */
            cl_raw_list_lock(connection->received_message_list);
            message_elem = cl_message_list_get_first_elem(connection->received_message_list);
            while(message_elem) {
               if (message_elem->message->message_state == CL_MS_READY) {
                  message_match = 1;  /* always match the message */
                 
                  /* try to find response for mid */
                  /* TODO: Just return a matchin response_mid !!!  0 = match all else match response_id */
                  if (response_mid != 0) {
                     if(message_elem->message->message_response_id != response_mid) {
                        /* if the response_mid parameter is set we can't return this message because
                           the response id is not the same. -> We have to wait for the correct message */
                        if ( response_mid > connection->last_send_message_id || connection->last_send_message_id == 0 ) {
                           CL_LOG(CL_LOG_WARNING, "protocol error: can't wait for unsent message!!!");
                           cl_raw_list_unlock(connection->received_message_list);
                           cl_raw_list_unlock(handle->received_message_queue);
                           pthread_mutex_unlock(handle->messages_ready_mutex);
                           return CL_RETVAL_PROTOCOL_ERROR;
                        }

                        if ( response_mid > message_elem->message->message_response_id ) {
                           CL_LOG(CL_LOG_INFO, "protocol error: There is still a lower message id than requested");
                        }

                        message_match = 0;  
                     } else {
                        CL_LOG_INT(CL_LOG_INFO,"received response for message id", (int)response_mid);
                     }
                  } else {
                     /* never return a message with response id  when response_mid is 0 */
                     if (message_elem->message->message_response_id != 0) {
                        if (handle->do_shutdown != 2) {
                           CL_LOG_INT(CL_LOG_INFO,"message response id is set for this message:", (int)message_elem->message->message_response_id);
                           message_match = 0;
                        } else {
                           /* this is handle shutdown mode without returning any message to application */
                           /* cl_commlib_shutdown_handle() has to delete this message */
                           CL_LOG_INT(CL_LOG_WARNING,"returning response message without request:", (int)message_elem->message->message_response_id);
                        }
                     }
                  }

                  if (message_match == 1) {
                     /* remove message from received message list*/
                     *message = message_elem->message;
                     cl_message_list_remove_receive(connection, *message, 0);

                     /* release the message list */
                     cl_raw_list_unlock(connection->received_message_list);
    
                     if (sender != NULL) {
                        *sender = cl_com_create_endpoint(connection->receiver->comp_host,
                                                         connection->receiver->comp_name,
                                                         connection->receiver->comp_id );
                     }

                    
#ifndef CL_DO_SEND_ACK_AT_COMMLIB_LAYER 
                     /* send a message acknowledge when application gets the message */
  
                     /* send acknowledge for CL_MIH_MAT_ACK type (application removed message from buffer) */
                     if ( (*message)->message_mat == CL_MIH_MAT_ACK) {
                        cl_commlib_send_ack_message(connection, *message );
                        message_sent = 1; 
                     } 
#endif /* CL_DO_SEND_ACK_AT_COMMLIB_LAYER */

                     if (connection->ccm_received == 1) {
                        CL_LOG(CL_LOG_INFO,"received ccm");

                        CL_LOG_INT(CL_LOG_WARNING,"receive buffer:",(int)cl_raw_list_get_elem_count(connection->received_message_list) );
                        CL_LOG_INT(CL_LOG_WARNING,"send buffer   :",(int)cl_raw_list_get_elem_count(connection->send_message_list) );
                        CL_LOG_INT(CL_LOG_WARNING,"ccm_received  :",(int)connection->ccm_received);

                        if( cl_raw_list_get_elem_count(connection->send_message_list) == 0 && 
                            cl_raw_list_get_elem_count(connection->received_message_list) == 0 ) {
                           connection->ccm_received = 2;
                           connection->connection_sub_state = CL_COM_SENDING_CCRM;
                           /* disable #if 1 if you want to test a client, that is frozen! */
                           cl_commlib_send_ccrm_message(connection);
                           message_sent = 1;
                           CL_LOG(CL_LOG_WARNING,"sending ccrm");
                        } else {
                           CL_LOG(CL_LOG_WARNING,"can't send ccrm, still messages in buffer");
                        }
                     }
  
                     handle->last_receive_message_connection = connection;

                     /* decrease counter for ready messages */
                     handle->messages_ready_for_read = handle->messages_ready_for_read - 1;
                     cl_app_message_queue_remove(handle->received_message_queue, connection, 0, CL_FALSE);
                     cl_raw_list_unlock(handle->received_message_queue);
                     pthread_mutex_unlock(handle->messages_ready_mutex);

                     if ( message_sent ) {
                        switch(cl_com_create_threads) {
                        case CL_NO_THREAD:
                           CL_LOG(CL_LOG_INFO,"no threads enabled");
                           /* we just want to trigger write , no wait for read*/
                           cl_commlib_trigger(handle, 1);
                           break;
                        case CL_RW_THREAD:
                           /* we just want to trigger write , no wait for read*/
                           CL_LOG(CL_LOG_INFO,"trigger write thread");
                           cl_thread_trigger_event(handle->write_thread);
                           break;
                        }
                     }
#if 0
                     CL_LOG_STR(CL_LOG_ERROR,"commlib returns message: ", connection->tag_name_func((*message)->message_tag));
#endif
                     return CL_RETVAL_OK;
                  }
               }
               /* get next message */
               message_elem = cl_message_list_get_next_elem(message_elem);
            }
            /* unlock message list */
            cl_raw_list_unlock(connection->received_message_list);

            app_mq_elem = cl_app_message_queue_get_next_elem(app_mq_elem);
         }
         cl_raw_list_unlock(handle->received_message_queue); 
         pthread_mutex_unlock(handle->messages_ready_mutex);
         CL_LOG(CL_LOG_INFO,"got no message, but thought there should be one");
      } else {
         pthread_mutex_unlock(handle->messages_ready_mutex);

         /* return if there are no connections in list */
         /* check if it is possible to receive messages */
         if (handle->service_provider == CL_FALSE) {
            cl_raw_list_lock(handle->send_message_queue);
            if (cl_connection_list_get_first_elem(handle->send_message_queue) == NULL) {
               cl_raw_list_lock(handle->connection_list);
               if (cl_connection_list_get_first_elem(handle->connection_list) == NULL) {     
                  leave_reason = CL_RETVAL_CONNECTION_NOT_FOUND;
               }
               cl_raw_list_unlock(handle->connection_list);
            }
            cl_raw_list_unlock(handle->send_message_queue);
         }
      }
       
      if (synchron == CL_TRUE) {
         switch(cl_com_create_threads) {
            case CL_NO_THREAD:
               cl_commlib_trigger(handle, 1);
               break;
            case CL_RW_THREAD:
               cl_thread_trigger_event(handle->read_thread);
               return_value = cl_thread_wait_for_thread_condition(handle->app_condition,
                                                   handle->select_sec_timeout,
                                                   handle->select_usec_timeout);
               if (return_value == CL_RETVAL_CONDITION_WAIT_TIMEOUT) {
                  CL_LOG(CL_LOG_INFO,"APPLICATION GOT CONDITION WAIT TIMEOUT");
               }
               break;
         }
         /* at this point the handle->connection_list must be unlocked */
         if ( leave_reason == CL_RETVAL_CONNECTION_NOT_FOUND) {
            /* 
             *  we are no service provider AND we have no connection !
             *  we can't wait for a (possible) new connection, so we return immediately 
             */
            return leave_reason;
         }
         gettimeofday(&now,NULL);
         if (now.tv_sec > my_timeout) {
            return CL_RETVAL_SYNC_RECEIVE_TIMEOUT;
         }
      } 
   } while (synchron == CL_TRUE && cl_com_get_ignore_timeouts_flag() == CL_FALSE);

   /* 
    * when leave_reason is CL_RETVAL_CONNECTION_NOT_FOUND the connection list
    * is empty return this as indication for an error otherwise return
    * CL_RETVAL_NO_MESSAGE to indicate that there is no message available
    */  

   if (leave_reason == CL_RETVAL_OK) {
      return CL_RETVAL_NO_MESSAGE;
   }
   return leave_reason;
}



/*
 *   o search for endpoint with matching hostname, component name or component id
 *   o un_resolved_hostname, component_name and component_id can be NULL or 0 
 *   o caller must free returned endpoint list with cl_endpoint_list_cleanup()
 *
 */

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_search_endpoint()"
int cl_commlib_search_endpoint(cl_com_handle_t* handle,
                               char* un_resolved_hostname, char* component_name, unsigned long component_id, 
                               cl_bool_t only_connected,
                               cl_raw_list_t** endpoint_list ) {

   cl_com_connection_t* connection = NULL;
   cl_connection_list_elem_t* elem = NULL;
   char* resolved_hostname = NULL;
   int retval = CL_RETVAL_OK;


   if ( handle == NULL || endpoint_list == NULL ) {
      return CL_RETVAL_PARAMS;
   }

   if (*endpoint_list != NULL) {
      return CL_RETVAL_PARAMS;
   }

   if (un_resolved_hostname != NULL) {
      retval = cl_com_cached_gethostbyname(un_resolved_hostname, &resolved_hostname, NULL, NULL, NULL );
      if (retval != CL_RETVAL_OK) {
         CL_LOG_STR(CL_LOG_ERROR,"could not resolve host",un_resolved_hostname);
         return retval;
      }
   } 

   retval = cl_endpoint_list_setup(endpoint_list, "matching endpoints", 0, 0); 
   if (retval != CL_RETVAL_OK) {
      free(resolved_hostname);
      resolved_hostname = NULL;
      cl_endpoint_list_cleanup(endpoint_list);
      return retval;
   }

   cl_raw_list_lock(handle->connection_list);
   elem = cl_connection_list_get_first_elem(handle->connection_list);     
   while(elem) {
      connection = elem->connection;
      elem = cl_connection_list_get_next_elem(elem);

      if ( connection->remote != NULL ) {
         if ( component_id > 0 ) {
            if ( connection->remote->comp_id == component_id ) {
               cl_endpoint_list_define_endpoint(*endpoint_list, connection->remote, 0, connection->auto_close_type, CL_FALSE );
               continue;
            } 
         }
         if ( component_name != NULL && connection->remote->comp_name != NULL  ) {
            if ( strcmp(connection->remote->comp_name, component_name) == 0 ) {
               cl_endpoint_list_define_endpoint(*endpoint_list, connection->remote, 0, connection->auto_close_type, CL_FALSE );
               continue;
            }
         }
         if ( resolved_hostname != NULL) {
            if ( cl_com_compare_hosts( resolved_hostname, connection->remote->comp_host ) == CL_RETVAL_OK ) {
               cl_endpoint_list_define_endpoint(*endpoint_list, connection->remote, 0, connection->auto_close_type, CL_FALSE );
               continue;
            }
         }     
      }
   }   
   cl_raw_list_unlock(handle->connection_list);

   if ( only_connected == CL_FALSE ) {
      /* also search in known endpoint list for matching connections */
      cl_raw_list_t* global_endpoint_list = cl_com_get_endpoint_list();
      
      if ( global_endpoint_list != NULL ) {
         cl_endpoint_list_elem_t* endpoint_elem = NULL;
         cl_endpoint_list_elem_t* act_endpoint_elem = NULL;
         
         cl_raw_list_lock(global_endpoint_list);
         endpoint_elem = cl_endpoint_list_get_first_elem(global_endpoint_list);
         while(endpoint_elem) {
            act_endpoint_elem = endpoint_elem;
            endpoint_elem = cl_endpoint_list_get_next_elem(endpoint_elem);

            if (act_endpoint_elem->endpoint) {
               if ( component_id > 0 ) {
                  if ( act_endpoint_elem->endpoint->comp_id == component_id ) {
                     cl_endpoint_list_define_endpoint(*endpoint_list, 
                                                      act_endpoint_elem->endpoint, 
                                                      act_endpoint_elem->service_port, 
                                                      act_endpoint_elem->autoclose, 
                                                      act_endpoint_elem->is_static );
                     continue;
                  } 
               }
               if ( component_name != NULL && act_endpoint_elem->endpoint->comp_name != NULL  ) {
                  if ( strcmp(act_endpoint_elem->endpoint->comp_name, component_name) == 0 ) {
                     cl_endpoint_list_define_endpoint(*endpoint_list, 
                                                      act_endpoint_elem->endpoint, 
                                                      act_endpoint_elem->service_port, 
                                                      act_endpoint_elem->autoclose, 
                                                      act_endpoint_elem->is_static );
                     continue;
                  }
               }
               if ( resolved_hostname != NULL) {
                  if ( cl_com_compare_hosts( resolved_hostname, act_endpoint_elem->endpoint->comp_host ) == CL_RETVAL_OK ) {
                     cl_endpoint_list_define_endpoint(*endpoint_list, 
                                                      act_endpoint_elem->endpoint, 
                                                      act_endpoint_elem->service_port, 
                                                      act_endpoint_elem->autoclose, 
                                                      act_endpoint_elem->is_static );
                     continue;
                  }
               }
    
            }
         }
         cl_raw_list_unlock(global_endpoint_list);
      }
   }
   free(resolved_hostname); 
   resolved_hostname = NULL;
   return CL_RETVAL_OK;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_send_ack_message()"
/* connection_list is locked inside of this call */
static int cl_commlib_send_ack_message(cl_com_connection_t* connection, cl_com_message_t* message) {
   cl_byte_t* ack_message_data = NULL;
   unsigned long ack_message_size = 0;
   int ret_val = CL_RETVAL_OK;
   cl_com_message_t* ack_message = NULL;

   if (connection == NULL || message == NULL) {
      return CL_RETVAL_PARAMS;
   }
   

   ack_message_size = CL_AM_MESSAGE_SIZE;
   ack_message_size = ack_message_size + strlen(CL_AM_MESSAGE_VERSION);
   ack_message_size = ack_message_size + cl_util_get_ulong_number_length(message->message_id);
   
   ack_message_data = (cl_byte_t*)malloc(sizeof(cl_byte_t)* ( ack_message_size + 1) ) ;
   if (ack_message_data == NULL) {
      return CL_RETVAL_MALLOC;
   }
   sprintf((char*)ack_message_data,CL_AM_MESSAGE  ,
                  CL_AM_MESSAGE_VERSION,
                  message->message_id );

 
   ret_val = cl_com_setup_message(&ack_message, connection, ack_message_data , ack_message_size ,CL_MIH_MAT_NAK , 0 ,0);
   if (ret_val != CL_RETVAL_OK) {
      return ret_val;
   }
   ack_message->message_df = CL_MIH_DF_AM;

   CL_LOG_INT(CL_LOG_INFO,"sending ack for message=", (int)message->message_id);
   
   ret_val = cl_message_list_append_send(connection, ack_message,1);

   return ret_val;
}

/* connection_list is locked inside of this call */
#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_send_ccm_message()"
static int cl_commlib_send_ccm_message(cl_com_connection_t* connection) {
   cl_byte_t* ccm_message_data = NULL;
   unsigned long ccm_message_size = 0;
   int ret_val = CL_RETVAL_OK;
   cl_com_message_t* ccm_message = NULL;

   if (connection == NULL ) {
      return CL_RETVAL_PARAMS;
   }

   ccm_message_size = CL_CCM_MESSAGE_SIZE;
   ccm_message_size = ccm_message_size + strlen(CL_CCM_MESSAGE_VERSION);
   
   ccm_message_data = (cl_byte_t*)malloc(sizeof(cl_byte_t)* ( ccm_message_size + 1) ) ;
   if (ccm_message_data == NULL) {
      return CL_RETVAL_MALLOC;
   }
   sprintf((char*)ccm_message_data,CL_CCM_MESSAGE , CL_CCM_MESSAGE_VERSION );

   ret_val = cl_com_setup_message(&ccm_message, connection, ccm_message_data , ccm_message_size , CL_MIH_MAT_NAK , 0 ,0);
   if (ret_val != CL_RETVAL_OK) {
      return ret_val;
   }
   ccm_message->message_df = CL_MIH_DF_CCM;
   CL_LOG(CL_LOG_INFO,"sending connection close message");
   ret_val = cl_message_list_append_send(connection, ccm_message, 1);
   return ret_val;
}

/* connection_list is locked inside of this call */
static int cl_commlib_send_sim_message(cl_com_connection_t* connection, unsigned long* mid) {
   cl_byte_t* sim_message_data = NULL;
   unsigned long sim_message_size = 0;
   int ret_val = CL_RETVAL_OK;
   cl_com_message_t* sim_message = NULL;

   if (connection == NULL ) {
      return CL_RETVAL_PARAMS;
   }

   sim_message_size = CL_SIM_MESSAGE_SIZE;
   sim_message_size = sim_message_size + strlen(CL_SIM_MESSAGE_VERSION);
   
   sim_message_data = (cl_byte_t*)malloc(sizeof(cl_byte_t)* ( sim_message_size + 1) ) ;
   if (sim_message_data == NULL) {
      return CL_RETVAL_MALLOC;
   }
   sprintf((char*)sim_message_data,CL_SIM_MESSAGE , CL_SIM_MESSAGE_VERSION );

   ret_val = cl_com_setup_message(&sim_message, connection, sim_message_data , sim_message_size , CL_MIH_MAT_NAK , 0 ,0);
   if (ret_val != CL_RETVAL_OK) {
      return ret_val;
   }
   sim_message->message_df = CL_MIH_DF_SIM;
   if (mid != NULL) {
      *mid = sim_message->message_id;
   }
   CL_LOG(CL_LOG_INFO,"sending information message (SIM)");
   ret_val = cl_message_list_append_send(connection, sim_message, 1);
   return ret_val;
}

/* connection_list is locked inside of this call */
static int cl_commlib_send_sirm_message(cl_com_connection_t* connection, 
                                        cl_com_message_t* message,
                                        unsigned long starttime,
                                        unsigned long runtime,
                                        unsigned long buffered_read_messages,
                                        unsigned long buffered_write_messages,
                                        unsigned long connection_count,
                                        unsigned long application_status,
                                        char* infotext ) {
   cl_byte_t* sirm_message_data = NULL;
   char* xml_infotext = NULL;
   unsigned long sirm_message_size = 0;
   int ret_val = CL_RETVAL_OK;
   cl_com_message_t* sirm_message = NULL;

   if (connection == NULL|| message == NULL || infotext == NULL ) {
      return CL_RETVAL_PARAMS;
   }

   ret_val = cl_com_transformString2XML(infotext, &xml_infotext);
   if (ret_val != CL_RETVAL_OK) {
      return ret_val;
   }

   sirm_message_size = CL_SIRM_MESSAGE_SIZE;
   sirm_message_size = sirm_message_size + strlen(CL_SIRM_MESSAGE_VERSION);
   sirm_message_size = sirm_message_size + cl_util_get_ulong_number_length(message->message_id);
   sirm_message_size = sirm_message_size + cl_util_get_ulong_number_length(starttime);
   sirm_message_size = sirm_message_size + cl_util_get_ulong_number_length(runtime);
   sirm_message_size = sirm_message_size + cl_util_get_ulong_number_length(buffered_read_messages);
   sirm_message_size = sirm_message_size + cl_util_get_ulong_number_length(buffered_write_messages);
   sirm_message_size = sirm_message_size + cl_util_get_ulong_number_length(connection_count);
   sirm_message_size = sirm_message_size + cl_util_get_ulong_number_length(application_status);
   sirm_message_size = sirm_message_size + strlen(xml_infotext);

   sirm_message_data = (cl_byte_t*)malloc(sizeof(cl_byte_t)* ( sirm_message_size + 1) ) ;
   if (sirm_message_data == NULL) {
      if (xml_infotext != NULL) {
         free(xml_infotext);
         xml_infotext = NULL;
      }
      return CL_RETVAL_MALLOC;
   }
   sprintf((char*)sirm_message_data, CL_SIRM_MESSAGE , 
           CL_SIRM_MESSAGE_VERSION,
           message->message_id,
           starttime,
           runtime,
           buffered_read_messages,
           buffered_write_messages,
           connection_count,
           application_status,
           xml_infotext );

   if (xml_infotext != NULL) {
      free(xml_infotext);
      xml_infotext = NULL;
   }

   ret_val = cl_com_setup_message(&sirm_message, connection, sirm_message_data , sirm_message_size , CL_MIH_MAT_NAK , 0 ,0);
   if (ret_val != CL_RETVAL_OK) {
      return ret_val;
   }
   sirm_message->message_df = CL_MIH_DF_SIRM;
   CL_LOG_INT(CL_LOG_INFO,"sending SIRM for message=", (int)message->message_id);

   ret_val = cl_message_list_append_send(connection, sirm_message, 1);
   return ret_val;
}


/* connection_list is locked inside of this call */
#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_send_ccrm_message()"
static int cl_commlib_send_ccrm_message(cl_com_connection_t* connection) {
   cl_byte_t* ccrm_message_data = NULL;
   unsigned long ccrm_message_size = 0;
   int ret_val = CL_RETVAL_OK;
   cl_com_message_t* ccrm_message = NULL;

   if (connection == NULL ) {
      return CL_RETVAL_PARAMS;
   }

   ccrm_message_size = CL_CCRM_MESSAGE_SIZE;
   ccrm_message_size = ccrm_message_size + strlen(CL_CCRM_MESSAGE_VERSION);
   
   ccrm_message_data = (cl_byte_t*)malloc(sizeof(cl_byte_t)* ( ccrm_message_size + 1) ) ;
   if (ccrm_message_data == NULL) {
      return CL_RETVAL_MALLOC;
   }
   sprintf((char*)ccrm_message_data,CL_CCRM_MESSAGE , CL_CCRM_MESSAGE_VERSION );

   ret_val = cl_com_setup_message(&ccrm_message, connection, ccrm_message_data , ccrm_message_size , CL_MIH_MAT_NAK , 0 ,0);
   if (ret_val != CL_RETVAL_OK) {
      return ret_val;
   }
   ccrm_message->message_df = CL_MIH_DF_CCRM;
   CL_LOG(CL_LOG_INFO,"sending connection close response message");
   ret_val = cl_message_list_append_send(connection, ccrm_message, 1);
   return ret_val;

}



/* ack_type: 
   CL_MIH_MAT_NAK  = send message and don't expect any acknowledge
   CL_MIH_MAT_ACK  = send message and block till communication partner application has read the message
   CL_MIH_MAT_SYNC = send message and block till communication partner application has processed all message actions (TODO)
*/

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_check_for_ack()"
int cl_commlib_check_for_ack(cl_com_handle_t* handle, char* un_resolved_hostname, char* component_name, unsigned long component_id, unsigned long mid , cl_bool_t do_block) {
   int found_message = 0;
   int message_added = 0;
   cl_connection_list_elem_t* elem = NULL;
   cl_com_connection_t* connection;
   cl_com_endpoint_t receiver;
   cl_message_list_elem_t* message_list_elem = NULL;
   cl_message_list_elem_t* next_message_list_elem = NULL;
   cl_com_message_t*     message = NULL;
   int return_value = CL_RETVAL_OK;
   char* unique_hostname = NULL;
   int do_stop = 0;


   cl_commlib_check_callback_functions();

   if ( handle == NULL) {
      return CL_RETVAL_HANDLE_NOT_FOUND;
   }

   /* check endpoint parameters: un_resolved_hostname, componenet_name and componenet_id */
   if ( un_resolved_hostname == NULL || component_name == NULL || component_id == 0 ) {
      return CL_RETVAL_UNKNOWN_ENDPOINT;
   }

   /* resolve hostname */
   return_value = cl_com_cached_gethostbyname(un_resolved_hostname, &unique_hostname, NULL, NULL, NULL);
   if (return_value != CL_RETVAL_OK) {
      return return_value;
   }

   /* setup endpoint */
   receiver.comp_host = unique_hostname;
   receiver.comp_name = component_name;
   receiver.comp_id   = component_id;

   while(do_stop == 0) {
      found_message = 0;
      /* lock handle connection list */
      cl_raw_list_lock(handle->connection_list);
      
      elem = cl_connection_list_get_first_elem(handle->connection_list);     
      connection = NULL;
      while(elem) {
         connection = elem->connection;
         if ( cl_com_compare_endpoints(connection->receiver, &receiver) ) {
            break;
         }
         elem = cl_connection_list_get_next_elem(elem);
         connection = NULL;
      }
    
      if (connection != NULL) {
         /* check for message acknowledge */
         cl_raw_list_lock(connection->send_message_list);
   
         message_list_elem = cl_message_list_get_first_elem(connection->send_message_list);
         while(message_list_elem != NULL && found_message == 0) {
            message = message_list_elem->message;
            next_message_list_elem = cl_message_list_get_next_elem(message_list_elem);
            if (message->message_id == mid) {
               /* found message */
               found_message = 1;
               if (message->message_ack_flag == 1) {
                  cl_message_list_remove_send(connection, message, 0);
                  cl_com_free_message(&message);
                  cl_raw_list_unlock(connection->send_message_list);

                  if (connection->ccm_received == 1) {
                     CL_LOG(CL_LOG_INFO,"received ccm");
              
                     CL_LOG_INT(CL_LOG_WARNING,"receive buffer:",(int)cl_raw_list_get_elem_count(connection->received_message_list) );
                     CL_LOG_INT(CL_LOG_WARNING,"send buffer   :",(int)cl_raw_list_get_elem_count(connection->send_message_list) );
                     CL_LOG_INT(CL_LOG_WARNING,"ccm_received  :",(int)connection->ccm_received);
          
                     if( cl_raw_list_get_elem_count(connection->send_message_list) == 0 && 
                         cl_raw_list_get_elem_count(connection->received_message_list) == 0 ) {
                        connection->ccm_received = 2;
                        connection->connection_sub_state = CL_COM_SENDING_CCRM;
                        /* disable #if 1 if you want to test a client, that is frozen! */
                        cl_commlib_send_ccrm_message(connection);
                        message_added = 1;
                        CL_LOG(CL_LOG_WARNING,"sending ccrm");
                     } else {
                        CL_LOG(CL_LOG_WARNING,"can't send ccrm, still messages in buffer");
                     }
                  }

                  cl_raw_list_unlock(handle->connection_list);
                  free(unique_hostname);
                  CL_LOG_INT(CL_LOG_INFO,"got message acknowledge:",(int)mid);
                  if (message_added) {
                     switch(cl_com_create_threads) {
                        case CL_NO_THREAD:
                           CL_LOG(CL_LOG_INFO,"no threads enabled");
                           /* we just want to trigger write , no wait for read*/
                           cl_commlib_trigger(handle, 1);
                           break;
                        case CL_RW_THREAD:
                           /* we just want to trigger write , no wait for read*/
                           cl_thread_trigger_event(handle->write_thread);
                           break;
                     }
                  }


                  return CL_RETVAL_OK;
               } else {
                  CL_LOG_INT(CL_LOG_INFO,"message is not acknowledged:", (int)mid);
               }
            }
            message_list_elem = next_message_list_elem;
         }
         cl_raw_list_unlock(connection->send_message_list);
      } else {
         CL_LOG_STR(CL_LOG_ERROR,"can't find connection to:", receiver.comp_host);
         cl_raw_list_unlock(handle->connection_list);
         free(unique_hostname);
         return CL_RETVAL_CONNECTION_NOT_FOUND; 
      }
      cl_raw_list_unlock(handle->connection_list);

      if (found_message == 0) {
         CL_LOG_INT(CL_LOG_ERROR,"message not found or removed because of ack timeout", (int)mid);
         free(unique_hostname);
         return CL_RETVAL_MESSAGE_ACK_ERROR; /* message not found or removed because of ack timeout */
      }

      if (do_block == CL_TRUE) {
         switch(cl_com_create_threads) {
            case CL_NO_THREAD:
               CL_LOG(CL_LOG_INFO,"no threads enabled");
               cl_commlib_trigger(handle, 1);
               break;
            case CL_RW_THREAD:
               cl_thread_wait_for_thread_condition(handle->read_condition,
                                                   handle->select_sec_timeout,
                                                   handle->select_usec_timeout);
               break;
         } /* switch */
      } else  {
         free(unique_hostname);
         return CL_RETVAL_MESSAGE_WAIT_FOR_ACK;
      }
   } /* while (1) */
   return CL_RETVAL_UNKNOWN;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_open_connection()"
int cl_commlib_open_connection(cl_com_handle_t* handle, char* un_resolved_hostname, char* component_name, unsigned long component_id) {

   int ret_val;
   char* unique_hostname = NULL;
   cl_bool_t messages_for_app = CL_FALSE;
   cl_com_endpoint_t receiver;
   cl_connection_list_elem_t* elem = NULL;
   cl_com_connection_t* connection = NULL;
   cl_com_connection_t* new_con = NULL;
   cl_com_endpoint_t* remote_endpoint = NULL;
   cl_com_endpoint_t* local_endpoint  = NULL;
   cl_com_endpoint_t* receiver_endpoint  = NULL;
   cl_com_endpoint_t* sender_endpoint  = NULL;

   int shutdown_received = 0;

   cl_commlib_check_callback_functions();

   /* check endpoint parameters: un_resolved_hostname , componenet_name and componenet_id */
   if ( un_resolved_hostname == NULL || component_name == NULL || component_id == 0 ) {
      CL_LOG(CL_LOG_ERROR,cl_get_error_text(CL_RETVAL_UNKNOWN_ENDPOINT));
      return CL_RETVAL_UNKNOWN_ENDPOINT;
   }


   CL_LOG_STR(CL_LOG_INFO,"open host           :",un_resolved_hostname );
   CL_LOG_STR(CL_LOG_INFO,"open component_name :",component_name );
   CL_LOG_INT(CL_LOG_INFO,"open component_id   :",(int)component_id );


   if ( handle == NULL) {
      CL_LOG(CL_LOG_ERROR,cl_get_error_text(CL_RETVAL_HANDLE_NOT_FOUND));
      return CL_RETVAL_HANDLE_NOT_FOUND;
   }
 
   /* resolve hostname */
   ret_val = cl_com_cached_gethostbyname(un_resolved_hostname, &unique_hostname,
                                         NULL, NULL, NULL);
   if (ret_val != CL_RETVAL_OK) {
      CL_LOG(CL_LOG_ERROR,cl_get_error_text(ret_val));
      return ret_val;
   }


   /* setup endpoint */
   receiver.comp_host = unique_hostname;
   receiver.comp_name = component_name;
   receiver.comp_id   = component_id;

   /* lock handle mutex, because we don't want to allow more threads to create a new 
      connection for the same receiver endpoint */
   pthread_mutex_lock(handle->connection_list_mutex);


   /* lock handle connection list */
   cl_raw_list_lock(handle->connection_list);

   elem = cl_connection_list_get_first_elem(handle->connection_list);     
   connection = NULL;
   while(elem) {

      connection = elem->connection;
      if ( cl_com_compare_endpoints(connection->receiver, &receiver) ) {
         struct timeval now;

         if (connection->ccm_received != 0) {

            /* we have sent connection close response, do not accept any new message */
            /* we wait till connection is down and try to reconnect  */
            CL_LOG(CL_LOG_ERROR,"connection is open, but going down now");
            gettimeofday(&now,NULL);
            connection->shutdown_timeout = now.tv_sec + handle->acknowledge_timeout + handle->close_connection_timeout;
            shutdown_received = 1;
            break;
         }

         if (connection->connection_state == CL_CONNECTED &&
             connection->connection_sub_state != CL_COM_WORK) {
            CL_LOG(CL_LOG_ERROR,"connection is open, but going down now");
            gettimeofday(&now,NULL);
            connection->shutdown_timeout = now.tv_sec + handle->acknowledge_timeout + handle->close_connection_timeout;
            shutdown_received = 2;
            break;
         }
         /* connection is open, just return */
         cl_raw_list_unlock(handle->connection_list);
         CL_LOG(CL_LOG_WARNING,"connection is already open");
         free(unique_hostname);
         unique_hostname = NULL;
         receiver.comp_host = NULL;

         /* unlock connection list */
         pthread_mutex_unlock(handle->connection_list_mutex);
         return CL_RETVAL_OK;
      }
      elem = cl_connection_list_get_next_elem(elem);
      connection = NULL;
   }
   cl_raw_list_unlock(handle->connection_list);


   if (shutdown_received != 0) {
      /* the connection is going down -> wait for removal of connection*/  
      int still_in_list = 1;
      /* TODO: save unread messages with state READY in new connection object? If so,
         the application don't have to take care about any error messages. But for now
         it should be ok to let the application hande those kind of problems. (gdi) */

      /* we don't have a port to connect to, re open would fail, return now */
      if (handle->connect_port <= 0) {
         int tcp_port = 0;
         if ( cl_com_get_known_endpoint_port(&receiver, &tcp_port) != CL_RETVAL_OK) {
            CL_LOG(CL_LOG_ERROR,"no port to connect");
            free(unique_hostname); /* don't access receiver after this */
            unique_hostname = NULL;
            receiver.comp_host = NULL;
            /* unlock connection list */
            pthread_mutex_unlock(handle->connection_list_mutex);
            return CL_RETVAL_NO_PORT_ERROR;
         }
      }

      while (still_in_list == 1) {
         int message_sent = 0;

         still_in_list = 0;
         /* lock handle connection list */
         cl_raw_list_lock(handle->connection_list);
         elem = cl_connection_list_get_first_elem(handle->connection_list);
         while(elem) {
            message_sent = 0;
            connection = elem->connection;
            if ( cl_com_compare_endpoints(connection->receiver, &receiver)) {
               struct timeval now;
               still_in_list = 1;
/**/
               if ( shutdown_received == 1 ) {
                  if (connection->ccm_received == 0) {  
                     /* This must be a new connection ( initiated from other endpoint ), so 
                        we return that connection is already open */
                     cl_raw_list_unlock(handle->connection_list);
                     free(unique_hostname);
                     unique_hostname = NULL;
                     receiver.comp_host = NULL;

                     pthread_mutex_unlock(handle->connection_list_mutex);
                     CL_LOG(CL_LOG_INFO,"This is a new connected client, don't reopen, use new connection");
                     return CL_RETVAL_OK;
                  }
               } else {
                  if ( (connection->connection_state == CL_CONNECTED && connection->connection_sub_state == CL_COM_WORK) || 
                        connection->connection_state == CL_OPENING ||
                        connection->connection_state == CL_ACCEPTING ||
                        connection->connection_state == CL_CONNECTING ||
                        connection->connection_state == CL_DISCONNECTED) {
                     /* This must be a new connection ( initiated from other endpoint ), so 
                        we return that connection is already open */
                     cl_raw_list_unlock(handle->connection_list);
                     free(unique_hostname);
                     unique_hostname = NULL;
                     receiver.comp_host = NULL;

                     pthread_mutex_unlock(handle->connection_list_mutex);
                     CL_LOG(CL_LOG_INFO,"This is a new connected client, don't reopen, use new connection");
                     return CL_RETVAL_OK;
                  }
               }
/**/
               CL_LOG(CL_LOG_WARNING,"connection still alive ...");
               CL_LOG_INT(CL_LOG_WARNING,"receive buffer:",(int)cl_raw_list_get_elem_count(connection->received_message_list) );
               CL_LOG_INT(CL_LOG_WARNING,"send buffer   :",(int)cl_raw_list_get_elem_count(connection->send_message_list) );
              
               if ( connection->ccm_received == 1 && 
                    cl_raw_list_get_elem_count(connection->received_message_list) == 0 &&  
                    cl_raw_list_get_elem_count(connection->send_message_list) == 0 ) {
                  CL_LOG(CL_LOG_WARNING,"received ccm");
                  connection->ccm_received = 2;
                  connection->connection_sub_state = CL_COM_SENDING_CCRM;
                  cl_commlib_send_ccrm_message(connection);
                  message_sent = 1;
                  CL_LOG(CL_LOG_WARNING,"sending ccrm");
               }

               /* There are messages to read for application, return */
               if ( cl_raw_list_get_elem_count(connection->received_message_list) != 0 ) {
                  messages_for_app = CL_TRUE;
               }

               gettimeofday(&now,NULL);
               if (connection->shutdown_timeout <= now.tv_sec || cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
                  CL_LOG(CL_LOG_WARNING,"got timeout while waiting for connection close");
                  connection->connection_state = CL_CLOSING;
                  connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                  cl_raw_list_unlock(handle->connection_list);
                  free(unique_hostname);
                  unique_hostname = NULL;
                  receiver.comp_host = NULL;

                  /* unlock connection list */
                  pthread_mutex_unlock(handle->connection_list_mutex);
                  return CL_RETVAL_CONNECTION_GOING_DOWN;
               }
            }
            elem = cl_connection_list_get_next_elem(elem);
         }
         cl_raw_list_unlock(handle->connection_list);
         

         if (still_in_list == 1) {
            switch(cl_com_create_threads) {
               case CL_NO_THREAD:
                  CL_LOG(CL_LOG_INFO,"no threads enabled");
                  cl_commlib_trigger(handle, 1);
                  break;
               case CL_RW_THREAD:
                  /* unlock connection list */
                  if (message_sent != 0) {
                     cl_thread_trigger_event(handle->write_thread);
                  }
                  cl_thread_wait_for_thread_condition(handle->read_condition,
                                                   handle->select_sec_timeout,
                                                   handle->select_usec_timeout);
                  break;
            }
            /* at this point the handle->connection_list must be unlocked */
         }

         if ( messages_for_app == CL_TRUE ) {
            free(unique_hostname);
            unique_hostname = NULL;
            receiver.comp_host = NULL;

            pthread_mutex_unlock(handle->connection_list_mutex);
            return CL_RETVAL_MESSAGE_IN_BUFFER;
         }
      }
      CL_LOG(CL_LOG_WARNING,"connection is down! Try to reopen ...");
   }
   

   /* here a new connection setup is done */
   CL_LOG(CL_LOG_INFO,"open new connection"); 
     
   ret_val = cl_com_setup_connection(handle, &new_con);
   if (ret_val != CL_RETVAL_OK) {
      CL_LOG(CL_LOG_ERROR,"could not setup connection");
      cl_com_close_connection(&new_con);
      free(unique_hostname);
      unique_hostname = NULL;
      receiver.comp_host = NULL;

      /* unlock connection list */
      pthread_mutex_unlock(handle->connection_list_mutex);
      return ret_val;
   }
  
   /* now open connection to the endpoint */
   local_endpoint    = cl_com_create_endpoint(handle->local->comp_host, handle->local->comp_name, handle->local->comp_id);
   sender_endpoint   = cl_com_create_endpoint(handle->local->comp_host, handle->local->comp_name, handle->local->comp_id);
   remote_endpoint   = cl_com_create_endpoint(receiver.comp_host, component_name , component_id);
   receiver_endpoint = cl_com_create_endpoint(receiver.comp_host, component_name , component_id);
      
   ret_val = cl_com_open_connection(new_con, 
                                    handle->open_connection_timeout,
                                    remote_endpoint,
                                    local_endpoint,
                                    receiver_endpoint,
                                    sender_endpoint);

   cl_com_free_endpoint(&remote_endpoint);
   cl_com_free_endpoint(&local_endpoint);
   cl_com_free_endpoint(&receiver_endpoint);
   cl_com_free_endpoint(&sender_endpoint);

   if (ret_val != CL_RETVAL_OK && ret_val != CL_RETVAL_UNCOMPLETE_WRITE ) {
      CL_LOG(CL_LOG_ERROR,"could not open connection");
      cl_com_close_connection(&new_con);
      free(unique_hostname);
      unique_hostname = NULL;
      receiver.comp_host = NULL;

      /* unlock connection list */
      pthread_mutex_unlock(handle->connection_list_mutex);

      /* do this for compatibility to remote clients, which
         will never immediately will return a connect error */
      switch(cl_com_create_threads) {
         case CL_NO_THREAD:
            CL_LOG(CL_LOG_INFO,"no threads enabled");
            cl_commlib_trigger(handle, 1);
            break;
         case CL_RW_THREAD:
            cl_thread_trigger_event(handle->read_thread);
            cl_commlib_trigger(handle, 1); /* TODO: check if this is ok ???  */
            break;
      }

      return ret_val;
   }
   new_con->handler = handle;


   /* lock connection list */
   cl_raw_list_lock(handle->connection_list);
   
   /* Check if this connection is unique */
   elem = cl_connection_list_get_first_elem(handle->connection_list);
   while(elem != NULL) {
      if ( cl_com_compare_endpoints(elem->connection->receiver, &receiver)) {
         break;
      }
      elem = cl_connection_list_get_next_elem(elem);
   }
   if (elem == NULL) {
      /* endpoint is unique, add it to connection list */
      ret_val = cl_connection_list_append_connection(handle->connection_list, new_con,0);
      cl_raw_list_unlock(handle->connection_list);
   } else {
      if ( elem->connection->connection_state != CL_CLOSING ) {
         CL_LOG(CL_LOG_INFO,"try to open connection to already connected endpoint");

         cl_raw_list_unlock(handle->connection_list);
         cl_com_close_connection(&new_con);
         free(unique_hostname); /* don't access receiver after this */
         unique_hostname = NULL;
         receiver.comp_host = NULL;

         /* unlock connection list */
         pthread_mutex_unlock(handle->connection_list_mutex);
         return CL_RETVAL_OK;
      } else {
         CL_LOG(CL_LOG_ERROR,"client not unique error, can't add opened connection into connection list");
         cl_raw_list_unlock(handle->connection_list);
         cl_com_close_connection(&new_con);
         free(unique_hostname); /* don't access receiver after this */
         unique_hostname = NULL;
         receiver.comp_host = NULL;

         /* unlock connection list */
         pthread_mutex_unlock(handle->connection_list_mutex);
         return CL_RETVAL_ENDPOINT_NOT_UNIQUE;
      }
   }

   free(unique_hostname); /* don't access receiver after this */
   unique_hostname = NULL;
   receiver.comp_host = NULL;

   CL_LOG(CL_LOG_INFO,"new connection created");
   handle->statistic->new_connections =  handle->statistic->new_connections + 1;
   /* unlock connection list */
   pthread_mutex_unlock(handle->connection_list_mutex);

   switch(cl_com_create_threads) {
      case CL_NO_THREAD:
         CL_LOG(CL_LOG_INFO,"no threads enabled");
         cl_commlib_trigger(handle, 1);
         break;
      case CL_RW_THREAD:
         /* new connection, trigger read_thread */
         cl_thread_trigger_event(handle->write_thread);
         cl_thread_trigger_event(handle->read_thread);
         break;
   }
   
   return ret_val;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_close_connection()"
int cl_commlib_close_connection(cl_com_handle_t* handle,char* un_resolved_hostname, char* component_name, unsigned long component_id,  cl_bool_t return_for_messages) {
   int closed = 0;
   int return_value = CL_RETVAL_OK;
   cl_bool_t trigger_write = CL_FALSE;
   char* unique_hostname = NULL;
   cl_com_endpoint_t receiver;
   cl_connection_list_elem_t* elem = NULL;
   cl_com_connection_t* connection = NULL;
   cl_app_message_queue_elem_t* mq_elem = NULL;
   int mq_return_value = CL_RETVAL_OK;

   cl_commlib_check_callback_functions();

   if ( handle == NULL) {
      CL_LOG(CL_LOG_ERROR,cl_get_error_text(CL_RETVAL_HANDLE_NOT_FOUND));
      return CL_RETVAL_HANDLE_NOT_FOUND;
   }
   /* check endpoint parameters: un_resolved_hostname , componenet_name and componenet_id */
   if ( un_resolved_hostname == NULL || component_name == NULL || component_id == 0 ) {
      CL_LOG(CL_LOG_ERROR,cl_get_error_text(CL_RETVAL_UNKNOWN_ENDPOINT));
      return CL_RETVAL_UNKNOWN_ENDPOINT;
   }
   /* resolve hostname */
   return_value = cl_com_cached_gethostbyname(un_resolved_hostname, &unique_hostname,NULL, NULL, NULL);
   if (return_value != CL_RETVAL_OK) {
      CL_LOG(CL_LOG_ERROR,cl_get_error_text(return_value));
      return return_value;
   }
   /* setup endpoint */
   receiver.comp_host = unique_hostname;
   receiver.comp_name = component_name;
   receiver.comp_id   = component_id;

   /* flush send message queue */
   cl_raw_list_lock(handle->send_message_queue);
   while((mq_elem = cl_app_message_queue_get_first_elem(handle->send_message_queue)) != NULL) {
      CL_LOG(CL_LOG_INFO,"flushing send message queue ...");

      mq_return_value = cl_commlib_send_message_to_endpoint(handle, mq_elem->snd_destination,
                                                            mq_elem->snd_ack_type, mq_elem->snd_data,
                                                            mq_elem->snd_size, mq_elem->snd_response_mid,
                                                            mq_elem->snd_tag); 
      /* remove queue entries */
      cl_raw_list_remove_elem(handle->send_message_queue, mq_elem->raw_elem);
      if (mq_return_value != CL_RETVAL_OK) {
         CL_LOG_STR(CL_LOG_ERROR,"can't send message:", cl_get_error_text(mq_return_value));
         free(mq_elem->snd_data);
      }
      cl_com_free_endpoint(&(mq_elem->snd_destination));
      free(mq_elem);
   }
   cl_raw_list_unlock(handle->send_message_queue);


   /* lock handle connection list */
   cl_raw_list_lock(handle->connection_list);

   elem = cl_connection_list_get_first_elem(handle->connection_list);     
   connection = NULL;
   while(elem) {
      connection = elem->connection;
      if ( cl_com_compare_endpoints(connection->receiver, &receiver) ) {
         if (connection->data_flow_type == CL_CM_CT_MESSAGE) {
            if (connection->connection_state == CL_CONNECTED &&
                connection->connection_sub_state == CL_COM_WORK  &&
                connection->ccm_received         == 0) {
               cl_commlib_send_ccm_message(connection);
               trigger_write = CL_TRUE;
               connection->connection_sub_state = CL_COM_SENDING_CCM;
               CL_LOG_STR(CL_LOG_WARNING,"closing connection to host:", connection->remote->comp_host );
               CL_LOG_STR(CL_LOG_WARNING,"component name:            ", connection->remote->comp_name );
               CL_LOG_INT(CL_LOG_WARNING,"component id:              ", (int)connection->remote->comp_id );
               closed = 1;
            }
         } 

         if (connection->data_flow_type == CL_CM_CT_STREAM) {
            CL_LOG(CL_LOG_WARNING,"closing stream connection");
            CL_LOG_STR(CL_LOG_WARNING,"closing connection to host:", connection->remote->comp_host );
            CL_LOG_STR(CL_LOG_WARNING,"component name:            ", connection->remote->comp_name );
            CL_LOG_INT(CL_LOG_WARNING,"component id:              ", (int)connection->remote->comp_id );
            connection->connection_state = CL_CLOSING;
            connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
            closed = 1;
         }
      }
      elem = cl_connection_list_get_next_elem(elem);
      connection = NULL;
   }
   cl_raw_list_unlock(handle->connection_list);

   if ( trigger_write == CL_TRUE ) {
      switch(cl_com_create_threads) {
         case CL_NO_THREAD: {
            CL_LOG(CL_LOG_INFO,"no threads enabled");
            /* we just want to trigger write , no wait for read*/
            cl_commlib_trigger(handle, 1);
            break;
         }
         case CL_RW_THREAD: {
            /* we just want to trigger write , no wait for read*/
            cl_thread_trigger_event(handle->write_thread);
            break;
         }
      }
   }
   if (closed == 1) {
      /* Wait for removal of connection */
      cl_bool_t connection_removed = CL_FALSE;
      cl_bool_t do_return_after_trigger = CL_FALSE;

      while ( connection_removed == CL_FALSE ) {
         connection_removed = CL_TRUE;

         /* is connection still in list ??? */
         cl_raw_list_lock(handle->connection_list);
         elem = cl_connection_list_get_first_elem(handle->connection_list);     
         connection = NULL;
         while(elem) {
            connection = elem->connection;
            if ( cl_com_compare_endpoints(connection->receiver, &receiver) ) {
               cl_message_list_elem_t* message_elem = NULL;
               cl_message_list_elem_t* current_message_elem = NULL;

               connection_removed = CL_FALSE;

               cl_raw_list_lock(connection->received_message_list);
               if (cl_raw_list_get_elem_count(connection->received_message_list) > 0) {
                  message_elem = cl_message_list_get_first_elem(connection->received_message_list);
                  while(message_elem) {
                     /* set current message element */
                     current_message_elem = message_elem;

                     /* get next message */
                     message_elem = cl_message_list_get_next_elem(message_elem);
                     
                     if (current_message_elem->message->message_state == CL_MS_READY) {
                        /* there are messages in ready to deliver state for this connection */
                        if (return_for_messages == CL_TRUE) {
                           do_return_after_trigger = CL_TRUE;
                        } else {
                           cl_com_message_t* message = NULL;

                           /* remove message from received message list*/
                           message = current_message_elem->message;
                           cl_message_list_remove_receive(connection, message, 0);

                           /* decrease counter for ready messages */
                           pthread_mutex_lock(handle->messages_ready_mutex);
                           handle->messages_ready_for_read = handle->messages_ready_for_read - 1;
                           cl_app_message_queue_remove(handle->received_message_queue, connection, 1, CL_FALSE);
                           pthread_mutex_unlock(handle->messages_ready_mutex);

                           /* delete message */
                           cl_com_free_message(&message);
                        }
                     }
                  }
                  
               }
               cl_raw_list_unlock(connection->received_message_list);
               break;
            }
            elem = cl_connection_list_get_next_elem(elem);
            connection = NULL;
         }
         cl_raw_list_unlock(handle->connection_list);

         if ( connection_removed == CL_FALSE ) {
            switch(cl_com_create_threads) {
               case CL_NO_THREAD:
                  CL_LOG(CL_LOG_INFO,"no threads enabled");
                  cl_commlib_trigger(handle, 1);
                  break;
               case CL_RW_THREAD:
                  /* we just want to trigger write , no wait for read*/
                  cl_thread_trigger_event(handle->write_thread);
                  break;
            }
         }
         if (do_return_after_trigger == CL_TRUE) {
            free(unique_hostname);
            unique_hostname = NULL;
            return CL_RETVAL_MESSAGE_IN_BUFFER;
         }
      }
      free(unique_hostname);
      unique_hostname = NULL;
      return CL_RETVAL_OK;
   }
   free(unique_hostname);
   unique_hostname = NULL;
   return CL_RETVAL_CONNECTION_NOT_FOUND;
}

#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_get_endpoint_status()"
int cl_commlib_get_endpoint_status(cl_com_handle_t* handle,
                                   char* un_resolved_hostname, char* component_name, unsigned long component_id,
                                   cl_com_SIRM_t** status) {


   cl_com_connection_t* connection = NULL;
   cl_connection_list_elem_t* elem = NULL;
   int message_added = 0;
   int new_message_added = 0;
   unsigned long my_mid = 0;
   int return_value = CL_RETVAL_OK;
   cl_com_endpoint_t receiver;
   char* unique_hostname = NULL;
   int retry_send = 1;
   int found_message = 0;
   int do_stop = 0;
   cl_message_list_elem_t* message_list_elem = NULL;
   cl_message_list_elem_t* next_message_list_elem = NULL;

   cl_com_message_t*     message = NULL;

   cl_commlib_check_callback_functions();

   if ( handle == NULL || status == NULL) {
      CL_LOG(CL_LOG_ERROR,cl_get_error_text(CL_RETVAL_HANDLE_NOT_FOUND));
      return CL_RETVAL_PARAMS;
   }

   /* check endpoint parameters: un_resolved_hostname , componenet_name and componenet_id */
   if ( un_resolved_hostname == NULL || component_name == NULL || component_id == 0 ) {
      CL_LOG(CL_LOG_ERROR,cl_get_error_text(CL_RETVAL_UNKNOWN_ENDPOINT));
      return CL_RETVAL_UNKNOWN_ENDPOINT;
   }

   

   if (*status != NULL) {
      CL_LOG(CL_LOG_ERROR,"expected empty status pointer address");
      return CL_RETVAL_PARAMS;
   }
   
   CL_LOG_STR_STR_INT(CL_LOG_INFO, "ping",un_resolved_hostname, component_name, (int)component_id );

   /* resolve hostname */
   return_value = cl_com_cached_gethostbyname(un_resolved_hostname, &unique_hostname,NULL, NULL, NULL);
   if (return_value != CL_RETVAL_OK) {
      CL_LOG(CL_LOG_ERROR,cl_get_error_text(return_value));
      return return_value;
   }

   /* setup endpoint */
   receiver.comp_host = unique_hostname;
   receiver.comp_name = component_name;
   receiver.comp_id   = component_id;


   while(retry_send != 0) {
   
      /* lock handle connection list */
      cl_raw_list_lock(handle->connection_list);
      elem = cl_connection_list_get_first_elem(handle->connection_list);     
      connection = NULL;
      while(elem) {
   
         connection = elem->connection;
   
         /* send message to client (no broadcast) */
         if ( cl_com_compare_endpoints(connection->receiver, &receiver) ) {
   
            if (connection->ccm_received != 0) {  
               /* we have sent connection close response, do not accept any new message */
               /* we wait till connection is down and try to reconnect  */
               CL_LOG(CL_LOG_ERROR,"connection is going down now, can't send message (ccrm sent)");
               break;
            }
   
            if (connection->connection_state == CL_CONNECTED && connection->connection_sub_state != CL_COM_WORK) {
               CL_LOG(CL_LOG_WARNING,"connection is going down now, can't send message");
               break;
            }
   
            return_value = cl_commlib_send_sim_message(connection, &my_mid); 
   
            if (return_value != CL_RETVAL_OK) {
               cl_raw_list_unlock(handle->connection_list);
               free(unique_hostname);
               return return_value;
            }
            message_added = 1;
            break;
         }
         elem = cl_connection_list_get_next_elem(elem);
         connection = NULL;
      }
      cl_raw_list_unlock(handle->connection_list);

      /* if message is not added, the connection was not found -> try to open it */
      if (message_added != 1) {
         retry_send++;  
         return_value = cl_commlib_open_connection(handle, un_resolved_hostname, component_name, component_id);
         if (return_value != CL_RETVAL_OK) {
            free(unique_hostname);
            CL_LOG_STR(CL_LOG_ERROR,"cl_commlib_open_connection() returned: ",cl_get_error_text(return_value));
            return return_value;
         }
         if (retry_send >= 3) {
            CL_LOG(CL_LOG_ERROR,"can't open connection, don't retry to send this message");
            retry_send = 0;
         }
      } else {
         retry_send = 0; /* break */
      }
   }


   if (message_added == 1) {
      
      switch(cl_com_create_threads) {
         case CL_NO_THREAD: {
            CL_LOG(CL_LOG_INFO,"no threads enabled");
            /* we just want to trigger write , no wait for read*/
            cl_commlib_trigger(handle, 1);
            break;
         }
         case CL_RW_THREAD: {
            /* we just want to trigger write , no wait for read*/
            cl_thread_trigger_event(handle->write_thread);
            break;
         }
      }
   } else {
      free(unique_hostname);
      return CL_RETVAL_SEND_ERROR;
   } 
 
   CL_LOG_INT(CL_LOG_WARNING,"waiting for SIRM with id",(int)my_mid);
   while(do_stop == 0) {
      found_message = 0;
      /* lock handle connection list */
      cl_raw_list_lock(handle->connection_list);
      
      elem = cl_connection_list_get_first_elem(handle->connection_list);     
      connection = NULL;
      while(elem) {
         connection = elem->connection;
         if ( cl_com_compare_endpoints(connection->receiver, &receiver) ) {
            break;
         }
         elem = cl_connection_list_get_next_elem(elem);
         connection = NULL;
      }
      if (connection != NULL) {
         /* check for message acknowledge */
         cl_raw_list_lock(connection->send_message_list);
   
         message_list_elem = cl_message_list_get_first_elem(connection->send_message_list);
         while(message_list_elem != NULL && found_message == 0) {
            message = message_list_elem->message;
            next_message_list_elem = cl_message_list_get_next_elem(message_list_elem );
            if (message->message_id == my_mid) {
               /* found message */
               found_message = 1;
               if (message->message_sirm != NULL) {
                  cl_message_list_remove_send(connection, message, 0);
                  *status = message->message_sirm;
                  message->message_sirm = NULL;
                  cl_com_free_message(&message);
                  cl_raw_list_unlock(connection->send_message_list);

                  if (connection->ccm_received == 1) {
                     CL_LOG(CL_LOG_INFO,"received ccm");
              
                     CL_LOG_INT(CL_LOG_WARNING,"receive buffer:",(int)cl_raw_list_get_elem_count(connection->received_message_list) );
                     CL_LOG_INT(CL_LOG_WARNING,"send buffer   :",(int)cl_raw_list_get_elem_count(connection->send_message_list) );
                     CL_LOG_INT(CL_LOG_WARNING,"ccm_received  :",(int)connection->ccm_received);
         
                     if( cl_raw_list_get_elem_count(connection->send_message_list) == 0 && 
                         cl_raw_list_get_elem_count(connection->received_message_list) == 0 ) {
                        connection->ccm_received = 2;
                        connection->connection_sub_state = CL_COM_SENDING_CCRM;
                        /* disable #if 1 if you want to test a client, that is frozen! */
                        cl_commlib_send_ccrm_message(connection);
                        new_message_added = 1;
                        CL_LOG(CL_LOG_WARNING,"sending ccrm");
                     } else {
                        CL_LOG(CL_LOG_WARNING,"can't send ccrm, still messages in buffer");
                     }
                  }


                  cl_raw_list_unlock(handle->connection_list);
                  free(unique_hostname);
                  CL_LOG_INT(CL_LOG_WARNING,"got SIRM for SIM with id:",(int)my_mid);

                  if (new_message_added) {
                     switch(cl_com_create_threads) {
                        case CL_NO_THREAD:
                           CL_LOG(CL_LOG_INFO,"no threads enabled");
                           /* we just want to trigger write , no wait for read*/
                           cl_commlib_trigger(handle, 1);
                           break;
                        case CL_RW_THREAD:
                           /* we just want to trigger write , no wait for read*/
                           cl_thread_trigger_event(handle->write_thread);
                           break;
                     }
                  }

                  return CL_RETVAL_OK;
               } else {
                  CL_LOG_INT(CL_LOG_WARNING,"no SRIM for SIM with id", (int)my_mid);
               }
            }
            message_list_elem = next_message_list_elem;
         }
         cl_raw_list_unlock(connection->send_message_list);
      } else {
         CL_LOG(CL_LOG_ERROR,"no connection FOUND");
         cl_raw_list_unlock(handle->connection_list);
         free(unique_hostname);
         return CL_RETVAL_CONNECTION_NOT_FOUND; 
      }
      cl_raw_list_unlock(handle->connection_list);

      if (found_message == 0) {
         CL_LOG_INT(CL_LOG_ERROR,"SIM not found or removed because of SIRM ack timeout", (int)my_mid);
         free(unique_hostname);
         return CL_RETVAL_MESSAGE_ACK_ERROR; /* message not found or removed because of ack timeout */
      }

      switch(cl_com_create_threads) {
         case CL_NO_THREAD:
            CL_LOG(CL_LOG_INFO,"no threads enabled");
            cl_commlib_trigger(handle, 1);
            break;
         case CL_RW_THREAD:
            cl_thread_wait_for_thread_condition(handle->read_condition,
                                                handle->select_sec_timeout,
                                                handle->select_usec_timeout);
            break;
      } 
   } /* while(1) */
   return CL_RETVAL_UNKNOWN;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_send_message_to_endpoint()"
static int cl_commlib_send_message_to_endpoint(cl_com_handle_t*   handle,
                                               cl_com_endpoint_t* endpoint, 
                                               cl_xml_ack_type_t  ack_type, 
                                               cl_byte_t*         data, 
                                               unsigned long      size , 
                                               unsigned long      response_mid,
                                               unsigned long      tag) {

   cl_com_connection_t* connection = NULL;
   cl_connection_list_elem_t* elem = NULL;
   cl_com_message_t* message = NULL;
   int message_added = 0;
   int return_value = CL_RETVAL_OK;
   int retry_send = 1;
   cl_bool_t ignore_connection = CL_FALSE;

   /* check acknowledge method */
   if (ack_type == CL_MIH_MAT_UNDEFINED || data == NULL || size == 0 ) {
      CL_LOG(CL_LOG_ERROR,cl_get_error_text(CL_RETVAL_PARAMS));
      return CL_RETVAL_PARAMS;
   }

   if ( handle == NULL) {
      CL_LOG(CL_LOG_ERROR,cl_get_error_text(CL_RETVAL_HANDLE_NOT_FOUND));
      return CL_RETVAL_HANDLE_NOT_FOUND;
   }

   if ( endpoint == NULL ) {
      CL_LOG(CL_LOG_ERROR,cl_get_error_text(CL_RETVAL_UNKNOWN_ENDPOINT));
      return CL_RETVAL_UNKNOWN_ENDPOINT;
   }

   if ( endpoint->comp_id == 0) {
      CL_LOG(CL_LOG_ERROR,cl_get_error_text(CL_RETVAL_UNKNOWN_ENDPOINT));
      return CL_RETVAL_UNKNOWN_ENDPOINT;
   }

   if ( handle->do_shutdown != 0) {
      CL_LOG(CL_LOG_WARNING,"handle is going down, don't send message");
      return CL_RETVAL_HANDLE_SHUTDOWN_IN_PROGRESS;
   }

   while(retry_send != 0) {
   
      /* lock handle connection list */
      cl_raw_list_lock(handle->connection_list);
      elem = cl_connection_list_get_first_elem(handle->connection_list);     
      while(elem) {
         ignore_connection = CL_FALSE;
         connection = elem->connection;
   
         if (connection->was_accepted == CL_TRUE) {
            /* ignore duplicate endpoints */
            switch (connection->crm_state) {
               case CL_CRM_CS_UNDEFINED:
               case CL_CRM_CS_CONNECTED: {
                  ignore_connection = CL_FALSE;
                  break;
               }
               default: {
                  ignore_connection = CL_TRUE;
                  CL_LOG(CL_LOG_WARNING,"ignore connection with unexpected connection state");
                  break;
               }
            }
         }
         
         if (ignore_connection == CL_FALSE && 
             cl_com_compare_endpoints(connection->receiver, endpoint)) {
   
            /* send message to client (no broadcast) */

            if (connection->ccm_received != 0) {  
               /* we have sent connection close response, do not accept any new message */
               /* we wait till connection is down and try to reconnect  */
               CL_LOG(CL_LOG_ERROR,"connection is going down now, can't send message (ccrm sent)");
               break;
            }
   
            if (connection->connection_state == CL_CONNECTED && connection->connection_sub_state != CL_COM_WORK) {
               CL_LOG(CL_LOG_WARNING,"connection is going down now, can't send message");
               break;
            }
   

            if (  response_mid > 0 && response_mid > connection->last_received_message_id ) {
               CL_LOG_INT(CL_LOG_DEBUG,"last_received_message_id:", (int)connection->last_received_message_id );
               CL_LOG_INT(CL_LOG_DEBUG,"last_send_message_id    :", (int)connection->last_send_message_id);
               CL_LOG_INT(CL_LOG_DEBUG,"response_mid to send    :", (int)response_mid);

               CL_LOG(CL_LOG_ERROR,"Protocol error: haven't received such a high message id till now");
               cl_raw_list_unlock(handle->connection_list);
               return CL_RETVAL_PROTOCOL_ERROR;
            }    

            CL_LOG_STR_STR_INT(CL_LOG_INFO, "sending it to:        ", connection->receiver->comp_host,
                                                                      connection->receiver->comp_name,
                                                                      (int)connection->receiver->comp_id);

            return_value = cl_com_setup_message(&message, connection, data, size, ack_type,response_mid,tag);


   
            if (return_value != CL_RETVAL_OK) {
               cl_raw_list_unlock(handle->connection_list);
               return return_value;
            }
   
            return_value = cl_message_list_append_send(connection, message, 1);
            if (return_value != CL_RETVAL_OK) {
               cl_com_free_message(&message);
               cl_raw_list_unlock(handle->connection_list);
               return return_value;
            }
            message_added = 1;
            
            break;
         }
         elem = cl_connection_list_get_next_elem(elem);
      }
      cl_raw_list_unlock(handle->connection_list);

      /* if message is not added, the connection was not found -> try to open it */
      if (message_added != 1) {
         retry_send++;  
         return_value = cl_commlib_open_connection(handle, endpoint->comp_host, endpoint->comp_name, endpoint->comp_id );
         if (return_value != CL_RETVAL_OK) {
            CL_LOG_STR(CL_LOG_ERROR,"cl_commlib_open_connection() returned: ",cl_get_error_text(return_value));
            return return_value;
         }
         if (retry_send >= 3) {
            CL_LOG(CL_LOG_ERROR,"can't open connection, don't retry to send this message");
            retry_send = 0;
         }
      } else {
         retry_send = 0; /* break */
      }
   }

   if (message_added != 1) {
      return CL_RETVAL_SEND_ERROR;
   }

   return return_value;
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_send_message()"
int 
cl_commlib_send_message(cl_com_handle_t* handle, char *un_resolved_hostname, 
                        char *component_name, unsigned long component_id, 
                        cl_xml_ack_type_t ack_type, cl_byte_t *data, 
                        unsigned long size, unsigned long *mid, 
                        unsigned long response_mid, unsigned long tag ,
                        cl_bool_t copy_data, cl_bool_t wait_for_ack) 
{
   cl_com_connection_t* connection = NULL;
   cl_connection_list_elem_t* elem = NULL;
   cl_com_message_t* message = NULL;
   int message_added = 0;
   unsigned long my_mid = 0;
   int return_value = CL_RETVAL_OK;
   cl_com_endpoint_t receiver;
   char* unique_hostname = NULL;
   cl_byte_t* help_data = NULL;
   int retry_send = 1;
   cl_bool_t ignore_connection = CL_FALSE;

   cl_commlib_check_callback_functions();

   /* check acknowledge method */
   if (ack_type == CL_MIH_MAT_UNDEFINED || data == NULL || size == 0 ) {
      CL_LOG(CL_LOG_ERROR,cl_get_error_text(CL_RETVAL_PARAMS));
      return CL_RETVAL_PARAMS;
   }

   if ( handle == NULL) {
      CL_LOG(CL_LOG_ERROR,cl_get_error_text(CL_RETVAL_HANDLE_NOT_FOUND));
      return CL_RETVAL_HANDLE_NOT_FOUND;
   }

   /* check endpoint parameters: un_resolved_hostname , componenet_name and componenet_id */
   if ( un_resolved_hostname == NULL || component_name == NULL || component_id == 0 ) {
      CL_LOG(CL_LOG_ERROR,cl_get_error_text(CL_RETVAL_UNKNOWN_ENDPOINT));
      return CL_RETVAL_UNKNOWN_ENDPOINT;
   }

   /* make a copy of the message date (if wished) */
   if (copy_data == CL_TRUE) {
      help_data = (cl_byte_t*) malloc((sizeof(cl_byte_t)*size));
      if (help_data == NULL) {
         return CL_RETVAL_MALLOC;
      }
      memcpy(help_data, data, (sizeof(cl_byte_t)*size) );
   } else {
      help_data = data;
   }


   /*
    *  The send_message_queue can only be used if the following parameters are not requested
    *  by the user:
    *
    *  - mid != NULL               : The caller wants the message id which can't be set before
    *                                the message is added to the connection's message list
    *
    *  - wait_for_ack == CL_TRUE   : The caller wants to wait for the response, we have to do
    *                                the transaction at once
    *
    */


   if (mid == NULL && wait_for_ack == CL_FALSE && cl_com_create_threads != CL_NO_THREAD ) {
      cl_com_endpoint_t* destination_endpoint = NULL;

      /* using send_message_queue for this message */
      CL_LOG_STR_STR_INT(CL_LOG_INFO, "add message into send queue for:      ", un_resolved_hostname, component_name, (int)component_id);
    
      /* resolve hostname and create endpoint structure */
      return_value = cl_com_cached_gethostbyname(un_resolved_hostname, &unique_hostname,NULL, NULL, NULL);
      if (return_value != CL_RETVAL_OK) {
         CL_LOG(CL_LOG_ERROR,cl_get_error_text(return_value));
         if (copy_data == CL_TRUE) {
            free(help_data);
         }
         return return_value;
      }

      destination_endpoint = cl_com_create_endpoint(unique_hostname,component_name,component_id);
      free(unique_hostname);
      unique_hostname = NULL;

      if (destination_endpoint == NULL) {
         if (copy_data == CL_TRUE) {
            free(help_data);
         }
         return CL_RETVAL_MALLOC;
      }

      return_value = cl_app_message_queue_append(handle->send_message_queue, NULL,
                                                 destination_endpoint, ack_type,
                                                 help_data, size, response_mid, tag, 1);
      if (return_value != CL_RETVAL_OK) {
         CL_LOG(CL_LOG_ERROR,cl_get_error_text(return_value));
         if (copy_data == CL_TRUE) {
            free(help_data);
         }
         return return_value;
      }

      switch(cl_com_create_threads) {
         case CL_NO_THREAD:
            CL_LOG(CL_LOG_INFO,"no threads enabled");
            /* we just want to trigger write , no wait for read*/
            cl_commlib_trigger(handle, 1);
            break;
         case CL_RW_THREAD:
            /* we just want to trigger write , no wait for read*/
            cl_thread_trigger_event(handle->write_thread);
            break;
      }

   } else {

      CL_LOG_STR_STR_INT(CL_LOG_INFO, "add new message for:      ", un_resolved_hostname, component_name, (int)component_id);
   
      /* resolve hostname */
      return_value = cl_com_cached_gethostbyname(un_resolved_hostname, &unique_hostname,NULL, NULL, NULL);
      if (return_value != CL_RETVAL_OK) {
         CL_LOG(CL_LOG_ERROR,cl_get_error_text(return_value));
         if (copy_data == CL_TRUE) {
            free(help_data);
         }
         return return_value;
      }
   
   
      /* setup endpoint */
      receiver.comp_host = unique_hostname;
      receiver.comp_name = component_name;
      receiver.comp_id   = component_id;
   
      while(retry_send != 0) {
      
         /* lock handle connection list */
         cl_raw_list_lock(handle->connection_list);
         elem = cl_connection_list_get_first_elem(handle->connection_list);     
         while(elem) {
            ignore_connection = CL_FALSE;
            connection = elem->connection;
      
            if (connection->was_accepted == CL_TRUE) {
               /* ignore duplicate endpoints */
               switch (connection->crm_state) {
                  case CL_CRM_CS_UNDEFINED:
                  case CL_CRM_CS_CONNECTED: {
                     ignore_connection = CL_FALSE;
                     break;
                  }
                  default: {
                     ignore_connection = CL_TRUE;
                     CL_LOG(CL_LOG_WARNING,"ignore connection with unexpected connection state");
                     break;
                  }
               }
            }
            
            if (ignore_connection == CL_FALSE && 
                cl_com_compare_endpoints(connection->receiver, &receiver)) {
      
               /* send message to client (no broadcast) */
   
               if (connection->ccm_received != 0) {  
                  /* we have sent connection close response, do not accept any new message */
                  /* we wait till connection is down and try to reconnect  */
                  CL_LOG(CL_LOG_ERROR,"connection is going down now, can't send message (ccrm sent)");
                  break;
               }
      
               if (connection->connection_state == CL_CONNECTED && connection->connection_sub_state != CL_COM_WORK) {
                  CL_LOG(CL_LOG_WARNING,"connection is going down now, can't send message");
                  break;
               }
      
   
               if (  response_mid > 0 && response_mid > connection->last_received_message_id ) {
                  CL_LOG_INT(CL_LOG_DEBUG,"last_received_message_id:", (int)connection->last_received_message_id );
                  CL_LOG_INT(CL_LOG_DEBUG,"last_send_message_id    :", (int)connection->last_send_message_id);
                  CL_LOG_INT(CL_LOG_DEBUG,"response_mid to send    :", (int)response_mid);
   
                  CL_LOG(CL_LOG_ERROR,"Protocol error: haven't received such a high message id till now");
                  cl_raw_list_unlock(handle->connection_list);
                  free(unique_hostname);
                  if (copy_data == CL_TRUE) {
                     free(help_data);
                  }
                  return CL_RETVAL_PROTOCOL_ERROR;
               }    
   
               CL_LOG_STR_STR_INT(CL_LOG_INFO, "sending it to:        ", connection->receiver->comp_host,
                                                                         connection->receiver->comp_name,
                                                                         (int)connection->receiver->comp_id);

               return_value = cl_com_setup_message(&message, connection, help_data, size, ack_type,response_mid,tag);

               if (return_value != CL_RETVAL_OK) {
                  cl_raw_list_unlock(handle->connection_list);
                  free(unique_hostname);
                  if (copy_data == CL_TRUE) {
                     free(help_data);
                  }
                  return return_value;
               }
      
               my_mid = message->message_id;
               if (mid != NULL) {
                 *mid = message->message_id;
               }
               return_value = cl_message_list_append_send(connection, message, 1);
               if (return_value != CL_RETVAL_OK) {
                  cl_com_free_message(&message);
                  cl_raw_list_unlock(handle->connection_list);
                  free(unique_hostname);
                  return return_value;
               }
               message_added = 1;
               
               break;
            }
            elem = cl_connection_list_get_next_elem(elem);
         }
         cl_raw_list_unlock(handle->connection_list);
   
         /* if message is not added, the connection was not found -> try to open it */
         if (message_added != 1) {
            retry_send++;  
            return_value = cl_commlib_open_connection(handle, un_resolved_hostname, component_name, component_id );
            if (return_value != CL_RETVAL_OK) {
               free(unique_hostname);
               CL_LOG_STR(CL_LOG_ERROR,"cl_commlib_open_connection() returned: ",cl_get_error_text(return_value));
               if (copy_data == CL_TRUE) {
                  free(help_data);
               }
               return return_value;
            }
            if (retry_send >= 3) {
               CL_LOG(CL_LOG_ERROR,"can't open connection, don't retry to send this message");
               retry_send = 0;
            }
         } else {
            retry_send = 0; /* break */
         }
      }
   
      if (message_added == 1) {
         switch(cl_com_create_threads) {
               case CL_NO_THREAD:
                  CL_LOG(CL_LOG_INFO,"no threads enabled");
                  /* we just want to trigger write , no wait for read*/
                  cl_commlib_trigger(handle, 1);
                  break;
               case CL_RW_THREAD:
                  /* we just want to trigger write , no wait for read*/
                  cl_thread_trigger_event(handle->write_thread);
                  break;
         }
      } else {
         free(unique_hostname);
         if (copy_data == CL_TRUE) {
            free(help_data);
         }
         return CL_RETVAL_SEND_ERROR;
      } 
    
      if (ack_type == CL_MIH_MAT_NAK) {
         free(unique_hostname);
         return CL_RETVAL_OK;
      }
   
      if (wait_for_ack == CL_FALSE) {
         free(unique_hostname);
         return CL_RETVAL_OK;
      }
   
      CL_LOG_INT(CL_LOG_INFO,"message acknowledge expected, waiting for ack", (int)my_mid);
      return_value = cl_commlib_check_for_ack(handle, receiver.comp_host, component_name, component_id, my_mid, CL_TRUE);
      free(unique_hostname);
   }
   return return_value;
}







#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_commlib_get_last_message_time()"
int cl_commlib_get_last_message_time(cl_com_handle_t* handle, 
                                    char* un_resolved_hostname, char* component_name, unsigned long component_id, 
                                    unsigned long* message_time) {

   char* unique_hostname = NULL;
   int return_value;
   cl_com_endpoint_t receiver;

   /* set time to 0 if endpoint not found, otherwise return last communication time */
   /* otherwise return error */
   if (message_time) {
      *message_time = 0;
   }

   if (handle == NULL || un_resolved_hostname == NULL || component_name == NULL) {
      return CL_RETVAL_PARAMS;
   }
   
   if (component_id <= 0) {
      CL_LOG(CL_LOG_ERROR,"component id 0 is not allowed");
      return CL_RETVAL_PARAMS;
   }

   /* resolve hostname */
   return_value = cl_com_cached_gethostbyname(un_resolved_hostname, &unique_hostname, NULL, NULL, NULL);
   if (return_value != CL_RETVAL_OK) {
      return return_value;
   }

   /* setup endpoint */
   receiver.comp_host = unique_hostname;
   receiver.comp_name = component_name;
   receiver.comp_id   = component_id;


   return_value = cl_endpoint_list_get_last_touch_time(cl_com_get_endpoint_list(),&receiver,message_time);
   if (message_time) {
      CL_LOG_STR(CL_LOG_DEBUG,"host              :", receiver.comp_host);
      CL_LOG_STR(CL_LOG_DEBUG,"component         :", receiver.comp_name);
      CL_LOG_INT(CL_LOG_DEBUG,"last transfer time:", (int)*message_time);
   }

   free(unique_hostname); /* don't touch receiver object after this */

   return return_value;

#if 0

   /* This was old behaviour, when there was no cl_endpoint_list !!! */
   /* This code can be removed when endpoint list is used in future - CR */

   /* lock handle connection list */
   cl_raw_list_lock(handle->connection_list);

   elem = cl_connection_list_get_first_elem(handle->connection_list);
   while(elem) {
      connection = elem->connection;
      if ( cl_com_compare_endpoints(connection->receiver, &receiver) ) {
         /* found endpoint */
         if (time) {
            *time = (connection->last_transfer_time).tv_sec;
         }
         CL_LOG(CL_LOG_INFO,"found connection");
         cl_raw_list_unlock(handle->connection_list);
         free(unique_hostname); /* don't touch receiver object after this */
         return CL_RETVAL_OK;
      }
      elem = cl_connection_list_get_next_elem(handle->connection_list, elem);
   }

   /* unlock handle connection list */
   cl_raw_list_unlock(handle->connection_list);

   CL_LOG(CL_LOG_WARNING,"can't find connection");
   free(unique_hostname); /* don't touch receiver object after this */
   return CL_RETVAL_CONNECTION_NOT_FOUND;
#endif
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_trigger_thread()"
/* This thread is used for commlib specific calls (e.g. hostlist refresh) */
static void *cl_com_trigger_thread(void *t_conf) {
   int ret_val = CL_RETVAL_OK;
   int do_exit = 0;
   /* get pointer to cl_thread_settings_t struct */
   cl_thread_settings_t *thread_config = (cl_thread_settings_t*)t_conf; 


   /* set thread config data */
   if (cl_thread_set_thread_config(thread_config) != CL_RETVAL_OK) {
      CL_LOG(CL_LOG_ERROR,"thread setup error"); 
      do_exit = 1;
   }

   /* setup thread */
   CL_LOG(CL_LOG_INFO, "starting initialization ...");

   ret_val = CL_RETVAL_OK;
   if (ret_val != CL_RETVAL_OK) {
      do_exit = 1;
   }

   /* thread init done, trigger startup conditon variable*/
   cl_thread_func_startup(thread_config);
   CL_LOG(CL_LOG_INFO, "starting main loop ...");

   /* ok, thread main */
   while (do_exit == 0) {
      cl_thread_func_testcancel(thread_config);

      CL_LOG(CL_LOG_INFO,"trigger host list refresh ...");
      cl_com_host_list_refresh(cl_com_get_host_list());
      cl_com_endpoint_list_refresh(cl_com_get_endpoint_list());

      CL_LOG(CL_LOG_INFO,"wait for event ...");
      if ((ret_val = cl_thread_wait_for_event(thread_config,1,0 )) != CL_RETVAL_OK) {  /* nothing to do */
         switch(ret_val) {
            case CL_RETVAL_CONDITION_WAIT_TIMEOUT:
               CL_LOG(CL_LOG_INFO,"condition wait timeout");
               break;
            default:
               CL_LOG_STR( CL_LOG_INFO, ">got error<: ", cl_get_error_text(ret_val));
               do_exit = 1;
         }
      }
   }

   CL_LOG(CL_LOG_INFO, "exiting ...");
   /* at least set exit state */
   cl_thread_func_cleanup(thread_config);  
   return(NULL);
}


#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_handle_service_thread()"
static void *cl_com_handle_service_thread(void *t_conf) {
   int ret_val = CL_RETVAL_OK;
   int do_exit = 0;
   cl_com_handle_t* handle = NULL;


   /* get pointer to cl_thread_settings_t struct */
   cl_thread_settings_t *thread_config = (cl_thread_settings_t*)t_conf; 

   /* set thread config data */
   if (cl_thread_set_thread_config(thread_config) != CL_RETVAL_OK) {
      CL_LOG(CL_LOG_ERROR,"thread setup error"); 
      do_exit = 1;
   }

   /* setup thread */
   CL_LOG(CL_LOG_INFO, "starting initialization ...");

   /* get handle from thread_config */
   handle = (cl_com_handle_t*) thread_config->thread_user_data;

   /* thread init done, trigger startup conditon variable*/
   cl_thread_func_startup(thread_config);
   CL_LOG(CL_LOG_INFO, "starting main loop ...");

   /* ok, thread main */
   while (do_exit == 0) {
      cl_thread_func_testcancel(thread_config);
 
#ifdef CL_DO_COMMLIB_DEBUG
      {
         struct timeval now;
         cl_thread_list_elem_t* elem = NULL;

         gettimeofday(&now,NULL);
   
         cl_raw_list_lock(cl_com_thread_list);
         elem = cl_thread_list_get_first_elem(cl_com_thread_list);
         while(elem) {
            if (elem->thread_config->thread_last_cancel_test_time.tv_sec + 15 < now.tv_sec ) {
               CL_LOG_STR(CL_LOG_ERROR,"POSSIBLE DEADLOCK DETECTED (thread_list) => ",elem->thread_config->thread_name);
            }
            elem = cl_thread_list_get_next_elem(elem);
         }
         cl_raw_list_unlock(cl_com_thread_list);
#if 0   
         cl_raw_list_lock(handle->work_thread_list);
         elem = cl_thread_list_get_first_elem(handle->work_thread_list);
         while(elem) {
            if (elem->thread_config->thread_last_cancel_test_time.tv_sec + 15 < now.tv_sec ) {
               CL_LOG_STR(CL_LOG_ERROR,"POSSIBLE DEADLOCK DETECTED (work list) => ",elem->thread_config->thread_name );
            }
            elem = cl_thread_list_get_next_elem(elem);
         }
         cl_raw_list_unlock(handle->work_thread_list);
#endif
      }
#endif /* CL_DO_COMMLIB_DEBUG */

      /* we don't want to force statistics update every one second */
      cl_commlib_calculate_statistic(handle, CL_FALSE, 1);
      /* ceck for debug clients */
      cl_commlib_handle_debug_clients(handle, CL_TRUE);

      /* there is nothing to do, wait for events */
      CL_LOG(CL_LOG_INFO,"wait for event ...");
      if ((ret_val = cl_thread_wait_for_event(thread_config,handle->select_sec_timeout, handle->select_usec_timeout)) != CL_RETVAL_OK) {
         switch(ret_val) {
            case CL_RETVAL_CONDITION_WAIT_TIMEOUT:
               CL_LOG(CL_LOG_INFO,"condition wait timeout");
               break;
            default:
               CL_LOG_STR( CL_LOG_INFO, ">got error<: ", cl_get_error_text(ret_val));
               do_exit = 1;
         }
      }
      cl_thread_clear_events(thread_config);
   }

   CL_LOG(CL_LOG_INFO, "exiting ...");

   /* at least set exit state */
   cl_thread_func_cleanup(thread_config);  
   return(NULL);
}




#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_handle_read_thread()"
static void *cl_com_handle_read_thread(void *t_conf) {
   int ret_val = CL_RETVAL_OK;
   int return_value;
   int do_exit = 0;
   int wait_for_events = 1;
   int select_sec_timeout = 0;
   int select_usec_timeout = 100*1000;
   cl_app_message_queue_elem_t* mq_elem = NULL;
   int mq_return_value = CL_RETVAL_OK; 


   int message_received = 0;
   int trigger_write_thread = 0;
   cl_connection_list_elem_t* elem = NULL;
   cl_com_connection_t* the_handler = NULL;
   char tmp_string[1024];


   cl_handle_list_elem_t* handle_elem = NULL;
   cl_com_handle_t* handle = NULL;

   /* get pointer to cl_thread_settings_t struct */
   cl_thread_settings_t *thread_config = (cl_thread_settings_t*)t_conf; 


   /* set thread config data */
   if (cl_thread_set_thread_config(thread_config) != CL_RETVAL_OK) {
      CL_LOG(CL_LOG_ERROR,"thread setup error"); 
      do_exit = 1;
   }

   /* setup thread */
   CL_LOG(CL_LOG_INFO, "starting initialization ...");

   /* thread init done, trigger startup conditon variable*/
   cl_thread_func_startup(thread_config);
   CL_LOG(CL_LOG_INFO, "starting main loop ...");

   /* ok, thread main */
   while (do_exit == 0) {
      wait_for_events = 1;
      trigger_write_thread = 0;
      message_received = 0;


      cl_thread_func_testcancel(thread_config);
 
      if (handle == NULL) {
         cl_raw_list_lock(cl_com_handle_list);
         handle_elem = cl_handle_list_get_first_elem(cl_com_handle_list);
         while(handle_elem && handle == NULL ) {
            if (handle_elem->handle->read_thread == thread_config) {
               handle = handle_elem->handle;
               select_sec_timeout = handle->select_sec_timeout;
               select_usec_timeout = handle->select_usec_timeout;
               wait_for_events = 0;
            }
            handle_elem = cl_handle_list_get_next_elem(handle_elem);
         }
         cl_raw_list_unlock(cl_com_handle_list);
      } else {
         struct timeval now;

         
         /* check number of connections */
         cl_commlib_check_connection_count(handle);

         cl_connection_list_destroy_connections_to_close(handle->connection_list,1); /* OK */

         if (handle->do_shutdown == 0 && handle->max_connection_count_reached == CL_FALSE) {
            the_handler = handle->service_handler;
         } else {
            the_handler = NULL;
         }

         cl_raw_list_lock(handle->send_message_queue);
         while((mq_elem = cl_app_message_queue_get_first_elem(handle->send_message_queue)) != NULL) {
            mq_return_value = cl_commlib_send_message_to_endpoint(handle, mq_elem->snd_destination,
                                                                  mq_elem->snd_ack_type, mq_elem->snd_data,
                                                                  mq_elem->snd_size, mq_elem->snd_response_mid,
                                                                  mq_elem->snd_tag); 
            /* remove queue entries */
            cl_raw_list_remove_elem(handle->send_message_queue, mq_elem->raw_elem);
            if (mq_return_value != CL_RETVAL_OK) {
               CL_LOG_STR(CL_LOG_ERROR,"can't send message:", cl_get_error_text(mq_return_value));
               free(mq_elem->snd_data);
            }
            cl_com_free_endpoint(&(mq_elem->snd_destination));
            free(mq_elem);
         }
         cl_raw_list_unlock(handle->send_message_queue);


         ret_val = cl_com_open_connection_request_handler(handle->framework, 
                                                          handle->connection_list, 
                                                          the_handler,
                                                          handle->select_sec_timeout ,
                                                          handle->select_usec_timeout,
                                                          CL_R_SELECT);
      
         switch (ret_val) {
            case CL_RETVAL_SELECT_TIMEOUT:
               CL_LOG(CL_LOG_INFO,"got select timeout");
               wait_for_events = 0;
               break;
            case CL_RETVAL_SELECT_INTERRUPT:
               CL_LOG(CL_LOG_WARNING,"got select interrupt");
               wait_for_events = 0;
               break;
            case CL_RETVAL_NO_SELECT_DESCRIPTORS:
               CL_LOG(CL_LOG_INFO,"no select descriptors");
               wait_for_events = 1;
               break;
            case CL_RETVAL_OK:
               /* do not wait for events, because we do select */
               wait_for_events = 0;
               break;
            default:
               CL_LOG_STR(CL_LOG_ERROR,"got error:",cl_get_error_text(ret_val));
               break;
         }


         cl_raw_list_lock(handle->connection_list);
         /* read messages */
         elem = cl_connection_list_get_first_elem(handle->connection_list);     
         gettimeofday(&now,NULL);
         
         while(elem) {
            switch(elem->connection->connection_state) {
   
               case CL_DISCONNECTED: {
                  /* open connection if there are messages to send */
                  if ( cl_raw_list_get_elem_count(elem->connection->send_message_list) > 0) {
                     CL_LOG(CL_LOG_INFO,"setting connection state to CL_OPENING");
                     elem->connection->connection_state = CL_OPENING;
                  }
                  break;
               }
   
               case CL_OPENING: {
                  /* trigger connect */
                  if (elem->connection->data_read_flag == CL_COM_DATA_READY) {
                     return_value = cl_com_open_connection(elem->connection, handle->open_connection_timeout,NULL , NULL, NULL, NULL);
                     if (return_value != CL_RETVAL_OK && return_value != CL_RETVAL_UNCOMPLETE_WRITE ) {
                        CL_LOG_STR(CL_LOG_ERROR,"could not open connection:",cl_get_error_text(return_value));
                        elem->connection->connection_state = CL_CLOSING;
                        elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                     }
                     if (return_value != CL_RETVAL_OK && cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
                        CL_LOG(CL_LOG_WARNING,"setting connection state to closing");
                        elem->connection->connection_state = CL_CLOSING;
                        elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                     }
                  } else {
                     /* check timeouts */
                     if ( elem->connection->read_buffer_timeout_time != 0) {
                        if ( now.tv_sec >= elem->connection->read_buffer_timeout_time || cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
                           CL_LOG(CL_LOG_ERROR,"read timeout for connection opening");
                           elem->connection->connection_state = CL_CLOSING;
                           elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                        }
                     }
                     if ( elem->connection->write_buffer_timeout_time != 0) {
                        if ( now.tv_sec >= elem->connection->write_buffer_timeout_time || cl_com_get_ignore_timeouts_flag() == CL_TRUE ) {
                           CL_LOG(CL_LOG_ERROR,"write timeout for connection opening");
                           elem->connection->connection_state = CL_CLOSING;
                           elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                        }
                     }
                  }
                  break;
               }
   
               case CL_ACCEPTING: {
                  int return_value;
                  if (elem->connection->data_read_flag == CL_COM_DATA_READY  ) {
                     return_value = cl_com_connection_complete_accept(elem->connection,handle->open_connection_timeout,1);
                     if (return_value != CL_RETVAL_OK && 
                         return_value != CL_RETVAL_UNCOMPLETE_READ && 
                         return_value != CL_RETVAL_UNCOMPLETE_WRITE && 
                         return_value != CL_RETVAL_SELECT_ERROR ) {
                        CL_LOG_STR(CL_LOG_ERROR,"connection accept error:",cl_get_error_text(return_value));
                        elem->connection->connection_state = CL_CLOSING;
                        elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                     }
                     if (return_value != CL_RETVAL_OK && cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
                        elem->connection->connection_state = CL_CLOSING;
                        elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                     } 
                     if (return_value == CL_RETVAL_OK) {
                        elem->connection->connection_state = CL_CONNECTING;
                        elem->connection->connection_sub_state = CL_COM_READ_INIT;
                        elem->connection->data_read_flag = CL_COM_DATA_NOT_READY;
                     }
                  } else {
                     /* check timeouts */
                     if ( elem->connection->read_buffer_timeout_time != 0) {
                        if ( now.tv_sec >= elem->connection->read_buffer_timeout_time ) {
                           CL_LOG(CL_LOG_ERROR,"accept timeout for connection");
                           elem->connection->connection_state = CL_CLOSING;
                           elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                        }
                     }
                     if ( elem->connection->write_buffer_timeout_time != 0) {
                        if ( now.tv_sec >= elem->connection->write_buffer_timeout_time ) {
                           CL_LOG(CL_LOG_ERROR,"accept timeout for connection");
                           elem->connection->connection_state = CL_CLOSING;
                           elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                        }
                     }
                  }
                  break;
               }
   
               case CL_CONNECTING: {
                  if ( elem->connection->data_read_flag == CL_COM_DATA_READY  ) {
                     return_value = cl_com_connection_complete_request(elem->connection,handle->open_connection_timeout,1,CL_R_SELECT );
   
                     if (return_value != CL_RETVAL_OK && 
                         return_value != CL_RETVAL_UNCOMPLETE_READ && 
                         return_value != CL_RETVAL_UNCOMPLETE_WRITE && 
                         return_value != CL_RETVAL_SELECT_ERROR ) {
                        CL_LOG_STR(CL_LOG_ERROR,"connection establish error:",cl_get_error_text(return_value));
                        elem->connection->connection_state = CL_CLOSING;
                        elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                     }
                     if (return_value != CL_RETVAL_OK && cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
                        elem->connection->connection_state = CL_CLOSING;
                        elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                     } 
                     if ( elem->connection->connection_state == CL_CONNECTED ) {
                        cl_commlib_finish_request_completeness(elem->connection);
                        /* connection is now in connect state, do select before next reading */
                        elem->connection->data_read_flag = CL_COM_DATA_NOT_READY;
                     }
                  } else {
                     /* check timeouts */
                     if ( elem->connection->read_buffer_timeout_time != 0) {
                        if ( now.tv_sec >= elem->connection->read_buffer_timeout_time || cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
                           CL_LOG(CL_LOG_ERROR,"read timeout for connection completion");
                           elem->connection->connection_state = CL_CLOSING;
                           elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                        }
                     }
                     if ( elem->connection->write_buffer_timeout_time != 0) {
                        if ( now.tv_sec >= elem->connection->write_buffer_timeout_time || cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
                           CL_LOG(CL_LOG_ERROR,"write timeout for connection completion");
                           elem->connection->connection_state = CL_CLOSING;
                           elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                        }
                     }
                  }
                  break;
               }
               
               case CL_CONNECTED: {
   
                  /* check ack timeouts */
                  return_value = cl_commlib_handle_connection_ack_timeouts(elem->connection);
   
                  if (elem->connection->data_read_flag == CL_COM_DATA_READY && 
                      elem->connection->ccrm_sent      == 0 && 
                      elem->connection->ccrm_received  == 0) {
                     /* TODO: use read thread pool */
                     return_value = cl_commlib_handle_connection_read(elem->connection);
                     if ( return_value != CL_RETVAL_OK && 
                          return_value != CL_RETVAL_UNCOMPLETE_READ && 
                          return_value != CL_RETVAL_SELECT_ERROR ) {
                        elem->connection->connection_state = CL_CLOSING;
                        elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                        CL_LOG_STR(CL_LOG_ERROR,"read from connection: setting close flag! Reason:", cl_get_error_text(return_value));
                        snprintf(tmp_string, 1024, MSG_CL_COMMLIB_CLOSING_SSU,
                                 elem->connection->remote->comp_host,
                                 elem->connection->remote->comp_name,
                                 sge_u32c(elem->connection->remote->comp_id));
                        cl_commlib_push_application_error(CL_LOG_ERROR, return_value, tmp_string);
                     }
                     if (return_value != CL_RETVAL_OK && cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
                        elem->connection->connection_state = CL_CLOSING;
                        elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                     }
                     message_received = 1;
                  } else {
                     /* check timeouts */
                     if ( elem->connection->read_buffer_timeout_time != 0) {
                        if ( now.tv_sec >= elem->connection->read_buffer_timeout_time ) {
                           CL_LOG(CL_LOG_ERROR,"connection read timeout");
                           elem->connection->connection_state = CL_CLOSING;
                           elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                        }
                     }
                     if ( elem->connection->ccrm_received != 0 ) {
                        CL_LOG(CL_LOG_WARNING, "will not read from this connection, because ccrm was received!");
                     }
   
                  }
                  if (elem->connection->ccm_received == 1 ) {
                     if( cl_raw_list_get_elem_count(elem->connection->send_message_list) == 0 && 
                         cl_raw_list_get_elem_count(elem->connection->received_message_list) == 0) {
                        elem->connection->ccm_received = 2;
                        elem->connection->connection_sub_state = CL_COM_SENDING_CCRM;
                        cl_commlib_send_ccrm_message(elem->connection);
                     } else {
                        CL_LOG_INT(CL_LOG_INFO,"receive buffer:",(int)cl_raw_list_get_elem_count(elem->connection->received_message_list) );
                        CL_LOG_INT(CL_LOG_INFO,"send buffer   :",(int)cl_raw_list_get_elem_count(elem->connection->send_message_list) );
                     }
                  }
                  break;
               }
               default: {
                  break;
               }
            } /* end of switch */

            if (elem->connection->data_write_flag == CL_COM_DATA_READY) {
               /* there is data to write, trigger write thread */
               trigger_write_thread = 1;
            }

            elem = cl_connection_list_get_next_elem(elem);
         }
         cl_raw_list_unlock(handle->connection_list);


         /* check for new connections */
         if (handle->service_provider                == CL_TRUE           && 
             handle->service_handler->data_read_flag == CL_COM_DATA_READY) {

            /* we have a new connection request */
            cl_com_connection_t* new_con = NULL;
            cl_com_connection_request_handler(handle->service_handler, &new_con);
            if (new_con != NULL) {
               /* got new connection request */
               new_con->handler = handle->service_handler->handler;
               CL_LOG(CL_LOG_INFO,"adding new client");
               gettimeofday(&now,NULL);
               new_con->read_buffer_timeout_time = now.tv_sec + handle->open_connection_timeout;
               cl_connection_list_append_connection(handle->connection_list, new_con,1);
               handle->statistic->new_connections = handle->statistic->new_connections + 1;
               new_con = NULL;
            }
         }


         if (trigger_write_thread != 0) {
            cl_thread_trigger_event(handle->write_thread);
         }
         
         /* trigger threads which are waiting for read_condition when
            we will do a wait by itself (because we have no descriptors 
            for reading) or when we have received a message */
         if ( wait_for_events || message_received != 0 ) {
            cl_thread_trigger_thread_condition(handle->read_condition,1);

            /* if we have received a message which was no protocol message
               trigger application ( cl_commlib_receive_message() ) */
         } 
      }


      /* TODO: never wait for events here, use dummy pipe to wakeup read_select when
               a new connection is added, use select() to sleep */

      if (wait_for_events != 0) {
         if ((ret_val = cl_thread_wait_for_event(thread_config,select_sec_timeout,select_usec_timeout )) != CL_RETVAL_OK) {
            switch(ret_val) {
               case CL_RETVAL_CONDITION_WAIT_TIMEOUT:
                  CL_LOG(CL_LOG_INFO,"READ THREAD GOT CONDITION WAIT TIMEOUT");
                  break;
               default:
                  CL_LOG_STR( CL_LOG_INFO, ">got error<: ", cl_get_error_text(ret_val));
                  do_exit = 1;
            }
         }
         /* cleanup all trigger events */
         cl_thread_clear_events(thread_config);
      }
   }

   CL_LOG(CL_LOG_INFO, "exiting ...");

   /* at least set exit state */
   cl_thread_func_cleanup(thread_config);  
   return(NULL);
}






#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "cl_com_handle_write_thread()"
static void *cl_com_handle_write_thread(void *t_conf) {
   int return_value = CL_RETVAL_OK;
   int ret_val = CL_RETVAL_OK;
   int do_exit = 0;
   int wait_for_events = 1;
   int select_sec_timeout = 0;
   int select_usec_timeout = 100*1000;
   cl_connection_list_elem_t* elem = NULL;
   struct timeval now;
   cl_handle_list_elem_t* handle_elem = NULL;
   cl_com_handle_t* handle = NULL;
   int trigger_read_thread = 0;
   char tmp_string[1024];
   cl_app_message_queue_elem_t* mq_elem = NULL;
   int mq_return_value = CL_RETVAL_OK; 

   /* get pointer to cl_thread_settings_t struct */
   cl_thread_settings_t *thread_config = (cl_thread_settings_t*)t_conf; 


   /* set thread config data */
   if (cl_thread_set_thread_config(thread_config) != CL_RETVAL_OK) {
      CL_LOG(CL_LOG_ERROR,"thread setup error"); 
      do_exit = 1;
   }

   /* setup thread */
   CL_LOG(CL_LOG_INFO, "starting initialization ...");

   /* thread init done, trigger startup conditon variable*/
   cl_thread_func_startup(thread_config);
   CL_LOG(CL_LOG_INFO, "starting main loop ...");

   /* ok, thread main */
   while (do_exit == 0) {
      trigger_read_thread = 0;
      cl_thread_func_testcancel(thread_config);
 
      if (handle == NULL) {
         cl_raw_list_lock(cl_com_handle_list);
         handle_elem = cl_handle_list_get_first_elem(cl_com_handle_list);
         while(handle_elem && handle == NULL ) {
            if (handle_elem->handle->write_thread == thread_config) {
               handle = handle_elem->handle;
               select_sec_timeout = handle->select_sec_timeout; 
               select_usec_timeout = handle->select_usec_timeout;
               wait_for_events = 0;
            }
            handle_elem = cl_handle_list_get_next_elem(handle_elem);
         }
         cl_raw_list_unlock(cl_com_handle_list);
         
      } else {

         cl_raw_list_lock(handle->send_message_queue);
         while((mq_elem = cl_app_message_queue_get_first_elem(handle->send_message_queue)) != NULL) {
            mq_return_value = cl_commlib_send_message_to_endpoint(handle, mq_elem->snd_destination,
                                                                  mq_elem->snd_ack_type, mq_elem->snd_data,
                                                                  mq_elem->snd_size, mq_elem->snd_response_mid,
                                                                  mq_elem->snd_tag); 
            /* remove queue entries */
            cl_raw_list_remove_elem(handle->send_message_queue, mq_elem->raw_elem);
            if (mq_return_value != CL_RETVAL_OK) {
               CL_LOG_STR(CL_LOG_ERROR,"can't send message:", cl_get_error_text(mq_return_value));
               free(mq_elem->snd_data);
            }
            cl_com_free_endpoint(&(mq_elem->snd_destination));
            free(mq_elem);
         }
         cl_raw_list_unlock(handle->send_message_queue);


         /* do write select */
         ret_val = cl_com_open_connection_request_handler(handle->framework, 
                                                          handle->connection_list, 
                                                          handle->service_handler,
                                                          handle->select_sec_timeout ,
                                                          handle->select_usec_timeout,
                                                          CL_W_SELECT  );
         switch (ret_val) {
            case CL_RETVAL_SELECT_TIMEOUT:
               CL_LOG(CL_LOG_INFO,"write select timeout");
               wait_for_events = 0;
               break;
            case CL_RETVAL_SELECT_INTERRUPT:
               CL_LOG(CL_LOG_WARNING,"select interrupt");
               wait_for_events = 0;
               break;
            case CL_RETVAL_NO_SELECT_DESCRIPTORS:
               CL_LOG(CL_LOG_INFO,"no descriptors to select");
               wait_for_events = 1;
               break;
            case CL_RETVAL_OK: 
               wait_for_events = 1;
               cl_raw_list_lock(handle->connection_list);
               /* read messages */
               elem = cl_connection_list_get_first_elem(handle->connection_list);     
               gettimeofday(&now,NULL);
               while(elem) {
                  switch(elem->connection->connection_state) {
                     case CL_OPENING: {
                        /* trigger connect */
                        if (elem->connection->fd_ready_for_write == CL_COM_DATA_READY &&
                             elem->connection->data_write_flag == CL_COM_DATA_READY ) {
                           return_value = cl_com_open_connection(elem->connection, handle->open_connection_timeout,NULL , NULL, NULL, NULL);
                           if (return_value != CL_RETVAL_OK && return_value != CL_RETVAL_UNCOMPLETE_WRITE ) {
                              CL_LOG_STR(CL_LOG_ERROR,"could not open connection:",cl_get_error_text(return_value));
                              elem->connection->connection_state = CL_CLOSING;
                              elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                           }
                           if (return_value != CL_RETVAL_OK && cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
                              CL_LOG(CL_LOG_WARNING,"setting connection state to closing");
                              elem->connection->connection_state = CL_CLOSING;
                              elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                           }
                        } else {
                           /* check timeouts */
                           if ( elem->connection->read_buffer_timeout_time != 0) {
                              if ( now.tv_sec >= elem->connection->read_buffer_timeout_time || cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
                                 CL_LOG(CL_LOG_ERROR,"read timeout for connection opening");
                                 elem->connection->connection_state = CL_CLOSING;
                                 elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                              }
                           }
                           if ( elem->connection->write_buffer_timeout_time != 0) {
                              if ( now.tv_sec >= elem->connection->write_buffer_timeout_time  || cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
                                 CL_LOG(CL_LOG_ERROR,"write timeout for connection opening");
                                 elem->connection->connection_state = CL_CLOSING;
                                 elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                              }
                           }
                        }
                        break;
                     }
   
                     case CL_ACCEPTING: {
                        int return_value;
                        CL_LOG(CL_LOG_WARNING,"connection state is CL_ACCEPTING");
                        
                        if (elem->connection->fd_ready_for_write == CL_COM_DATA_READY && 
                            elem->connection->data_write_flag == CL_COM_DATA_READY        ) {
                           return_value = cl_com_connection_complete_accept(elem->connection,handle->open_connection_timeout,1);
                           if (return_value != CL_RETVAL_OK && 
                               return_value != CL_RETVAL_UNCOMPLETE_READ && 
                               return_value != CL_RETVAL_UNCOMPLETE_WRITE && 
                               return_value != CL_RETVAL_SELECT_ERROR ) {
                              CL_LOG_STR(CL_LOG_ERROR,"connection accept error:",cl_get_error_text(return_value));
                              elem->connection->connection_state = CL_CLOSING;
                              elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                           }
                           if (return_value != CL_RETVAL_OK && cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
                              elem->connection->connection_state = CL_CLOSING;
                              elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                           } 
                           if (return_value == CL_RETVAL_OK) {
                              elem->connection->connection_state = CL_CONNECTING;
                              elem->connection->connection_sub_state = CL_COM_READ_INIT;
                              elem->connection->data_read_flag = CL_COM_DATA_NOT_READY;
                           }
                        } else {
                           /* check timeouts */
                           if ( elem->connection->read_buffer_timeout_time != 0) {
                              if ( now.tv_sec >= elem->connection->read_buffer_timeout_time ) {
                                 CL_LOG(CL_LOG_ERROR,"accept timeout for connection");
                                 elem->connection->connection_state = CL_CLOSING;
                                 elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                              }
                           }
                           if ( elem->connection->write_buffer_timeout_time != 0) {
                              if ( now.tv_sec >= elem->connection->write_buffer_timeout_time ) {
                                 CL_LOG(CL_LOG_ERROR,"accept timeout for connection");
                                 elem->connection->connection_state = CL_CLOSING;
                                 elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                              }
                           }
                        }
                        break;
                     }
   
                     case CL_CONNECTING: {
                        if ( elem->connection->fd_ready_for_write == CL_COM_DATA_READY &&
                             elem->connection->data_write_flag == CL_COM_DATA_READY ) {
                           return_value = cl_com_connection_complete_request(elem->connection,handle->open_connection_timeout,1,CL_W_SELECT );
                           if (return_value != CL_RETVAL_OK && 
                               return_value != CL_RETVAL_UNCOMPLETE_READ && 
                               return_value != CL_RETVAL_UNCOMPLETE_WRITE && 
                               return_value != CL_RETVAL_SELECT_ERROR ) {
                              CL_LOG_STR(CL_LOG_ERROR,"connection establish error:",cl_get_error_text(return_value));
                              elem->connection->connection_state = CL_CLOSING;
                              elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                           }
                           if (return_value != CL_RETVAL_OK && cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
                              elem->connection->connection_state = CL_CLOSING;
                              elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                           }
                           if ( elem->connection->connection_state == CL_CONNECTED ) {
                              cl_commlib_finish_request_completeness(elem->connection);
                              /* connection is now in connect state, do select before next reading */
                              elem->connection->fd_ready_for_write = CL_COM_DATA_NOT_READY;
                           }
                        } else {
                           /* check timeouts */
                           if ( elem->connection->read_buffer_timeout_time != 0) {
                              if ( now.tv_sec >= elem->connection->read_buffer_timeout_time || cl_com_get_ignore_timeouts_flag() == CL_TRUE ) {
                                 CL_LOG(CL_LOG_ERROR,"read timeout for connection completion");
                                 elem->connection->connection_state = CL_CLOSING;
                                 elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                              }
                           }
                           if ( elem->connection->write_buffer_timeout_time != 0) {
                              if ( now.tv_sec >= elem->connection->write_buffer_timeout_time || cl_com_get_ignore_timeouts_flag() == CL_TRUE ) {
                                 CL_LOG(CL_LOG_ERROR,"write timeout for connection completion");
                                 elem->connection->connection_state = CL_CLOSING;
                                 elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                              }
                           }
                        }
                        break;
                     }
                     
                     case CL_CONNECTED: {
                        if (elem->connection->fd_ready_for_write == CL_COM_DATA_READY &&
                            elem->connection->data_write_flag    == CL_COM_DATA_READY &&
                            elem->connection->ccrm_sent == 0 ) {
                           /* TODO implement thread pool for data writing */
                           return_value = cl_commlib_handle_connection_write(elem->connection);
                           if (return_value != CL_RETVAL_OK && 
                               return_value != CL_RETVAL_UNCOMPLETE_WRITE && 
                               return_value != CL_RETVAL_SELECT_ERROR ) {
                              elem->connection->connection_state = CL_CLOSING;
                              elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                              CL_LOG_STR(CL_LOG_ERROR,"write to connection: setting close flag! Reason:", cl_get_error_text(return_value));
                              snprintf(tmp_string, 1024, MSG_CL_COMMLIB_CLOSING_SSU,
                                       elem->connection->remote->comp_host,
                                       elem->connection->remote->comp_name,
                                       sge_u32c(elem->connection->remote->comp_id));
                              cl_commlib_push_application_error(CL_LOG_ERROR, return_value, tmp_string);
                           }
                           if (return_value != CL_RETVAL_OK && cl_com_get_ignore_timeouts_flag() == CL_TRUE) {
                              elem->connection->connection_state = CL_CLOSING;
                              elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                           }
                        } else {
                           /* check timeouts */
                           if ( elem->connection->write_buffer_timeout_time != 0) {
                              if ( now.tv_sec >= elem->connection->write_buffer_timeout_time ) {
                                 CL_LOG(CL_LOG_ERROR,"write timeout for connected endpoint");
                                 snprintf(tmp_string, 1024, MSG_CL_COMMLIB_CLOSING_SSU,
                                          elem->connection->remote->comp_host,
                                          elem->connection->remote->comp_name,
                                          sge_u32c(elem->connection->remote->comp_id));
                                 cl_commlib_push_application_error(CL_LOG_ERROR, CL_RETVAL_SEND_TIMEOUT, tmp_string);
                                 elem->connection->connection_state = CL_CLOSING;
                                 elem->connection->connection_sub_state = CL_COM_DO_SHUTDOWN;
                              }
                           }
                        }
                        break;
                     }
                     default: {
                        break;
                     }
                  } /* end of switch */
                  if (elem->connection->ccm_received == 1 ) {
                     if( cl_raw_list_get_elem_count(elem->connection->send_message_list)     == 0 && 
                         cl_raw_list_get_elem_count(elem->connection->received_message_list) == 0)   {

                        elem->connection->ccm_received = 2;
                        elem->connection->connection_sub_state = CL_COM_SENDING_CCRM;
                        cl_commlib_send_ccrm_message(elem->connection);
                        CL_LOG(CL_LOG_INFO,"sending ccrm");
                     } else {
                        CL_LOG_INT(CL_LOG_INFO,"receive buffer:",(int)cl_raw_list_get_elem_count(elem->connection->received_message_list) );
                        CL_LOG_INT(CL_LOG_INFO,"send buffer   :",(int)cl_raw_list_get_elem_count(elem->connection->send_message_list) );
                     }
                  }
                  if (elem->connection->ccrm_sent != 0) {
                     trigger_read_thread = 1;
                  }
                  if (elem->connection->data_write_flag == CL_COM_DATA_READY) {
                     /* still data to write, do not wait for events */
                     wait_for_events = 0;
                  }
                  elem = cl_connection_list_get_next_elem(elem);
               }

               /* now take list entry and add it at least if there are more connections */
               /* this must be done in order to NOT prefer the first connection in list */
               if ( cl_raw_list_get_elem_count(handle->connection_list) > 1) {
                     elem = cl_connection_list_get_first_elem(handle->connection_list);
                     cl_raw_list_dechain_elem(handle->connection_list,elem->raw_elem);
                     cl_raw_list_append_dechained_elem(handle->connection_list,elem->raw_elem);
               }

               cl_raw_list_unlock(handle->connection_list);
               if (trigger_read_thread != 0) {
#ifdef CL_DO_COMMLIB_DEBUG
                  CL_LOG(CL_LOG_DEBUG,"triggering read thread");
#endif
                  cl_thread_trigger_event(handle->read_thread);
               }
               break;
            default:
               wait_for_events = 1;
               CL_LOG_STR(CL_LOG_ERROR,"got error:",cl_get_error_text(ret_val));
               break;
         }  /* switch */
      }
      if (wait_for_events != 0 ) {
         if ((ret_val = cl_thread_wait_for_event(thread_config, select_sec_timeout, select_usec_timeout )) != CL_RETVAL_OK) {
            switch(ret_val) {
               case CL_RETVAL_CONDITION_WAIT_TIMEOUT:
                  CL_LOG(CL_LOG_INFO,"WRITE THREAD GOT CONDITION WAIT TIMEOUT");
                  break;
               default:
                  CL_LOG_STR( CL_LOG_INFO, ">got error<: ", cl_get_error_text(ret_val));
                  do_exit = 1;
            }
         }
         /* set event count to zero, because this thread will
            not wait when something is to do */
         cl_thread_clear_events(thread_config);
      }
   }
   CL_LOG(CL_LOG_INFO, "exiting ...");

   /* at least set exit state */
   cl_thread_func_cleanup(thread_config);  
   return(NULL);
}



#ifdef __CL_FUNCTION__
#undef __CL_FUNCTION__
#endif
#define __CL_FUNCTION__ "getuniquehostname()"
/* MT-NOTE: getuniquehostname() is MT safe */
int getuniquehostname(const char *hostin, char *hostout, int refresh_aliases) {
   char* resolved_host = NULL;
   int ret_val;

   if (refresh_aliases != 0) {
      /* TODO: refresh host alias file? But it's never used */
      CL_LOG(CL_LOG_ERROR,"getuniquehostname() refresh of alias file not implemented");
   }
   ret_val = cl_com_cached_gethostbyname((char*)hostin, &resolved_host, NULL, NULL, NULL );
   if (resolved_host != NULL) {
      if (strlen(resolved_host) >= CL_MAXHOSTLEN ) {
         char tmp_buffer[1024];
         snprintf(tmp_buffer, 1024, MSG_CL_COMMLIB_HOSTNAME_EXEEDS_MAX_HOSTNAME_LENGTH_SU,
                  resolved_host, sge_u32c(CL_MAXHOSTLEN));
         cl_commlib_push_application_error(CL_LOG_ERROR, CL_RETVAL_HOSTNAME_LENGTH_ERROR, tmp_buffer);
         free(resolved_host);
         return CL_RETVAL_HOSTNAME_LENGTH_ERROR;
      }
      snprintf(hostout, CL_MAXHOSTLEN, "%s", resolved_host );
      free(resolved_host);
   }
   return ret_val;
}

