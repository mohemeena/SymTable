/*--------------------------------------------------------------------*/
/* symtable.h                                                         */
/* Author: Mohemeen Ahmed                                             */
/*--------------------------------------------------------------------*/

#ifndef SYMTABLE_INCLUDED
#define SYMTABLE_INCLUDED

#include <stddef.h>

/* A SymTable_T is a pointer to a SymTable object.
   Each SymTable object is a collection of unique string keys
   mapped to void pointer values. */
typedef struct SymTable *SymTable_T;

/* Return a new, empty SymTable, or NULL if out of memory. */
SymTable_T SymTable_new(void);

/* Free oSymTable. */
void SymTable_free(SymTable_T oSymTable);

/* Return the number of bindings in oSymTable. */
size_t SymTable_getLength(SymTable_T oSymTable);

/* Insert pcKey -> pvValue in oSymTable if pcKey isn't already present.
   Creates a defensive copy of pcKey.
   Return 1 if it works, 0 if it already exists). */
int SymTable_put(SymTable_T oSymTable,
                 const char *pcKey, const void *pvValue);

/* If pcKey exists in oSymTable, replace its value with pvValue.
   Return the old value. Otherwise return NULL and do nothing. */
void *SymTable_replace(SymTable_T oSymTable,
                       const char *pcKey, const void *pvValue);

/* Return 1 if pcKey exists in oSymTable, 0 otherwise. */
int SymTable_contains(SymTable_T oSymTable, const char *pcKey);

/* Return value for pcKey in oSymRable, or NULL if not in symbol table. */
void *SymTable_get(SymTable_T oSymTable, const char *pcKey);

/* If pcKey exists in oSymTable, remove that binding and return its value.
   Otherwise return NULL and don't do anything. */
void *SymTable_remove(SymTable_T oSymTable, const char *pcKey);

/* For each binding in oSymTable, call
   pfApply(pcKey, pvValue, pvExtra). */
void SymTable_map(SymTable_T oSymTable,
                  void (*pfApply)(const char *pcKey,
                                  void *pvValue,
                                  void *pvExtra),
                  const void *pvExtra);

#endif
