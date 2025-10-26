/*--------------------------------------------------------------------*/
/* symtablelist.c                                                     */
/* Author: Mohemeen Ahmed (modeled on stack6.c style)                 */
/*--------------------------------------------------------------------*/

#include "symtable.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/*--------------------------------------------------------------------*/

/* Each binding is stored in a Binding.  Bindings are linked to form
   a list. */

struct Binding
{
   /* The key string.  The SymTable owns this memory. */
   char *pcKey;

   /* The value associated with the key.  The SymTable does NOT own
      this memory. */
   const void *pvValue;

   /* The address of the next Binding. */
   struct Binding *psNextBinding;
};

/*--------------------------------------------------------------------*/

/* A SymTable is a "dummy" node that points to the first Binding, and
   also stores the number of bindings. */

struct SymTable
{
   /* The address of the first Binding. */
   struct Binding *psFirstBinding;

   /* The number of bindings in the SymTable. */
   size_t uLength;
};

/*--------------------------------------------------------------------*/

/* Return a new, empty SymTable, or NULL if insufficient memory. */

SymTable_T SymTable_new(void)
{
   SymTable_T oSymTable;

   oSymTable = (SymTable_T)malloc(sizeof(struct SymTable));
   if (oSymTable == NULL)
      return NULL;

   oSymTable->psFirstBinding = NULL;
   oSymTable->uLength = 0U;

   return oSymTable;
}

/*--------------------------------------------------------------------*/

/* Free all memory occupied by oSymTable, including all bindings and
   all key strings.  Do not free any values. */

void SymTable_free(SymTable_T oSymTable)
{
   struct Binding *psCurrentBinding;
   struct Binding *psNextBinding;

   assert(oSymTable != NULL);

   for (psCurrentBinding = oSymTable->psFirstBinding;
        psCurrentBinding != NULL;
        psCurrentBinding = psNextBinding)
   {
      psNextBinding = psCurrentBinding->psNextBinding;

      /* Free the key string (SymTable owns it). */
      free(psCurrentBinding->pcKey);

      /* Do not free pvValue.  The client owns that memory. */

      /* Free the binding struct. */
      free(psCurrentBinding);
   }

   free(oSymTable);
}

/*--------------------------------------------------------------------*/

/* Return the number of bindings in oSymTable. */

size_t SymTable_getLength(SymTable_T oSymTable)
{
   assert(oSymTable != NULL);

   return oSymTable->uLength;
}

/*--------------------------------------------------------------------*/

/* Helper function.
   Return the Binding whose key is pcKey, or NULL if not found. */

static struct Binding *SymTable_findBinding(SymTable_T oSymTable,
                                            const char *pcKey)
{
   struct Binding *psCurrentBinding;

   assert(oSymTable != NULL);
   assert(pcKey != NULL);

   for (psCurrentBinding = oSymTable->psFirstBinding;
        psCurrentBinding != NULL;
        psCurrentBinding = psCurrentBinding->psNextBinding)
   {
      if (strcmp(psCurrentBinding->pcKey, pcKey) == 0)
         return psCurrentBinding;
   }

   return NULL;
}

/*--------------------------------------------------------------------*/

/* If pcKey is not already in oSymTable, then create a new binding
   (pcKey -> pvValue), insert it, and return 1.
   Otherwise leave oSymTable unchanged and return 0.
   Return 0 if insufficient memory. */

int SymTable_put(SymTable_T oSymTable,
                 const char *pcKey, const void *pvValue)
{
   struct Binding *psNewBinding;
   char *pcKeyCopy;

   assert(oSymTable != NULL);
   assert(pcKey != NULL);

   /* Fail if key already exists. */
   if (SymTable_findBinding(oSymTable, pcKey) != NULL)
      return 0;

   /* Allocate new binding node. */
   psNewBinding = (struct Binding*)malloc(sizeof(struct Binding));
   if (psNewBinding == NULL)
      return 0;

   /* Copy the key string so we own it. */
   pcKeyCopy = (char*)malloc(strlen(pcKey) + 1U);
   if (pcKeyCopy == NULL)
   {
      free(psNewBinding);
      return 0;
   }
   strcpy(pcKeyCopy, pcKey);

   /* Initialize binding fields. */
   psNewBinding->pcKey = pcKeyCopy;
   psNewBinding->pvValue = pvValue;

   /* Insert new binding at the front of the list. */
   psNewBinding->psNextBinding = oSymTable->psFirstBinding;
   oSymTable->psFirstBinding = psNewBinding;

   /* Update length. */
   oSymTable->uLength++;

   return 1;
}

