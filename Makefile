SRC_DIR := src
INCLUDE_DIR := include
FNAME := file_synchronisee
CNAME := client
LNAME := lanceur
PROJET_NAME := projet

FSYNC_SRC_FILES := $(SRC_DIR)/$(FNAME).c
CNAME_SRC_FILES := $(SRC_DIR)/$(CNAME).c
LNAME_SRC_FILES := $(SRC_DIR)/$(LNAME).c

FSYNC_INCLUDE_FILES := $(INCLUDE_DIR)/$(FNAME).h

CC := gcc
FLAGS := -std=c18 \
  -Wall -D_POSIX_C_SOURCE=200809L -Wconversion -Werror -Wextra -Wpedantic -Wfatal-errors  -Wwrite-strings -g

LIB_DIR := lib
FLIB_NAME := lib$(FNAME).a

.PHONY: lib clean $(CNAME) $(LNAME) archive all 

all : $(CNAME) $(LNAME)

archive : 
	tar -cvf $(PROJET_NAME).tar $(SRC_DIR) $(INCLUDE_DIR) Makefile

lib: $(FNAME).o 
	ar -rcs $(LIB_DIR)/$(FLIB_NAME) $(LIB_DIR)/$(FNAME).o

$(FNAME).o : $(FSYNC_SRC_FILES) $(FSYNC_INCLUDE_FILES)
	mkdir -p $(LIB_DIR)
	$(CC) $(FLAGS) -c $(FSYNC_SRC_FILES) -I $(INCLUDE_DIR) -o $(LIB_DIR)/$(FNAME).o

$(CNAME): lib 
	$(CC) $(FLAGS) $(CNAME_SRC_FILES) -L$(LIB_DIR) -l$(FNAME) -o $(CNAME)

$(LNAME): lib 
	$(CC) $(FLAGS) $(LNAME_SRC_FILES) -L$(LIB_DIR) -l$(FNAME) -o $(LNAME)

clean:
	rm -rf $(LIB_DIR) $(CNAME) $(LNAME) $(PROJET_NAME).tar
