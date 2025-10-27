/*--------------------------------------------------------------------*/
/* symtablehash.c                                                     */
/* Author: Mohemeen Ahmed (modeled on stack6.c style)                 */
/*--------------------------------------------------------------------*/

#include "symtable.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/*--------------------------------------------------------------------*/
/* Expansion bucket sizes, required by spec.                          */

static const size_t auBucketCounts[] = {
   509, 1021, 2039, 4093, 8191, 16381, 32749, 65521
};

enum { BUCKET_COUNTS_LEN = 8 };

/*--------------------------------------------------------------------*/
/* Each binding is stored in a Binding. Bindings in the same bucket   */
/* are linked to form a chain.                                        */

struct Binding
{
   /* The key string. SymTable owns this memory. */
   char *pcKey;

   /* The value associated with pcKey.
      SymTable does NOT own this memory. */
   const void *pvValue;

   /* The address of the next Binding in this bucket's chain. */
   struct Binding *psNextBinding;
};

/*--------------------------------------------------------------------*/
/* A SymTable is a hash table with separate chaining.                 */

struct SymTable
{
   /* Array of bucket heads. ppsBuckets[i] is a linked list of
      Bindings, or NULL if bucket i is empty. */
   struct Binding **ppsBuckets;

   /* Number of buckets currently in use. */
   size_t uBucketCount;

   /* Total number of bindings stored. */
   size_t uLength;

   /* Index into auBucketCounts[] that corresponds to uBucketCount. */
   size_t uBucketIndex;
};

/*--------------------------------------------------------------------*/
/* Return a hash code for pcKey in the range [0, uBucketCount-1].     */

static size_t SymTable_hash(const char *pcKey, size_t uBucketCount)
{
   const size_t HASH_MULTIPLIER = 65599;
   size_t u;
   size_t uHash = 0;

   assert(pcKey != NULL);

   for (u = 0; pcKey[u] != '\0'; u++)
      uHash = uHash * HASH_MULTIPLIER + (size_t)pcKey[u];

   return uHash % uBucketCount;
}

/*--------------------------------------------------------------------*/
/* Allocate an array of uBucketCount buckets, initialize them to NULL */
/* and return it, or return NULL on failure.                          */

static struct Binding **SymTable_allocateBuckets(size_t uBucketCount)
{
   struct Binding **ppsBuckets;
   size_t u;

   ppsBuckets = (struct Binding**)malloc(uBucketCount *
                                         sizeof(struct Binding*));
   if (ppsBuckets == NULL)
      return NULL;

   for (u = 0; u < uBucketCount; u++)
      ppsBuckets[u] = NULL;

   return ppsBuckets;
}

/*--------------------------------------------------------------------*/
/* Return a new, empty SymTable or NULL if not enough memory.         */

SymTable_T SymTable_new(void)
{
   SymTable_T oSymTable;

   oSymTable = (SymTable_T)malloc(sizeof(struct SymTable));
   if (oSymTable == NULL)
      return NULL;

   oSymTable->uBucketIndex = 0U;
   oSymTable->uBucketCount = auBucketCounts[oSymTable->uBucketIndex];

   oSymTable->ppsBuckets =
      SymTable_allocateBuckets(oSymTable->uBucketCount);
   if (oSymTable->ppsBuckets == NULL)
   {
      free(oSymTable);
      return NULL;
   }

   oSymTable->uLength = 0U;
   return oSymTable;
}

/*--------------------------------------------------------------------*/
/* Free all memory owned by oSymTable: the copied key strings, the    */
/* Binding structs, the bucket array, and then the SymTable itself.   */
/* Never free pvValue.                                                */

void SymTable_free(SymTable_T oSymTable)
{
   size_t u;
   struct Binding *psCurrent;
   struct Binding *psNext;

   assert(oSymTable != NULL);

   for (u = 0; u < oSymTable->uBucketCount; u++)
   {
      for (psCurrent = oSymTable->ppsBuckets[u];
           psCurrent != NULL;
           psCurrent = psNext)
      {
         psNext = psCurrent->psNextBinding;

         free(psCurrent->pcKey);
         /* Do not free psCurrent->pvValue. */

         free(psCurrent);
      }
   }

   free(oSymTable->ppsBuckets);
   free(oSymTable);
}

