main:
	if [ ! -d ./bin ]; then \
		mkdir ./bin/; \
	fi
	g++ ./src/main.cpp -lpthread -I"include" -L"lib" -Wall -lSDL2main -lSDL2 -o ./bin/main.exe

clean:
	rm ./bin/main.exe