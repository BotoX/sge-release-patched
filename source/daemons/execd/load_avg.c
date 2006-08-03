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
#include <string.h>
#include <float.h>

#include "sge_ja_task.h"
#include "sge_pe_task.h"
#include "sge_usageL.h"
#include "sge_time.h"
#include "sge_prog.h"
#include "commlib.h"

#include "sge_gdiP.h"
#include "job_report_execd.h"
#include "sge_host.h"
#include "sge_load_sensor.h"
#include "load_avg.h"
#include "execd_ck_to_do.h"
#include "sge_report_execd.h"
#include "sgermon.h"
#include "sge_log.h"
#include "sge_conf.h"
#include "sge_parse_num_par.h"
#include "msg_execd.h"
#include "sge_string.h"
#include "sge_feature.h"
#include "sge_uidgid.h"
#include "sge_hostname.h"
#include "sge_os.h"
#include "sge_job.h"
#include "sge_qinstance.h"
#include "sge_pe.h"
#include "sge_report.h"
#include "sgeobj/sge_object.h"

#ifdef COMPILE_DC
#  include "ptf.h"
#endif

static void 
get_reserved_usage(lList **job_usage_list);
static int 
execd_add_load_report(lList *report_list, u_long32 now, u_long32 *next_send);
static int 
execd_add_conf_report(lList *report_list, u_long32 now, u_long32 *next_send);
static int 
execd_add_license_report(lList *report_list, u_long32 now, u_long32 *next_send);
static int 
execd_add_job_report(lList *report_list, u_long32 now, u_long32 *next_send);
static int 
sge_get_loadavg(lList **lpp);

report_source execd_report_sources[] = {
   { NUM_REP_REPORT_LOAD, execd_add_load_report , 0 },
   { NUM_REP_REPORT_CONF, execd_add_conf_report , 0 },
   { NUM_REP_REPORT_PROCESSORS, execd_add_license_report , 0 },
   { NUM_REP_REPORT_JOB, execd_add_job_report , 0 },
   { 0, NULL }
};

lUlong sge_execd_report_seqno = 0;

extern lList *jr_list;

static int 
execd_add_load_report(lList *report_list, u_long32 now, u_long32 *next_send) 
{
   if (*next_send <= now) {
      lListElem *report;

      *next_send = now + mconf_get_load_report_time();

      /*
      ** problem: add some more error handling here
      */
      /*
      ** 1. load report
      */
      report = lCreateElem(REP_Type);
      lSetUlong(report, REP_type, NUM_REP_REPORT_LOAD);
      lSetUlong(report, REP_version, GRM_GDI_VERSION);
      lSetUlong(report, REP_seqno, sge_execd_report_seqno);
      lSetHost(report, REP_host, uti_state_get_qualified_hostname());
      lSetList(report, REP_list, sge_build_load_report());
      lAppendElem(report_list, report);
   }

   return 0;
}


static int 
execd_add_conf_report(lList *report_list, u_long32 now, u_long32 *next_send) 
{
   if (*next_send <= now) {
      lListElem *report;

      *next_send = now + mconf_get_load_report_time();

      /*
      ** 2. report about the configuration versions
      ** that the exec daemon has right now
      ** order of the reports is irrelevant
      */
      report = lCreateElem(REP_Type);
      lSetUlong(report, REP_type, NUM_REP_REPORT_CONF);
      lSetUlong(report, REP_version, GRM_GDI_VERSION);
      lSetUlong(report, REP_seqno, sge_execd_report_seqno);
      lSetHost(report, REP_host, uti_state_get_qualified_hostname());
      lSetList(report, REP_list, 
         lCopyList("execd config list copy", Execd_Config_List));
      lAppendElem(report_list, report);
   }

   return 0;
}

static int 
execd_add_license_report(lList *report_list, u_long32 now, u_long32 *next_send) 
{
   if (*next_send <= now) {
      lListElem *report;

      *next_send = now + mconf_get_load_report_time();

      /*
      ** 3. license report
      */
      report = lCreateElem(REP_Type);
      lSetUlong(report, REP_type, NUM_REP_REPORT_PROCESSORS);
      lSetUlong(report, REP_version, GRM_GDI_VERSION);
      lSetUlong(report, REP_seqno, sge_execd_report_seqno);
      lSetHost(report, REP_host, uti_state_get_qualified_hostname());
      {
         lList *lp_lic;
         lListElem *ep_lic;
         
         lp_lic = lCreateList("license report list", LIC_Type);
         ep_lic = lCreateElem(LIC_Type);
         /*
         ** no error handling here cause we can just send 1 as #processors
         */
         lSetUlong(ep_lic, LIC_processors,  sge_nprocs());
         lSetString(ep_lic, LIC_arch, sge_get_arch());
         lAppendElem(lp_lic, ep_lic);
         lSetList(report, REP_list, lp_lic);
      }
      lAppendElem(report_list, report);
   }

   return 0;
}

