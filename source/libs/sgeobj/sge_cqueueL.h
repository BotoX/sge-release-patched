#ifndef __SGE_CQUEUEL_H
#define __SGE_CQUEUEL_H

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

#include "sge_boundaries.h"
#include "cull.h"

#ifdef  __cplusplus
extern "C" {
#endif

enum {
   CQ_name = CQ_LOWERBOUND,
   CQ_hostlist,
   CQ_qinstances,

   CQ_seq_no,
   CQ_nsuspend,
   CQ_job_slots,

   CQ_rerun,

   CQ_s_fsize,
   CQ_h_fsize,
   CQ_s_data,
   CQ_h_data,
   CQ_s_stack,
   CQ_h_stack,
   CQ_s_core,
   CQ_h_core,
   CQ_s_rss,
   CQ_h_rss,
   CQ_s_vmem,
   CQ_h_vmem,

   CQ_s_rt,
   CQ_h_rt,
   CQ_s_cpu,
   CQ_h_cpu,

   CQ_suspend_interval,
   CQ_min_cpu_interval,
   CQ_notify,

   CQ_tmpdir,
   CQ_shell,
   CQ_calendar,
   CQ_priority,
   CQ_processors,
   CQ_prolog,
   CQ_epilog,
   CQ_shell_start_mode,
   CQ_starter_method,
   CQ_suspend_method,
   CQ_resume_method,
   CQ_terminate_method,
   CQ_initial_state,

   CQ_pe_list,
   CQ_ckpt_list,

   CQ_owner_list,
   CQ_acl,
   CQ_xacl,

   CQ_projects,
   CQ_xprojects,

   CQ_load_thresholds,
   CQ_suspend_thresholds,
   CQ_consumable_config_list,

   CQ_subordinate_list,

   CQ_qtype,
   CQ_tag
};

LISTDEF(CQ_Type)
   JGDI_ROOT_OBJ(ClusterQueue, SGE_CQUEUE_LIST, ADD | MODIFY | DELETE | GET | GET_LIST)
   JGDI_EVENT_OBJ(ADD(sgeE_CQUEUE_ADD) | MODIFY(sgeE_CQUEUE_MOD) | DELETE(sgeE_CQUEUE_DEL) | GET_LIST(sgeE_CQUEUE_LIST))
   SGE_STRING(CQ_name, CULL_HASH | CULL_UNIQUE | CULL_SPOOL | CULL_CONFIGURE | CULL_PRIMARY_KEY)
   SGE_LIST(CQ_hostlist, HR_Type, CULL_SPOOL | CULL_CONFIGURE)
   SGE_LIST(CQ_qinstances, QU_Type, CULL_CONFIGURE | CULL_JGDI_RO)

   SGE_MAP_D(CQ_seq_no, AULNG_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "seqNo") 
   SGE_MAP_D(CQ_nsuspend, AULNG_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 1, "hostname", "nsuspend")
   SGE_MAP_D(CQ_job_slots, AULNG_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 1, "hostname", "jobSlots")

   SGE_MAP_D(CQ_rerun, ABOOL_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "rerun")

   SGE_MAP_D(CQ_s_fsize, AMEM_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "sFSize")
   SGE_MAP_D(CQ_h_fsize, AMEM_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "hFSize")
   SGE_MAP_D(CQ_s_data, AMEM_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "sData")
   SGE_MAP_D(CQ_h_data, AMEM_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "hData")
   SGE_MAP_D(CQ_s_stack, AMEM_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "sStack")
   SGE_MAP_D(CQ_h_stack, AMEM_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "hStack")
   SGE_MAP_D(CQ_s_core, AMEM_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "sCore")
   SGE_MAP_D(CQ_h_core, AMEM_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "hCore") 
   SGE_MAP_D(CQ_s_rss, AMEM_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "sRss") 
   SGE_MAP_D(CQ_h_rss, AMEM_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "hRss")  
   SGE_MAP_D(CQ_s_vmem, AMEM_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "sVmem") 
   SGE_MAP_D(CQ_h_vmem, AMEM_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "hVmem")

   SGE_MAP_D(CQ_s_rt, ATIME_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "sRt")
   SGE_MAP_D(CQ_h_rt, ATIME_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "hRt")
   SGE_MAP_D(CQ_s_cpu, ATIME_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "sCpu")
   SGE_MAP_D(CQ_h_cpu, ATIME_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "hCpu")   

   SGE_MAP_D(CQ_suspend_interval, AINTER_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "suspendInterval")
   SGE_MAP_D(CQ_min_cpu_interval, AINTER_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "minCpuInterval") 
   SGE_MAP_D(CQ_notify, AINTER_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "notify")

   SGE_MAP_D(CQ_tmpdir, ASTR_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "tmpDir")
   SGE_MAP_D(CQ_shell, ASTR_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "shell")
   SGE_MAP_D(CQ_calendar, ASTR_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "calendar")
   SGE_MAP_D(CQ_priority, ASTR_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "0", "hostname", "priority")
   SGE_MAP_D(CQ_processors, ASTR_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "1", "hostname", "processors")
   SGE_MAP_D(CQ_prolog, ASTR_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "prolog")  
   SGE_MAP_D(CQ_epilog, ASTR_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "epilog") 
   SGE_MAP_D(CQ_shell_start_mode, ASTR_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "posix_compliant", "hostname", "shellStartMode")
   SGE_MAP_D(CQ_starter_method, ASTR_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "starterMethod")  
   SGE_MAP_D(CQ_suspend_method, ASTR_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "suspendMethod") 
   SGE_MAP_D(CQ_resume_method, ASTR_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "resumeMethod") 
   SGE_MAP_D(CQ_terminate_method, ASTR_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "terminateMethod")
   SGE_MAP_D(CQ_initial_state, ASTR_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "default", "hostname", "initialState") 

   SGE_MAPLIST_D(CQ_pe_list, ASTRLIST_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "pe")
   SGE_MAPLIST_D(CQ_ckpt_list, ASTRLIST_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "checkpoint")

   SGE_MAPLIST_D(CQ_owner_list, AUSRLIST_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "owner")
   SGE_MAPLIST_D(CQ_acl, AUSRLIST_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "acl")   
   SGE_MAPLIST_D(CQ_xacl, AUSRLIST_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "acl")

   SGE_MAPLIST_D(CQ_projects, APRJLIST_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "project")       
   SGE_MAPLIST_D(CQ_xprojects, APRJLIST_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "project")    

   SGE_MAPLIST_D(CQ_load_thresholds, ACELIST_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "loadThreshold") 
   SGE_MAPLIST_D(CQ_suspend_thresholds, ACELIST_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "suspendThreshold")
   SGE_MAPLIST_D(CQ_consumable_config_list, ACELIST_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "consumable") 
   SGE_MAPLIST_D(CQ_subordinate_list, ASOLIST_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", "NONE", "hostname", "subordinate")

   SGE_MAP_D(CQ_qtype, AQTLIST_Type, CULL_SPOOL | CULL_CONFIGURE, "@/", 0, "hostname", "qType")  

   SGE_ULONG(CQ_tag, CULL_CONFIGURE)
LISTEND 

NAMEDEF(CQN)
   NAME("CQ_name")
   NAME("CQ_hostlist")
   NAME("CQ_qinstances")

   NAME("CQ_seq_no")
   NAME("CQ_nsuspend")
   NAME("CQ_job_slots")

   NAME("CQ_rerun")

   NAME("CQ_s_fsize")
   NAME("CQ_h_fsize")
   NAME("CQ_s_data")
   NAME("CQ_h_data")
   NAME("CQ_s_stack")
   NAME("CQ_h_stack")
   NAME("CQ_s_core")
   NAME("CQ_h_core")
   NAME("CQ_s_rss")
   NAME("CQ_h_rss")
   NAME("CQ_s_vmem")
   NAME("CQ_h_vmem")

   NAME("CQ_s_rt")
   NAME("CQ_h_rt")
   NAME("CQ_s_cpu")
   NAME("CQ_h_cpu")

   NAME("CQ_suspend_interval")
   NAME("CQ_min_cpu_interval")
   NAME("CQ_notify")

   NAME("CQ_tmpdir")
   NAME("CQ_shell")
   NAME("CQ_calendar")
   NAME("CQ_priority")
   NAME("CQ_processors")
   NAME("CQ_prolog")
   NAME("CQ_epilog")
   NAME("CQ_shell_start_mode")
   NAME("CQ_starter_method")
   NAME("CQ_suspend_method")
   NAME("CQ_resume_method")
   NAME("CQ_terminate_method")
   NAME("CQ_initial_state")

   NAME("CQ_pe_list")
   NAME("CQ_ckpt_list")

   NAME("CQ_owner_list")
   NAME("CQ_acl")
   NAME("CQ_xacl")

   NAME("CQ_projects")
   NAME("CQ_xprojects")

   NAME("CQ_load_thresholds")
   NAME("CQ_suspend_thresholds")
   NAME("CQ_consumable_config_list")
   NAME("CQ_subordinate_list")

   NAME("CQ_qtype")
   NAME("CQ_tag")
NAMEEND

#define CQS sizeof(CQN)/sizeof(char*)

/* *INDENT-ON* */ 

#ifdef  __cplusplus
}
#endif

#endif /* __SGE_CQUEUEL_H */
