#ifndef __READ_DEFAULTS_H
#define __READ_DEFAULTS_H
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

#define SGE_COMMON_DEF_REQ_FILE "common/sge_request"

#define SGE_HOME_DEF_REQ_FILE   ".sge_request"

void opt_list_append_opts_from_default_files(lList **pcmdline,  
                                             lList **answer_list,
                                             char **envp);

void opt_list_append_opts_from_qsub_cmdline(lList **opts_cmdline,
                                            lList **answer_list,
                                            char **argv,
                                            char **envp);

void opt_list_append_opts_from_qalter_cmdline(lList **opts_cmdline,
                                              lList **answer_list,
                                              char **argv,
                                              char **envp);

void opt_list_append_opts_from_script(lList **opts_scriptfile,
                                      lList **answer_list,
                                      const lList *opts_cmdline,
                                      char **envp);

void opt_list_append_opts_from_script_path(lList **opts_scriptfile, const char *path,
                                           lList **answer_list,
                                           const lList *opts_cmdline,
                                           char **envp);
                                           
void opt_list_merge_command_lines(lList **opts_all,
                                  lList **opts_defaults,
                                  lList **opts_scriptfile,
                                  lList **opts_cmdline);

int opt_list_has_X(lList *opts, const char *option);

int opt_list_is_X_true(lList *opts, const char *option);

#endif /* __READ_DEFAULTS_H */

