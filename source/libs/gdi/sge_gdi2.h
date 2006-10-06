#ifndef __SGE_GDI2_H
#define __SGE_GDI2_H
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

#ifdef TEST_GDI2

#include "sgeobj/sge_conf.h"

#ifdef  __cplusplus
extern "C" {
#endif

enum {
   TAG_NONE            = 0,     /* usable e.g. as delimiter in a tag array */
   TAG_OLD_REQUEST,
   TAG_GDI_REQUEST,
   TAG_ACK_REQUEST,
   TAG_REPORT_REQUEST,
   TAG_FINISH_REQUEST,
   TAG_JOB_EXECUTION,
   TAG_SLAVE_ALLOW,
   TAG_CHANGE_TICKET,
   TAG_SIGJOB,
   TAG_SIGQUEUE,
   TAG_KILL_EXECD,
   TAG_NEW_FEATURES,     /*12*/
   TAG_GET_NEW_CONF,
   TAG_JOB_REPORT,              /* cull based job reports */
   TAG_QSTD_QSTAT,
   TAG_TASK_EXIT,
   TAG_TASK_TID,
   TAG_EVENT_CLIENT_EXIT

#ifdef KERBEROS
  ,TAG_AUTH_FAILURE
#endif

};


lList
*sge_gdi2(sge_gdi_ctx_class_t *ctx, u_long32 target, u_long32 cmd, lList **lpp, lCondition *cp, lEnumeration *enp);

int
sge_gdi2_multi(sge_gdi_ctx_class_t *ctx, lList **alpp, int mode, u_long32 target, u_long32 cmd, lList **lp, 
              lCondition *cp, lEnumeration *enp, lList **malpp,
              state_gdi_multi *state, bool do_copy);

int
sge_gdi2_multi_sync(sge_gdi_ctx_class_t *ctx, lList **alpp, int mode, u_long32 target, u_long32 cmd, lList **lp,
              lCondition *cp, lEnumeration *enp, lList **malpp,
              state_gdi_multi *state, bool do_copy, bool do_sync);

int sge_gdi2_get_any_request(sge_gdi_ctx_class_t *ctx, char *rhost, char *commproc, u_short *id, sge_pack_buffer *pb, 
                    int *tag, int synchron, u_long32 for_request_mid, u_long32* mid);

int sge_gdi2_send_any_request(sge_gdi_ctx_class_t *ctx, int synchron, u_long32 *mid,
                              const char *rhost, const char *commproc, int id,
                              sge_pack_buffer *pb, 
                              int tag, u_long32  response_id, lList **alpp);


int sge_gdi2_send_ack_to_qmaster(sge_gdi_ctx_class_t *ctx, int sync, u_long32 type, u_long32 ulong_val, 
                            u_long32 ulong_val_2, lList **alpp);

lList *gdi2_kill(sge_gdi_ctx_class_t *thiz, lList *id_list, const char *cell, u_long32 option_flags, u_long32 action_flag);
lList *gdi2_tsm( sge_gdi_ctx_class_t *thiz, const char *schedd_name, const char *cell);

bool sge_gdi2_check_permission(sge_gdi_ctx_class_t *ctx, lList **alpp, int option);
bool sge_gdi2_get_mapping_name(sge_gdi_ctx_class_t *ctx, const char *requestedHost, 
                                 char *buf, int buflen);

int gdi2_send_message_pb(sge_gdi_ctx_class_t *ctx, 
                         int synchron, const char *tocomproc, int toid, 
                         const char *tohost, int tag, sge_pack_buffer *pb, 
                         u_long32 *mid);

int gdi2_receive_message(sge_gdi_ctx_class_t *ctx, 
                         char *fromcommproc, u_short *fromid, char *fromhost, 
                         int *tag, char **buffer, u_long32 *buflen, int synchron);

int gdi2_get_configuration(sge_gdi_ctx_class_t *ctx, const char *config_name, 
                           lListElem **gepp, lListElem **lepp );

int gdi2_get_merged_configuration(sge_gdi_ctx_class_t *ctx, 
                                  lList **conf_list);

int gdi2_get_conf_and_daemonize(sge_gdi_ctx_class_t *ctx,
                                tDaemonizeFunc dfunc,
                                lList **conf_list,
                                volatile int* abort_flag);
                                
int sge_gdi2_shutdown(void **context);

#define ASYNC_GDI2
#ifdef ASYNC_GDI2
bool gdi2_receive_multi_async(sge_gdi_ctx_class_t* ctx, sge_gdi_request **answer, lList **malpp, bool is_sync);
bool gdi2_send_multi_async(sge_gdi_ctx_class_t *ctx, lList **alpp, state_gdi_multi *state);
#endif
                                
/* 
** commlib handler functions 
*/                                
#include "comm/lists/cl_list_types.h"

const char* sge_dump_message_tag(unsigned long tag);

typedef enum sge_gdi_stored_com_error_type {
   SGE_COM_ACCESS_DENIED = 101,
   SGE_COM_ENDPOINT_NOT_UNIQUE,
   SGE_COM_WAS_COMMUNICATION_ERROR
} sge_gdi_stored_com_error_t;
bool sge_get_com_error_flag(u_long32 progid, sge_gdi_stored_com_error_t error_type);

void general_communication_error(const cl_application_error_list_elem_t* commlib_error);
int gdi_log_flush_func(cl_raw_list_t* list_p);

#ifdef DEBUG_CLIENT_SUPPORT
void gdi_rmon_print_callback_function(const char *progname,
                                      const char *message,
                                      unsigned long traceid,
                                      unsigned long pid,
                                      unsigned long thread_id);
#endif


#ifdef  __cplusplus
}
#endif


#endif /* TEST_GDI2 */

#endif



