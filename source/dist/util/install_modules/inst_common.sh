#!/bin/sh
#
# SGE configuration script (Installation/Uninstallation/Upgrade/Downgrade)
# Scriptname: inst_common.sh
# Module: common functions
#
#___INFO__MARK_BEGIN__
##########################################################################
#
#  The Contents of this file are made available subject to the terms of
#  the Sun Industry Standards Source License Version 1.2
#
#  Sun Microsystems Inc., March, 2001
#
#
#  Sun Industry Standards Source License Version 1.2
#  =================================================
#  The contents of this file are subject to the Sun Industry Standards
#  Source License Version 1.2 (the "License"); You may not use this file
#  except in compliance with the License. You may obtain a copy of the
#  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
#
#  Software provided under this License is provided on an "AS IS" basis,
#  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
#  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
#  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
#  See the License for the specific provisions governing your rights and
#  obligations concerning the Software.
#
#  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
#
#  Copyright: 2001 by Sun Microsystems, Inc.
#
#  All Rights Reserved.
#
##########################################################################
#___INFO__MARK_END__

#-------------------------------------------------------------------------
#Setting up common variables and paths (eg. SGE_ARCH, utilbin, util)
#
BasicSettings()
{
  unset SGE_ND
  unset SGE_DEBUG_LEVEL

  SGE_UTIL="./util"
  SGE_ARCH=`$SGE_UTIL/arch`
  SGE_UTILBIN="./utilbin/$SGE_ARCH"
  SGE_BIN="./bin/$SGE_ARCH"


  shlib_path_name=`util/arch -lib`
  old_value=`eval echo '$'$shlib_path_name`
  if [ x$old_value = x ]; then
     eval $shlib_path_name=lib/$SGE_ARCH
  else
     eval $shlib_path_name=$old_value:lib/$SGE_ARCH
  fi
  export $shlib_path_name

  ADMINUSER=default
  MYUID=`$SGE_UTILBIN/uidgid -uid`
  MYGID=`$SGE_UTILBIN/uidgid -gid`
  DIRPERM=755
  FILEPERM=644

  SGE_MASTER_NAME=sge_qmaster
  SGE_EXECD_NAME=sge_execd
  SGE_SCHEDD_NAME=sge_schedd
  SGE_SHEPHERD_NAME=sge_shepherd
  SGE_SHADOWD_NAME=sge_shadowd
  SGE_SERVICE=sge_qmaster
  SGE_QMASTER_SRV=sge_qmaster
  SGE_EXECD_SRV=sge_execd

  unset SGE_NOMSG

  if [ ! -d $SGE_BIN ]; then
     $ECHO "Can't find binaries for architecture: $SGE_ARCH!"
     $ECHO "Please check your binaries. Installation failed!"
     $ECHO "Exiting installation."
     exit 1
  fi

   if [ "$SGE_ARCH" != "win32-x86" ]; then
      # set spooldefaults binary path
      SPOOLDEFAULTS=$SGE_UTILBIN/spooldefaults
      if [ ! -f $SPOOLDEFAULTS ]; then
         $ECHO "can't find \"$SPOOLDEFAULTS\""
         $ECHO "Installation failed."
         exit 1
      fi

      SPOOLINIT=$SGE_UTILBIN/spoolinit
      if [ ! -f $SPOOLINIT ]; then
         $ECHO "can't find \"$SPOOLINIT\""
         $ECHO "Installation failed."
         exit 1
      fi
   fi

  HOST=`$SGE_UTILBIN/gethostname -name`
  if [ "$HOST" = "" ]; then
     $INFOTEXT -e "can't get hostname of this machine. Installation failed."
     exit 1
  fi

  RM="rm -f"
  TOUCH="touch"

}


#-------------------------------------------------------------------------
#Setting up INFOTEXT
#
SetUpInfoText()
{
# set column break for automatic line break
SGE_INFOTEXT_MAX_COLUMN=5000; export SGE_INFOTEXT_MAX_COLUMN

# export locale directory
if [ "$SGE_ROOT" = "" ]; then
   TMP_SGE_ROOT=`pwd | sed 's/\/tmp_mnt//'`
   GRIDLOCALEDIR="$TMP_SGE_ROOT/locale"
   export GRIDLOCALEDIR
fi

# set infotext binary path
INFOTXT_DUMMY=$SGE_UTILBIN/infotext
INFOTEXT=$INFOTXT_DUMMY
if [ ! -f $INFOTXT_DUMMY ]; then
   $ECHO "can't find \"$INFOTXT_DUMMY\""
   $ECHO "Installation failed."
   exit 1
fi
}

#-------------------------------------------------------------------------
# Enter: input is read and returned to stdout. If input is empty echo $1
#
# USES: variable "$AUTO"
#
Enter()
{
   if [ "$AUTO" = "true" ]; then
      $ECHO $1
   else
      read INP
      if [ "$INP" = "" ]; then
         $ECHO $1
      else
         $ECHO $INP
      fi
   fi
}


#-------------------------------------------------------------------------
# Makedir: make directory, chown/chgrp/chmod it. Exit if failure
#
Makedir()
{
   dir=$1

   if [ ! -d $dir ]; then
       $INFOTEXT "creating directory: %s" "$dir"
       if [ "`$SGE_UTILBIN/filestat -owner $SGE_ROOT`" != "$ADMINUSER" ]; then
         Execute $MKDIR -p $dir
         if [ "$ADMINUSER" = "default" ]; then
            Execute $CHOWN root $dir
         else
            Execute $CHOWN $ADMINUSER $dir
         fi
       else
         ExecuteAsAdmin $MKDIR -p $dir
       fi
   fi

   ExecuteAsAdmin $CHMOD $DIRPERM $dir
}

#-------------------------------------------------------------------------
# Execute command as user $ADMINUSER and exit if exit status != 0
# if ADMINUSER = default then execute command unchanged
#
# uses binary "adminrun" form SGE distribution
#
# USES: variables "$verbose"    (if set to "true" print arguments)
#                  $ADMINUSER   (if set to "default" do not use "adminrun)
#                 "$SGE_UTILBIN"  (path to the binary in utilbin)
#
ExecuteAsAdmin()
{
   if [ "$verbose" = true ]; then
      $ECHO $*
   fi

   if [ $ADMINUSER = default ]; then
      $*
   else
      if [ -f $SGE_UTILBIN/adminrun ]; then
         $SGE_UTILBIN/adminrun $ADMINUSER $*
      else
         $SGE_ROOT/utilbin/$SGE_ARCH/adminrun $ADMINUSER $*
      fi
   fi

   if [ $? != 0 ]; then

      $ECHO >&2
      Translate 1 "Command failed: %s" "$*"
      $ECHO >&2
      Translate 1 "Probably a permission problem. Please check file access permissions."
      Translate 1 "Check read/write permission. Check if SGE daemons are running."
      $ECHO >&2

      $INFOTEXT -log "Command failed: %s" $*
      $INFOTEXT -log "Probably a permission problem. Please check file access permissions."
      $INFOTEXT -log "Check read/write permission. Check if SGE daemons are running."

      MoveLog
      if [ "$ADMINRUN_NO_EXIT" != "true" ]; then
         exit 1
      fi
   fi
   return 0
}


#-------------------------------------------------------------------------
# SetPerm: set permissions
#
SetPerm()
{
   file=$1
   ExecuteAsAdmin $CHMOD $FILEPERM $file
}


#--------------------------------------------------------------------------
# SetCellDependentVariables
#
SetCellDependentVariables()
{
   COMMONDIR=$SGE_CELL_VAL/common
   LCONFDIR=$SGE_CELL_VAL/common/local_conf
   CASHAREDDIR=$COMMONDIR/sgeCA
}



#-------------------------------------------------------------------------
# CheckPath: Find and Remove Slash at the end of SGE_ROOT path
#
CheckPath()
{
      MYTEMP=`echo $SGE_ROOT | sed 's/\/$//'`
      SGE_ROOT=$MYTEMP
      export SGE_ROOT
}

#--------------------------------------------------------------------------
# CheckBinaries
#
CheckBinaries()
{

BINFILES="sge_coshepherd \
          sge_execd sge_qmaster  \
          sge_schedd sge_shadowd \
          sge_shepherd qacct qalter qconf qdel qhold \
          qhost qlogin qmake qmod qmon qresub qrls qrsh qselect qsh \
          qstat qsub qtcsh qping"

WINBINFILES="sge_coshepherd sge_execd sge_shepherd  \
             qacct qalter qconf qdel qhold qhost qlogin \
             qmake qmod qresub qrls qrsh qselect qsh \
             qstat qsub qtcsh qping qloadsensor.exe sgepasswd"

UTILFILES="adminrun checkprog checkuser filestat gethostbyaddr gethostbyname \
           gethostname getservbyname loadcheck now qrsh_starter rlogin rsh rshd \
           testsuidroot uidgid infotext"

THIRD_PARTY_FILES="openssl"

if [ $SGE_ARCH = "win32-x86" ]; then
   BINFILES="$WINBINFILES"
fi

   missing=false
   for f in $BINFILES; do
      if [ ! -f $SGE_BIN/$f ]; then
         missing=true
         $INFOTEXT "missing program >%s< in directory >%s<" $f $SGE_BIN
         $INFOTEXT -log "missing program >%s< in directory >%s<" $f $SGE_BIN
      fi
   done


   for f in $THIRD_PARTY_FILES; do
      if [ $f = openssl -a "$CSP" = true ]; then
         if [ ! -f $SGE_UTILBIN/$f ]; then
           missing=true
           $INFOTEXT "missing program >%s< in directory >%s<" $f $SGE_BIN
           $INFOTEXT -log "missing program >%s< in directory >%s<" $f $SGE_BIN

         fi
      fi
   done

   for f in $UTILFILES; do
      if [ ! -f $SGE_UTILBIN/$f ]; then
         missing=true
         $INFOTEXT "missing program >%s< in directory >%s<" $f $SGE_UTILBIN
         $INFOTEXT -log "missing program >%s< in directory >%s<" $f $SGE_UTILBIN
      fi
   done

   if [ $missing = true ]; then
      if [ "$SGE_ARCH" = "win32-x86" ]; then
         $INFOTEXT "\nMissing Grid Engine binaries!\n\n" \
         "A complete installation needs the following binaries in >%s<:\n\n" \
         "qacct           qlogin          qrsh            sge_shepherd\n" \
         "qalter          qmake           qselect         sge_coshepherd\n" \
         "qconf           qmod            qsh             sge_execd\n" \
         "qdel            qmon            qstat           qhold\n" \
         "qresub          qsub            qhost           qrls\n" \
         "qtcsh           qping           sgepasswd       qloadsensor.exe\n\n" \
         "and the binaries in >%s< should be:\n\n" \
         "adminrun       gethostbyaddr  loadcheck      rlogin         uidgid\n" \
         "checkprog      gethostbyname  now            rsh            infotext\n" \
         "checkuser      gethostname    openssl        rshd\n" \
         "filestat       getservbyname  qrsh_starter   testsuidroot\n\n" \
         "Installation failed. Exit.\n" $SGE_BIN $SGE_UTILBIN
      else
         $INFOTEXT "\nMissing Grid Engine binaries!\n\n" \
         "A complete installation needs the following binaries in >%s<:\n\n" \
         "qacct           qlogin          qrsh            sge_shepherd\n" \
         "qalter          qmake           qselect         sge_coshepherd\n" \
         "qconf           qmod            qsh             sge_execd\n" \
         "qdel            qmon            qstat           sge_qmaster\n" \
         "qhold           qresub          qsub            sge_schedd\n" \
         "qhost           qrls            qtcsh           sge_shadowd\n" \
         "qping\n\n" \
         "and the binaries in >%s< should be:\n\n" \
         "adminrun       gethostbyaddr  loadcheck      rlogin         uidgid\n" \
         "checkprog      gethostbyname  now            rsh            infotext\n" \
         "checkuser      gethostname    openssl        rshd\n" \
         "filestat       getservbyname  qrsh_starter   testsuidroot\n\n" \
         "Installation failed. Exit.\n" $SGE_BIN $SGE_UTILBIN
      fi

      $INFOTEXT -log "\nMissing Grid Engine binaries!\n\n" \
      "A complete installation needs the following binaries in >%s<:\n\n" \
      "qacct           qlogin          qrsh            sge_shepherd\n" \
      "qalter          qmake           qselect         sge_coshepherd\n" \
      "qconf           qmod            qsh             sge_execd\n" \
      "qdel            qmon            qstat           sge_qmaster\n" \
      "qhold           qresub          qsub            sge_schedd\n" \
      "qhost           qrls            qtcsh           sge_shadowd\n" \
      "qping\n\n" \
      "and the binaries in >%s< should be:\n\n" \
      "adminrun       gethostbyaddr  loadcheck      rlogin         uidgid\n" \
      "checkprog      gethostbyname  now            rsh            infotext\n" \
      "checkuser      gethostname    openssl        rshd\n" \
      "filestat       getservbyname  qrsh_starter   testsuidroot\n\n" \
      "Installation failed. Exit.\n" $SGE_BIN $SGE_UTILBIN

      if [ $AUTO = true ]; then
         MoveLog
      fi

      exit 1
   fi
}