static int 
execd_add_job_report(lList *report_list, u_long32 now, u_long32 *next_send) 
{
   bool do_send = false;
   bool only_flush = false;
   static u_long32 last_send = 0;

   /* if report interval expired: send all reports */
   if (*next_send <= now) {
      *next_send = now + mconf_get_load_report_time();
      do_send = true;
   } else {
      /* if we shall flush reports: send only reports marked to flush */
      if (sge_get_flush_jr_flag() == true) {
         do_send = true;
         only_flush = true;
      }
   }

   /* 
    * send only one report message per second 
    * we do send empty reports - they trigger rescheduling of a job
    * to the same host on which it executed before.
    */
   if (do_send && (last_send < now)) {
      lListElem *job_report;
      lList *job_report_list;
      lListElem *jr;

      /* remember last send time */
      last_send = now;

      /* create job report */
      job_report = lCreateElem(REP_Type);
      lSetUlong(job_report, REP_type, NUM_REP_REPORT_JOB);
      lSetUlong(job_report, REP_version, GRM_GDI_VERSION);
      lSetUlong(job_report, REP_seqno, sge_execd_report_seqno);
      lSetHost(job_report, REP_host, uti_state_get_qualified_hostname());

      /* create job report list */
      job_report_list = lCreateList("jr", JR_Type);
      lSetList(job_report, REP_list, job_report_list);

      /* copy reports (all or only to flush) */
      for_each (jr, jr_list) {
         if (!only_flush || lGetBool(jr, JR_flush)) {
            lAppendElem(job_report_list, lCopyElem(jr));
            lSetBool(jr, JR_flush, false);
         }
      }
     
      /* append this new job report to the report list */
      lAppendElem(report_list, job_report);

      /* now all is sent, reset flush_jr */
      sge_set_flush_jr_flag(false);
   }

   return 0;
}

lList *sge_build_load_report(void)
{
   lList *lp = NULL;
   lListElem *ep;
   int nprocs = 1;
   double load;
   const char *s;
   const void *iterator = NULL;
#if defined(NECSX4) || defined(NECSX5)
   char lv_name[256];
   int rsg_id;
#endif
 

   DENTER(TOP_LAYER, "sge_build_load_report");

   /* 
      adding load values to the load report 
      overwrites load values that are still
      in the load list; 
      so we first validate external load values
      then we get internal load values 
      overwriting the external values

   */

   /* build up internal report list */
   sge_switch2start_user();
   sge_get_loadavg(&lp);
   sge_switch2admin_user(); 

   /* get load report from external load sensor */
   sge_ls_get(&lp);

   /* make derived load values */
   /* retrieve num procs first - we need it for all other derived load values */
   ep = lGetElemStrFirst(lp, LR_name, LOAD_ATTR_NUM_PROC, &iterator);
   while(ep != NULL) {
      if ((sge_hostcmp(lGetHost(ep, LR_host), uti_state_get_qualified_hostname()) == 0) && (s = lGetString(ep, LR_value))) {
         nprocs = MAX(1, atoi(s));
         break;
      }   
      ep = lGetElemStrNext(lp, LR_name, LOAD_ATTR_NUM_PROC, &iterator);
   }

   /* now make the derived load values */
   ep = lGetElemHostFirst(lp, LR_host, uti_state_get_qualified_hostname(), &iterator);
   while(ep != NULL) {
      if ((strcmp(lGetString(ep, LR_name), LOAD_ATTR_LOAD_AVG) == 0) && (s = lGetString(ep, LR_value))) {
         load = strtod(s, NULL);
         sge_add_double2load_report(&lp, LOAD_ATTR_NP_LOAD_AVG, (load/nprocs), uti_state_get_qualified_hostname(), NULL);
      }

      if ((strcmp(lGetString(ep, LR_name), LOAD_ATTR_LOAD_SHORT) == 0) && (s = lGetString(ep, LR_value))) {
         load = strtod(s, NULL);
         sge_add_double2load_report(&lp, LOAD_ATTR_NP_LOAD_SHORT, (load/nprocs), 
                                       uti_state_get_qualified_hostname(), NULL);
      }
      if ((strcmp(lGetString(ep, LR_name), LOAD_ATTR_LOAD_MEDIUM) == 0) && (s = lGetString(ep, LR_value))) {
         load = strtod(s, NULL);
         sge_add_double2load_report(&lp, LOAD_ATTR_NP_LOAD_MEDIUM, (load/nprocs), 
                                       uti_state_get_qualified_hostname(), NULL);
      }
      if ((strcmp(lGetString(ep, LR_name), LOAD_ATTR_LOAD_LONG) == 0) && (s = lGetString(ep, LR_value))) {
         load = strtod(s, NULL);
         sge_add_double2load_report(&lp, LOAD_ATTR_NP_LOAD_LONG, (load/nprocs), 
                                       uti_state_get_qualified_hostname(), NULL);
      }

   #if defined(NECSX4) || defined(NECSX5)
      /* make derived load values for NEC*/

      for (rsg_id = 0; rsg_id < 32; rsg_id++) {
         sprintf(lv_name, "rsg%d_%s", rsg_id, LOAD_ATTR_NUM_PROC);
         if ((strcmp(lGetString(ep, LR_name), lv_name) == 0) && (s = lGetString(ep, LR_value)))
            nprocs = MAX(1, atoi(s));

         sprintf(lv_name, "rsg%d_%s", rsg_id, LOAD_ATTR_LOAD_AVG);
         if ((strcmp(lGetString(ep, LR_name), lv_name) == 0) && (s = lGetString(ep, LR_value))) {
            load = strtod(s, NULL);
            sprintf(lv_name, "rsg%d_%s", rsg_id, LOAD_ATTR_NP_LOAD_AVG);
            sge_add_double2load_report(&lp, lv_name, (load/nprocs), uti_state_get_qualified_hostname(), NULL);
         }

         sprintf(lv_name, "rsg%d_%s", rsg_id, LOAD_ATTR_LOAD_SHORT);
         if ((strcmp(lGetString(ep, LR_name), lv_name) == 0) && (s = lGetString(ep, LR_value))) {
            load = strtod(s, NULL);
            sprintf(lv_name, "rsg%d_%s", rsg_id, LOAD_ATTR_NP_LOAD_SHORT);
            sge_add_double2load_report(&lp, lv_name, (load/nprocs), uti_state_get_qualified_hostname(), NULL);
         }

         sprintf(lv_name, "rsg%d_%s", rsg_id, LOAD_ATTR_LOAD_MEDIUM);
         if ((strcmp(lGetString(ep, LR_name), lv_name) == 0) && (s = lGetString(ep, LR_value))) {
            load = strtod(s, NULL);
            sprintf(lv_name, "rsg%d_%s", rsg_id, LOAD_ATTR_NP_LOAD_MEDIUM);
            sge_add_double2load_report(&lp, lv_name, (load/nprocs), uti_state_get_qualified_hostname(), NULL);
         }

         sprintf(lv_name, "rsg%d_%s", rsg_id, LOAD_ATTR_LOAD_LONG);
         if ((strcmp(lGetString(ep, LR_name), lv_name) == 0) && (s = lGetString(ep, LR_value))) {
            load = strtod(s, NULL);
            sprintf(lv_name, "rsg%d_%s", rsg_id, LOAD_ATTR_NP_LOAD_LONG);
            sge_add_double2load_report(&lp, lv_name, (load/nprocs), uti_state_get_qualified_hostname(), NULL);
         }
      }
#endif
      ep = lGetElemHostNext(lp, LR_host, uti_state_get_qualified_hostname(), &iterator);
   }

   /* qmaster expects a list sorted by host */
   lPSortList(lp, "%I+", LR_host);

   DEXIT;
   return lp;
}

