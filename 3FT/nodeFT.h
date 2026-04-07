/*--------------------------------------------------------------------*/
/* nodeFT.h                                                           */
/*--------------------------------------------------------------------*/

#ifndef NODEFT_INCLUDED
#define NODEFT_INCLUDED

#include <stddef.h>
#include "a4def.h"
#include "path.h"

/*
  Opaque pointer type for a node in the FT.
*/
typedef struct node *Node_T;

/*
  Creates a new FT node with path oPPath and parent oNParent.

  If bIsFile is TRUE, creates a file node with initial contents
  pvContents (which may be NULL) and content length ulLength.

  If bIsFile is FALSE, creates a directory node. In that case,
  pvContents and ulLength are ignored.

  Returns SUCCESS and sets *poNResult to the new node if successful.
  Otherwise sets *poNResult to NULL and returns:
  * MEMORY_ERROR if memory could not be allocated
  * CONFLICTING_PATH if oNParent's path is not an ancestor of oPPath
  * NO_SUCH_PATH if oPPath is of depth 0, or oNParent is not the
    direct parent of oPPath, or oNParent is NULL but oPPath is not
    a valid root path, or a file is requested as the FT root
  * ALREADY_IN_TREE if oNParent already has a child with this path
  * NOT_A_DIRECTORY if oNParent is a file

  If oNParent is NULL, the new node must be the FT root, and thus
  must be a directory of depth 1.
*/
int Node_new(Path_T oPPath, Node_T oNParent, boolean bIsFile,
             void *pvContents, size_t ulLength, Node_T *poNResult);

/*
  Frees oNNode, removing it from its parent's children array.
  If oNNode is a directory, recursively frees its subtree.

  Does NOT free a file node's contents -- those are owned by the
  client.

  Returns the number of nodes freed.
*/
size_t Node_free(Node_T oNNode);

/*
  Returns the path of oNNode.
*/
Path_T Node_getPath(Node_T oNNode);

/*
  Returns the parent of oNNode.
  Returns NULL iff oNNode is the root.
*/
Node_T Node_getParent(Node_T oNNode);

/*
  Returns TRUE iff oNNode is a file.
*/
boolean Node_isFile(Node_T oNNode);

/*
  Returns TRUE iff oNNode is a directory.
*/
boolean Node_isDir(Node_T oNNode);

/*
  Returns TRUE iff directory oNParent has a child whose path is oPPath,
  and in that case sets *pulChildID to that child's index in the
  children array.

  Returns FALSE otherwise.

  If oNParent is a file, always returns FALSE.
*/
boolean Node_hasChild(Node_T oNParent, Path_T oPPath,
                      size_t *pulChildID);

/*
  Returns the number of children of directory oNParent.
  Returns 0 if oNParent is a file.
*/
size_t Node_getNumChildren(Node_T oNParent);

/*
  Sets *poNResult to child number ulChildID of directory oNParent
  and returns SUCCESS.

  Otherwise sets *poNResult to NULL and returns:
  * NOT_A_DIRECTORY if oNParent is a file
  * NO_SUCH_PATH if ulChildID is out of range
*/
int Node_getChild(Node_T oNParent, size_t ulChildID,
                  Node_T *poNResult);

/*
  Returns the contents of file oNNode. May legitimately be NULL --
  callers cannot use a NULL return value to detect errors.

  Returns NULL as well if oNNode is a directory.
*/
void *Node_getContents(Node_T oNNode);

/*
  Returns the length in bytes of file oNNode's contents.
  Returns 0 if oNNode is a directory.
*/
size_t Node_getLength(Node_T oNNode);

/*
  Replaces file oNNode's contents with pvNewContents (which may be NULL)
  and updates its stored length to ulNewLength. Returns the previous
  contents pointer, transferring ownership of that memory back to
  the caller.

  Returns NULL if oNNode is a directory.
*/
void *Node_replaceContents(Node_T oNNode, void *pvNewContents,
                           size_t ulNewLength);

/*
  Compares oNFirst and oNSecond lexicographically by path.
  Returns <0, 0, or >0 as usual.
*/
int Node_compare(Node_T oNFirst, Node_T oNSecond);

/*
  Returns a newly allocated string containing the pathname of oNNode,
  or NULL if allocation fails. Caller owns the returned string.
*/
char *Node_toString(Node_T oNNode);

#endif