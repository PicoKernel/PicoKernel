# Modules

## Team Members
- @Nikhil-771 (Lead)
- @Amarworks

## What are Modules?
Modules are **OS services written in pure C** that provide system functionallity using **kernel APIs only**. They never access hardware directly and never act like drivers.

Modules are responsible for : 
- Providing system information (e.g. `uptime` , `sysinfo` ,`version`)
- Processing logic for user commands
- Acting as a safe layer between Interface and Kernel