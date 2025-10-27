/*--------------------------------------------------------------------*/
/* symtablehash.c                                                     */
/* Author: Mohemeen Ahmed (modeled on stack6.c style)                 */
/*--------------------------------------------------------------------*/

#include "symtable.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/*--------------------------------------------------------------------*/

/* The required bucket counts for expansion. */
static const size_t auBucketCounts[] = {
   509, 1021, 2039, 4093, 8191, 16381, 32749, 65521
};
enum { BUCKET_COUNTS_LEN = 8 };

/*--------------------------------------------------------------------*/

/* Each binding is stored in a Binding.  Bindings are linked
   to form a list within each bucket. */

struct Binding
{
   /* The key string.  The SymTable owns this memory. */
   char *pcKey;

   /* The value associated with pcKey.
      The SymTable does NOT own this memory. */
   const void *pvValue;

   /* The address of the next Binding in this bucket's chain. */
   struct Binding *psNextBinding;
};

/*--------------------------------------------------------------------*/

/* A SymTable is a hash table with separate chaining. */

struct SymTable
{
   /* The array of buckets. Each bucket is a linked list of Bindings.
      ppsBuckets[i] is the head of that bucket's list, or NULL. */
   struct Binding **ppsBuckets;

   /* The number of buckets currently allocated. */
   size_t uBucketCount;

   /* The number of bindings stored total. */
   size_t uLength;

   /* The index in auBucketCounts[] that matches uBucketCount. */
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

/* Allocate a bucket array of uBucketCount buckets, set them all to
   NULL. Return pointer, or NULL on failure. */
static struct Binding **SymTable_allocateBuckets(size_t uBucketCount)
{
   size_t u;
   struct Binding **ppsBuckets;

   ppsBuckets = (struct Binding**)malloc(uBucketCount *
                                         sizeof(struct Binding*));
   if (ppsBuckets == NULL)
      return NULL;

   for (u = 0; u < uBucketCount; u++)
      ppsBuckets[u] = NULL;

   return ppsBuckets;
}

/*--------------------------------------------------------------------*/

/* Create a new SymTable with the initial bucket count (509), or
   return NULL if insufficient memory. */

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

/* Free all memory owned by oSymTable, including:
   - all bindings
   - all copied keys
   - the bucket array
   Then free oSymTable itself.
   Do NOT free any pvValue. */

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

         /* Free the key (we own it). */
         free(psCurrent->pcKey);

         /* Do not free pvValue. */

         free(psCurrent);
      }
   }

   free(oSymTable->ppsBuckets);
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

/* Helper:
   Return the Binding for pcKey in oSymTable, or NULL if not present.
   Only search the appropriate bucket. */

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

/* Helper for expansion:
   Rehash all existing bindings into a new bucket array with
   uNewBucketCount buckets. On success:
     - Free the old bucket array
     - Install the new one
     - Update bucket count/index
   On failure to allocate new buckets, leave table unchanged. */

static void SymTable_expand(SymTable_T oSymTable)
{
   struct Binding **ppsNewBuckets;
   size_t uNewBucketIndex;
   size_t uNewBucketCount;
   size_t u;
   struct Binding *psCurrent;
   struct Binding *psNext;
   size_t uIndex;

   assert(oSymTable != NULL);

   /* If we're already at the largest bucket count, don't expand. */
   if (oSymTable->uBucketIndex + 1U >= (size_t)BUCKET_COUNTS_LEN)
      return;

   /* Only expand if number of bindings exceeds current bucket count.
      (Caller should have checked this, but we double-check.) */
   if (oSymTable->uLength <= oSymTable->uBucketCount)
      return;

   uNewBucketIndex = oSymTable->uBucketIndex + 1U;
   uNewBucketCount = auBucketCounts[uNewBucketIndex];

   /* Attempt to allocate new bucket array. */
   ppsNewBuckets = SymTable_allocateBuckets(uNewBucketCount);
   if (ppsNewBuckets == NULL)
   {
      /* Spec says: if expansion fails due to memory, just continue. */
      return;
   }

   /* Rehash every binding from old buckets into new buckets. */
   for (u = 0; u < oSymTable->uBucketCount; u++)
   {
      for (psCurrent = oSymTable->ppsBuckets[u];
           psCurrent != NULL;
           psCurrent = psNext)
      {
         psNext = psCurrent->psNextBinding;

         /* Compute new bucket index for this binding's key. */
         uIndex = SymTable_hash(psCurrent->pcKey, uNewBucketCount);

         /* Insert at head of new bucket list. */
         psCurrent->psNextBinding = ppsNewBuckets[uIndex];
         ppsNewBuckets[uIndex] = psCurrent;
      }
   }

   /* Free old bucket array, but NOT the bindings. */
   free(oSymTable->ppsBuckets);

   /* Install new. */
   oSymTable->ppsBuckets = ppsNewBuckets;
   oSymTable->uBucketCount = uNewBucketCount;
   oSymTable->uBucketIndex = uNewBucketIndex;
}

