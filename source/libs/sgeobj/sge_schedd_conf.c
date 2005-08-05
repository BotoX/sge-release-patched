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

#include "sgeobj/sge_object.h"
#include "sge.h"
#include "sgermon.h"
#include "sge_log.h"
#include "cull.h"
#include "lck/sge_lock.h"
#include "sge_stdio.h"
#include "sge_stdlib.h"
#include "sge_string.h"
#include "sge_answer.h"
#include "sge_centry.h"
#include "sge_feature.h"
#include "sge_usage.h"
#include "sge_range.h"

#include "sge_schedd_conf.h"
#include "msg_schedd.h"

#include "cull_parse_util.h"

#include "sge_parse_num_par.h"

#include "msg_sgeobjlib.h"
#include "msg_common.h"


/******************************************************
 *
 * All configuration values are stored in one cull
 * master list: Master_Sched_Config_List. This list
 * has only one element: the config element (SC_Type).
 *
 * The configuratin is stored in a CULL list due to the
 * current configuration handling in the system. There
 * are methods outside this module, which need complete
 * access to the list. Therefore exist two methods to 
 * access the whole configuration:
 *
 * - sconf_get_config_list 
 *     (returns a pointer to the cull list 
 *
 * - sconf_get_config 
 *      returns a const pointer the config object.
 *
 * Both methods should only be used, when there is no
 * other way. Usualy the values in the configuration
 * should be accessed via access-function. The module
 * stores the position for each config element for
 * fast access, validates the configuration, when it
 * is changed, and pre-computes some values to return
 * them in the right way.
 *
 * If the configuration is changed directly in the CULL
 * list without using 
 *                "sconf_set_config"
 * , the 
 *               "sconf_validate_config" 
 * needs to be called to ensure that the internal data 
 * is updated.
 *
 * The access is not thread save. The current implementation
 * of the configuration does not allow an easy way to
 * make it thread save. But if only access functions
 * are used it should be easy to make this module thread
 * save, when the configuration is made thread save.
 *
 ******************************************************/



/* default values for scheduler configuration 
 * not all defaults are defined here :-)
 */
#define DEFAULT_LOAD_ADJUSTMENTS_DECAY_TIME "0:7:30"
#define _DEFAULT_LOAD_ADJUSTMENTS_DECAY_TIME 7*60+30
#define DEFAULT_LOAD_FORMULA                "np_load_avg"
#define SCHEDULE_TIME                       "0:0:15"
#define _SCHEDULE_TIME                      15
#define REPRIORITIZE_INTERVAL               "0:0:0"
#define REPRIORITIZE_INTERVAL_I             0
#define MAXUJOBS                            0
#define MAXGJOBS                            0
#define SCHEDD_JOB_INFO                     "true"
#define DEFAULT_DURATION                    "0:10:00" /* the default_duration and default_duration_I have to be */
#define DEFAULT_DURATION_I                  600       /* in sync. On is the strin version of the other (based on seconds)*/
#define DEFAULT_DURATION_OFFSET             60

/**
 * multithreading support, thread local
 **/

static pthread_key_t   sc_state_key;  

typedef struct {
   qs_state_t queue_state;
   bool       global_load_correction;
   u_long32   now;
   int        schedd_job_info;
   bool       host_order_changed;
   int        last_dispatch_type;
}  sc_state_t; 

static void sc_state_init(sc_state_t* state) 
{
   state->queue_state = QS_STATE_FULL;
   state->global_load_correction = true;
   state->now = 0;
   state->schedd_job_info = SCHEDD_JOB_INFO_FALSE;
   state->host_order_changed = true;
   state->last_dispatch_type = 0;
}

static void sc_state_destroy(void* state) 
{
   free(state);
}

void sc_init_mt(void) 
{
   pthread_key_create(&sc_state_key, &sc_state_destroy);
} 

/*-----*/
/* end */
/*-----*/

/* 
 * addes a parameter to the config_pos.params list and evaluates the settings
 *
 * Parameters:
 * - lList *param_list : target list
 * - lList ** answer_list : error messages
 * - const char* param :  the character version of the paramter
 *
 * Return:
 * - bool : true, when everything was fine, otherwise false
 *
 * See:
 * - sconf_eval_set_profiling
 */
typedef bool (*setParam)(lList *param_list, lList **answer_list, const char* param);

/**
 * specifies an array of valid parameters and its validation functions
 */
typedef struct {
      const char* name;
      setParam setParam; 
}parameters_t;

/**
 * stores the positions of all structure elemens and some
 * precalculated settings.
 */
typedef struct{
   bool empty;          /* marks this structure as empty or set */
   
   int algorithm;       /* pos settings */
   int schedule_interval;
   int maxujobs;
   int queue_sort_method;
   int job_load_adjustments;
   int load_adjustment_decay_time;
   int load_formula;
   int schedd_job_info;
   int flush_submit_sec;
   int flush_finish_sec;
   int params;
   
   int reprioritize_interval;  
   int halftime;
   int usage_weight_list;
   int compensation_factor;
   int weight_user;
   int weight_project;
   int weight_department;
   int weight_job;
   int weight_tickets_functional;
   int weight_tickets_share;

   int weight_tickets_override;
   int share_override_tickets;
   int share_functional_shares;
   int max_functional_jobs_to_schedule;
   int report_pjob_tickets;
   int max_pending_tasks_per_job;
   int halflife_decay_list; 
   int policy_hierarchy;

   int weight_ticket;
   int weight_waiting_time;
   int weight_deadline;
   int weight_urgency;
   int max_reservation;
   int weight_priority;
   int default_duration;

   int c_is_schedd_job_info;       /* cached configuration */
   u_long32 s_duration_offset;
   lList *c_schedd_job_info_range;
   lList *c_halflife_decay_list;   
   lList *c_params;
   u_long32 c_default_duration;

   bool new_config;     /* identifies an update in the configuration */
}config_pos_type;

static bool schedd_profiling = false;
static bool is_category_job_filtering = false;
static bool current_serf_do_monitoring = false;

static bool calc_pos(void);

static void sconf_clear_pos(void);

static bool is_config_set(void);
static const char * get_algorithm(void);
static const char *get_schedule_interval_str(void);
static const char *get_load_adjustment_decay_time_str(void);
static const char *reprioritize_interval_str(void);
static const char *get_halflife_decay_list_str(void);
static const char *get_default_duration_str(void);
static const lList *get_usage_weight_list(void);
static const lList *get_job_load_adjustments(void); 
static const char* get_load_formula(void); 

static bool 
sconf_eval_set_profiling(lList *param_list, lList **answer_list, const char* param); 

static bool 
sconf_eval_set_monitoring(lList *param_list, lList **answer_list, const char* param);

static bool 
sconf_eval_set_job_category_filtering(lList *param_list, lList **answer_list, 
                                      const char* param);
static bool 
sconf_eval_set_duration_offset(lList *param_list, lList **answer_list, const char* param);                                      
static char policy_hierarchy_enum2char(policy_type_t value);

static policy_type_t policy_hierarchy_char2enum(char character);

static int policy_hierarchy_verify_value(const char* value);

/* array structure. pre-init. Make sure that a default value is added
 * when the config_pos_type is edited
 */
static config_pos_type pos = {true, 
                       -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                       -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                       -1, -1, -1, -1, -1, -1, -1, -1, -1,
                       -1, -1, -1, -1, -1, -1, 
                       SCHEDD_JOB_INFO_UNDEF, 0, NULL, NULL, NULL, MAX_ULONG32, 

                       false};

/*
 * a list of all valid "params" parameters
 *
 * The implementation has a problem. If an entry is removed, its setting is not
 * changed, but it should be turned off. This means we have to turn everything off,
 * before we work on the params 
 */
const parameters_t params[] = {
   {"PROFILE",         sconf_eval_set_profiling},
   {"MONITOR",         sconf_eval_set_monitoring},
   {"JC_FILTER",       sconf_eval_set_job_category_filtering},
   {"DURATION_OFFSET", sconf_eval_set_duration_offset},
   {"NONE",            NULL},
   {NULL,              NULL}
};

/* stores the overall configuraion */
lList *Master_Sched_Config_List = NULL;

const char *const policy_hierarchy_chars = "OFS";

/* SG: TODO: should be const */
int load_adjustment_fields[] = { CE_name, CE_stringval, 0 };
/* SG: TODO: should be const */
int usage_fields[] = { UA_name, UA_value, 0 };
const char *delis[] = {"=", ",", ""};

/****** sge_schedd_conf/clear_pos() ********************************************
*  NAME
*     clear_pos() -- resets the position information 
*
*  SYNOPSIS
*     static void clear_pos(void) 
*
*  FUNCTION
*     is needed, when a new configuration is set 
*
* MT-NOTE: is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(write)
*
*******************************************************************************/
static void sconf_clear_pos(void){

/* set config empty */
         pos.empty = true;

/* reset positions */
         pos.algorithm = -1; 
         pos.schedule_interval = -1; 
         pos.maxujobs =  -1;
         pos.queue_sort_method = -1; 
         pos.job_load_adjustments = -1; 
         pos.load_formula =  -1;
         pos.schedd_job_info = -1; 
         pos.flush_submit_sec = -1; 
         pos.flush_finish_sec =  -1;
         pos.params = -1;
         
         pos.reprioritize_interval= -1; 
         pos.halftime = -1; 
         pos.usage_weight_list = -1; 
         pos.compensation_factor = -1; 
         pos.weight_user = -1; 
         pos.weight_project = -1; 
         pos.weight_department = -1; 
         pos.weight_job = -1; 
         pos.weight_tickets_functional = -1; 
         pos.weight_tickets_share = -1; 
         pos.weight_tickets_override = -1; 
         pos.share_override_tickets = -1; 
         pos.share_functional_shares = -1; 
         pos.max_functional_jobs_to_schedule = -1; 
         pos.report_pjob_tickets = -1; 
         pos.max_pending_tasks_per_job =  -1;
         pos.halflife_decay_list = -1; 
         pos.policy_hierarchy = -1;

         pos.weight_ticket = -1;
         pos.weight_waiting_time = -1;
         pos.weight_deadline = -1;
         pos.weight_urgency = -1;
         pos.max_reservation = -1;
         pos.weight_priority = -1;
         pos.default_duration = -1;

/* reseting cached values */
         pos.c_is_schedd_job_info = SCHEDD_JOB_INFO_UNDEF;
         lFreeList(&(pos.c_schedd_job_info_range));
         lFreeList(&(pos.c_halflife_decay_list));
         lFreeList(&(pos.c_params));
         pos.c_default_duration = DEFAULT_DURATION_I;

}

