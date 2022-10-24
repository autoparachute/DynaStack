#include <assert.h>
#include <inttypes.h>
#include <sys/types.h>
#include <stddef.h>
#include <string.h>

#include "dr_api.h"
#include "drmgr.h"
#include "drsyms.h"
#include "drvector.h"

/* Definition of get_sym() */

#define printf dr_printf
int tabs = 0;
#define indent (++tabs)
#define unindent ({ if (tabs > 0) --tabs; })
#define tdebug(...) ({ \
    for (int i = 0; i < tabs; ++i) dr_printf("\t"); \
    dr_printf(__VA_ARGS__);})
static char sym_name_buf[256];

static const char *get_sym(app_pc addr)
{
    module_data_t *data = dr_lookup_module(addr);	/*If a module containing pc is found returns a module_data_t describing that module*/
    if (data != NULL)
    {
        char file[MAXIMUM_PATH];	/* #define MAXIMUM_PATH 260 */
        drsym_info_t sym;
        sym.struct_size = sizeof(sym);	/* Input should be set by caller to sizeof(drsym_info_t) */
        sym.name = sym_name_buf;	
        sym.name_size = 256;
        sym.file = file;
        sym.file_size = MAXIMUM_PATH;
        drsym_error_t res = drsym_lookup_address(data->full_path, addr - data->start, &sym, DRSYM_DEFAULT_FLAGS);	/* Retrieves symbol information for a given module offset */

        dr_free_module_data(data);	/* Free a module_data_t returned by dr_lookup_module() */
        if (res == DRSYM_SUCCESS || res == DRSYM_ERROR_LINE_NOT_AVAILABLE)
            return sym_name_buf;	/* Return the instruction */
    }
    return NULL;
}

int tls_index=-1;

/*********************** stack (Definition of PUSH and POP) ***********************/
void push(void *addr)
{
    void *drcontext = dr_get_current_drcontext();
    drvector_t *vec = drmgr_get_tls_field(drcontext, tls_index);		/* Returns the user-controlled thread-local-storage field for the given index*/
    drvector_append(vec, addr);		/* Adds a new entry to the end of the vector */
}

void *pop()
{
    void *drcontext = dr_get_current_drcontext();
    drvector_t *vec = drmgr_get_tls_field(drcontext, tls_index);		/* Returns the user-controlled thread-local-storage field for the given index*/
    assert(vec->entries > 0);
    vec->entries--;		
    return vec->array[vec->entries];	/*Returns the lastest pushed address */
}

/*********************** main **********************/
static void event_exit(void);
static dr_emit_flags_t event_basic_block(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst, bool for_trace, bool translating, void *user_data);
static void event_thread_init(void *drcontext);
static void event_thread_exit(void *drcontext);

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    printf("Starting shadow stack\n");

    /* Initialize drmgr && drsym */
    /* Initializes the drmgr extension. Must be called prior to any of the other routines. Each call must be paired with a corresponding call to drmgr_exit() */
    drmgr_init();
    /* Initialize the symbol access library. Each call must be paired with a corresponding call to drsym_exit() */ 
    drsym_init(0);

    /* Reserves a thread-local storage (tls) slot for every thread */
    tls_index=drmgr_register_tls_field();

    /*register events */
    dr_register_exit_event(event_exit);

    /* Registers callback functions for the second and third instrumentation stage:application analysis and instrumentation insertion. drmgr will call function as the second of five instrumentation stages for each dynamic application basic block. */
    drmgr_register_bb_instrumentation_event(NULL, &event_basic_block, NULL);

    drmgr_register_thread_init_event(&event_thread_init);
    drmgr_register_thread_exit_event(&event_thread_exit);
}

static void event_exit(void)
{
    printf("Stopping shadow stack\n");    

    /* Cleans up the drmgr extension */
    drmgr_exit();
    /* Cleans up the drsym extension */
    drsym_exit();
}

/******************* instrumentation *****************/
void on_call(void *call_ins, void *target_addr)
{
    const char* str1 = (char*)malloc(30*sizeof(char));
    str1 = get_sym(call_ins);
    const char* str2 = (char*)malloc(30*sizeof(char));
    str2 = "__GI_stpcpy";
    const char* str5 = (char*)malloc(30*sizeof(char));
    str5 = "__GI_strcpy";
    if(str1==NULL)
    {
        push(call_ins);  /*push the call_ins onto the stack */
        tdebug("%p: call %p <%s>\n", call_ins, target_addr, get_sym(target_addr));  /* printf the call_ins address,target address,and the call_ins instruction */
        indent;         /* tab++ */
    }
    else if(strcmp(str1,str2) == 0)     /*ignore the stpcpy function*/
    {
        tdebug("ignore <%s> function\n", get_sym(call_ins));
    }
    else if(strcmp(str1,str5) == 0)     /*ignore the strcpy function*/
    {
        tdebug("ignore <%s> function\n", get_sym(call_ins));
    }
    else 
    {
        push(call_ins);  /*push the call_ins onto the stack */
        tdebug("%p: call %p <%s>\n", call_ins, target_addr, get_sym(target_addr));  /* printf the call_ins address,target address,and the call_ins instruction */
        indent;         /* tab++ */
    }
}

