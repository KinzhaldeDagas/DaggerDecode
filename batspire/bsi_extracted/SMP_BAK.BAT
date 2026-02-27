@echo off
rem
rem convert CEL to BSI with correct scaling
rem *********** M.Jones 07/30/97 **********
rem Numbers after "-t" are scaling percentages
rem
echo converting %1*.cel to %1*.bsi
cimg -x -c6 -t29 smpa00.cel
cimg -x -c6 -t31 smpa01.cel
cimg -x -c6 -t34 smpa02.cel
cimg -x -c6 -t41 smpa03.cel
cimg -x -c6 -t47 smpa04.cel

cimg -x -c6 -t27 smpd00.cel
cimg -x -c6 -t33 smpd01.cel
cimg -x -c6 -t35 smpd02.cel
cimg -x -c6 -t43 smpd03.cel
cimg -x -c6 -t48 smpd04.cel

cimg -x -c6 -t27 smpw00.cel
cimg -x -c6 -t33 smpw01.cel
cimg -x -c6 -t35 smpw02.cel
cimg -x -c6 -t43 smpw03.cel
cimg -x -c6 -t48 smpw04.cel

cimg -x -c6 -t29 smps00.cel
cimg -x -c6 -t31 smps01.cel
cimg -x -c6 -t34 smps02.cel
cimg -x -c6 -t41 smps03.cel
cimg -x -c6 -t47 smps04.cel