/****** sge_schedd_conf/calc_pos() ********************************************
*  NAME
*     calc_pos() -- 
*
*  SYNOPSIS
*     static void calc_pos(void) 
*
*  FUNCTION
*
* MT-NOTE: is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(write)
*
*******************************************************************************/
static bool calc_pos(void)
{
   bool ret = true;

   DENTER(TOP_LAYER, "calc_pos");

   if (pos.empty) {
      const lListElem *config = lFirst(Master_Sched_Config_List);

      if (config) {
         pos.empty = false;

         ret &= (pos.algorithm = lGetPosViaElem(config, SC_algorithm )) != -1; 
         ret &= (pos.schedule_interval = lGetPosViaElem(config, SC_schedule_interval)) != -1; 
         ret &= (pos.maxujobs = lGetPosViaElem(config, SC_maxujobs)) != -1;
         ret &= (pos.queue_sort_method = lGetPosViaElem(config, SC_queue_sort_method)) != -1;

         ret &= (pos.job_load_adjustments = lGetPosViaElem(config,SC_job_load_adjustments )) != -1;
         ret &= (pos.load_adjustment_decay_time = lGetPosViaElem(config, SC_load_adjustment_decay_time)) != -1;
         ret &= (pos.load_formula = lGetPosViaElem(config, SC_load_formula)) != -1;
         ret &= (pos.schedd_job_info = lGetPosViaElem(config, SC_schedd_job_info)) != -1;
         ret &= (pos.flush_submit_sec = lGetPosViaElem(config, SC_flush_submit_sec)) != -1;
         ret &= (pos.flush_finish_sec = lGetPosViaElem(config, SC_flush_finish_sec)) != -1;
         ret &= (pos.params = lGetPosViaElem(config, SC_params)) != -1;

         ret &= (pos.reprioritize_interval = lGetPosViaElem(config, SC_reprioritize_interval)) != -1;
         ret &= (pos.halftime = lGetPosViaElem(config, SC_halftime)) != -1;
         ret &= (pos.usage_weight_list = lGetPosViaElem(config, SC_usage_weight_list)) != -1;

         ret &= (pos.compensation_factor = lGetPosViaElem(config, SC_compensation_factor)) != -1;
         ret &= (pos.weight_user = lGetPosViaElem(config, SC_weight_user)) != -1;
         ret &= (pos.weight_project = lGetPosViaElem(config, SC_weight_project)) != -1;
         ret &= (pos.weight_department = lGetPosViaElem(config, SC_weight_department)) != -1;
         ret &= (pos.weight_job = lGetPosViaElem(config, SC_weight_job)) != -1;

         ret &= (pos.weight_tickets_functional = lGetPosViaElem(config, SC_weight_tickets_functional)) != -1;
         ret &= (pos.weight_tickets_share = lGetPosViaElem(config, SC_weight_tickets_share)) != -1;
         ret &= (pos.weight_tickets_override = lGetPosViaElem(config, SC_weight_tickets_override)) != -1;

         ret &= (pos.share_override_tickets = lGetPosViaElem(config, SC_share_override_tickets)) != -1;
         ret &= (pos.share_functional_shares = lGetPosViaElem(config, SC_share_functional_shares)) != -1;
         ret &= (pos.max_functional_jobs_to_schedule = lGetPosViaElem(config, SC_max_functional_jobs_to_schedule)) != -1;
         ret &= (pos.report_pjob_tickets = lGetPosViaElem(config, SC_report_pjob_tickets)) != -1;
         ret &= (pos.max_pending_tasks_per_job = lGetPosViaElem(config, SC_max_pending_tasks_per_job)) != -1;
         ret &= (pos.halflife_decay_list = lGetPosViaElem(config, SC_halflife_decay_list)) != -1;
         ret &= (pos.policy_hierarchy = lGetPosViaElem(config, SC_policy_hierarchy)) != -1;

         ret &= (pos.weight_ticket = lGetPosViaElem(config, SC_weight_ticket)) != -1;
         ret &= (pos.weight_waiting_time = lGetPosViaElem(config, SC_weight_waiting_time)) != -1;
         ret &= (pos.weight_deadline = lGetPosViaElem(config, SC_weight_deadline)) != -1;
         ret &= (pos.weight_urgency = lGetPosViaElem(config, SC_weight_urgency)) != -1;
         ret &= (pos.weight_priority = lGetPosViaElem(config, SC_weight_priority)) != -1;
         ret &= (pos.max_reservation = lGetPosViaElem(config, SC_max_reservation)) != -1;
         ret &= (pos.default_duration = lGetPosViaElem(config, SC_default_duration)) != -1;
      }
      else {
         ret = false;
      }   
   }

   DRETURN(ret);
}

/****** sge_schedd_conf/schedd_conf_set_config() *******************************
*  NAME
*     schedd_conf_set_config() -- overwrites the existing configuration 
*
*  SYNOPSIS
*     bool schedd_conf_set_config(lList **config, lList **answer_list) 
*
*  FUNCTION
*    - validates the new configuration 
*    - precalculates some values and caches them
*    - stores the position of each attribute in the structure
*    - and sets the new configuration, if the validation worked
*
*    If the new configuration is a NULL pointer, the current configuration
*    if deleted.
*
*  INPUTS
*     lList **config      - new configuration (SC_Type)
*     lList **answer_list - error messages 
*
*  RESULT
*     bool - true, if it worked
*
*  MT-NOTE: is MT-safe, uses LOCK_SCHED_CONF(write)
*
*
*  SG TODO: use a internal eval config function and not the external one.
*
*******************************************************************************/
bool sconf_set_config(lList **config, lList **answer_list)
{
   lList *store = NULL;
   bool ret = true;

   DENTER(TOP_LAYER,"sconf_set_config"); 

   SGE_LOCK(LOCK_SCHED_CONF, LOCK_WRITE);
   
   store = Master_Sched_Config_List;
   
   if (config) {
      Master_Sched_Config_List = *config;

      SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_WRITE);
      ret = sconf_validate_config_(answer_list);
      SGE_LOCK(LOCK_SCHED_CONF, LOCK_WRITE);
      
      if (ret) {
         lFreeList(&store);
         *config = NULL;
      }
      else {
         Master_Sched_Config_List = store;
         if (!Master_Sched_Config_List){
            SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_USE_DEFAULT_CONFIG)); 
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING);
 
            Master_Sched_Config_List = lCreateList("schedd config list", SC_Type);
            lAppendElem(Master_Sched_Config_List, sconf_create_default());

         }
         SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_WRITE);
         sconf_validate_config_(NULL);
         SGE_LOCK(LOCK_SCHED_CONF, LOCK_WRITE);
      }   
   }
   else {
      lFreeList(&Master_Sched_Config_List);
      sconf_clear_pos();
   }
  
   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_WRITE);
   DRETURN(ret);
}

/****** sge_schedd_conf/sconf_is_valid_load_formula_() *******************
*  NAME
*     sconf_is_valid_load_formula_() -- ??? 
*
*  SYNOPSIS
*     bool sconf_is_valid_load_formula_(lList **answer_list, lList 
*     *centry_list) 
*
*  INPUTS
*     lList **answer_list - ??? 
*     lList *centry_list  - ??? 
*
*  RESULT
*     bool - 
*
*  MT-NOTE:  is MT safe, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
bool sconf_is_valid_load_formula_(lList **answer_list,
                                  lList *centry_list)
{
   bool is_valid = false;

   DENTER(TOP_LAYER, "sconf_is_valid_load_formula");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   is_valid = sconf_is_valid_load_formula(lFirst(Master_Sched_Config_List),
                                      answer_list, centry_list);

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(is_valid);
}
/****** sge_schedd_conf/sconf_is_valid_load_formula() ********************
*  NAME
*     sconf_is_valid_load_formula() -- ??? 
*
*  SYNOPSIS
*     bool sconf_is_valid_load_formula(lListElem *schedd_conf, lList 
*     **answer_list, lList *centry_list) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     lListElem *schedd_conf - ??? 
*     lList **answer_list    - ??? 
*     lList *centry_list     - ??? 
*
*  RESULT
*     bool - 
*
* MT-NOTE: is MT-safe, works only on the passed in data
*
*******************************************************************************/
bool sconf_is_valid_load_formula(lListElem *schedd_conf,
                                       lList **answer_list,
                                       lList *centry_list)
{
   const char *load_formula = NULL;
   bool ret = true;
   char *new_load_formula = NULL;
   
   DENTER(TOP_LAYER, "sconf_is_valid_load_formula");

   /* Modify input */
   load_formula = lGetString(schedd_conf, SC_load_formula);
   new_load_formula = sge_strdup(new_load_formula, load_formula);
   sge_strip_blanks(new_load_formula);
   lSetString(schedd_conf, SC_load_formula, new_load_formula);
   sge_free(new_load_formula);
   
   load_formula = lGetString(schedd_conf, SC_load_formula);

   /* Check for keyword 'none' */
   if (ret == true) {
      if (!strcasecmp(load_formula, "none")) {
         answer_list_add(answer_list, MSG_NONE_NOT_ALLOWED, STATUS_ESYNTAX, 
                         ANSWER_QUALITY_ERROR);
         ret = false;
      }
   }

   /* Check complex attributes and type */
   if (ret == true) {
      const char *term_delim = "+-";
      const char *term, *next_term;
      struct saved_vars_s *term_context = NULL;

      next_term = sge_strtok_r(load_formula, term_delim, &term_context);
      while ((term = next_term) && ret == true) {
         const char *fact_delim = "*";
         const char *fact, *next_fact, *end;
         lListElem *cmplx_attr = NULL;
         struct saved_vars_s *fact_context = NULL;
         
         next_term = sge_strtok_r(NULL, term_delim, &term_context);

         fact = sge_strtok_r(term, fact_delim, &fact_context);
         next_fact = sge_strtok_r(NULL, fact_delim, &fact_context);
         end = sge_strtok_r(NULL, fact_delim, &fact_context);

         /* first factor has to be a complex attr */
         if (fact != NULL) {
            cmplx_attr = centry_list_locate(centry_list, fact);

            if (cmplx_attr != NULL) {
               int type = lGetUlong(cmplx_attr, CE_valtype);

               if (type == TYPE_STR || type == TYPE_CSTR || type == TYPE_HOST) {
                  SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_WRONGTYPE_ATTRIBUTE_S, 
                                         fact));
                  answer_list_add(answer_list, SGE_EVENT, 
                                  STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                  ret = false;
               }
            } 
            else {
               SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_NOTEXISTING_ATTRIBUTE_S, 
                              fact));
               answer_list_add(answer_list, SGE_EVENT, 
                               STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
               ret = false;
            }
         }
         /* is weighting factor a number? */
         if (next_fact != NULL) {
            if (!sge_str_is_number(next_fact)) {
               SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_WEIGHTFACTNONUMB_S, 
                              next_fact));
               answer_list_add(answer_list, SGE_EVENT, 
                               STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
               ret = false;
            }
         }

         /* multiple weighting factors? */
         if (end != NULL) {
            SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_MULTIPLEWEIGHTFACT));
            answer_list_add(answer_list, SGE_EVENT, 
                            STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            ret = false;
         }
         sge_free_saved_vars(fact_context);
      }
      sge_free_saved_vars(term_context);
   }
   
   DRETURN(ret);
}


/****** sge_schedd_conf/sconf_create_default() ***************************
*  NAME
*     sconf_create_default() -- returns a default configuration 
*
*  SYNOPSIS
*     lListElem* sconf_create_default() 
*
*  FUNCTION
*     Creates a default configuration, but does not change the current
*     active configuration. A set config has to be used to make the
*     current configuration the active one.
*
*  RESULT
*     lListElem* - default configuration (SC_Type)
*
* MT-NOTE: is MT-safe, does not use global variables
*
*******************************************************************************/
lListElem *sconf_create_default()
{
   lListElem *ep, *added;

   DENTER(TOP_LAYER, "sconf_create_default");

   ep = lCreateElem(SC_Type);

   lSetString(ep, SC_algorithm, "default");
   lSetString(ep, SC_schedule_interval, SCHEDULE_TIME);
   lSetUlong(ep, SC_maxujobs, MAXUJOBS);

   lSetUlong(ep, SC_queue_sort_method, QSM_LOAD);

   added = lAddSubStr(ep, CE_name, "np_load_avg", SC_job_load_adjustments, CE_Type);
   lSetString(added, CE_stringval, "0.50");

   lSetString(ep, SC_load_adjustment_decay_time, 
                     DEFAULT_LOAD_ADJUSTMENTS_DECAY_TIME);
   lSetString(ep, SC_load_formula, DEFAULT_LOAD_FORMULA);
   lSetString(ep, SC_schedd_job_info, SCHEDD_JOB_INFO);
   lSetUlong(ep, SC_flush_submit_sec, 0);
   lSetUlong(ep, SC_flush_finish_sec, 0);
   lSetString(ep, SC_params, "none");
   
   lSetString(ep, SC_reprioritize_interval, REPRIORITIZE_INTERVAL);
   lSetUlong(ep, SC_halftime, 168);

   added = lAddSubStr(ep, UA_name, USAGE_ATTR_CPU, SC_usage_weight_list, UA_Type);
   lSetDouble(added, UA_value, 1.00);
   added = lAddSubStr(ep, UA_name, USAGE_ATTR_MEM, SC_usage_weight_list, UA_Type);
   lSetDouble(added, UA_value, 0.0);
   added = lAddSubStr(ep, UA_name, USAGE_ATTR_IO, SC_usage_weight_list, UA_Type);
   lSetDouble(added, UA_value, 0.0);

   lSetDouble(ep, SC_compensation_factor, 5);
   lSetDouble(ep, SC_weight_user, 0.25);
   lSetDouble(ep, SC_weight_project, 0.25);
   lSetDouble(ep, SC_weight_department, 0.25);
   lSetDouble(ep, SC_weight_job, 0.25);
   lSetUlong(ep, SC_weight_tickets_functional, 0);
   lSetUlong(ep, SC_weight_tickets_share, 0);

   lSetBool(ep, SC_share_override_tickets, true);  
   lSetBool(ep, SC_share_functional_shares, true);
   lSetUlong(ep, SC_max_functional_jobs_to_schedule, 200);
   lSetBool(ep, SC_report_pjob_tickets, true);
   lSetUlong(ep, SC_max_pending_tasks_per_job, 50);
   lSetString(ep, SC_halflife_decay_list, "none"); 
   lSetString(ep, SC_policy_hierarchy, policy_hierarchy_chars );

   lSetDouble(ep, SC_weight_ticket, 0.5);
   lSetDouble(ep, SC_weight_waiting_time, 0.278); 
   lSetDouble(ep, SC_weight_deadline, 3600000 );
   lSetDouble(ep, SC_weight_urgency, 0.5 );
   lSetUlong(ep, SC_max_reservation, 0);
   lSetDouble(ep, SC_weight_priority, 0.0 );
   lSetString(ep, SC_default_duration, DEFAULT_DURATION);

   DRETURN(ep);
}

