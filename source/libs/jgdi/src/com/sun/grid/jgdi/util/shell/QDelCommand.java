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
package com.sun.grid.jgdi.util.shell;

import com.sun.grid.jgdi.JGDIException;
import com.sun.grid.jgdi.JGDIFactory;
import com.sun.grid.jgdi.configuration.JGDIAnswer;
import com.sun.grid.jgdi.monitoring.filter.UserFilter;
import static com.sun.grid.jgdi.util.JGDIShell.getResourceString;
import java.util.LinkedList;
import java.util.List;

/**
 *
 */
@CommandAnnotation("qdel")
public class QDelCommand extends AbstractCommand {
    
    public String getUsage() {
        return JGDIFactory.getJGDIVersion() + "\n" + getResourceString("usage.qdel");
    }
    
    public void run(String[] args) throws Exception {
        
        boolean force = false;
        UserFilter users = null;
        List<JGDIAnswer> answers = new LinkedList<JGDIAnswer>();
        String[] jobs = null;
        
        if (jgdi == null) {
            throw new IllegalStateException("Not connected");
        }
        if (args.length == 0) {
            pw.println(getUsage());
            pw.flush();
            return;
        }
        
        for (int i = 0; i < args.length; i++) {
            if (args[i].equals("-help")) {
                i++;
                pw.println(getUsage());
                pw.flush();
                break;
            } else if (args[i].equals("-u")) {
                i++;
                if (i >= args.length) {
                    throw new IllegalArgumentException("user_list is missing");
                }
                users = UserFilter.parse(args[i]);
            } else if (args[i].equals("-f")) {
                force = true;
            } else if (args[i].charAt(0) == '-') {
                pw.println(getUsage());
                pw.flush();
                throw new IllegalArgumentException("error: ERROR! invalid option argument \"" + args[i] + "\"");
            } else {
                jobs = parseDestinIdList(args[i]);
            }
        }
//      try {
        jgdi.deleteJobsWithAnswer(jobs, force, users, answers);
//      } catch (JGDIException je) {
//         // ignore
//      }
        printAnswers(answers);
    }
    
    private String[] parseDestinIdList(String arg) {
        String[] ret = arg.split(",");
        return ret;
    }
}
