all: build gen_rand_file run clear

run: main
	./main -i randFile -o firstFile.txt -x 5325 -a 53463 -c 64363 -m 34634
	./main -i firstFile.txt -o secondFile.txt -x 5325 -a 53463 -c 64363 -m 34634
	diff randFile secondFile.txt

gen_rand_file:
	head -c 5M < /dev/urandom > randFile

build: main.c
	gcc main.c -o main

clear:
	rm firstFile.txt secondFile.txt main randFile