/****** sge_schedd_conf/sconf_is_centry_referenced() ***************************
*  NAME
*     sconf_is_centry_referenced() -- ??? 
*
*  SYNOPSIS
*     bool sconf_is_centry_referenced(const lListElem *this_elem, const 
*     lListElem *centry) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     const lListElem *centry    - ??? 
*
*  RESULT
*     bool - 
*
*  MT-NOTE:   is MT save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
bool sconf_is_centry_referenced(const lListElem *centry) 
{
   bool ret = false;
   const lListElem *sc_ep = NULL;
   
   DENTER(TOP_LAYER, "sconf_is_centry_referenced");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   sc_ep = lFirst(Master_Sched_Config_List);
   
   if (sc_ep != NULL) {
      const char *name = lGetString(centry, CE_name);
      lList *centry_list = lGetList(sc_ep, SC_job_load_adjustments);
      lListElem *centry_ref = lGetElemStr(centry_list, CE_name, name);

      ret = ((centry_ref != NULL)? true : false);
   }

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(ret);
}

/****** sge_schedd_conf/get_load_adjustment_decay_time_str() *************
*  NAME
*     get_load_adjustment_decay_time_str() -- ??? 
*
*  SYNOPSIS
*     const char * get_load_adjustment_decay_time_str() 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*
*  RESULT
*     const char * - 
*
*  MT-NOTE:  is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(read)
*
*******************************************************************************/
static const char * get_load_adjustment_decay_time_str()
{
   const lListElem *sc_ep = lFirst(Master_Sched_Config_List); 
      
   if (pos.load_adjustment_decay_time != -1) {
      return lGetPosString(sc_ep, pos.load_adjustment_decay_time );
   }   
   else {
      return DEFAULT_LOAD_ADJUSTMENTS_DECAY_TIME;
   }   
}

/****** sge_schedd_conf/sconf_get_load_adjustment_decay_time() *****************
*  NAME
*     sconf_get_load_adjustment_decay_time() -- ??? 
*
*  SYNOPSIS
*     u_long32 sconf_get_load_adjustment_decay_time() 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*
*  RESULT
*     u_long32 - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
u_long32 sconf_get_load_adjustment_decay_time() 
{
   u_long32 uval;
   const char *time = NULL;

   DENTER(TOP_LAYER, "sconf_get_load_adjustment_decay_time");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);

   time = get_load_adjustment_decay_time_str();

   if (!extended_parse_ulong_val(NULL, &uval, TYPE_TIM, time, NULL, 0, 0)) {
      uval = _DEFAULT_LOAD_ADJUSTMENTS_DECAY_TIME;
   }

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(uval);
}

/****** sge_schedd_conf/sconf_get_job_load_adjustments() ***********************
*  NAME
*     sconf_get_job_load_adjustments() -- ??? 
*
*  SYNOPSIS
*     const lList* sconf_get_job_load_adjustments(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     const lList* - returns a copy, needs to be freed
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
lList *sconf_get_job_load_adjustments(void) {
   lList *load_adjustments = NULL;      
  
   DENTER(TOP_LAYER, "sconf_get_job_load_adjustments");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);

   load_adjustments = lCopyList("load_adj_copy", get_job_load_adjustments());
   
   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ); 
   DRETURN(load_adjustments);
}

/****** sge_schedd_conf/get_job_load_adjustments() ***********************
*  NAME
*     get_job_load_adjustments() -- ??? 
*
*  SYNOPSIS
*     const lList* get_job_load_adjustments(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     const lList* - 
*
*  MT-NOTE:  is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(read)
*
*******************************************************************************/
static const lList *get_job_load_adjustments(void) 
{
   const lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      
   if (pos.job_load_adjustments!= -1) {
      return lGetPosList(sc_ep, pos.job_load_adjustments); 
   }   
   else {
      return NULL;
   }   
}


/****** sge_schedd_conf/sconf_get_load_formula() *******************************
*  NAME
*     sconf_get_load_formula() -- ??? 
*
*  SYNOPSIS
*     const char* sconf_get_load_formula(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     const char* - this is a copy of the load formula, the caller has to free it
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
char* sconf_get_load_formula(void) {
   char *formula = NULL;      
  
   DENTER(TOP_LAYER, "sconf_get_load_formula");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);

   formula = sge_strdup(formula, get_load_formula());  
  
   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ); 
   DRETURN(formula);
}

/****** sge_schedd_conf/get_load_formula() *******************************
*  NAME
*     get_load_formula() -- ??? 
*
*  SYNOPSIS
*     const char* get_load_formula(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     const char* - 
*
*     MT-NOTE: is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(read)
*
*******************************************************************************/
static const char* get_load_formula(void) 
{
   const lListElem *sc_ep =  lFirst(Master_Sched_Config_List);
      
   if (pos.load_formula != -1) {
      return lGetPosString(sc_ep, pos.load_formula);
   }   
   else {
      return DEFAULT_LOAD_FORMULA;
   }   
}

/****** sge_schedd_conf/sconf_get_queue_sort_method() **************************
*  NAME
*     sconf_get_queue_sort_method() -- ??? 
*
*  SYNOPSIS
*     u_long32 sconf_get_queue_sort_method(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     u_long32 - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
u_long32 sconf_get_queue_sort_method(void) 
{
   const lListElem *sc_ep =  NULL;
   u_long32 sort_method = 0;
  
   DENTER(TOP_LAYER, "sconf_get_queue_sort_method");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   if (pos.queue_sort_method != -1) {
      sc_ep = lFirst(Master_Sched_Config_List);
      sort_method = lGetPosUlong(sc_ep, pos.queue_sort_method);
   }

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(sort_method);
}

/****** sge_schedd_conf/sconf_get_maxujobs() ***********************************
*  NAME
*     sconf_get_maxujobs() -- ??? 
*
*  SYNOPSIS
*     u_long32 sconf_get_maxujobs(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     u_long32 - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
u_long32 sconf_get_maxujobs(void) 
{
   u_long32 jobs = MAXUJOBS;
   
   DENTER(TOP_LAYER, "sconf_get_maxujobs");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   if (pos.maxujobs != -1) {
      const lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      jobs = lGetPosUlong(sc_ep, pos.maxujobs );
   }   

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(jobs);
}

/****** sge_schedd_conf/get_schedule_interval_str() **********************
*  NAME
*     get_schedule_interval_str() -- ??? 
*
*  SYNOPSIS
*     const char* get_schedule_interval_str(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     const char* - 
*
*  MT-NOTE: is not MT-safe, the caller needs to hold the LOCK_SCHED_CONF(read)
*
*******************************************************************************/
static const char *get_schedule_interval_str(void)
{
   if (pos.schedule_interval != -1) {
      const lListElem *sc_ep =  lFirst(Master_Sched_Config_List);
      return lGetPosString(sc_ep, pos.schedule_interval);
   }   
   else {
      return SCHEDULE_TIME;
   }   
}

/****** sge_schedd_conf/sconf_get_schedule_interval() **************************
*  NAME
*     sconf_get_schedule_interval() -- ??? 
*
*  SYNOPSIS
*     u_long32 sconf_get_schedule_interval(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     u_long32 - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
u_long32 sconf_get_schedule_interval(void) {
   u_long32 uval = _SCHEDULE_TIME;   
   const char *time = NULL;
   
   DENTER(TOP_LAYER, "sconf_get_schedule_interval");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   time = get_schedule_interval_str();
   if (!extended_parse_ulong_val(NULL, &uval, TYPE_TIM, time, NULL, 0, 0) ) {
         uval = _SCHEDULE_TIME;
   }

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ); 
   DRETURN(uval);  
} 


/****** sge_schedd_conf/reprioritize_interval_str() ********************
*  NAME
*     reprioritize_interval_str() -- ??? 
*
*  SYNOPSIS
*     const char* reprioritize_interval_str(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     const char* - 
*
*  MT-NOTE: is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(read)
*
*******************************************************************************/
static const char *reprioritize_interval_str(void)
{
   if (pos.reprioritize_interval!= -1) {
      const lListElem *sc_ep =  lFirst(Master_Sched_Config_List);
      return lGetPosString(sc_ep, pos.reprioritize_interval);
   }   
   else {
      return REPRIORITIZE_INTERVAL;
   }   
}

/****** sge_schedd_conf/sconf_get_reprioritize_interval() ********************
*  NAME
*     sconf_get_reprioritize_interval() -- ??? 
*
*  SYNOPSIS
*     u_long32 sconf_get_reprioritize_interval(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     u_long32 - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
u_long32 sconf_get_reprioritize_interval(void) {
   u_long32 uval = REPRIORITIZE_INTERVAL_I;
   const char *time = NULL;

   DENTER(TOP_LAYER, "sconf_get_reprioritize_interval");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
  
   time = reprioritize_interval_str();

   if (!extended_parse_ulong_val(NULL, &uval, TYPE_TIM,time, NULL, 0 ,0)) {
      uval = REPRIORITIZE_INTERVAL_I;
   }
   
   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ); 
   DRETURN(uval);
}

/****** sge_schedd_conf/sconf_enable_schedd_job_info() *************************
*  NAME
*     sconf_enable_schedd_job_info() -- ??? 
*
*  SYNOPSIS
*     void sconf_enable_schedd_job_info() 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*
*  RESULT
*     void - 
*
*  MT-NOTE: is thread save, uses local storage
*
*******************************************************************************/
void sconf_enable_schedd_job_info() 
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key, "sconf_enable_schedd_job_info");
   sc_state->schedd_job_info = SCHEDD_JOB_INFO_TRUE;
}

/****** sge_schedd_conf/sconf_disable_schedd_job_info() ************************
*  NAME
*     sconf_disable_schedd_job_info() -- ??? 
*
*  SYNOPSIS
*     void sconf_disable_schedd_job_info() 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*
*  RESULT
*     void - 
*
*  MT-NOTE: is thread save, uses local storage
*
*******************************************************************************/
void sconf_disable_schedd_job_info() 
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key, "sconf_disable_schedd_job_info");
   sc_state->schedd_job_info = SCHEDD_JOB_INFO_FALSE;
}

