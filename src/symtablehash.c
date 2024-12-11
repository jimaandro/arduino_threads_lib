#include "symtable.h"
#include "threadsafe_libc.h"
#include <stddef.h>
#include <stdio.h>

#define HASHTABLE_SIZE 16

struct SymTable_bind {
    int pcKey;
    void *value;

    struct SymTable_bind *next;
};

struct SymTable {
    unsigned int length;

    struct SymTable_bind *table[HASHTABLE_SIZE];
};

/* Return a hash code for pcKey. */
static unsigned int SymTable_hash(const int pcKey) {
    return pcKey % HASHTABLE_SIZE;
}

SymTable_T SymTable_new(void) {
    /* Allocate memory for the new symbol table */
    SymTable_T new = malloc(sizeof(struct SymTable));
    unsigned int i;

    /* Check allocated memory */
    if (new == NULL) {
        printf("Ran out of memory while trying to allocate a new SymTable!\n"
               "Exiting now...");
        exit(-1);
    }

    /* Initialize the new symbol table */
    for (i = 0; i < HASHTABLE_SIZE; i++) {
        new->table[i] = NULL;
    }
    new->length = 0;

    return new;
}

void SymTable_free(SymTable_T oSymTable) {
    struct SymTable_bind *tmp;
    size_t i;

    /* Don't do anything if oSymTable == NULL */
    if (oSymTable == NULL)
        return;

    /* Free each binding and its key in oSymTable */
    for (i = 0; i < HASHTABLE_SIZE; i++) {
        while ((tmp = oSymTable->table[i]) != NULL) {
            oSymTable->table[i] = (oSymTable->table)[i]->next;
            free(tmp);
        }
    }

    /* Free the symbol table */
    free(oSymTable);
}

unsigned int SymTable_getLength(SymTable_T oSymTable) {
    threadsafe_assert(oSymTable != NULL);

    return oSymTable->length;
}

int SymTable_put(SymTable_T oSymTable, const int pcKey, const void *pvValue) {
    struct SymTable_bind *tmp;
    unsigned int hashing;

    threadsafe_assert(oSymTable != NULL);

    /* Calculate the hash of the given key % the size of the table */
    hashing = SymTable_hash(pcKey);

    tmp = oSymTable->table[hashing];

    /* Move through the chain to find if the key exists */
    while (tmp != NULL && tmp->pcKey != pcKey) {
        tmp = tmp->next;
    }

    /* If the key does not exist, create a new
     * binding at the beginning of the chain */
    if (tmp == NULL) {
        /* Allocate memory for the new binding */
        tmp = malloc(sizeof(struct SymTable_bind));

        /* Check allocated memory */
        if (tmp == NULL) {
            printf("Ran out of memory while trying to allocate a new SymTable "
                   "binding!\n"
                   "Exiting now...");
            exit(-1);
        }

        /* Set the data of the binding and connect it to the chain */
        tmp->pcKey = pcKey;
        tmp->value = (void *)pvValue;
        tmp->next = oSymTable->table[hashing];
        oSymTable->table[hashing] = tmp;

        /* Increase counter of bindings */
        oSymTable->length++;

        return 1;
    }

    return 0;
}

int SymTable_remove(SymTable_T oSymTable, const int pcKey) {
    struct SymTable_bind *prev = NULL, *tmp;
    unsigned int hashing;

    threadsafe_assert(oSymTable != NULL);

    /* Calculate the hash of the given key % the size of the table */
    hashing = SymTable_hash(pcKey);
    tmp = oSymTable->table[hashing];

    /* If the binding is in the begining of the chain, remove it */
    if (tmp != NULL && pcKey == tmp->pcKey) {
        oSymTable->table[hashing] = oSymTable->table[hashing]->next;

        free(tmp);

        oSymTable->length--;

        return 1;
    }

    /* Locate the binding, if it exists in the chain */
    while (tmp != NULL && pcKey != tmp->pcKey) {
        prev = tmp;
        tmp = tmp->next;
    }

    /* If a binding with key==pcKey exists, remove it */
    if (tmp != NULL) {
        prev->next = tmp->next;

        free(tmp);

        oSymTable->length--;

        return 1;
    }

    return 0;
}

int SymTable_contains(SymTable_T oSymTable, const int pcKey) {
    struct SymTable_bind *tmp;
    unsigned int hashing;

    threadsafe_assert(oSymTable != NULL);

    /* Calculate the hash of the given key % the size of the table */
    hashing = SymTable_hash(pcKey);
    tmp = oSymTable->table[hashing];

    /* Lookup the key in the chain */
    while (tmp != NULL && pcKey != tmp->pcKey) {
        tmp = tmp->next;
    }

    /* Return if the key was found */
    return (tmp != NULL);
}

void *SymTable_get(SymTable_T oSymTable, const int pcKey) {
    struct SymTable_bind *tmp;
    unsigned int hashing;

    threadsafe_assert(oSymTable != NULL);

    /* Calculate the hash of the given key % the size of the table */
    hashing = SymTable_hash(pcKey);
    tmp = oSymTable->table[hashing];

    /* Lookup the key in the chain */
    while (tmp != NULL && pcKey != tmp->pcKey) {
        tmp = tmp->next;
    }

    /* If found, return the value otherwise return NULL */
    if (tmp != NULL) {
        return tmp->value;
    }

    return NULL;
}

void SymTable_map(SymTable_T oSymTable,
                  void (*pfApply)(const int pcKey, void *pvValue, void *pvExtra),
                  const void *pvExtra) {
    struct SymTable_bind *tmp;
    unsigned int i;

    threadsafe_assert(oSymTable != NULL);
    threadsafe_assert(pfApply != NULL);

    /* Call pfApply on each binding in every chain */
    for (i = 0; i < HASHTABLE_SIZE; i++) {
        tmp = oSymTable->table[i];

        while (tmp != NULL) {
            pfApply(tmp->pcKey, tmp->value, (void *)pvExtra);

            tmp = tmp->next;
        }
    }
}