/*--------------------------------------------------------------------*/

/* If pcKey exists, replace its value with pvValue and return the old
   value. Otherwise, leave oSymTable unchanged and return NULL. */

void *SymTable_replace(SymTable_T oSymTable,
                       const char *pcKey, const void *pvValue)
{
   struct Binding *psBinding;
   const void *pvOldValue;

   assert(oSymTable != NULL);
   assert(pcKey != NULL);

   psBinding = SymTable_findBinding(oSymTable, pcKey);
   if (psBinding == NULL)
      return NULL;

   pvOldValue = psBinding->pvValue;
   psBinding->pvValue = pvValue;
   return (void*)pvOldValue;
}

/*--------------------------------------------------------------------*/

/* Return 1 if oSymTable contains pcKey, 0 otherwise. */

int SymTable_contains(SymTable_T oSymTable, const char *pcKey)
{
   assert(oSymTable != NULL);
   assert(pcKey != NULL);

   return (SymTable_findBinding(oSymTable, pcKey) != NULL);
}

/*--------------------------------------------------------------------*/

/* Return the value associated with pcKey, or NULL if no such key
   exists in oSymTable. */

void *SymTable_get(SymTable_T oSymTable, const char *pcKey)
{
   struct Binding *psBinding;

   assert(oSymTable != NULL);
   assert(pcKey != NULL);

   psBinding = SymTable_findBinding(oSymTable, pcKey);
   if (psBinding == NULL)
      return NULL;

   return (void*)psBinding->pvValue;
}

/*--------------------------------------------------------------------*/

/* If pcKey exists in oSymTable, then remove that binding from the
   list, free its key string and its struct, decrement length,
   and return the binding's value pointer.
   Otherwise return NULL. */

void *SymTable_remove(SymTable_T oSymTable, const char *pcKey)
{
   struct Binding *psCurrentBinding;
   struct Binding *psPrevBinding;
   const void *pvValue;

   assert(oSymTable != NULL);
   assert(pcKey != NULL);

   psPrevBinding = NULL;
   psCurrentBinding = oSymTable->psFirstBinding;

   while (psCurrentBinding != NULL)
   {
      if (strcmp(psCurrentBinding->pcKey, pcKey) == 0)
      {
         /* Save value to return. */
         pvValue = psCurrentBinding->pvValue;

         /* Unlink from list. */
         if (psPrevBinding == NULL)
            oSymTable->psFirstBinding
               = psCurrentBinding->psNextBinding;
         else
            psPrevBinding->psNextBinding
               = psCurrentBinding->psNextBinding;

         /* Free owned memory. */
         free(psCurrentBinding->pcKey);
         free(psCurrentBinding);

         /* Update length. */
         assert(oSymTable->uLength > 0U);
         oSymTable->uLength--;

         return (void*)pvValue;
      }

      psPrevBinding = psCurrentBinding;
      psCurrentBinding = psCurrentBinding->psNextBinding;
   }

   return NULL;
}

/*--------------------------------------------------------------------*/

/* Apply function pfApply to each binding in oSymTable.
   For each binding, call
      pfApply(pcKey, pvValue, pvExtra).
   pvExtra is the extra argument supplied by the caller. */

void SymTable_map(SymTable_T oSymTable,
                  void (*pfApply)(const char *pcKey,
                                  void *pvValue,
                                  void *pvExtra),
                  const void *pvExtra)
{
   struct Binding *psCurrentBinding;

   assert(oSymTable != NULL);
   assert(pfApply != NULL);

   for (psCurrentBinding = oSymTable->psFirstBinding;
        psCurrentBinding != NULL;
        psCurrentBinding = psCurrentBinding->psNextBinding)
   {
      (*pfApply)(psCurrentBinding->pcKey,
                 (void*)psCurrentBinding->pvValue,
                 (void*)pvExtra);
   }
}

/*--------------------------------------------------------------------*/
