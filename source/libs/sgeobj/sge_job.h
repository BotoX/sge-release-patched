#ifndef __SGE_JOB_H 
#define __SGE_JOB_H 
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

#include "sge_htable.h"
#include "sge_dstring.h"
#include "sge_jobL.h"
#include "sge_job_refL.h"
#include "sge_messageL.h"

/****** sgeobj/job/jb_now *****************************************************
*  NAME
*     jb_now -- macros to handle flag JB_type 
*
*  SYNOPSIS
*
*     JOB_TYPE_IMMEDIATE
*     JOB_TYPE_QSH
*     JOB_TYPE_QLOGIN
*     JOB_TYPE_QRSH
*     JOB_TYPE_QRLOGIN
*        
*     JOB_TYPE_NO_ERROR
*        When a job of this type fails and the error condition usually
*        would result in the job error state the job is finished. Thus
*        no qmod -c "*" is supported.
*******************************************************************************/

#define JOB_TYPE_IMMEDIATE  0x01L
#define JOB_TYPE_QSH        0x02L
#define JOB_TYPE_QLOGIN     0x04L
#define JOB_TYPE_QRSH       0x08L
#define JOB_TYPE_QRLOGIN    0x10L
#define JOB_TYPE_NO_ERROR   0x20L

/* submitted via "qsub -b y" or "qrsh [-b y]" */ 
#define JOB_TYPE_BINARY     0x40L

/* array job (qsub -t ...) */
#define JOB_TYPE_ARRAY      0x80L
/* Do a raw exec (qsub -noshell) */
#define JOB_TYPE_NO_SHELL   0x100L

#define JOB_TYPE_QXXX_MASK \
   (JOB_TYPE_QSH | JOB_TYPE_QLOGIN | JOB_TYPE_QRSH | JOB_TYPE_QRLOGIN | JOB_TYPE_NO_ERROR)

#define JOB_TYPE_STR_IMMEDIATE  "IMMEDIATE"
#define JOB_TYPE_STR_QSH        "INTERACTIVE"
#define JOB_TYPE_STR_QLOGIN     "QLOGIN"
#define JOB_TYPE_STR_QRSH       "QRSH"
#define JOB_TYPE_STR_QRLOGIN    "QRLOGIN"
#define JOB_TYPE_STR_NO_ERROR   "NO_ERROR"

#define JOB_TYPE_CLEAR_IMMEDIATE(jb_now) \
   jb_now = jb_now & ~JOB_TYPE_IMMEDIATE 

#define JOB_TYPE_SET_IMMEDIATE(jb_now) \
   jb_now =  jb_now | JOB_TYPE_IMMEDIATE

#define JOB_TYPE_SET_QSH(jb_now) \
   jb_now = (jb_now & (~JOB_TYPE_QXXX_MASK)) | JOB_TYPE_QSH

#define JOB_TYPE_SET_QLOGIN(jb_now) \
   jb_now = (jb_now & (~JOB_TYPE_QXXX_MASK)) | JOB_TYPE_QLOGIN

#define JOB_TYPE_SET_QRSH(jb_now) \
   jb_now = (jb_now & ~JOB_TYPE_QXXX_MASK) | JOB_TYPE_QRSH

#define JOB_TYPE_SET_QRLOGIN(jb_now) \
   jb_now = (jb_now & ~JOB_TYPE_QXXX_MASK) | JOB_TYPE_QRLOGIN

#define JOB_TYPE_SET_BINARY(jb_now) \
   jb_now = jb_now | JOB_TYPE_BINARY

#define JOB_TYPE_SET_ARRAY(jb_now) \
   jb_now = jb_now | JOB_TYPE_ARRAY

#define JOB_TYPE_CLEAR_NO_ERROR(jb_now) \
   jb_now = jb_now & ~JOB_TYPE_NO_ERROR

#define JOB_TYPE_SET_NO_ERROR(jb_now) \
   jb_now =  jb_now | JOB_TYPE_NO_ERROR

