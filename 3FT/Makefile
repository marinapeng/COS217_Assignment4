#--------------------------------------------------------------------
# Makefile for building the file tree
#--------------------------------------------------------------------

CC = gcc217

all: ft

clean:
	rm -f ft *.o

clobber: clean
	rm -f *~

ft: ft.o nodeFT.o path.o dynarray.o ft_client.o
	$(CC) ft.o nodeFT.o path.o dynarray.o ft_client.o -o ft

ft.o: ft.c ft.h nodeFT.h path.h dynarray.h a4def.h
	$(CC) -c ft.c

nodeFT.o: nodeFT.c nodeFT.h path.h dynarray.h a4def.h
	$(CC) -c nodeFT.c

path.o: path.c path.h dynarray.h a4def.h
	$(CC) -c path.c

dynarray.o: dynarray.c dynarray.h
	$(CC) -c dynarray.c

ft_client.o: ft_client.c ft.h a4def.h
	$(CC) -c ft_client.c