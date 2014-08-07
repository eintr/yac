#ifndef YAC_ATOMIC_H
#define YAC_ATOMIC_H

#ifdef __GNUC__
#define	YAC_CAS(PTR, OLD, NEW)  __sync_bool_compare_and_swap(PTR, OLD, NEW)
#define	YAC_INC(PTR)			do{(void)__sync_add_and_fetch(PTR, 1)}while(0)
#define	YAC_DEC(PTR)			do{(void)__sync_sub_and_fetch(PTR, 1)}while(0)
#else
#error This compiler is NOT supported yet!
#endif

#endif

