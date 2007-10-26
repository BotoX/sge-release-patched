       Detailed Information about the new installation script "inst_sge"
       -----------------------------------------------------------------

Content:
--------
1.  Common Info
2.  Preparations
3.  Automatic Installation
4.  Un-installation
4.1 Un-installation Execd
4.2 Un-installation Qmaster
5.  Copyright

1. Common Info:
---------------

   The new install script supports an automatic installation mode. The
   "inst_sge" script can be found in the $SGE_ROOT directory.

   The modules of the installation script and a auto install configuration
   template are in $SGE_ROOT/util/install_modules.

   The combination of csp (increased security) and autoinstall is currently
   not supported.


2. Preparations:
----------------

   Make sure, what kind of installation you want to use (Interactive or
   automatic). For automatic installation you have to configure a
   configuration file.

   A template can be found in $SGE_ROOT/util/install_modules. (For
   details see 3.1 Automatic Installation) If you want to use automatic
   installation, it is mandatory, that root has rsh or ssh access to the
   hosts, which should be installed, because during the installation, the
   script tries to execute itself on the remote hosts. This happens over rsh
   or ssh.

   For Berkeley DB spooling you can decide, if you want to run a separated
   Berkeley DB spooling server or local spooling.

   In case of using a Berkeley DB server you have to choose a host, where
   your Berkeley DB is running on. On this host, rpc services have to be
   installed and configured.

   For local spooling no special configuration have to be done. It necessary,
   that the directory, where Berkeley DB spools is local. It's not allowed
   to use a NFS mounted directory -> the spooling will fail! This is a
   limitation of Berkeley DB!

   If you are going to setup a separated Berkeley DB Spooling Server, please
   install the Server on the preferred host with ./inst.sge -db and than
   start the automatic installation.
   In update 4 you can also install the Berkeley DB Spooling Server with auto-
   installation. Starting the auto install with 
   ./inst_sge -db -m -x -auto /path/to/config checks the SPOOLING_SERVER entry
   within the config file and starts the Berkeley DB installation on Server
   host. This requires a passwordless rsh/ssh connection for root, the 
   BDB Server host.