/*--------------------------------------------------------------------*/

/* If pcKey is not already in oSymTable, create a new binding
   (pcKey -> pvValue), insert it into the correct bucket,
   increment length, possibly expand the table, and return 1.
   Otherwise leave oSymTable unchanged and return 0.
   Return 0 if insufficient memory at any point before insertion. */

int SymTable_put(SymTable_T oSymTable,
                 const char *pcKey, const void *pvValue)
{
   struct Binding *psNewBinding;
   char *pcKeyCopy;
   size_t uIndex;

   assert(oSymTable != NULL);
   assert(pcKey != NULL);

   /* If key already exists, do nothing. */
   if (SymTable_findBinding(oSymTable, pcKey) != NULL)
      return 0;

   /* Allocate a new Binding node. */
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

   /* Fill in the new Binding. */
   psNewBinding->pcKey = pcKeyCopy;
   psNewBinding->pvValue = pvValue;

   /* Insert in the right bucket. */
   uIndex = SymTable_hash(pcKey, oSymTable->uBucketCount);
   psNewBinding->psNextBinding = oSymTable->ppsBuckets[uIndex];
   oSymTable->ppsBuckets[uIndex] = psNewBinding;

   /* Update length. */
   oSymTable->uLength++;

   /* Attempt expansion IF length is now "too large". 
      Rule: when a put causes binding count to exceed the current
      bucket count, we should expand to the next bucket count.
      If expansion fails due to memory, we just keep going. */
   if (oSymTable->uLength > oSymTable->uBucketCount)
      SymTable_expand(oSymTable);

   return 1;
}

/*--------------------------------------------------------------------*/

/* If pcKey exists, replace its value with pvValue and return the old
   value. Otherwise return NULL and do nothing. */

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

/* Return 1 if oSymTable has a binding whose key is pcKey,
   otherwise return 0. */

int SymTable_contains(SymTable_T oSymTable, const char *pcKey)
{
   assert(oSymTable != NULL);
   assert(pcKey != NULL);

   return (SymTable_findBinding(oSymTable, pcKey) != NULL);
}

/*--------------------------------------------------------------------*/

/* Return the value associated with pcKey if it exists,
   or NULL otherwise. */

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

/* If pcKey exists in oSymTable, remove that binding from its bucket's
   chain, free the key string and the binding struct, decrement
   length, and return the binding's value pointer. Otherwise return
   NULL.
   The table does NOT free the value pointer itself. */

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
         /* Save the value to return. */
         pvValue = psCurrent->pvValue;

         /* Unlink from this bucket's chain. */
         if (psPrev == NULL)
            oSymTable->ppsBuckets[uIndex] =
               psCurrent->psNextBinding;
         else
            psPrev->psNextBinding =
               psCurrent->psNextBinding;

         /* Free owned memory. */
         free(psCurrent->pcKey);
         free(psCurrent);

         /* Update length. */
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

