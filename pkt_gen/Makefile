#
# simple program to see how stanford packet generator
# can be integrated in a c program 
#

all: pkt_gen pkt_gen_2 pkt_cap

pkt_gen: pkt_gen.c common/nf2util.o
	gcc -g -c pkt_gen.c
	gcc -g -lm common/nf2util.o pkt_gen.o -o pkt_gen

pkt_gen_2: pkt_gen_2.c common/nf2util.o
	gcc -g -c pkt_gen_2.c
	gcc -g -lm -lpcap common/nf2util.o pkt_gen_2.o -o pkt_gen_2

pkt_cap: pkt_cap.c common/nf2util.o
	gcc -g -c pkt_cap.c
	gcc -g -lm -lpcap common/nf2util.o pkt_cap.o -o pkt_cap

common/nf2util.o: common/nf2.h common/nf2util.c common/nf2util.h common/reg_defines.h common/util.c common/util.h
	make -C common/

clean:
	rm -rf *.o pkt_gen pkt_gen_2 pkt_cap *~