/****** sge_schedd_conf/sconf_get_schedd_job_info() ****************************
*  NAME
*     sconf_get_schedd_job_info() -- ??? 
*
*  SYNOPSIS
*     u_long32 sconf_get_schedd_job_info(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     u_long32 - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read) and local storage
*
*******************************************************************************/
u_long32 sconf_get_schedd_job_info(void) {
   u_long32 info = 0;

   DENTER(TOP_LAYER, "sconf_get_schedd_job_info");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   info = pos.c_is_schedd_job_info;

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);    

   if (info == SCHEDD_JOB_INFO_FALSE) {
      GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key, "sconf_get_schedd_job_info");
      info = sc_state->schedd_job_info;
   }
   
   DRETURN(info);
}

/****** sge_schedd_conf/sconf_get_schedd_job_info_range() **********************
*  NAME
*     sconf_get_schedd_job_info_range() -- ??? 
*
*  SYNOPSIS
*     const lList* sconf_get_schedd_job_info_range(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     const lList* -  returns a copy, needs to be freed
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
lList *sconf_get_schedd_job_info_range(void) {
   lList *range_copy = NULL;        
  
   DENTER(TOP_LAYER, "sconf_get_job_load_adjustments");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   range_copy = lCopyList("copy_range", pos.c_schedd_job_info_range);
   
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);    
   DRETURN(range_copy);
}

/****** sge_schedd_conf/sconf_is_id_in_schedd_job_info_range() **********************
*  NAME
*     sconf_is_id_in_schedd_job_info_range() -- ??? 
*
*  SYNOPSIS
*     const lList* sconf_is_id_in_schedd_job_info_range(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     bool -
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
bool sconf_is_id_in_schedd_job_info_range(u_long32 job_number) 
{
   bool is_in_range = false;        
  
   DENTER(TOP_LAYER, "sconf_is_id_in_schedd_job_info_range");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);

   is_in_range = range_list_is_id_within(pos.c_schedd_job_info_range, job_number);

   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);     
   DRETURN(is_in_range);
}

/****** sge_schedd_conf/get_algorithm() **********************************
*  NAME
*     get_algorithm() -- ??? 
*
*  SYNOPSIS
*     const char* get_algorithm(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     const char* -
*
*  MT-NOTE: is not MT-safe, the caller needs the LOCK_SCHED_CONF(read)
*
*******************************************************************************/
static const char * get_algorithm(void) 
{
   if (pos.algorithm!= -1) {
      const lListElem *sc_ep =  lFirst(Master_Sched_Config_List);
      return lGetPosString(sc_ep, pos.algorithm);
   }   
   else {
      return "default";
   }   
}


/****** sge_schedd_conf/sconf_get_usage_weight_list() **************************
*  NAME
*     sconf_get_usage_weight_list() -- ??? 
*
*  SYNOPSIS
*     const lList* sconf_get_usage_weight_list(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     lList* - returns a copy, needs to be freed
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
lList *sconf_get_usage_weight_list(void) 
{
   lList *weight_list = NULL;      
  
   DENTER(TOP_LAYER, "sconf_get_usage_weight_list");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   weight_list = lCopyList("copy_weight", get_usage_weight_list());

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ); 
   DRETURN(weight_list);
}


/****** sge_schedd_conf/get_usage_weight_list() **************************
*  NAME
*     get_usage_weight_list() -- ??? 
*
*  SYNOPSIS
*     const lList* get_usage_weight_list(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     const lList* - 
*
*  MT-NOTE: is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(read)
*
*******************************************************************************/
static const lList *get_usage_weight_list(void) 
{
   const lListElem *sc_ep =  lFirst(Master_Sched_Config_List);

   if (pos.usage_weight_list != -1) {
      return lGetPosList(sc_ep, pos.usage_weight_list );
   }   
   else {
      return NULL;
   }   
}



/****** sge_schedd_conf/sconf_get_weight_user() ********************************
*  NAME
*     sconf_get_weight_user() -- ??? 
*
*  SYNOPSIS
*     double sconf_get_weight_user(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     double - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
double sconf_get_weight_user(void) 
{
   double weight = 0;
   
   DENTER(TOP_LAYER, "sconf_get_weight_user");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   if (pos.weight_user!= -1) {
      const lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      weight = lGetPosDouble(sc_ep, pos.weight_user);
   }   

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(weight);
}

/****** sge_schedd_conf/sconf_get_weight_department() **************************
*  NAME
*     sconf_get_weight_department() -- ??? 
*
*  SYNOPSIS
*     double sconf_get_weight_department(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     double - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
double sconf_get_weight_department(void) 
{
   double weight = 0;
   
   DENTER(TOP_LAYER, "sconf_get_weight_department");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);

   if (pos.weight_department != -1) {
      const lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      weight = lGetPosDouble(sc_ep, pos.weight_department);
   }   

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(weight);
}

/****** sge_schedd_conf/sconf_get_weight_project() *****************************
*  NAME
*     sconf_get_weight_project() -- ??? 
*
*  SYNOPSIS
*     double sconf_get_weight_project(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     double - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
double sconf_get_weight_project(void) 
{
   double weight = 0;
   
   DENTER(TOP_LAYER, "sconf_get_weight_project");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);

   if (pos.weight_project != -1) {
      const lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      weight = lGetPosDouble(sc_ep, pos.weight_project);
   }

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(weight);
}

/****** sge_schedd_conf/sconf_get_weight_job() *********************************
*  NAME
*     sconf_get_weight_job() -- ??? 
*
*  SYNOPSIS
*     double sconf_get_weight_job(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     double - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
double sconf_get_weight_job(void) 
{
   double weight = 0;
   
   DENTER(TOP_LAYER, "sconf_get_weight_job");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);

   if (pos.weight_job != -1) {
      const lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      weight = lGetPosDouble(sc_ep, pos.weight_job);
   }   

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(weight);
}

/****** sge_schedd_conf/sconf_get_weight_tickets_share() ***********************
*  NAME
*     sconf_get_weight_tickets_share() -- ??? 
*
*  SYNOPSIS
*     u_long32 sconf_get_weight_tickets_share(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     u_long32 - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
u_long32 sconf_get_weight_tickets_share(void) 
{
   double weight = 0;
   
   DENTER(TOP_LAYER, "sconf_get_weight_tickets_share");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);

   if (pos.weight_tickets_share != -1) {
      const lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      weight = lGetPosUlong(sc_ep, pos.weight_tickets_share );
   }   

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(weight);
}

/****** sge_schedd_conf/sconf_get_weight_tickets_functional() ******************
*  NAME
*     sconf_get_weight_tickets_functional() -- ??? 
*
*  SYNOPSIS
*     u_long32 sconf_get_weight_tickets_functional(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     u_long32 - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
u_long32 sconf_get_weight_tickets_functional(void) 
{
   double weight = 0;
   
   DENTER(TOP_LAYER, "sconf_get_weight_tickets_functional");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);

   if (pos.weight_tickets_functional != -1) {
      const lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      weight = lGetPosUlong(sc_ep, pos.weight_tickets_functional);
   }   

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(weight);
}

/****** sge_schedd_conf/sconf_get_halftime() ***********************************
*  NAME
*     sconf_get_halftime() -- ??? 
*
*  SYNOPSIS
*     u_long32 sconf_get_halftime(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     u_long32 - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
u_long32 sconf_get_halftime(void) 
{
   const lListElem *sc_ep = NULL;
   u_long32 halftime = 0;
  
   DENTER(TOP_LAYER, "sconf_get_halftime");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   if (pos.halftime != -1) {
      sc_ep = lFirst(Master_Sched_Config_List);
      halftime = lGetPosUlong(sc_ep, pos.halftime);
   }   

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(halftime);
}


/****** sge_schedd_conf/sconf_set_weight_tickets_override() ********************
*  NAME
*     sconf_set_weight_tickets_override() -- ??? 
*
*  SYNOPSIS
*     void sconf_set_weight_tickets_override(u_long32 active) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     u_long32 active - ??? 
*
*  RESULT
*     void - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(write)
*
*******************************************************************************/
void sconf_set_weight_tickets_override(u_long32 active) 
{
   lListElem *sc_ep = NULL;

   DENTER(TOP_LAYER, "sconf_get_job_load_adjustments");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_WRITE);   

   sc_ep = lFirst(Master_Sched_Config_List);
   
   if (pos.weight_tickets_override!= -1) {
      lSetPosUlong(sc_ep, pos.weight_tickets_override, active);
   }

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_WRITE);
   DEXIT;
}

/****** sge_schedd_conf/sconf_get_weight_tickets_override() ********************
*  NAME
*     sconf_get_weight_tickets_override() -- ??? 
*
*  SYNOPSIS
*     void sconf_get_weight_tickets_override(u_long32 active) 
*
*  FUNCTION
*     ??? 
*
*  RESULT
*     u_long32 - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
u_long32 sconf_get_weight_tickets_override() 
{
   u_long32 tickets = 0;
   
   DENTER(TOP_LAYER, "sconf_get_weight_tickets_override");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);   

   if (pos.weight_tickets_override!= -1) {
      lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      tickets = lGetPosUlong(sc_ep, pos.weight_tickets_override);
   }

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ); 
   DRETURN(tickets);

}

/****** sge_schedd_conf/sconf_get_compensation_factor() ************************
*  NAME
*     sconf_get_compensation_factor() -- ??? 
*
*  SYNOPSIS
*     double sconf_get_compensation_factor(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     double - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
double sconf_get_compensation_factor(void) 
{
   double factor = 1;
   
   DENTER(TOP_LAYER, "sconf_get_queue_sort_method");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);

   if (pos.compensation_factor!= -1) {
      const lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      factor = lGetPosDouble(sc_ep, pos.compensation_factor);
   }

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(factor);
}

/****** sge_schedd_conf/sconf_get_weight_ticket() ****************************
*  NAME
*     sconf_get_weight_ticket() -- ??? 
*
*  SYNOPSIS
*     double sconf_get_weight_ticket(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     double - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
double sconf_get_weight_ticket(void) 
{
   double  weight = 0;   

   DENTER(TOP_LAYER, "sconf_get_share_override_tickets");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   if (pos.weight_ticket != -1) {
      const lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      weight = lGetPosDouble(sc_ep, pos.weight_ticket);
   }   

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(weight);
}     

/****** sge_schedd_conf/sconf_get_weight_waiting_time() ************************
*  NAME
*     sconf_get_weight_waiting_time() -- ??? 
*
*  SYNOPSIS
*     double sconf_get_weight_waiting_time(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     double - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
double sconf_get_weight_waiting_time(void) 
{
   double weight = 0;

   DENTER(TOP_LAYER, "sconf_get_weight_waiting_time");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   if (pos.weight_waiting_time != -1) {
      const lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      weight = lGetPosDouble(sc_ep, pos.weight_waiting_time);
   }   

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(weight);
}     

/****** sge_schedd_conf/sconf_get_weight_deadline() ****************************
*  NAME
*     sconf_get_weight_deadline() -- ??? 
*
*  SYNOPSIS
*     double sconf_get_weight_deadline(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     double - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
double sconf_get_weight_deadline(void) 
{
   double weight = 0;

   DENTER(TOP_LAYER, "sconf_get_weight_deadline");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   if (pos.weight_deadline != -1) {
      const lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      weight = lGetPosDouble(sc_ep, pos.weight_deadline);
   }   

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(weight);
}     

/****** sge_schedd_conf/sconf_get_weight_urgency() ****************************
*  NAME
*     sconf_get_weight_urgency() -- ??? 
*
*  SYNOPSIS
*     double sconf_get_weight_urgency(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     double - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
double sconf_get_weight_urgency(void) 
{
   double weight = 0;

   DENTER(TOP_LAYER, "sconf_get_weight_urgency");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   if (pos.weight_urgency != -1) {
      const lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      weight = lGetPosDouble(sc_ep, pos.weight_urgency);
   }   

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(weight);
}     

/****** sge_schedd_conf/sconf_get_max_reservations() ***************************
*  NAME
*     sconf_get_max_reservations() -- Max reservation tuning parameter
*
*  SYNOPSIS
*     int sconf_get_max_reservations(void) 
*
*  FUNCTION
*     Tuning parameter. 
*     Returns maximum number of reservations that should be done by 
*     scheduler. If 0 is returned this no single job shall get a reservation
*     and assignments are to be made for 'now' only.
*     
*  RESULT
*     int - Max. number of reservations
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
u_long32 sconf_get_max_reservations(void) {
   u_long32 max_res = 0;
 
   DENTER(TOP_LAYER, "sconf_get_max_reservations");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   if (!pos.empty && (pos.max_reservation != -1)) {
      lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      max_res = lGetPosUlong(sc_ep, pos.max_reservation);
   }

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(max_res);
}

/****** sge_schedd_conf/get_default_duration_str() **********************
*  NAME
*     get_default_duration_str() -- Return default_duration string
*
*  SYNOPSIS
*     const char* get_default_duration_str(void) 
*
*  FUNCTION
*     Returns default duration string from scheduler configuration.
*
*  RESULT
*     const char* - 
*
*  MT-NOTE: is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(read)
*
*******************************************************************************/
static const char *get_default_duration_str(void)
{
   const lListElem *sc_ep =  lFirst(Master_Sched_Config_List);
      
   if (pos.schedule_interval != -1) {
      return lGetPosString(sc_ep, pos.default_duration);
   }   
   else {
      return DEFAULT_DURATION;
   }   
}
/****** sge_schedd_conf/sconf_get_weight_priority() ****************************
*  NAME
*     sconf_get_weight_priority() -- ??? 
*
*  SYNOPSIS
*     double sconf_get_weight_priority(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     double - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
double sconf_get_weight_priority(void) 
{
   double weight = 0;

   DENTER(TOP_LAYER, "sconf_get_weight_priority");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
      
   if (pos.weight_priority != -1) {
      const lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      weight = lGetPosDouble(sc_ep, pos.weight_priority);
   }   

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(weight);
}     


/****** sge_schedd_conf/sconf_get_share_override_tickets() *********************
*  NAME
*     sconf_get_share_override_tickets() -- ??? 
*
*  SYNOPSIS
*     bool sconf_get_share_override_tickets(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     bool - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
bool sconf_get_share_override_tickets(void) 
{
   bool is_share = false;

   DENTER(TOP_LAYER, "sconf_get_share_override_tickets");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);

   if (pos.share_override_tickets != -1) {
      const lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      is_share = lGetPosBool(sc_ep, pos.share_override_tickets) ? true : false;
   }   

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(is_share);
}
/****** sge_schedd_conf/sconf_get_share_functional_shares() ********************
*  NAME
*     sconf_get_share_functional_shares() -- ??? 
*
*  SYNOPSIS
*     bool sconf_get_share_functional_shares(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     bool - 
*******************************************************************************/
bool sconf_get_share_functional_shares(void)
{
   bool is_share = true;

   DENTER(TOP_LAYER, "sconf_get_share_functional_shares");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);

   if (pos.share_functional_shares != -1) {
      const lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      is_share = lGetPosBool(sc_ep, pos.share_functional_shares) ? true : false;
   }   

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(is_share);
}