void on_ret(void *ret_ins, void *target_addr)
{
    const char* str3 =(char*)malloc(30*sizeof(char));
    str3 = get_sym(ret_ins);
    const char* str4 =(char*)malloc(30*sizeof(char));
    str4 = "_dl_runtime_resolve";
    if(str3==NULL)
    {
        unindent;       /* tab--1 */
        ptrdiff_t diff;
        diff = target_addr - pop();
        if (!(0<=diff && diff<=8))
        {
            tdebug("returning to %p, the ptrdiff is %p ,buffer overflow occur, please check your input!\n",target_addr ,diff); 
            dr_abort();
        }else
        {
            tdebug("This is the return instr in <%s> , returning to %p\n" , get_sym(ret_ins), target_addr);  /*printf the return address */
        }
    }
    else if(strcmp(str3,str4) == 0)     /*Delay binding occur , ignore it*/
    {
        tdebug("Delay binding: skipping this ret instruction\n");
    }
    else 
    {
        unindent;       /* tab--1 */
        ptrdiff_t diff;
        diff = target_addr - pop();
        if (!(0<=diff && diff<=8))
        {
            tdebug("returning to %p, the ptrdiff is %p ,buffer overflow occur, please check your input!\n",target_addr ,diff); 
            dr_abort();
        }else
        {
            tdebug("This is the return instr in <%s> , returning to %p\n" , get_sym(ret_ins), target_addr);  /*printf the return address */
        }
    }
}

/******************** analysis **********************/
static dr_emit_flags_t event_basic_block(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr, bool for_trace, bool translating, void *user_data)
{
    /* In units of basic blocks, sequences of instructions ending with a single control transfer instruction
	   We only need to check whether the last instruction in the basic block's instruction sequence is a (direct or indirect)call instruction or a return instruction*/

    if (instr_is_cti(instr)) {

	/* Assumes that inst is a near call. Inserts into bb prior to inst instruction to call on_call function and passing two arguments: 1.address of call instruction 2.target address of call */
        if (instr_is_call_direct(instr)) {
            dr_insert_call_instrumentation(drcontext, bb, instr, on_call);
	}
	/* Assumes that inst is an indirect branch. Inserts into bb prior to inst instruction to call on_call function and passing two arguments: 1.address of branch instruction 2.target address of branch */	
        else if (instr_is_call_indirect(instr)) {
            dr_insert_mbr_instrumentation(drcontext, bb, instr, on_call, SPILL_SLOT_1);
	}
	/* Assumes that inst is an indirect branch. Inserts into bb prior to inst instruction to call on_ret function and passing two arguments: 1.address of branch instruction 2.target address of branch */
         else if (instr_is_return(instr)) {
            dr_insert_mbr_instrumentation(drcontext, bb, instr, on_ret, SPILL_SLOT_2);
	}
    }

    return DR_EMIT_DEFAULT;
}

/******************* threads ************************/
static void event_thread_init(void *drcontext)
{
    drvector_t *vec = dr_thread_alloc(drcontext, sizeof(*vec));	/* Allocates sizeof(*vec) bytes of memory from DR's memory pool specific to the thread associated with drcontext */
    assert(vec != NULL);

    /* Initializes a drvector with the given parameters: 1:The vector to be initialized. 2:The initial number of entries allocated for the vector. 3:whether to synchronize each operation. 4:Leave it NULL if no callback is needed */
    drvector_init(vec, 10, false, NULL);			
    drmgr_set_tls_field(drcontext, tls_index, vec);		/*Sets the user-controlled thread-local-storage field for the given index, relate vec to tls_id*/
}

static void event_thread_exit(void *drcontext)
{
    drvector_t *vec = drmgr_get_tls_field(drcontext, tls_index);	/* Returns the user-controlled thread-local-storage field for the given index*/
    drvector_delete(vec);					            /* Destroys all storage for the vector */
    dr_thread_free(drcontext, vec, sizeof(*vec));		/* Frees thread-specific memory allocated by dr_thread_alloc() */
}