static int sge_get_loadavg(lList **lpp) 
{
   double avg[3];
   int loads;
   int nprocs; 
#ifdef SGE_LOADMEM
#  if NECSX4 || NECSX5
   int rsg_id;
#  endif
   sge_mem_info_t mem_info;
#endif

   DENTER(TOP_LAYER, "sge_get_loadavg");

   loads = sge_getloadavg(avg, 3);
   nprocs = sge_nprocs();
#ifndef INTERIX
   if (loads == -1) {
      static u_long32 next_log = 0;
      u_long32 now;

      avg[0] = avg[1] = avg[2] = 0.0;

      now = sge_get_gmt();
      if (now >= next_log) {
         WARNING((SGE_EVENT, MSG_SGETEXT_NO_LOAD));     
         next_log = now + 7200;
      }
   }
#endif

   /* build a list of load values */
   if (loads != -1) {
      DPRINTF(("---> %f %f %f - %d\n", avg[0], avg[1], avg[2], 
         (int) (avg[2] * 100.0)));

      sge_add_double2load_report(lpp, LOAD_ATTR_LOAD_AVG, avg[1], uti_state_get_qualified_hostname(), 
         NULL);
      sge_add_double2load_report(lpp, LOAD_ATTR_LOAD_SHORT, avg[0], uti_state_get_qualified_hostname(), 
         NULL);
      sge_add_double2load_report(lpp, LOAD_ATTR_LOAD_MEDIUM, avg[1], uti_state_get_qualified_hostname(), 
         NULL);
      sge_add_double2load_report(lpp, LOAD_ATTR_LOAD_LONG, avg[2], uti_state_get_qualified_hostname(), 
         NULL);
   }

   /* these are some static load values */
   sge_add_str2load_report(lpp, LOAD_ATTR_ARCH, sge_get_arch(), uti_state_get_qualified_hostname());
   sge_add_int2load_report(lpp, LOAD_ATTR_NUM_PROC, nprocs, uti_state_get_qualified_hostname());

#if defined(NECSX4) || defined(NECSX5)
   /* Write load values for each resource sharing group */
   for (rsg_id=0; rsg_id<32; rsg_id++) {
      double loadv[3];
      char lv_name[256];

      memset(loadv, 0, 3*sizeof(double));
      if (getloadavg_necsx_rsg(rsg_id, loadv) != -1) {
         sprintf(lv_name, "rsg%d_%s", rsg_id, LOAD_ATTR_LOAD_AVG);
         sge_add_double2load_report(lpp, lv_name, loadv[1], uti_state_get_qualified_hostname(), NULL);
         sprintf(lv_name, "rsg%d_%s", rsg_id, LOAD_ATTR_LOAD_SHORT);
         sge_add_double2load_report(lpp, lv_name, loadv[0], uti_state_get_qualified_hostname(), NULL);
         sprintf(lv_name, "rsg%d_%s", rsg_id, LOAD_ATTR_LOAD_MEDIUM);
         sge_add_double2load_report(lpp, lv_name, loadv[1], uti_state_get_qualified_hostname(), NULL);
         sprintf(lv_name, "rsg%d_%s", rsg_id, LOAD_ATTR_LOAD_LONG);
         sge_add_double2load_report(lpp, lv_name, loadv[2], uti_state_get_qualified_hostname(), NULL);
         sprintf(lv_name, "rsg%d_%s", rsg_id, LOAD_ATTR_NUM_PROC);
         sge_add_int2load_report(lpp, lv_name, sge_nprocs_rsg(rsg_id), uti_state_get_qualified_hostname());
      }
   }
#endif


#ifdef SGE_LOADMEM
   /* memory load report */
   memset(&mem_info, 0, sizeof(sge_mem_info_t));
   if (sge_loadmem(&mem_info)) {
      static int mem_fail = 0;
      if (!mem_fail) {
         ERROR((SGE_EVENT, MSG_LOAD_NOMEMINDICES));
         mem_fail =1;
      }
      DEXIT;
      return 1;
   }

   sge_add_double2load_report(lpp, LOAD_ATTR_MEM_FREE,        mem_info.mem_free, uti_state_get_qualified_hostname(), "M");
   sge_add_double2load_report(lpp, LOAD_ATTR_SWAP_FREE,       mem_info.swap_free, uti_state_get_qualified_hostname(), "M");
   sge_add_double2load_report(lpp, LOAD_ATTR_VIRTUAL_FREE,    mem_info.mem_free  + mem_info.swap_free,
   uti_state_get_qualified_hostname(),"M");

   sge_add_double2load_report(lpp, LOAD_ATTR_MEM_TOTAL,       mem_info.mem_total, uti_state_get_qualified_hostname(), "M");
   sge_add_double2load_report(lpp, LOAD_ATTR_SWAP_TOTAL,      mem_info.swap_total, uti_state_get_qualified_hostname(), "M");
   sge_add_double2load_report(lpp, LOAD_ATTR_VIRTUAL_TOTAL,   mem_info.mem_total + mem_info.swap_total,
   uti_state_get_qualified_hostname(), "M");

   sge_add_double2load_report(lpp, LOAD_ATTR_MEM_USED,        mem_info.mem_total - mem_info.mem_free, uti_state_get_qualified_hostname(), "M");
   sge_add_double2load_report(lpp, LOAD_ATTR_SWAP_USED,       mem_info.swap_total - mem_info.swap_free,
   uti_state_get_qualified_hostname(), "M");
   sge_add_double2load_report(lpp, LOAD_ATTR_VIRTUAL_USED,    (mem_info.mem_total + mem_info.swap_total)- 
                                          (mem_info.mem_free  + mem_info.swap_free), uti_state_get_qualified_hostname(), "M");

#ifdef IRIX
   sge_add_double2load_report(lpp, LOAD_ATTR_SWAP_RSVD,        mem_info.swap_rsvd, uti_state_get_qualified_hostname(), "M");
#endif

#if defined(NECSX4) || defined(NECSX5)
   for (rsg_id=0; rsg_id<32; rsg_id++) {
      sge_mem_info_t mem_i_l, mem_i_s;
      int num_proc;
      char lv_name[256];

      memset(&mem_i_l, 0, sizeof(sge_mem_info_t));
      memset(&mem_i_s, 0, sizeof(sge_mem_info_t));
      if (loadmem_rsg(rsg_id, &mem_i_l, &mem_i_s) != -1) {
         sprintf(lv_name, "rsg%d_l_mem_free", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_l.mem_free, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_l_swap_free", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_l.swap_free, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_l_virtual_free", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_l.mem_free + mem_i_l.swap_free, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_l_mem_total", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_l.mem_total, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_l_swap_total", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_l.swap_total, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_l_virtual_total", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_l.mem_total + mem_i_l.swap_total, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_l_mem_used", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_l.mem_total - mem_i_l.mem_free, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_l_swap_used", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_l.swap_total - mem_i_l.swap_free, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_l_virtual_used", rsg_id);
         sge_add_double2load_report(lpp, lv_name, (mem_i_l.mem_total + mem_i_l.swap_total)-
            (mem_i_l.mem_free + mem_i_l.swap_free), uti_state_get_qualified_hostname(), "M");

         sprintf(lv_name, "rsg%d_s_mem_free", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_s.mem_free, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_s_swap_free", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_s.swap_free, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_s_virtual_free", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_s.mem_free + mem_i_s.swap_free, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_s_mem_total", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_s.mem_total, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_s_swap_total", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_s.swap_total, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_s_virtual_total", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_s.mem_total + mem_i_s.swap_total, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_s_mem_used", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_s.mem_total - mem_i_s.mem_free, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_s_swap_used", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_s.swap_total - mem_i_s.swap_free, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_s_virtual_used", rsg_id);
         sge_add_double2load_report(lpp, lv_name, (mem_i_s.mem_total + mem_i_s.swap_total)-
            (mem_i_s.mem_free + mem_i_s.swap_free), uti_state_get_qualified_hostname(), "M");

         sprintf(lv_name, "rsg%d_mem_free", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_s.mem_free + mem_i_l.mem_free, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_swap_free", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_s.swap_free + mem_i_l.swap_free, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_virtual_free", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_s.mem_free + mem_i_s.swap_free +
            mem_i_l.mem_free + mem_i_l.swap_free, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_mem_total", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_s.mem_total + mem_i_l.mem_total, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_swap_total", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_s.swap_total + mem_i_l.swap_total, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_virtual_total", rsg_id);
         sge_add_double2load_report(lpp, lv_name, mem_i_s.mem_total + mem_i_s.swap_total +
            mem_i_l.mem_total + mem_i_l.swap_total, uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_mem_used", rsg_id);
         sge_add_double2load_report(lpp, lv_name, (mem_i_s.mem_total - mem_i_s.mem_free) +
            (mem_i_l.mem_total - mem_i_l.mem_free), uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_swap_used", rsg_id);
         sge_add_double2load_report(lpp, lv_name, (mem_i_s.swap_total - mem_i_s.swap_free) +
            (mem_i_l.swap_total - mem_i_l.swap_free), uti_state_get_qualified_hostname(), "M");
         sprintf(lv_name, "rsg%d_virtual_used", rsg_id);
         sge_add_double2load_report(lpp, lv_name, ((mem_i_s.mem_total + mem_i_s.swap_total)-
            (mem_i_s.mem_free + mem_i_s.swap_free)) + ((mem_i_l.mem_total + mem_i_l.swap_total)-
            (mem_i_l.mem_free + mem_i_l.swap_free)), uti_state_get_qualified_hostname(), "M");

      }
      if ((num_proc = sge_nprocs_rsg(rsg_id))) {
         sprintf(lv_name, "rsg%d_num_proc", rsg_id);
         sge_add_int2load_report(lpp, lv_name, num_proc, uti_state_get_qualified_hostname());
      }
   }
#endif


#if 0
   /* identical to "virtual_free" */
   if (!getenv("SGE_MAP_LOADVALUE")) {
      sge_add_double2load_report(lpp, "s_vmem",          mem_info.mem_total + mem_info.swap_total, uti_state_get_qualified_hostname(), "M");
      sge_add_double2load_report(lpp, "h_vmem",          mem_info.mem_total + mem_info.swap_total, uti_state_get_qualified_hostname(), "M");
   }
#endif
#endif /* SGE_LOADMEM */

#ifdef SGE_LOADCPU
   /* this code can cause nasty problems
      this is a hack to work around the problem */
   {
      double cpu_percentage;

      /* sge_getcpuload() must be called multiple 
         before it can return correct load values */
      static int first_time = 1;
      if (first_time) {
         sge_getcpuload(&cpu_percentage);
         first_time = 0;
      }

      if (sge_getcpuload(&cpu_percentage) != -1) {
         sge_add_double2load_report(lpp, "cpu", cpu_percentage, uti_state_get_qualified_hostname(), NULL);
      } else {
#ifndef INTERIX
         static u_long32 next_log2 = 0;

         u_long32 now = sge_get_gmt();
         if (now >= next_log2) {
            WARNING((SGE_EVENT, MSG_SGETEXT_NO_LOAD));
            next_log2 = now + 7200;
         }
#endif
      }
         
   }
#endif /* SGE_LOADCPU */

#ifdef INTERIX
   {
      int   svc_running;
      pid_t pids[1];

      /* look if SGE_Helper_Service.exe is running */
      svc_running = sge_get_pids(pids, 1, "SGE_Helper_Service.exe", PSCMD);
      if(svc_running <= 0) {
         svc_running = 0;
      }

      /* report if SGE_Helper_Service.exe is running and GUI can be displayed */
      sge_add_int2load_report(lpp, "display_win_gui", 
         svc_running, uti_state_get_qualified_hostname());
   }
#endif

   DEXIT;
   return 0;
}

void update_job_usage(void)
{
   lList *usage_list = NULL;
   lListElem *jr;
   lListElem *usage;
   lListElem *next_usage;

   DENTER(TOP_LAYER, "update_job_usage");

#ifdef COMPILE_DC
   if (!mconf_get_sharetree_reserved_usage()) {
      int ptf_error;

      if ((ptf_error=ptf_get_usage(&usage_list))) {
         ERROR((SGE_EVENT, MSG_LOAD_NOPTFUSAGE_S, ptf_errstr(ptf_error)));
         /*
            use the old usage values in job report or none
            in case this is the first call to ptf_get_usage()
            since a new job was started
         */
         DEXIT;
         return;
      }
   }
#endif

   if (mconf_get_sharetree_reserved_usage())
      get_reserved_usage(&usage_list);

   if (usage_list == NULL) {
      DEXIT;
      return;
   }

   if (lGetNumberOfElem(usage_list) == 0) {
      /* could be an empty list head */
      lFreeList(&usage_list);
      DEXIT;
      return;
   }

#ifdef COMPILE_DC
   ptf_show_registered_jobs();
#endif

   /* replace existing usage in the job report with the new one */
   next_usage = lFirst(usage_list);
   while( (usage=next_usage) ) {
      u_long32 job_id;
      lListElem *ja_task;
      lListElem *next_ja_task;

      next_usage = lNext(usage);
      job_id = lGetUlong(usage, JB_job_number);

      next_ja_task = lFirst(lGetList(usage, JB_ja_tasks));
      while ( (ja_task=next_ja_task) ) {
         u_long32 ja_task_id;
         lListElem *uep;
         lListElem *pe_task;

         next_ja_task = lNext(ja_task);
         ja_task_id = lGetUlong(ja_task, JAT_task_number);
         /* search matching job report */
         if (!(jr = get_job_report(job_id, ja_task_id, NULL))) {
            /* should not happen in theory */
            ERROR((SGE_EVENT, "removing unreferenced job "sge_u32"."sge_u32" without job report from ptf",job_id ,ja_task_id ));
#ifdef COMPILE_DC
            ptf_unregister_registered_job(job_id ,ja_task_id);
#endif
            continue;
         }

         /* JG: TODO: make a function updating all load values in job report */
         /* replace cpu/mem/io with newer values */
         if ((uep = lGetSubStr(ja_task, UA_name, USAGE_ATTR_CPU, JAT_usage_list))) {
            DPRINTF(("added/updated 'cpu' usage: %f\n", lGetDouble(uep, UA_value)));
            add_usage(jr, USAGE_ATTR_CPU, NULL, lGetDouble(uep, UA_value));
         }
         if ((uep = lGetSubStr(ja_task, UA_name, USAGE_ATTR_MEM, JAT_usage_list))) {
            DPRINTF(("added/updated 'mem' usage: %f\n", lGetDouble(uep, UA_value)));
            add_usage(jr, USAGE_ATTR_MEM, NULL, lGetDouble(uep, UA_value));
         }
         if ((uep = lGetSubStr(ja_task, UA_name, USAGE_ATTR_IO, JAT_usage_list))) {
            DPRINTF(("added/updated 'io' usage: %f\n", lGetDouble(uep, UA_value)));
            add_usage(jr, USAGE_ATTR_IO, NULL, lGetDouble(uep, UA_value));
         }
         if ((uep = lGetSubStr(ja_task, UA_name, USAGE_ATTR_IOW, JAT_usage_list))) {
            DPRINTF(("added/updated 'iow' usage: %f\n", lGetDouble(uep, UA_value)));
            add_usage(jr, USAGE_ATTR_IOW, NULL, lGetDouble(uep, UA_value));
         }
         if ((uep = lGetSubStr(ja_task, UA_name, USAGE_ATTR_VMEM, JAT_usage_list))) {
            DPRINTF(("added/updated 'vmem' usage: %f\n", lGetDouble(uep, UA_value)));
            add_usage(jr, USAGE_ATTR_VMEM, NULL, lGetDouble(uep, UA_value));
         }
         if ((uep = lGetSubStr(ja_task, UA_name, USAGE_ATTR_MAXVMEM, JAT_usage_list))) {
            DPRINTF(("added/updated 'maxvmem' usage: %f\n", lGetDouble(uep, UA_value)));
            add_usage(jr, USAGE_ATTR_MAXVMEM, NULL, lGetDouble(uep, UA_value));
         }

         DPRINTF(("---> updating job report usage for job "sge_u32"."sge_u32"\n",
             job_id, ja_task_id));

         for_each(pe_task, lGetList(ja_task, JAT_task_list)) {
            const char *pe_task_id = lGetString(pe_task, PET_id);

            /* search matching job report */
            if (!(jr = get_job_report(job_id, ja_task_id, pe_task_id))) {
               /* should not happen in theory */
               ERROR((SGE_EVENT, "could not find job report for job "sge_u32"."sge_u32" "
                  "task %s contained in job usage from ptf", job_id, ja_task_id, pe_task_id));
#ifdef COMPILE_DC
               ptf_show_registered_jobs();
#endif
               continue;
            }

            /* replace cpu/mem/io with newer values */
            if ((uep = lGetSubStr(pe_task, UA_name, USAGE_ATTR_CPU, PET_usage))) {
               DPRINTF(("added/updated 'cpu' usage: %f\n", lGetDouble(uep, UA_value)));
               add_usage(jr, USAGE_ATTR_CPU, NULL, lGetDouble(uep, UA_value));
            }
            if ((uep = lGetSubStr(pe_task, UA_name, USAGE_ATTR_MEM, PET_usage))) {
               DPRINTF(("added/updated 'mem' usage: %f\n", lGetDouble(uep, UA_value)));
               add_usage(jr, USAGE_ATTR_MEM, NULL, lGetDouble(uep, UA_value));
            }
            if ((uep = lGetSubStr(pe_task, UA_name, USAGE_ATTR_IO, PET_usage))) {
               DPRINTF(("added/updated 'io' usage: %f\n", lGetDouble(uep, UA_value)));
               add_usage(jr, USAGE_ATTR_IO, NULL, lGetDouble(uep, UA_value));
            }
            if ((uep = lGetSubStr(pe_task, UA_name, USAGE_ATTR_IOW, PET_usage))) {
               DPRINTF(("added/updated 'iow' usage: %f\n", lGetDouble(uep, UA_value)));
               add_usage(jr, USAGE_ATTR_IOW, NULL, lGetDouble(uep, UA_value));
            }
            if ((uep = lGetSubStr(pe_task, UA_name, USAGE_ATTR_VMEM, PET_usage))) {
               DPRINTF(("added/updated 'vmem' usage: %f\n", lGetDouble(uep, UA_value)));
               add_usage(jr, USAGE_ATTR_VMEM, NULL, lGetDouble(uep, UA_value));
            }
            if ((uep = lGetSubStr(pe_task, UA_name, USAGE_ATTR_MAXVMEM, PET_usage))) {
               DPRINTF(("added/updated 'maxvmem' usage: %f\n", lGetDouble(uep, UA_value)));
               add_usage(jr, USAGE_ATTR_MAXVMEM, NULL, lGetDouble(uep, UA_value));
            }

            DPRINTF(("---> updating job report usage for job "sge_u32"."sge_u32" task \"%s\"\n",
                job_id, ja_task_id, pe_task_id));

         }
      }
   }
   lFreeList(&usage_list);

   DEXIT;
   return;
}

static double
calculate_reserved_vmem(lListElem *queue, int nslots) 
{
   double vmem = 0.0;


   if (queue != NULL) {
      double lim, h_vmem_lim, s_vmem_lim;
      char err_str[128];

      parse_ulong_val(&h_vmem_lim, NULL, TYPE_MEM,
                      lGetString(queue, QU_h_vmem),
                      err_str, sizeof(err_str)-1);

      parse_ulong_val(&s_vmem_lim, NULL, TYPE_MEM,
                      lGetString(queue, QU_s_vmem),
                      err_str, sizeof(err_str)-1);

      lim = h_vmem_lim<s_vmem_lim ? h_vmem_lim : s_vmem_lim;

      /* INFINITY is mapped to DBL_MAX -> use 0; we cannot account INFINITY! */
      if (lim == DBL_MAX) {
         lim = 0.0;
      }

      vmem = lim * nslots;
   }

   return vmem;
}

static lList *
calculate_reserved_usage(const lListElem *ja_task, const lListElem *pe_task,
                         u_long32 job_id, u_long32 ja_task_id, 
                         const char *pe_task_id,
                         const lListElem *pe, u_long32 now)
{
   lList *ul;
   lListElem *u;
   int nslots=0, total_slots=0, usage_mul_factor = 1;
   u_long32 start_time;
   double cpu_val, vmem_val, io_val, iow_val, vmem, maxvmem;
   double wall_clock_time;

   lListElem *gdil_ep;

   /* calculate usage */
   if (pe_task == NULL) {
      start_time = lGetUlong(ja_task, JAT_start_time);
   } else {
      start_time = lGetUlong(pe_task, PET_start_time);
   }
   if (start_time && start_time < now)
      wall_clock_time = now - start_time;
   else
      wall_clock_time = 0;

   cpu_val = vmem_val = vmem = 0;

   if (pe != NULL && lGetBool(pe, PE_control_slaves)) {
      /* tight integration:
       * master task: ja_task contains all granted queues. Take the first
       *              queue of this host.
       * slave tasks: pe_ja_task contains the queue the task is running in,
       *              but without the queue information.
       *              Get queue name from pe_ja_task and lookup queue info
       *              from ja_task.
       */
      if (pe_task == NULL) {
         gdil_ep = lGetElemHost(lGetList(ja_task, 
                                         JAT_granted_destin_identifier_list),
                                JG_qhostname, 
                                uti_state_get_qualified_hostname());
      } else {
         const char *queue_name;

         queue_name = lGetString(lFirst(lGetList(pe_task, 
                                        PET_granted_destin_identifier_list)),
                                 JG_qname);
         gdil_ep = lGetElemStr(lGetList(ja_task, 
                                        JAT_granted_destin_identifier_list), 
                               JG_qname, queue_name);
      }

      if (gdil_ep != NULL) {
         vmem = calculate_reserved_vmem(lGetObject(gdil_ep, JG_queue), 1);
      }

      /* we have the master task or a slave task of a tightly integrated job:
       * reserved usage = wallclock * 1
       */
      usage_mul_factor = 1;
   } else {
      /*loose integration:
       * loop over granted_destin_identifier_list and sum up limits * nslots
       * of each queue.
       */
      lListElem *master_queue = lFirst(lGetList(ja_task, 
                                           JAT_granted_destin_identifier_list));

      for_each (gdil_ep, lGetList(ja_task, 
                                  JAT_granted_destin_identifier_list)) {
         nslots = lGetUlong(gdil_ep, JG_slots);
         total_slots += nslots;

         /* for loose integration with job_is_first_task:
          * account vmem for one additional slot in the master queue
          */
         if (gdil_ep == master_queue && pe != NULL && !lGetBool(pe, PE_job_is_first_task)) {
            nslots++;
         }
         vmem += calculate_reserved_vmem(lGetObject(gdil_ep, JG_queue), 
                                         nslots);
      }
      usage_mul_factor = execd_get_acct_multiplication_factor(pe, total_slots,
                                                              false);
   }

   /* calc reserved vmem (in GB seconds) */
   vmem_val = (vmem / 1073741824.0) * wall_clock_time;

   /* calc reserved CPU time */
   cpu_val = usage_mul_factor * wall_clock_time;

   io_val = iow_val = maxvmem = 0;

#ifdef COMPILE_DC
   {
      /* use PDC actual I/O if available */
      lList *jul;
      lListElem *uep;
      if ((jul=ptf_get_job_usage(job_id, ja_task_id, pe_task_id))) {
         io_val = ((uep=lGetElemStr(jul, UA_name, USAGE_ATTR_IO))) ?
                  lGetDouble(uep, UA_value) : 0;
         iow_val = ((uep=lGetElemStr(jul, UA_name, USAGE_ATTR_IOW))) ?
                  lGetDouble(uep, UA_value) : 0;
         maxvmem = ((uep=lGetElemStr(jul, UA_name, USAGE_ATTR_MAXVMEM))) ?
                  lGetDouble(uep, UA_value) : 0;
         lFreeList(&jul);
      }
   }
#endif

   /* create the reserved usage list */
   ul = lCreateList("usage_list", UA_Type);

   if (!(u=lGetElemStr(ul, UA_name, USAGE_ATTR_CPU)))
      u = lAddElemStr(&ul, UA_name, USAGE_ATTR_CPU, UA_Type);
   lSetDouble(u, UA_value, cpu_val);
   if (!(u=lGetElemStr(ul, UA_name, USAGE_ATTR_MEM)))
      u = lAddElemStr(&ul, UA_name, USAGE_ATTR_MEM, UA_Type);
   lSetDouble(u, UA_value, vmem_val);
   if (!(u=lGetElemStr(ul, UA_name, USAGE_ATTR_IO)))
      u = lAddElemStr(&ul, UA_name, USAGE_ATTR_IO, UA_Type);
   lSetDouble(u, UA_value, io_val);
   if (!(u=lGetElemStr(ul, UA_name, USAGE_ATTR_IOW)))
      u = lAddElemStr(&ul, UA_name, USAGE_ATTR_IOW, UA_Type);
   lSetDouble(u, UA_value, iow_val);

   if (vmem != DBL_MAX) {
      if (!(u=lGetElemStr(ul, UA_name, USAGE_ATTR_MAXVMEM)))
         u = lAddElemStr(&ul, UA_name, USAGE_ATTR_MAXVMEM, UA_Type);
      lSetDouble(u, UA_value, vmem);
   }
   if (maxvmem != 0) {
      if (!(u=lGetElemStr(ul, UA_name, USAGE_ATTR_MAXVMEM)))
         u = lAddElemStr(&ul, UA_name, USAGE_ATTR_MAXVMEM, UA_Type);
      lSetDouble(u, UA_value, maxvmem);
   }

   return ul;
}

static lListElem *
calculate_reserved_usage_ja_task(const lListElem *ja_task, 
                                 u_long32 job_id, u_long32 ja_task_id, 
                                 const lListElem *pe, u_long32 now, 
                                 lListElem *new_job) 
{
   lList *usage_list;
   lListElem *new_ja_task;

   /* create data structures */
   if (new_job == NULL) {
      new_job = lCreateElem(JB_Type);
      lSetUlong(new_job, JB_job_number, job_id);
   }

   new_ja_task = lAddSubUlong(new_job, JAT_task_number, ja_task_id, 
                              JB_ja_tasks, JAT_Type); 

   usage_list = calculate_reserved_usage(ja_task, NULL, 
                                         job_id, ja_task_id, NULL, 
                                         pe, now);
  
   lSetList(new_ja_task, JAT_usage_list, usage_list);

   return new_job;
}

static lListElem *
calculate_reserved_usage_pe_task(const lListElem *ja_task, 
                                 const lListElem *pe_task,
                                 u_long32 job_id, u_long32 ja_task_id, 
                                 const char *pe_task_id, 
                                 const lListElem *pe, u_long32 now, 
                                 lListElem *new_job) 
{
   lListElem *new_ja_task, *new_pe_task;
   lList *usage_list;

   /* create data structures */
   if (new_job == NULL) {
      new_job = lCreateElem(JB_Type);
      lSetUlong(new_job, JB_job_number, job_id);
   }

   new_ja_task = lGetElemUlong(lGetList(new_job, JB_ja_tasks), 
                               JAT_task_number, ja_task_id);
   
   if (new_ja_task == NULL) {
      new_ja_task = lAddSubUlong(new_job, JAT_task_number, ja_task_id, 
                                 JB_ja_tasks, JAT_Type); 
   }

   new_pe_task = lAddSubStr(new_ja_task, PET_id, pe_task_id, JAT_task_list, 
                            PET_Type);
   
   usage_list = calculate_reserved_usage(ja_task, pe_task, 
                                         job_id, ja_task_id, pe_task_id,
                                         pe, now);
  
   lSetList(new_pe_task, PET_usage, usage_list);

   return new_job;
}

/* calculate reserved resource usage */
static void get_reserved_usage(lList **job_usage_list)
{
   lList *temp_job_usage_list;
   const lListElem *job;
   lEnumeration *what;
   u_long32 now;

   DENTER(TOP_LAYER, "get_reserved_usage");

   now = sge_get_gmt();
   what = lWhat("%T(%I %I)", JB_Type, JB_job_number, JB_ja_tasks);
   /* JG: TODO: why use JB_Type etc.? We only need an object containing
    * job_id, ja_task_id, pe_task_id and a usage_list.
    * Same structure is delivered from PTF.
    */

   temp_job_usage_list = lCreateList("JobResUsageList", JB_Type);

   for_each (job, *(object_type_get_master_list(SGE_TYPE_JOB))) {
      u_long32 job_id;
      const lListElem *pe, *ja_task;
      lListElem *new_job = NULL;

      job_id = lGetUlong(job, JB_job_number);

      for_each (ja_task, lGetList(job, JB_ja_tasks)) {
         u_long32 ja_task_id;
         lListElem *pe_task;

         ja_task_id = lGetUlong(ja_task, JAT_task_number);

         /* we need the pe to be able to calculate the number of slots used
          * as multiplication factor for usage
          */
         pe = lGetObject(ja_task, JAT_pe_object);

         /* if we have a pid for the ja_task: it's either a non parallel job
          * or the master task of a parallel job.
          * Produce a usage record for it.
          */
         if (lGetUlong(ja_task, JAT_pid) != 0) { 
            new_job = calculate_reserved_usage_ja_task(ja_task,
                                                       job_id, ja_task_id,
                                                       pe, now, new_job);
         }

         /* if we have pe tasks (tightly integrated): Produce a usage record
          * for each of them.
          */
         for_each(pe_task, lGetList(ja_task, JAT_task_list)) {
            const char *pe_task_id;

            pe_task_id = lGetString(pe_task, PET_id);
            new_job = calculate_reserved_usage_pe_task(ja_task, pe_task, 
                                                       job_id, ja_task_id, 
                                                       pe_task_id, 
                                                       pe, now, new_job);
         }
      }

      if (new_job != NULL) {
         lAppendElem(temp_job_usage_list, new_job);
      }
   }

   *job_usage_list = lSelect("PtfJobUsageList", temp_job_usage_list, NULL,
                             what);

   lFreeList(&temp_job_usage_list);
   lFreeWhat(&what);

   DEXIT;
}