/*--------------------------------------------------------------------*/
/* Return number of bindings. Runs in O(1).                           */

size_t SymTable_getLength(SymTable_T oSymTable)
{
   assert(oSymTable != NULL);

   return oSymTable->uLength;
}

/*--------------------------------------------------------------------*/
/* Helper: Return Binding for pcKey in oSymTable, or NULL if missing. */
/* Only searches the correct bucket.                                  */

static struct Binding *SymTable_findBinding(SymTable_T oSymTable,
                                            const char *pcKey)
{
   size_t uIndex;
   struct Binding *psCurrent;

   assert(oSymTable != NULL);
   assert(pcKey != NULL);

   uIndex = SymTable_hash(pcKey, oSymTable->uBucketCount);

   for (psCurrent = oSymTable->ppsBuckets[uIndex];
        psCurrent != NULL;
        psCurrent = psCurrent->psNextBinding)
   {
      if (strcmp(psCurrent->pcKey, pcKey) == 0)
         return psCurrent;
   }

   return NULL;
}

/*--------------------------------------------------------------------*/
/* Helper: expand the table if uLength > uBucketCount and we're not   */
/* already at the max bucket size. Rehash all bindings into a new     */
/* bucket array with the next size.                                   */
/* If allocation fails, silently skip expansion (allowed by spec).    */

static void SymTable_expand(SymTable_T oSymTable)
{
   size_t uNewBucketIndex;
   size_t uNewBucketCount;
   struct Binding **ppsNewBuckets;
   size_t u;
   struct Binding *psCurrent;
   struct Binding *psNext;
   size_t uIndex;

   assert(oSymTable != NULL);

   /* Are we already at the last bucket size? */
   if (oSymTable->uBucketIndex + 1U >= (size_t)BUCKET_COUNTS_LEN)
      return;

   /* Do we actually need to expand? (length just exceeded old size) */
   if (oSymTable->uLength <= oSymTable->uBucketCount)
      return;

   uNewBucketIndex = oSymTable->uBucketIndex + 1U;
   uNewBucketCount = auBucketCounts[uNewBucketIndex];

   /* Try to allocate new buckets. If this fails, bail. */
   ppsNewBuckets = SymTable_allocateBuckets(uNewBucketCount);
   if (ppsNewBuckets == NULL)
      return;

   /* Rehash every binding into the new bucket array. */
   for (u = 0; u < oSymTable->uBucketCount; u++)
   {
      for (psCurrent = oSymTable->ppsBuckets[u];
           psCurrent != NULL;
           psCurrent = psNext)
      {
         psNext = psCurrent->psNextBinding;

         uIndex = SymTable_hash(psCurrent->pcKey, uNewBucketCount);

         psCurrent->psNextBinding = ppsNewBuckets[uIndex];
         ppsNewBuckets[uIndex] = psCurrent;
      }
   }

   /* Free old bucket array (not the bindings themselves). */
   free(oSymTable->ppsBuckets);

   /* Install the new table properties. */
   oSymTable->ppsBuckets = ppsNewBuckets;
   oSymTable->uBucketCount = uNewBucketCount;
   oSymTable->uBucketIndex = uNewBucketIndex;
}

/*--------------------------------------------------------------------*/
/* Insert (pcKey -> pvValue) if pcKey not already present.            */
/* Make a defensive copy of pcKey.                                   */
/* Insert into the appropriate bucket chain (front of list).         */
/* Increment length.                                                 */
/* Possibly expand.                                                  */
/* Return 1 on success, 0 if key exists or memory error.             */

