#ifndef __SCHEDD_MONITOR_H
#define __SCHEDD_MONITOR_H
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

#define SCHED_LOG_NAME "schedd_runlog"

/* free answer list if anyone is registered */
void clean_monitor_alp(void);

/* registers answer list to be used */
void set_monitor_alpp(lList **alpp);

/* retunrs string representation of jobid */
const char *job_descr(u_long32 jobid);

/* if monitor_next_run flag is set adds log string to 
   registered answer list or writes to schedd runlog file otherwise */
int schedd_log(const char *logstr);

/* used for multiple calling schedd_log() and generating list of items such as jobids */
int schedd_log_list(const char *logstr, lList *lp, int nm);

void schedd_set_monitor_next_run(bool set);
bool schedd_is_monitor_next_run(void);

char* schedd_get_log_string(void);

#define SCHED_MON(x) (sprintf x,schedd_log(log_string))

#endif /* __SCHEDD_MONITOR_H */