/****** sge_schedd_conf/sconf_get_report_pjob_tickets() *************************
*  NAME
*     sconf_get_report_pjob_tickets() -- ??? 
*
*  SYNOPSIS
*     bool sconf_get_report_pjob_tickets(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     bool - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
bool sconf_get_report_pjob_tickets(void)
{
   bool is_report = true;

   DENTER(TOP_LAYER, "sconf_get_report_pjob_tickets");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);

   if (pos.report_pjob_tickets!= -1) {
      const lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      is_report = lGetPosBool(sc_ep, pos.report_pjob_tickets) ? true : false;
   }   

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(is_report);
}

/****** sge_schedd_conf/sconf_is_job_category_filtering() **********************
*  NAME
*     sconf_is_job_category_filtering() -- true, if the job_category_filtering is on
*
*  SYNOPSIS
*     bool sconf_is_job_category_filtering(void) 
*
*  FUNCTION
*     returns the status of the job category filtering 
*
*  RESULT
*     bool - true, the job category filtering is on
*
*  NOTES
*     MT-NOTE: is MT save, uses LOCK_SCHED_CONF(read) 
*
*******************************************************************************/
bool sconf_is_job_category_filtering(void){
   bool filtering = false;

   DENTER(TOP_LAYER, "sconf_is_job_category_filtering");   
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   filtering = is_category_job_filtering;   

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(filtering);
}

/****** sge_schedd_conf/sconf_get_flush_submit_sec() ***************************
*  NAME
*     sconf_get_flush_submit_sec() -- ??? 
*
*  SYNOPSIS
*     int sconf_get_flush_submit_sec(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     int - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
u_long32 sconf_get_flush_submit_sec(void)
{
   const lListElem *sc_ep = NULL;
   u_long32 flush_sec = (u_long32)-1;
  
   DENTER(TOP_LAYER, "sconf_get_flush_submit_sec");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
      
   if (pos.flush_submit_sec!= -1) {
      sc_ep = lFirst(Master_Sched_Config_List);
      flush_sec = lGetPosUlong(sc_ep, pos.flush_submit_sec);
   }

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(flush_sec);
}
   
/****** sge_schedd_conf/sconf_get_flush_finish_sec() ***************************
*  NAME
*     sconf_get_flush_finish_sec() -- ??? 
*
*  SYNOPSIS
*     int sconf_get_flush_finish_sec(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     int - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
u_long32 sconf_get_flush_finish_sec(void)
{
   const lListElem *sc_ep = NULL;
   u_long32 flush_sec = (u_long32)-1;
  
   DENTER(TOP_LAYER, "sconf_get_flush_finish_sec");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
      
   if (pos.flush_finish_sec!= -1) {
      sc_ep = lFirst(Master_Sched_Config_List);
      flush_sec = lGetPosUlong(sc_ep, pos.flush_finish_sec);
   }

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(flush_sec);
}


/****** sge_schedd_conf/sconf_get_max_functional_jobs_to_schedule() ************
*  NAME
*     sconf_get_max_functional_jobs_to_schedule() -- ??? 
*
*  SYNOPSIS
*     u_long32 sconf_get_max_functional_jobs_to_schedule(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     u_long32 - 
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
u_long32 sconf_get_max_functional_jobs_to_schedule(void)
{
   u_long32 amount = 200;

   DENTER(TOP_LAYER, "sconf_get_share_override_tickets");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);

   if (pos.max_functional_jobs_to_schedule != -1) {
      const lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      amount = lGetPosUlong(sc_ep, pos.max_functional_jobs_to_schedule);
   }   

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(amount);
}

/****** sge_schedd_conf/sconf_get_max_pending_tasks_per_job() ******************
*  NAME
*     sconf_get_max_pending_tasks_per_job() -- ??? 
*
*  SYNOPSIS
*     u_long32 sconf_get_max_pending_tasks_per_job(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     u_long32 - 
*
*  MT-NOTE:   is MT save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
u_long32 sconf_get_max_pending_tasks_per_job(void)
{
   u_long32 max_pending = 50;
 
   DENTER(TOP_LAYER, "sconf_get_max_pending_tasks_per_job");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   if (pos.max_pending_tasks_per_job != -1) {
      const lListElem *sc_ep = lFirst(Master_Sched_Config_List);
      max_pending = lGetPosUlong(sc_ep, pos.max_pending_tasks_per_job);
   }
      
   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(max_pending);
}

/****** sge_schedd_conf/get_halflife_decay_list_str() ********************
*  NAME
*     get_halflife_decay_list_str() -- ??? 
*
*  SYNOPSIS
*     const char* get_halflife_decay_list_str(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     const char* - 
*
* MT-NOTE: is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(read)
*
*******************************************************************************/
static const char *get_halflife_decay_list_str(void)
{
   const lListElem *sc_ep = lFirst(Master_Sched_Config_List);

   if (pos.halflife_decay_list != -1) {
      return lGetPosString(sc_ep, pos.halflife_decay_list);
   }   
   else {
      return "none";
   }
}

/****** sge_schedd_conf/sconf_get_halflife_decay_list() ************************
*  NAME
*     sconf_get_halflife_decay_list() -- ??? 
*
*  SYNOPSIS
*     const lList* sconf_get_halflife_decay_list(void) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     void - ??? 
*
*  RESULT
*     const lList* - 
*
*  MT-NOTE:   is MT save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
lList* sconf_get_halflife_decay_list(void){
   lList *decay_list = NULL; 

   DENTER(TOP_LAYER, "sconf_get_halflife_decay_list");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
  
   decay_list = lCopyList("copy_decay_list",pos.c_halflife_decay_list);
      
   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);    
   DRETURN(decay_list);      
}

/****** sge_schedd_conf/is_config_set() *********************************************
*  NAME
*     is_config_set() -- checks, if a configuration exists
*
*  SYNOPSIS
*     bool is_config_set(void) 
*
*  RESULT
*     bool - true, if a configuration exists
*
*  MT-NOTE: is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(read)
*
*******************************************************************************/
static bool is_config_set(void) 
{
   const lListElem *sc_ep = NULL;
   
   if (Master_Sched_Config_List) {
      sc_ep = lFirst(Master_Sched_Config_List);
   }   

   return ((sc_ep != NULL) ? true : false);
}

/****** sge_schedd_conf/sconf_is() *********************************************
*  NAME
*     sconf_is() -- checks, if a configuration exists
*
*  SYNOPSIS
*     bool sconf_is(void) 
*
*  RESULT
*     bool - true, if a configuration exists
*
*
*  MT-NOTE:   is MT save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
bool sconf_is(void) {
   bool is = false;
  
   DENTER(TOP_LAYER, "sconf_is");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);

   is = is_config_set();

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(is);
}


/****** sge_schedd_conf/sconf_get_config() *************************************
*  NAME
*     sconf_get_config() -- returns a config object.  
*
*  SYNOPSIS
*     const lListElem* sconf_get_config(void) 
*
*  RESULT
*     const lListElem* - a copy of the current config object
*
* NOTE
*  DO NOT USE this method. ONLY when there is NO OTHER way. All config
*  settings can be accessed via access function.
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
*******************************************************************************/
lListElem *sconf_get_config(void)
{
   lListElem *config = NULL;
  
   DENTER(TOP_LAYER, "sconf_get_config");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   config = lCopyElem(lFirst(Master_Sched_Config_List));

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ); 
   DRETURN(config);
}

/****** sge_schedd_conf/sconf_get_config_list() ********************************
*  NAME
*     sconf_get_config_list() -- returns a pointer to the list, which contains
*                                the configuration
*
*  SYNOPSIS
*     lList* sconf_get_config_list(void) 
*
*  RESULT
*     lList* - a copy of the config list
*
*  MT-NOTE: is thread save, uses LOCK_SCHED_CONF(read)
*
* NOTE
*  DO NOT USE this method. ONLY when there is NO OTHER way. All config
*  settings can be accessed via access function. The config can be set
*  via sconf_set_config(...)
*
* IMPORTANT
*
*  If you modify the configuration by directly accessing, you have to call
*  sconf_validate_config_ afterwards to ensure, that the caches reflect
*  your changes.
*
*******************************************************************************/
lList *sconf_get_config_list(void)
{
   lList *copy_list = NULL;

   DENTER(TOP_LAYER, "sconf_get_config_list");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
 
   copy_list = lCopyList("sched_conf_copy", Master_Sched_Config_List);

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ); 
   DRETURN(copy_list);
}

