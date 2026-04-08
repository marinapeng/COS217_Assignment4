/*--------------------------------------------------------------------*/
/* nodeFT.c                                                           */
/* Author: adapted from nodeDT.c                                      */
/*--------------------------------------------------------------------*/

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "dynarray.h"
#include "nodeFT.h"

/* A node in an FT */
struct node {
   /* the absolute path represented by this node */
   Path_T oPPath;

   /* this node's parent */
   Node_T oNParent;

   /* TRUE if this node is a file, FALSE if it is a directory */
   boolean bIsFile;

   /* for directory nodes only: children in lexicographic path order */
   DynArray_T oDChildren;

   /* for file nodes only: raw client-owned contents pointer + length */
   void *pvContents;
   size_t ulLength;
};


/*
  Links new child oNChild into directory oNParent's children array at
  index ulIndex. Returns SUCCESS if successful or MEMORY_ERROR if not.
*/
static int Node_addChild(Node_T oNParent, Node_T oNChild,
   size_t ulIndex) {
   assert(oNParent != NULL);
   assert(oNChild != NULL);
   assert(!oNParent->bIsFile);
   assert(oNParent->oDChildren != NULL);

   if(DynArray_addAt(oNParent->oDChildren, ulIndex, oNChild))
      return SUCCESS;
   else
      return MEMORY_ERROR;
}

/*
  Compares the string representation of oNFirst with a string pcSecond
  representing a node path.
*/
static int Node_compareString(const Node_T oNFirst,
   const char *pcSecond) {
   assert(oNFirst != NULL);
   assert(pcSecond != NULL);

   return Path_compareString(oNFirst->oPPath, pcSecond);
}


/*
  Creates a new FT node with path oPPath and parent oNParent.
  If bIsFile is TRUE, creates a file node with contents pvContents
  and length ulLength.
  If bIsFile is FALSE, creates a directory node.

  Returns:
  * SUCCESS
  * MEMORY_ERROR
  * CONFLICTING_PATH
  * NO_SUCH_PATH
  * ALREADY_IN_TREE
  * NOT_A_DIRECTORY
*/
int Node_new(Path_T oPPath, Node_T oNParent, boolean bIsFile,
   void *pvContents, size_t ulLength, Node_T *poNResult) {
   struct node *psNew;
   Path_T oPParentPath = NULL;
   Path_T oPNewPath = NULL;
   size_t ulParentDepth;
   size_t ulIndex;
   int iStatus;

   assert(oPPath != NULL);
   assert(poNResult != NULL);

   psNew = malloc(sizeof(struct node));
   if(psNew == NULL) {
      *poNResult = NULL;
      return MEMORY_ERROR;
   }

   iStatus = Path_dup(oPPath, &oPNewPath);
   if(iStatus != SUCCESS) {
      free(psNew);
      *poNResult = NULL;
      return iStatus;
   }

   psNew->oPPath = oPNewPath;
   psNew->oNParent = oNParent;
   psNew->bIsFile = bIsFile;
   psNew->oDChildren = NULL;
   psNew->pvContents = NULL;
   psNew->ulLength = 0;

   /* validate and set parent relation */
   if(oNParent != NULL) {
      size_t ulSharedDepth;

      /* files cannot have children */
      if(oNParent->bIsFile) {
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return NOT_A_DIRECTORY;
      }

      oPParentPath = oNParent->oPPath;
      ulParentDepth = Path_getDepth(oPParentPath);
      ulSharedDepth = Path_getSharedPrefixDepth(psNew->oPPath, oPParentPath);

      /* parent must be an ancestor of child */
      if(ulSharedDepth < ulParentDepth) {
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return CONFLICTING_PATH;
      }

      /* parent must be exactly one level up from child */
      if(Path_getDepth(psNew->oPPath) != ulParentDepth + 1) {
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return NO_SUCH_PATH;
      }

      /* parent must not already have child with this path */
      if(Node_hasChild(oNParent, oPPath, &ulIndex)) {
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return ALREADY_IN_TREE;
      }
   }
   else {
      /* root must be a directory and depth 1 */
      if(bIsFile || Path_getDepth(psNew->oPPath) != 1) {
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return NO_SUCH_PATH;
      }
   }

   /* initialize node contents by type */
   if(bIsFile) {
      psNew->pvContents = pvContents;
      psNew->ulLength = ulLength;
      psNew->oDChildren = NULL;
   }
   else {
      psNew->oDChildren = DynArray_new(0);
      if(psNew->oDChildren == NULL) {
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return MEMORY_ERROR;
      }
   }

   /* link into parent's children list */
   if(oNParent != NULL) {
      iStatus = Node_addChild(oNParent, psNew, ulIndex);
      if(iStatus != SUCCESS) {
         if(psNew->oDChildren != NULL)
            DynArray_free(psNew->oDChildren);
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return iStatus;
      }
   }

   *poNResult = psNew;
   return SUCCESS;
}

