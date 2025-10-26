#ifndef SYMTABLE_INCLUDED
#define SYMTABLE_INCLUDED

#include <stddef.h>  /* for size_t */

/* A SymTable_T is an opaque pointer to a SymTable object.
   Each SymTable object is a collection of unique string keys
   mapped to void* values. */
typedef struct SymTable *SymTable_T;

/* Return a new, empty SymTable, or NULL if out of memory. */
SymTable_T SymTable_new(void);

/* Free all memory owned by oSymTable. */
void SymTable_free(SymTable_T oSymTable);

/* Return number of bindings in oSymTable. */
size_t SymTable_getLength(SymTable_T oSymTable);

/* Insert (pcKey -> pvValue) if pcKey not already present.
   Make a defensive copy of pcKey.
   Return 1 on success, 0 otherwise (already exists or OOM). */
int SymTable_put(SymTable_T oSymTable,
                 const char *pcKey, const void *pvValue);

/* If pcKey exists, replace its value with pvValue.
   Return old value. Else return NULL and do nothing. */
void *SymTable_replace(SymTable_T oSymTable,
                       const char *pcKey, const void *pvValue);

/* Return 1 if pcKey exists in oSymTable, 0 otherwise. */
int SymTable_contains(SymTable_T oSymTable, const char *pcKey);

/* Return value for pcKey, or NULL if not present. */
void *SymTable_get(SymTable_T oSymTable, const char *pcKey);

/* If pcKey exists, remove that binding and return its value.
   Otherwise return NULL and do nothing. */
void *SymTable_remove(SymTable_T oSymTable, const char *pcKey);

/* For each binding, call
   pfApply(pcKey, pvValue, pvExtra). */
void SymTable_map(SymTable_T oSymTable,
                  void (*pfApply)(const char *pcKey,
                                  void *pvValue,
                                  void *pvExtra),
                  const void *pvExtra);

#endif
