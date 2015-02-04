# jmc - a simple c program to issue one or many igmp joins on a given interface.
#     - NOTE: it does not establish a socket so can be run as a non-privileged user
#
# Usage:
#     jmc -n               : Do not fork the control process (run in foreground)
#         -d               : debug info
#         -v               : verbose info
#         -c <config file> : join the groups listed in the config file (one per line)
#         -i <interface>   : interface name 
#         <multicast addr> : the multicast group(s) to join if not specified in a config file
#