/****** sge_schedd_conf/sconf_print_config() ***********************************
*  NAME
*     sconf_print_config() -- prints the current configuration to the INFO stream 
*
*  SYNOPSIS
*     void sconf_print_config() 
*
*  MT-NOTE:  is MT safe, uses LOCK_SCHED_CONF(read/write) 
*
*******************************************************************************/
void sconf_print_config(void){

   char tmp_buffer[1024];
   u_long32 uval;
   const char *s;
   const lList *lval= NULL;
   double dval;

   DENTER(TOP_LAYER, "sconf_print_config");

   if (!sconf_is()){
      ERROR((SGE_EVENT, "sconf_printf_config: no config to validate\n"));
      DEXIT;
      return;
   }

   sconf_validate_config_(NULL);

   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);

   /* --- SC_algorithm */
   s = get_algorithm();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXASY_SS , s, "algorithm"));
     
   /* --- SC_schedule_interval */
   s = get_schedule_interval_str();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_SS , s, "schedule_interval"));

   /* --- SC_load_adjustment_decay_time */
   s = get_load_adjustment_decay_time_str();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_SS, s, "load_adjustment_decay_time"));

   /* --- SC_load_formula */
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_SS, get_load_formula(), "load_formula"));

   /* --- SC_schedd_job_info */
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_SS, lGetString(lFirst(Master_Sched_Config_List), SC_schedd_job_info), "schedd_job_info"));
   
   /* --- SC_params */
   s=lGetString(lFirst(Master_Sched_Config_List), SC_params);
   INFO((SGE_EVENT, MSG_READ_PARAM_S, s)); 

   /* --- SC_reprioritize_interval */
   s = reprioritize_interval_str();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_SS, s, "reprioritize_interval"));

   /* --- SC_usage_weight_list */
   uni_print_list(NULL, tmp_buffer, sizeof(tmp_buffer), get_usage_weight_list(), usage_fields, delis, 0);
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_SS, tmp_buffer, "usage_weight_list"));

   /* --- SC_halflife_decay_list_str */
   s = get_halflife_decay_list_str();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_SS, s, "halflife_decay_list"));
  
   /* --- SC_policy_hierarchy */
   s = lGetString(lFirst(Master_Sched_Config_List), SC_policy_hierarchy);
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_SS, s, "policy_hierarchy"));

   /* --- SC_job_load_adjustments */
   lval = get_job_load_adjustments();
   uni_print_list(NULL, tmp_buffer, sizeof(tmp_buffer), lval, load_adjustment_fields, delis, 0);
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_SS, tmp_buffer, "job_load_adjustments"));
   
   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);

   /* --- SC_maxujobs */
   uval = sconf_get_maxujobs();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_US, sge_u32c( uval), "maxujobs"));

   /* --- SC_queue_sort_method (was: SC_sort_seq_no) */
   uval = sconf_get_queue_sort_method();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_US,  sge_u32c (uval), "queue_sort_method"));

   /* --- SC_flush_submit_sec */
   uval = sconf_get_flush_submit_sec();
   INFO((SGE_EVENT,MSG_ATTRIB_USINGXFORY_US,  sge_u32c (uval) , "flush_submit_sec"));

   /* --- SC_flush_finish_sec */
   uval = sconf_get_flush_finish_sec();
   INFO((SGE_EVENT,MSG_ATTRIB_USINGXFORY_US,  sge_u32c (uval) , "flush_finish_sec"));
   
   /* --- SC_halftime */
   uval = sconf_get_halftime();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_US ,  sge_u32c (uval), "halftime"));

   /* --- SC_compensation_factor */
   dval = sconf_get_compensation_factor();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_6FS, dval, "compensation_factor"));

   /* --- SC_weight_user */
   dval = sconf_get_weight_user();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_6FS, dval, "weight_user"));

   /* --- SC_weight_project */
   dval = sconf_get_weight_project();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_6FS, dval, "weight_project"));

   /* --- SC_weight_department */
   dval = sconf_get_weight_department();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_6FS, dval, "weight_department"));

   /* --- SC_weight_job */
   dval = sconf_get_weight_job();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_6FS, dval, "weight_job"));

   /* --- SC_weight_tickets_functional */
   uval = sconf_get_weight_tickets_functional();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_US,  sge_u32c (uval), "weight_tickets_functional"));

   /* --- SC_weight_tickets_share */
   uval = sconf_get_weight_tickets_share();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_US,  sge_u32c (uval), "weight_tickets_share"));

   /* --- SC_share_override_tickets */
   uval = sconf_get_share_override_tickets();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_US,  sge_u32c (uval), "share_override_tickets"));

   /* --- SC_share_functional_shares */
   uval = sconf_get_share_functional_shares();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_US,  sge_u32c (uval), "share_functional_shares"));

   /* --- SC_max_functional_jobs_to_schedule */
   uval = sconf_get_max_functional_jobs_to_schedule();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_US,  sge_u32c (uval), "max_functional_jobs_to_schedule"));
   
   /* --- SC_report_job_tickets */
   uval = sconf_get_report_pjob_tickets();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_US,  sge_u32c (uval), "report_pjob_tickets"));
   
   /* --- SC_max_pending_tasks_per_job */
   uval = sconf_get_max_pending_tasks_per_job();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_US,  sge_u32c (uval), "max_pending_tasks_per_job"));

   /* --- SC_weight_ticket */
   dval = sconf_get_weight_ticket();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_6FS,  dval, "weight_ticket"));

   /* --- SC_weight_waiting_time */
   dval = sconf_get_weight_waiting_time();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_6FS,  dval, "weight_waiting_time"));

   /* --- SC_weight_deadline */
   dval = sconf_get_weight_deadline();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_6FS,  dval, "weight_deadline"));

   /* --- SC_weight_urgency */
   dval = sconf_get_weight_urgency();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_6FS,  dval, "weight_urgency"));

   /* --- SC_weight_priority */
   dval = sconf_get_weight_priority();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_6FS,  dval, "weight_priority"));

   /* --- SC_max_reservation */
   dval = sconf_get_max_reservations();
   INFO((SGE_EVENT, MSG_ATTRIB_USINGXFORY_6FS,  dval, "max_reservation"));

   DEXIT;
   return;
}

/****** sge_schedd_conf/sconf_is_new_config() *******************************
*  NAME
*     sconf_is_new_config() -- 
*
*  SYNOPSIS
*     bool sconf_is_new_config() 
*
*  FUNCTION
*
*  RESULT
*     bool - 
*
*  MT-NOTE:  is MT safe, uses LOCK_SCHED_CONF(read) 
*
*******************************************************************************/
bool sconf_is_new_config() {
   bool is_new_config = false;
   
   DENTER(TOP_LAYER, "sconf_is_new_config");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   is_new_config = pos.new_config;
   
   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(is_new_config);
}

/****** sge_schedd_conf/sconf_reset_new_config() *******************************
*  NAME
*     sconf_reset_new_config() -- 
*
*  SYNOPSIS
*     bool sconf_reset_new_config() 
*
*  FUNCTION
*
*  MT-NOTE:  is MT safe, uses LOCK_SCHED_CONF(write) 
*
*******************************************************************************/
void sconf_reset_new_config() 
{
   DENTER(TOP_LAYER, "sconf_reset_new_config");   
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_WRITE);  
   
   pos.new_config = false;
   
   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_WRITE);
   DEXIT;
}

