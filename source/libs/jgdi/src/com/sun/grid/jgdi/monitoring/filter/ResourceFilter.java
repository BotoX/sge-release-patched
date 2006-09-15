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
package com.sun.grid.jgdi.monitoring.filter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.StringTokenizer;

/**
 *
 * @author richard.hierlmeier@sun.com
 */
public class ResourceFilter {
    
    private Map resourceMap = new HashMap();
    
    /** Creates a new instance of ResourceFilter */
    public ResourceFilter() {
    }
    
    public static ResourceFilter parse(String str) {
        ResourceFilter ret = new ResourceFilter();
        
        StringTokenizer st = new StringTokenizer(str, ",");
        while(st.hasMoreTokens()) {
            String resource = st.nextToken();
            int index = resource.indexOf('=');
            if(index <= 0) {
                throw new IllegalArgumentException("invalid resource list:  " + resource);
            }
            ret.addResource(resource.substring(0, index),
                            resource.substring(index+1));
        }
        return ret;
    }
    
    public void addResource(String name, String value) {
        resourceMap.put(name, value);
    }
    
    public Set getResourceNames() {
        return resourceMap.keySet();
    }
    
    public List getResources() {
        ArrayList ret = new ArrayList(resourceMap.size());
        Iterator iter = getResourceNames().iterator();
        while(iter.hasNext()) {
            String name = (String)iter.next();
            ret.add("name=" + getResource(name));
        }
        return ret;
    }
    
    public String getResource(String name) {
        String ret = (String)resourceMap.get(name);
        System.out.println("getResource: " + name + " = " + ret);
        return ret;
    }
    
}