#-------------------------------------------------------------------------
# ErrUsage: print usage string, exit
#
ErrUsage()
{
   myname=`basename $0`
   $INFOTEXT -e \
             "Usage: %s -m|-um|-x|-ux [all]|-rccreate|-sm|-usm|-db|-udb|-updatedb \\\n" \
             "       -upd <sge-root> <sge-cell>|-bup|-rst [-auto <filename>] [-csp] \\\n" \
             "       [-resport] [-afs] [-host <hostname>] [-rsh] [-noremote]\n" \
             "   -m         install qmaster host\n" \
             "   -um        uninstall qmaster host\n" \
             "   -x         install execution host\n" \
             "   -ux        uninstall execution host\n" \
             "   -sm        install shadow host\n" \
             "   -usm       uninstall shadow host\n" \
             "   -db        install Berkeley DB on seperated spooling server\n" \
             "   -udb       uninstall Berkeley DB RPC spooling server\n" \
             "   -bup       backup of your configuration (Berkeley DB spooling)\n" \
             "   -rst       restore configuration from backup (Berkeley DB spooling)\n" \
             "   -upd       upgrade cluster from 5.x to 6.0\n" \
             "   -rccreate  create startup scripts from templates\n" \
             "   -updatedb  BDB update from SGE Version 6.0/6.0u1 to 6.0u2\n" \
             "   -host      hostname for shadow master installation or uninstallation \n" \
             "              (eg. exec host)\n" \
             "   -rsh       use rsh instead of ssh (default is ssh)\n" \
             "   -auto      full automatic installation (qmaster and exec hosts)\n" \
             "   -csp       install system with security framework protocol\n" \
             "              functionality\n" \
             "   -afs       install system with AFS functionality\n" \
             "   -noremote  supress remote installation during autoinstall\n" \
             "   -help      show this help text\n\n" \
             "   Examples:\n" \
             "   inst_sge -m -x\n" \
             "                     Installs qmaster and exechost on localhost\n" \
             "   inst_sge -m -x -auto /path/to/config-file\n" \
             "                     Installs qmaster and exechost using the given\n" \
             "                     configuration file\n" \
             "                     (A template can be found in:\n" \
             "                     util/install_modules/inst_template.conf)\n" \
             "   inst_sge -ux -host hostname\n" \
             "                     Uninstalls execd on given execution host\n" \
             "   inst_sge -ux all  Uninstalls all registered execution hosts\n" \
             "   inst_sge -db      Install a Berkeley DB Server on local host\n" \
             "   inst_sge -sm      Install a Shadow Master Host on local host\n" \
             "   inst_sge -upd <SGE_ROOT> <SGE_CELL>                         \n" \
             "   <sge-root> = SGE_ROOT directory of old 5.x installation.\n" \
             "   <sge-cell> = SGE_CELL name of old 5.x installation.\n" $myname

   if [ "$option" != "" ]; then 
      $INFOTEXT -e "   The option %s is not valid!" $option 
   fi

   $INFOTEXT -log "It seems, that you have entered a wrong option, please check the usage!"

      if [ "$AUTO" = true ]; then
         MoveLog
      fi

   exit 1
}

#--------------------------------------------------------------------------
# Get SGE configuration from file (autoinstall mode)
#
GetConfigFromFile()
{
  IFS="
"
  if [ $FILE != "undef" ]; then
     $INFOTEXT "Reading configuration from file %s" $FILE
     . $FILE
  else
     $INFOTEXT "No config file. Please, start the installation with\n a valid configuration file"
  fi
  IFS="   
"
   CheckConfigFile
   if [ "$BACKUP" = "false" ]; then
      SGE_CELL=$CELL_NAME
      DB_SPOOLING_SERVER=`ResolveHosts $DB_SPOOLING_SERVER`
      ADMIN_HOST_LIST=`ResolveHosts $ADMIN_HOST_LIST`
      SUBMIT_HOST_LIST=`ResolveHosts $SUBMIT_HOST_LIST`
      EXEC_HOST_LIST=`ResolveHosts $EXEC_HOST_LIST`
      SHADOW_HOST=`ResolveHosts $SHADOW_HOST`
      EXEC_HOST_LIST_RM=`ResolveHosts $EXEC_HOST_LIST_RM`
   fi
  
}

#--------------------------------------------------------------------------
# Resolves the given hostname list to the names used by SGE
#
ResolveHosts()
{
   HOSTS=""
   if [ $# -ge 1 ]; then
      for host in $*; do
         if [ "$host" = "none" ]; then
            HOSTS="$HOSTS $host"
         else
            if [ -f $host ]; then
               for fhost in `cat $host`; do
                  HOSTS="$HOSTS `$SGE_UTILBIN/gethostbyname -name $fhost`"
               done
            fi
            HOSTS="$HOSTS `$SGE_UTILBIN/gethostbyname -name $host`" 
         fi
      done
   fi
   echo $HOSTS
}

#--------------------------------------------------------------------------
# Checking the rsh/ssh connection, if working (autoinstall mode)
#
CheckRSHConnection()
{
   check_host=$1
   $SHELL_NAME $check_host hostname

   return $?
}

#--------------------------------------------------------------------------
# Checking the configuration file, if valid (autoinstall mode)
#
CheckConfigFile()
{
   if [ "$CSP" = "true" ]; then
      if [ "$CSP_COUNTRY_CODE" = "" -o `echo $CSP_COUNTRY_CODE | wc -c` != 3 ]; then
         $INFOTEXT -log "The CSP_COUNTRY_CODE entry contains more or less than 2 characters!\n"
         MoveLog
         exit 1
      fi

      if [ "$CSP_STATE" = "" ]; then
         $INFOTEXT -log "The CSP_STATE entry is empty!\n"
         MoveLog
         exit 1
      fi

      if [ "$CSP_LOCATION" = "" ]; then
         $INFOTEXT -log "The CSP_LOCATION entry is empty!\n"
         MoveLog
         exit 1
      fi

      if [ "$CSP_ORGA" = "" ]; then
         $INFOTEXT -log "The CSP_ORGA entry is empty!\n"
         MoveLog
         exit 1
      fi

      if [ "$CSP_ORGA_UNIT" = "" ]; then
         $INFOTEXT -log "The CSP_ORGA_UNIT entry is empty!\n"
         MoveLog
         exit 1
      fi

      if [ "$CSP_MAIL_ADDRESS" = "" ]; then
         $INFOTEXT -log "The CSP_MAIL_ADDRESS entry is empty!\n"
         MoveLog
         exit 1
      fi
   fi
}

#--------------------------------------------------------------------------
#
WelcomeTheUser()
{
   if [ $AUTO = true ]; then
      return
   fi

   $INFOTEXT -u "\nWelcome to the Grid Engine installation"
   $INFOTEXT -u "\nGrid Engine qmaster host installation"
   $INFOTEXT "\nBefore you continue with the installation please read these hints:\n\n" \
             "   - Your terminal window should have a size of at least\n" \
             "     80x24 characters\n\n" \
             "   - The INTR character is often bound to the key Ctrl-C.\n" \
             "     The term >Ctrl-C< is used during the installation if you\n" \
             "     have the possibility to abort the installation\n\n" \
             "The qmaster installation procedure will take approximately 5-10 minutes.\n"
   $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
   $CLEAR
}

#--------------------------------------------------------------------------
#
WelcomeTheUserUpgrade()
{
   $INFOTEXT -u "\nWelcome to the Grid Engine Upgrade"
   $INFOTEXT "\nBefore you continue with the upgrade please read these hints:\n\n" \
             "   - Your terminal window should have a size of at least\n" \
             "     80x24 characters\n\n" \
             "   - The INTR character is often bound to the key Ctrl-C.\n" \
             "     The term >Ctrl-C< is used during the upgrade if you\n" \
             "     have the possibility to abort the upgrade\n\n" \
             "The upgrade procedure will take approximately 5-10 minutes.\n" \
             "After this upgrade you will get a running qmaster and schedd with\n" \
             "the configuration of your old installation. If the upgrade was\n" \
             "successfully completed it is necessary to install your execution hosts\n" \
             "with the install_execd script."
   $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
   $CLEAR
}

#-------------------------------------------------------------------------
# CheckWhoInstallsSGE
#
CheckWhoInstallsSGE()
{
   euid=`$SGE_UTILBIN/uidgid -euid`
   if [ $euid != 0 ]; then
      $CLEAR
      if [ $BERKELEY = "install" ]; then
         $INFOTEXT -u "\nBerkeley DB - test installation"
      else
         $INFOTEXT -u "\nGrid Engine - test installation"
      fi

      $INFOTEXT "\nYou are installing not as user >root<!\n\n" \
                  "This will allow you to run Grid Engine only under your user id for testing\n" \
                  "a limited functionality of Grid Engine.\n"

      ADMINUSER=`whoami`
      #ADMINUSER="none"
      $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> if this is ok or stop the installation with Ctrl-C >> "
      $CLEAR
      return 0
   fi
   # from here only root
   this_dir_user=`$SGE_UTILBIN/filestat -owner . 2> /dev/null`
   ret=$?
   if [ $ret != 0 ]; then
      $INFOTEXT "\nCan't resolve user name from current directory.\n" \
                "Installation failed. Exit.\n"
      $INFOTEXT -log "\nCan't resolve user name from current directory.\n" \
                     "Installation failed. Exit.\n"
      exit 1
   fi

   if [ $this_dir_user != root ]; then
      $CLEAR
      $INFOTEXT -u "\nGrid Engine admin user account"

      $INFOTEXT "\nThe current directory\n\n" \
                "   %s\n\n" \
                "is owned by user\n\n" \
                "   %s\n\n" \
                "If user >root< does not have write permissions in this directory on *all*\n" \
                "of the machines where Grid Engine will be installed (NFS partitions not\n" \
                "exported for user >root< with read/write permissions) it is recommended to\n" \
                "install Grid Engine that all spool files will be created under the user id\n" \
                "of user >%s<.\n\n" \
                "IMPORTANT NOTE: The daemons still have to be started by user >root<.\n" \
                `pwd` $this_dir_user $this_dir_user

      $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
                "Do you want to install Grid Engine as admin user >%s< (y/n) [y] >> " $this_dir_user
                ret=$?
      if [ "$AUTO" = "true" -a "$ADMIN_USER" != "" ]; then
         ret=1
      fi

      if [ "$ret" = 0 ]; then
         $INFOTEXT "Installing Grid Engine as admin user >%s<" "$this_dir_user"
         $INFOTEXT -log "Installing Grid Engine as admin user >%s<" "$this_dir_user"
         ADMINUSER=$this_dir_user
         $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
         $CLEAR
         return
      else
         $CLEAR
      fi
   fi
   $INFOTEXT -u "\nChoosing Grid Engine admin user account"

   $INFOTEXT "\nYou may install Grid Engine that all files are created with the user id of an\n" \
             "unprivileged user.\n\n" \
             "This will make it possible to install and run Grid Engine in directories\n" \
             "where user >root< has no permissions to create and write files and directories.\n\n" \
             "   - Grid Engine still has to be started by user >root<\n\n" \
             "   - this directory should be owned by the Grid Engine administrator\n"

   $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
             "Do you want to install Grid Engine\n" \
             "under an user id other than >root< (y/n) [y] >> "

   if [ $? = 0 ]; then
      done=false
      while [ $done = false ]; do
         $CLEAR
         $INFOTEXT -u "\nChoosing a Grid Engine admin user name"
         $INFOTEXT -n "\nPlease enter a valid user name >> "
         INP=`Enter ""`
         if [ "$AUTO" = "true" ]; then
            if [ "$ADMIN_USER" = "" ]; then
               INP=`Enter root`
            else
               INP=`Enter $ADMIN_USER`
            fi
         fi

         if [ "$INP" != "" ]; then
            $SGE_UTILBIN/checkuser -check "$INP"
            if [ $? != 0 ]; then
               $INFOTEXT "User >%s< does not exist - please correct the username" $INP
               $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
               $CLEAR
            else
               $INFOTEXT "\nInstalling Grid Engine as admin user >%s<\n" $INP
               $INFOTEXT -log "Installing Grid Engine as user >%s<" $INP
               ADMINUSER=$INP
               if [ $ADMINUSER = root ]; then
                  ADMINUSER=default
               fi
               $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
               $CLEAR
               done=true
            fi
         fi
      done
   else
      $INFOTEXT "\nInstalling Grid Engine as user >root<\n"
      $INFOTEXT -log "Installing Grid Engine as user >root<"
      ADMINUSER=default
      $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
      $CLEAR
   fi
}

#-------------------------------------------------------------------------
# CheckForLocalHostResolving
#   "localhost", localhost.localdomain and 127.0.x.x are not supported
#   
#
CheckForLocalHostResolving()
{
   output=`$SGE_UTILBIN/gethostname| cut -f2 -d:`

   notok=false
   for cmp in $output; do
      case "$cmp" in
      localhost*|127.0*)
         notok=true
         ;;
      esac
   done

   if [ $notok = true ]; then
      $INFOTEXT -u "\nUnsupported local hostname"
      $INFOTEXT "\nThe current hostname is resolved as follows:\n\n"
      $SGE_UTILBIN/gethostname
      $INFOTEXT -wait -auto $AUTO -n \
                "\nIt is not supported for a Grid Engine installation that the local hostname\n" \
                "contains the hostname \"localhost\" and/or the IP address \"127.0.x.x\" of the\n" \
                "loopback interface.\n" \
                "The \"localhost\" hostname should be reserved for the loopback interface\n" \
                "(\"127.0.0.1\") and the real hostname should be assigned to one of the\n" \
                "physical or logical network interfaces of this machine.\n\n" \
                "Installation failed.\n\n" \
                "Press <RETURN> to exit the installation procedure >> "
      $INFOTEXT -log "\nThe current hostname is resolved as follows:\n\n"
      $SGE_UTILBIN/gethostname
      $INFOTEXT -log -wait -auto $AUTO -n \
                "\nIt is not supported for a Grid Engine installation that the local hostname\n" \
                "contains the hostname \"localhost\" and/or the IP address \"127.0.x.x\" of the\n" \
                "loopback interface.\n" \
                "The \"localhost\" hostname should be reserved for the loopback interface\n" \
                "(\"127.0.0.1\") and the real hostname should be assigned to one of the\n" \
                "physical or logical network interfaces of this machine.\n\n" \
                "Installation failed.\n\n" \
                "Press <RETURN> to exit the installation procedure >> "
      exit
   fi
}
               