/*
  Frees the subtree rooted at oNNode.
  Returns the number of nodes freed.
*/
size_t Node_free(Node_T oNNode) {
   size_t ulIndex;
   size_t ulCount = 0;

   assert(oNNode != NULL);

   /* remove from parent's child list */
   if(oNNode->oNParent != NULL) {
      assert(!oNNode->oNParent->bIsFile);
      if(DynArray_bsearch(
            oNNode->oNParent->oDChildren,
            oNNode, &ulIndex,
            (int (*)(const void *, const void *)) Node_compare))
         (void)DynArray_removeAt(oNNode->oNParent->oDChildren, ulIndex);
   }

   /* recursively free children if directory */
   if(!oNNode->bIsFile) {
      while(DynArray_getLength(oNNode->oDChildren) != 0) {
         ulCount += Node_free(DynArray_get(oNNode->oDChildren, 0));
      }
      DynArray_free(oNNode->oDChildren);
   }

   /* contents pointer is client-owned, so do not free it */
   Path_free(oNNode->oPPath);
   free(oNNode);
   ulCount++;
   return ulCount;
}


Path_T Node_getPath(Node_T oNNode) {
   assert(oNNode != NULL);
   return oNNode->oPPath;
}


Node_T Node_getParent(Node_T oNNode) {
   assert(oNNode != NULL);
   return oNNode->oNParent;
}


boolean Node_isFile(Node_T oNNode) {
   assert(oNNode != NULL);
   return oNNode->bIsFile;
}

boolean Node_isDir(Node_T oNNode) {
   assert(oNNode != NULL);
   return (boolean)!oNNode->bIsFile;
}

boolean Node_hasChild(Node_T oNParent, Path_T oPPath,
                      size_t *pulChildID) {
   assert(oNParent != NULL);
   assert(oPPath != NULL);
   assert(pulChildID != NULL);

   if(oNParent->bIsFile) {
      return FALSE;
   }

   return DynArray_bsearch(oNParent->oDChildren,
            (char *)Path_getPathname(oPPath), pulChildID,
            (int (*)(const void *, const void *))Node_compareString);
}

size_t Node_getNumChildren(Node_T oNParent) {
   assert(oNParent != NULL);

   if(oNParent->bIsFile) {
      return 0;
   }

   return DynArray_getLength(oNParent->oDChildren);
}


int Node_getChild(Node_T oNParent, size_t ulChildID,
                  Node_T *poNResult) {
   assert(oNParent != NULL);
   assert(poNResult != NULL);

   if(oNParent->bIsFile) {
      *poNResult = NULL;
      return NOT_A_DIRECTORY;
   }

   if(ulChildID >= Node_getNumChildren(oNParent)) {
      *poNResult = NULL;
      return NO_SUCH_PATH;
   }

   *poNResult = DynArray_get(oNParent->oDChildren, ulChildID);
   return SUCCESS;
}

void *Node_getContents(Node_T oNNode) {
   assert(oNNode != NULL);

   if(!oNNode->bIsFile)
      return NULL;

   return oNNode->pvContents;
}


size_t Node_getLength(Node_T oNNode) {
   assert(oNNode != NULL);

   if(!oNNode->bIsFile)
      return 0;

   return oNNode->ulLength;
}

void *Node_replaceContents(Node_T oNNode, void *pvNewContents,
   size_t ulNewLength) {
   void *pvOldContents;

   assert(oNNode != NULL);

   if(!oNNode->bIsFile) {
      return NULL;
   }

   pvOldContents = oNNode->pvContents;
   oNNode->pvContents = pvNewContents;
   oNNode->ulLength = ulNewLength;

   return pvOldContents;
}


int Node_compare(Node_T oNFirst, Node_T oNSecond) {
   assert(oNFirst != NULL);
   assert(oNSecond != NULL);

   return Path_comparePath(oNFirst->oPPath, oNSecond->oPPath);
}

char *Node_toString(Node_T oNNode) {
   char *copyPath;

   assert(oNNode != NULL);

   copyPath = malloc(Path_getStrLength(Node_getPath(oNNode)) + 1);
   if(copyPath == NULL) {
      return NULL;
   }

   return strcpy(copyPath, Path_getPathname(Node_getPath(oNNode)));
}