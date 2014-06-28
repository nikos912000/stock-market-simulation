CC = gcc
FLG = -O4
NAME = StockMarket 

StockMarket: StockMarket.o

	$(CC) $(FLG) StockMarket.o -lpthread -o $(NAME)

StockMarket.o: StockMarket.c StockMarket.h

	$(CC) StockMarket.c -c -lpthread

clean:
	rm -f *.o *.out *.exe
	rm -f *.bin  
