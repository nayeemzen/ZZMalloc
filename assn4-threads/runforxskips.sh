#usage is ./runxscripts $num_skips $num_threads
#will produce output for $num_threads for skips 1 to $num_skips 
#in a folder called output, will also use a temp folder
mkdir output; mkdir temp; for i in `seq 1 $1` ; do ./randtrack $2 $i > temp/$i;  sort -n temp/$i > output/$i;  done; rm -rf temp;
