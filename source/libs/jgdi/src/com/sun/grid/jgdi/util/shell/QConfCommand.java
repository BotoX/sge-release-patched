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
import com.sun.grid.jgdi.configuration.ComplexEntry;
import com.sun.grid.jgdi.configuration.ComplexEntryImpl;
import com.sun.grid.jgdi.configuration.ExecHost;
import com.sun.grid.jgdi.configuration.GEObject;
import com.sun.grid.jgdi.configuration.Hostgroup;
import com.sun.grid.jgdi.configuration.JGDIAnswer;
import com.sun.grid.jgdi.configuration.ShareTree;
import com.sun.grid.jgdi.configuration.ShareTreeImpl;
import com.sun.grid.jgdi.configuration.UserSet;
import com.sun.grid.jgdi.util.OutputTable;
import com.sun.grid.jgdi.util.shell.editor.EditorParser;
import java.beans.IntrospectionException;
import java.io.IOException;
import java.io.PrintWriter;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.LinkedList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

import static com.sun.grid.jgdi.util.JGDIShell.getResourceString;
import static com.sun.grid.jgdi.util.shell.OptionAnnotation.MAX_ARG_VALUE;
/**
 *
 */
@CommandAnnotation("qconf")
public class QConfCommand extends QConfCommandGenerated {
    
    public String getUsage() {
        return JGDIFactory.getJGDIVersion() + "\n" +
            getResourceString("usage.qconf");
    }
    
    public void run(String[] args) throws Exception {
        if (args.length == 0) {
            throw new IllegalArgumentException("Invalid number of arguments");
        }
        parseArgsInvokeOptions(args);
    }
    
    //-help
    @OptionAnnotation(value = "-help", min = 0)
    public void printUsage(final OptionInfo oi) throws JGDIException {
        pw.println(getUsage());
    }
    
    String getTextFromFile(final List<String> args) {
        if (args.size() <= 0) {
            pw.println("no file argument given");
            return null;
        }
        if (args.size() != 1) {
            pw.println("error: invalid option argument \"" + args.get(1) + "\"");
            pw.println("Usage: qconf -help");
            return null;
        }
        String fileName = args.get(0);
        String inputText = null;
        try {
            inputText = readFile(fileName);
        } catch (IOException ex) {
            pw.println("error: error opening file \"" + fileName + "\" for reading: " + ex.getMessage());
            pw.println("error reading in file");
        }
        return inputText;
    }
    
    @SuppressWarnings("unchecked")
    private List getNameList(final List objList) {
        List nameList = new LinkedList();
        Object obj;
        Class cls;
        String name;
        for (Iterator iter = objList.iterator(); iter.hasNext();) {
            obj = iter.next();
            Method getName;
            try {
                getName = obj.getClass().getDeclaredMethod("getName");
                name = (String) getName.invoke(obj);
                nameList.add(name);
            } catch (SecurityException ex) {
                ex.printStackTrace();
            } catch (NoSuchMethodException ex) {
                ex.printStackTrace();
            } catch (IllegalArgumentException ex) {
                ex.printStackTrace();
            } catch (IllegalAccessException ex) {
                ex.printStackTrace();
            } catch (InvocationTargetException ex) {
                ex.printStackTrace();
            }
        }
        return nameList;
    }
    
    @SuppressWarnings("unchecked")
    void printListSortedByName(List list, final List args, final PrintWriter pw) {
        list = getNameList(list);
        Collections.sort(list);
        printList(list, args, pw);
    }
    
    private void printList(List list, final List args, final PrintWriter pw) {
        for (Iterator iter = list.iterator(); iter.hasNext();) {
            pw.println(iter.next());
        }
    }
    
    //SUBMITHOST
    @OptionAnnotation(value = "-ss", min = 0)
    public void showSubmitHostList(final OptionInfo oi) throws JGDIException {
        printListSortedByName(jgdi.getSubmitHostList(), oi.getArgs(), pw);
        oi.optionDone();
    }
    
