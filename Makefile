main:
	rm ./bin/main.exe
	g++ ./src/main.cpp -lpthread -I"include" -L"lib" -Wall -lSDL2main -lSDL2 -lSDL2_image -o ./bin/main.exe
