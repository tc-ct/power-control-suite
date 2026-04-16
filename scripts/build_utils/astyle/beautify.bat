#!/bin/bash

function usage() {
	echo "Usage:"
	echo "$0 file1 folder1 ... -- beautify specified files or folders"
}

tool_dir="$(cd "$(dirname $0)" && pwd)"

# default option file
option_file=.astylerc

if [ $# -eq 0 ]; then
	usage
	exit 1
fi

for f in "$@"
do
	if [ -f $f ]; then
		$tool_dir/astyle.exe --options=$tool_dir/$option_file $f
	elif [ -d $f ]; then
		# --project option is path sensitive. so do it one by one
		find $f -name "*.h" -exec $tool_dir/astyle.exe --options=$tool_dir/$option_file {} \;
		find $f -name "*.c" -exec $tool_dir/astyle.exe --options=$tool_dir/$option_file {} \;
	else
		echo "!!! not done on $f"
	fi
done
