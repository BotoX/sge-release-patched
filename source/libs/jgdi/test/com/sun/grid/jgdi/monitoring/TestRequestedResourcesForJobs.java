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
package com.sun.grid.jgdi.monitoring;

import com.sun.grid.jgdi.JobSubmitter;
import java.util.Set;
import junit.framework.Test;
import junit.framework.TestSuite;
import com.sun.grid.jgdi.JGDI;
import java.io.PrintWriter;
import java.util.Iterator;
import java.util.List;
/*
 * Copyright 2003-2004 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms.
 * -----------------------------------------------------------------------------
 * $Id: TestRequestedResourcesForJobs.java,v 1.1 2006-09-15 09:19:58 andre Exp $
 * -----------------------------------------------------------------------------
 */
/**
 *
 * @author  richard.hierlmeier@sun.com
 */
public class TestRequestedResourcesForJobs extends com.sun.grid.jgdi.BaseTestCase {
    
//   public void testSimple() throws Exception {
//
//      JGDI jgdi = createJGDI();
//      try {
//         QueueInstanceSummaryOptions options = new QueueInstanceSummaryOptions();
//         options.setShowRequestedResourcesForJobs(true);
//         options.setShowFullOutput(true);
//         options.setShowArrayJobs(true);
//         // options.setShowExtendedSubTaskInfo(true);
//
//         QueueInstanceSummaryResult result = jgdi.getQueueInstanceSummary(options);
//         PrintWriter pw = new PrintWriter(System.out);
//         QueueInstanceSummaryPrinter.print(pw, result, options);
//         pw.flush();
//      } finally {
//         jgdi.close();
//      }
//   }
    
    
    /** Creates a new instance of TestQHost */
    public TestRequestedResourcesForJobs(String testName) {
        super(testName);
    }
    
    protected void setUp() throws Exception {
        super.setUp();
    }
    
    public static Test suite() {
        TestSuite suite = new TestSuite( TestRequestedResourcesForJobs.class);
        return suite;
    }
    
    public void testSimple() throws Exception {
        
        JGDI jgdi = createJGDI();
        
        try {
            jgdi.disableQueues(new String [] { "*" }, false);
            try {
                
                String [] args = new String [] {
                    "-e", "/dev/null",
                    "-o", "/dev/null",
                    "-l", "arch=*",
                    "-soft",
                    "-l", "arch=*",
                    "$SGE_ROOT/examples/jobs/sleeper.sh"
                };
                
                int jobId = JobSubmitter.submitJob(getCurrentCluster(), args);
                
                QueueInstanceSummaryOptions options = new QueueInstanceSummaryOptions();
                options.setShowRequestedResourcesForJobs(true);
                options.setShowFullOutput(true);
                options.setShowArrayJobs(true);
                options.setShowExtendedSubTaskInfo(true);
                
                QueueInstanceSummaryResult result = jgdi.getQueueInstanceSummary(options);
                
                PrintWriter pw = new PrintWriter(System.out);
                QueueInstanceSummaryPrinter.print(pw, result, options);
                pw.flush();
                
                
                List pendingJobs = result.getPendingJobs();
                Iterator iter = pendingJobs.iterator();
                
                boolean foundJob = false;
                JobSummary js = null;
                while(iter.hasNext()) {
                    js = (JobSummary)iter.next();
                    if(js.getId() == jobId) {
                        break;
                    }
                    js = null;
                }
                
                assertNotNull("job with id " + jobId + " not found in pending job list", js);
                
                Set hardRequestNames = js.getHardRequestNames();
                assertFalse("Job has not hard requested value", hardRequestNames.isEmpty());
                assertTrue(hardRequestNames.contains("arch"));
                assertEquals(js.getHardRequestValue("arch").getValue(), "*");
                
            } finally {
                jgdi.enableQueues(new String [] { "*" }, false);
            }
        } finally {
            jgdi.close();
        }
    }
    
}
