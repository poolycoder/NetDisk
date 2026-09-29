.PHONY: all server client tests clean

all: server client tests

server:
	$(MAKE) -C server

client:
	$(MAKE) -C client

tests:
	$(MAKE) -C tests

clean:
	$(MAKE) -C server clean
	$(MAKE) -C client clean
	$(MAKE) -C tests clean
