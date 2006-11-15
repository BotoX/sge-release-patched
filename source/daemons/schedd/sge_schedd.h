#ifndef __SGE_SCHEDD_H
#define __SGE_SCHEDD_H
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

#include "cull.h"
#include "evc/sge_event_client2.h"


typedef struct {
   char *name;
   char *descr;
   int (*subscribe_func)(sge_evc_class_t *evc);
   int (*event_func)(sge_evc_class_t *evc); 
   void *alg; /* can't make any assumption on this here 
                 though it usually it will be:
                  int (*alg)(sge_Sdescr_t *);        */
} sched_func_struct;

extern sched_func_struct sched_funcs[];
extern int current_scheduler;

int use_alg(const char *alg_name);
int sge_before_dispatch(sge_evc_class_t *evc);
void sge_schedd_mirror_register(sge_evc_class_t *evc);

/* Scheduler spool directory defines */
#define SCHED_BASE_DIR          "/usr/SGE"
#define SCHED_SPOOL_DIR         "schedd"
#define SCHED_PID_FILE          "sge_schedd.pid"

#endif /* __SGE_SCHEDD_H */