    @OptionAnnotation(value = "-as", extra = MAX_ARG_VALUE)
    public void addSubmitHost(final OptionInfo oi) throws JGDIException {
        final String hostName = oi.getFirstArg();
        List<JGDIAnswer> answers = new LinkedList<JGDIAnswer>();
        jgdi.addSubmitHostWithAnswer(hostName, answers);
        printAnswers(answers);
    }
    
    @OptionAnnotation(value = "-ds", extra = MAX_ARG_VALUE)
    public void deleteSubmitHost(final OptionInfo oi) throws JGDIException {
        final String hostName = oi.getFirstArg();
        List<JGDIAnswer> answers = new LinkedList<JGDIAnswer>();
        jgdi.deleteSubmitHostWithAnswer(hostName, answers);
        printAnswers(answers);
    }
    
    //ADMINHOST
    @OptionAnnotation(value = "-sh", min = 0)
    public void showAdminHostList(final OptionInfo oi) throws JGDIException {
        final List<String> args = oi.getArgs();
        printListSortedByName(jgdi.getAdminHostList(), args, pw);
        oi.optionDone();
    }
    
    @OptionAnnotation(value = "-ah", extra = MAX_ARG_VALUE)
    public void addAdminHost(final OptionInfo oi) throws JGDIException {
        List<JGDIAnswer> answers = new LinkedList<JGDIAnswer>();
        String hostName = oi.getFirstArg();
        jgdi.addAdminHostWithAnswer(hostName, answers);
        printAnswers(answers);
    }
    
    @OptionAnnotation(value = "-dh", extra = MAX_ARG_VALUE)
    public void deleteAdminHost(final OptionInfo oi) throws JGDIException {
        final String hostName = oi.getFirstArg();
        List<JGDIAnswer> answers = new LinkedList<JGDIAnswer>();
        jgdi.deleteAdminHostWithAnswer(hostName, answers);
        printAnswers(answers);
    }
    
    //SHARETREE
    @OptionAnnotation(value = "-astree", min = 0)
    public void addShareTree(final OptionInfo oi) throws JGDIException {
        String text = runEditor(showShareTreeNode("Root", true));
        pw.println("NOT IMPLEMENTED");
        oi.optionDone();
    }
    
    @OptionAnnotation("-Astree")
    public void addShareTreeFromFile(final OptionInfo oi) throws JGDIException {
        String inputText = getTextFromFile(oi.getArgs());
        if (inputText == null) {
            return;
        }
        pw.println("NOT IMPLEMENTED");
    }
    
    @OptionAnnotation(value = "-mstree", min = 0)
    public void modifyShareTree(final OptionInfo oi) throws JGDIException {
        String text = runEditor(showShareTreeNode("Root", true));
        pw.println("NOT IMPLEMENTED");
    }
    
    @OptionAnnotation("-Mstree")
    public void modifyShareTreeFromFile(final OptionInfo oi) throws JGDIException {
        String inputText = getTextFromFile(oi.getArgs());
        pw.println("NOT IMPLEMENTED");
    }
    
    @OptionAnnotation(value = "-sstree", min = 0)
    public void showShareTree(final OptionInfo oi) throws JGDIException {
        pw.println(showShareTreeNode("Root", true));
    }
    
    @OptionAnnotation(value = "-dstree", min = 0)
    public void deleteShareTree(final OptionInfo oi) throws JGDIException {
        ShareTree empty = new ShareTreeImpl(true);
        List<JGDIAnswer> answers = new LinkedList<JGDIAnswer>();
        jgdi.updateShareTreeWithAnswer(empty, answers);
        printAnswers(answers);
    }
    
    /**
     * Show sharetree node
     * @param oi
     * @return
     * @throws com.sun.grid.jgdi.JGDIException
     */
    String showShareTreeNode(final OptionInfo oi) throws JGDIException {
        final String name = oi.getFirstArg();
        return showShareTreeNode(name, false);
    }
    
