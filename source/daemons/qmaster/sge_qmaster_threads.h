#ifndef _SGE_QMASTER_THREADS_H_
#define _SGE_QMASTER_THREADS_H_
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
 *   Copyright: 2003 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "gdi/sge_gdi_request.h"
#include "gdi/sge_gdi_ctx.h"

void sge_gdi_kill_master(char *host, sge_gdi_request *request, sge_gdi_request *answer);

/* thread management */
void sge_create_and_join_threads(sge_gdi_ctx_class_t *ctx);

/* misc functions */
bool sge_daemonize_qmaster(void);
void sge_become_admin_user(const char *admin_user);
void sge_exit_func(void **ctx_ref, int);
void sge_start_heartbeat(void);
void sge_start_periodic_tasks(void);
void sge_qmaster_shutdown(sge_gdi_ctx_class_t *ctx, bool do_spool);
void sge_register_event_handler(void); 
int sge_get_qmaster_exit_state(void);

#endif /* _SGE_QMASTER_THREADS_H_ */