#-------------------------------------------------------------------------
# ProcessSGERoot: read SGE root directory and set $SGE_ROOT
#                    check if $SGE_ROOT matches current directory
#
ProcessSGERoot()
{
   export SGE_ROOT

   done=false

   while [ $done = false ]; do
      if [ "$SGE_ROOT" = "" ]; then
         while [ "$SGE_ROOT" = "" ]; do
            $CLEAR
            $INFOTEXT -u "\nChecking \$SGE_ROOT directory"
            $ECHO
            eval SGE_ROOT=`pwd | sed 's/\/tmp_mnt//'`
            $INFOTEXT -n "The Grid Engine root directory is not set!\n" \
                         "Please enter a correct path for SGE_ROOT.\n" 
            $INFOTEXT -n "If this directory is not correct (e.g. it may contain an automounter\n" \
                         "prefix) enter the correct path to this directory or hit <RETURN>\n" \
                         "to use default [%s] >> " $SGE_ROOT
         
            eval SGE_ROOT=`Enter $SGE_ROOT`
         done
         export SGE_ROOT
      else
         $CLEAR
         $INFOTEXT -u "\nChecking \$SGE_ROOT directory"
         $ECHO
         SGE_ROOT_VAL=`eval echo $SGE_ROOT`

         $INFOTEXT -n "The Grid Engine root directory is:\n\n" \
                      "   \$SGE_ROOT = %s\n\n" \
                      "If this directory is not correct (e.g. it may contain an automounter\n" \
                      "prefix) enter the correct path to this directory or hit <RETURN>\n" \
                      "to use default [%s] >> " $SGE_ROOT_VAL $SGE_ROOT_VAL

         eval SGE_ROOT=`Enter $SGE_ROOT_VAL`
         $ECHO
      fi
      SGE_ROOT_VAL=`eval echo $SGE_ROOT`

      # do not check for correct SGE_ROOT in case of -nostrict
      if [ "$strict" = true ]; then
         # create a file in SGE_ROOT
         if [ $ADMINUSER != default ]; then
            $SGE_UTILBIN/adminrun $ADMINUSER $TOUCH $SGE_ROOT_VAL/tst$$ 2> /dev/null > /dev/null
         else
            touch $SGE_ROOT_VAL/tst$$ 2> /dev/null > /dev/null
         fi
         ret=$?
         # check if we have write permission
         if [ $ret != 0 ]; then
            $CLEAR
            $INFOTEXT "Can't create a temporary file in the directory\n\n   %s\n\n" \
                      "This may be a permission problem (e.g. no read/write permission\n" \
                      "on a NFS mounted filesystem).\n" \
                      "Please check your permissions. You may cancel the installation now\n" \
                      "and restart it or continue and try again.\n" $SGE_ROOT_VAL
            unset $SGE_ROOT
            $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
            $CLEAR
         elif [ ! -f tst$$ ]; then
            # check if SGE_ROOT points to current directory
            $INFOTEXT "Your \$SGE_ROOT environment variable\n\n   \$SGE_ROOT = %s\n\n" \
                        "doesn't match the current directory.\n" $SGE_ROOT_VAL
            ExecuteAsAdmin $RM -f $SGE_ROOT_VAL/tst$$
            unset $SGE_ROOT
         else
            ExecuteAsAdmin $RM -f $SGE_ROOT_VAL/tst$$
            done=true
         fi
      else
         done=true
      fi
   done

   CheckPath
   $INFOTEXT "Your \$SGE_ROOT directory: %s\n" $SGE_ROOT_VAL
   $INFOTEXT -log "Your \$SGE_ROOT directory: %s" $SGE_ROOT_VAL
   $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
   $CLEAR
}

 
#-------------------------------------------------------------------------
# GiveHints: give some useful hints at the end of the installation
#
GiveHints()
{
   $CLEAR

   if [ $AUTO = true ]; then
      return
   fi

   done=false
   while [ $done = false ]; do
      $CLEAR
      $INFOTEXT -u "\nUsing Grid Engine"
      $INFOTEXT "\nYou should now enter the command:\n\n" \
                "   source %s\n\n" \
                "if you are a csh/tcsh user or\n\n" \
                "   # . %s\n\n" \
                "if you are a sh/ksh user.\n\n" \
                "This will set or expand the following environment variables:\n\n" \
                "   - \$SGE_ROOT         (always necessary)\n" \
                "   - \$SGE_CELL         (if you are using a cell other than >default<)\n" \
                "   - \$SGE_QMASTER_PORT (if you haven't added the service >sge_qmaster<)\n" \
                "   - \$SGE_EXECD_PORT   (if you haven't added the service >sge_execd<)\n" \
                "   - \$PATH/\$path       (to find the Grid Engine binaries)\n" \
                "   - \$MANPATH          (to access the manual pages)\n" \
                $SGE_ROOT_VAL/$SGE_CELL_VAL/common/settings.csh \
                $SGE_ROOT_VAL/$SGE_CELL_VAL/common/settings.sh

      $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to see where Grid Engine logs messages >> "
      $CLEAR

      tmp_spool=`cat $SGE_ROOT/$SGE_CELL/common/bootstrap | grep qmaster_spool_dir | awk '{ print $2 }'`
      master_spool=`dirname $tmp_spool`

      $INFOTEXT -u "\nGrid Engine messages"
      $INFOTEXT "\nGrid Engine messages can be found at:\n\n" \
                "   /tmp/qmaster_messages (during qmaster startup)\n" \
                "   /tmp/execd_messages   (during execution daemon startup)\n\n" \
                "After startup the daemons log their messages in their spool directories.\n\n" \
                "   Qmaster:     %s\n" \
                "   Exec daemon: <execd_spool_dir>/<hostname>/messages\n" $master_spool/qmaster/messages

      $INFOTEXT -u "\nGrid Engine startup scripts"
      $INFOTEXT "\nGrid Engine startup scripts can be found at:\n\n" \
                "   %s (qmaster and scheduler)\n" \
                "   %s (execd)\n" $SGE_ROOT/$SGE_CELL/common/sgemaster $SGE_ROOT/$SGE_CELL/common/sgeexecd

      $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" -n \
                "Do you want to see previous screen about using Grid Engine again (y/n) [n] >> "
      if [ $? = 0 ]; then
         :
      else
         done=true
      fi
   done

   if [ $QMASTER = install ]; then
      $CLEAR
      $INFOTEXT -u "\nYour Grid Engine qmaster installation is now completed"
      $INFOTEXT   "\nPlease now login to all hosts where you want to run an execution daemon\n" \
                  "and start the execution host installation procedure.\n\n" \
                  "If you want to run an execution daemon on this host, please do not forget\n" \
                  "to make the execution host installation in this host as well.\n\n" \
                  "All execution hosts must be administrative hosts during the installation.\n" \
                  "All hosts which you added to the list of administrative hosts during this\n" \
                  "installation procedure can now be installed.\n\n" \
                  "You may verify your administrative hosts with the command\n\n" \
                  "   # qconf -sh\n\n" \
                  "and you may add new administrative hosts with the command\n\n" \
                  "   # qconf -ah <hostname>\n\n"
       $INFOTEXT -wait -n "Please hit <RETURN> >> "
       $CLEAR
       QMASTER="undef"
      return 0
   else
      $INFOTEXT "Your execution daemon installation is now completed."
   fi
}


#-------------------------------------------------------------------------
# PrintLocalConf:  print execution host local SGE configuration
#
PrintLocalConf()
{

   arg=$1
   if [ $arg = 1 ]; then
      $ECHO "# Version: 6.0u7"
      $ECHO "#"
      $ECHO "# DO NOT MODIFY THIS FILE MANUALLY!"
      $ECHO "#"
      $ECHO "conf_version           0"
   fi
   $ECHO "mailer                 $MAILER"
   if [ "$XTERM" = "" ]; then
      $ECHO "xterm                  none"
   else
      $ECHO "xterm                  $XTERM"
   fi
   $ECHO "qlogin_daemon          $QLOGIN_DAEMON"
   $ECHO "rlogin_daemon          $RLOGIN_DAEMON"
   if [ "$LOCAL_EXECD_SPOOL" != "undef" ]; then
      $ECHO "execd_spool_dir        $LOCAL_EXECD_SPOOL"
   fi
   if [ "$RSH_DAEMON" != "undef" ]; then
      $ECHO "rsh_daemon             $RSH_DAEMON"
   fi
   if [ "$LOADSENSOR_COMMAND" != "undef" ]; then
      $ECHO "load_sensor            $SGE_ROOT/$LOADSENSOR_COMMAND"
   fi
}


#-------------------------------------------------------------------------
# CreateSGEStartUpScripts: create startup scripts 
#
CreateSGEStartUpScripts()
{
   euid=$1
   create=$2
   hosttype=$3

   if [ $hosttype = "master" ]; then
      TMP_SGE_STARTUP_FILE=/tmp/sgemaster.$$
      STARTUP_FILE_NAME=sgemaster
      S95NAME=S95sgemaster
   else
      TMP_SGE_STARTUP_FILE=/tmp/sgeexecd.$$
      STARTUP_FILE_NAME=sgeexecd
      S95NAME=S95sgeexecd
   fi

   if [ -f $TMP_SGE_STARTUP_FILE ]; then
      Execute rm $TMP_SGE_STARTUP_FILE
      Execute touch $TMP_SGE_STARTUP_FILE
      Execute $CHMOD a+rx $TMP_SGE_STARTUP_FILE
   fi
   if [ -f ${TMP_SGE_STARTUP_FILE}.0 ]; then
      Execute rm ${TMP_SGE_STARTUP_FILE}.0
   fi
   if [ -f ${TMP_SGE_STARTUP_FILE}.1 ]; then
      Execute rm ${TMP_SGE_STARTUP_FILE}.1
   fi

   SGE_STARTUP_FILE=$SGE_ROOT_VAL/$COMMONDIR/$STARTUP_FILE_NAME

   if [ $create = true ]; then

      if [ $hosttype = "master" ]; then
         ExecuteAsAdmin sed -e "s%GENROOT%${SGE_ROOT_VAL}%g" \
                            -e "s%GENCELL%${SGE_CELL_VAL}%g" \
                            -e "/#+-#+-#+-#-/,/#-#-#-#-#-#/d" \
                            util/rctemplates/sgemaster_template > ${TMP_SGE_STARTUP_FILE}.0

         if [ "$SGE_QMASTER_PORT" != "" ]; then
            ExecuteAsAdmin sed -e "s/=GENSGE_QMASTER_PORT/=$SGE_QMASTER_PORT/" \
                               ${TMP_SGE_STARTUP_FILE}.0 > $TMP_SGE_STARTUP_FILE.1
         else
            ExecuteAsAdmin sed -e "/GENSGE_QMASTER_PORT/d" \
                               ${TMP_SGE_STARTUP_FILE}.0 > $TMP_SGE_STARTUP_FILE.1
         fi

         if [ "$SGE_EXECD_PORT" != "" ]; then
            ExecuteAsAdmin sed -e "s/=GENSGE_EXECD_PORT/=$SGE_EXECD_PORT/" \
                               ${TMP_SGE_STARTUP_FILE}.1 > $TMP_SGE_STARTUP_FILE
         else
            ExecuteAsAdmin sed -e "/GENSGE_EXECD_PORT/d" \
                               ${TMP_SGE_STARTUP_FILE}.1 > $TMP_SGE_STARTUP_FILE
         fi
      else
         ExecuteAsAdmin sed -e "s%GENROOT%${SGE_ROOT_VAL}%g" \
                            -e "s%GENCELL%${SGE_CELL_VAL}%g" \
                            -e "/#+-#+-#+-#-/,/#-#-#-#-#-#/d" \
                            util/rctemplates/sgeexecd_template > ${TMP_SGE_STARTUP_FILE}.0

         if [ "$SGE_QMASTER_PORT" != "" ]; then
            ExecuteAsAdmin sed -e "s/=GENSGE_QMASTER_PORT/=$SGE_QMASTER_PORT/" \
                               ${TMP_SGE_STARTUP_FILE}.0 > $TMP_SGE_STARTUP_FILE.1
         else
            ExecuteAsAdmin sed -e "/GENSGE_QMASTER_PORT/d" \
                               ${TMP_SGE_STARTUP_FILE}.0 > $TMP_SGE_STARTUP_FILE.1
         fi

         if [ "$SGE_EXECD_PORT" != "" ]; then
            ExecuteAsAdmin sed -e "s/=GENSGE_EXECD_PORT/=$SGE_EXECD_PORT/" \
                               ${TMP_SGE_STARTUP_FILE}.1 > $TMP_SGE_STARTUP_FILE
         else
            ExecuteAsAdmin sed -e "/GENSGE_EXECD_PORT/d" \
                               ${TMP_SGE_STARTUP_FILE}.1 > $TMP_SGE_STARTUP_FILE
         fi

       fi

      ExecuteAsAdmin $CP $TMP_SGE_STARTUP_FILE $SGE_STARTUP_FILE
      ExecuteAsAdmin $CHMOD a+x $SGE_STARTUP_FILE

      rm -f $TMP_SGE_STARTUP_FILE ${TMP_SGE_STARTUP_FILE}.0 ${TMP_SGE_STARTUP_FILE}.1

      if [ $euid = 0 -a $ADMINUSER != default -a $QMASTER = "install" -a $hosttype = "master" ]; then
         AddDefaultManager root $ADMINUSER
         AddDefaultOperator $ADMINUSER
      elif [ $euid != 0 -a $hosttype = "master" ]; then
         AddDefaultManager $USER
         AddDefaultOperator $USER
      fi

      $INFOTEXT "Creating >%s< script" $STARTUP_FILE_NAME 
   fi

}



