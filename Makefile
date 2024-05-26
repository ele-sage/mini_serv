NAME = miniserv

PORT = 777

CC = gcc
RM = rm -rf
FLAGS = -Wall -Wextra -Werror -g -fsanitize=address

SRCS = mini_serv.c

OBJS = ${SRCS:.c=.o}

.c.o:
	${CC} -c $< -o ${<:.c=.o}

${NAME}: ${OBJS}
	${CC} ${FLAGS} ${OBJS} -o ${NAME}

all: ${NAME} 

clean:
	${RM} ${OBJS} ${NAME} 

fclean: clean
	${RM} ${NAME}

run: 
	make && ./${NAME} ${PORT}

re: clean all

.PHONY: all clean fclean re run