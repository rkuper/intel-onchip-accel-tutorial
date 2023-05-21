CC = gcc
CFLAGS = -I.
LDFLAGS = -laccel-config

COMMON_SOURCES =
SINGLE_SOURCES = sample-single.c
BATCH_SOURCES = sample-batch.c
COMMON_OBJECTS =
SINGLE_OBJ = $(SINGLE_SOURCES:.c=.o)
BATCH_OBJ = $(BATCH_SOURCES:.c=.o)
SINGLE_EX = test-single
BATCH_EX = test-batch

.PHONY: all single batch clean

all: single batch

single: $(SINGLE_EX)

batch: $(BATCH_EX)

$(SINGLE_EX): $(COMMON_OBJ) $(TARGET_OBJ)
		$(CC) $(CFLAGS) $(COMMON_SOURCES) $(SINGLE_SOURCES) $(LDFLAGS) $^ -o $@

$(BATCH_EX): $(COMMON_OBJ) $(TEST_OBJ)
		$(CC) $(CFLAGS) $(COMMON_SOURCES) $(BATCH_SOURCES) $(LDFLAGS) $^ -o $@

.c.o:
		$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(SINGLE_EX) $(BATCH_EX)
