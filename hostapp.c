
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "tagha.h"

/*
* This is an example of a host application.
* Meant to test initializing, evaluating, shutting down VM, and testing natives.
*/

/* void print_helloworld(void); */
static void native_print_helloworld(struct Tagha *pSys, union CValue params[], union CValue *restrict pRetval, const uint32_t argc)
{
	puts("native_print_helloworld :: hello world from bytecode!\n");
}

/* void test(struct Player *p); */
static void native_test(struct Tagha *pSys, union CValue params[], union CValue *restrict pRetval, const uint32_t argc)
{
	struct Player {
		float		speed;
		uint32_t	health;
		uint32_t	ammo;
	} *player=NULL;
	
	// get first arg which is the virtual address to our data.
	player = (struct Player *)params[0].Ptr;
	if( !player ) {
		puts("native_test_ptr reported an ERROR :: **** param 'p' is NULL ****\n");
		return;
	}
	
	// debug print to see if our data is accurate.
	printf("native_test_ptr :: ammo: %" PRIu32 " | health: %" PRIu32 " | speed: %f\n", player->ammo, player->health, player->speed);
	player=NULL;
}

/* void getglobal(void); */
static void native_getglobal(struct Tagha *pSys, union CValue params[], union CValue *restrict pRetval, const uint32_t argc)
{
	int *p = Tagha_GetGlobalByName(pSys, "i");
	if( !p )
		return;
	printf("native_getglobal :: i == %i\n", *p);
}


int main(int argc, char *argv[])
{
	if( !argv[1] ) {
		printf("[Tagha Usage]: '%s' '.tbc file' \n", argv[0]);
		return 1;
	}
	
	struct Tagha *vm = Tagha_New();
	if( !vm ) {
		puts("Tagha :: Tagha_New returned NULL\n");
		return 1;
	}
	struct NativeInfo tagha_host_natives[] = {
		{"test", native_test},
		{"printHW", native_print_helloworld},
		{"getglobal", native_getglobal},
		{NULL, NULL}
	};
	Tagha_RegisterNatives(vm, tagha_host_natives);
	Tagha_LoadLibCNatives(vm);
	Tagha_LoadSelfNatives(vm);
	
	Tagha_LoadScriptByName(vm, argv[1]);
	
	char *args[] = {
		argv[1],
		"kektus",
		NULL
	};
	Tagha_SetCmdArgs(vm, args);
	printf("[Tagha] :: result: %" PRIi32 "\n", Tagha_RunScript(vm));
	
	/* // tested with test_3d_vecs.tbc
	float vect[3]={ 10.f, 15.f, 20.f };
	Tagha_PushValue(vm, (CValue){ .Ptr=vect });
	Tagha_CallFunc(vm, "VecInvert");
	printf("vect[3]=={ %f , %f, %f }\n", vect[0], vect[1], vect[2]);
	*/
	
	/* // For testing with "factorial.tbc".
	//Tagha_PushValue(vm, (CValue){ .UInt32=6 });	// param b
	Tagha_PushValue(vm, (CValue){ .UInt32=7 });	// param a
	Tagha_CallFunc(vm, "factorial");
	printf("factorial result == %" PRIu32 "\n", Tagha_PopValue(vm).UInt32);
	*/
	
	/*
	int32_t x;
	do {
		printf("0 or less to exit.\n");
		scanf("%i", &x);
	}
	while( x>0 );
	*/
	
	Tagha_Free(vm);
	gfree((void **)&vm);
	return 0;
}