#-------------------------------------------------------------------------
# AddSGEStartUpScript: Add startup script to rc files if root installs
#
AddSGEStartUpScript()
{
   euid=$1
   hosttype=$2

   $CLEAR
   if [ $hosttype = "master" ]; then
      TMP_SGE_STARTUP_FILE=/tmp/sgemaster.$$
      STARTUP_FILE_NAME=sgemaster
      S95NAME=S95sgemaster
      K03NAME=K03sgemaster
      DAEMON_NAME="qmaster/scheduler"
   elif [ $hosttype = "bdb" ]; then
      TMP_SGE_STARTUP_FILE=/tmp/sgebdb.$$
      STARTUP_FILE_NAME=sgebdb
      S95NAME=S94sgebdb
      K03NAME=K04sgebdb
      DAEMON_NAME="berkeleydb"
   else
      TMP_SGE_STARTUP_FILE=/tmp/sgeexecd.$$
      STARTUP_FILE_NAME=sgeexecd
      S95NAME=S96sgeexecd
      K03NAME=K02sgeexecd
      DAEMON_NAME="execd"
   fi

   SGE_STARTUP_FILE=$SGE_ROOT/$SGE_CELL/common/$STARTUP_FILE_NAME

   InstallRcScript 

   $INFOTEXT -wait -auto $AUTO -n "\nHit <RETURN> to continue >> "
   $CLEAR
}


InstallRcScript()
{
   if [ $euid != 0 ]; then
      return 0
   fi

   $INFOTEXT -u "\n%s startup script" $DAEMON_NAME

   # --- from here only if root installs ---
   $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
             "\nWe can install the startup script that will\n" \
             "start %s at machine boot (y/n) [y] >> " $DAEMON_NAME

   ret=$?
   if [ $AUTO = "true" -a $ADD_TO_RC = "false" ]; then
      $CLEAR
      return
   else
      if [ $ret = 1 ]; then
         return
      fi
   fi

   # If system is Linux Standard Base (LSB) compliant, use the install_initd utility
   if [ "$RC_FILE" = lsb ]; then
      echo cp $SGE_STARTUP_FILE $RC_PREFIX/$STARTUP_FILE_NAME
      echo /usr/lib/lsb/install_initd $RC_PREFIX/$STARTUP_FILE_NAME
      Execute cp $SGE_STARTUP_FILE $RC_PREFIX/$STARTUP_FILE_NAME
      Execute /usr/lib/lsb/install_initd $RC_PREFIX/$STARTUP_FILE_NAME
      # Several old Red Hat releases do not create proper startup links from LSB conform
      # scripts. So we need to check if the proper links were created.
      # See RedHat: https://bugzilla.redhat.com/bugzilla/long_list.cgi?buglist=106193
      if [ -f "/etc/redhat-release" -o -f "/etc/fedora-release" ]; then
         # According to Red Hat documentation all rcX.d directories are in /etc/rc.d
         # we hope this will never change for Red Hat
         RCD_PREFIX="/etc/rc.d"
         for runlevel in 0 1 2 3 4 5 6; do
            # check for a corrupted startup link
            if [ -L "$RCD_PREFIX/rc$runlevel.d/S-1$STARTUP_FILE_NAME" ]; then
               Execute rm -f $RCD_PREFIX/rc$runlevel.d/S-1$STARTUP_FILE_NAME
               # create new correct startup link
               if [ $runlevel -eq 3 -o $runlevel -eq 5 ]; then
                  Execute rm -f $RCD_PREFIX/rc$runlevel.d/$S95NAME
                  Execute ln -s $RC_PREFIX/$STARTUP_FILE_NAME $RCD_PREFIX/rc$runlevel.d/$S95NAME
               fi
            fi
            # check for a corrupted shutdown link
            if [ -L "$RCD_PREFIX/rc$runlevel.d/K-1$STARTUP_FILE_NAME" ]; then
               Execute rm -f $RCD_PREFIX/rc$runlevel.d/K-1$STARTUP_FILE_NAME
               Execute rm -f $RCD_PREFIX/rc$runlevel.d/$K03NAME
               # create new correct shutdown link
               if [ $runlevel -eq 0 -o $runlevel -eq 1 -o $runlevel -eq 2 -o $runlevel -eq 6 ]; then
                  Execute rm -f $RCD_PREFIX/rc$runlevel.d/$K03NAME
                  Execute ln -s $RC_PREFIX/$STARTUP_FILE_NAME $RCD_PREFIX/rc$runlevel.d/$K03NAME
               fi
            fi
         done
      fi
   # If we have System V we need to put the startup script to $RC_PREFIX/init.d
   # and make a link in $RC_PREFIX/rc2.d to $RC_PREFIX/init.d
   elif [ "$RC_FILE" = "sysv_rc" ]; then
      $INFOTEXT "Installing startup script %s and %s" "$RC_PREFIX/$RC_DIR/$S95NAME" "$RC_PREFIX/$RC_DIR/$K03NAME"
      Execute rm -f $RC_PREFIX/$RC_DIR/$S95NAME
      Execute rm -f $RC_PREFIX/$RC_DIR/$K03NAME
      Execute cp $SGE_STARTUP_FILE $RC_PREFIX/init.d/$STARTUP_FILE_NAME
      Execute chmod a+x $RC_PREFIX/init.d/$STARTUP_FILE_NAME
      Execute ln -s $RC_PREFIX/init.d/$STARTUP_FILE_NAME $RC_PREFIX/$RC_DIR/$S95NAME
      Execute ln -s $RC_PREFIX/init.d/$STARTUP_FILE_NAME $RC_PREFIX/$RC_DIR/$K03NAME

      # runlevel management in Linux is different -
      # each runlevel contains full set of links
      # RedHat uses runlevel 5 and SUSE runlevel 3 for xdm
      # RedHat uses runlevel 3 for full networked mode
      # Suse uses runlevel 2 for full networked mode
      # we already installed the script in level 3
      SGE_ARCH=`$SGE_UTIL/arch`
      case $SGE_ARCH in
      lx2?-*)
         runlevel=`grep "^id:.:initdefault:"  /etc/inittab | cut -f2 -d:`
         if [ "$runlevel" = 2 -o  "$runlevel" = 5 ]; then
            $INFOTEXT "Installing startup script also in %s and %s" "$RC_PREFIX/rc${runlevel}.d/$S95NAME" "$RC_PREFIX/rc${runlevel}.d/$K03NAME"
            Execute rm -f $RC_PREFIX/rc${runlevel}.d/$S95NAME
            Execute rm -f $RC_PREFIX/rc${runlevel}.d/$K03NAME
            Execute ln -s $RC_PREFIX/init.d/$STARTUP_FILE_NAME $RC_PREFIX/rc${runlevel}.d/$S95NAME
            Execute ln -s $RC_PREFIX/init.d/$STARTUP_FILE_NAME $RC_PREFIX/rc${runlevel}.d/$K03NAME
         fi
         ;;
       esac

   elif [ "$RC_FILE" = "insserv-linux" ]; then
      echo  cp $SGE_STARTUP_FILE $RC_PREFIX/$STARTUP_FILE_NAME
      echo /sbin/insserv $RC_PREFIX/$STARTUP_FILE_NAME
      Execute cp $SGE_STARTUP_FILE $RC_PREFIX/$STARTUP_FILE_NAME
      /sbin/insserv $RC_PREFIX/$STARTUP_FILE_NAME
   elif [ "$RC_FILE" = "update-rc.d" ]; then
      # let Debian install scripts according to defaults
      echo  cp $SGE_STARTUP_FILE $RC_PREFIX/$STARTUP_FILE_NAME
      echo /usr/sbin/update-rc.d $STARTUP_FILE_NAME
      Execute cp $SGE_STARTUP_FILE $RC_PREFIX/$STARTUP_FILE_NAME
      /usr/sbin/update-rc.d $STARTUP_FILE_NAME defaults 95 03
   elif [ "$RC_FILE" = "freebsd" ]; then
      echo  cp $SGE_STARTUP_FILE $RC_PREFIX/sge${RC_SUFFIX}
      Execute cp $SGE_STARTUP_FILE $RC_PREFIX/sge${RC_SUFFIX}
   elif [ "$RC_FILE" = "SGE" ]; then
      echo  mkdir -p "$RC_PREFIX/$RC_DIR"
      Execute mkdir -p "$RC_PREFIX/$RC_DIR"

cat << PLIST > "$RC_PREFIX/$RC_DIR/StartupParameters.plist"
{
   Description = "SUN Grid Engine";
   Provides = ("SGE");
   Requires = ("Disks", "NFS", "Resolver");
   Uses = ("NetworkExtensions");
   OrderPreference = "Late";
   Messages =
   {
     start = "Starting SUN Grid Engine";
     stop = "Stopping SUN Grid Engine";
     restart = "Restarting SUN Grid Engine";
   };
}
PLIST

     if [ $hosttype = "master" ]; then
        DARWIN_GEN_REPLACE="#GENMASTERRC"
     elif [ $hosttype = "bdb" ]; then
        DARWIN_GEN_REPLACE="#GENBDBRC"
     else
        DARWIN_GEN_REPLACE="#GENEXECDRC"
     fi

     if [ -f "$RC_PREFIX/$RC_DIR/$RC_FILE" ]; then
        DARWIN_TEMPLATE="$RC_PREFIX/$RC_DIR/$RC_FILE"
     else
        DARWIN_TEMPLATE="util/rctemplates/darwin_template"
     fi

     Execute sed -e "s%${DARWIN_GEN_REPLACE}%${SGE_STARTUP_FILE}%g" \
          "$DARWIN_TEMPLATE" > "$RC_PREFIX/$RC_DIR/$RC_FILE.$$"
     Execute chmod a+x "$RC_PREFIX/$RC_DIR/$RC_FILE.$$"
     Execute mv "$RC_PREFIX/$RC_DIR/$RC_FILE.$$" "$RC_PREFIX/$RC_DIR/$RC_FILE"
   else
      # if this is not System V we simple add the call to the
      # startup script to RC_FILE

      # Start-up script already installed?
      #------------------------------------
      grep $STARTUP_FILE_NAME $RC_FILE > /dev/null 2>&1
      status=$?
      if [ $status != 0 ]; then
         cat $RC_FILE | sed -e "s/exit 0//g" > $RC_FILE.new.1 2>/dev/null
         cp $RC_FILE $RC_FILE.save_sge
         cp $RC_FILE.new.1 $RC_FILE
         $INFOTEXT "Adding application startup to %s" $RC_FILE
         # Add the procedure
         #------------------
         $ECHO "" >> $RC_FILE
         $ECHO "# Grid Engine start up" >> $RC_FILE
         $ECHO "#-$LINE---------" >> $RC_FILE
         $ECHO $SGE_STARTUP_FILE >> $RC_FILE
         $ECHO "exit 0" >> $RC_FILE
         rm $RC_FILE.new.1
      else
         $INFOTEXT "Found a call of %s in %s. Replacing with new call.\n" \
                   "Your old file %s is saved as %s" $STARTUP_FILE_NAME $RC_FILE $RC_FILE $RC_FILE.org.1

         mv $RC_FILE.org.3 $RC_FILE.org.4    2>/dev/null
         mv $RC_FILE.org.2 $RC_FILE.org.3    2>/dev/null
         mv $RC_FILE.org.1 $RC_FILE.org.2    2>/dev/null

         # save original file modes of RC_FILE
         uid=`$SGE_UTILBIN/filestat -uid $RC_FILE`
         gid=`$SGE_UTILBIN/filestat -gid $RC_FILE`
         perm=`$SGE_UTILBIN/filestat -mode $RC_FILE`

         Execute cp $RC_FILE $RC_FILE.org.1

         savedfile=`basename $RC_FILE`

         sed -e "s%.*$STARTUP_FILE_NAME.*%$SGE_STARTUP_FILE%" \
                 $RC_FILE > /tmp/$savedfile.1

         Execute cp /tmp/$savedfile.1 $RC_FILE
         Execute chown $uid $RC_FILE
         Execute chgrp $gid $RC_FILE
         Execute chmod $perm $RC_FILE
         Execute rm -f /tmp/$savedfile.1
      fi
   fi
}