   /*
    * Show sharetree node
    */
    @SuppressWarnings("unchecked")
    private String showShareTreeNode(final String name, final boolean showTree) throws JGDIException {
        StringBuilder sb = new StringBuilder();
        List queue = new LinkedList();
        List childList;
        String childStr;
        String stName;
        ShareTree shareTree = jgdi.getShareTree(name);
        ShareTree tempTree;
        queue.add(shareTree);
        while (!queue.isEmpty()) {
            shareTree = (ShareTree) queue.remove(0);
            //Add children to queue
            childList = new LinkedList();
            childList.addAll(shareTree.getChildrenList());
            //Sort the list by ID
            Collections.sort(childList, new Comparator() {
                
                public int compare(Object a, Object b) {
                    int na = ((ShareTree) a).getId();
                    int nb = ((ShareTree) b).getId();
                    return (na >= nb) ? ((na == nb) ? 0 : 1) : -1;
                }
            });
            childStr = "";
            for (int i = 0; i < childList.size(); i++) {
                tempTree = (ShareTree) childList.get(i);
                queue.add(tempTree);
                childStr += tempTree.getId() + ",";
            }
            //For show node
            if (!showTree) {
                stName = shareTree.getName();
                stName = stName.equals("Root") ? "" : stName;
                sb.append("/" + stName + "=" + shareTree.getShares() + "\n");
                //For show tree
            } else {
                sb.append("id=" + shareTree.getId() + "\n");
                sb.append("name=" + shareTree.getName() + "\n");
                sb.append("type=" + shareTree.getType() + "\n");
                sb.append("shares=" + shareTree.getShares() + "\n");
                childStr = (childStr.length() > 0) ? childStr.substring(0, childStr.length() - 1) : "NONE";
                sb.append("childnodes=" + childStr + "\n");
            }
        }
        return sb.toString();
    }
    
    //-tsm
    @OptionAnnotation(value = "-tsm", min = 0)
    public void triggerSchedulerMonitoring(OptionInfo oi) throws JGDIException {
        List<JGDIAnswer> answers = new LinkedList<JGDIAnswer>();
        jgdi.triggerSchedulerMonitoringWithAnswer(answers);
        printAnswers(answers);
    }
    
    //-clearusage
    @OptionAnnotation(value = "-clearusage", min = 0)
    public void clearUsage(final OptionInfo oi) throws JGDIException {
        List<JGDIAnswer> answers = new LinkedList<JGDIAnswer>();
        jgdi.clearShareTreeUsageWithAnswer(answers);
        //TODO LP: Bug - got no answers
        printAnswers(answers);
    }
    
    //-cq
    @OptionAnnotation(value = "-cq", extra = MAX_ARG_VALUE)
    public void cleanQueue(final OptionInfo oi) throws JGDIException {
        final List<String> args = oi.getArgs();
        final String[] queues = args.toArray(new String[oi.getArgs().size()]);
        List<JGDIAnswer> answers = new LinkedList<JGDIAnswer>();
        jgdi.cleanQueuesWithAnswer(queues, answers);
        printAnswers(answers);
        oi.optionDone();
    }
    
    //-kec
    @OptionAnnotation(value = "-kec", extra = MAX_ARG_VALUE)
    public void killEventClient(final OptionInfo oi) throws JGDIException {
        final List<String> args = oi.getArgs();
        List<JGDIAnswer> answers = new LinkedList<JGDIAnswer>();
        String arg;
        int[] ids = new int[args.size()];
        for (int i = 0; i < args.size(); i++) {
            arg = args.get(i);
            if (arg.equals("all")) {
                jgdi.killAllEventClientsWithAnswer(answers);
                printAnswers(answers);
                return;
            } else {
                try {
                    ids[i] = Integer.parseInt(arg);
                } catch (NumberFormatException nfe) {
                    throw new IllegalArgumentException(arg + " is not a valid event client id");
                }
            }
        }
        jgdi.killEventClientsWithAnswer(ids, answers);
        printAnswers(answers);
        oi.optionDone();
    }
    
