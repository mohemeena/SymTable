/*--------------------------------------------------------------------*/
/* symtablehash.c                                                     */
/*--------------------------------------------------------------------*/

#include "symtable.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* list of bucket sizes used when expanding the hash table */
static const size_t auBucketCounts[] = {
   509, 1021, 2039, 4093, 8191, 16381, 32749, 65521
};

/* number of bucket sizes in auBucketCounts */
enum { BUCKET_COUNTS_LEN = 8 };

/*--------------------------------------------------------------------*/
/* Each binding is stored in a Binding. Bindings in the same bucket   */
/* are linked to form a chain.                                        */

struct Binding
{
   /* The key string. */
   char *pcKey;

   /* The value associated with the key. */
   const void *pvValue;

   /* The address of the next Binding in this bucket's chain. */
   struct Binding *psNextBinding;
};

/*--------------------------------------------------------------------*/
/* A SymTable is a hash table with separate chaining.                 */

struct SymTable
{
   /* array of bucket heads. ppsBuckets[i] is a linked list of
      Bindings, or it is NULL if bucket i is empty. */
   struct Binding **ppsBuckets;

   /* number of buckets in use. */
   size_t uBucketCount;

   /* total number of bindings stored. */
   size_t uLength;

   /* index into auBucketCounts[] that corresponds to uBucketCount. */
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
/* allocating an array of uBucketCount buckets, initialize them to NULL */
/* and return it, or return NULL if it doesn't work.                    */

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

SymTable_T SymTable_new(void)
{
   SymTable_T oSymTable;

   oSymTable = (SymTable_T)malloc(sizeof(struct SymTable));
   if (oSymTable == NULL)
      return NULL;

    /* initialize bucket index and count */
   oSymTable->uBucketIndex = 0U;
   oSymTable->uBucketCount = auBucketCounts[oSymTable->uBucketIndex];

   /* allocate and initialize the bucket array */
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
        /* store next binding before freeing current */
         psNext = psCurrent->psNextBinding;
        /* free the key string and the binding */
         free(psCurrent->pcKey);
         free(psCurrent);
      }
   }
/* free the bucket array and the symbol table itself */
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
/* challenge helper: expand oSymTable if uLength > uBucketCount & not */
/* already at the max bucket size. rehash all bindings into a new     */
/* bucket array with the next size.                                   */
/* if allocation fails, skip expansion                                */

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

   /* if already at max bucket size, return */
   if (oSymTable->uBucketIndex >= (size_t)(BUCKET_COUNTS_LEN - 1))
    return;

    /* if length is not greater than bucket count, return */
   if (oSymTable->uLength <= oSymTable->uBucketCount)
      return;

     /* allocate new bucket array with next size */
   uNewBucketIndex = oSymTable->uBucketIndex + 1U;
   uNewBucketCount = auBucketCounts[uNewBucketIndex];

   /* allocate new bucket array */
   ppsNewBuckets = SymTable_allocateBuckets(uNewBucketCount);
   if (ppsNewBuckets == NULL)
      return;

     /* rehash all existing bindings into new bucket array */
   for (u = 0; u < oSymTable->uBucketCount; u++)
   {
      for (psCurrent = oSymTable->ppsBuckets[u];
           psCurrent != NULL;
           psCurrent = psNext)
      {
        /* store next binding before rehashing current */
         psNext = psCurrent->psNextBinding;

         /* rehash the current binding into the new bucket array */
         uIndex = SymTable_hash(psCurrent->pcKey, uNewBucketCount);

         psCurrent->psNextBinding = ppsNewBuckets[uIndex];
         ppsNewBuckets[uIndex] = psCurrent;
      }
   }

   free(oSymTable->ppsBuckets);

   /* update oSymTable to use the new bucket array and size */
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

    /* check if key already exists */
    uIndex = SymTable_hash(pcKey, oSymTable->uBucketCount);

    for (psCurrent = oSymTable->ppsBuckets[uIndex];
         psCurrent != NULL;
         psCurrent = psCurrent->psNextBinding)
    {
        /* if we find the key */
        if (strcmp(psCurrent->pcKey, pcKey) == 0)
            return 0;
    }

    /* create new binding */
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

    /* initialize new binding */
    psNewBinding->pcKey = pcKeyCopy;
    psNewBinding->pvValue = pvValue;

    /* insert new binding at the front of the correct bucket's chain */
    psNewBinding->psNextBinding = oSymTable->ppsBuckets[uIndex];
    oSymTable->ppsBuckets[uIndex] = psNewBinding;

    oSymTable->uLength++;

    /* expand the hash table if necessary - challenge part*/
    if (oSymTable->uLength > oSymTable->uBucketCount)
    SymTable_expand(oSymTable);

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
            /* if we find the key that we want to replace */
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
 
         /* remove psCurrent from the chain */
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
        /* call pfApply for each binding */
         (*pfApply)(psCurrent->pcKey,
                    (void*)psCurrent->pvValue,
                    (void*)pvExtra);
      }
   }
}

/*--------------------------------------------------------------------*/