#-------------------------------------------------------------------------
# AddDefaultManager
#
AddDefaultManager()
{
   ExecuteAsAdmin $SPOOLDEFAULTS managers $*
#  TruncCreateAndMakeWriteable $QMDIR/managers
#  $ECHO $1 >> $QMDIR/managers
#  SetPerm $QMDIR/managers
}


#-------------------------------------------------------------------------
# AddDefaultOperator
#
AddDefaultOperator()
{
   ExecuteAsAdmin $SPOOLDEFAULTS operators $*
}

MoveLog()
{
   if [ "$AUTO" = "false" ]; then
      return
   fi

   #due to problems with adminrun and ADMINUSER permissions, on windows systems
   #the auto install log files couldn't be copied to qmaster_spool_dir
   # leaving log file in /tmp dir. There is a need for a better solution
   if [ "$SGE_ARCH" = "win32-x86" ]; then
      RestoreStdout
      $INFOTEXT "Check %s to get the install log!" /tmp/$LOGSNAME
      return
   fi

   if [ "$BACKUP" = "true" -a "$AUTO" = "true" ]; then
      ExecuteAsAdmin cp /tmp/$LOGSNAME $backup_dir/backup.log 
      rm -f /tmp/$LOGSNAME 
      return   
   fi

   if [ -f $SGE_ROOT/$SGE_CELL/common/bootstrap ]; then
      master_spool_dir=`cat $SGE_ROOT/$SGE_CELL/common/bootstrap | grep qmaster_spool_dir | awk '{ print $2 }'`

      if [ -d $master_spool_dir ]; then
         if [ $EXECD = "uninstall" -o $QMASTER = "uninstall" ]; then
            if [ -f /tmp/$LOGSNAME ]; then
               ExecuteAsAdmin cp -f /tmp/$LOGSNAME $master_spool_dir/uninstall_`hostname`_$DATE.log 2>&1
            fi
            RestoreStdout
            $INFOTEXT "Install log can be found in: %s" $master_spool_dir/uninstall_`hostname`_$DATE.log
         else
            if [ -f /tmp/$LOGSNAME ]; then
               ExecuteAsAdmin cp -f /tmp/$LOGSNAME $master_spool_dir/install_`hostname`_$DATE.log 2>&1
            fi
            RestoreStdout
            $INFOTEXT "Install log can be found in: %s" $master_spool_dir/install_`hostname`_$DATE.log
         fi
         if [ -f /tmp/$LOGSNAME ]; then
            rm -f /tmp/$LOGSNAME 2>&1
         fi
      else
         RestoreStdout
         $INFOTEXT "%s does not exist.\n Please check your installation!" $master_spool_dir
         $INFOTEXT "Check %s to get the install log!" /tmp/$LOGSNAME
      fi
   else
         RestoreStdout
         $INFOTEXT "%s does not exist.\n Qmaster isn't installed, yet!\nPlease check your installation!" $SGE_ROOT/$SGE_CELL/common/bootstrap 
         $INFOTEXT "Check %s to get the install log!" /tmp/$LOGSNAME 
   fi
}


CreateLog()
{
LOGSNAME=install.$$
DATE=`date '+%Y-%m-%d_%H:%M:%S'`

if [ -f /tmp/$LOGSNAME ]; then
   rm /tmp/$LOGSNAME
   touch /tmp/$LOGSNAME
else
   touch /tmp/$LOGSNAME
fi
}


Stdout2Log()
{
   if [ "$STDOUT2LOG" = "0" ]; then
      CLEAR=:
      SGE_NOMSG=1
      export SGE_NOMSG
      CreateLog
      # make Filedescriptor(FD) 4 a copy of stdout (FD 1)
      exec 4>&1
      # open logfile for writing
      exec 1> /tmp/$LOGSNAME 2>&1
      STDOUT2LOG=1
   fi
}


RestoreStdout()
{
   if [ "$STDOUT2LOG" = "1" ]; then
      unset SGE_NOMSG
      # close file logfile 
      exec 1>&-
      # make stdout a copy of FD 4 (reset stdout)
      exec 1>&4
      # close FD4
      exec 4>&-
      STDOUT2LOG=0
   fi
}
#-------------------------------------------------------------------------
# CheckRunningDaemon
#
CheckRunningDaemon()
{
   daemon_name=$1

   case $daemon_name in

      sge_qmaster )
       if [ -f $QMDIR/qmaster.pid ]; then
          daemon_pid=`cat $QMDIR/qmaster.pid`
          $SGE_UTILBIN/checkprog $daemon_pid $daemon_name > /dev/null
          return $?      
       fi
      ;;

      sge_schedd )
       if [ -f $QMDIR/schedd/schedd.pid ]; then
          daemon_pid=`cat $QMDIR/schedd/schedd.pid`
          $SGE_UTILBIN/checkprog $daemon_pid $daemon_name > /dev/null
          return $?
       fi
      ;;

      sge_execd )
       h=`hostname`
       $SGE_BIN/qping -info $h $SGE_EXECD_PORT execd 1 > /dev/null
       return $?      
      ;;

      sge_shadowd )

      ;;
   esac


}

#----------------------------------------------------------------------------
# Backup configuration
# BackupConfig
#
BackupConfig()
{
   DATE=`date '+%Y-%m-%d_%H_%M_%S'`
   BUP_BDB_COMMON_FILE_LIST_TMP="accounting bootstrap qtask settings.sh act_qmaster sgemaster host_aliases settings.csh sgeexecd sgebdb shadow_masters"
   BUP_BDB_COMMON_DIR_LIST_TMP="sgeCA"
   BUP_BDB_SPOOL_FILE_LIST_TMP="jobseqnum"
   BUP_CLASSIC_COMMON_FILE_LIST_TMP="configuration sched_configuration accounting bootstrap qtask settings.sh act_qmaster sgemaster host_aliases settings.csh sgeexecd shadow_masters"
   BUP_CLASSIC_DIR_LIST_TMP="sgeCA local_conf" 
   BUP_CLASSIC_SPOOL_FILE_LIST_TMP="jobseqnum admin_hosts calendars centry ckpt cqueues exec_hosts hostgroups managers operators pe projects qinstances schedd submit_hosts usermapping users usersets zombies"
   BUP_COMMON_FILE_LIST=""
   BUP_SPOOL_FILE_LIST=""
   BUP_SPOOL_DIR_LIST=""

   if [ "$AUTO" = "true" ]; then
      Stdout2Log
   fi

   $INFOTEXT -u "SGE Configuration Backup"
   $INFOTEXT -n "\nThis feature does a backup of all configuration you made\n" \
                "within your cluster."
                if [ $AUTO != "true" ]; then
                   SGE_ROOT=`pwd`
                fi
   $INFOTEXT -n "\nPlease enter your SGE_ROOT directory. \nDefault: [%s]" $SGE_ROOT 
                SGE_ROOT=`Enter $SGE_ROOT`
   $INFOTEXT -n "\nPlease enter your SGE_CELL name. Default: [default]"
                if [ $AUTO != "true" ]; then
                   SGE_CELL=`Enter default`
                fi

   $INFOTEXT -log "SGE_ROOT: %s" $SGE_ROOT
   $INFOTEXT -log "SGE_CELL: %s" $SGE_CELL

   BackupCheckBootStrapFile
   CheckArchBins
   SetBackupDir


   $INFOTEXT  -auto $AUTO -ask "y" "n" -def "y" -n "\nIf you are using different tar versions (gnu tar/ solaris tar), this option\n" \
                                                     "can make some trouble. In some cases the tar packages may be corrupt.\n" \
                                                     "Using the same tar binary for packing and unpacking works without problems!\n\n" \
                                                     "Shall the backup function create a compressed tarpackage with your files? (y/n) [y] >>"

   if [ $? = 0 -a $AUTO != "true" ]; then
      TAR=true
   else
      TAR=$TAR
   fi

   DoBackup 
   CreateTarArchive

   $INFOTEXT -n "\n... backup completed"
   $INFOTEXT -n "\nAll information is saved in \n[%s]\n\n" $backup_dir

   if [ "$AUTO" = "true" ]; then
      MoveLog
   fi  
   exit 0
}



