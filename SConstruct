# Use the Makefile if possible. It doesn't vomit files all over the place

output="a.out"
for line in open("Makefile").readlines():
	if line[:8] == "OUTPUT:=":
		output=line[8:-1]
		break

e=Environment()
e.Append( CCFLAGS=Split("-g -std=c89 -ansi -Wall -fopenmp") )
e.Append( LINKFLAGS=Split("-fopenmp") )
e.Append( LIBS=["m"] )
e.ParseConfig( "pkg-config --cflags --libs glew gl glu" )
e.ParseConfig( "sdl-config --cflags --libs" )
src=Glob("code/*.c" )+Glob("code/font/*.c")
obj=e.Object( source=src )
prog=e.Program( source=obj, target=output )
