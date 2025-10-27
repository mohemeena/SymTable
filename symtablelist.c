/*--------------------------------------------------------------------*/
/* symtablelist.c                                                     */
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
   /* The key string. */
   char *pcKey;

   /* The value associated with the key. */
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

   /* The number of bindings in SymTable. */
   size_t uLength;
};

/*--------------------------------------------------------------------*/

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

      /* freeing the key string */
      free(psCurrentBinding->pcKey);

      /* freeing the binding struct */
      free(psCurrentBinding);
   }

   free(oSymTable);
}

/*--------------------------------------------------------------------*/

size_t SymTable_getLength(SymTable_T oSymTable)
{
   assert(oSymTable != NULL);

   return oSymTable->uLength;
}

/*--------------------------------------------------------------------*/

int SymTable_put(SymTable_T oSymTable,
                 const char *pcKey, const void *pvValue)
{
   struct Binding *psNewBinding;
   struct Binding *psCurrent;
   char *pcKeyCopy;

   assert(oSymTable != NULL);
   assert(pcKey != NULL);

   /* return 0 if key already exists */
   for (psCurrent = oSymTable->psFirstBinding;
         psCurrent != NULL;
         psCurrent = psCurrent->psNextBinding)
    {
        if (strcmp(psCurrent->pcKey, pcKey) == 0)
            return 0;
    }

   /* allocating memory for the new binding node */
   psNewBinding = (struct Binding*)malloc(sizeof(struct Binding));
   if (psNewBinding == NULL)
      return 0;

   pcKeyCopy = (char*)malloc(strlen(pcKey) + 1U);
   if (pcKeyCopy == NULL)
   {
      free(psNewBinding);
      return 0;
   }
   strcpy(pcKeyCopy, pcKey);


   psNewBinding->pcKey = pcKeyCopy;
   psNewBinding->pvValue = pvValue;

   /* insert new binding at the front of the list & update length */
   psNewBinding->psNextBinding = oSymTable->psFirstBinding;
   oSymTable->psFirstBinding = psNewBinding;
   oSymTable->uLength++;

   return 1;
}

/*--------------------------------------------------------------------*/

void *SymTable_replace(SymTable_T oSymTable,
                       const char *pcKey, const void *pvValue)
{
   struct Binding *psCurrent;
    const void *pvOldValue;

    assert(oSymTable != NULL);
    assert(pcKey != NULL);

    for (psCurrent = oSymTable->psFirstBinding;
         psCurrent != NULL;
         psCurrent = psCurrent->psNextBinding)
    {
        if (strcmp(psCurrent->pcKey, pcKey) == 0)
        {
            pvOldValue = psCurrent->pvValue;
            psCurrent->pvValue = pvValue;
            return (void*)pvOldValue;
        }
    }

    return NULL;
}

/*--------------------------------------------------------------------*/

int SymTable_contains(SymTable_T oSymTable, const char *pcKey)
{
   struct Binding *psCurrent;

    assert(oSymTable != NULL);
    assert(pcKey != NULL);

    for (psCurrent = oSymTable->psFirstBinding;
         psCurrent != NULL;
         psCurrent = psCurrent->psNextBinding)
    {
        if (strcmp(psCurrent->pcKey, pcKey) == 0)
            return 1;
    }

    return 0;
}

/*--------------------------------------------------------------------*/

void *SymTable_get(SymTable_T oSymTable, const char *pcKey)
{
   struct Binding *psCurrent;

    assert(oSymTable != NULL);
    assert(pcKey != NULL);

    for (psCurrent = oSymTable->psFirstBinding;
         psCurrent != NULL;
         psCurrent = psCurrent->psNextBinding)
    {
        if (strcmp(psCurrent->pcKey, pcKey) == 0)
            return (void*)psCurrent->pvValue;
    }

    return NULL;
}

/*--------------------------------------------------------------------*/

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
         pvValue = psCurrentBinding->pvValue;

         if (psPrevBinding == NULL)
            oSymTable->psFirstBinding
               = psCurrentBinding->psNextBinding;
         else
            psPrevBinding->psNextBinding
               = psCurrentBinding->psNextBinding;

         free(psCurrentBinding->pcKey);
         free(psCurrentBinding);

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
