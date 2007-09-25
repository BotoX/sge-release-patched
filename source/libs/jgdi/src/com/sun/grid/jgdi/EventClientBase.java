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
package com.sun.grid.jgdi;

import com.sun.grid.jgdi.event.EventListener;


/**
 * 
 */
public interface EventClientBase {

    /**
     * Get the id of this event client
     * @return the event client id
     */
    public int getId();

    /**
     *  Close this event client
     */
    public void close() throws JGDIException, InterruptedException;

    /**
     *  Start the event client
     */
    public void start() throws InterruptedException;

    /**
     *  Determine if the event client is running
     *  @return <code>true</code> if the event client is running
     */
    public boolean isRunning();

    /**
     *  Subscribe all events for this event client
     *  @throws JGDIException if the subscribtion is failed
     */
    public void subscribeAll() throws JGDIException;

    /**
     *  Unsubscribe all events for this event client
     *  @throws JGDIException if the unsubscribtion is failed
     */
    public void unsubscribeAll();

    /**
     * Add an event listener to this event client
     * @param lis the event listener
     */
    public void addEventListener(EventListener lis);

    /**
     * Remove an event listener from this event client
     * @param lis the event listener
     */
    public void removeEventListener(EventListener lis);

    /**
     * Commit the subscribtion
     * @throws JGDIException if the commit has failed
     */
    public void commit() throws JGDIException;
}