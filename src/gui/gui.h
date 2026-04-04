#include "include/graphics.h"
#include "src/gui/vendor/stb_truetype.h"


typedef struct arena
{
	usize bytes;
	usize head;
	usize slop;
}arena;
typedef struct arenalist
{
	arena block;
	struct arenalist *next;
} arenalist;

typedef struct guictx
{
	arenalist mem;
	usize memusage;
	u32 alive;
} guictx; 