#----------------------------------------------------------------------------
# Restore configuration
# RestoreConfig
#
RestoreConfig()
{
   DATE=`date '+%H_%M_%S'`
   BUP_COMMON_FILE_LIST="accounting bootstrap qtask settings.sh act_qmaster sgemaster host_aliases settings.csh sgeexecd sgebdb shadow_masters"
   BUP_COMMON_DIR_LIST="sgeCA"
   BUP_SPOOL_FILE_LIST="jobseqnum"
   BUP_CLASSIC_COMMON_FILE_LIST="configuration sched_configuration accounting bootstrap qtask settings.sh act_qmaster sgemaster host_aliases settings.csh sgeexecd shadow_masters"
   BUP_CLASSIC_DIR_LIST="sgeCA local_conf" 
   BUP_CLASSIC_SPOOL_FILE_LIST="jobseqnum admin_hosts calendars centry ckpt cqueues exec_hosts hostgroups managers operators pe projects qinstances schedd submit_hosts usermapping users usersets zombies"

   MKDIR="mkdir -p"
   CP="cp -f"
   CPR="cp -fR"

   $INFOTEXT -u "SGE Configuration Restore"
   $INFOTEXT -n "\nThis feature restores the configuration from a backup you made\n" \
                "previously.\n\n"

   $INFOTEXT -wait -n "Hit, <ENTER> to continue!" 
   $CLEAR
                SGE_ROOT=`pwd`
   $INFOTEXT -n "\nPlease enter your SGE_ROOT directory. \nDefault: [%s]" $SGE_ROOT
                SGE_ROOT=`Enter $SGE_ROOT`
   $INFOTEXT -n "\nPlease enter your SGE_CELL name. Default: [default]"
                SGE_CELL=`Enter default`

   export SGE_ROOT
   export SGE_CELL

   $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n "\nIs your backupfile in tar.gz[Z] format? (y/n) [y] "

   
   if [ $? = 0 ]; then
      ExtractBackup

      cd $SGE_ROOT
      RestoreCheckBootStrapFile "/tmp/bup_tmp_$DATE/"
      CheckArchBins

      if [ "$spooling_method" = "berkeleydb" ]; then
         if [ $is_rpc = 0 ]; then
            $INFOTEXT -n "\nThe path to your spooling db is [%s]" $db_home
            $INFOTEXT -n "\nIf this is correct hit <ENTER> to continue, else enter the path. >>"
            db_home=`Enter $db_home`
         fi

         #reinitializing berkeley db
         if [ -d $db_home ]; then
            for f in `ls $db_home`; do
                  ExecuteAsAdmin rm $db_home/$f
            done
         else
            ExecuteAsAdmin $MKDIR $db_home
         fi

         SwitchArchRst /tmp/bup_tmp_$DATE/

            if [ -d $SGE_ROOT/$SGE_CELL ]; then
               if [ -d $SGE_ROOT/$SGE_CELL/common ]; then
                  :
               else
                  ExecuteAsAdmin $MKDIR $SGE_ROOT/$SGE_CELL/common
               fi
            else
               ExecuteAsAdmin $MKDIR $SGE_ROOT/$SGE_CELL
               ExecuteAsAdmin $MKDIR $SGE_ROOT/$SGE_CELL/common
            fi

         for f in $BUP_COMMON_FILE_LIST; do
            if [ -f /tmp/bup_tmp_$DATE/$f ]; then
               ExecuteAsAdmin $CP /tmp/bup_tmp_$DATE/$f $SGE_ROOT/$SGE_CELL/common/
            fi
         done

         for f in $BUP_COMMON_DIR_LIST; do
            if [ -d /tmp/bup_tmp_$DATE/$f ]; then
               ExecuteAsAdmin $CPR /tmp/bup_tmp_$DATE/$f $SGE_ROOT/$SGE_CELL/common/
            fi
         done

         if [ -d $master_spool ]; then
            if [ -d $master_spool/job_scipts ]; then
               :
            else
               ExecuteAsAdmin $MKDIR $master_spool/job_scripts
            fi
         else
            ExecuteAsAdmin $MKDIR $master_spool
            ExecuteAsAdmin $MKDIR $master_spool/job_scripts
         fi

         for f in $BUP_SPOOL_FILE_LIST; do
            if [ -f /tmp/bup_tmp_$DATE/$f ]; then
               ExecuteAsAdmin $CP /tmp/bup_tmp_$DATE/$f $master_spool
            fi
         done
      else

         if [ -d $SGE_ROOT/$SGE_CELL ]; then
            if [ -d $SGE_ROOT/$SGE_CELL/common ]; then
               :
            else
               ExecuteAsAdmin $MKDIR $SGE_ROOT/$SGE_CELL/common
            fi
         else
            ExecuteAsAdmin $MKDIR $SGE_ROOT/$SGE_CELL
            ExecuteAsAdmin $MKDIR $SGE_ROOT/$SGE_CELL/common
         fi

         for f in $BUP_CLASSIC_COMMON_FILE_LIST; do
            if [ -f /tmp/bup_tmp_$DATE/$f ]; then
               ExecuteAsAdmin $CP /tmp/bup_tmp_$DATE/$f $SGE_ROOT/$SGE_CELL/common/
            fi
         done

         master_spool_tmp=`echo $master_spool | cut -d";" -f2`
         if [ -d $master_spool_tmp ]; then
            if [ -d $master_spool_tmp/job_scipts ]; then
               :
            else
               ExecuteAsAdmin $MKDIR $master_spool_tmp/job_scripts
            fi
         else
            ExecuteAsAdmin $MKDIR $master_spool_tmp
            ExecuteAsAdmin $MKDIR $master_spool_tmp/job_scripts
         fi

         for f in $BUP_CLASSIC_SPOOL_FILE_LIST; do
            if [ -f /tmp/bup_tmp_$DATE/$f -o -d /tmp/bup_tmp_$DATE/$f ]; then
               ExecuteAsAdmin $CPR /tmp/bup_tmp_$DATE/$f $master_spool_tmp
            fi
         done

         for f in $BUP_CLASSIC_DIR_LIST; do
            if [ -d /tmp/bup_tmp_$DATE/$f ]; then
               ExecuteAsAdmin $CPR /tmp/bup_tmp_$DATE/$f $SGE_ROOT/$SGE_CELL/common
            fi
         done

         for f in $BUP_CLASSIC_COMMON_FILE_LIST; do
            if [ -d /tmp/bup_tmp_$DATE/$f ]; then
               ExecuteAsAdmin $CP /tmp/bup_tmp_$DATE/$f $SGE_ROOT/$SGE_CELL/common
            fi
         done
 
      fi

      $INFOTEXT -n "\nYour configuration has been restored\n"
      rm -fR /tmp/bup_tmp_$DATE
   else
      loop_stop="false"
      while [ $loop_stop = "false" ]; do
         $INFOTEXT -n "\nPlease enter the full path to your backup files." \
                      "\nDefault: [%s]" $SGE_ROOT/backup
         bup_file=`Enter $SGE_ROOT/backup`

         if [ -d $bup_file ]; then
            loop_stop="true"
         else
            $INFOTEXT -n "\n%s does not exist!\n" $bup_file
            loop_stop="false"
         fi
      done

      RestoreCheckBootStrapFile $bup_file
      CheckArchBins
  
      if [ "$spooling_method" = "berkeleydb" ]; then 
         if [ "$is_rpc" = 0 ]; then
            $INFOTEXT -n "\nThe path to your spooling db is [%s]" $db_home
            $INFOTEXT -n "\nIf this is correct hit <ENTER> to continue, else enter the path. >> "
            db_home=`Enter $db_home`
         fi

         #reinitializing berkeley db
         if [ -d $db_home ]; then
            for f in `ls $db_home`; do
                  ExecuteAsAdmin rm $db_home/$f
            done
         else
            ExecuteAsAdmin $MKDIR $db_home
         fi

         SwitchArchRst $bup_file

            if [ -d $SGE_ROOT/$SGE_CELL ]; then
               if [ -d $SGE_ROOT/$SGE_CELL/common ]; then
                  :
               else
                  ExecuteAsAdmin $MKDIR $SGE_ROOT/$SGE_CELL/common
               fi
            else
               ExecuteAsAdmin $MKDIR $SGE_ROOT/$SGE_CELL
               ExecuteAsAdmin $MKDIR $SGE_ROOT/$SGE_CELL/common
            fi

         for f in $BUP_COMMON_FILE_LIST; do
            if [ -f $bup_file/$f ]; then
               ExecuteAsAdmin $CP $bup_file/$f $SGE_ROOT/$SGE_CELL/common/
            fi
         done

         if [ -d $master_spool ]; then
            if [ -d $master_spool/job_scipts ]; then
               :
            else
               ExecuteAsAdmin $MKDIR $master_spool/job_scripts
            fi
         else
            ExecuteAsAdmin $MKDIR $master_spool
            ExecuteAsAdmin $MKDIR $master_spool/job_scripts
         fi

         for f in $BUP_SPOOL_FILE_LIST; do
            if [ -f $bup_file/$f ]; then
               ExecuteAsAdmin $CP $bup_file/$f $master_spool
            fi
         done      
      else

         if [ -d $SGE_ROOT/$SGE_CELL ]; then
            if [ -d $SGE_ROOT/$SGE_CELL/common ]; then
               :
            else
               ExecuteAsAdmin $MKDIR $SGE_ROOT/$SGE_CELL/common
            fi
         else
            ExecuteAsAdmin $MKDIR $SGE_ROOT/$SGE_CELL
            ExecuteAsAdmin $MKDIR $SGE_ROOT/$SGE_CELL/common
         fi

         for f in $BUP_CLASSIC_COMMON_FILE_LIST; do
            if [ -f $bup_file/$f ]; then
               ExecuteAsAdmin $CP $bup_file/$f $SGE_ROOT/$SGE_CELL/common/
            fi
         done

         master_spool_tmp=`echo $master_spool | cut -d";" -f2`
         if [ -d $master_spool_tmp ]; then
            if [ -d $master_spool_tmp/job_scipts ]; then
               :
            else
               ExecuteAsAdmin $MKDIR $master_spool_tmp/job_scripts
            fi
         else
            ExecuteAsAdmin $MKDIR $master_spool_tmp
            ExecuteAsAdmin $MKDIR $master_spool_tmp/job_scripts
         fi

         for f in $BUP_CLASSIC_SPOOL_FILE_LIST; do
            if [ -f $bup_file/$f -o -d $bup_file/$f ]; then
               ExecuteAsAdmin $CPR $bup_file/$f $master_spool_tmp
            fi
         done

         for f in $BUP_CLASSIC_DIR_LIST; do
            if [ -d $bup_file/$f ]; then
               ExecuteAsAdmin $CPR $bup_file/$f $SGE_ROOT/$SGE_CELL/common
            fi
         done

         for f in $BUP_CLASSIC_COMMON_FILE_LIST; do
            if [ -f $bup_file/$f ]; then
               ExecuteAsAdmin $CP $bup_file/$f $SGE_ROOT/$SGE_CELL/common
            fi
         done

      fi
      $INFOTEXT -n "\nYour configuration has been restored\n"
   fi
}



SwitchArchBup()
{
      if [ "$is_rpc" = 1 -a "$SGE_ARCH" = "sol-sparc64" ]; then
         OLD_LD_PATH=$LD_LIBRARY_PATH
         LD_LIBRARY_PATH="$OLD_LD_PATH:./lib/sol-sparc"
         export LD_LIBRARY_PATH
         DUMPIT="$SGE_ROOT/utilbin/sol-sparc/db_dump -f"
         ExecuteAsAdmin $DUMPIT $backup_dir/$DATE.dump -h $db_home sge
         LD_LIBRARY_PATH="$OLD_LD_PATH:./lib/sol-sparc64"
         export LD_LIBRARY_PATH
      else
         DUMPIT="$SGE_UTILBIN/db_dump -f"
         ExecuteAsAdmin $DUMPIT $backup_dir/$DATE.dump -h $db_home sge
      fi

}