#define JOB_TYPE_SET_NO_SHELL(jb_now) \
   jb_now =  jb_now | JOB_TYPE_NO_SHELL

#define JOB_TYPE_UNSET_BINARY(jb_now) \
   jb_now = jb_now & ~JOB_TYPE_BINARY

#define JOB_TYPE_UNSET_NO_SHELL(jb_now) \
   jb_now =  jb_now & ~JOB_TYPE_NO_SHELL

#define JOB_TYPE_IS_IMMEDIATE(jb_now)      (jb_now & JOB_TYPE_IMMEDIATE)
#define JOB_TYPE_IS_QSH(jb_now)            (jb_now & JOB_TYPE_QSH)
#define JOB_TYPE_IS_QLOGIN(jb_now)         (jb_now & JOB_TYPE_QLOGIN)
#define JOB_TYPE_IS_QRSH(jb_now)           (jb_now & JOB_TYPE_QRSH)
#define JOB_TYPE_IS_QRLOGIN(jb_now)        (jb_now & JOB_TYPE_QRLOGIN)
#define JOB_TYPE_IS_BINARY(jb_now)         (jb_now & JOB_TYPE_BINARY)
#define JOB_TYPE_IS_ARRAY(jb_now)          (jb_now & JOB_TYPE_ARRAY)
#define JOB_TYPE_IS_NO_ERROR(jb_now)       (jb_now & JOB_TYPE_NO_ERROR)
#define JOB_TYPE_IS_NO_SHELL(jb_now)       (jb_now & JOB_TYPE_NO_SHELL)


bool job_is_enrolled(const lListElem *job, 
                     u_long32 ja_task_number);

u_long32 job_get_ja_tasks(const lListElem *job);

u_long32 job_get_not_enrolled_ja_tasks(const lListElem *job);

u_long32 job_get_enrolled_ja_tasks(const lListElem *job);

u_long32 job_get_submit_ja_tasks(const lListElem *job);
 
lListElem *job_enroll(lListElem *job, lList **answer_list, 
                      u_long32 task_number);  

void job_delete_not_enrolled_ja_task(lListElem *job, lList **answer_list,
                                     u_long32 ja_task_number);

int job_count_pending_tasks(lListElem *job, bool count_all);

bool job_has_soft_requests(lListElem *job);

bool job_is_ja_task_defined(const lListElem *job, u_long32 ja_task_number);

void job_set_hold_state(lListElem *job, 
                        lList **answer_list, u_long32 ja_task_id,
                        u_long32 new_hold_state);

u_long32 job_get_hold_state(lListElem *job, u_long32 ja_task_id);

/* int job_add_job(lList **job_list, char *name, lListElem *job, int check,
                 int hash, htable* Job_Hash_Table); */

void job_list_print(lList *job_list);

lListElem *job_get_ja_task_template(const lListElem *job, u_long32 ja_task_id); 

lListElem *job_get_ja_task_template_hold(const lListElem *job,
                                         u_long32 ja_task_id, 
                                         u_long32 hold_state);

lListElem *job_get_ja_task_template_pending(const lListElem *job,
                                            u_long32 ja_task_id);

lListElem *job_search_task(const lListElem *job, lList **answer_list, u_long32 ja_task_id);
lListElem *job_create_task(lListElem *job, lList **answer_list, u_long32 ja_task_id);

void job_add_as_zombie(lListElem *zombie, lList **answer_list, 
                       u_long32 ja_task_id);

int job_list_add_job(lList **job_list, const char *name, lListElem *job, 
                     int check); 

u_long32 job_get_ja_task_hold_state(const lListElem *job, u_long32 ja_task_id);

void job_destroy_hold_id_lists(const lListElem *job, lList *id_list[8]);

void job_create_hold_id_lists(const lListElem *job, lList *id_list[8],
                              u_long32 hold_state[8]);

bool job_is_zombie_job(const lListElem *job); 

const char *job_get_shell_start_mode(const lListElem *job,
                                     const lListElem *queue,
                                     const char *conf_shell_start_mode);

bool job_is_array(const lListElem *job); 

