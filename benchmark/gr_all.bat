rem Set it to a GNUPLOT 4.4 binary
rem In case download it from http://sourceforge.net/projects/gnuplot/files/gnuplot/4.4.2/gp442win32.zip/download
set GNUPLOT=..\contrib\gnuplot\binary\gnuplot.exe

%GNUPLOT% gr_def_random_hit.gnu
%GNUPLOT% gr_def_random_change.gnu
%GNUPLOT% gr_other_judy_problem.gnu
%GNUPLOT% gr_other_slow_problem.gnu

DIR=data\core_i5_650_3G2_linux
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_insert.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_hit.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_insert.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_miss.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_change.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_remove.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_size.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_random_hit.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_random_insert.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_random_miss.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_random_change.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_random_remove.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_random_size.gnu

DIR=data\core_i7_3740_2G7_win
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_insert.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_hit.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_insert.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_miss.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_change.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_remove.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_size.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_random_hit.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_random_insert.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_random_miss.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_random_change.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_random_remove.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_random_size.gnu

DIR=data\test
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_insert.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_hit.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_insert.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_miss.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_change.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_remove.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_forward_size.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_random_hit.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_random_insert.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_random_miss.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_random_change.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_random_remove.gnu
%GNUPLOT% %DIR%\gr_def.gnu gr_random_size.gnu