    //-km
    @OptionAnnotation(value = "-km", min = 0)
    public void killMaster(final OptionInfo oi) throws JGDIException {
        List<JGDIAnswer> answers = new LinkedList<JGDIAnswer>();
        jgdi.killMasterWithAnswer(answers);
        printAnswers(answers);
    }
    
    //-ks
    @OptionAnnotation(value = "-ks", min = 0)
    public void killScheduler(final OptionInfo oi) throws JGDIException {
        List<JGDIAnswer> answers = new LinkedList<JGDIAnswer>();
        jgdi.killSchedulerWithAnswer(answers);
        printAnswers(answers);
    }
    
    //-sds
    @OptionAnnotation(value = "-sds", min = 0)
    public void showDetachedSettings(final OptionInfo oi) throws JGDIException {
        String sds;
        sds = jgdi.showDetachedSettingsAll();
        pw.println(sds);
        oi.optionDone();
    }
    
    //-secl
    @OptionAnnotation(value = "-secl", min = 0)
    public void showEventClientList(final OptionInfo oi) throws JGDIException {
        List evcl;
        evcl = jgdi.getEventClientList();
        if (evcl.size() > 0) {
            OutputTable table = new OutputTable(com.sun.grid.jgdi.configuration.EventClient.class);
            try {
                table.addCol("id", "ID", 8, OutputTable.Column.RIGHT);
                table.addCol("name", "NAME", 15, OutputTable.Column.LEFT);
                table.addCol("host", "HOST", 24, OutputTable.Column.LEFT);
            } catch (IntrospectionException ex) {
                ex.printStackTrace();
            }
            table.printHeader(pw);
            pw.write("--------------------------------------------------\n");
            Iterator iter = evcl.iterator();
            while (iter.hasNext()) {
                com.sun.grid.jgdi.configuration.EventClient evc = (com.sun.grid.jgdi.configuration.EventClient) iter.next();
                table.printRow(pw, evc);
            }
        } else {
            pw.println("no event clients registered");
        }
    }
    
    //-sep
    @OptionAnnotation(value = "-sep", min = 0)
    public void showProcessors(final OptionInfo oi) throws JGDIException {
        List<ExecHost> hosts;
        hosts = (List<ExecHost>) jgdi.getExecHostList();
        String name;
        String arch;
        int cpu;
        int totalCpu = 0;
        pw.println("HOST                      PROCESSOR        ARCH");
        pw.println("===============================================");
        //TODO LP: How about we sort the hosts by name first?
        for (ExecHost host : hosts) {
            name = host.getName();
            if (name.equals("global") || name.equals("template")) {
                continue;
            }
            cpu = host.getProcessors();
            totalCpu += cpu;
            if (host.isSetLoad()) {
                arch = host.getLoad("arch");
            } else {
                arch = "";
            }
            pw.printf("%-25.25s %9d %11s%n",name ,cpu, arch);
        }
        pw.println("===============================================");
        pw.printf("%-25.25s %9d%n", "SUM", totalCpu);
    }
    
    //-sss
    @OptionAnnotation(value = "-sss", min = 0)
    public void showSchedulerState(final OptionInfo oi) throws JGDIException {
        pw.println(jgdi.getSchedulerHost());
    }
    
    //-ke
    @OptionAnnotation(value = "-ke", extra = MAX_ARG_VALUE)
    public void killExecd(final OptionInfo oi) throws JGDIException {
        final List<String> args = oi.getArgs();
        killExecd(false, args);
    }
    