bool job_is_parallel(const lListElem *job);

bool job_is_tight_parallel(const lListElem *job, const lList *pe_list);

bool job_might_be_tight_parallel(const lListElem *job, const lList *pe_list);

void job_get_submit_task_ids(const lListElem *job, u_long32 *start, 
                             u_long32 *end, u_long32 *step); 

int job_set_submit_task_ids(lListElem *job, u_long32 start, u_long32 end,
                            u_long32 step);

u_long32 job_get_smallest_unenrolled_task_id(const lListElem *job);

u_long32 job_get_smallest_enrolled_task_id(const lListElem *job);

u_long32 job_get_biggest_unenrolled_task_id(const lListElem *job);

u_long32 job_get_biggest_enrolled_task_id(const lListElem *job);

int job_list_register_new_job(const lList *job_list, u_long32 max_jobs,
                              int force_registration);   

void jatask_list_print_to_string(const lList *task_list, dstring *range_string);

lList* ja_task_list_split_group(lList **task_list);

int job_initialize_id_lists(lListElem *job, lList **answer_list);

void job_initialize_env(lListElem *job, 
                        lList **answer_list,
                        const lList* path_alias_list,
                        const char *unqualified_hostname,
                        const char *qualified_hostname);

const char* job_get_env_string(const lListElem *job, const char *variable);

void job_set_env_string(lListElem *job, const char *variable, 
                        const char *value);

void job_check_correct_id_sublists(lListElem *job, lList **answer_list);

const char *job_get_id_string(u_long32 job_id, u_long32 ja_task_id, 
                              const char *pe_task_id);

const char *job_get_job_key(u_long32 job_id, dstring *buffer);

const char *job_get_key(u_long32 job_id, u_long32 ja_task_id, 
                        const char *pe_task_id, dstring *buffer);
bool job_parse_key(char *key, u_long32 *job_id, u_long32 *ja_task_id,
                   char **pe_task_id, bool *only_job);

bool job_is_pe_referenced(const lListElem *job, const lListElem *pe);

bool job_is_ckpt_referenced(const lListElem *job, const lListElem *ckpt);

void job_get_state_string(char *str, u_long32 op);

lListElem *job_list_locate(lList *job_list, u_long32 job_id);

void job_add_parent_id_to_context(lListElem *job);

int job_check_qsh_display(const lListElem *job, 
                          lList **answer_list, 
                          bool output_warning);

int job_check_owner(const char *user_name, u_long32 job_id, lList *master_job_list);

int job_resolve_host_for_path_list(const lListElem *job, lList **answer_list, int name);

lListElem *
job_get_request(const lListElem *this_elem, const char *centry_name);

bool
job_get_contribution(const lListElem *this_elem, lList **answer_list,
                     const char *name, double *value,
                     const lListElem *implicit_centry);

void queue_or_job_get_states(int nm, char *str, u_long32 op);


/* unparse functions */
bool sge_unparse_string_option_dstring(dstring *category_str, const lListElem *job_elem, 
                               int nm, char *option);

bool sge_unparse_ulong_option_dstring(dstring *category_str, const lListElem *job_elem, 
                               int nm, char *option);
                               
bool sge_unparse_pe_dstring(dstring *category_str, const lListElem *job_elem, int pe_pos, int range_pos,
                            const char *option); 

bool sge_unparse_resource_list_dstring(dstring *category_str, lListElem *job_elem, 
                                       int nm, const char *option);

bool sge_unparse_queue_list_dstring(dstring *category_str, lListElem *job_elem, 
                                    int nm, const char *option);   

bool sge_unparse_acl_dstring(dstring *category_str, const char *owner, const char *group, 
                             const lList *acl_list, const char *option);

bool job_verify(const lListElem *job, lList **answer_list);
bool job_verify_submitted_job(const lListElem *job, lList **answer_list);
bool job_verify_execd_job(const lListElem *job, lList **answer_list);

bool job_get_wallclock_limit(u_long32 *limit, const lListElem *jep);

#endif /* __SGE_JOB_H */    
