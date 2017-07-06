make clean multi
pv -q -L 100m tests/test2048.pdf | ./multi -v -otmp.zst
