sh /home/plank/cs494/labs/Lab-1-FAT/Import-Grader.sh /home/plank/cs494/labs/Lab-1-FAT t3.jdisk FATRW | tail -n 1 | awk '{ print $NF }'
