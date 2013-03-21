//
//  CBDatabase.h
//  cbitcoin
//
//  Created by Matthew Mitchell on 03/11/2012.
//  Copyright (c) 2012 Matthew Mitchell
//
//  This file is part of cbitcoin. It is subject to the license terms
//  in the LICENSE file found in the top-level directory of this
//  distribution and at http://www.cbitcoin.com/license.html. No part of
//  cbitcoin, including this file, may be copied, modified, propagated,
//  or distributed except according to the terms contained in the
//  LICENSE file.

/**
 @file
 @brief Implements a database for use with the storage requirements.
 */

#ifndef CBDATABASEH
#define CBDATABASEH

#include "CBDependencies.h"
#include "CBAssociativeArray.h"
#include "CBFileDependencies.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define CB_OVERWRITE_DATA 0xFFFFFFFF
#define CB_DELETED_VALUE 0xFFFFFFFF
#define CB_DOESNT_EXIST CB_DELETED_VALUE
#define CB_DATABASE_BTREE_ELEMENTS 64
#define CB_DATABASE_BTREE_HALF_ELEMENTS (CB_DATABASE_BTREE_ELEMENTS/2)

typedef enum{
	CB_DATABASE_FILE_TYPE_INDEX,
	CB_DATABASE_FILE_TYPE_DELETION_INDEX,
	CB_DATABASE_FILE_TYPE_DATA,
	CB_DATABASE_FILE_TYPE_NONE,
} CBDatabaseFileType;

typedef enum{
	CB_DATABASE_INDEX_FOUND,
	CB_DATABASE_INDEX_NOT_FOUND,
	CB_DATABASE_INDEX_ERROR,
} CBIndexFindStatus;

/**
 @brief An index value which references the value's data position with a key.
 */
typedef struct{
	uint8_t * key; /**< The key for the value */
	uint16_t fileID; /**< The file ID for the data */
	uint32_t pos; /**< The position of the data in the file. */
	uint32_t length; /**< The length of the data or CB_DELETED_VALUE if deleted. */
} CBIndexValue;

/**
 @brief Describes a deleted section of the database.
 */
typedef struct{
	uint8_t key[12]; /**< The key for this deleted section which begins with 0x01 if the deleted section is active or 0x00 if it is no longer active, then has four bytes for the length of the deleted section in big-endian, then the file ID in little-endian and finally the offset of the deleted section in little-endian. */
	uint32_t indexPos; /**< The position in the index file where this value exists */
} CBDeletedSection;

typedef struct{
	uint16_t indexFile;
	uint32_t offset;
} CBIndexDiskNodeLocation;

/**
 @brief A union for describing a btree node in memory cache or on disk.
 */
union CBIndexNodeLocation{
	CBIndexDiskNodeLocation diskNodeLoc; /**< The node location on disk. */
	void * cachedNode; /**< A pointer to the CBIndexCacheNode child in memory. */
};

/**
 @brief Information for a btree node in memory cache or on disk.
 */
typedef struct{
	bool cached; /**< True if the child is cached in memory. */
	union CBIndexNodeLocation location; /**< The location of the child. */
} CBIndexNodeLocation;

/**
 @brief The memory cache of the index B-Tree
 */
typedef struct{
	void * parent; /**< The parent of this node. */
	uint8_t parentChildIndex; /**< The position of this node in the parent. */
	uint8_t numElements; /**< The number of elements in the node. */
	CBIndexValue elements[CB_DATABASE_BTREE_ELEMENTS]; /**< The elements. */
	CBIndexNodeLocation children[CB_DATABASE_BTREE_ELEMENTS]; /**< The children. */
	uint8_t indexFile; /**> The index file number to find this node. */
	uint32_t offset; /**< The offset in the file to the node. */
} CBIndexNode;

typedef struct{
	uint8_t ID; /**< The ID for the index */
	CBIndexNode indexCache; /**< The memory cache of the btree nodes for the index, at the lowest levels of the tree. */
	uint64_t numCached; /**< The number of cached nodes */
	uint8_t keySize; /**< The size in bytes of the keys. */
	uint32_t cacheLimit; /**< The number of bytes that can be cached */
	uint16_t lastFile; /**< The last index file ID. */
	uint32_t lastSize; /**< Size of last index file */
	uint16_t newLastFile; /** Used to store the new last file number during a commit. */
	uint32_t newLastSize; /** Used to store the new last file size during a commit. */
} CBDatabaseIndex;

typedef struct{
	bool cached;
	CBIndexNode * node;
} CBIndexNodeAndIfCached;