    //-kej
    @OptionAnnotation(value = "-kej", extra = MAX_ARG_VALUE)
    public void killExecdWithJobs(final OptionInfo oi) throws JGDIException {
        final List<String> args = oi.getArgs();
        killExecd(true, args);
    }
    
    private void killExecd(final boolean terminateJobs, final List<String> args) {
        String host;
        List<JGDIAnswer> answers = new LinkedList<JGDIAnswer>();
        try {
            for (int i = 0; i < args.size(); i++) {
                host = args.get(i);
                if (host.equals("all")) {
                    jgdi.killAllExecdsWithAnswer(terminateJobs, answers);
                    printAnswers(answers);
                    return;
                }
            }
            jgdi.killExecdWithAnswer(args.toArray(new String[args.size()]), terminateJobs, answers);
            printAnswers(answers);
        } catch (JGDIException ex) {
            pw.println(ex.getMessage());
        }
    }
    
   /*HOSTGROUP
    * Special handling methods for Hostgroup
    */
    //-shgrp_tree
    @OptionAnnotation(value = "-shgrp_tree")
    public void showHostgroupTree(final OptionInfo oi) throws JGDIException {
        final List<String> args = oi.getArgs();
        
        int i = 0;
        while (i < args.size()) {
            //Prepare the hgroup
            String hgroup = args.get(i++);
            int level = 0;
            Hostgroup obj = null;
            try {
                obj = jgdi.getHostgroup(hgroup);
            } catch (JGDIException ex) {
                pw.println("Host group \"" + hgroup + "\" does not exist");
                continue;
            }
            //Print the tree
            if (obj != null) {
                showHostgroupTree(obj, "", "   ");
            }
        }
    }
    
    //-shgrp_resolved
    @OptionAnnotation(value = "-shgrp_resolved")
    @SuppressWarnings("unchecked")
    public void showHostgroupResolved(final OptionInfo oi) throws JGDIException {
        final List<String> args = oi.getArgs();
        int i = 0;
        while (i < args.size()) {
            //Prepare the hgroup
            String hgroup = args.get(i++);
            int level = 0;
            Hostgroup obj = null;
            try {
                obj = jgdi.getHostgroup(hgroup);
            } catch (JGDIException ex) {
                pw.println("Host group \"" + hgroup + "\" does not exist");
            }
            //Print the tree
            if (obj != null) {
                LinkedList hnames = new LinkedList();
                printHostgroupResolved(hnames, obj);
                String out = "";
                for (Iterator iter = hnames.iterator(); iter.hasNext();) {
                    out += (String) iter.next() + " ";
                }
                pw.println(out.substring(0, out.length() - 1));
            }
        }
    }
    
    //TODO LP: Remove recursion in shgrp_tree
    private void showHostgroupTree(Hostgroup obj, String prefix, final String tab) {
        pw.println(prefix + obj.getName());
        prefix += tab;
        
        String hgroup;
        for (Iterator iter = obj.getHostList().iterator(); iter.hasNext();) {
            hgroup = (String) iter.next();
            //Another hroup
            if (hgroup.startsWith("@")) {
                try {
                    obj = jgdi.getHostgroup(hgroup);
                } catch (JGDIException ex) {
                    pw.println(ex.getMessage());
                }
                showHostgroupTree(obj, prefix, tab);
            } else {
                pw.println(prefix + hgroup);
            }
        }
    }
    
    //TODO: Clients use postorder, better to use sort?
    private void printHostgroupResolved(List<String> result, Hostgroup obj) {
        LinkedList<Hostgroup> queue = new LinkedList<Hostgroup>();
        queue.add(obj);
        String hgroup;
        while (!queue.isEmpty()) {
            obj = queue.remove(0);
            for (Iterator iter = obj.getHostList().iterator(); iter.hasNext();) {
                hgroup = (String) iter.next();
                //Another hroup
                if (hgroup.startsWith("@")) {
                    try {
                        obj = jgdi.getHostgroup(hgroup);
                        queue.add(obj);
                    } catch (JGDIException ex) {
                        pw.println(ex.getMessage());
                    }
                } else {
                    if (!result.contains(hgroup)) {
                        result.add(hgroup);
                    }
                }
            }
        }
        //Collections.sort(result);
    }
    
