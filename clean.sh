#!/bin/bash

dryrun=false
#dryrun=true

# in case the files don't exist:
shopt -s nullglob

# lets us use ?() for globs
shopt -s extglob

deletepatterns="build/ vm/Logs/ vm/disk-flat.vmdk"
deletefiles=$(find $deletepatterns)

if [ "$deletefiles" = "" ]
then
	exit 1	
fi

deletefiles=$(ls -rd $deletefiles)

echo "About to delete all of these files and directories: $deletefiles"

# convert to array:
deletefiles=($deletefiles)

totalfiles=${#deletefiles[@]}

echo "Total: $totalfiles"

if $dryrun
then
	echo_cmd="echo "
	echo "DRY RUN"
fi

read -p "Continue? [nY]"

if [ "$REPLY" != Y ]
then
	exit 1
fi

(( totaldeleted=0 ))
for f in ${deletefiles[@]}
do
	rmout=$( $echo_cmd rm -vr "$f" )
	deletedchunk=$( echo $rmout | wc -l )
	(( totaldeleted+=deletedchunk ))
	echo $rmout	
done

echo "Total deleted: $totaldeleted"
