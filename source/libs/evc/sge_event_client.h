#ifndef __SGE_C_EVENT_H
#define __SGE_C_EVENT_H
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

#include "sge_gdi.h"

#include "sge_eventL.h"

#define DEFAULT_EVENT_DELIVERY_INTERVAL (10)

bool ec_prepare_registration(ev_registration_id id, const char *name);
bool ec_register(bool exit_on_qmaster_down, lList **alpp);
bool ec_deregister(void);
bool ec_is_initialized(void);

bool ec_subscribe(ev_event event);
bool ec_subscribe_all(void);

bool ec_unsubscribe(ev_event event);
bool ec_unsubscribe_all(void);

int ec_get_flush(ev_event event);
bool ec_set_flush(ev_event event, bool flush, int interval);
bool ec_unset_flush(ev_event event);

bool ec_subscribe_flush(ev_event event, int flush);

bool ec_mod_subscription_where(ev_event event, const lListElem *what, const lListElem *where);

int ec_set_edtime(int intval);
int ec_get_edtime(void);

bool ec_set_busy_handling(ev_busy_handling handling);
ev_busy_handling ec_get_busy_handling(void);

bool ec_set_flush_delay(int flush_delay);
int ec_get_flush_delay(void);

bool ec_set_busy(int busy);
bool ec_get_busy(void);

bool ec_set_session(const char *session);
const char *ec_get_session(void);

ev_registration_id ec_get_id(void);

void ec_set_clientdata(u_long32 data);
u_long32 ec_get_clientdata(void);

bool ec_commit(void);
bool ec_commit_multi(lList **malp, state_gdi_multi *state);

bool ec_get(lList **, bool exit_on_qmaster_down);

void ec_mark4registration(void);
bool ec_need_new_registration(void);

#endif /* __SGE_C_EVENT_H */

