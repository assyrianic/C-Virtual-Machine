#include <stdio.h>
#include <stdlib.h>
#include "tagha_assembler.h"


static inline bool IsSpace(const char c)
{
	return( c == ' ' or c == '\t' or c == '\r' or c == '\v' or c == '\f' );
}

static inline bool IsDecimal(const char c)
{
	return( c >= '0' and c <= '9' );
}

static inline bool IsHex(const char c)
{
	return( (c >= 'a' and c <= 'f') or (c >= 'A' and c <= 'F') or IsDecimal(c) );
}

static inline bool IsBinary(const char c)
{
	return( c=='0' or c=='1' );
}

static inline bool IsOctal(const char c)
{
	return( c >= '0' and c <= '7' );
}

static inline bool IsPossibleIdentifier(const char c)
{
	return( (c >= 'a' and c <= 'z')
		or (c >= 'A' and c <= 'Z')
		or c == '_'
		or (c >= '0' and c <= '9')
		or c < -1 );
}

static inline bool IsAlphabetic(const char c)
{
	return( (c >= 'a' and c <= 'z')
		or (c >= 'A' and c <= 'Z')
		or c == '_'
		or c < -1 );
}

static bool SkipWhiteSpace(char **restrict strRef)
{
	if( !*strRef or !**strRef )
		return false;
	
	while( **strRef and IsSpace(**strRef) )
		(*strRef)++;
	return **strRef != 0;
}

static bool LexIdentifier(char **restrict strRef, struct String *const restrict strobj)
{
	if( !*strRef or !**strRef or !strobj )
		return false;
	
	String_Del(strobj);
	String_AddChar(strobj, *(*strRef)++);
	while( **strRef and IsPossibleIdentifier(**strRef) )
		String_AddChar(strobj, *(*strRef)++);
	
	return strobj->Len > 0;
}

static bool LexNumber(char **restrict strRef, struct String *const restrict strobj)
{
	if( !*strRef or !**strRef or !strobj )
		return false;
	
	String_Del(strobj);
	if( **strRef=='0' ) {
		if( (*strRef)[1]=='x' or (*strRef)[1]=='X' ) { // hexadecimal.
			String_AddStr(strobj, (*strRef)[1]=='x' ? "0x" : "0X");
			(*strRef) += 2;
			while( **strRef and IsHex(**strRef) )
				String_AddChar(strobj, *(*strRef)++);
		}
		else if( (*strRef)[1]=='b' or (*strRef)[1]=='B' ) { // binary.
			String_AddStr(strobj, (*strRef)[1]=='b' ? "0b" : "0B");
			(*strRef) += 2;
			while( **strRef and **strRef=='1' or **strRef=='0' )
				String_AddChar(strobj, *(*strRef)++);
		}
		else { // octal.
			String_AddChar(strobj, *(*strRef)++);
			while( **strRef and IsOctal(**strRef) )
				String_AddChar(strobj, *(*strRef)++);
		}
	}
	else {
		while( **strRef and IsDecimal(**strRef) )
			String_AddChar(strobj, *(*strRef)++);
	}
	return strobj->Len > 0;
}

