#ifndef __UNPARSE_JOB_CULL_H
#define __UNPARSE_JOB_CULL_H
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



#define FLG_FULL_CMDLINE 1

lList *cull_unparse_job_parameter(lList **pcmdline, lListElem *job, int flags);

int sge_unparse_pe(lListElem *job, lList **pcmdline, lList **alpp);

int sge_unparse_resource_list(lListElem *job, int nm, lList **pcmdline, lList **alpp);

int sge_unparse_string_option(lListElem *job, int nm, char *option, lList **pcmdline, lList **alpp);

int sge_unparse_ulong_option(lListElem *job, int nm, char *option, lList **pcmdline, lList **alpp);

int sge_unparse_id_list(lListElem *job, int nm, char *option, lList **pcmdline, lList **alpp);

int sge_unparse_acl(const char *owner, const char *group, const char *option, lList *acl_list, lList **pcmdline, lList **alpp);



#endif /* __UNPARSE_JOB_CULL_H */

