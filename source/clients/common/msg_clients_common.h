#ifndef __MSG_CLIENTS_COMMON_H
#define __MSG_CLIENTS_COMMON_H
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


#include "basis_types.h"

#define MSG_JOB_UNASSIGNED                               _MESSAGE(1001, _("unassigned"))
#define MSG_FILE_CANTREADCURRENTWORKINGDIR               _MESSAGE(1002, _("cannot read current working directory"))
#define MSG_SRC_USAGE                                    _MESSAGE(1003, _("usage:"))
#define MSG_QDEL_not_available_OPT_USAGE_S               _MESSAGE(1005, _("no usage for "SFQ" available"))
#define MSG_WARNING                                      _MESSAGE(1006, _("warning: "))
#define MSG_SEC_SETJOBCRED                               _MESSAGE(1007, _("\nCannot set job credentials."))
#define MSG_GDI_QUEUESGEGDIFAILED                        _MESSAGE(1008, _("queue: sge_gdi failed"))
#define MSG_GDI_JOBSGEGDIFAILED                          _MESSAGE(1009, _("job: sge_gdi failed"))
#define MSG_GDI_EXECHOSTSGEGDIFAILED                     _MESSAGE(1010, _("exec host: sge_gdi failed"))
#define MSG_GDI_COMPLEXSGEGDIFAILED                      _MESSAGE(1011, _("complex: sge_gdi failed"))
#define MSG_GDI_SCHEDDCONFIGSGEGDIFAILED                 _MESSAGE(1012, _("scheduler configuration: sge_gdi failed"))
#define MSG_GDI_HGRPCONFIGGDIFAILED                      _MESSAGE(1013, _("host group configuration: sge_gdi failed"))
#define MSG_GDI_GLOBALCONFIGGDIFAILED                    _MESSAGE(1014, _("global configuration: sge_gdi failed"))
/*
 * qstat_printing.c
 */
#define MSG_QSTAT_PRT_QUEUENAME    "queuename"
#define MSG_QSTAT_PRT_QTYPE        "qtype"
#define MSG_QSTAT_PRT_USEDTOT      "used/tot."
#define MSG_QSTAT_PRT_STATES       "states"
#define MSG_QSTAT_PRT_PEDINGJOBS                _MESSAGE(1020, _(" - PENDING JOBS - PENDING JOBS - PENDING JOBS - PENDING JOBS - PENDING JOBS"))
#define MSG_QSTAT_PRT_JOBSWAITINGFORACCOUNTING  _MESSAGE(1021, _(" -----   JOBS WAITING FOR ACCOUNTING  -  JOBS WAITING FOR ACCOUNTING   ----- "))
#define MSG_QSTAT_PRT_ERRORJOBS                 _MESSAGE(1022, _("  -  ERROR JOBS  -  ERROR JOBS  -  ERROR JOBS  -  ERROR JOBS  -  ERROR JOBS  -"))
#define MSG_QSTAT_PRT_FINISHEDJOBS              _MESSAGE(1023, _(" --  FINISHED JOBS  -  FINISHED JOBS  -  FINISHED JOBS  -  FINISHED JOBS  --  "))

#define MSG_CENTRY_NOTCHANGED         _MESSAGE(1024, _("Complex attribute configuration has not been changed"))
#define MSG_CENTRY_DOESNOTEXIST_S     _MESSAGE(1025, _("Complex attribute "SFQ" does not exist"))
#define MSG_CENTRY_FILENOTCORRECT_S   _MESSAGE(1026, _("Complex attribute file "SFQ" is not correct"))
#define MSG_QINSTANCE_NOQUEUES        _MESSAGE(1027, _("No queues remaining after -q queue selection"))
#define MSG_HGROUP_NOTEXIST_S         _MESSAGE(1028, _("Host group "SFQ" does not exist"))
#define MSG_HGROUP_FILEINCORRECT_S    _MESSAGE(1029, _("Host group file "SFQ" is not correct"))

#define MSG_QSTAT_HELP_WCCQ           _MESSAGE(1030, _("wildcard expression matching a cluster queue"))
#define MSG_QSTAT_HELP_WCHOST         _MESSAGE(1031, _("wildcard expression matching a host"))
#define MSG_QSTAT_HELP_WCHG           _MESSAGE(1032, _("wildcard expression matching a hostgroup"))


#define MSG_CQUEUE_DOESNOTEXIST_S     _MESSAGE(1033, _("Cluster queue entry "SFQ" does not exist"))
#define MSG_CQUEUE_FILENOTCORRECT_S   _MESSAGE(1034, _("Cluster queue file "SFQ" is not correct"))
#define MSG_CQUEUE_NOQMATCHING_S      _MESSAGE(1035, _("No cluster queue or queue instance matches the phrase "SFQ))
#define MSG_CQUEUE_DEFOVERWRITTEN_SSSSS  _MESSAGE(1036, _("default value of "SFQ" is overwritten for hostgroup "SFQ" in queue "SFQ". Not all hosts of "SFQ" are contained in the hostlist specification of queue "SFQ"."))
#define MSG_CQUEUE_UNUSEDATTRSETTING_SS  _MESSAGE(1037, _("unused setting for attribute "SFQ" and host "SFQ" in queue "SFQ"."))
#define MSG_CQUEUE_NAMENOTCORRECT_SS  _MESSAGE(1038, _("The queue name "SFQ" is not correct.  Queue names may not begin with @.  Perhaps you mean \"*"SFN"\"?"))

#define MSG_PE_NOSUCHPARALLELENVIRONMENT                 _MESSAGE(1039, _("error: no such parallel environment"))
#define MSG_USER_ABORT                                   _MESSAGE(1040, _("error: aborted by user"))

#define MSG_CENTRY_NULL_NAME          _MESSAGE(1041, _("Invalid complex attribute definition"))
#define MSG_CENTRY_NULL_SHORTCUT_S    _MESSAGE(1042, _("Complex attribute "SFQ" has no shortcut defined"))

#define MSG_LIRSNOTFOUNDINFILE_SS    _MESSAGE(1043, _("limitation rule set "SFQ" not found in file "SFQ))
#define MSG_NOLIRSFOUND              _MESSAGE(1044, _("No limitation rule set found"))

#endif /* __MSG_CLIENTS_COMMON_H */

