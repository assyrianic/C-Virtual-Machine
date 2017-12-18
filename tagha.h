#ifndef TAGHA_H_INCLUDED
	#define TAGHA_H_INCLUDED


#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>
#include <iso646.h>
#include "ds.h"

/* For Colored Debugging Printing! */
#define KNRM	"\x1B[0m"	// Normal
#define KRED	"\x1B[31m"
#define KGRN	"\x1B[32m"
#define KYEL	"\x1B[33m"
#define KBLU	"\x1B[34m"
#define KMAG	"\x1B[35m"
#define KCYN	"\x1B[36m"
#define KWHT	"\x1B[37m"
#define RESET	"\033[0m"	// Reset obviously


struct Tagha;
struct NativeInfo;
union CValue;

typedef struct Tagha			Tagha;
typedef struct NativeInfo		NativeInfo;
typedef union CValue			CValue;


union CValue {
	bool Bool, *BoolPtr;
	int8_t Char, *CharPtr;
	int16_t Short, *ShortPtr;
	int32_t Int32, *Int32Ptr;
	int64_t Int64, *Int64Ptr;
	
	uint8_t UChar, *UCharPtr;
	uint16_t UShort, *UShortPtr;
	uint32_t UInt32, *UInt32Ptr;
	uint64_t UInt64, *UInt64Ptr;
	
	float Float, *FloatPtr;
	double Double, *DoublePtr;
	
	void *Ptr, **PtrPtr;
	const char *String, **StringPtr;
	char *Str, **StrPtr;
	union CValue *SelfPtr;
};

//	API for scripts to call C/C++ host functions.
typedef		void (*fnNative_t)(struct Tagha *, union CValue [], union CValue *, const uint32_t);

struct NativeInfo {
	const char	*strName;	// use as string literals
	fnNative_t	pFunc;
};


/* addressing modes
 *	immediate - simple constant value.
 *	register - register holds the exact data.
 *	register indirect - register holds memory address and dereferenced. Can be used as displacement as well.
 *	IPRelative - instruction ptr + offset. required for static data like global vars.
*/
enum AddrMode {
	Immediate	= 1,
	Register	= 2,
	RegIndirect	= 4,
	IPRelative	= 8,
	Byte		= 16,
	TwoBytes	= 32,
	FourBytes	= 64,
	EightBytes	= 128,
};

// Register ID list
enum RegID {
	ras=0,		// general purpose, accumulator - all return results go here.
	rbs,rcs,	// general purpose
	rds,res,	// general purpose
	rfs,rgs,rhs,// general purpose, floating point regs, rfs is used as float accumulator
	ris,rjs,rks,// general purpose
	
	// do not modify after this. Add more registers, if u need, above.
	rsp,rbp,	// stack ptrs, do not touch
	rip,		// instr ptr, do not touch as well.
	regsize		// for lazily updating id list
};


/* Script File Structure.
 * magic verifier
 * Stack Vector
 * Native Table Vector
 * Func Table Dictionary
 * Data Table Dictionary
 */

typedef struct FuncTable {
	uint32_t	m_uiEntry;	// TODO: make this into uint64?
} FuncTable;

typedef struct DataTable {
	uint32_t	m_uiBytes;
	uint32_t	m_uiOffset;	// TODO: make this into uint64?
} DataTable;

// memory format of the script file.
typedef struct TaghaHeader {
	uint16_t m_usMagic;
	uint32_t
		m_uiStackSize,
		m_uiDataSize
	;
	uint32_t m_uiNativeCount;
	uint32_t m_uiNativeTableOffset;
	
	uint32_t m_uiFuncCount;
	uint32_t m_uiFuncTableOffset;
	
	uint32_t m_uiGlobalCount;
	uint32_t m_uiGlobalTableOffset;
	
	char m_cSafeDebugFlags;
	uint32_t m_uiDataOffset;
	uint32_t m_uiTextOffset;
} TaghaHeader;

struct Tagha {
	union CValue m_Regs[regsize];
	uint8_t
		*m_pMemory,			// script memory, entirely aligned by 8 bytes.
		*m_pStackSegment,	// stack segment ptr where the stack's lowest address lies.
		*m_pDataSegment,	// data segment is the address AFTER the stack segment ptr. Aligned by 8 bytes.
		*m_pTextSegment		// text segment is the address after the last global variable AKA the last opcode.
		// rip register will start at m_pMemory + 0.
	;
	// stores a C/C++ function ptr using the script-side name as the key.
	char **m_pstrNativeCalls;		// natives string table.
	struct hashmap *m_pmapNatives;	// native C/C++ interface hashmap.
	
	union CValue *m_pArgv;	// forcing char** to 8 bytes
	struct hashmap
		*m_pmapFuncs,		// stores the functions compiled to script.
		*m_pmapGlobals		// stores global vars like string literals or variables.
	;
	uint32_t
		m_uiMemsize,		// total size of m_pMemory
		m_uiInstrSize,		// size of the text segment
		m_uiMaxInstrs,		// max amount of instrs a script can execute.
		m_uiNatives,		// amount of natives the script uses.
		m_uiFuncs,			// how many functions the script has.
		m_uiGlobals			// how many globals variables the script has.
	;
	int32_t m_iArgc;
	bool
		m_bSafeMode : 1,	// does the script want bounds checking?
		m_bDebugMode : 1,	// print debug info.
		m_bZeroFlag : 1		// conditional zero flag.
	;
};