typedef struct{
	CBIndexFindStatus status;
	uint8_t index;
	CBIndexNodeAndIfCached * nodeIfCached;
	CBIndexDiskNodeLocation location; /**< Location of the element on the disk. */
} CBIndexFindResult;

typedef struct{
	CBAssociativeArray valueWrites; /**< Values to write. A pointer to the index object followed by the key followed by a 32 bit integer for the data length and then the data. */
	CBAssociativeArray deleteKeys; /**< An array of keys to delete with the first bytes being the index pointer and the remaining bytes being the key data. */
	void ** changeKeys; /**< A list of keys to be changed with the last bytes being the new key. The index pointer comes before the old key and then the new key. */
	uint32_t numChangeKeys;
	uint8_t numIndexes; /**< The number indexes that are involved in the transaction */
	CBAssociativeArray indexes; /**< An array of index objects. */
} CBDatabaseTransaction;

/**
 @brief Structure for CBDatabase objects. @see CBDatabase.h
 */
typedef struct{
	char * dataDir; /**< The data directory. */
	char * folder; /**< The folder for this database. */
	uint16_t lastFile; /**< The last file ID. */
	uint32_t lastSize; /**< Size of last file */
	CBAssociativeArray deletionIndex; /**< Index of all deleted sections. The key begins with 0x01 if the deleted section is active or 0x00 if it is no longer active, the key is then followed by the length in big endian. */
	uint32_t numDeletionValues; /**< Number of values in the deletion index */
	// Files
	uint64_t deletionIndexFile;
	uint64_t fileObjectCache; /**< Stores last used file object for reuse until new file is needed */
	uint16_t lastUsedFileID; /**< The last used file number or CB_DATABASE_FILE_TYPE_NONE if none. */
	CBDatabaseFileType lastUsedFileType; /**< The last used file type. */
	uint8_t lastUsedFileIndexID; /**< The last used index ID. */
	uint64_t logFile;
} CBDatabase;

// Initialisation

/**
 @brief Returns a new database object.
 @param dataDir The directory where the data files should be stored.
 @param folder A folder for the data files to prevent conflicts. Will be created if it doesn't exist.
 @returns The database object or NULL on failure.
 */
CBDatabase * CBNewDatabase(char * dataDir, char * folder);
/**
 @brief Returns a new database transaction object for modifying the database.
 @returns The database transaction object or NULL on failure.
 */
CBDatabaseTransaction * CBNewDatabaseTransaction(void);
/**
 @brief Initialises a database object.
 @param self The CBDatabase object to initialise.
 @param dataDir The directory where the data files should be stored.
 @param prefix A prefix for the data files to prevent conflicts.
 @returns true on success and false on failure.
 */
bool CBInitDatabase(CBDatabase * self, char * dataDir, char * folder);
/**
 @brief Initialises a database transaction object.
 @param self The CBDatabaseTransaction object to initialise.
 @returns true on success and false on failure.
 */
bool CBInitDatabaseTransaction(CBDatabaseTransaction * self);
/**
 @brief Loads an index or creates it if it doesn't exist.
 @param self The database object.
 @param indexID The identifier for this index.
 @param keySize The key size for this index.
 @param cacheLimit The number of bytes that can be cached for this index.
 @returns The index or NULL on failure.
 */
CBDatabaseIndex * CBLoadIndex(CBDatabase * self, uint8_t indexID, uint8_t keySize, uint32_t cacheLimit);
/**
 @brief Loads a node from the disk into memory.
 @param self The database object.
 @param index The index object.
 @param node The allocated node.
 @param nodeFile The index of the index files where the node exists.
 @param nodeOffset The offset from the begining of the file where the node exists.
 */
bool CBDatabaseLoadIndexNode(CBDatabase * self, CBDatabaseIndex * index, CBIndexNode * node, uint16_t nodeFile, uint32_t nodeOffset);
/**
 @brief Reads and opens the deletion index during initialisation
 @param self The storage object.
 @param filename The index filename.
 @returns true on success or false on failure.
 */
bool CBDatabaseReadAndOpenDeletionIndex(CBDatabase * self, char * filename);
/**
 @brief Creates a deletion index during the first initialisation
 @param self The storage object.
 @param filename The index filename.
 @returns true on success or false on failure.
 */
bool CBDatabaseCreateDeletionIndex(CBDatabase * self, char * filename);

/**
 @brief Gets a CBDatabase from another object. Use this to avoid casts.
 @param self The object to obtain the CBDatabase from.
 @returns The CBDatabase object.
 */
CBDatabase * CBGetDatabase(void * self);

// Destructor

