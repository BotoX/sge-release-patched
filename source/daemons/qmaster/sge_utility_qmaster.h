#ifndef __GDI_UTILITY_QMASTER_H
#define __GDI_UTILITY_QMASTER_H
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
#include "sge_answer.h"

int attr_mod_procedure(lList **alpp, lListElem *qep, lListElem *new_queue, int nm, char *attr_name, char *variables[]);

int attr_mod_ctrl_method(lList **alpp, lListElem *qep, lListElem *new_queue, int nm, char *attr_name);

int attr_mod_zerostr(lListElem *qep, lListElem *new_queue, int nm, char *attr_name);

int attr_mod_str(lList **alpp, lListElem *qep, lListElem *new_queue, int nm, char *attr_name);

int attr_mod_double(lListElem *qep, lListElem *new_queue, int nm, char *attr_name);

int attr_mod_bool(lListElem *qep, lListElem *new_queue, int nm, char *attr_name);

int attr_mod_ulong(lListElem *qep, lListElem *new_queue, int nm, char *attr_name);

int attr_mod_mem_str(lList **alpp, lListElem *qep, lListElem *new_queue, int nm, char *attr_name);

int attr_mod_time_str(lList **alpp, lListElem *qep, lListElem *new_queue, int nm, char *attr_name, int enable_infinity);

int multiple_occurances(lList **alpp, lList *lp1, lList *lp2, int nm, const char *name, const char *obj_name);

void normalize_sublist(lListElem *ep, int nm);

#endif /* __GDI_UTILITY_QMASTER_H  */

