NAME = th
CC = cc
FLAGS := -Wall -Wextra #-Werror -g
SRCS =	main.c	\
		timer.c	\

OBJS = $(SRCS:%.c=objs/%.o)
OBJS_DIR = objs

all: $(OBJS) $(NAME)


$(OBJS_DIR)/%.o: %.c
	@mkdir -p $(OBJS_DIR)
	$(CC) $(FLAGS) -c $< -o $@

$(NAME): $(OBJS)
	$(CC) $(FLAGS) $(OBJS) -o $(NAME)

clean:
	rm -rf $(OBJS) $(OBJS_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re