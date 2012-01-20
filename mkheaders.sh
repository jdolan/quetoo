#!/bin/bash

for i in $(find . -name "*.c"); do

	header=${i/.c/.h}

	test -f $header || {
		
		define=$(basename $header)
		define=$(echo ${define/./_} | tr a-z A-Z)

		echo "
#ifndef __${define}__
#define __${define}__

#endif /* __${define}__ */
" > $header
	}
done
