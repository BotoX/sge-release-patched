#ifndef __SGE_JOB_QMASTER_H
#define __SGE_JOB_QMASTER_H
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

#ifndef __SGE_GDIP_H
#   include "sge_gdiP.h"
#endif

#include "sge_eventL.h"
#include "sge_qmaster_timed_event.h"
#include "uti/sge_monitor.h"

int sge_gdi_add_job(void *context, lListElem *jepp, lList **alpp, lList **lpp, char *ruser, char *rhost, 
                    uid_t uid, gid_t gid, char *group, sge_gdi_request *request, 
                     monitoring_t *monitor);
                    
int sge_gdi_copy_job(void *context, lListElem *jep, lList **alpp, lList **lpp, char *ruser, char *rhost, 
                     uid_t uid, gid_t gid, char *group, sge_gdi_request *request, 
                     monitoring_t *monitor);

int sge_gdi_mod_job(void *context, lListElem *jep, lList **alpp, char *ruser, char *rhost, int sub_command);

int sge_gdi_del_job(void *context, lListElem *jep, lList **alpp, char *ruser, char *rhost, int sub_command, monitoring_t *monitor);

void sge_add_job_event(ev_event type, lListElem *jep, lListElem *jatep);

bool job_has_valid_account_string(const char *name, lList **answer_list);

bool is_pe_master_task_send(lListElem *jatep); 

void sge_add_jatask_event(ev_event type, lListElem *jep, lListElem *jatask);

void job_suc_pre(lListElem *jep);

void sge_init_job_number(void); 
void sge_store_job_number(void *context, te_event_t anEvent, monitoring_t *monitor);

void job_ja_task_send_abort_mail(const lListElem *job, const lListElem *ja_task, const char *ruser,
                                 const char *rhost,  const char *err_str);

void get_rid_of_job_due_to_qdel(void *context,
                                lListElem *j, lListElem *t, lList **answer_list, const char *ruser,
                                int force, monitoring_t *monitor);

void job_mark_job_as_deleted(void *context, lListElem *j, lListElem *t);

void sge_job_spool(void *context);

#endif /* __SGE_JOB_QMASTER_H */
