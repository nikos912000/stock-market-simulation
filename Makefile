CC = gcc
FLG = -O4
NAME = StockMarket 

ergasia: StockMarket.o

	$(CC) $(FLG) StockMarket.o -lpthread -o $(NAME)

ergasia.o: StockMarket.c StockMarket.h

	$(CC) StockMarket.c -c -lpthread

clean:
	rm -f *.o *.out *.exe
	rm -f *.bin  
