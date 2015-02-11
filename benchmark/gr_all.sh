echo Ensure to use GNUPLOT 4.4

export GDFONTPATH=.
export GNUPLOT_DEFAULT_GDFONT=arial

gnuplot gr_def_random_hit.gnu
gnuplot gr_def_random_change.gnu
gnuplot gr_other_judy_problem.gnu
gnuplot gr_other_googlelibchash_problem.gnu
gnuplot gr_other_ck_problem.gnu

DIR=data/core_i5_650_3G2_linux
gnuplot $DIR/gr_def.gnu gr_forward_insert.gnu
gnuplot $DIR/gr_def.gnu gr_forward_hit.gnu
gnuplot $DIR/gr_def.gnu gr_forward_insert.gnu
gnuplot $DIR/gr_def.gnu gr_forward_miss.gnu
gnuplot $DIR/gr_def.gnu gr_forward_change.gnu
gnuplot $DIR/gr_def.gnu gr_forward_remove.gnu
gnuplot $DIR/gr_def.gnu gr_forward_size.gnu
gnuplot $DIR/gr_def.gnu gr_random_hit.gnu
gnuplot $DIR/gr_def.gnu gr_random_insert.gnu
gnuplot $DIR/gr_def.gnu gr_random_miss.gnu
gnuplot $DIR/gr_def.gnu gr_random_change.gnu
gnuplot $DIR/gr_def.gnu gr_random_remove.gnu
gnuplot $DIR/gr_def.gnu gr_random_size.gnu

DIR=data/core_i7_3740_2G7_linux
gnuplot $DIR/gr_def.gnu gr_forward_insert.gnu
gnuplot $DIR/gr_def.gnu gr_forward_hit.gnu
gnuplot $DIR/gr_def.gnu gr_forward_insert.gnu
gnuplot $DIR/gr_def.gnu gr_forward_miss.gnu
gnuplot $DIR/gr_def.gnu gr_forward_change.gnu
gnuplot $DIR/gr_def.gnu gr_forward_remove.gnu
gnuplot $DIR/gr_def.gnu gr_forward_size.gnu
gnuplot $DIR/gr_def.gnu gr_random_hit.gnu
gnuplot $DIR/gr_def.gnu gr_random_insert.gnu
gnuplot $DIR/gr_def.gnu gr_random_miss.gnu
gnuplot $DIR/gr_def.gnu gr_random_change.gnu
gnuplot $DIR/gr_def.gnu gr_random_remove.gnu
gnuplot $DIR/gr_def.gnu gr_random_size.gnu

DIR=data/core_i7_3740_2G7_win
gnuplot $DIR/gr_def.gnu gr_forward_insert.gnu
gnuplot $DIR/gr_def.gnu gr_forward_hit.gnu
gnuplot $DIR/gr_def.gnu gr_forward_insert.gnu
gnuplot $DIR/gr_def.gnu gr_forward_miss.gnu
gnuplot $DIR/gr_def.gnu gr_forward_change.gnu
gnuplot $DIR/gr_def.gnu gr_forward_remove.gnu
gnuplot $DIR/gr_def.gnu gr_forward_size.gnu
gnuplot $DIR/gr_def.gnu gr_random_hit.gnu
gnuplot $DIR/gr_def.gnu gr_random_insert.gnu
gnuplot $DIR/gr_def.gnu gr_random_miss.gnu
gnuplot $DIR/gr_def.gnu gr_random_change.gnu
gnuplot $DIR/gr_def.gnu gr_random_remove.gnu
gnuplot $DIR/gr_def.gnu gr_random_size.gnu

DIR=data/test
gnuplot $DIR/gr_def.gnu gr_forward_insert.gnu
gnuplot $DIR/gr_def.gnu gr_forward_hit.gnu
gnuplot $DIR/gr_def.gnu gr_forward_insert.gnu
gnuplot $DIR/gr_def.gnu gr_forward_miss.gnu
gnuplot $DIR/gr_def.gnu gr_forward_change.gnu
gnuplot $DIR/gr_def.gnu gr_forward_remove.gnu
gnuplot $DIR/gr_def.gnu gr_forward_size.gnu
gnuplot $DIR/gr_def.gnu gr_random_hit.gnu
gnuplot $DIR/gr_def.gnu gr_random_insert.gnu
gnuplot $DIR/gr_def.gnu gr_random_miss.gnu
gnuplot $DIR/gr_def.gnu gr_random_change.gnu
gnuplot $DIR/gr_def.gnu gr_random_remove.gnu
gnuplot $DIR/gr_def.gnu gr_random_size.gnu