    //COMPLEXENTRY
    //-mc
    @OptionAnnotation(value = "-mc", min = 0)
    public void modifyComplexEntry(final OptionInfo oi) throws JGDIException {
        String text = runEditor(showComplexes());
        modifyComplexes(text);
    }
    
    //-Mc
    @OptionAnnotation(value = "-Mc")
    public void modifyComplexEntryFromFile(final OptionInfo oi) throws JGDIException {
        final List<String> args = oi.getArgs();
        String inputText = getTextFromFile(args);
        if (inputText == null) {
            return;
        }
        modifyComplexes(inputText);
    }
    
    //-sc
    @OptionAnnotation(value = "-sc", min = 0)
    public void showComplexEntry(final OptionInfo oi) throws JGDIException {
        pw.print(showComplexes());
    }
    
    /**
     * Updates the complex entry list based on the text
     * @param text
     */
    @SuppressWarnings("unchecked")
    private void modifyComplexes(final String text) {
        ComplexEntry ce;
        //Now parse lines and fields ignore whitespaces join lines on \
        List lineList = EditorParser.tokenizeToList(text);
        List singleLine;
        Map newCEMap = new HashMap();
        String elem;
        int val;
        Boolean boolVal;
        for (Iterator lineIter = lineList.iterator(); lineIter.hasNext();) {
            singleLine = (List) lineIter.next();
            elem = (String) singleLine.get(0);
            
            //We check if the line starts with # and skip it
            if (elem.startsWith("#")) {
                continue;
            }
            if (singleLine.size() != 8) {
                throw new IllegalArgumentException("Expected 8 elements: name, shortcut, type, relop, requestable, consumable, default, urgency");
            }
            ce = new ComplexEntryImpl(true);
            ce.setName(elem);
            ce.setShortcut((String) singleLine.get(1));
            
            val = ce.typeToInt((String) singleLine.get(2));
            //TODO LP: deny invalid
            ce.setValtype(val);
            
            val = ce.opToInt((String) singleLine.get(3));
            //TODO LP: deny invalid
            ce.setRelop(val);
            
            val = ce.reqToInt((String) singleLine.get(4));
            //TODO LP: deny invalid
            ce.setRequestable(val);
            
            boolVal = null;
            if (((String) singleLine.get(5)).equalsIgnoreCase("YES")) {
                boolVal = Boolean.TRUE;
            } else if (((String) singleLine.get(5)).equalsIgnoreCase("NO")) {
                boolVal = Boolean.FALSE;
            }
            if (boolVal == null) {
                throw new IllegalArgumentException("Expected consumable YES or NO. Got: \"" + (String) singleLine.get(5) + "\"");
            }
            ce.setConsumable(boolVal.booleanValue());
            
            ce.setDefault((String) singleLine.get(6));
            
            //val = Integer.parseInt((String) singleLine.get(7));
            ce.setUrgencyWeight((String) singleLine.get(7));
            
            newCEMap.put(elem, ce);
        }
        //TODO LP: Better to have jgdi.replaceCEList(List newList) and make the checks on qmaster side
        List toModify = new LinkedList();
        List toDelete = new LinkedList();
        List origList;
        try {
            origList = jgdi.getComplexEntryList();
            String name;
            String shortCut;
            for (Iterator iter = origList.iterator(); iter.hasNext();) {
                ce = (ComplexEntry) iter.next();
                name = ce.getName();
                //Check if existing value is modified
                if (newCEMap.containsKey(name)) {
                    if (!ce.equalsCompletely(newCEMap.get(name))) {
                        toModify.add(newCEMap.get(name));
                    }
                    newCEMap.remove(name);
                } else {
                    toDelete.add(ce);
                }
            }
            //Add new complexes
            for (Iterator iter = newCEMap.keySet().iterator(); iter.hasNext();) {
                ce = (ComplexEntry) newCEMap.get(iter.next());
                try {
                    jgdi.addComplexEntry(ce);
                } catch (JGDIException ex) {
                    pw.println(ex.getMessage());
                }
            }
            //Modify existing
            for (Iterator iter = toModify.iterator(); iter.hasNext();) {
                ce = (ComplexEntry) iter.next();
                try {
                    jgdi.updateComplexEntry(ce);
                } catch (JGDIException ex) {
                    pw.println(ex.getMessage());
                }
            }
            //Remove not defined anymore
            for (Iterator iter = toDelete.iterator(); iter.hasNext();) {
                ce = (ComplexEntry) iter.next();
                try {
                    jgdi.deleteComplexEntry(ce);
                } catch (JGDIException ex) {
                    pw.println(ex.getMessage());
                }
            }
        } catch (JGDIException ex) {
            pw.println(ex.getMessage());
        }
    }
    