/****** sge_schedd_conf/sconf_validate_config_() *******************************
*  NAME
*     sconf_validate_config_() -- validates the current config 
*
*  SYNOPSIS
*     bool sconf_validate_config_(lList **answer_list) 
*
*  FUNCTION
*     validates the current config and updates the caches. 
*
*  INPUTS
*     lList **answer_list - error messages 
*
*  RESULT
*     bool - false for invalid scheduler configuration
*
*  MT-NOTE:  is MT safe, uses LOCK_SCHED_CONF(read/write) 
*
*******************************************************************************/
bool sconf_validate_config_(lList **answer_list)
{
   char tmp_buffer[1024], tmp_error[1024];
   u_long32 uval;
   const char *s;
   const lList *lval= NULL;
   bool ret = true;
   u_long32 max_reservation = 0;

   DENTER(TOP_LAYER, "sconf_validate_config_");

   if (!sconf_is()){
      DPRINTF(("sconf_validate: no config to validate\n"));
      DRETURN(true);
   }
   
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_WRITE);
   sconf_clear_pos();

   pos.new_config = true; 
   
   if (!calc_pos()){
      SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_INCOMPLETE_SCHEDD_CONFIG)); 
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false; 
   }

 /* --- SC_params */
   {
      const char *sparams = lGetString(lFirst(Master_Sched_Config_List), SC_params); 
      char *s = NULL; 
      
      /* the implementation has a problem. If an entry is removed, its setting is not
         changed, but it should be turned off. This means we have to turn everything off,
         before we work on the params */
      schedd_profiling = false;
      is_category_job_filtering = false;
      current_serf_do_monitoring = false;
      pos.s_duration_offset = DEFAULT_DURATION_OFFSET; 

      if (sparams) {
         struct saved_vars_s *context = NULL;

         if (pos.c_params == NULL) {
            pos.c_params = lCreateList("params", PARA_Type);
         }
         for (s=sge_strtok_r(sparams, ",; ", &context); s; s=sge_strtok_r(NULL, ",; ", &context)) {
            int i = 0;
            bool added = false;
            for(i=0; params[i].name ;i++ ){
               if (!strncasecmp(s, params[i].name, sizeof(params[i].name)-1)){
                  if (params[i].setParam) {
                     ret &= params[i].setParam(pos.c_params, answer_list, s);
                  }
                  added = true;
               }               
            }
            if (!added){
               SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_UNKNOWN_PARAM_S, s));
               answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
               ret = false;
            }
         }
         sge_free_saved_vars(context);
      } else {
         lSetString(lFirst(Master_Sched_Config_List), SC_params, "none");
      }
   }
   
   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_WRITE);

   max_reservation = sconf_get_max_reservations();
   
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   /* --- SC_algorithm */
   s = get_algorithm();
   if ( !s || (strcmp(s, "default") && strcmp(s, "simple_sched") && strncmp(s, "ext_", 4))) {
      if (!s)
         s = "not defined";
      SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_ATTRIB_ALGORITHMNOVALIDNAME_S, s)); 
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false; 
   }
     
   /* --- SC_schedule_interval */
   s = get_schedule_interval_str();
   if (!s || !extended_parse_ulong_val(NULL, &uval, TYPE_TIM, s, tmp_error, sizeof(tmp_error),0) ) {
      if (!s)
         SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_ATTRIB_XISNOTAY_SS , "schedule_interval", "not defined"));   
      else
         SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_ATTRIB_XISNOTAY_SS , "schedule_interval", tmp_error));    
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret =  false;
   }

   /* --- SC_load_adjustment_decay_time */
   s = get_load_adjustment_decay_time_str();
   if (!s || !extended_parse_ulong_val(NULL, &uval, TYPE_TIM, s, tmp_error, sizeof(tmp_error),0)) {
      if (!s) {
         SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_ATTRIB_XISNOTAY_SS , "schedule_interval", "not defined"));
      }   
      else {
         SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_ATTRIB_XISNOTAY_SS, "load_adjustment_decay_time", tmp_error));    
      }   
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false; 
   }
  
  /* --- SC_schedd_job_info */
   {
      char buf[4096];
      char* key = NULL;
      int ikey = 0;
      lList *rlp=NULL, *alp=NULL;
      const char *schedd_info = lGetString(lFirst(Master_Sched_Config_List), SC_schedd_job_info);

      if (schedd_info == NULL){
         SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_ATTRIB_SCHEDDJOBINFONOVALIDPARAM ));
         answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);  
         ret = false;
      }
      else {
         struct saved_vars_s *context = NULL;
         strcpy(buf, schedd_info);
         /* on/off or watch a set of jobs */
         key = sge_strtok_r(buf, " \t", &context);
         if (!strcmp("true", key)) 
            ikey = SCHEDD_JOB_INFO_TRUE;
         else if (!strcmp("false", key)) 
            ikey = SCHEDD_JOB_INFO_FALSE;
         else if (!strcmp("job_list", key)) 
            ikey = SCHEDD_JOB_INFO_JOB_LIST;
         else {
            SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_ATTRIB_SCHEDDJOBINFONOVALIDPARAM ));
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            ret = false; 
         }
         /* check list of groups */
         if (ikey == SCHEDD_JOB_INFO_JOB_LIST) {
            key = sge_strtok_r(NULL, "\n", &context);
            range_list_parse_from_string(&rlp, &alp, key, false, false, 
                                         INF_NOT_ALLOWED);
            if (rlp == NULL) {
               lFreeList(&alp);
               SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_ATTRIB_SCHEDDJOBINFONOVALIDJOBLIST));
               answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
               ret = false; 
            }   
            else{
               pos.c_is_schedd_job_info = ikey;
               pos.c_schedd_job_info_range = rlp;
            }
         }
         else{
            pos.c_is_schedd_job_info = ikey;
         }
         sge_free_saved_vars(context);
      }
   }

   /* --- SC_reprioritize_interval */
   s = reprioritize_interval_str();
   if (s == NULL || !extended_parse_ulong_val(NULL, &uval, TYPE_TIM, s, tmp_error, sizeof(tmp_error),0)) {
      if (s == NULL) {
         SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_ATTRIB_XISNOTAY_SS , "schedule_interval", "not defined"));
      }   
      else {   
         SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_ATTRIB_XISNOTAY_SS , "reprioritize_interval", tmp_error));    
      }   
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false; 
   }
  
   /* --- SC_halflife_decay_list_str */
   {
      s = get_halflife_decay_list_str();
      if (s && (strcasecmp(s, "none") != 0)) {
         lList *halflife_decay_list = NULL;
         lListElem *ep;
         const char *s0,*s1,*s2,*s3;
         double value;
         struct saved_vars_s *sv1=NULL, *sv2=NULL;
         s0 = s; 
         for(s1=sge_strtok_r(s0, ":", &sv1); s1;
             s1=sge_strtok_r(NULL, ":", &sv1)) {
            if ((s2=sge_strtok_r(s1, "=", &sv2)) &&
                (s3=sge_strtok_r(NULL, "=", &sv2)) &&
                (sscanf(s3, "%lf", &value)==1)) {
               ep = lAddElemStr(&halflife_decay_list, UA_name, s2, UA_Type);
               lSetDouble(ep, UA_value, value);
            }
            if (sv2)
               free(sv2);
         }
         if (sv1)
            free(sv1);
         pos.c_halflife_decay_list = halflife_decay_list;   
      } 
   }
  
   /* --- SC_policy_hierarchy */
   {
      const char *value_string = lGetString(lFirst(Master_Sched_Config_List), SC_policy_hierarchy);
      if (value_string) {
         if (policy_hierarchy_verify_value(value_string) != 0) {
            answer_list_add(answer_list, MSG_GDI_INVALIDPOLICYSTRING, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);  
            ret = false;
            lSetString(lFirst(Master_Sched_Config_List), SC_policy_hierarchy, policy_hierarchy_chars);
         }
      } 
      else {
         if (!s)
         value_string = "not defined";
         SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_ATTRIB_XISNOTAY_SS , "policy hierarchy", value_string));    
         answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         ret = false;          
      }
   }
  
   /* --- SC_max_reservation and SC_default_duration */
   {
      const char *s = get_default_duration_str();

      if (s == NULL || !extended_parse_ulong_val(NULL, &uval, TYPE_TIM, s, tmp_error, sizeof(tmp_error),0) ) {
         if (s == NULL) {
            SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_ATTRIB_XISNOTAY_SS , "default_duration", "not defined"));   
         }   
         else {
            SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_ATTRIB_XISNOTAY_SS , "default_duration", tmp_error));    
         }   
         answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         ret =  false;
      } 
      else {
         /* ensure we get a non-zero/non-infinity duration default in reservation scheduling mode */
         if (max_reservation != 0 && (uval == 0 || uval == MAX_ULONG32)) {
            SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_RR_REQUIRES_DEFAULT_DURATION));    
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);      
            ret = false; 
         }
         else {
            pos.c_default_duration = uval;
         }
      }
   }
  
   /* --- SC_usage_weight_list */
   if (uni_print_list(NULL, tmp_buffer, sizeof(tmp_buffer), 
       get_usage_weight_list(), usage_fields, delis, 0) < 0) {
      SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_SCHEDD_USAGE_WEIGHT_LIST_S, tmp_error));    
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);      
      ret = false; 
   }

   /* --- SC_job_load_adjustments */
   lval = get_job_load_adjustments();
   if (uni_print_list(NULL, tmp_buffer, sizeof(tmp_buffer), lval, load_adjustment_fields, delis, 0) < 0) {
      SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_SCHEDD_JOB_LOAD_ADJUSTMENTS_S, s)); 
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   /* --- SC_load_formula */
   {
      lList *master_centry_list = *object_type_get_master_list(SGE_TYPE_CENTRY);
      if (master_centry_list != NULL && !sconf_is_valid_load_formula_(answer_list, master_centry_list )) {
         ret = false; 
      }
   }

   /* --- max_pending_tasks_per_job */
   if (sconf_get_max_pending_tasks_per_job() == 0) {
      SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_ATTRIB_WRONG_SETTING_SS, "max_pending_tasks_per_job", ">0"));    
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }
   
   DRETURN(ret);
}


/****** sge_schedd_conf/sconf_validate_config() ********************************
*  NAME
*     sconf_validate_config() -- validate a given configuration 
*
*  SYNOPSIS
*     bool sconf_validate_config(lList **answer_list, lList *config) 
*
*  INPUTS
*     lList **answer_list - error messages 
*     lList *config       - config to validate 
*
*  RESULT
*     bool - true, if the config is valid
*
*  MT-NOTE:  is MT safe, uses LOCK_SCHED_CONF(write)
*
* SG TODO: needs cleanup!!
*
*******************************************************************************/
bool sconf_validate_config(lList **answer_list, lList *config){
   lList *store = NULL;
   bool ret = true;

   DENTER(TOP_LAYER, "sconf_validate_config");

   if (config){
      SGE_LOCK(LOCK_SCHED_CONF, LOCK_WRITE);
      store = Master_Sched_Config_List;
      Master_Sched_Config_List = config;
      SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_WRITE);
      
      ret = sconf_validate_config_(answer_list);
      
      SGE_LOCK(LOCK_SCHED_CONF, LOCK_WRITE);
      Master_Sched_Config_List = store;
      SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_WRITE);

      sconf_validate_config_(NULL);
   }
   
   DRETURN(ret);
}

/****** sgeobj/conf/policy_hierarchy_verify_value() ***************************
*  NAME
*     policy_hierarchy_verify_value() -- verify a policy string 
*
*  SYNOPSIS
*     int policy_hierarchy_verify_value(const char* value) 
*
*  FUNCTION
*     The function tests whether the given policy string (value) is i
*     valid. 
*
*  INPUTS
*     const char* value - policy string 
*
*  RESULT
*     int - 0 -> OK
*           1 -> ERROR: one char is at least twice in "value"
*           2 -> ERROR: invalid char in "value"
*           3 -> ERROR: value == NULL
*
*  MT-NOTE:  is MT safe, uses only the given data.
*
******************************************************************************/
static int policy_hierarchy_verify_value(const char* value) 
{
   int ret = 0;

   DENTER(TOP_LAYER, "policy_hierarchy_verify_value");

   if (value != NULL) {
      if (strcmp(value, "") && strcasecmp(value, "NONE")) {
         int is_contained[POLICY_VALUES]; 
         int i;

         for (i = 0; i < POLICY_VALUES; i++) {
            is_contained[i] = 0;
         }

         for (i = 0; i < strlen(value); i++) {
            char c = value[i];
            int index = policy_hierarchy_char2enum(c);

            if (is_contained[index]) {
               DPRINTF(("character \'%c\' is contained at least twice\n", c));
               ret = 1;
               break;
            } 

            is_contained[index] = 1;

            if (is_contained[INVALID_POLICY]) {
               DPRINTF(("Invalid character \'%c\'\n", c));
               ret = 2;
               break;
            }
         }
      }
   } 
   else {
      ret = 3;
   }

   DRETURN(ret);
}

/****** sgeobj/conf/sconf_ph_fill_array() *****************************
*  NAME
*     sconf_ph_fill_array() -- fill the policy array 
*
*  SYNOPSIS
*     void sconf_ph_fill_array(policy_hierarchy_t array[], 
*                                      const char *value) 
*
*  FUNCTION
*     Fill the policy "array" according to the characters given by 
*     "value".
*
*     value == "FODS":
*        policy_hierarchy_t array[4] = {
*            {FUNCTIONAL_POLICY, 1},
*            {OVERRIDE_POLICY, 1},
*            {DEADLINE_POLICY, 1},
*            {SHARE_TREE_POLICY, 1}
*        };
*
*     value == "FS":
*        policy_hierarchy_t array[4] = {
*            {FUNCTIONAL_POLICY, 1},
*            {SHARE_TREE_POLICY, 1},
*            {OVERRIDE_POLICY, 0},
*            {DEADLINE_POLICY, 0}
*        };
*
*     value == "OFS":
*        policy_hierarchy_t hierarchy[4] = {
*            {OVERRIDE_POLICY, 1},
*            {FUNCTIONAL_POLICY, 1},
*            {SHARE_TREE_POLICY, 1},
*            {DEADLINE_POLICY, 0}
*        }; 
*
*     value == "NONE":
*        policy_hierarchy_t hierarchy[4] = {
*            {OVERRIDE_POLICY, 0},
*            {FUNCTIONAL_POLICY, 0},
*            {SHARE_TREE_POLICY, 0},
*            {DEADLINE_POLICY, 0}
*        }; 
*
*  INPUTS
*     policy_hierarchy_t array[] - array with at least POLICY_VALUES 
*                                  values 
*     const char* value          - "NONE" or any combination
*                                  of the first letters of the policy 
*                                  names (e.g. "OFSD")
*
*  RESULT
*     "array" will be modified 
*
*  MT-NOTE:  is MT safe, uses LOCK_SCHED_CONF(read)
*
******************************************************************************/
void sconf_ph_fill_array(policy_hierarchy_t array[])
{
   int is_contained[POLICY_VALUES];
   int index = 0;
   int i;
   const char *policy_hierarchy_string = NULL;
   
   DENTER(TOP_LAYER, "sconf_ph_fill_array");

   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   policy_hierarchy_string = lGetPosString(lFirst(Master_Sched_Config_List), 
                                           pos.policy_hierarchy);
   
   for (i = 0; i < POLICY_VALUES; i++) {
      is_contained[i] = 0;
      array[i].policy = INVALID_POLICY;
   }     
   if (policy_hierarchy_string != NULL && strcmp(policy_hierarchy_string, "") && 
       strcasecmp(policy_hierarchy_string, "NONE")) {
      
      for (i = 0; i < strlen(policy_hierarchy_string); i++) {
         char c = policy_hierarchy_string[i];
         policy_type_t enum_value = policy_hierarchy_char2enum(c); 

         is_contained[enum_value] = 1;
         array[index].policy = enum_value;
         array[index].dependent = 1;
         index++;
      }
   }
   
   for (i = INVALID_POLICY + 1; i < LAST_POLICY_VALUE; i++) {
      if (!is_contained[i])  {
         array[index].policy = (policy_type_t)i;
         array[index].dependent = 0;
         index++;
      }
   }

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DEXIT;
}

