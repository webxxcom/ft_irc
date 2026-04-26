CXX      := c++
CXXFLAGS := -Iinclude -g -D_GLIBCXX_DEBUG -Wall -Wextra -Werror 
CXXFLAGS += -std=c++98

SRC_DIR  := src
OBJ_DIR  := obj
INC_DIR  := include

# Files 
SRCS     := $(SRC_DIR)/main.cpp		\
			$(SRC_DIR)/Server.cpp	$(SRC_DIR)/Exceptions.cpp $(SRC_DIR)/CommandHandler.cpp $(SRC_DIR)/FileSendHandler.cpp\
			$(SRC_DIR)/Client.cpp	$(SRC_DIR)/Channel.cpp $(SRC_DIR)/ReplyHandler.cpp
OBJS     := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
NAME     := irc

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