    private String showComplexes() {
        StringBuilder sb = new StringBuilder();
        List cList = null;
        try {
            cList = jgdi.getComplexEntryList();
        } catch (JGDIException ex) {
            return ex.getMessage();
        }
        //Format is:nameLen+3 shortcutLen+3 typeLen+3 relopLen+3 requestableLen+8 consumableLen+7 defaultLen+3 urgencyLen+4
        int nameLen = 0;
        int shortcutLen = 0;
        int typeLen = 0;
        int relopLen = 2;
        int requestableLen = 0;
        int consumableLen = 0;
        int defaultLen = 0;
        int urgencyLen = 0;
        //Sort the list alphabetically
        List complexList = sortListByName(cList);
        ComplexEntry ceDummy = new ComplexEntryImpl(true);
        //Need to first get the maximum column lengths
        for (Iterator iter = complexList.iterator(); iter.hasNext();) {
            ComplexEntry complex = (ComplexEntry) iter.next();
            nameLen = Math.max(nameLen, complex.getName().length());
            shortcutLen = Math.max(shortcutLen, complex.getShortcut().length());
            typeLen = Math.max(typeLen, ceDummy.typeToString(complex.getValtype()).length());
            requestableLen = Math.max(requestableLen, (complex.getRequestable() == 1) ? 2 : 3); //length of YES, NO
            consumableLen = Math.max(consumableLen, (complex.isConsumable()) ? 3 : 2); //length of YES, NO);
            defaultLen = Math.max(defaultLen, complex.getDefault().length());
            urgencyLen = Math.max(urgencyLen, complex.getUrgencyWeight().length());
        }
        //Now format the columns
        String header0=String.format("%-19.19s %-10.10s %-11.11s %-5.5s %-11.11s %-10.10s %-8.8s %-8s","#name", "shortcut", "type", "relop", "requestable", "consumable", "default", "urgency");
        StringBuilder header1 = new StringBuilder("#");
        for (int j = 0; j < header0.length() - 1; j++) {
            header1.append("-");
        }
        sb.append(header0 + "\n");
        sb.append(header1 + "\n");
        String val;
        //And finally print the columns
        for (Iterator iter = complexList.iterator(); iter.hasNext();) {
            ComplexEntry complex = (ComplexEntry) iter.next();
            val = String.format("%-19.19s %-10.10s %-11.11s %-5.5s %-11.11s %-10.10s %-8.8s %-8s", complex.getName(),
                complex.getShortcut(),ceDummy.typeToString(complex.getValtype()),
                ceDummy.opToString(complex.getRelop()), ceDummy.reqToString(complex.getRequestable()),
                (complex.isConsumable()) ? "YES" : "NO", complex.getDefault(), complex.getUrgencyWeight());
            sb.append(val + "\n");
        }
        sb.append("# >#< starts a comment but comments are not saved across edits --------\n");
        return sb.toString();
    }
    
