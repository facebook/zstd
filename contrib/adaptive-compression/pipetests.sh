make clean multi
pv -q -L 500m tests/test2048.pdf | ./multi -v -otmp.zst