3. Automatic Installation
-------------------------

   During the automatic installation, the user is not asked for any
   question.  No message will be displayed on the terminal. After the
   installation you will find a log file in $SGE_ROOT/$SGE_CELL/spool/qmaster. 
   The name is: install_hostname_<combination of date time>.log Here you can
   find information about the used configuration and about errors during
   installation. In case of serious errors, it can be impossible for the
   installation script to move the log file into the spool directory (eg. the
   qmaster spooling could not be created), then please have a look into the 
   /tmp dir. There you can also find the install logs. The name of the
   install.log file will be shown at the end of the autoinstallation.

   Common Errors:

   The auto installation mostly breaks on the following errors.

   The <sge_cell> directory exists. This is a kind of security, because
   during the autoinstall, the user can't stop the procedure in time,
   if he has enterd the wrong <sge_cell>. So we can make sure, that
   no necessary date will be lost.

   The berkeley db spooling directory exists. This is the same like
   <sge_cell>. Data of other SGE installations can be deleted.
   -> the installations stops to make sure, no date will be lost. 

   The execution host installation seems to run very well, but the
   execution daemon is not started, or no load values are shown.
   (eg. qhost). Please check, if user root is allowd to rsh/ssh
   to the other host, without entering the password.
   To use the autoinstallation without giving root the permissions
   to rsh/ssh to any other host without a password use the installation
   as described below. 


   For a automatic installation it is recommended, that root has the right
   to connect via rsh/ssh to the different hosts, without asking for a
   password.  If this is not allowed in your network, the automatic
   installation won't work remotely. In this case it is possible to login to
   the host and start the automatic installation again.

   Example:

   On master host:

      % ./inst_sge -m -auto /tmp/install_config_file.conf

   Then login to the execution host and enter

      % ./inst_sge -x -noremote -auto /tmp/install_config_file.conf

   First, the master host will be installed automatically, using the config
   file Second, the execution host will be installed automatically using the
   config file. The -noremote switch ensures, that the install script
   doesn't try the rsh/ssh'ing to other host's which are configured within
   the config file. (e.g. EXECD_HOST_LIST="host1 host2 ...."

   Another point is the configuration file. Before the installation is
   started, the configuration file has to be adapted to your requirements. A
   template of this file can be found in

      $SGE_ROOT/$SGE_CELL/util/install_modules/inst_template.conf

   Please copy this template and make your changes.

   The template files looks like this:

      #-------------------------------------------------
      # SGE default configuration file
      #-------------------------------------------------

      # Use always fully qualified pathnames, please

      # SGE_ROOT Path, this is basic information
      #(mandatory for qmaster and execd installation)
      SGE_ROOT="Please enter path"

      # SGE_QMASTER_PORT is used by qmaster for communication
      # Please enter the port in this way: 1300
      # Please do not this: 1300/tcp
      #(mandatory for qmaster installation)
      SGE_QMASTER_PORT="Please enter port"

      # SGE_EXECD_PORT is used by execd for communication
      # Please enter the port in this way: 1300
      # Please do not this: 1300/tcp
      #(mandatory for qmaster installation)
      SGE_EXECD_PORT="Please enter port"

      # SGE_JMX_PORT is used by qmasters JMX MBean server
      # optional
      # if set qmaster will start the JMX MBean server
      SGE_JMX_PORT=""

      # CELL_NAME, will be a dir in SGE_ROOT, contains the common dir
      # Please enter only the name of the cell. No path, please
      #(mandatory for qmaster and execd installation)
      CELL_NAME="default"

      # ADMIN_USER, if you want to use a different admin user than the owner,
      # of SGE_ROOT, you have to enter the user name, here
      # Leaving this blank, the owner of the SGE_ROOT dir will be used as admin
      # user
      ADMIN_USER=""

      # The dir, where qmaster spools this parts, which are not spooled by DB
      #(mandatory for qmaster installation)
      QMASTER_SPOOL_DIR="Please, enter spooldir"

      # The dir, where the execd spools (active jobs)
      # This entry is needed, even if your are going to use
      # berkeley db spooling. Only cluster configuration and jobs will
      # be spooled in the database. The execution daemon still needs a spool
      # directory  
      #(mandatory for qmaster installation)
      EXECD_SPOOL_DIR="Please, enter spooldir"

      # For monitoring and accounting of jobs, every job will get
      # unique GID. So you have to enter a free GID Range, which
      # is assigned to each job running on a machine.
      # If you want to run 100 Jobs at the same time on one host you
      # have to enter a GID-Range like that: 16000-16100
      #(mandatory for qmaster installation)
      GID_RANGE="Please, enter GID range"

      # If SGE is compiled with -spool-dynamic, you have to enter here, which
      # spooling method should be used. (classic or berkeleydb)
      #(mandatory for qmaster installation)
      SPOOLING_METHOD="berkeleydb"

      # Name of the Server, where the Spooling DB is running on
      # if spooling methode is berkeleydb, it must be "none", when
      # using no spooling server and it must containe the servername
      # if a server should be used. In case of "classic" spooling,
      # can be left out
      DB_SPOOLING_SERVER="none"

      # The dir, where the DB spools
      # If berkeley db spooling is used, it must contain the path to
      # the spooling db. Please enter the full path. (eg. /tmp/data/spooldb)
      # Remember, this directory must be local on the qmaster host or on the
      # Berkeley DB Server host. No NSF mount, please
      DB_SPOOLING_DIR="spooldb"

      # A List of Host which should become admin hosts
      # If you do not enter any host here, you have to add all of your hosts
      # by hand, after the installation. The autoinstallation works without
      # any entry
      ADMIN_HOST_LIST="host1 host2 host3 host4"

      # A List of Host which should become submit hosts
      # If you do not enter any host here, you have to add all of your hosts
      # by hand, after the installation. The autoinstallation works without
      # any entry
      SUBMIT_HOST_LIST="host1 host2 host3 host4"

      # A List of Host which should become exec hosts
      # If you do not enter any host here, you have to add all of your hosts
      # by hand, after the installation. The autoinstallation works without
      # any entry
      # (mandatory for execution host installation)
      EXEC_HOST_LIST="host1 host2 host3 host4"

      # The dir, where the execd spools (local configuration)
      # If you want configure your execution daemons to spool in
      # a local directory, you have to enter this directory here.
      # If you do not want to configure a local execution host spool directory
      # please leave this empty
      EXECD_SPOOL_DIR_LOCAL="Please, enter spooldir"

      # If true, the domainnames will be ignored, during the hostname resolving
      # if false, the fully qualified domain name will be used for name resolving
      HOSTNAME_RESOLVING="true"

      # Shell, which should be used for remote installation (rsh/ssh)
      # This is only supported, if your hosts and rshd/sshd is configured,
      # not to ask for a password, or promting any message.
      SHELL_NAME="ssh"

      # Enter your default domain, if you are using /etc/hosts or NIS configuration
      DEFAULT_DOMAIN="none"

      # If a job stops, fails, finnish, you can send a mail to this adress
      ADMIN_MAIL="none"

      # If true, the rc scripts (sgemaster, sgeexecd, sgebdb) will be added,
      # to start automatically during boottime
      ADD_TO_RC="false"

      #If this is "true" the file permissions of executables will be set to 755
      #and of ordenary file to 644.  
      SET_FILE_PERMS="true"

      # This option is not implemented, yet.
      # When a exechost should be uninstalled, the running jobs will be rescheduled
      RESCHEDULE_JOBS="wait"

      # Enter a one of the three distributed scheduler tuning configuration sets
      # (1=normal, 2=high, 3=max)
      SCHEDD_CONF="1"

      # The name of the shadow host. This host must have read/write permission
      # to the qmaster spool directory
      # If you want to setup a shadow host, you must enter the servername
      # (mandatory for shadowhost installation)
      SHADOW_HOST="hostname"

      # Remove this execution hosts in automatic mode
      # (mandatory for unistallation of execution hosts)
      EXEC_HOST_LIST_RM="host1 host2 host3 host4"

      # This option is used for startup script removing. 
      # If true, all rc startup scripts will be removed during
      # automatic deinstallation. If false, the scripts won't
      # be touched.
      # (mandatory for unistallation of execution/qmaster hosts)
      REMOVE_RC="false"


   After writing a valid configuration file you can start the installation
   with this options:

   % inst_sge -m -x -auto path/to/configfile       
      --> This installs master locally, execution host locally and remotely
          as configured in EXEC_HOST_LIST

   % inst_sge -m -x -sm -auto path/to/configfile   
      --> This installs a additional Shadow Master Host as configured in
          SHADOW_HOST

   % inst_sge -db -m -x -auto path/to/configfile
     --> This installs a Berkeley DB RPC Server (must be configured in
         configfile) and qmaster host locally, execution host locally and 
         remotely as configured in EXEC_HOST_LIST

4. Un-installation
------------------

   An other new feature is the un-installation of Grid Engine It is also
   possible to do this automatic mode. It is recommended to un-install the
   execution hosts first. If you try to un-install the qmaster first, you
   have to un-install all execution hosts manually.

   To make sure, that you have a clean environment, please always source the
   settings.csh file, which is located in $SGE_ROOT/$SGE_CELL/common

   Using tcsh:

      % source $SGE_ROOT/$SGE_CELL/common/settings.csh

   Using the Bourne shell:

      % . $SGE_ROOT/$SGE_CELL/common/settings.sh


4.1 Execution host un-installation
----------------------------------

   During the execution host un-installation all host configuration will be
   removed. So if you are not sure about the the un-installation than please
   make a backup of your configuration.

   The un-installation goes step by step and tries to stop the exec hosts in
   a gentle way. First of all, the queue which are concerned of the
   un-installation will be disabled. So we can be sure, that no new jobs
   will be scheduled. Than, the script tries to checkpoint, reschedule,
   forced rescheduling running jobs. After this action, the queue should be
   empty.  The execution host will be shutdown and the configuration, global
   spool directory, local directory will be removed.

   To start the automatic un-installation of execution hosts enter the
   following command:

      % ./inst_sge -ux -auto "path/to/filename"

   During the automatic un-installation no terminal output will be produced.
   Be sure to have a well configured config file (see. 3.1). The entry:

   # Remove this execution hosts in automatic mode EXEC_HOST_LIST_RM="host1
   host2 host3 host4"

   is used for the un-installation. Every host, which is in the
   EXEC_HOST_LIST_RM will be automatically removed from the cluster.
 

4.2 Master host un-installation
-------------------------------

   The master host un-installation will remove all of our configuration of
   the cluster. After this procedure only the binary installation will be
   left.

   Please be careful by using this functionality. To make sure, that no
   configuration will become lost, make backups. The master host
   un-installation also supports the interactive and automatic mode.

   To start the automatic un-installation please enter the following
   command:

      % inst_sge -um -auto /path/to/config

   This mode is the same as in interactive mode. But the user won't be asked
   anymore. If the uninstall process is started, it can't be stopped
   anymore.  All terminal output will be suppressed!

4.3 Berkeley DB RPC Server uninstall
------------------------------------

   Starting the following command uninstalls the Berkeley DB RPC Server

      % inst_sge -udb -auto /path/to/config



5. Copyright
------------
___INFO__MARK_BEGIN__                                                       
The Contents of this file are made available subject to the terms of the Sun
Industry Standards Source License Version 1.2

Sun Microsystems Inc., March, 2001

Sun Industry Standards Source License Version 1.2
=================================================

The contents of this file are subject to the Sun Industry Standards Source
License Version 1.2 (the "License"); You may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://gridengine.sunsource.net/Gridengine_SISSL_license.html             

Software provided under this License is provided on an "AS IS" basis,   
WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.

See the License for the specific provisions governing your rights and
obligations concerning the Software.                                 

The Initial Developer of the Original Code is: Sun Microsystems, Inc.

Copyright: 2001 by Sun Microsystems, Inc.                     

All Rights Reserved.
___INFO__MARK_END__                                                  
