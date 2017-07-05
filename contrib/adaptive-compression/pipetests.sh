make clean multi
pv -q -L 50m tests/test2048.pdf | ./multi -v -otmp.zst