/* Apply pfApply to each binding in oSymTable. For each binding,
   call pfApply(pcKey, pvValue, pvExtra). */

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
/*--------------------------------------------------------------------*/
/* symtablehash.c                                                     */
/* Author: Mohemeen Ahmed (modeled on stack6.c style)                 */
/*--------------------------------------------------------------------*/

#include "symtable.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/*--------------------------------------------------------------------*/

/* The required bucket counts for expansion. */
static const size_t auBucketCounts[] = {
   509, 1021, 2039, 4093, 8191, 16381, 32749, 65521
};
enum { BUCKET_COUNTS_LEN = 8 };

/*--------------------------------------------------------------------*/

/* Each binding is stored in a Binding.  Bindings are linked
   to form a list within each bucket. */

struct Binding
{
   /* The key string.  The SymTable owns this memory. */
   char *pcKey;

   /* The value associated with pcKey.
      The SymTable does NOT own this memory. */
   const void *pvValue;

   /* The address of the next Binding in this bucket's chain. */
   struct Binding *psNextBinding;
};

/*--------------------------------------------------------------------*/

/* A SymTable is a hash table with separate chaining. */

struct SymTable
{
   /* The array of buckets. Each bucket is a linked list of Bindings.
      ppsBuckets[i] is the head of that bucket's list, or NULL. */
   struct Binding **ppsBuckets;

   /* The number of buckets currently allocated. */
   size_t uBucketCount;

   /* The number of bindings stored total. */
   size_t uLength;

   /* The index in auBucketCounts[] that matches uBucketCount. */
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

/* Allocate a bucket array of uBucketCount buckets, set them all to
   NULL. Return pointer, or NULL on failure. */
static struct Binding **SymTable_allocateBuckets(size_t uBucketCount)
{
   size_t u;
   struct Binding **ppsBuckets;

   ppsBuckets = (struct Binding**)malloc(uBucketCount *
                                         sizeof(struct Binding*));
   if (ppsBuckets == NULL)
      return NULL;

   for (u = 0; u < uBucketCount; u++)
      ppsBuckets[u] = NULL;

   return ppsBuckets;
}

/*--------------------------------------------------------------------*/

/* Create a new SymTable with the initial bucket count (509), or
   return NULL if insufficient memory. */

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

/* Free all memory owned by oSymTable, including:
   - all bindings
   - all copied keys
   - the bucket array
   Then free oSymTable itself.
   Do NOT free any pvValue. */

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

         /* Free the key (we own it). */
         free(psCurrent->pcKey);

         /* Do not free pvValue. */

         free(psCurrent);
      }
   }

   free(oSymTable->ppsBuckets);
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

/* Helper:
   Return the Binding for pcKey in oSymTable, or NULL if not present.
   Only search the appropriate bucket. */

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

/* Helper for expansion:
   Rehash all existing bindings into a new bucket array with
   uNewBucketCount buckets. On success:
     - Free the old bucket array
     - Install the new one
     - Update bucket count/index
   On failure to allocate new buckets, leave table unchanged. */

static void SymTable_expand(SymTable_T oSymTable)
{
   struct Binding **ppsNewBuckets;
   size_t uNewBucketIndex;
   size_t uNewBucketCount;
   size_t u;
   struct Binding *psCurrent;
   struct Binding *psNext;
   size_t uIndex;

   assert(oSymTable != NULL);

   /* If we're already at the largest bucket count, don't expand. */
   if (oSymTable->uBucketIndex + 1U >= (size_t)BUCKET_COUNTS_LEN)
      return;

   /* Only expand if number of bindings exceeds current bucket count.
      (Caller should have checked this, but we double-check.) */
   if (oSymTable->uLength <= oSymTable->uBucketCount)
      return;

   uNewBucketIndex = oSymTable->uBucketIndex + 1U;
   uNewBucketCount = auBucketCounts[uNewBucketIndex];

   /* Attempt to allocate new bucket array. */
   ppsNewBuckets = SymTable_allocateBuckets(uNewBucketCount);
   if (ppsNewBuckets == NULL)
   {
      /* Spec says: if expansion fails due to memory, just continue. */
      return;
   }

   /* Rehash every binding from old buckets into new buckets. */
   for (u = 0; u < oSymTable->uBucketCount; u++)
   {
      for (psCurrent = oSymTable->ppsBuckets[u];
           psCurrent != NULL;
           psCurrent = psNext)
      {
         psNext = psCurrent->psNextBinding;

         /* Compute new bucket index for this binding's key. */
         uIndex = SymTable_hash(psCurrent->pcKey, uNewBucketCount);

         /* Insert at head of new bucket list. */
         psCurrent->psNextBinding = ppsNewBuckets[uIndex];
         ppsNewBuckets[uIndex] = psCurrent;
      }
   }

   /* Free old bucket array, but NOT the bindings. */
   free(oSymTable->ppsBuckets);

   /* Install new. */
   oSymTable->ppsBuckets = ppsNewBuckets;
   oSymTable->uBucketCount = uNewBucketCount;
   oSymTable->uBucketIndex = uNewBucketIndex;
}