int SymTable_put(SymTable_T oSymTable,
                 const char *pcKey, const void *pvValue)
{
   struct Binding *psNewBinding;
   char *pcKeyCopy;
   size_t uIndex;

   assert(oSymTable != NULL);
   assert(pcKey != NULL);

   /* If key is already present, do nothing. */
   if (SymTable_findBinding(oSymTable, pcKey) != NULL)
      return 0;

   /* Allocate new binding. */
   psNewBinding = (struct Binding*)malloc(sizeof(struct Binding));
   if (psNewBinding == NULL)
      return 0;

   /* Copy the key string. We own this memory now. */
   pcKeyCopy = (char*)malloc(strlen(pcKey) + 1U);
   if (pcKeyCopy == NULL)
   {
      free(psNewBinding);
      return 0;
   }
   strcpy(pcKeyCopy, pcKey);

   psNewBinding->pcKey = pcKeyCopy;
   psNewBinding->pvValue = pvValue;

   /* Insert in proper bucket chain (front insert). */
   uIndex = SymTable_hash(pcKey, oSymTable->uBucketCount);
   psNewBinding->psNextBinding = oSymTable->ppsBuckets[uIndex];
   oSymTable->ppsBuckets[uIndex] = psNewBinding;

   /* Update length. */
   oSymTable->uLength++;

   /* Try to expand if needed. If expansion malloc fails, that's fine. */
   if (oSymTable->uLength > oSymTable->uBucketCount)
      SymTable_expand(oSymTable);

   return 1;
}

/*--------------------------------------------------------------------*/
/* If pcKey exists, replace its stored value with pvValue and return  */
/* the old value. Otherwise return NULL.                              */

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
/* Return 1 if pcKey exists in oSymTable, 0 otherwise.                */

int SymTable_contains(SymTable_T oSymTable, const char *pcKey)
{
   assert(oSymTable != NULL);
   assert(pcKey != NULL);

   return (SymTable_findBinding(oSymTable, pcKey) != NULL);
}

/*--------------------------------------------------------------------*/
/* Return the value associated with pcKey, or NULL if not found.      */

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
/* If pcKey exists, remove that binding from its bucket chain, free   */
/* the key and the Binding struct, decrement uLength, and return the  */
/* value pointer. Otherwise return NULL.                              */
/* We never free the value pointer itself.                            */

void *SymTable_remove(SymTable_T oSymTable, const char *pcKey)
{
   size_t uIndex;
   struct Binding *psCurrent;
   struct Binding *psPrev;
   const void *pvValue;

   assert(oSymTable != NULL);
   assert(pcKey != NULL);

   uIndex = SymTable_hash(pcKey, oSymTable->uBucketCount);

   psPrev = NULL;
   psCurrent = oSymTable->ppsBuckets[uIndex];

   while (psCurrent != NULL)
   {
      if (strcmp(psCurrent->pcKey, pcKey) == 0)
      {
         pvValue = psCurrent->pvValue;

         /* Unlink from list. */
         if (psPrev == NULL)
            oSymTable->ppsBuckets[uIndex] =
               psCurrent->psNextBinding;
         else
            psPrev->psNextBinding =
               psCurrent->psNextBinding;

         free(psCurrent->pcKey);
         free(psCurrent);

         assert(oSymTable->uLength > 0U);
         oSymTable->uLength--;

         return (void*)pvValue;
      }

      psPrev = psCurrent;
      psCurrent = psCurrent->psNextBinding;
   }

   return NULL;
}

/*--------------------------------------------------------------------*/
/* Call pfApply(pcKey, pvValue, pvExtra) for each binding in the      */
/* table.                                                             */

void SymTable_map(SymTable_T oSymTable,
                  void (*pfApply)(const char *pcKey,
                                  void *pvValue,
                                  void *pvExtra),
                  const void *pvExtra)
{
   size_t u;
   struct Binding *psCurrent;

   assert(oSymTable != NULL);
   assert(pfApply != NULL);

   for (u = 0; u < oSymTable->uBucketCount; u++)
   {
      for (psCurrent = oSymTable->ppsBuckets[u];
           psCurrent != NULL;
           psCurrent = psCurrent->psNextBinding)
      {
         (*pfApply)(psCurrent->pcKey,
                    (void*)psCurrent->pvValue,
                    (void*)pvExtra);
      }
   }
}

/*--------------------------------------------------------------------*/
