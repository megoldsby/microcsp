
CFLAGS=-g
#CFLAGS=

SOURCES = memory.c timer.c sched.c hardware.c 
OBJS = $(SOURCES:.c=.o)
HDRS = memory.h timer.h sched.h hardware.h timer.h atomic.h internals/timer.h internals/sched.h internals/hardware.h

all:	os

run:	${_T_}
	-./${_T_}
	
$(_T_):	${_T_}.o ${OBJS} 
	gcc -m32 ${CFLAGS} -o ${_T_} ${_T_}.o ${OBJS} -lrt -lm

${_T_}.o:	${_T_}.c
	gcc -m32 ${CFLAGS} -I. -c ${_T_}.c -o ${_T_}.o
		
clean:
	-rm ${OBJS} os.o
	-rm examples/*\.o
#	find examples -type f ! -name "*\.c" -exec rm {} \;

count:
	wc  ${SOURCES} ${HDRS}
	ruby codecount.rb ${SOURCES} ${HDRS}

lib:	microcsp.a

microcsp.a:	${OBJS}
	ar rs microcsp.a ${OBJS}

asm:	${SOURCES}
	gcc -m32 -S -I. $^

os:	os.o ${OBJS}
	gcc -m32 -o os $^ -lrt

%.o:	%.c ${HDRS}
	gcc -m32 -c $(CFLAGS) $< -o $@




