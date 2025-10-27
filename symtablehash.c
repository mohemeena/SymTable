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
/* Return a hash code for pcKey that is between 0 and uBucketCount-1,
   inclusive. */

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
         free(psCurrent);
      }
   }

   free(oSymTable->ppsBuckets);
   free(oSymTable);
}

/*--------------------------------------------------------------------*/

size_t SymTable_getLength(SymTable_T oSymTable)
{
   assert(oSymTable != NULL);

   return oSymTable->uLength;
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

int SymTable_put(SymTable_T oSymTable,
                 const char *pcKey, const void *pvValue)
{
   struct Binding *psNewBinding;
    struct Binding *psCurrent;
    char *pcKeyCopy;
    size_t uIndex;

    assert(oSymTable != NULL);
    assert(pcKey != NULL);

    /* Hash to find the correct bucket. */
    uIndex = SymTable_hash(pcKey, oSymTable->uBucketCount);

    /* Return 0 if the key already exists in that bucket. */
    for (psCurrent = oSymTable->ppsBuckets[uIndex];
         psCurrent != NULL;
         psCurrent = psCurrent->psNextBinding)
    {
        if (strcmp(psCurrent->pcKey, pcKey) == 0)
            return 0;
    }

    /* Allocate a new binding node. */
    psNewBinding = (struct Binding*)malloc(sizeof(struct Binding));
    if (psNewBinding == NULL)
        return 0;

    /* Copy the key string so this table owns it. */
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

    /* Insert at the front of this bucket's linked list. */
    psNewBinding->psNextBinding = oSymTable->ppsBuckets[uIndex];
    oSymTable->ppsBuckets[uIndex] = psNewBinding;

    /* Update binding count. */
    oSymTable->uLength++;

    /* No expansion attempted. */
    return 1;
}

/*--------------------------------------------------------------------*/

void *SymTable_replace(SymTable_T oSymTable,
                       const char *pcKey, const void *pvValue)
{
    struct Binding *psCurrent;
    const void *pvOldValue;
    size_t uIndex;

    assert(oSymTable != NULL);
    assert(pcKey != NULL);

    uIndex = SymTable_hash(pcKey, oSymTable->uBucketCount);

    for (psCurrent = oSymTable->ppsBuckets[uIndex];
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
    size_t uIndex;

    assert(oSymTable != NULL);
    assert(pcKey != NULL);

    uIndex = SymTable_hash(pcKey, oSymTable->uBucketCount);

    for (psCurrent = oSymTable->ppsBuckets[uIndex];
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
    size_t uIndex;

    assert(oSymTable != NULL);
    assert(pcKey != NULL);

    uIndex = SymTable_hash(pcKey, oSymTable->uBucketCount);

    for (psCurrent = oSymTable->ppsBuckets[uIndex];
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