    @SuppressWarnings("unchecked")
    private static List sortListByName(final List list) {
        Collections.sort(list, new Comparator() {
            
            public int compare(Object o1, Object o2) {
                if (o1 == null && o2 == null) {
                    return 0;
                }
                if (o1 == null) {
                    return -1;
                }
                if (o2 == null) {
                    return 1;
                }
                return ((GEObject) o1).getName().compareTo(((GEObject) o2).getName());
            }
        });
        return list;
    }
    
    //MANAGER
    @OptionAnnotation(value = "-am", extra=MAX_ARG_VALUE)
    public void addManager(final OptionInfo oi) throws JGDIException {
        final String name = oi.getFirstArg();
        List<JGDIAnswer> answer = new LinkedList<JGDIAnswer>();
        jgdi.addManagerWithAnswer(name, answer);
        printAnswers(answer);
    }
    
    @OptionAnnotation(value = "-dm", extra=MAX_ARG_VALUE)
    public void deleteManager(final OptionInfo oi) throws JGDIException {
        final String name = oi.getFirstArg();
        List<JGDIAnswer> answer = new LinkedList<JGDIAnswer>();
        jgdi.deleteManagerWithAnswer(name, answer);
        printAnswers(answer);
    }
    
    //OPERATOR
    @OptionAnnotation(value = "-ao", extra=MAX_ARG_VALUE)
    public void addOperator(final OptionInfo oi) throws JGDIException {
        final String name = oi.getFirstArg();
        List<JGDIAnswer> answer = new LinkedList<JGDIAnswer>();
        jgdi.addOperatorWithAnswer(name, answer);
        printAnswers(answer);
    }
    
    @OptionAnnotation(value = "-do", extra=MAX_ARG_VALUE)
    public void deleteOperator(final OptionInfo oi) throws JGDIException {
        final String name = oi.getFirstArg();
        List<JGDIAnswer> answer = new LinkedList<JGDIAnswer>();
        jgdi.deleteOperatorWithAnswer(name, answer);
        printAnswers(answer);
    }
    
    //USERSET
    //-sul
    @OptionAnnotation(value = "-sul", min=0)
    public void showUserSetList(final OptionInfo oi) throws JGDIException {
        printListSortedByName(jgdi.getUserSetList(), null , pw);
    }
    
    //-au
    @OptionAnnotation(value = "-au", min=2)
    public void addUserSet(final OptionInfo oi) throws JGDIException {
        List<JGDIAnswer> answer = new LinkedList<JGDIAnswer>();
        final String userName = oi.getFirstArg();
        String setName = oi.getFirstArg();
        UserSet obj = jgdi.getUserSet(setName);
        obj.addEntries(userName);
        jgdi.updateUserSetWithAnswer(obj, answer);
        printAnswers(answer);
    }
    
    //-du
    @OptionAnnotation(value = "-du", min=2)
    public void deleteUserSet(final OptionInfo oi) throws JGDIException {
        List<JGDIAnswer> answer = new LinkedList<JGDIAnswer>();
        final String userName = oi.getFirstArg();
        String setName = oi.getFirstArg();
        UserSet obj = jgdi.getUserSet(setName);
        obj.removeEntries(userName);
        jgdi.updateUserSetWithAnswer(obj, answer);
        printAnswers(answer);
    }
    
    //-dul
    @OptionAnnotation(value = "-dul", min=1)
    public void deleteUserSetList(final OptionInfo oi) throws JGDIException {
        List<JGDIAnswer> answer = new LinkedList<JGDIAnswer>();
        final String setName = oi.getFirstArg();
        UserSet obj = jgdi.getUserSet(setName);
        obj.removeAllEntries();
        jgdi.updateUserSetWithAnswer(obj, answer);
        printAnswers(answer);
    }
}