/**
 @brief Frees the database object.
 @param self The database object.
 */
void CBFreeDatabase(CBDatabase * self);
/**
 @brief Frees the database transaction object.
 @param self The database transaction object.
 */
void CBFreeDatabaseTransaction(CBDatabaseTransaction * self);
/**
 @brief Frees the node of an index.
 @param node The node to free.
 */
void CBFreeIndexNode(CBIndexNode * node);
/**
 @brief Frees an index.
 @param index The index to free.
 */
void CBFreeIndex(CBDatabaseIndex * index);

// Additional functions

/**
 @brief Add a deletion entry.
 @param self The storage object.
 @param fileID The file ID
 @param pos The position of the deleted section.
 @param len The length of the deleted section.
 @retruns true on success and false on failure
 */
bool CBDatabaseAddDeletionEntry(CBDatabase * self, uint16_t fileID, uint32_t pos, uint32_t len);
/**
 @brief Add an overwrite operation.
 @param self The storage object.
 @param fileType The type of the file.
 @param indexID The index ID used if the type is an index.
 @param fileID The file ID
 @param data The data to write.
 @param offset The offset to begin writting.
 @param dataLen The length of the data to write.
 @retruns true on success and false on failure
 */
bool CBDatabaseAddOverwrite(CBDatabase * self, CBDatabaseFileType fileType, uint8_t indexID, uint16_t fileID, uint8_t * data, uint32_t offset, uint32_t dataLen);
/**
 @brief Adds a value to the database without overwriting previous indexed data.
 @param self The storage object.
 @param dataSize The size of the data to write.
 @param data The data to write.
 @param indexValue The index data to write.
 @retruns true on success and false on failure
 */
bool CBDatabaseAddValue(CBDatabase * self, uint32_t dataSize, uint8_t * data, CBIndexValue * indexValue);
/**
 @brief Adds a write value to the valueWrites array for the transaction.
 @param self The database transaction object.
 @param writeValue The write value element data.
 @retruns true on success and false on failure
 */
bool CBDatabaseAddWriteValue(CBDatabaseTransaction * self, uint8_t * writeValue);
/**
 @brief Add an append operation.
 @param self The database object.
 @param fileType The type of the file to append.
 @param indexID The index ID used if appending an index.
 @param fileID The file ID
 @param data The data to write.
 @param dataLen The length of the data to write.
 @retruns true on success and false on failure
 */
bool CBDatabaseAppend(CBDatabase * self, CBDatabaseFileType fileType, uint8_t indexID, uint16_t fileID, uint8_t * data, uint32_t dataLen);
/**
 @brief Replaces a key for a value with a key of the same length.
 @param self The database transaction object.
 @param index The database index to change the key for.
 @param previousKey The current key to be replaced. The first byte is the length.
 @param newKey The new key for this value. The first byte is the length and should be the same as the first key.
 @returns true on success and false on failure.
 */
bool CBDatabaseChangeKey(CBDatabaseTransaction * self, CBDatabaseIndex * index, uint8_t * previousKey, uint8_t * newKey);
/**
 @brief Removes all of the pending value write, delete and change key operations.
 @param self The database transaction object.
 */
void CBDatabaseClearPending(CBDatabaseTransaction * self);
/**
 @brief The data is written to the disk.
 @param self The database object.
 @param tx The transaction to commit.
 @returns true on success and false on failure, and thus the database needs to be recovered with CBDatabaseEnsureConsistent.
 */
bool CBDatabaseCommit(CBDatabase * self, CBDatabaseTransaction * tx);
/**
 @brief Ensure the database is consistent and recover the database if it is not.
 @param self The database object.
 @returns true if the database is consistent and false on failure.
 */
bool CBDatabaseEnsureConsistent(CBDatabase * self);
/**
 @brief Returns a CBFindResult for largest active deleted section and "found" will be true if the largest active deleted section is above a length.
 @param self The database object.
 @param length The minimum length required.
 @returns The largest active deleted section as a CBFindResult.
 */
CBFindResult CBDatabaseGetDeletedSection(CBDatabase * self, uint32_t length);
/**
 @brief Gets a file object for a file number (eg. 0 for index, 1 for deleted index and 2 for first data file)
 @param self The database object.
 @param type The type of file to obtain.
 @param indexID The index ID, only applicable for CB_DATABASE_FILE_TYPE_INDEX.
 @param fileID The id of the file to get an object for. Not needed for CB_DATABASE_FILE_TYPE_DELETION_INDEX.
 @returns A pointer to the file object on success and NULL on failure.
 */