// $stacksize <number>
bool TaghaAsm_ParseStackDirective(struct TaghaAsmbler *const restrict tasm)
{
	if( !tasm or !tasm->Src or !tasm->Iter )
		return false;
	
	SkipWhiteSpace(&tasm->Iter);
	if( !IsDecimal(*tasm->Iter) ) {
		printf("tasm error: stack size directive requires a valid number! line: %zu\n", tasm->CurrLine);
		exit(-1);
	}
	LexNumber(&tasm->Iter, tasm->Lexeme);
	const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
	tasm->Stacksize = strtoul(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
	return true;
}

// $global varname bytes ...
bool TaghaAsm_ParseGlobalVarDirective(struct TaghaAsmbler *const restrict tasm)
{
	if( !tasm or !tasm->Src or !tasm->Iter )
		return false;
	
	SkipWhiteSpace(&tasm->Iter);
	if( !IsAlphabetic(*tasm->Iter) ) {
		printf("tasm error: global directive requires the 1st argument to be a variable name! line: %zu\n", tasm->CurrLine);
		exit(-1);
	}
	
	struct String *varname = &(struct String){0};
	LexIdentifier(&tasm->Iter, varname);
	SkipWhiteSpace(&tasm->Iter);
	if( *tasm->Iter==',' )
		tasm->Iter++;
	SkipWhiteSpace(&tasm->Iter);
	
	if( !IsDecimal(*tasm->Iter) ) {
		printf("tasm error: missing byte size number in global directive on line: %zu\n", tasm->CurrLine);
		exit(-1);
	}
	LexNumber(&tasm->Iter, tasm->Lexeme);
	const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
	size_t bytes = strtoull(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
	struct ByteBuffer *vardata = ByteBuffer_New();
	if( !vardata ) {
		puts("tasm error: out of memory!");
		exit(-1);
	}
	SkipWhiteSpace(&tasm->Iter);
	if( *tasm->Iter==',' )
		tasm->Iter++;
	
	while( bytes ) {
		SkipWhiteSpace(&tasm->Iter);
		if( *tasm->Iter=='"' ) { // string
			String_Del(tasm->Lexeme);
			const char quote = *tasm->Iter++;
			while( *tasm->Iter and *tasm->Iter != quote ) {
				char charval = *tasm->Iter++;
				if( !charval ) { // sudden EOF?
					puts("tasm error: sudden EOF in global directive string copying!");
					exit(-1);
				}
				String_AddChar(tasm->Lexeme, charval);
				// handle escape chars
				if( charval=='\\' )
					String_AddChar(tasm->Lexeme, *tasm->Iter++);
			}
			ByteBuffer_InsertString(vardata, tasm->Lexeme->CStr, tasm->Lexeme->Len);
			bytes = 0;
		}
		else if( *tasm->Iter==',' )
			tasm->Iter++;
		else if( IsDecimal(*tasm->Iter) ) {
			LexNumber(&tasm->Iter, tasm->Lexeme);
			const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
			const uint64_t data = strtoull(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
			if( data ) {
				printf("tasm error: single numeric arguments for global vars must be 0! on line: %zu\n", tasm->CurrLine);
				exit(-1);
			}
			while( bytes-- )
				ByteBuffer_InsertByte(vardata, 0);
			break;
		}
		else if( IsAlphabetic(*tasm->Iter) ) {
			LexIdentifier(&tasm->Iter, tasm->Lexeme);
			if( !String_NCmpCStr(tasm->Lexeme, "byte", sizeof "byte") ) {
				SkipWhiteSpace(&tasm->Iter);
				LexNumber(&tasm->Iter, tasm->Lexeme);
				const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
				const uint8_t data = strtoull(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
				ByteBuffer_InsertByte(vardata, data);
				bytes--;
			}
			else if( !String_NCmpCStr(tasm->Lexeme, "half", sizeof "half") ) {
				SkipWhiteSpace(&tasm->Iter);
				LexNumber(&tasm->Iter, tasm->Lexeme);
				const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
				const uint16_t data = strtoull(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
				ByteBuffer_InsertInt(vardata, data, sizeof(uint16_t));
				bytes -= sizeof(uint16_t);
			}
			else if( !String_NCmpCStr(tasm->Lexeme, "long", sizeof "long") ) {
				SkipWhiteSpace(&tasm->Iter);
				LexNumber(&tasm->Iter, tasm->Lexeme);
				const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
				const uint32_t data = strtoull(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
				ByteBuffer_InsertInt(vardata, data, sizeof(uint32_t));
				bytes -= sizeof(uint32_t);
			}
			else if( !String_NCmpCStr(tasm->Lexeme, "word", sizeof "word") ) {
				SkipWhiteSpace(&tasm->Iter);
				LexNumber(&tasm->Iter, tasm->Lexeme);
				const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
				const uint64_t data = strtoull(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
				ByteBuffer_InsertInt(vardata, data, sizeof(uint64_t));
				bytes -= sizeof(uint64_t);
			}
		}
		else {
			printf("tasm error: global var directive data set is incomplete, must be equal to bytesize given. line: %zu\n", tasm->CurrLine);
			exit(-1);
		}
	}
#ifdef TASM_DEBUG
	printf("tasm debug: vardata.Count: %zu\n", vardata->Count);
#endif
	/*
	printf("tasm: adding global var '%s'\n", varname->CStr);
	for( size_t i=0 ; i<ByteBuffer_Count(vardata) ; i++ )
		printf("tasm: global var[%zu] == %u\n", i, vardata->Buffer[i]);
	*/
	LinkMap_Insert(tasm->VarTable, varname->CStr, (union Value){.BufferPtr=vardata});
	String_Del(varname);
	return true;
}

// $native %name
bool TaghaAsm_ParseNativeDirective(struct TaghaAsmbler *const restrict tasm)
{
	if( !tasm or !tasm->Src or !tasm->Iter )
		return false;
	
	SkipWhiteSpace(&tasm->Iter);
	if( *tasm->Iter != '%' ) {
		printf("tasm error: missing %% for native name declaration! line: %zu\n", tasm->CurrLine);
		exit(-1);
	}
	LexIdentifier(&tasm->Iter, tasm->Lexeme);
	
	struct LabelInfo *label = calloc(1, sizeof *label);
	if( !label ) {
		puts("tasm error: OUT OF MEMORY! Exiting...");
		exit(-1);
	}
	label->Addr = 0;
	label->IsFunc = label->IsNativeFunc = true;
	union Value native = (union Value){.Ptr = label};
	LinkMap_Insert(tasm->FuncTable, tasm->Lexeme->CStr, native);
#ifdef TASM_DEBUG
	printf("tasm: added native function '%s'\n", tasm->Lexeme->CStr);
#endif
	return true;
}

// possible formats:
// opcode register, imm
// opcode register, register
// opcode register, [size register + offset]
// opcode [size register + offset], imm
// opcode [size register + offset], register
bool TaghaAsm_ParseTwoOpInstr(struct TaghaAsmbler *const restrict tasm, const bool firstpass)
{
	if( !tasm or !tasm->Src or !tasm->Iter )
		return false;
	
	tasm->ProgramCounter += 3;
	SkipWhiteSpace(&tasm->Iter);
	bool IsMemoryDeref = false;
	uint8_t
		addrmode1=0,
		//addrmode2=0,
		reg1=0
	;
	union Value imm = (union Value){0};
	int32_t offset = 0;
	
	// check the first operand.
	if( *tasm->Iter=='r' ) {
		// register operand and register addressing mode.
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		if( !LinkMap_HasKey(tasm->Registers, tasm->Lexeme->CStr) ) {
			printf("tasm error: invalid register name '%s' on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
		addrmode1 |= Reserved;
		reg1 = LinkMap_Get(tasm->Registers, tasm->Lexeme->CStr).UInt64;
	}
	else if( *tasm->Iter=='[' ) {
		tasm->Iter++;
		IsMemoryDeref = true;
		// register indirection operand + register indirect addressing mode + 4 byte offset.
		SkipWhiteSpace(&tasm->Iter);
		
		// get memory dereferencing size.
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		if( !String_NCmpCStr(tasm->Lexeme, "byte", sizeof "byte")
				or !String_NCmpCStr(tasm->Lexeme, "half", sizeof "half")
				or !String_NCmpCStr(tasm->Lexeme, "long", sizeof "long")
				or !String_NCmpCStr(tasm->Lexeme, "word", sizeof "word") )
		{
			//addrmode1 = RegIndirect;
			switch( *tasm->Lexeme->CStr ) {
				case 'b': addrmode1 |= Byte; break;
				case 'h': addrmode1 |= TwoBytes; break;
				case 'l': addrmode1 |= FourBytes; break;
				case 'w': addrmode1 |= EightBytes; break;
			}
			SkipWhiteSpace(&tasm->Iter);
			tasm->ProgramCounter += 4;
			
			// get register name.
			LexIdentifier(&tasm->Iter, tasm->Lexeme);
			if( !LinkMap_HasKey(tasm->Registers, tasm->Lexeme->CStr) ) {
				printf("tasm error: invalid register name '%s' in register indirection on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
				exit(-1);
			}
			reg1 = LinkMap_Get(tasm->Registers, tasm->Lexeme->CStr).UInt64;
			
			SkipWhiteSpace(&tasm->Iter);
			// if there's no plus/minus equation, assume `[reg+0]`.
			const char closer = *tasm->Iter;
			if( closer != '-' and closer != '+' and closer != ']' ) {
				printf("tasm error: invalid offset math operator '%c' in register indirection on line: %zu\n", closer, tasm->CurrLine);
				exit(-1);
			}
			else if( closer=='-' or closer=='+' ) {
				tasm->Iter++;
				SkipWhiteSpace(&tasm->Iter);
				if( !IsDecimal(*tasm->Iter) ) {
					printf("tasm error: invalid offset '%s' in register indirection on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
					exit(-1);
				}
				LexNumber(&tasm->Iter, tasm->Lexeme);
				SkipWhiteSpace(&tasm->Iter);
				const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
				offset = strtol(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
				offset = closer=='-' ? -offset : offset;
				//printf("offset == %i\n", offset);
			}
			if( *tasm->Iter != ']' ) {
				printf("tasm error: missing closing ']' bracket in register indirection on line: %zu\n", tasm->CurrLine);
				exit(-1);
			}
			tasm->Iter++;
		}
		else {
			printf("tasm error: missing or invalid indirection size '%s' on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
	}
	else {
		printf("tasm error: invalid 1st operand on line: %zu\n", tasm->CurrLine);
		exit(-1);
	}
	
	
	SkipWhiteSpace(&tasm->Iter);
	// ok, let's read the secondary operand!
	// the second operand can be an imm, register, or register indirection.
	if( *tasm->Iter==',' )
		tasm->Iter++;
	
	SkipWhiteSpace(&tasm->Iter);
	// immediate/constant operand value.
	if( IsDecimal(*tasm->Iter) ) {
		LexNumber(&tasm->Iter, tasm->Lexeme);
		tasm->ProgramCounter += 8;
		addrmode1 |= Immediate;
		const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
		imm.UInt64 = strtoull(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
	}
	else if( *tasm->Iter=='r' ) {
		// register operand and register addressing mode.
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		if( !LinkMap_HasKey(tasm->Registers, tasm->Lexeme->CStr) ) {
			printf("tasm error: invalid register name '%s' in line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
		tasm->ProgramCounter++;
		addrmode1 |= Register;
		imm.UInt64 = LinkMap_Get(tasm->Registers, tasm->Lexeme->CStr).UInt64;
	}
	else if( *tasm->Iter=='[' ) {
		tasm->Iter++;
		if( IsMemoryDeref ) {
			printf("tasm error: memory to memory operations are not allowed! line: %zu\n", tasm->CurrLine);
			exit(-1);
		}
		IsMemoryDeref = true;
		// register indirection operand + register indirect addressing mode + 4 byte offset.
		SkipWhiteSpace(&tasm->Iter);
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		if( !String_NCmpCStr(tasm->Lexeme, "byte", sizeof "byte")
				or !String_NCmpCStr(tasm->Lexeme, "half", sizeof "half")
				or !String_NCmpCStr(tasm->Lexeme, "long", sizeof "long")
				or !String_NCmpCStr(tasm->Lexeme, "word", sizeof "word") )
		{
			addrmode1 |= RegIndirect;
			switch( *tasm->Lexeme->CStr ) {
				case 'b': addrmode1 |= Byte; break;
				case 'h': addrmode1 |= TwoBytes; break;
				case 'l': addrmode1 |= FourBytes; break;
				case 'w': addrmode1 |= EightBytes; break;
			}
			tasm->ProgramCounter += 5;
			SkipWhiteSpace(&tasm->Iter);
			LexIdentifier(&tasm->Iter, tasm->Lexeme);
			if( !LinkMap_HasKey(tasm->Registers, tasm->Lexeme->CStr) ) {
				printf("tasm error: invalid register name '%s' in register indirection on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
				exit(-1);
			}
			imm.UInt64 = LinkMap_Get(tasm->Registers, tasm->Lexeme->CStr).UInt64;
			
			SkipWhiteSpace(&tasm->Iter);
			// if there's no plus/minus equation, assume `[reg+0]`
			const char closer = *tasm->Iter;
			if( closer != '-' and closer != '+' and closer != ']' ) {
				printf("tasm error: invalid offset math operator '%c' in register indirection on line: %zu\n", closer, tasm->CurrLine);
				exit(-1);
			}
			else if( closer=='-' or closer=='+' ) {
				tasm->Iter++;
				SkipWhiteSpace(&tasm->Iter);
				if( !IsDecimal(*tasm->Iter) ) {
					printf("tasm error: invalid offset '%s' in register indirection on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
					exit(-1);
				}
				LexNumber(&tasm->Iter, tasm->Lexeme);
				SkipWhiteSpace(&tasm->Iter);
				const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
				offset = strtol(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
				offset = closer=='-' ? -offset : offset;
				//printf("offset == %i\n", offset);
			}
			if( *tasm->Iter != ']' ) {
				printf("tasm error: missing closing ']' bracket in register indirection on line: %zu\n", tasm->CurrLine);
				exit(-1);
			}
			tasm->Iter++;
		}
		else {
			printf("tasm error: missing or invalid indirection size '%s' on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
	}
	if( !firstpass ) {
		struct LabelInfo *label = LinkMap_Get(tasm->FuncTable, tasm->ActiveFuncLabel->CStr).Ptr;
		if( !label ) {
			printf("tasm error: undefined label '%s' on line: %zu\n", tasm->ActiveFuncLabel->CStr, tasm->CurrLine);
			exit(-1);
		}
		ByteBuffer_InsertByte(&label->Bytecode, addrmode1);
		//ByteBuffer_InsertByte(&label->Bytecode, addrmode2);
		ByteBuffer_InsertByte(&label->Bytecode, reg1);
		//if( addrmode2 & Immediate )
		if( addrmode1 & Immediate )
			ByteBuffer_InsertInt(&label->Bytecode, imm.UInt64, sizeof imm.UInt64);
		else ByteBuffer_InsertByte(&label->Bytecode, imm.UChar);
		if( IsMemoryDeref )
			ByteBuffer_InsertInt(&label->Bytecode, offset, sizeof(int32_t));
	}
	return true;
}

// possible formats:
// opcode imm
// opcode register
// opcode [size register + offset]
bool TaghaAsm_ParseOneOpInstr(struct TaghaAsmbler *const restrict tasm, const bool firstpass)
{
	if( !tasm or !tasm->Src or !tasm->Iter )
		return false;
	
	tasm->ProgramCounter += 2;
	SkipWhiteSpace(&tasm->Iter);
	bool IsMemoryDeref = false;
	uint8_t addrmode1=0;
	union Value imm = (union Value){0};
	int32_t offset = 0;
	
	// immediate/constant operand value.
	if( IsDecimal(*tasm->Iter) ) {
		LexNumber(&tasm->Iter, tasm->Lexeme);
		tasm->ProgramCounter += 8;
		addrmode1 = Immediate;
		const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
		imm.UInt64 = strtoull(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
	}
	// label value.
	else if( *tasm->Iter=='.' or *tasm->Iter=='%' ) {
		const bool isfunclbl = *tasm->Iter=='%';
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		tasm->ProgramCounter += 8;
		if( !firstpass and !LinkMap_HasKey(isfunclbl ? tasm->FuncTable : tasm->LabelTable, tasm->Lexeme->CStr) ) {
			printf("tasm error: undefined label '%s' on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
		addrmode1 = Immediate;
		if( !firstpass ) {
			struct LabelInfo *label = LinkMap_Get(isfunclbl ? tasm->FuncTable : tasm->LabelTable, tasm->Lexeme->CStr).Ptr;
			if( !isfunclbl ) {
				imm.Int64 = label->Addr - tasm->ProgramCounter;
			#ifdef TASM_DEBUG
				printf("label->Addr (%llu) - tasm->ProgramCounter (%zu) == '%lli'\n", label->Addr, tasm->ProgramCounter, label->Addr - tasm->ProgramCounter);
			#endif
			}
			else {
				imm.Int64 = label->IsNativeFunc ? -(LinkMap_GetIndexByName(tasm->FuncTable, tasm->Lexeme->CStr) + 1) : (LinkMap_GetIndexByName(tasm->FuncTable, tasm->Lexeme->CStr) + 1);
			}
		}
	}
	else if( *tasm->Iter=='r' ) {
		// register operand and register addressing mode.
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		if( !LinkMap_HasKey(tasm->Registers, tasm->Lexeme->CStr) ) {
			printf("tasm error: invalid register name '%s' in line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
		tasm->ProgramCounter++;
		addrmode1 = Register;
		imm.UInt64 = LinkMap_Get(tasm->Registers, tasm->Lexeme->CStr).UInt64;
	}
	else if( *tasm->Iter=='[' ) {
		tasm->Iter++;
		IsMemoryDeref = true;
		
		// register indirection operand + register indirect addressing mode + 4 byte offset.
		SkipWhiteSpace(&tasm->Iter);
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		if( !String_NCmpCStr(tasm->Lexeme, "byte", sizeof "byte")
				or !String_NCmpCStr(tasm->Lexeme, "half", sizeof "half")
				or !String_NCmpCStr(tasm->Lexeme, "long", sizeof "long")
				or !String_NCmpCStr(tasm->Lexeme, "word", sizeof "word") )
		{
			addrmode1 = RegIndirect;
			switch( *tasm->Lexeme->CStr ) {
				case 'b': addrmode1 |= Byte; break;
				case 'h': addrmode1 |= TwoBytes; break;
				case 'l': addrmode1 |= FourBytes; break;
				case 'w': addrmode1 |= EightBytes; break;
			}
			tasm->ProgramCounter += 5;
			SkipWhiteSpace(&tasm->Iter);
			LexIdentifier(&tasm->Iter, tasm->Lexeme);
			if( !LinkMap_HasKey(tasm->Registers, tasm->Lexeme->CStr) ) {
				printf("tasm error: invalid register name '%s' in register indirection on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
				exit(-1);
			}
			imm.UInt64 = LinkMap_Get(tasm->Registers, tasm->Lexeme->CStr).UInt64;
			
			SkipWhiteSpace(&tasm->Iter);
			// if there's no plus/minus equation, assume `[reg+0]`
			const char closer = *tasm->Iter;
			if( closer != '-' and closer != '+' and closer != ']' ) {
				printf("tasm error: invalid offset math operator '%c' in register indirection on line: %zu\n", closer, tasm->CurrLine);
				exit(-1);
			}
			else if( closer=='-' or closer=='+' ) {
				tasm->Iter++;
				SkipWhiteSpace(&tasm->Iter);
				if( !IsDecimal(*tasm->Iter) ) {
					printf("tasm error: invalid offset '%s' in register indirection on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
					exit(-1);
				}
				LexNumber(&tasm->Iter, tasm->Lexeme);
				SkipWhiteSpace(&tasm->Iter);
				const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
				offset = strtol(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
				offset = closer=='-' ? -offset : offset;
				//printf("offset == %i\n", offset);
			}
			if( *tasm->Iter != ']' ) {
				printf("tasm error: missing closing ']' bracket in register indirection on line: %zu\n", tasm->CurrLine);
				exit(-1);
			}
			tasm->Iter++;
		}
		else {
			printf("tasm error: missing or invalid indirection size '%s' on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
	}
	if( !firstpass ) {
		struct LabelInfo *label = LinkMap_Get(tasm->FuncTable, tasm->ActiveFuncLabel->CStr).Ptr;
		if( !label ) {
			printf("tasm error: undefined label '%s' on line: %zu\n", tasm->ActiveFuncLabel->CStr, tasm->CurrLine);
			exit(-1);
		}
		ByteBuffer_InsertByte(&label->Bytecode, addrmode1);
		if( addrmode1 & Immediate )
			ByteBuffer_InsertInt(&label->Bytecode, imm.UInt64, sizeof imm.UInt64);
		else ByteBuffer_InsertByte(&label->Bytecode, imm.UChar);
		if( IsMemoryDeref )
			ByteBuffer_InsertInt(&label->Bytecode, offset, sizeof offset);
	}
	return true;
}

bool TaghaAsm_ParseNoOpsInstr(struct TaghaAsmbler *const restrict tasm, const bool firstpass)
{
	if( !tasm or !tasm->Src or !tasm->Iter )
		return false;
	
	tasm->ProgramCounter += 2;
	SkipWhiteSpace(&tasm->Iter);
	if( !firstpass ) {
		struct LabelInfo *label = LinkMap_Get(tasm->FuncTable, tasm->ActiveFuncLabel->CStr).Ptr;
		if( !label ) {
			printf("tasm error: undefined label '%s' on line: %zu\n", tasm->ActiveFuncLabel->CStr, tasm->CurrLine);
			exit(-1);
		}
		ByteBuffer_InsertByte(&label->Bytecode, 0);
	}
	return true;
}

// possible formats:
// lea register, global_var_name
// lea register, func_label
// lea register, [size register + offset]
bool TaghaAsm_ParseLEAInstr(struct TaghaAsmbler *const restrict tasm, const bool firstpass)
{
	if( !tasm or !tasm->Src or !tasm->Iter )
		return false;
	
	tasm->ProgramCounter += 3; // opcode + addrmode + register id
	SkipWhiteSpace(&tasm->Iter);
	bool IsMemoryDeref = false;
	uint8_t
		addrmode1=0, reg=0
	;
	union Value imm = (union Value){0};
	int32_t offset = 0;
	
	if( !IsAlphabetic(*tasm->Iter) ) {
		printf("tasm error: LEA opcode takes only a register as the first argument on line: %zu\n", tasm->CurrLine);
		exit(-1);
	}
	
	LexIdentifier(&tasm->Iter, tasm->Lexeme);
	if( !LinkMap_HasKey(tasm->Registers, tasm->Lexeme->CStr) ) {
		printf("tasm error: invalid register name '%s' on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
		exit(-1);
	}
	reg = LinkMap_Get(tasm->Registers, tasm->Lexeme->CStr).UInt64;
	SkipWhiteSpace(&tasm->Iter);
	if( *tasm->Iter==',' )
		tasm->Iter++;
	SkipWhiteSpace(&tasm->Iter);
	
	// global var name.
	if( IsAlphabetic(*tasm->Iter) ) {
		addrmode1 = Immediate;
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		if( !LinkMap_HasKey(tasm->VarTable, tasm->Lexeme->CStr) ) {
			printf("tasm error: undefined global var '%s' in LEA opcode on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
		tasm->ProgramCounter += 8;
		imm.UInt64 = LinkMap_GetIndexByName(tasm->VarTable, tasm->Lexeme->CStr);
	#ifdef TASM_DEBUG
		printf("tasm: lea global's '%s' index is '%zu'\n", tasm->Lexeme->CStr, LinkMap_GetIndexByName(tasm->VarTable, tasm->Lexeme->CStr));
	#endif
	}
	// function label
	else if( *tasm->Iter=='%' ) {
		addrmode1 = Register;
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		tasm->ProgramCounter += 8;
		if( !firstpass ) {
			if( !LinkMap_HasKey(tasm->FuncTable, tasm->Lexeme->CStr) ) {
				printf("tasm error: undefined func label '%s' in LEA opcode on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
				exit(-1);
			}
			struct LabelInfo *lbl_func = LinkMap_Get(tasm->FuncTable, tasm->Lexeme->CStr).Ptr;
			imm.Int64 = lbl_func->IsNativeFunc
				? -(LinkMap_GetIndexByName(tasm->FuncTable, tasm->Lexeme->CStr) + 1)
					: LinkMap_GetIndexByName(tasm->FuncTable, tasm->Lexeme->CStr) + 1;
		#ifdef TASM_DEBUG
			printf("tasm: lea func label's '%s' index is '%zu'\n", tasm->Lexeme->CStr, LinkMap_GetIndexByName(tasm->FuncTable, tasm->Lexeme->CStr));
		#endif
		}
	}
	// register indirection
	else if( *tasm->Iter=='[' ) {
		tasm->Iter++;
		IsMemoryDeref = true;
		// register indirection operand + register indirect addressing mode + 4 byte offset.
		SkipWhiteSpace(&tasm->Iter);
		
		// get memory dereferencing size IF available
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		if( !String_NCmpCStr(tasm->Lexeme, "byte", sizeof "byte")
				or !String_NCmpCStr(tasm->Lexeme, "half", sizeof "half")
				or !String_NCmpCStr(tasm->Lexeme, "long", sizeof "long")
				or !String_NCmpCStr(tasm->Lexeme, "word", sizeof "word") )
		{
			switch( *tasm->Lexeme->CStr ) {
				case 'b': addrmode1 |= Byte; break;
				case 'h': addrmode1 |= TwoBytes; break;
				case 'l': addrmode1 |= FourBytes; break;
				case 'w': addrmode1 |= EightBytes; break;
			}
			SkipWhiteSpace(&tasm->Iter);
			LexIdentifier(&tasm->Iter, tasm->Lexeme);
		}
		addrmode1 = RegIndirect;
		tasm->ProgramCounter += 5;
		if( !LinkMap_HasKey(tasm->Registers, tasm->Lexeme->CStr) ) {
			printf("tasm error: invalid register name '%s' in register indirection on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
		imm.UInt64 = LinkMap_Get(tasm->Registers, tasm->Lexeme->CStr).UInt64;
		
		SkipWhiteSpace(&tasm->Iter);
		// if there's no plus/minus equation, assume `[reg+0]`.
		const char closer = *tasm->Iter;
		if( closer != '-' and closer != '+' and closer != ']' ) {
			printf("tasm error: invalid offset math operator '%c' in register indirection on line: %zu\n", closer, tasm->CurrLine);
			exit(-1);
		}
		else if( closer=='-' or closer=='+' ) {
			tasm->Iter++;
			SkipWhiteSpace(&tasm->Iter);
			if( !IsDecimal(*tasm->Iter) ) {
				printf("tasm error: invalid offset '%s' in register indirection on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
				exit(-1);
			}
			LexNumber(&tasm->Iter, tasm->Lexeme);
			SkipWhiteSpace(&tasm->Iter);
			const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
			offset = strtol(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
			offset = closer=='-' ? -offset : offset;
			//printf("offset == %i\n", offset);
		}
		if( *tasm->Iter != ']' ) {
			printf("tasm error: missing closing ']' bracket in register indirection on line: %zu\n", tasm->CurrLine);
			exit(-1);
		}
		tasm->Iter++;
	}
	else {
		printf("tasm error: invalid second operand for LEA opcode on line: %zu\n", tasm->CurrLine);
		exit(-1);
	}
	
	if( !firstpass ) {
		struct LabelInfo *label = LinkMap_Get(tasm->FuncTable, tasm->ActiveFuncLabel->CStr).Ptr;
		if( !label ) {
			printf("tasm error: undefined label '%s' on line: %zu\n", tasm->ActiveFuncLabel->CStr, tasm->CurrLine);
			exit(-1);
		}
		ByteBuffer_InsertByte(&label->Bytecode, addrmode1);
		ByteBuffer_InsertByte(&label->Bytecode, reg);
		if( addrmode1 & (Immediate|Register) )
			ByteBuffer_InsertInt(&label->Bytecode, imm.UInt64, sizeof imm.UInt64);
		else ByteBuffer_InsertByte(&label->Bytecode, imm.UChar);
		if( IsMemoryDeref )
			ByteBuffer_InsertInt(&label->Bytecode, offset, sizeof offset);
	}
	return true;
}

// syscall func_name, number_of_arguments
bool TaghaAsm_ParseSysCallInstr(struct TaghaAsmbler *const restrict tasm, const bool firstpass)
{
	if( !tasm or !tasm->Src or !tasm->Iter )
		return false;
	
	bool IsMemoryDeref = false;
	uint8_t addrmode1=0;
	union Value imm = (union Value){0};
	int32_t
		offset=0, argcount=0
	;
	
	tasm->ProgramCounter += 6;
	SkipWhiteSpace(&tasm->Iter);
	if( *tasm->Iter=='%' ) {
		addrmode1 = Immediate;
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		if( !LinkMap_HasKey(tasm->FuncTable, tasm->Lexeme->CStr) ) {
			printf("tasm error: undefined native '%s', you can define the native by writing '$native %s'. error on line %zu\n", tasm->Lexeme->CStr, tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
		tasm->ProgramCounter += 8;
		struct LabelInfo *native_label = LinkMap_Get(tasm->FuncTable, tasm->Lexeme->CStr).Ptr;
		if( !native_label->IsNativeFunc ) {
			printf("tasm error: label '%s' is NOT a native, in line %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
		const size_t addr = LinkMap_GetIndexByName(tasm->FuncTable, tasm->Lexeme->CStr);
		imm.Int64 = -(addr + 1);
	}
	else if( *tasm->Iter=='r' ) {
		addrmode1 = Register;
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		if( !LinkMap_HasKey(tasm->Registers, tasm->Lexeme->CStr) ) {
			printf("tasm error: invalid register name '%s' in line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
		tasm->ProgramCounter++;
		imm.UInt64 = LinkMap_Get(tasm->Registers, tasm->Lexeme->CStr).UInt64;
	}
	else if( *tasm->Iter=='[' ) {
		tasm->Iter++;
		IsMemoryDeref = true;
		
		// register indirection operand + register indirect addressing mode + 4 byte offset.
		SkipWhiteSpace(&tasm->Iter);
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		if( !String_NCmpCStr(tasm->Lexeme, "byte", sizeof "byte")
				or !String_NCmpCStr(tasm->Lexeme, "half", sizeof "half")
				or !String_NCmpCStr(tasm->Lexeme, "long", sizeof "long")
				or !String_NCmpCStr(tasm->Lexeme, "word", sizeof "word") )
		{
			addrmode1 = RegIndirect;
			switch( *tasm->Lexeme->CStr ) {
				case 'b': addrmode1 |= Byte; break;
				case 'h': addrmode1 |= TwoBytes; break;
				case 'l': addrmode1 |= FourBytes; break;
				case 'w': addrmode1 |= EightBytes; break;
			}
			tasm->ProgramCounter += 5;
			SkipWhiteSpace(&tasm->Iter);
			LexIdentifier(&tasm->Iter, tasm->Lexeme);
			if( !LinkMap_HasKey(tasm->Registers, tasm->Lexeme->CStr) ) {
				printf("tasm error: invalid register name '%s' in register indirection on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
				exit(-1);
			}
			imm.UInt64 = LinkMap_Get(tasm->Registers, tasm->Lexeme->CStr).UInt64;
			
			SkipWhiteSpace(&tasm->Iter);
			// if there's no plus/minus equation, assume `[reg+0]`
			const char closer = *tasm->Iter;
			if( closer != '-' and closer != '+' and closer != ']' ) {
				printf("tasm error: invalid offset math operator '%c' in register indirection on line: %zu\n", closer, tasm->CurrLine);
				exit(-1);
			}
			else if( closer=='-' or closer=='+' ) {
				tasm->Iter++;
				SkipWhiteSpace(&tasm->Iter);
				if( !IsDecimal(*tasm->Iter) ) {
					printf("tasm error: invalid offset '%s' in register indirection on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
					exit(-1);
				}
				LexNumber(&tasm->Iter, tasm->Lexeme);
				SkipWhiteSpace(&tasm->Iter);
				const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
				offset = strtol(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
				offset = closer=='-' ? -offset : offset;
				//printf("offset == %i\n", offset);
			}
			if( *tasm->Iter != ']' ) {
				printf("tasm error: missing closing ']' bracket in register indirection on line: %zu\n", tasm->CurrLine);
				exit(-1);
			}
			tasm->Iter++;
		}
		else {
			printf("tasm error: missing or invalid indirection size '%s' on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
	}
	else {
		printf("tasm error: invalid syntax for syscall! line: %zu\n", tasm->CurrLine);
		exit(-1);
	}
	
	SkipWhiteSpace(&tasm->Iter);
	if( *tasm->Iter==',' )
		tasm->Iter++;
	
	SkipWhiteSpace(&tasm->Iter);
	if( !IsDecimal(*tasm->Iter) ) {
		printf("tasm error: missing argument number for syscall! line: %zu\n", tasm->CurrLine);
		exit(-1);
	}
	LexNumber(&tasm->Iter, tasm->Lexeme);
	const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
	argcount = strtoul(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
	SkipWhiteSpace(&tasm->Iter);
	
	if( !firstpass ) {
		struct LabelInfo *label = LinkMap_Get(tasm->FuncTable, tasm->ActiveFuncLabel->CStr).Ptr;
		if( !label ) {
			printf("tasm error: undefined label '%s' on line: %zu\n", tasm->ActiveFuncLabel->CStr, tasm->CurrLine);
			exit(-1);
		}
		ByteBuffer_InsertByte(&label->Bytecode, addrmode1);
		ByteBuffer_InsertInt(&label->Bytecode, argcount, sizeof argcount);
		if( addrmode1 & Immediate )
			ByteBuffer_InsertInt(&label->Bytecode, imm.UInt64, sizeof imm.UInt64);
		else ByteBuffer_InsertByte(&label->Bytecode, imm.UChar);
		if( IsMemoryDeref )
			ByteBuffer_InsertInt(&label->Bytecode, offset, sizeof offset);
	}
	return true;
}

#ifdef FLOATING_POINT_OPS
bool TaghaAsm_ParseFloatConvInstr(struct TaghaAsmbler *const restrict tasm, const bool firstpass)
{
	if( !tasm or !tasm->Src or !tasm->Iter )
		return false;
	
	tasm->ProgramCounter += 2;
	SkipWhiteSpace(&tasm->Iter);
	uint8_t reg = 0;
	if( !IsAlphabetic(*tasm->Iter) ) {
		printf("tasm error: float conversion ops only take register arguments! line: %zu\n", tasm->CurrLine);
		exit(-1);
	}
	
	LexIdentifier(&tasm->Iter, tasm->Lexeme);
	if( !LinkMap_HasKey(tasm->Registers, tasm->Lexeme->CStr) ) {
		printf("tasm error: invalid register name '%s' in register indirection on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
		exit(-1);
	}
	reg = LinkMap_Get(tasm->Registers, tasm->Lexeme->CStr).UInt64;
	if( !firstpass ) {
		struct LabelInfo *label = LinkMap_Get(tasm->FuncTable, tasm->ActiveFuncLabel->CStr).Ptr;
		if( !label ) {
			printf("tasm error: undefined label '%s' on line: %zu\n", tasm->ActiveFuncLabel->CStr, tasm->CurrLine);
			exit(-1);
		}
		ByteBuffer_InsertByte(&label->Bytecode, Register);
		ByteBuffer_InsertByte(&label->Bytecode, reg);
	}
	return true;
}

bool TaghaAsm_ParseTwoOpFloatInstr(struct TaghaAsmbler *const restrict tasm, const bool firstpass)
{
	if( !tasm or !tasm->Src or !tasm->Iter )
		return false;
	
	tasm->ProgramCounter += 3;
	SkipWhiteSpace(&tasm->Iter);
	bool IsMemoryDeref = false;
	uint8_t
		addrmode1=0,
		//addrmode2=0,
		reg1=0
	;
	union Value imm = (union Value){0};
	int32_t offset = 0;
	
	uint8_t floatsize = 0;
	
	// check the first operand.
	if( *tasm->Iter=='l' or *tasm->Iter=='w' ) {
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		if( !String_NCmpCStr(tasm->Lexeme, "long", sizeof "long") or !String_NCmpCStr(tasm->Lexeme, "word", sizeof "word") ) {
			switch( *tasm->Lexeme->CStr ) {
				case 'l': floatsize |= FourBytes; break;
				case 'w': floatsize |= EightBytes; break;
			}
		}
		SkipWhiteSpace(&tasm->Iter);
	}
	
	if( *tasm->Iter=='r' ) {
		// register operand and register addressing mode.
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		if( !LinkMap_HasKey(tasm->Registers, tasm->Lexeme->CStr) ) {
			printf("tasm error: invalid register name '%s' on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
		addrmode1 |= Reserved;
		reg1 = LinkMap_Get(tasm->Registers, tasm->Lexeme->CStr).UInt64;
	}
	else if( *tasm->Iter=='[' ) {
		tasm->Iter++;
		IsMemoryDeref = true;
		// register indirection operand + register indirect addressing mode + 4 byte offset.
		SkipWhiteSpace(&tasm->Iter);
		
		// get memory dereferencing size.
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		if( !String_NCmpCStr(tasm->Lexeme, "long", sizeof "long")
				or !String_NCmpCStr(tasm->Lexeme, "word", sizeof "word") )
		{
			//addrmode1 = RegIndirect;
			switch( *tasm->Lexeme->CStr ) {
				case 'l': addrmode1 |= FourBytes; break;
				case 'w': addrmode1 |= EightBytes; break;
			}
			SkipWhiteSpace(&tasm->Iter);
			tasm->ProgramCounter += 4;
			
			// get register name.
			LexIdentifier(&tasm->Iter, tasm->Lexeme);
			if( !LinkMap_HasKey(tasm->Registers, tasm->Lexeme->CStr) ) {
				printf("tasm error: invalid register name '%s' in register indirection on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
				exit(-1);
			}
			reg1 = LinkMap_Get(tasm->Registers, tasm->Lexeme->CStr).UInt64;
			
			SkipWhiteSpace(&tasm->Iter);
			// if there's no plus/minus equation, assume `[reg+0]`.
			const char closer = *tasm->Iter;
			if( closer != '-' and closer != '+' and closer != ']' ) {
				printf("tasm error: invalid offset math operator '%c' in register indirection on line: %zu\n", closer, tasm->CurrLine);
				exit(-1);
			}
			else if( closer=='-' or closer=='+' ) {
				tasm->Iter++;
				SkipWhiteSpace(&tasm->Iter);
				if( !IsDecimal(*tasm->Iter) ) {
					printf("tasm error: invalid offset '%s' in register indirection on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
					exit(-1);
				}
				LexNumber(&tasm->Iter, tasm->Lexeme);
				SkipWhiteSpace(&tasm->Iter);
				const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
				offset = strtol(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
				offset = closer=='-' ? -offset : offset;
				//printf("offset == %i\n", offset);
			}
			if( *tasm->Iter != ']' ) {
				printf("tasm error: missing closing ']' bracket in register indirection on line: %zu\n", tasm->CurrLine);
				exit(-1);
			}
			tasm->Iter++;
		}
		else {
			printf("tasm error: missing or invalid indirection size '%s' on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
	}
	else {
		printf("tasm error: invalid 1st operand on line: %zu\n", tasm->CurrLine);
		exit(-1);
	}
	
	
	SkipWhiteSpace(&tasm->Iter);
	// ok, let's read the secondary operand!
	// the second operand can be an imm, register, or register indirection.
	if( *tasm->Iter==',' )
		tasm->Iter++;
	
	SkipWhiteSpace(&tasm->Iter);
	// immediate/constant operand value.
	if( IsDecimal(*tasm->Iter) ) {
		LexNumber(&tasm->Iter, tasm->Lexeme);
		tasm->ProgramCounter += 8;
		addrmode1 |= Immediate;
		const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
		imm.UInt64 = strtoull(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
	}
	else if( *tasm->Iter=='r' ) {
		// register operand and register addressing mode.
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		if( !LinkMap_HasKey(tasm->Registers, tasm->Lexeme->CStr) ) {
			printf("tasm error: invalid register name '%s' in line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
		tasm->ProgramCounter++;
		addrmode1 |= Register;
		imm.UInt64 = LinkMap_Get(tasm->Registers, tasm->Lexeme->CStr).UInt64;
	}
	else if( *tasm->Iter=='[' ) {
		tasm->Iter++;
		if( IsMemoryDeref ) {
			printf("tasm error: memory to memory operations are not allowed! line: %zu\n", tasm->CurrLine);
			exit(-1);
		}
		IsMemoryDeref = true;
		// register indirection operand + register indirect addressing mode + 4 byte offset.
		SkipWhiteSpace(&tasm->Iter);
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		if( !String_NCmpCStr(tasm->Lexeme, "long", sizeof "long")
				or !String_NCmpCStr(tasm->Lexeme, "word", sizeof "word") )
		{
			addrmode1 |= RegIndirect;
			switch( *tasm->Lexeme->CStr ) {
				case 'l': addrmode1 |= FourBytes; break;
				case 'w': addrmode1 |= EightBytes; break;
			}
			tasm->ProgramCounter += 5;
			SkipWhiteSpace(&tasm->Iter);
			LexIdentifier(&tasm->Iter, tasm->Lexeme);
			if( !LinkMap_HasKey(tasm->Registers, tasm->Lexeme->CStr) ) {
				printf("tasm error: invalid register name '%s' in register indirection on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
				exit(-1);
			}
			imm.UInt64 = LinkMap_Get(tasm->Registers, tasm->Lexeme->CStr).UInt64;
			
			SkipWhiteSpace(&tasm->Iter);
			// if there's no plus/minus equation, assume `[reg+0]`
			const char closer = *tasm->Iter;
			if( closer != '-' and closer != '+' and closer != ']' ) {
				printf("tasm error: invalid offset math operator '%c' in register indirection on line: %zu\n", closer, tasm->CurrLine);
				exit(-1);
			}
			else if( closer=='-' or closer=='+' ) {
				tasm->Iter++;
				SkipWhiteSpace(&tasm->Iter);
				if( !IsDecimal(*tasm->Iter) ) {
					printf("tasm error: invalid offset '%s' in register indirection on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
					exit(-1);
				}
				LexNumber(&tasm->Iter, tasm->Lexeme);
				SkipWhiteSpace(&tasm->Iter);
				const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
				offset = strtol(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
				offset = closer=='-' ? -offset : offset;
				//printf("offset == %i\n", offset);
			}
			if( *tasm->Iter != ']' ) {
				printf("tasm error: missing closing ']' bracket in register indirection on line: %zu\n", tasm->CurrLine);
				exit(-1);
			}
			tasm->Iter++;
		}
		else {
			printf("tasm error: missing or invalid indirection size '%s' on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
	}
	
	if( !floatsize and !IsMemoryDeref ) {
		printf("tasm error: float operations require a 'long' or 'word' size when there's no memory operations. on line: %zu\n", tasm->CurrLine);
		exit(-1);
	}
	
	if( !firstpass ) {
		struct LabelInfo *label = LinkMap_Get(tasm->FuncTable, tasm->ActiveFuncLabel->CStr).Ptr;
		if( !label ) {
			printf("tasm error: undefined label '%s' on line: %zu\n", tasm->ActiveFuncLabel->CStr, tasm->CurrLine);
			exit(-1);
		}
		if( !IsMemoryDeref )
			addrmode1 |= floatsize;
		ByteBuffer_InsertByte(&label->Bytecode, addrmode1);
		//ByteBuffer_InsertByte(&label->Bytecode, addrmode2);
		ByteBuffer_InsertByte(&label->Bytecode, reg1);
		if( addrmode1 & Immediate )
			ByteBuffer_InsertInt(&label->Bytecode, imm.UInt64, sizeof imm.UInt64);
		else ByteBuffer_InsertByte(&label->Bytecode, imm.UChar);
		if( IsMemoryDeref )
			ByteBuffer_InsertInt(&label->Bytecode, offset, sizeof(int32_t));
	}
	return true;
}
bool TaghaAsm_ParseOneOpFloatInstr(struct TaghaAsmbler *const restrict tasm, const bool firstpass)
{
	if( !tasm or !tasm->Src or !tasm->Iter )
		return false;
	
	tasm->ProgramCounter += 2;
	SkipWhiteSpace(&tasm->Iter);
	bool IsMemoryDeref = false;
	uint8_t addrmode1=0;
	union Value imm = (union Value){0};
	int32_t offset = 0;
	
	
	uint8_t floatsize = 0;
	
	// check the first operand.
	if( *tasm->Iter=='l' or *tasm->Iter=='w' ) {
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		if( !String_NCmpCStr(tasm->Lexeme, "long", sizeof "long") or !String_NCmpCStr(tasm->Lexeme, "word", sizeof "word") ) {
			switch( *tasm->Lexeme->CStr ) {
				case 'l': floatsize |= FourBytes; break;
				case 'w': floatsize |= EightBytes; break;
			}
		}
		SkipWhiteSpace(&tasm->Iter);
	}
	
	// immediate/constant operand value.
	if( IsDecimal(*tasm->Iter) ) {
		LexNumber(&tasm->Iter, tasm->Lexeme);
		tasm->ProgramCounter += 8;
		addrmode1 = Immediate;
		const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
		imm.UInt64 = strtoull(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
	}
	// label value.
	else if( *tasm->Iter=='.' or *tasm->Iter=='%' ) {
		const bool isfunclbl = *tasm->Iter=='%';
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		tasm->ProgramCounter += 8;
		if( !firstpass and !LinkMap_HasKey(isfunclbl ? tasm->FuncTable : tasm->LabelTable, tasm->Lexeme->CStr) ) {
			printf("tasm error: undefined label '%s' on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
		addrmode1 = Immediate;
		if( !firstpass ) {
			struct LabelInfo *label = LinkMap_Get(isfunclbl ? tasm->FuncTable : tasm->LabelTable, tasm->Lexeme->CStr).Ptr;
			if( !isfunclbl ) {
				imm.Int64 = label->Addr - tasm->ProgramCounter;
			#ifdef TASM_DEBUG
				printf("label->Addr (%llu) - tasm->ProgramCounter (%zu) == '%lli'\n", label->Addr, tasm->ProgramCounter, label->Addr - tasm->ProgramCounter);
			#endif
			}
			else {
				imm.Int64 = label->IsNativeFunc ? -(LinkMap_GetIndexByName(tasm->FuncTable, tasm->Lexeme->CStr) + 1) : (LinkMap_GetIndexByName(tasm->FuncTable, tasm->Lexeme->CStr) + 1);
			}
		}
	}
	else if( *tasm->Iter=='r' ) {
		// register operand and register addressing mode.
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		if( !LinkMap_HasKey(tasm->Registers, tasm->Lexeme->CStr) ) {
			printf("tasm error: invalid register name '%s' in line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
		tasm->ProgramCounter++;
		addrmode1 = Register;
		imm.UInt64 = LinkMap_Get(tasm->Registers, tasm->Lexeme->CStr).UInt64;
	}
	else if( *tasm->Iter=='[' ) {
		tasm->Iter++;
		IsMemoryDeref = true;
		
		// register indirection operand + register indirect addressing mode + 4 byte offset.
		SkipWhiteSpace(&tasm->Iter);
		LexIdentifier(&tasm->Iter, tasm->Lexeme);
		if( !String_NCmpCStr(tasm->Lexeme, "long", sizeof "long")
				or !String_NCmpCStr(tasm->Lexeme, "word", sizeof "word") )
		{
			addrmode1 = RegIndirect;
			switch( *tasm->Lexeme->CStr ) {
				case 'l': addrmode1 |= FourBytes; break;
				case 'w': addrmode1 |= EightBytes; break;
			}
			tasm->ProgramCounter += 5;
			SkipWhiteSpace(&tasm->Iter);
			LexIdentifier(&tasm->Iter, tasm->Lexeme);
			if( !LinkMap_HasKey(tasm->Registers, tasm->Lexeme->CStr) ) {
				printf("tasm error: invalid register name '%s' in register indirection on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
				exit(-1);
			}
			imm.UInt64 = LinkMap_Get(tasm->Registers, tasm->Lexeme->CStr).UInt64;
			
			SkipWhiteSpace(&tasm->Iter);
			// if there's no plus/minus equation, assume `[reg+0]`
			const char closer = *tasm->Iter;
			if( closer != '-' and closer != '+' and closer != ']' ) {
				printf("tasm error: invalid offset math operator '%c' in register indirection on line: %zu\n", closer, tasm->CurrLine);
				exit(-1);
			}
			else if( closer=='-' or closer=='+' ) {
				tasm->Iter++;
				SkipWhiteSpace(&tasm->Iter);
				if( !IsDecimal(*tasm->Iter) ) {
					printf("tasm error: invalid offset '%s' in register indirection on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
					exit(-1);
				}
				LexNumber(&tasm->Iter, tasm->Lexeme);
				SkipWhiteSpace(&tasm->Iter);
				const bool isbinary = !String_NCmpCStr(tasm->Lexeme, "0b", 2) or !String_NCmpCStr(tasm->Lexeme, "0B", 2) ? true : false;
				offset = strtol(isbinary ? tasm->Lexeme->CStr+2 : tasm->Lexeme->CStr, NULL, isbinary ? 2 : 0);
				offset = closer=='-' ? -offset : offset;
				//printf("offset == %i\n", offset);
			}
			if( *tasm->Iter != ']' ) {
				printf("tasm error: missing closing ']' bracket in register indirection on line: %zu\n", tasm->CurrLine);
				exit(-1);
			}
			tasm->Iter++;
		}
		else {
			printf("tasm error: missing or invalid indirection size '%s' on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
			exit(-1);
		}
	}
	
	if( !floatsize and !IsMemoryDeref ) {
		printf("tasm error: float operations require a 'long' or 'word' size when there's no memory operations. on line: %zu\n", tasm->CurrLine);
		exit(-1);
	}
	
	if( !firstpass ) {
		struct LabelInfo *label = LinkMap_Get(tasm->FuncTable, tasm->ActiveFuncLabel->CStr).Ptr;
		if( !label ) {
			printf("tasm error: undefined label '%s' on line: %zu\n", tasm->ActiveFuncLabel->CStr, tasm->CurrLine);
			exit(-1);
		}
		if( !IsMemoryDeref )
			addrmode1 |= floatsize;
		ByteBuffer_InsertByte(&label->Bytecode, addrmode1);
		if( addrmode1 & Immediate )
			ByteBuffer_InsertInt(&label->Bytecode, imm.UInt64, sizeof imm.UInt64);
		else ByteBuffer_InsertByte(&label->Bytecode, imm.UChar);
		if( IsMemoryDeref )
			ByteBuffer_InsertInt(&label->Bytecode, offset, sizeof offset);
	}
	return true;
}
#endif


bool TaghaAsm_Assemble(struct TaghaAsmbler *const restrict tasm)
{
	if( !tasm or !tasm->Src )
		return false;
	
	// set up our data.
	tasm->Lexeme = &(struct String){0},
	tasm->LabelTable = &(struct LinkMap){0},
	tasm->FuncTable = &(struct LinkMap){0},
	tasm->VarTable = &(struct LinkMap){0},
	tasm->Opcodes = &(struct LinkMap){0},
	tasm->Registers = &(struct LinkMap){0};
	
	LinkMap_SetItemDestructor(tasm->LabelTable, Label_Free);
	LinkMap_SetItemDestructor(tasm->FuncTable, Label_Free);
	LinkMap_SetItemDestructor(tasm->VarTable, (bool(*)())ByteBuffer_Free);
	
	// set up registers and map their IDs
	LinkMap_Insert(tasm->Registers, "RALAF", (union Value){.UInt64 = regAlaf});
	LinkMap_Insert(tasm->Registers, "ralaf", (union Value){.UInt64 = regAlaf});
	
	LinkMap_Insert(tasm->Registers, "RBETH", (union Value){.UInt64 = regBeth});
	LinkMap_Insert(tasm->Registers, "rbeth", (union Value){.UInt64 = regBeth});
	
	LinkMap_Insert(tasm->Registers, "RGAMAL", (union Value){.UInt64 = regGamal});
	LinkMap_Insert(tasm->Registers, "rgamal", (union Value){.UInt64 = regGamal});
	
	LinkMap_Insert(tasm->Registers, "RDALATH", (union Value){.UInt64 = regDalath});
	LinkMap_Insert(tasm->Registers, "rdalath", (union Value){.UInt64 = regDalath});
	
	LinkMap_Insert(tasm->Registers, "RHEH", (union Value){.UInt64 = regHeh});
	LinkMap_Insert(tasm->Registers, "rheh", (union Value){.UInt64 = regHeh});
	
	LinkMap_Insert(tasm->Registers, "RWAW", (union Value){.UInt64 = regWaw});
	LinkMap_Insert(tasm->Registers, "rwaw", (union Value){.UInt64 = regWaw});
	
	LinkMap_Insert(tasm->Registers, "RZAIN", (union Value){.UInt64 = regZain});
	LinkMap_Insert(tasm->Registers, "rzain", (union Value){.UInt64 = regZain});
	
	LinkMap_Insert(tasm->Registers, "RHETH", (union Value){.UInt64 = regHeth});
	LinkMap_Insert(tasm->Registers, "rheth", (union Value){.UInt64 = regHeth});
	
	LinkMap_Insert(tasm->Registers, "RTETH", (union Value){.UInt64 = regTeth});
	LinkMap_Insert(tasm->Registers, "rteth", (union Value){.UInt64 = regTeth});
	
	LinkMap_Insert(tasm->Registers, "RYODH", (union Value){.UInt64 = regYodh});
	LinkMap_Insert(tasm->Registers, "ryodh", (union Value){.UInt64 = regYodh});
	
	LinkMap_Insert(tasm->Registers, "RKAF", (union Value){.UInt64 = regKaf});
	LinkMap_Insert(tasm->Registers, "rkaf", (union Value){.UInt64 = regKaf});
	
	LinkMap_Insert(tasm->Registers, "RLAMADH", (union Value){.UInt64 = regLamadh});
	LinkMap_Insert(tasm->Registers, "rlamadh", (union Value){.UInt64 = regLamadh});
	
	LinkMap_Insert(tasm->Registers, "RMEEM", (union Value){.UInt64 = regMeem});
	LinkMap_Insert(tasm->Registers, "rmeem", (union Value){.UInt64 = regMeem});
	
	LinkMap_Insert(tasm->Registers, "RNOON", (union Value){.UInt64 = regNoon});
	LinkMap_Insert(tasm->Registers, "rnoon", (union Value){.UInt64 = regNoon});
	
	LinkMap_Insert(tasm->Registers, "RSEMKATH", (union Value){.UInt64 = regSemkath});
	LinkMap_Insert(tasm->Registers, "rsemkath", (union Value){.UInt64 = regSemkath});
	
	LinkMap_Insert(tasm->Registers, "R_EH", (union Value){.UInt64 = reg_Eh});
	LinkMap_Insert(tasm->Registers, "r_eh", (union Value){.UInt64 = reg_Eh});
	
	LinkMap_Insert(tasm->Registers, "RPEH", (union Value){.UInt64 = regPeh});
	LinkMap_Insert(tasm->Registers, "rpeh", (union Value){.UInt64 = regPeh});
	
	LinkMap_Insert(tasm->Registers, "RSADHE", (union Value){.UInt64 = regSadhe});
	LinkMap_Insert(tasm->Registers, "rsadhe", (union Value){.UInt64 = regSadhe});
	
	LinkMap_Insert(tasm->Registers, "RQOF", (union Value){.UInt64 = regQof});
	LinkMap_Insert(tasm->Registers, "rqof", (union Value){.UInt64 = regQof});
	
	LinkMap_Insert(tasm->Registers, "RREESH", (union Value){.UInt64 = regReesh});
	LinkMap_Insert(tasm->Registers, "rreesh", (union Value){.UInt64 = regReesh});
	
	LinkMap_Insert(tasm->Registers, "RSHEEN", (union Value){.UInt64 = regSheen});
	LinkMap_Insert(tasm->Registers, "rsheen", (union Value){.UInt64 = regSheen});
	
	LinkMap_Insert(tasm->Registers, "RTAW", (union Value){.UInt64 = regTaw});
	LinkMap_Insert(tasm->Registers, "rtaw", (union Value){.UInt64 = regTaw});
	
	LinkMap_Insert(tasm->Registers, "RTAW", (union Value){.UInt64 = regTaw});
	LinkMap_Insert(tasm->Registers, "rtaw", (union Value){.UInt64 = regTaw});
	
	LinkMap_Insert(tasm->Registers, "RTAW", (union Value){.UInt64 = regTaw});
	LinkMap_Insert(tasm->Registers, "rtaw", (union Value){.UInt64 = regTaw});
	
	LinkMap_Insert(tasm->Registers, "RSP", (union Value){.UInt64 = regStk});
	LinkMap_Insert(tasm->Registers, "rsp", (union Value){.UInt64 = regStk});
	
	LinkMap_Insert(tasm->Registers, "RBP", (union Value){.UInt64 = regBase});
	LinkMap_Insert(tasm->Registers, "rbp", (union Value){.UInt64 = regBase});
	
	// set up our instruction set!
	#define X(x) LinkMap_Insert(tasm->Opcodes, #x, (union Value){.UInt64 = x});
		INSTR_SET;
	#undef X
	
	/* FIRST PASS. Collect labels and their PC relative addresses */
	#define MAX_LINE_CHARS 2048
	char line_buffer[MAX_LINE_CHARS] = {0};
	
#ifdef TASM_DEBUG
	puts("\ntasm: FIRST PASS Begin!\n");
#endif
	
	for( tasm->Iter = fgets(line_buffer, MAX_LINE_CHARS, tasm->Src) ; tasm->Iter ; tasm->Iter = fgets(line_buffer, MAX_LINE_CHARS, tasm->Src) ) {
		// set up first line for error checks.
		tasm->CurrLine++;
	#ifdef TASM_DEBUG
		//printf("tasm debug: printing line:: '%s'\n", tasm->Iter);
	#endif
		while( *tasm->Iter ) {
			String_Del(tasm->Lexeme);
			// skip whitespace.
			SkipWhiteSpace(&tasm->Iter);
			
			// skip to next line if comment or directive.
			if( *tasm->Iter=='\n' or *tasm->Iter==';' or *tasm->Iter=='#' )
				break;
			
			else if( *tasm->Iter=='}' ) {
				tasm->Iter++;
				String_Del(tasm->ActiveFuncLabel);
				tasm->ActiveFuncLabel = NULL;
				tasm->ProgramCounter = 0;
				break;
			}
			// parse the directives!
			else if( *tasm->Iter=='$' ) {
				LexIdentifier(&tasm->Iter, tasm->Lexeme);
				if( !String_NCmpCStr(tasm->Lexeme, "$stacksize", sizeof "$stacksize") ) {
					TaghaAsm_ParseStackDirective(tasm);
				#ifdef TASM_DEBUG
					printf("tasm: Stack size set to: %u\n", tasm->Stacksize);
				#endif
				}
				else if( !String_NCmpCStr(tasm->Lexeme, "$global", sizeof "$global") )
					TaghaAsm_ParseGlobalVarDirective(tasm);
				else if( !String_NCmpCStr(tasm->Lexeme, "$native", sizeof "$native") )
					TaghaAsm_ParseNativeDirective(tasm);
				break;
			}
			
			// holy cannoli, we found a label!
			else if( *tasm->Iter=='.' or *tasm->Iter=='%' ) {
				const bool funclbl = *tasm->Iter == '%';
				// the dot or percent is added to our lexeme
				LexIdentifier(&tasm->Iter, tasm->Lexeme);
				if( *tasm->Iter == ':' )
					tasm->Iter++;
				
				SkipWhiteSpace(&tasm->Iter);
				if( funclbl ) {
					if( *tasm->Iter != '{' ) {
						printf("tasm error: missing curly '{' bracket! Curly bracket must be on the same line as label. line: %zu\n", tasm->CurrLine);
						exit(-1);
					}
					tasm->Iter++;
				}
				
				if( !IsAlphabetic(tasm->Lexeme->CStr[1]) ) {
					printf("tasm error: %s labels must have alphabetic names! line: %zu\n", funclbl ? "function" : "jump", tasm->CurrLine);
					exit(-1);
				}
				else if( LinkMap_HasKey(funclbl ? tasm->FuncTable : tasm->LabelTable, tasm->Lexeme->CStr) ) {
					printf("tasm error: redefinition of label '%s' on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
					exit(-1);
				}
				
				struct LabelInfo *label = calloc(1, sizeof *label);
				if( !label ) {
					puts("tasm error: OUT OF MEMORY! Exiting...");
					exit(-1);
				}
				if( funclbl ) {
					tasm->ActiveFuncLabel = &(struct String){0};
					String_Copy(tasm->ActiveFuncLabel, tasm->Lexeme);
				}
				label->Addr = tasm->ProgramCounter;
				label->IsFunc = funclbl;
#ifdef TASM_DEBUG
				printf("%s Label '%s' is located at address: %zu\n", funclbl ? "Func" : "Local", tasm->Lexeme->CStr, tasm->ProgramCounter);
#endif
				LinkMap_Insert(funclbl ? tasm->FuncTable : tasm->LabelTable, tasm->Lexeme->CStr, (union Value){.Ptr = label});
			}
			// it's an opcode!
			else if( IsAlphabetic(*tasm->Iter) ) {
				if( !tasm->ActiveFuncLabel ) {
					printf("tasm error: opcode outside of function blocks on line: %zu\n", tasm->CurrLine);
					exit(-1);
				}
				LexIdentifier(&tasm->Iter, tasm->Lexeme);
				if( !LinkMap_HasKey(tasm->Opcodes, tasm->Lexeme->CStr) ) {
					printf("tasm error: unknown opcode '%s' on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
					exit(-1);
				}
				uint8_t opcode = LinkMap_Get(tasm->Opcodes, tasm->Lexeme->CStr).UInt64;
				switch( opcode ) {
					case mov:
					case add: case sub: case mul: case divi: case mod:
					case andb: case orb: case xorb: case shl: case shr:
					case lt: case gt: case cmp: case neq:
						TaghaAsm_ParseTwoOpInstr(tasm, true);
						break;
					
					case push: case pop: case notb:
					case inc: case dec: case neg:
					case jz: case jnz: case jmp: case call:
						TaghaAsm_ParseOneOpInstr(tasm, true);
						break;
					
					case ret: case nop:
						TaghaAsm_ParseNoOpsInstr(tasm, true);
						break;
					case lea:
						TaghaAsm_ParseLEAInstr(tasm, true);
						break;
					case syscall:
						TaghaAsm_ParseSysCallInstr(tasm, true);
						break;
				#ifdef FLOATING_POINT_OPS
					case flt2dbl: case dbl2flt:
						TaghaAsm_ParseFloatConvInstr(tasm, true);
						break;
					case addf: case subf: case mulf: case divf:
					case ltf: case gtf: case cmpf: case neqf:
						TaghaAsm_ParseTwoOpFloatInstr(tasm, true);
						break;
					case incf: case decf: case negf:
						TaghaAsm_ParseOneOpFloatInstr(tasm, true);
						break;
				#endif
				}
			}
		}
	}
#ifdef TASM_DEBUG
	puts("\ntasm: FIRST PASS End!\n");
#endif
	rewind(tasm->Src);
	
#ifdef TASM_DEBUG
	puts("\ntasm: SECOND PASS Start!\n");
#endif
	tasm->CurrLine = 0;
	
	for( tasm->Iter = fgets(line_buffer, MAX_LINE_CHARS, tasm->Src) ; tasm->Iter ; tasm->Iter = fgets(line_buffer, MAX_LINE_CHARS, tasm->Src) ) {
		// set up first line for error checks.
		tasm->CurrLine++;
	#ifdef TASM_DEBUG
		//printf("tasm debug: printing line:: '%s'\n", tasm->Iter);
	#endif
		while( *tasm->Iter ) {
			String_Del(tasm->Lexeme);
			// skip whitespace.
			SkipWhiteSpace(&tasm->Iter);
			
			// skip to next line if comment or directive.
			if( *tasm->Iter=='\n' or *tasm->Iter==';' or *tasm->Iter=='#' or *tasm->Iter=='$' )
				break;
			else if( *tasm->Iter=='}' ) {
				tasm->Iter++;
				String_Del(tasm->ActiveFuncLabel);
				tasm->ActiveFuncLabel = NULL;
				tasm->ProgramCounter = 0;
				break;
			}
			// skip labels in second pass.
			else if( *tasm->Iter=='.' or *tasm->Iter=='%' ) {
				const bool funclbl = *tasm->Iter == '%';
				LexIdentifier(&tasm->Iter, tasm->Lexeme);
				tasm->Iter++;
				SkipWhiteSpace(&tasm->Iter);
				if( funclbl ) {
					tasm->Iter++;
					tasm->ActiveFuncLabel = &(struct String){0};
					String_Copy(tasm->ActiveFuncLabel, tasm->Lexeme);
					SkipWhiteSpace(&tasm->Iter);
				}
			}
			// parse opcode!
			if( IsAlphabetic(*tasm->Iter) ) {
				LexIdentifier(&tasm->Iter, tasm->Lexeme);
				if( !LinkMap_HasKey(tasm->Opcodes, tasm->Lexeme->CStr) ) {
					printf("tasm error: unknown opcode '%s' on line: %zu\n", tasm->Lexeme->CStr, tasm->CurrLine);
					exit(-1);
				}
				uint8_t opcode = LinkMap_Get(tasm->Opcodes, tasm->Lexeme->CStr).UInt64;
				struct LabelInfo *label = LinkMap_Get(tasm->FuncTable, tasm->ActiveFuncLabel->CStr).Ptr;
				ByteBuffer_InsertByte(&label->Bytecode, opcode);
				switch( opcode ) {
					case mov:
					case add: case sub: case mul: case divi: case mod:
					case andb: case orb: case xorb: case shl: case shr:
					case lt: case gt: case cmp: case neq:
						TaghaAsm_ParseTwoOpInstr(tasm, false);
						break;
					
					case push: case pop: case notb:
					case inc: case dec: case neg:
					case jz: case jnz: case jmp: case call:
						TaghaAsm_ParseOneOpInstr(tasm, false);
						break;
					
					case ret: case nop:
						TaghaAsm_ParseNoOpsInstr(tasm, false);
						break;
					case lea:
						TaghaAsm_ParseLEAInstr(tasm, false);
						break;
					case syscall:
						TaghaAsm_ParseSysCallInstr(tasm, false);
						break;
				#ifdef FLOATING_POINT_OPS
					case flt2dbl: case dbl2flt:
						TaghaAsm_ParseFloatConvInstr(tasm, false);
						break;
					case addf: case subf: case mulf: case divf:
					case ltf: case gtf: case cmpf: case neqf:
						TaghaAsm_ParseTwoOpFloatInstr(tasm, false);
						break;
					case incf: case decf: case negf:
						TaghaAsm_ParseOneOpFloatInstr(tasm, false);
						break;
				#endif
				}
			}
		}
	}
	
#ifdef TASM_DEBUG
	puts("\ntasm: SECOND PASS End!\n");
#endif
	if( !tasm->Stacksize )
		tasm->Stacksize = 128;
	
	// build the header table.
	struct ByteBuffer tbcfile; ByteBuffer_Init(&tbcfile);
	ByteBuffer_InsertInt(&tbcfile, 0xC0DE, sizeof(uint16_t));
	ByteBuffer_InsertInt(&tbcfile, tasm->Stacksize, sizeof tasm->Stacksize);
	ByteBuffer_InsertByte(&tbcfile, 0);
	
	// build our func table
	struct ByteBuffer functable; ByteBuffer_Init(&functable);
	
	for( struct LinkNode *t=tasm->FuncTable->Head ; t ; t=t->After ) {
		struct LabelInfo *label = t->Data.Ptr;
		if( !label )
			continue;
		
		ByteBuffer_InsertByte(&functable, label->IsNativeFunc);
		ByteBuffer_InsertInt(&functable, t->KeyName.Len, sizeof(uint32_t));
		label->IsNativeFunc ? 
			ByteBuffer_InsertInt(&functable, 8, sizeof(uint32_t))
				: ByteBuffer_InsertInt(&functable, label->Bytecode.Count, sizeof(uint32_t));
		//printf("label->Bytecode.Count == %u\n", label->Bytecode.Count);
		
		ByteBuffer_InsertString(&functable, t->KeyName.CStr+1, t->KeyName.Len-1);
		label->IsNativeFunc ?
			ByteBuffer_InsertInt(&functable, 0, sizeof(uint64_t))
				: ByteBuffer_Append(&functable, &label->Bytecode) ;
		
		//printf("func label: %s\nData:\n", t->KeyName.CStr);
		//for( size_t i=0 ; i<label->Bytecode.Count ; i++ )
			//printf("bytecode[%zu] == %u\n", i, label->Bytecode.Buffer[i]);
		//puts("\n");
	}
	ByteBuffer_InsertInt(&tbcfile, functable.Count + 4, sizeof(uint32_t));
	//printf("functable.Count == %u\n", functable.Count);
	
	size_t funcs = 0;
	for( struct LinkNode *t=tasm->FuncTable->Head ; t ; t=t->After ) {
		struct LabelInfo *label = t->Data.Ptr;
		if( !label or !label->IsFunc )
			continue;
		funcs++;
	}
	ByteBuffer_InsertInt(&tbcfile, funcs, sizeof(uint32_t));
	ByteBuffer_Append(&tbcfile, &functable);
	ByteBuffer_Del(&functable);
	
	// build our global var table
	struct ByteBuffer datatable; ByteBuffer_Init(&datatable);
	
	for( struct LinkNode *t=tasm->VarTable->Head ; t ; t=t->After ) {
		struct ByteBuffer *bytedata = t->Data.Ptr;
		if( !bytedata )
			continue;
		
		ByteBuffer_InsertByte(&datatable, 0);
		ByteBuffer_InsertInt(&datatable, t->KeyName.Len+1, sizeof(uint32_t));
		
		ByteBuffer_InsertInt(&datatable, bytedata->Count, sizeof(uint32_t));
		ByteBuffer_InsertString(&datatable, t->KeyName.CStr, t->KeyName.Len);
		ByteBuffer_Append(&datatable, bytedata);
	#ifdef TASM_DEBUG
		printf("global var: %s\nData:\n", t->KeyName.CStr);
		for( size_t i=0 ; i<bytedata->Count ; i++ )
			printf("bytecode[%zu] == %u\n", i, bytedata->Buffer[i]);
		puts("\n");
	#endif
	}
	
	size_t datacount = 0;
	for( struct LinkNode *t=tasm->VarTable->Head ; t ; t=t->After ) {
		struct ByteBuffer *bytedata = t->Data.Ptr;
		if( !bytedata )
			continue;
		datacount++;
	}
	ByteBuffer_InsertInt(&tbcfile, datacount, sizeof(uint32_t));
	ByteBuffer_Append(&tbcfile, &datatable);
	ByteBuffer_Del(&datatable);
	
	char *iter = tasm->OutputName.CStr;
	size_t len = 0;
	while( *++iter );
	while( *--iter != '.' )
		++len;
	
	iter++;
	*iter = 't'; iter++; --len;
	*iter = 'b'; iter++; --len;
	*iter = 'c'; iter++; --len;
	memset(iter, 0, len);
	
	FILE *tbcscript = fopen(tasm->OutputName.CStr, "w+");
	if( !tbcscript ) {
		puts("tasm error: unable to create output file!");
		exit(-1);
	}
	ByteBuffer_DumpToFile(&tbcfile, tbcscript);
	fclose(tbcscript); tbcscript=NULL;
	
	ByteBuffer_Del(&tbcfile);
	String_Del(&tasm->OutputName);
	String_Del(tasm->Lexeme);
	String_Del(tasm->ActiveFuncLabel);
	
	LinkMap_Del(tasm->LabelTable);
	LinkMap_Del(tasm->FuncTable);
	LinkMap_Del(tasm->VarTable);
	LinkMap_Del(tasm->Opcodes);
	LinkMap_Del(tasm->Registers);
	fclose(tasm->Src); tasm->Src=NULL;
	return true;
}

bool Label_Free(struct LabelInfo **restrict labelref)
{
	if( !*labelref )
		return false;
	
	ByteBuffer_Del(&(*labelref)->Bytecode);
	free(*labelref); *labelref=NULL;
	return true;
}

static size_t GetFileSize(FILE *const restrict file)
{
	int64_t size = 0L;
	if( !file )
		return size;
	
	if( !fseek(file, 0, SEEK_END) ) {
		size = ftell(file);
		if( size == -1 )
			return 0L;
		rewind(file);
	}
	return (size_t)size;
}

int main(int argc, char *argv[static argc])
{
	if( !argv[1] ) {
		return -1;
	}
	
	FILE *tasmfile = fopen(argv[1], "r");
	if( !tasmfile )
		return -1;
	
	struct TaghaAsmbler *tasm = &(struct TaghaAsmbler){0};
	tasm->Src = tasmfile;
	tasm->SrcSize = GetFileSize(tasmfile);
	String_InitStr(&tasm->OutputName, argv[1]);
	return !TaghaAsm_Assemble(tasm);
}