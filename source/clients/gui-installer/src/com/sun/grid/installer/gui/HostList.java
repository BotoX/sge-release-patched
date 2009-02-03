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
package com.sun.grid.installer.gui;

import com.izforge.izpack.util.Debug;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.ReentrantLock;

public class HostList extends ArrayBlockingQueue<Host> {
    /** Main lock guarding all access */
    private final ReentrantLock lock;
    /** Condition for waiting takes */
    private final Condition notEmpty;
    /** Condition for waiting puts */
    private final Condition notFull;

    public HostList() {
        super(25000);
        lock = new ReentrantLock(false);
        notEmpty = lock.newCondition();
        notFull =  lock.newCondition();
    }

    public int indexOf(Host o) {
        final ReentrantLock lock = this.lock;
        lock.lock();
        try {
            Host h;
            int i=-1;
            for (Iterator<Host> iter = this.iterator(); iter.hasNext();) {
                i++;
                h = iter.next();
                if (h.equals(o)) {
                    return i;
                }
            }        
            return -1;
        } finally {
            lock.unlock();
        }
    }

    public Host get(int pos) {        
        final ReentrantLock lock = this.lock;
        lock.lock();
        try {
            Host h;
            int i=-1;
            for (Iterator<Host> iter = this.iterator(); iter.hasNext();) {
                i++;
                h = iter.next();
                if (i == pos) {
                    return h;
                }
            }
            throw new IndexOutOfBoundsException("Position "+pos+" is invalid. Valid range is 0.."+(size()-1));
        } finally {
            lock.unlock();
        }
    }

    public Host get(Host o) {
        final ReentrantLock lock = this.lock;
        lock.lock();
        try {
            Host h;
            int i=-1;
            for (Iterator<Host> iter = this.iterator(); iter.hasNext();) {
                i++;
                h = iter.next();
                if (h.equals(o)) {
                    return h;
                }
            }
            return null;
        } finally {
            lock.unlock();
        }
    }

    /**
     * Returns all of the host from the list in the given type.
     * @param type The type of the hosts should return.
     * @return All of the hosts with the given type (list is unique)
     *
     * @see Host
     */
    public ArrayList<Host> getHosts(String type) {
        ArrayList<Host> result = new ArrayList<Host>();
        ArrayList<String> hostnames = new ArrayList<String>();

        for (Host host : this) {
            //We need a unique list of Hosts of single type
            if (hostnames.contains(host.getHostAsString())) {
                continue;
            }

            if (type.equals(Host.HOST_TYPE_QMASTER) && host.isQmasterHost()) {
                result.add(host);
                hostnames.add(host.getHostAsString());
            }
            if (type.equals(Host.HOST_TYPE_EXECD) && host.isExecutionHost()) {
                result.add(host);
                hostnames.add(host.getHostAsString());
            }
            if (type.equals(Host.HOST_TYPE_SHADOWD) && host.isShadowHost()) {
                result.add(host);
                hostnames.add(host.getHostAsString());
            }
            if (type.equals(Host.HOST_TYPE_BDB) && host.isBdbHost()) {
                result.add(host);
                hostnames.add(host.getHostAsString());
            }
            if (type.equals(Host.HOST_TYPE_ADMIN) && host.isAdminHost()) {
                result.add(host);
                hostnames.add(host.getHostAsString());
            }
            if (type.equals(Host.HOST_TYPE_SUBMIT) && host.isSubmitHost()) {
                result.add(host);
                hostnames.add(host.getHostAsString());
            }
        }

        return result;
    }

    /**
     * Creates a string from the host's name
     * @param hosts The host which names should be appended
     * @param separator The separator string between the host names
     * @return The string from the host names
     */
    public static String getHostNames(ArrayList<Host> hosts, String separator) {
        StringBuffer result = new StringBuffer();

        for (int i = 0; i < hosts.size(); i++) {
            result.append(hosts.get(i).getHostAsString());
            // do not append separator to the end of the elements
            if (i + 1 != hosts.size()) {
                result.append(separator);
            }
        }

        return result.toString();
    }

