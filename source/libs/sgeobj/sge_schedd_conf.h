#ifndef __SGE_SCHEDD_CONF_H
#define __SGE_SCHEDD_CONF_H
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

#include "sge_schedd_confL.h"

enum { 
    DISPATCH_TYPE_NONE = 0, 
    DISPATCH_TYPE_FAST, 
    DISPATCH_TYPE_COMPREHENSIVE 
};

enum schedd_job_info_key {
   SCHEDD_JOB_INFO_FALSE=0,
   SCHEDD_JOB_INFO_TRUE,
   SCHEDD_JOB_INFO_JOB_LIST,
   SCHEDD_JOB_INFO_UNDEF
};

typedef enum {
   FIRST_POLICY_VALUE,
   INVALID_POLICY = FIRST_POLICY_VALUE,

   OVERRIDE_POLICY,
   FUNCTIONAL_POLICY,
   SHARE_TREE_POLICY,
/*removed   DEADLINE_POLICY,*/

   /* TODO: shouldn't LAST_POLICY_VALUE equal SHARE_TREE_POLICY? 
    * POLICY_VALUES = 4, should probably be 3
    */
   LAST_POLICY_VALUE,
   POLICY_VALUES = (LAST_POLICY_VALUE - FIRST_POLICY_VALUE)
} policy_type_t;

typedef struct {
   policy_type_t policy;
   int dependent;
} policy_hierarchy_t;  

void sconf_ph_fill_array(policy_hierarchy_t array[]);

void sconf_ph_print_array(policy_hierarchy_t array[]);

void sconf_print_config(void);

lListElem *sconf_create_default(void);

bool sconf_set_config(lList **config, lList **answer_list);

bool sconf_is_valid_load_formula_( lList **answer_list,
                                       lList *cmplx_list);

bool sconf_is_valid_load_formula(lListElem *sc_ep,
                                       lList **answer_list,
                                       lList *cmplx_list);

bool
sconf_is_centry_referenced(const lListElem *this_elem, const lListElem *centry);

bool sconf_validate_config(lList **answer_list, lList *config);

bool sconf_validate_config_(lList **answer_list);

const lListElem *sconf_get_config(void);

lList **sconf_get_config_list(void);

bool sconf_is_new_config(void); 
void sconf_reset_new_config(void);

bool sconf_is(void);

u_long32 sconf_get_load_adjustment_decay_time(void);

const lList *sconf_get_job_load_adjustments(void);

const char *sconf_get_schedule_interval_str(void);

const char *sconf_get_load_formula(void);

const char *sconf_get_load_adjustment_decay_time_str(void);

const char *sconf_reprioritize_interval_str(void);

const char *sconf_get_param(const char *name);

u_long32 sconf_get_queue_sort_method(void);

u_long32 sconf_get_maxujobs(void);

u_long32 sconf_get_schedule_interval(void);

u_long32 sconf_get_reprioritize_interval(void);

u_long32 sconf_get_schedd_job_info(void);

u_long32 sconf_get_weight_tickets_share(void);

void sconf_disable_schedd_job_info(void);

void sconf_enable_schedd_job_info(void);

const lList *sconf_get_schedd_job_info_range(void);

const char *sconf_get_algorithm(void);

const lList *sconf_get_usage_weight_list(void);

double sconf_get_weight_user(void);

double sconf_get_weight_department(void);

double sconf_get_weight_project(void);

double sconf_get_weight_job(void);

u_long32 sconf_get_weight_tickets_share(void);

u_long32 sconf_get_weight_tickets_functional(void);

u_long32 sconf_get_halftime(void);

void sconf_set_weight_tickets_override(u_long32 active);

double sconf_get_compensation_factor(void);

bool sconf_get_share_override_tickets(void);

bool sconf_get_share_functional_shares(void);

bool sconf_get_report_pjob_tickets(void);

bool sconf_is_job_category_filtering(void);

u_long32 sconf_get_flush_submit_sec(void);
   
u_long32 sconf_get_flush_finish_sec(void);
   
u_long32 sconf_get_max_functional_jobs_to_schedule(void);

u_long32 sconf_get_max_pending_tasks_per_job(void);

const char *sconf_get_halflife_decay_list_str(void);

const lList* sconf_get_halflife_decay_list(void);

double sconf_get_weight_ticket(void);
double sconf_get_weight_waiting_time(void);
double sconf_get_weight_deadline(void);
double sconf_get_weight_urgency(void);

u_long32 sconf_get_max_reservations(void);

typedef enum {
   QS_STATE_EMPTY,
   QS_STATE_FULL
} qs_state_t;

void sconf_set_qs_state(qs_state_t state);
qs_state_t sconf_get_qs_state(void);

void sconf_set_global_load_correction(bool flag);
bool sconf_get_global_load_correction(void);

u_long32 sconf_get_default_duration(void);
const char *sconf_get_default_duration_str(void);

void     sconf_set_now(u_long32);
u_long32 sconf_get_now(void);

bool sconf_get_host_order_changed(void);
void sconf_set_host_order_changed(bool changed);

int  sconf_get_last_dispatch_type(void);
void sconf_set_last_dispatch_type(int changed);

u_long32  sconf_get_duration_offset(void);

bool serf_get_active(void);
void serf_set_active(bool);

double sconf_get_weight_priority(void);

bool sconf_get_profiling(void);

#endif /* __SGE_SCHEDD_CONF_H */
