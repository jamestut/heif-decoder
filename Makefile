CPP=g++
LINKER=-pthread
DEPS=-lheif_shared -lheif_writer_shared
LIBS=-Lheif/build/lib
INCLUDE=-Iheif/srcs/api/reader -Iheif/srcs/api/common -Iheif/srcs/api/writer
FLAGS=-g

heifread : heifread.o procspawn.o
	$(CPP) $(LIBS) -o heifread heifread.o procspawn.o $(DEPS) $(LINKER)

heifread.o : heifread.cpp
	$(CPP) -c $(FLAGS) $(INCLUDE) heifread.cpp

procspawn.o : procspawn.cpp
	$(CPP) -c $(FLAGS) $(INCLUDE) procspawn.cpp

clean :
	rm -f *.o heifread