    @Override
    public boolean add(Host o) {
        return addHost(o) != null;
    }
    
    public Host addHost(Host o) {
        //Debug.trace("adding - "+o.toString());
        if (o == null) {
            throw new NullPointerException();
        }
        final ReentrantLock lock = this.lock;
        lock.lock();
        try {
            Host h;
            int index = indexOf(o);
            if (index >= 0) {
                h = get(index);
                if (o.isQmasterHost()) {
                    h.setQmasterHost(true);
                }
                if (o.isAdminHost()) {
                    h.setAdminHost(true);
                }
                if (o.isSubmitHost()) {
                    h.setSubmitHost(true);
                }
                if (o.isShadowHost()) {
                    h.setShadowHost(true);
                }
                if (o.isExecutionHost()) {
                    h.setExecutionHost(true);
                    //if (h.getSpoolDir().trim().length() == 0 || h.getSpoolDir().equals(Host.DEFAULT_EXECD_SPOOL)) {
                    h.setSpoolDir(o.getSpoolDir());
                    //}
                }
                if (o.isBdbHost()) {
                    h.setBdbHost(true);
                }
                return h;
            //TODO: Should be added only if we are actually installing that type
            } else { //if (o.isQmasterHost() || o.isAdminHost() || o.isExecutionHost() || o.isShadowHost() || o.isSubmitHost() || o.isBdbHost()) {
                return (super.add(o) ? o : null);
            }
            //return false;
        } finally {
            lock.unlock();
            //printList("add");
        }
    }

    public boolean remove(Host o) {
        //Debug.trace("remove - "+o.toString());
        if (o == null) {
            throw new NullPointerException();
        }
        final ReentrantLock lock = this.lock;
        lock.lock();
        try {
            Host h;
            int index = indexOf(o);
            if (index >= 0) {
                h = get(index);
                if (o.isQmasterHost()) {
                    h.setQmasterHost(false);
                }
                if (o.isAdminHost()) {
                    h.setAdminHost(false);
                }
                if (o.isSubmitHost()) {
                    h.setSubmitHost(false);
                }
                if (o.isShadowHost()) {
                    h.setShadowHost(false);
                }
                if (o.isExecutionHost()) {
                    h.setExecutionHost(false);
                    h.setSpoolDir("");
                }
                if (o.isBdbHost()) {
                    h.setBdbHost(false);
                }
                if (!h.isQmasterHost() && !h.isAdminHost() && !h.isExecutionHost() && !h.isShadowHost() && !h.isSubmitHost() && !h.isBdbHost()) {
                    return super.remove(h);
                }
            }
            return false;
        } finally {
            lock.unlock();
            //printList("remove");
        }
    }


    public boolean addUnchecked(Host o) {
        //Debug.trace("addingUn - "+o.toString());
        if (o == null) {
            throw new NullPointerException();
        }
        final ReentrantLock lock = this.lock;
        lock.lock();
        try {
            return super.add(o);
        } finally {
            lock.unlock();
            //printList("addUnchecked");
        }
    }

    public boolean removeUnchecked(Host o) {
        //Debug.trace("removeUn - "+o.toString());
        if (o == null) {
            throw new NullPointerException();
        }
        final ReentrantLock lock = this.lock;
        lock.lock();
        try {
            Host h;
            int index = indexOf(o);
            if (index >= 0) {
                h = get(index);
                return super.remove(h);
                //return true;

            }
            return false;
        } finally {
            lock.unlock();
            //printList("removeUnchecked");
        }
    }

    public void printList(String origin) {
        for (Host h : this) {
            Debug.trace(origin + " - " + h.getHostname() + " IP: " + h.getIp() + " State:" + h.getState() + " Components: "+h.getComponentString());
        }
    }
}
