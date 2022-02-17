setup:
	gcc -std=gnu99 -g -Wall -o smallsh smallsh.c

clean:
	rm smallsh
	
run:
	./smallsh

valgrind:
	gcc -o smallsh -g smallsh.c
	valgrind --leak-check=yes --show-reachable=yes --track-origins=yes -s ./smallsh

test1:
	./p3testscript 2>&1

test2:
	./p3testscript 2>&1 | more

test3:
	./p3testscript > mytestresults 2>&1 