SwitchArchRst()
{
   dump_dir=$1

         if [ "$is_rpc" = 1 -a "$SGE_ARCH" = "sol-sparc64" ]; then
            OLD_LD_PATH=$LD_LIBRARY_PATH
            LD_LIBRARY_PATH="$OLD_LD_PATH:./lib/sol-sparc"
            export LD_LIBRARY_PATH
            DB_LOAD="$SGE_ROOT/utilbin/sol-sparc/db_load -f"
            ExecuteAsAdmin $DB_LOAD $dump_dir/*.dump -h $db_home sge
            LD_LIBRARY_PATH="$OLD_LD_PATH:./lib/sol-sparc64"
            export LD_LIBRARY_PATH
         else
            DB_LOAD="$SGE_UTILBIN/db_load -f" 
            ExecuteAsAdmin $DB_LOAD $dump_dir/*.dump -h $db_home sge
         fi
}



CheckArchBins()
{
   if [ "$is_rpc" = 1 -a "$SGE_ARCH" = "sol-sparc64" ]; then
      DB_BIN="$SGE_ROOT/utilbin/sol-sparc/db_load $SGE_ROOT/utilbin/sol-sparc/db_dump"
      DB_LIB="$SGE_ROOT/lib/sol-sparc/libdb-4.2.so"
      for db in $DB_BIN; do 
         if [ -f $db ]; then
            :
         else
            $INFOTEXT "32 bit version of db_load or db_dump not found. These binaries needs \n" \
                      "to be installed to perform a backup/restore of your BDB RPC Server. \n" \
                      "Exiting backup/restore now"
            $INFOTEXT -log "32 bit version of db_load or db_dump not found. These binaries needs \n" \
                           "to be installed to perform a backup/restore of your BDB RPC Server. \n" \
                           "Exiting backup/restore now"

            exit 1
         fi
      done
      if [ -f $DB_LIB ]; then
         :
      else
            $INFOTEXT "32 bit version of lib_db not found. These library needs \n" \
                      "to be installed to perform a backup/restore of your BDB RPC Server. \n" \
                      "Exiting backup/restore now"
            $INFOTEXT -log "32 bit version of lib_db not found. These library needs \n" \
                           "to be installed to perform a backup/restore of your BDB RPC Server. \n" \
                           "Exiting backup/restore now"
            exit 1
      fi
   fi
}



RemoveRcScript()
{
   host=$1
   hosttype=$2
   euid=$3
   

   $INFOTEXT "Checking for installed rc startup scripts!\n"


   if [ $hosttype = "master" ]; then
      TMP_SGE_STARTUP_FILE=/tmp/sgemaster.$$
      STARTUP_FILE_NAME=sgemaster
      S95NAME=S95sgemaster
      K03NAME=K03sgemaster
      DAEMON_NAME="qmaster/scheduler"
   elif [ $hosttype = "bdb" ]; then
      TMP_SGE_STARTUP_FILE=/tmp/sgebdb.$$
      STARTUP_FILE_NAME=sgebdb
      S95NAME=S94sgebdb
      K03NAME=K04sgebdb
      DAEMON_NAME="berkeleydb"
   else
      TMP_SGE_STARTUP_FILE=/tmp/sgeexecd.$$
      STARTUP_FILE_NAME=sgeexecd
      S95NAME=S96sgeexecd
      K03NAME=K02sgeexecd
      DAEMON_NAME="execd"
   fi

   SGE_STARTUP_FILE=$SGE_ROOT/$SGE_CELL/common/$STARTUP_FILE_NAME


   if [ $euid != 0 ]; then
      return 0
   fi

   $INFOTEXT -u "\nRemoving %s startup script" $DAEMON_NAME

   # --- from here only if root installs ---
   $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
             "\nDo you want to remove the startup script \n" \
             "for %s at this machine? (y/n) [y] >> " $DAEMON_NAME

   ret=$?
   if [ $AUTO = "true" -a $REMOVE_RC = "false" ]; then
      $CLEAR
      return
   else
      if [ $ret = 1 ]; then
         $CLEAR
         return
      fi
   fi

   # If system is Linux Standard Base (LSB) compliant, use the install_initd utility
   if [ "$RC_FILE" = lsb ]; then
      echo /usr/lib/lsb/remove_initd $RC_PREFIX/$STARTUP_FILE_NAME
      Execute /usr/lib/lsb/remove_initd $RC_PREFIX/$STARTUP_FILE_NAME
      # Several old Red Hat releases do not create/remove startup links from LSB conform
      # scripts. So we need to check if the links were deleted.
      # See RedHat: https://bugzilla.redhat.com/bugzilla/long_list.cgi?buglist=106193
      if [ -f "/etc/redhat-release" -o -f "/etc/fedora-release" ]; then
         RCD_PREFIX="/etc/rc.d"
         # Are all startup links correctly removed?
         for runlevel in 3 5; do
            if [ -L "$RCD_PREFIX/rc$runlevel.d/$S95NAME" ]; then
               Execute rm -f $RCD_PREFIX/rc$runlevel.d/$S95NAME
            fi
         done
         # Are all shutdown links correctly removed?
         for runlevel in 0 1 2 6; do
            if [ -L "$RCD_PREFIX/rc$runlevel.d/$K03NAME" ]; then
               Execute rm -f $RCD_PREFIX/rc$runlevel.d/$K03NAME
            fi
         done
      fi
   # If we have System V we need to put the startup script to $RC_PREFIX/init.d
   # and make a link in $RC_PREFIX/rc2.d to $RC_PREFIX/init.d
   elif [ "$RC_FILE" = "sysv_rc" ]; then
      $INFOTEXT "Removing startup script %s and %s" "$RC_PREFIX/$RC_DIR/$S95NAME" "$RC_PREFIX/$RC_DIR/$K03NAME"
      Execute rm -f $RC_PREFIX/$RC_DIR/$S95NAME
      Execute rm -f $RC_PREFIX/$RC_DIR/$K03NAME
      Execute rm -f $RC_PREFIX/init.d/$STARTUP_FILE_NAME

      # runlevel management in Linux is different -
      # each runlevel contains full set of links
      # RedHat uses runlevel 5 and SUSE runlevel 3 for xdm
      # RedHat uses runlevel 3 for full networked mode
      # Suse uses runlevel 2 for full networked mode
      # we already installed the script in level 3
      SGE_ARCH=`$SGE_UTIL/arch`
      case $SGE_ARCH in
      lx2?-*)
         runlevel=`grep "^id:.:initdefault:"  /etc/inittab | cut -f2 -d:`
         if [ "$runlevel" = 2 -o  "$runlevel" = 5 ]; then
            $INFOTEXT "Removing startup script %s and %s" "$RC_PREFIX/rc${runlevel}.d/$S95NAME" "$RC_PREFIX/rc${runlevel}.d/$K03NAME"
            Execute rm -f $RC_PREFIX/rc${runlevel}.d/$S95NAME
            Execute rm -f $RC_PREFIX/rc${runlevel}.d/$K03NAME
         fi
         ;;
       esac

   elif [ "$RC_FILE" = "insserv-linux" ]; then
      echo  rm $SGE_STARTUP_FILE $RC_PREFIX/$STARTUP_FILE_NAME
      echo /sbin/insserv -r $RC_PREFIX/$STARTUP_FILE_NAME
      Execute rm $SGE_STARTUP_FILE $RC_PREFIX/$STARTUP_FILE_NAME
      /sbin/insserv -r $RC_PREFIX/$STARTUP_FILE_NAME
   elif [ "$RC_FILE" = "freebsd" ]; then
      echo  rm $SGE_STARTUP_FILE $RC_PREFIX/sge${RC_SUFFIX}
      Execute rm $SGE_STARTUP_FILE $RC_PREFIX/sge${RC_SUFFIX}
   elif [ "$RC_FILE" = "SGE" ]; then
      if [ $hosttype = "master" ]; then
        DARWIN_GEN_REPLACE="#GENMASTERRC"
      elif [ $hosttype = "bdb" ]; then
        DARWIN_GEN_REPLACE="#GENBDBRC"
      else
        DARWIN_GEN_REPLACE="#GENEXECDRC"
      fi

      Execute sed -e "s%${SGE_STARTUP_FILE}%${DARWIN_GEN_REPLACE}%g" \
          "$RC_PREFIX/$RC_DIR/$RC_FILE" > "$RC_PREFIX/$RC_DIR/$RC_FILE.$$"
      Execute chmod a+x "$RC_PREFIX/$RC_DIR/$RC_FILE.$$"
      Execute mv "$RC_PREFIX/$RC_DIR/$RC_FILE.$$" "$RC_PREFIX/$RC_DIR/$RC_FILE"

      if [ "`grep '#GENMASTERRC' $RC_PREFIX/$RC_DIR/$RC_FILE`" = "" -o \
           "`grep '#GENBDBRC' $RC_PREFIX/$RC_DIR/$RC_FILE`" = "" -o \
           "`grep '#GENEXECDRC' $RC_PREFIX/$RC_DIR/$RC_FILE`" ]; then
         echo rm -rf "$RC_PREFIX/$RC_DIR"
         Execute  rm -rf "$RC_PREFIX/$RC_DIR"
      fi
   else
      # if this is not System V we simple add the call to the
      # startup script to RC_FILE

      # Start-up script already installed?
      #------------------------------------
      grep $STARTUP_FILE_NAME $RC_FILE > /dev/null 2>&1
      status=$?
      if [ $status = 0 ]; then
         mv $RC_FILE.sge_uninst.3 $RC_FILE.sge_uninst.4
         mv $RC_FILE.sge_uninst.2 $RC_FILE.sge_uninst.3
         mv $RC_FILE.sge_uninst.1 $RC_FILE.sge_uninst.2
         mv $RC_FILE.sge_uninst $RC_FILE.sge_uninst.1

         cat $RC_FILE | sed -e "s/# Grid Engine start up//g" | sed -e "s/$SGE_STARTUP_FILE//g"  > $RC_FILE.new.1 2>/dev/null
         cp $RC_FILE $RC_FILE.sge_uninst
         cp $RC_FILE.new.1 $RC_FILE
         $INFOTEXT "Application removed from %s" $RC_FILE
         
         rm $RC_FILE.new.1
      fi
   fi

   $INFOTEXT -wait -auto $AUTO -n "\nHit <RETURN> to continue >> "
   $CLEAR
}

CheckMasterHost()
{
   if [ -f $SGE_ROOT/$SGE_CELL/common/act_qmaster ]; then
      MASTER=`cat $SGE_ROOT/$SGE_CELL/common/act_qmaster`
   else
      $INFOTEXT -n "Can't find the act_qmaster file! Check your installation!"
   fi

   THIS_HOST=`hostname`

   if [ ${THIS_HOST%%.*} = ${MASTER%%.*} ]; then
      :
   else
      $INFOTEXT -n "This is not a master host. Please execute backup/restore on master host.\n"
      exit 1
   fi


}

BackupCheckBootStrapFile()
{
   if [ -f $SGE_ROOT/$SGE_CELL/common/bootstrap ]; then
      spooling_method=`cat $SGE_ROOT/$SGE_CELL/common/bootstrap | grep "spooling_method" | awk '{ print $2 }'`
      db_home=`cat $SGE_ROOT/$SGE_CELL/common/bootstrap | grep "spooling_params" | awk '{ print $2 }'`
      master_spool=`cat $SGE_ROOT/$SGE_CELL/common/bootstrap | grep "qmaster_spool_dir" | awk '{ print $2 }'`
      GetAdminUser

      if [ `echo $db_home | cut -d":" -f2` = "$db_home" ]; then
         $INFOTEXT -n "\nSpooling Method: %s detected!\n" $spooling_method
         is_rpc=0
      else
         is_rpc=1
         BDB_SERVER=`echo $db_home | cut -d":" -f1`
         BDB_BASEDIR=`echo $db_home | cut -d":" -f2`

         if [ -f $SGE_ROOT/$SGE_CELL/common/sgebdb ]; then
            BDB_HOME=`cat $SGE_ROOT/$SGE_CELL/common/sgebdb | grep $BDB_BASEDIR | grep BDBHOMES | cut -d" " -f2 | sed -e s/\"//`
         else
            $INFOTEXT -n "Your Berkeley DB home directory could not be detected!\n"
            $INFOTEXT -n "Please enter your Berkeley DB home directory >>" BDB_HOME=`Enter `
         fi

         $INFOTEXT -n "\nThe following settings could be detected.\n"
         $INFOTEXT -n "Spooling Method: Berkeley DB RPC Server spooling.\n"
         $INFOTEXT -n "Berkeley DB Server host: %s\n" $BDB_SERVER   
         $INFOTEXT -n "Berkeley DB home directory: %s\n\n" $BDB_HOME

         $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n "Are all settings right? (y/n) [y] >>"
         if [ $? = 1 ]; then
            $INFOTEXT -n "Please enter your Berkeley DB Server host. >>" 
            BDB_SERVER=`Enter`
            $INFOTEXT -n "Please enter your Berkeley DB home directory. >>" 
            BDB_HOME=`Enter`
         fi
      
         if [ `hostname` != "$BDB_SERVER" ]; then
            $INFOTEXT -n "You're not on the BDB Server host.\nPlease start the backup on the Server host again!\n"
            $INFOTEXT -n "Exiting backup!\n"
            exit 1
         fi
      db_home=$BDB_HOME
      fi
   else
      $INFOTEXT -n "bootstrap file could not be found in:\n %s !\n" $SGE_ROOT/$SGE_CELL/common
      $INFOTEXT -n "please check your installation! Exiting backup!\n"

      $INFOTEXT -log "bootstrap file could not be found in:\n %s !\n" $SGE_ROOT/$SGE_CELL/common
      $INFOTEXT -log "please check your installation! Exiting backup!\n"

      exit 1
   fi
}


SetBackupDir()
{
   $CLEAR
   loop_stop=false
   while [ $loop_stop = "false" ]; do
      $INFOTEXT -n "\nWhere do you want to save the backupfiles? \nDefault: [%s]" $SGE_ROOT/backup

                   if [ $AUTO != "true" ]; then
                      backup_dir=`Enter $SGE_ROOT/backup`
                      if [ -d $backup_dir ]; then
                         $INFOTEXT -n "\n The directory [%s] \nalready exists!\n" $backup_dir
                         $INFOTEXT  -auto $AUTO -ask "y" "n" -def "n" -n "Do you want to overwrite the existing backup directory? (y/n) [n] >>"
                         if [ $? = 0 ]; then
                            RMBUP="rm -fR"
                            ExecuteAsAdmin $RMBUP $backup_dir
                            MKDIR="mkdir -p"
                            ExecuteAsAdmin $MKDIR $backup_dir
                            loop_stop=true
                         else
                            loop_stop=false
                         fi
                      else
                         MKDIR="mkdir -p"
                         ExecuteAsAdmin $MKDIR $backup_dir
                         loop_stop=true
                      fi
                   else
                      MYTEMP=`echo $BACKUP_DIR | sed 's/\/$//'`
                      BACKUP_DIR=$MYTEMP
                      backup_dir="$BACKUP_DIR"_"$DATE"
                      MKDIR="mkdir -p"
                      ExecuteAsAdmin $MKDIR $backup_dir
                      loop_stop=true
                   fi
   done
}


CreateTarArchive()
{
   if [ $TAR = "true" ]; then
      if [ $AUTO != "true" ]; then
         $INFOTEXT -n "\nPlease enter a filename for your backupfile. Default: [backup.tar] >>"
         bup_file=`Enter backup.tar`
      else
         bup_file=$BACKUP_FILE
      fi
      cd $backup_dir

      TAR=`which tar`
      if [ $? -eq 0 ]; then
         TAR=$TAR" -cvf"
         if [ "$spooling_method" = "berkeleydb" ]; then
            ExecuteAsAdmin $TAR $bup_file $DATE.dump $BUP_COMMON_FILE_LIST $BUP_SPOOL_FILE_LIST $BUP_COMMON_DIR_LIST
         else
            ExecuteAsAdmin $TAR $bup_file $BUP_COMMON_FILE_LIST $BUP_SPOOL_FILE_LIST $BUP_COMMON_DIR_LIST
         fi          

         ZIP=`which gzip`
         if [ $? -eq 0 ]; then
            ExecuteAsAdmin $ZIP $bup_file 
         else
            ZIP=`which compress`
            if [ $? -ep 0 ]; then
               ExecuteAsAdmin $ZIP $bup_file
            else
               $INFOTEXT -n "Neither gzip, nor compress could be found!\n Can't compress your tar file!"
            fi 
         fi
       else
         $INFOTEXT -n "tar could not be found! No tar archive can be created!\n You will find your backup files" \
                      "in: \n%s\n" $backup_dir 
       fi   

      cd $SGE_ROOT
         $INFOTEXT -n "\n... backup completed"
      $INFOTEXT -n "\nAll information is saved in \n[%s]\n\n" $backup_dir/$bup_file".gz[Z]"

      cd $backup_dir     
      RMF="rm -fR" 
      ExecuteAsAdmin $RMF $DATE.dump.tar $DATE.dump $BUP_COMMON_FILE_LIST $BUP_SPOOL_FILE_LIST $BUP_COMMON_DIR_LIST

      cd $SGE_ROOT

      if [ $AUTO = "true" ]; then
         MoveLog
      fi 

      exit 0
   fi
}


DoBackup()
{
   $INFOTEXT -n "\n... starting with backup\n"    

   CPF="cp -f"
   CPFR="cp -fR"

   if [ "$spooling_method" = "berkeleydb" ]; then

      SwitchArchBup

      for f in $BUP_BDB_COMMON_FILE_LIST_TMP; do
         if [ -f $SGE_ROOT/$SGE_CELL/common/$f ]; then
            BUP_COMMON_FILE_LIST="$BUP_COMMON_FILE_LIST $f"
            ExecuteAsAdmin $CPF $SGE_ROOT/$SGE_CELL/common/$f $backup_dir
         fi
      done

      for f in $BUP_BDB_COMMON_DIR_LIST_TMP; do
         if [ -d $SGE_ROOT/$SGE_CELL/common/$f ]; then
            BUP_COMMON_DIR_LIST="$BUP_COMMON_DIR_LIST $f"
            ExecuteAsAdmin $CPFR $SGE_ROOT/$SGE_CELL/common/$f $backup_dir
         fi
      done

      for f in $BUP_BDB_SPOOL_FILE_LIST_TMP; do
         if [ -f $master_spool/$f ]; then
            BUP_SPOOL_FILE_LIST="$BUP_SPOOL_FILE_LIST $f"
            ExecuteAsAdmin $CPF $master_spool/$f $backup_dir
         fi
      done
   else
      for f in $BUP_CLASSIC_COMMON_FILE_LIST_TMP; do
         if [ -f $SGE_ROOT/$SGE_CELL/common/$f ]; then
            BUP_COMMON_FILE_LIST="$BUP_COMMON_FILE_LIST $f"
            ExecuteAsAdmin $CPF $SGE_ROOT/$SGE_CELL/common/$f $backup_dir
         fi
      done

      master_spool_tmp=`echo $master_spool | cut -d";" -f2`
      for f in $BUP_CLASSIC_SPOOL_FILE_LIST_TMP; do
         if [ -f $master_spool_tmp/$f -o -d $master_spool_tmp/$f ]; then
            BUP_SPOOL_FILE_LIST="$BUP_SPOOL_FILE_LIST $f"
            ExecuteAsAdmin $CPFR $master_spool_tmp/$f $backup_dir
         fi
      done

      for f in $BUP_CLASSIC_DIR_LIST_TMP; do
         if [ -d $SGE_ROOT/$SGE_CELL/common/$f ]; then
            BUP_COMMON_DIR_LIST="$BUP_COMMON_DIR_LIST $f"
            ExecuteAsAdmin $CPFR $SGE_ROOT/$SGE_CELL/common/$f $backup_dir
         fi
      done
   fi
}

ExtractBackup()
{
      loop_stop=false
      while [ $loop_stop = "false" ]; do
         $INFOTEXT -n "\nPlease enter the full path and name of your backup file." \
                      "\nDefault: [%s]" $SGE_ROOT/backup/backup.tar.gz
         bup_file=`Enter $SGE_ROOT/backup/backup.tar.gz`
         
         if [ -f $bup_file ]; then
            loop_stop="true"
         else
            $INFOTEXT -n "\n%s does not exist!\n" $bup_file
            loop_stop="false"
         fi
      done
      mkdir /tmp/bup_tmp_$DATE # don't call here Makedir because $ADMINUSER is not set
      $INFOTEXT -n "\nCopying backupfile to /tmp/bup_tmp_%s\n" $DATE
      cp $bup_file /tmp/bup_tmp_$DATE
      cd /tmp/bup_tmp_$DATE/
     
      echo $bup_file | grep "tar.gz"
      if [ $? -eq 0 ]; then
         ZIP_TYPE="gz"
      else
         echo $bup_file | grep "tar.Z"
         if [ $? -eq 0 ]; then
            ZIP_TYPE="Z"
         fi
      fi

      if [ $ZIP_TYPE = "gz" ]; then
         ZIP="gzip"
      elif [ $ZIP_TYPE = "Z" ]; then
         ZIP="uncompress"
      fi
      
      TAR="tar"
      ZIP=`which $ZIP`
      if [ $? -eq 0 ]; then
         TAR=`which $TAR`
         if [ $? -eq 0 ]; then
            TAR=$TAR" -xvf"
            ExecuteAsAdmin $ZIP -d /tmp/bup_tmp_$DATE/*.$ZIP_TYPE 
            ExecuteAsAdmin $TAR /tmp/bup_tmp_$DATE/*.tar
         else
            $INFOTEXT -n "tar could not be found! Can't extract the backup file\n"
         fi
      else
         $INFOTEXT -n "gzip/uncompress could not be found! Can't extract the backup file\n"
         exit 1
      fi
}


RestoreCheckBootStrapFile()
{
   BACKUP_DIR=$1

   if [ -f $BACKUP_DIR/bootstrap ]; then
      spooling_method=`cat $BACKUP_DIR/bootstrap | grep "spooling_method" | awk '{ print $2 }'`
      db_home=`cat $BACKUP_DIR/bootstrap | grep "spooling_params" | awk '{ print $2 }'`
      master_spool=`cat $BACKUP_DIR/bootstrap | grep "qmaster_spool_dir" | awk '{ print $2 }'`
      ADMINUSER=`cat $BACKUP_DIR/bootstrap | grep "admin_user" | awk '{ print $2 }'`

      MASTER_PORT=`cat $BACKUP_DIR/sgemaster | grep "SGE_QMASTER_PORT=" | head -1 | awk '{ print $1 }' | cut -d"=" -f2 | cut -d";" -f1` 

      ACT_QMASTER=`cat $BACKUP_DIR/act_qmaster`
      
      $SGE_BIN/qping -info $ACT_QMASTER $MASTER_PORT qmaster 1 > /dev/null 2>&1
      ret=$?

      while [ $ret = 0 ]; do 
         $INFOTEXT -n "\nFound a running qmaster on your masterhost: %s\nPlease, check this and " \
                      "make sure, that the daemon is down during the restore!\n\n" $ACT_QMASTER
         $INFOTEXT -n -wait "Shutdown qmaster and hit, <ENTER> to continue, or <CTRL-C> to stop\n" \
                            "the restore procedure!\n"
         $CLEAR
         $SGE_BIN/qping -info $ACT_QMASTER $MASTER_PORT qmaster 1 > /dev/null 2>&1
         ret=$?
      done

      if [ `echo $db_home | cut -d":" -f2` = "$db_home" ]; then
         $INFOTEXT -n "\nSpooling Method: %s detected!\n" $spooling_method
         is_rpc=0
      else
         is_rpc=1
         BDB_SERVER=`echo $db_home | cut -d":" -f1`
         BDB_BASEDIR=`echo $db_home | cut -d":" -f2`

         if [ -f $BACKUP_DIR/sgebdb ]; then
            BDB_HOME=`cat $BACKUP_DIR/sgebdb | grep $BDB_BASEDIR | grep BDBHOMES | cut -d" " -f2 | sed -e s/\"//`
         else
            $INFOTEXT -n "Your Berkeley DB home directory could not be detected!\n"
            $INFOTEXT -n "Please enter your Berkeley DB home directory >>"
            BDB_HOME=`Enter `
         fi

         $INFOTEXT -n "\nThe following settings could be detected.\n"
         $INFOTEXT -n "Spooling Method: Berkeley DB RPC Server spooling.\n"
         $INFOTEXT -n "Berkeley DB Server host: %s\n" $BDB_SERVER   
         $INFOTEXT -n "Berkeley DB home directory: %s\n\n" $BDB_HOME

         $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n "Are all settings right? (y/n) [y] >>"
         if [ $? = 1 ]; then
            $INFOTEXT -n "Please enter your Berkeley DB Server host. >>" 
            BDB_SERVER=`Enter`
            $INFOTEXT -n "Please enter your Berkeley DB home directory. >>" 
            BDB_HOME=`Enter`
         fi
      
         if [ `hostname` != "$BDB_SERVER" ]; then
            $INFOTEXT -n "You're not on the BDB Server host.\nPlease start the backup on the Server host again!\n"
            $INFOTEXT -n "Exiting backup!\n"
            exit 1
         fi
         db_home=$BDB_HOME
         if [ `ps -efa | grep berkeley | grep -v "grep" | wc -l` = 1 ]; then
            $INFOTEXT -n "The restore procedure detected a running Berkeley DB\n" \
                         "service on this machine! Please stop this service first and\n" \
                         "and continue with restore or do a restart of the Berkeley DB after restore!\n" 
            $INFOTEXT -wait -auto $AUTO "Hit, <ENTER> to continue!"
         fi
      fi
   else
      $INFOTEXT -n "bootstrap file could not be found in:\n %s !\n" $BACKUP_DIR
      $INFOTEXT -n "please check your installation! Exiting backup!\n"

      $INFOTEXT -log "bootstrap file could not be found in:\n %s !\n" $BACKUP_DIR
      $INFOTEXT -log "please check your installation! Exiting backup!\n"

      exit 1
   fi
}

CheckServiceAndPorts()
{
   to_check=$1
   check_val=$2

   if [ "$to_check" = "service" ]; then
      case $SGE_ARCH in

       win*)
          cat /etc/services | grep $check_val | grep "^[^#]" > /dev/null 2>&1
          ret=$? 
          if [ "$ret" = 1 ]; then
             ypcat.exe services.byname | grep $check_val | grep "^[^#]" > /dev/null 2>&1
             ret=$?
          fi
       ;;

       lx*)
          cat /etc/services | grep $check_val | grep "^[^#]" > /dev/null 2>&1
          ret=$? 
          if [ "$ret" = 1 ]; then
             ypcat services.byname | grep $check_val | grep "^[^#]" > /dev/null 2>&1
             ret=$?
          fi
       ;;

       *)
          $SGE_UTILBIN/getservbyname $check_val > /dev/null 2>&1
          ret=$?
       ;;

      esac
   elif [ "$to_check" = "port" ]; then
      case $SGE_ARCH in

       win*)
          cat /etc/services | grep -w "$check_val/tcp" > /dev/null 2>&1
          ret=$? 
          if [ "$res" = 1 ]; then
             ypcat.exe services.byname | grep -w "$check_val/tcp" > /dev/null 2>&1
             ret=$?
          fi
       ;;

       lx*)
          cat /etc/services | grep -w "$check_val/tcp" > /dev/null 2>&1
          ret=$? 
          if [ "$res" = 1 ]; then
             ypcat services.byname | grep -w "$check_val/tcp" > /dev/null 2>&1
             ret=$?
          fi
       ;;

       *)
          $SGE_UTILBIN/getservbyname -check $check_val > /dev/null 2>&1
          ret=$?
       ;;

      esac
   fi
}


CopyCA()
{
   if [ "$AUTO" = "true" -a "$CSP_COPY_CERTS" = "false" ]; then
      return
   fi

   if [ "$CSP" = "false" -a \( "$WINDOWS_SUPPORT" = "false" -o "$WIN_DOMAIN_ACCESS" = "false" \) ]; then
      return
   fi

   $INFOTEXT -u "Installing SGE in CSP mode"
   $INFOTEXT "\nInstalling SGE in CSP mode needs to move the cert\n" \
             "files to each execution host. This can be done by script!\n"
   $INFOTEXT "To use this functionality, it is recommended, that user root\n" \
             "may do rsh/ssh to the executions host, without being asked for a password!\n"
   $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n "Should the script try to copy the cert files, for you, to each\n" \
   "execution host? (y/n) [y] >>"

   if [ "$?" = 0 ]; then
      $INFOTEXT "You can use a rsh or a ssh copy to transfer the cert files to each\n" \
                "execution host (default: ssh)"
      $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" -n "Do you want to use rsh/rcp instead of ssh/scp? (y/n) [n] >>"
      if [ "$?" = 0 ]; then
         SHELL_NAME="rsh"
         COPY_COMMAND="rcp"
      fi
      which $COPY_COMMAND > /dev/null
      if [ "$?" != 0 ]; then
         $INFOTEXT "The remote copy command <%s> could not be found!" $COPY_COMMAND
         $INFOTEXT -log "The remote copy command <%s> could not be found!" $COPY_COMMAND
         return
      fi
   else
      return
   fi

   for RHOST in `$SGE_BIN/qconf -sh`; do
      if [ "$RHOST" != "$HOST" ]; then
         CheckRSHConnection $RHOST
         if [ "$?" = 0 ]; then
            $INFOTEXT "Copying certificates to host %s" $RHOST
            $INFOTEXT -log "Copying certificates to host %s" $RHOST
            echo "mkdir /var/sgeCA" | $SHELL_NAME $RHOST /bin/sh &
            if [ "$SGE_QMASTER_PORT" = "" ]; then 
               $COPY_COMMAND -pr $HOST:/var/sgeCA/sge_qmaster $RHOST:/var/sgeCA
            else
               $COPY_COMMAND -pr $HOST:/var/sgeCA/port$SGE_QMASTER_PORT $RHOST:/var/sgeCA
            fi
            if [ "$?" = 0 ]; then
               $INFOTEXT "Setting ownership to adminuser %s" $ADMINUSER
               $INFOTEXT -log "Setting ownership to adminuser %s" $ADMINUSER
               if [ "$SGE_QMASTER_PORT" = "" ]; then
                  echo "chown -R $ADMINUSER /var/sgeCA/sge_qmaster/$SGE_CELL/userkeys/$ADMINUSER" | $SHELL_NAME $RHOST /bin/sh &
               else
                  echo "chown -R $ADMINUSER /var/sgeCA/port$SGE_QMASTER_PORT/$SGE_CELL/userkeys/$ADMINUSER" | $SHELL_NAME $RHOST /bin/sh &
               fi
            else
               $INFOTEXT "The certificate copy failed!"      
               $INFOTEXT -log "The certificate copy failed!"      
            fi
         else
            $INFOTEXT "rsh/ssh connection to host %s is not working!" $RHOST
            $INFOTEXT "Certificates couldn't be copied!"
            $INFOTEXT -log "rsh/ssh connection to host %s is not working!" $RHOST
            $INFOTEXT -log "Certificates couldn't be copied!"
            $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
         fi
      fi
   done
}

#-------------------------------------------------------------------------
# GetAdminUser
#
GetAdminUser()
{
   ADMINUSER=`cat $SGE_ROOT/$SGE_CELL/common/bootstrap | grep "admin_user" | awk '{ print $2 }'`
   euid=`$SGE_UTILBIN/uidgid -euid`

   if [ `echo "$ADMINUSER" |tr "A-Z" "a-z"` = "none" -a $euid = 0 ]; then
      ADMINUSER=default
   fi

   if [ "$SGE_ARCH" = "win32-x86" ]; then
      HOSTNAME=`hostname | tr "a-z" "A-Z"`
      ADMINUSER="$HOSTNAME+$ADMINUSER"
   fi
}


PreInstallCheck()
{
   CheckBinaries
}