/*--------------------------------------------------------------------*/

/* If pcKey is not already in oSymTable, create a new binding
   (pcKey -> pvValue), insert it into the correct bucket,
   increment length, possibly expand the table, and return 1.
   Otherwise leave oSymTable unchanged and return 0.
   Return 0 if insufficient memory at any point before insertion. */

int SymTable_put(SymTable_T oSymTable,
                 const char *pcKey, const void *pvValue)
{
   struct Binding *psNewBinding;
   char *pcKeyCopy;
   size_t uIndex;

   assert(oSymTable != NULL);
   assert(pcKey != NULL);

   /* If key already exists, do nothing. */
   if (SymTable_findBinding(oSymTable, pcKey) != NULL)
      return 0;

   /* Allocate a new Binding node. */
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

   /* Fill in the new Binding. */
   psNewBinding->pcKey = pcKeyCopy;
   psNewBinding->pvValue = pvValue;

   /* Insert in the right bucket. */
   uIndex = SymTable_hash(pcKey, oSymTable->uBucketCount);
   psNewBinding->psNextBinding = oSymTable->ppsBuckets[uIndex];
   oSymTable->ppsBuckets[uIndex] = psNewBinding;

   /* Update length. */
   oSymTable->uLength++;

   /* Attempt expansion IF length is now "too large". 
      Rule: when a put causes binding count to exceed the current
      bucket count, we should expand to the next bucket count.
      If expansion fails due to memory, we just keep going. */
   if (oSymTable->uLength > oSymTable->uBucketCount)
      SymTable_expand(oSymTable);

   return 1;
}

/*--------------------------------------------------------------------*/

/* If pcKey exists, replace its value with pvValue and return the old
   value. Otherwise return NULL and do nothing. */

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

/* Return 1 if oSymTable has a binding whose key is pcKey,
   otherwise return 0. */

int SymTable_contains(SymTable_T oSymTable, const char *pcKey)
{
   assert(oSymTable != NULL);
   assert(pcKey != NULL);

   return (SymTable_findBinding(oSymTable, pcKey) != NULL);
}

/*--------------------------------------------------------------------*/

/* Return the value associated with pcKey if it exists,
   or NULL otherwise. */

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

/* If pcKey exists in oSymTable, remove that binding from its bucket's
   chain, free the key string and the binding struct, decrement
   length, and return the binding's value pointer. Otherwise return
   NULL.
   The table does NOT free the value pointer itself. */

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
         /* Save the value to return. */
         pvValue = psCurrent->pvValue;

         /* Unlink from this bucket's chain. */
         if (psPrev == NULL)
            oSymTable->ppsBuckets[uIndex] =
               psCurrent->psNextBinding;
         else
            psPrev->psNextBinding =
               psCurrent->psNextBinding;

         /* Free owned memory. */
         free(psCurrent->pcKey);
         free(psCurrent);

         /* Update length. */
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

/* Apply pfApply to each binding in oSymTable. For each binding,
   call pfApply(pcKey, pvValue, pvExtra). */

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
