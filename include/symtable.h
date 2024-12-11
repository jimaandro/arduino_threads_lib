#ifndef __SYMTABLE_H
#define __SYMTABLE_H

typedef struct SymTable *SymTable_T;

/* Returns a new, empty symbol table */
SymTable_T SymTable_new(void);

/* Frees the oSymTable symbol table and all of its bindings.
 * All memory previously allocated is freed. */
void SymTable_free(SymTable_T oSymTable);

/* Returns the number of bindings in oSymTable */
unsigned int SymTable_getLength(SymTable_T oSymTable);

/* Inserts a new binding with key pcKey and value pvValue in oSymTable,
 * if the key is not in oSymTable.
 * If the key already exists in oSymTable, oSymTable is not modified.
 * A copy of the key only is kept in the binding.
 * Returns 1 if the key doesn't exist already,
 *         0 if the key already exists. */
int SymTable_put(SymTable_T oSymTable, const int pcKey, const void *pvValue);

/* If a binding with key pcKey is in oSymTable, remove the binding and return 1.
 * Otherwise, oSymTable is unaffected and 0 is returned. */
int SymTable_remove(SymTable_T oSymTable, const int pcKey);

/* Returns 1 if oSymTable contains a binding with key pcKey, 0 otherwise. */
int SymTable_contains(SymTable_T oSymTable, const int pcKey);

/* Returns the value of the binding of oSymTable with key pcKey, if there is one.
 * Otherwise, it returns NULL. */
void *SymTable_get(SymTable_T oSymTable, const int pcKey);

/* Applies pfApply in each binding of oSymTable, passing pvExtra as an extra
 * parameter to it. */
void SymTable_map(SymTable_T oSymTable,
                  void (*pfApply)(const int pcKey, void *pvValue, void *pvExtra),
                  const void *pvExtra);
#endif
