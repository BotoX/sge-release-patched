To perform an BDB update from the SGE Versions 6.0/6.0u1 to 6.0u2
there are only a few steps to do! Attention: This is not for classic spooling
user!

- First of all, please make a full backup of your existing configuration.
  To perform a backup use this command: inst_sge -bup
- Then shutdown your cluster (use: qconf -ke all -ks -km)
- unpack the 6.0u2 binaries into your SGE_ROOT
- execute the 6.0u2 upgrade script. This is a part of the inst_sge script
  to execute enter the following command: inst_sge -updatedb

- To perform the Berkeley DB update, the script has to be executed on
  this host, where the Database is located physically.

  eg:
  - Using Berkeley Local spooling: execute this script on the master host
  - Using Berkeley DB RPC Server spooling, please execute this
    script on the Berkeley DB RPC Server host.

- after a successful update you can restart your cluster again.