uint64_t CBDatabaseGetFile(CBDatabase * self, CBDatabaseFileType type, uint8_t indexID, uint16_t fileID);
/**
 @brief Gets the length of a value in the database or CB_DOESNT_EXIST if it does not exist.
 @param self The database object.
 @param index The database index.
 @param tx The database transaction.
 @param key The key. The first byte is the length.
 @returns The total length of the value or CB_DOESNT_EXIST if the value does not exist in the database.
 */
uint32_t CBDatabaseGetLength(CBDatabase * self, CBDatabaseIndex * index, CBDatabaseTransaction * tx, uint8_t * key);
/**
 @brief Deletes an index element.
 @param self The database object.
 @param index The index object.
 @param res The information of the node we are inserting to.
 @returns true on success and false on failure.
 */
bool CBDatabaseIndexDelete(CBDatabase * self, CBDatabaseIndex * index, CBIndexFindResult * res);
/**
 @brief Finds an index entry, or where the index should be inserted.
 @param self The database object.
 @param index The index object.
 @param key The key to find.
 @returns @see CBIndexFindResult
 */
CBIndexFindResult CBDatabaseIndexFind(CBDatabase * self, CBDatabaseIndex * index, uint8_t * key);
/**
 @brief Copies elements in an index node.
 @param self The database object.
 @param index The index object.
 @param dest The destination node.
 @param source The source node.
 @param startPos The starting position to take elements from the source node.
 @param endPos The starting position to place elements in the destination node.
 @param amount The number of elements to copy.
 @param true on success or false on failure.
 */
bool CBDatabaseIndexMoveElements(CBDatabase * self, CBDatabaseIndex * index, CBIndexNodeAndIfCached * dest, CBIndexNodeAndIfCached * source, uint8_t startPos, uint8_t endPos, uint8_t amount);
/**
 @brief Inserts an index value into an index.
 @param self The database object.
 @param index The index object.
 @param indexVal The index value to insert.
 @param res The information of the node we are inserting to.
 @returns true on success and false on failure.
 */
bool CBDatabaseIndexInsert(CBDatabase * self, CBDatabaseIndex * index, CBIndexValue * indexVal, CBIndexFindResult * res);
/**
 @brief Does a binary search on a node in the cache and returns the location and index data.
 @param index The index object.
 @param node The node to search.
 @param key The key to search for.
 @returns @see CBIndexFindResult.
 */
CBIndexFindResult CBDatabaseIndexNodeBinarySearch(CBDatabaseIndex * index, CBIndexNode * node, uint8_t * key);
/**
 @brief Queues a key-value read operation.
 @param self The database object.
 @param key The key for this data. The first byte is the length.
 @param data A pointer to memory with enough space to hold the data. The data will be set to this.
 @param dataSize The size to read.
 @param offset The offset to begin reading.
 @returns true on success and false on failure.
 */
bool CBDatabaseReadValue(CBDatabase * self, uint8_t * key, uint8_t * data, uint32_t dataSize, uint32_t offset);
/**
 @brief Queues a key-value delete operation.
 @param self The database object.
 @param key The key for this data. The first byte is the length.
 @returns true on success and false on failure.
 */
bool CBDatabaseRemoveValue(CBDatabase * self, uint8_t * key);
/**
 @brief Queues a key-value write operation from a many data parts. The data is concatenated.
 @param self The database object.
 @param key The key for this data. The first byte is the length.
 @param numDataParts The number of data parts.
 @param data A list of the data.
 @param dataSize A list of the data sizes
 @returns true on success and false on failure.
 */
bool CBDatabaseWriteConcatenatedValue(CBDatabase * self, uint8_t * key, uint8_t numDataParts, uint8_t ** data, uint32_t * dataSize);
/**
 @brief Queues a key-value write operation.
 @param self The database object.
 @param key The key for this data. The first byte is the length.
 @param data The data to store.
 @param size The size of the data to store. A new value replaces the old one.
 @returns true on success and false on failure.
 */
bool CBDatabaseWriteValue(CBDatabase * self, uint8_t * key, uint8_t * data, uint32_t size);
/**
 @brief Queues a key-value write operation for a sub-section of existing data.
 @param self The database object.
 @param key The key for this data. The first byte is the length.
 @param data The data to store.
 @param size The size of the data to write.
 @param offset The offset to start writing. CB_OVERWRITE_DATA to remove the old data and write anew.
 @returns true on success and false on failure.
 */
bool CBDatabaseWriteValueSubSection(CBDatabase * self, uint8_t * key, uint8_t * data, uint32_t size, uint32_t offset);

#endif