/*
 * I think you may wanna spend a bit thinking about what scope you want. A VM running 1 "script" (properly called a program, process, or thread) blurs the line between VM and interpreter. Having multiple programs means an OS program has to be built on top of the VM allowing it to run multiple programs concurrently.
 * 
 * There's no reason not to make a good VM, provide one or two compilers/interpreters in its native language. You don't have to write an OS to write code for the VM.
If it's generic enough, somebody can come along later and build an OS on top
* 
* Yeah, if you want it to be embedable, then just write an interpreter for one language with hooks to call it in other languages. If you need to run other languages on top, then go for a VM.
* 
* There you go, so you don't even really need a VM to embed C
*/


// tagha_exec.c
int32_t			Tagha_Exec(struct Tagha *pSys);
const char		*RegIDToStr(const enum RegID id);


// tagha_libc.c
void			Tagha_LoadLibCNatives(struct Tagha *pSys);
void			Tagha_LoadSelfNatives(struct Tagha *pSys);


// tagha_api.c
struct Tagha	*Tagha_New(void);
void			Tagha_Init(struct Tagha *pSys);
void			Tagha_LoadScriptByName(struct Tagha *pSys, char *filename);
bool			Tagha_RegisterNatives(struct Tagha *pSys, struct NativeInfo arrNatives[]);
void			Tagha_Free(struct Tagha *pSys);
int32_t			Tagha_RunScript(struct Tagha *pSys);
int32_t			Tagha_CallFunc(struct Tagha *pSys, const char *strFunc);
void			gfree(void **ppPtr);

void			Tagha_BuildFromFile(struct Tagha *pSys, const char *strFilename);
void			Tagha_BuildFromPtr(struct Tagha *pSys, void *pProgram, const uint64_t Programsize);

void			Tagha_PrintPtrs(const struct Tagha *pSys);
void			Tagha_PrintStack(const struct Tagha *pSys);
void			Tagha_PrintData(const struct Tagha *pSys);
void			Tagha_PrintInstrs(const struct Tagha *pSys);
void			Tagha_PrintRegData(const struct Tagha *pSys);
void			Tagha_Reset(struct Tagha *pSys);

void			*Tagha_GetGlobalByName(struct Tagha *pSys, const char *strGlobalName);
bool			Tagha_BindGlobalPtr(struct Tagha *pSys, const char *strGlobalName, void *pVar);
void			Tagha_PushValue(struct Tagha *pSys, const union CValue value);
union CValue	Tagha_PopValue(struct Tagha *pSys);
void			Tagha_SetCmdArgs(struct Tagha *pSys, char *argv[]);

uint32_t		Tagha_GetMemSize(const struct Tagha *pSys);
uint32_t		Tagha_GetInstrSize(const struct Tagha *pSys);
uint32_t		Tagha_GetMaxInstrs(const struct Tagha *pSys);
uint32_t		Tagha_GetNativeCount(const struct Tagha *pSys);
uint32_t		Tagha_GetFuncCount(const struct Tagha *pSys);
uint32_t		Tagha_GetGlobalsCount(const struct Tagha *pSys);
bool			Tagha_IsSafemodeActive(const struct Tagha *pSys);
bool			Tagha_IsDebugActive(const struct Tagha *pSys);
void			Tagha_PrintErr(struct Tagha *pSys, const char *funcname, const char *err, ...);

/*
*	r = register is first operand
*	m = memory address is first operand
*/
#define INSTR_SET \
	X(halt) \
	/* single operand opcodes */ \
	/* stack ops */ \
	X(push) X(pop) \
	/* unary arithmetic and bitwise ops */ \
	X(neg) X(inc) X(dec) X(bnot) \
	/* jump ops */ \
	X(jmp) X(jz) X(jnz) \
	\
	/* subroutine ops */ \
	X(call) X(ret) X(callnat) \
	\
	/* two operand opcodes */ \
	X(movr) X(movm) X(lea) \
	/* signed and unsigned integer arithmetic ops */ \
	X(addr) X(addm) X(uaddr) X(uaddm) \
	X(subr) X(subm) X(usubr) X(usubm) \
	X(mulr) X(mulm) X(umulr) X(umulm) \
	X(divr) X(divm) X(udivr) X(udivm) \
	X(modr) X(modm) X(umodr) X(umodm) \
	/* bitwise ops */ \
	X(shrr) X(shrm) X(shlr) X(shlm) \
	X(andr) X(andm) X(orr) X(orm) X(xorr) X(xorm) \
	/* comparison ops */ \
	X(ltr) X(ltm) X(ultr) X(ultm) \
	X(gtr) X(gtm) X(ugtr) X(ugtm) \
	X(cmpr) X(cmpm) X(ucmpr) X(ucmpm) \
	X(neqr) X(neqm) X(uneqr) X(uneqm) \
	X(reset) \
	\
	/* floating point opcodes */ \
	X(int2float) X(int2dbl) X(float2dbl) X(dbl2float) \
	X(faddr) X(faddm) X(fsubr) X(fsubm) X(fmulr) X(fmulm) X(fdivr) X(fdivm) \
	X(fneg) X(fltr) X(fltm) X(fgtr) X(fgtm) X(fcmpr) X(fcmpm) X(fneqr) X(fneqm) \
	/* misc opcodes */ \
	X(nop) \

#define X(x)	x,
enum InstrSet{ INSTR_SET };
#undef X

#ifdef __cplusplus
}
#endif

#endif	// TAGHA_H_INCLUDED
