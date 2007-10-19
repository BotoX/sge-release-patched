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

import com.sun.grid.jgdi.JGDI;
import com.sun.grid.jgdi.configuration.JGDIAnswer;
import java.io.PrintWriter;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 *
 */
public abstract class AbstractCommand implements HistoryCommand {
    Shell shell=null;
    JGDI jgdi = null;
    PrintWriter out=null;
    PrintWriter err=null;
    private int exitCode = 0;
    
    public Logger getLogger() {
        return shell.getLogger();
    }
    
    public Shell getShell() {
        return shell;
    }
    
    public void init(Shell shell) throws Exception {
        this.shell=shell;
        this.jgdi = shell.getConnection();
        this.out = shell.getOut();
        this.err = shell.getErr();
    }
    
    public String[] parseWCQueueList(String arg) {
        String [] ret = arg.split(",");
        if(getLogger().isLoggable(Level.FINE)) {
            StringBuilder buf = new StringBuilder();
            buf.append("wc_queue_list [");
            for(int i = 0; i < ret.length; i++) {
                if(i>0) {
                    buf.append(", ");
                }
                buf.append(ret[i]);
            }
            buf.append("]");
            getLogger().fine(buf.toString());
        }
        return ret;
    }
    
    public String[] parseJobWCQueueList(String arg) {
        String [] ret = arg.split(",");
        if (getLogger().isLoggable(Level.FINE)) {
            StringBuilder buf = new StringBuilder();
            buf.append("job_wc_queue_list [");
            for (int i = 0; i < ret.length; i++) {
                if (i > 0) {
                    buf.append(", ");
                }
                buf.append(ret[i]);
            }
            buf.append("]");
            getLogger().fine(buf.toString());
        }
        return ret;
    }
    
    public String[] parseJobList(String arg) {
        String[] ret = arg.split(",");
        if (getLogger().isLoggable(Level.FINE)) {
            StringBuilder buf = new StringBuilder();
            buf.append("job_list [");
            for (int i = 0; i < ret.length; i++) {
                if (i > 0) {
                    buf.append(", ");
                }
                buf.append(ret[i]);
            }
            buf.append("]");
            getLogger().fine(buf.toString());
        }
        return ret;
    }
    
    /**
     * <p>Prints the JGDI answer list to specified PrintWriter.</p>
     * <p>Helper method for JGDI methods *withAnswer</p>
     * @param answers a JGDI answer list
     */
    public void printAnswers(java.util.List<JGDIAnswer> answers) {
        int exitCode = 0;
        int status;
        int i=0;
        if (answers.size() == 0) {
            return;
        }
        JGDIAnswer answer;
        String text;
        for (i=0; i<answers.size()-1; i++) {
            answer = answers.get(i);
            status = answer.getQuality();
            if (status == 0 || status == 1) {  //If critical or error
                exitCode = status;
            }
            text = answer.getText().trim();
            if (text.length()>0 && !text.equals("ok")) out.println(answer.getText());
        }
        //Get the last
        answer = answers.get(i);
        status = answer.getQuality();
        text = answer.getText();
        
        if (status == 0 || status == 1 || exitCode != 0) {
            throw new IllegalArgumentException(text);
        }
        if (text.length()>0 && !text.equals("ok")) out.println(text);
    }
    
    /**
     * <p>Gets the commands exit code</p>
     * @return int exitCode
     */
    public int getExitCode() {
        return exitCode;
    }
    
    /**
     * <p>Sets the commands exit code to newCode</p>
     * @param int newCode new exitCode of this command
     */
    protected void setExitCode(int newCode) {
        exitCode = newCode;
    }
}
