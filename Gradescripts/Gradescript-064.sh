sh /home/plank/cs494/labs/Lab-1-FAT/Import-Grader.sh /home/plank/cs494/labs/Lab-1-FAT t1.jdisk Export-Grader.sh | tail -n 1 | awk '{ print $NF }'
