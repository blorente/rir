SCRIPTPATH=`cd $(dirname "$0") && pwd`
PLAIN_R=~/src/freshr/R-3-2-branch/bin/R
BASE=`cd $SCRIPTPATH/.. && pwd`
TIMEOUT=40

for i in `ls ${BASE}/benchmarks/shootout/*/*.r`; do
    T=`basename $i`;
    echo $T;
    D="`dirname $i`/../..";
    R_COMPILER_OPTIMIZE=3 R_ENABLE_JIT=3 timeout $TIMEOUT /usr/bin/time -f'%E' -- ${SCRIPTPATH}/R -e "{setwd('$D'); source('$i'); execute()}" > /dev/null;
    echo ";;"
    R_ENABLE_JIT=0 timeout $TIMEOUT /usr/bin/time -f'%E' -- $PLAIN_R -e "{setwd('$D'); source('$i'); execute()}" > /dev/null;
    echo ";;"
    R_ENABLE_JIT=2 timeout $TIMEOUT /usr/bin/time -f'%E' -- $PLAIN_R -e "{setwd('$D'); source('$i'); execute()}" > /dev/null;
    echo ";;"
    R_COMPILER_OPTIMIZE=3 R_ENABLE_JIT=3 timeout $TIMEOUT /usr/bin/time -f'%E' -- $PLAIN_R -e "{setwd('$D'); source('$i'); execute()}" > /dev/null;
    echo ";;"
    timeout $TIMEOUT /usr/bin/time -f'%E' -- ${SCRIPTPATH}/R -e "{rir.enableJit(level=1, type='sticky');setwd('$D'); source('$i'); execute()}" > /dev/null;
    echo ";;"
    timeout $TIMEOUT /usr/bin/time -f'%E' -- ${SCRIPTPATH}/R -e "{rir.enableJit(level=2, type='sticky');setwd('$D'); source('$i'); execute()}" > /dev/null;
    echo ";;"
    timeout $TIMEOUT /usr/bin/time -f'%E' -- ${SCRIPTPATH}/R -e "{rir.enableJit(level=2, type='force');setwd('$D'); source('$i'); execute()}" > /dev/null;
    echo ";;"
done 