/****** sgeobj/conf/policy_hierarchy_char2enum() ******************************
*  NAME
*     policy_hierarchy_char2enum() -- Return value for a policy char
*
*  SYNOPSIS
*     policy_type_t policy_hierarchy_char2enum(char character) 
*
*  FUNCTION
*     This function returns a enum value for the first letter of a 
*     policy name. 
*
*  INPUTS
*     char character - "O", "F" or "S"
*
*  RESULT
*     policy_type_t - enum value 
*
*  MT-NOTE:
*     not thread safe, needs LOCK_SCHED_CONF(read)
*
******************************************************************************/
static policy_type_t policy_hierarchy_char2enum(char character)
{
   const char *pointer;
   policy_type_t ret;
   
   pointer = strchr(policy_hierarchy_chars, character);
   if (pointer != NULL) {
      ret = (policy_type_t)((pointer - policy_hierarchy_chars) + 1);
   } else {
      ret = INVALID_POLICY;
   }
   return ret;
}


/****** sgeobj/conf/sconf_ph_print_array() ****************************
*  NAME
*     sconf_ph_print_array() -- print hierarchy array 
*
*  SYNOPSIS
*     void sconf_ph_print_array(policy_hierarchy_t array[]) 
*
*  FUNCTION
*     Print hierarchy array in the debug output 
*
*  INPUTS
*     policy_hierarchy_t array[] - array with at least 
*                                  POLICY_VALUES values 
*
*  MT-NOTE: is MT save, no lock needed, works on the passed in data
*
******************************************************************************/
void sconf_ph_print_array(policy_hierarchy_t array[])
{
   int i;

   DENTER(TOP_LAYER, "sconf_ph_print_array");
   
   for (i = INVALID_POLICY + 1; i < LAST_POLICY_VALUE; i++) {
      char character = policy_hierarchy_enum2char(array[i-1].policy);
      
      DPRINTF(("policy: %c; dependent: %d\n", character, array[i-1].dependent));
   }   

   DEXIT;
}

/****** sgeobj/conf/policy_hierarchy_enum2char() ******************************
*  NAME
*     policy_hierarchy_enum2char() -- Return policy char for a value 
*
*  SYNOPSIS
*     char policy_hierarchy_enum2char(policy_type_t value) 
*
*  FUNCTION
*     Returns the first letter of a policy name corresponding to the 
*     enum "value".
*
*  INPUTS
*     policy_type_t value - enum value 
*
*  RESULT
*     char - "O", "F", "S", "D"
*
*  MT-NOTE:
*     not thread safe, needs LOCK_SCHED_CONF(read)
*
******************************************************************************/
static char policy_hierarchy_enum2char(policy_type_t value) 
{
   return policy_hierarchy_chars[value - 1];
}

/****** sge_schedd_conf/sconf_eval_set_profiling() *****************************
*  NAME
*     sconf_eval_set_profiling() -- ??? 
*
*  SYNOPSIS
*     static bool sconf_eval_set_profiling(lList *param_list, lList 
*     **answer_list, const char* param) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     lList *param_list   - ??? 
*     lList **answer_list - ??? 
*     const char* param   - ??? 
*
*  RESULT
*     static bool - 
*
*  MT-NOTE:
*     not thread safe, needs LOCK_SCHED_CONF(write)
*
*******************************************************************************/
static bool sconf_eval_set_profiling(lList *param_list, lList **answer_list, const char* param){
   bool ret = true;
   lListElem *elem = NULL;
   DENTER(TOP_LAYER, "sconf_eval_set_profiling");

   schedd_profiling = false;

   if (!strncasecmp(param, "PROFILE=1", sizeof("PROFILE=1")-1) || 
       !strncasecmp(param, "PROFILE=TRUE", sizeof("PROFILE=FALSE")-1) ) {
      schedd_profiling = true;
      elem = lCreateElem(PARA_Type);
      lSetString(elem, PARA_name, "profile");
      lSetString(elem, PARA_value, "true");
   }      
   else if (!strncasecmp(param, "PROFILE=0", sizeof("PROFILE=1")-1) ||
            !strncasecmp(param, "PROFILE=FALSE", sizeof("PROFILE=FALSE")-1) ) {
      elem = lCreateElem(PARA_Type);
      lSetString(elem, PARA_name, "profile");
      lSetString(elem, PARA_value, "false");
   }
   else {
      SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_INVALID_PARAM_SETTING_S, param)); 
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }
   if (elem){
      lAppendElem(param_list, elem);
   }

   DRETURN(ret);
}

/****** sge_schedd_conf/sconf_eval_set_job_category_filtering() ****************
*  NAME
*     sconf_eval_set_job_category_filtering() -- enable jc filtering or not.
*
*  SYNOPSIS
*     static bool sconf_eval_set_job_category_filtering(lList *param_list, 
*     lList **answer_list, const char* param) 
*
*  FUNCTION
*     A parsing function for the prama settings in the scheduler configuration.
*     It is looking for JC_FILTER to be set to true, or false and updates the
*     settings accordingly.
*
*  INPUTS
*     lList *param_list   - the param list
*     lList **answer_list - the answer list, in case of an error
*     const char* param   - the param string
*
*  RESULT
*     static bool - true, if everything went fine
*
*     MT-NOTE: is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(write)
*
*******************************************************************************/
static bool sconf_eval_set_job_category_filtering(lList *param_list, lList **answer_list, const char* param){
   bool ret = true;
   lListElem *elem = NULL;
   
   DENTER(TOP_LAYER, "sconf_eval_set_job_category_filtering");

   is_category_job_filtering= false;

   if (!strncasecmp(param, "JC_FILTER=1", sizeof("JC_FILTER=1")-1) || 
       !strncasecmp(param, "JC_FILTER=TRUE", sizeof("JC_FILTER=FALSE")-1) ) {
      is_category_job_filtering= true;
      elem = lCreateElem(PARA_Type);
      lSetString(elem, PARA_name, "jc_filter");
      lSetString(elem, PARA_value, "true");
   }      
   else if (!strncasecmp(param, "JC_FILTER=0", sizeof("JC_FILTER=1")-1) ||
            !strncasecmp(param, "JC_FILTER=FALSE", sizeof("JC_FILTER=FALSE")-1) ) {
      elem = lCreateElem(PARA_Type);
      lSetString(elem, PARA_name, "jc_filter");
      lSetString(elem, PARA_value, "false");
   }
   else {
      SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_INVALID_PARAM_SETTING_S, param)); 
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }
   if (elem){
      lAppendElem(param_list, elem);
   }

   DRETURN(ret);
}


/****** sge_schedd_conf/sconf_eval_set_monitoring() ****************************
*  NAME
*     sconf_eval_set_monitoring() -- Control SERF on/off via MONITOR param 
*
*  SYNOPSIS
*     static bool sconf_eval_set_monitoring(lList *param_list, lList 
*     **answer_list, const char* param) 
*
*  FUNCTION
*     The MONITOR param allows schedule entry recording facility module 
*     be switched on/off. 
*
*  INPUTS
*     lList *param_list   - ??? 
*     lList **answer_list - ??? 
*     const char* param   - ??? 
*
*  RESULT
*     static bool - parsing error
*
*  NOTES
*     MT-NOTE: is not MT safe, the calling function needs to lock LOCK_SCHED_CONF(write)
*
*******************************************************************************/
static bool sconf_eval_set_monitoring(lList *param_list, lList **answer_list, const char* param){
   bool ret = true;
   lListElem *elem = NULL;
   const char mon_true[] = "MONITOR=TRUE", mon_one[] = "MONITOR=1";
   const char mon_false[] = "MONITOR=FALSE", mon_zero[] = "MONITOR=0";
   bool do_monitoring = false;

   DENTER(TOP_LAYER, "sconf_eval_set_monitoring");

   if (!strncasecmp(param, mon_one, sizeof(mon_one)-1) || 
       !strncasecmp(param, mon_true, sizeof(mon_true)-1) ) {
      do_monitoring = true;
      elem = lCreateElem(PARA_Type);
      lSetString(elem, PARA_name, "monitor");
      lSetString(elem, PARA_value, "true");
   }      
   else if (!strncasecmp(param, mon_zero, sizeof(mon_zero)-1) ||
            !strncasecmp(param, mon_false, sizeof(mon_false)-1) ) {
      elem = lCreateElem(PARA_Type);
      lSetString(elem, PARA_name, "monitor");
      lSetString(elem, PARA_value, "false");
   }
   else {
      SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_INVALID_PARAM_SETTING_S, param)); 
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }
   if (elem){
      lAppendElem(param_list, elem);
   }

   current_serf_do_monitoring = do_monitoring;

   DRETURN(ret);
}

static bool sconf_eval_set_duration_offset(lList *param_list, lList **answer_list, const char* param)
{
   u_long32 uval;
   char *s;

   DENTER(TOP_LAYER, "sconf_eval_set_monitoring");

   if (!(s=strchr(param, '=')) ||
       !extended_parse_ulong_val(NULL, &uval, TYPE_TIM, ++s, NULL, 0, 0)) {
      pos.s_duration_offset = DEFAULT_DURATION_OFFSET; 
      DEXIT;
      return false;
   }
   pos.s_duration_offset = DEFAULT_DURATION_OFFSET;

   DEXIT;
   return true;
}


/* 
   QS_STATE_FULL
      All debitations caused by running jobs are in effect.
   QS_STATE_EMPTY
      We ignore all debitations caused by running jobs.
      Ignore all but static load values.
*/
void sconf_set_qs_state(qs_state_t qs_state) 
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key, "sconf_set_qs_state");
   sc_state->queue_state = qs_state;
}

qs_state_t sconf_get_qs_state(void) 
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key, "sconf_get_qs_state");
   return sc_state->queue_state;
}
void sconf_set_global_load_correction(bool flag) 
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key, "sconf_set_global_load_correction");
   sc_state->global_load_correction = flag;
}
bool sconf_get_global_load_correction(void) 
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key, "sconf_get_global_load_correction");
   return sc_state->global_load_correction;
}

u_long32 sconf_get_default_duration(void) 
{
   return pos.c_default_duration;
}

void sconf_set_now(u_long32 now)
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key, "sconf_set_now");
   sc_state->now = now;
}

u_long32 sconf_get_now(void)
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key, "sconf_get_now");
   return sc_state->now;
}

bool sconf_get_host_order_changed(void)
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key, "sconf_get_host_order_changed");
   return sc_state->host_order_changed;
}

void sconf_set_host_order_changed(bool changed)
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key, "sconf_set_host_order_changed");
   sc_state->host_order_changed = changed;
}

int sconf_get_last_dispatch_type(void)
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key, "sconf_get_last_dispatch_type");
   return sc_state->last_dispatch_type;
}

void sconf_set_last_dispatch_type(int last)
{
   GET_SPECIFIC(sc_state_t, sc_state, sc_state_init, sc_state_key, "sconf_set_last_dispatch_type");
   sc_state->last_dispatch_type = last;
}

u_long32 sconf_get_duration_offset(void)
{
   u_long32 offset = 0;

   DENTER(TOP_LAYER, "sconf_get_duration_offset");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   offset = pos.s_duration_offset;

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(offset);
}

/****** sge_resource_utilization/serf_control() ********************************
*  NAME
*     serf_get_active() -- Retrieve whether SERF is active or not
*
*  SYNOPSIS
*     bool serf_get_active(void);
*
*  FUNCTION
*     Returns whether SERF is active or not
*
*  RETURN
*     bool - true = on 
*            false = off
*
*  MT-NOTE: is MT safe, uses LOCK_SCHED_CONF(read)
*
*  NOTES
*     Actually belongs to sge_serf.c but this would cause a link dependency 
*     libsgeobj -> libschedd !! 
*******************************************************************************/
bool serf_get_active(void)
{
   bool is = false;

   DENTER(TOP_LAYER, "serf_get_active");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   is = current_serf_do_monitoring;

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(is);
}

bool sconf_get_profiling(void)
{
   bool profiling = false;

   DENTER(TOP_LAYER, "sconf_get_profiling");
   SGE_LOCK(LOCK_SCHED_CONF, LOCK_READ);
   
   profiling = schedd_profiling;

   SGE_UNLOCK(LOCK_SCHED_CONF, LOCK_READ);
   DRETURN(profiling);
}
