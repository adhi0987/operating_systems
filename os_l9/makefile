run :demandpaging.c
	 g++ -Wall -o demandpaging claude_demand.c
	 ./demandpaging search.txt
vrun:demandpaging.c
	g++ -Wall -DVERBOSE -o demandpaging claude_demand.c
	./demandpaging search.txt

db: gensearch.c
	gcc -o gensearch gensearch.c
	./gensearch

clean:
	rm gensearch demandpaging
	
