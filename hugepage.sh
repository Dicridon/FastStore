#!/bin/bash
echo 512 > /proc/sys/vm/nr_hugepages
grep Hugepage /proc/meminfo 
