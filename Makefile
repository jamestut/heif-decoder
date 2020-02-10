CPP=g++
LINKER=-pthread
DEPS=-lheif_shared -lheif_writer_shared
DEPS_STATIC=-lheif_static -lheif_writer_static
LIBS=-Lheif/build/lib
INCLUDE=-Iheif/srcs/api/reader -Iheif/srcs/api/common -Iheif/srcs/api/writer
FLAGS=-g

heifread : heifread.o procspawn.o
	$(CPP) $(LIBS) -o heifread heifread.o procspawn.o $(DEPS) $(LINKER)
    
heifread_static : heifread.o procspawn.o
	$(CPP) $(LIBS) -static -o heifread_static heifread.o procspawn.o $(DEPS_STATIC) $(LINKER)

heifread.o : heifread.cpp
	$(CPP) -c $(FLAGS) $(INCLUDE) heifread.cpp

procspawn.o : procspawn.cpp
	$(CPP) -c $(FLAGS) $(INCLUDE) procspawn.cpp

clean :
	rm -f *.o heifread heifread_static
