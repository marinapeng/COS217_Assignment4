/*--------------------------------------------------------------------*/
/* ft.c                                                               */
/* Author: adapted from dt.c                                          */
/*--------------------------------------------------------------------*/

#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "dynarray.h"
#include "path.h"
#include "nodeFT.h"
#include "ft.h"

/* A File Tree is a representation of a hierarchy of directories and
  files, represented as an AO with 3 state variables: */

/* a flag for being in an initialized state (TRUE) or not (FALSE) */
static boolean bIsInitialized;

/* a pointer to the root node in the hierarchy */
static Node_T oNRoot;

/* a counter of the number of nodes in the hierarchy */
static size_t ulCount;

/*
  Traverses the FT starting at the root as far as possible towards
  path oPPath. If able to traverse, returns SUCCESS and sets
  *poNFurthest to the furthest node reached, which may be NULL if the
  tree is empty, a prefix of oPPath, or the exact node for oPPath.
  Else, it sets *poNFurthest to NULL and returns
  CONFLICTING_PATH if the root's path is not a prefix of oPPath
  or MEMORY_ERROR if memory could not be allocated.
*/
static int FT_traversePath(Path_T oPPath, Node_T *poNFurthest) {
   /* status code from helper function calls */
   int iStatus;
   /* current prefix of oPPath being examined */
   Path_T oPPrefix = NULL;
   /* current node reached during traversal */
   Node_T oNCurr;
   /* child node found for the current prefix */
   Node_T oNChild = NULL;
   /* depth of the target path */
   size_t ulDepth;
   /* loop index over path depth levels */
   size_t i;
   /* index of matching child in current node's children */
   size_t ulChildID;

   assert(oPPath != NULL);
   assert(poNFurthest != NULL);

   if(oNRoot == NULL) {
      *poNFurthest = NULL;
      return SUCCESS;
   }

   iStatus = Path_prefix(oPPath, 1, &oPPrefix);
   if(iStatus != SUCCESS) {
      *poNFurthest = NULL;
      return iStatus;
   }

   if(Path_comparePath(Node_getPath(oNRoot), oPPrefix)) {
      Path_free(oPPrefix);
      *poNFurthest = NULL;
      return CONFLICTING_PATH;
   }
   Path_free(oPPrefix);
   oPPrefix = NULL;

   oNCurr = oNRoot;
   ulDepth = Path_getDepth(oPPath);

   for(i = 2; i <= ulDepth; i++) {
      /* files cannot have children, so traversal must stop */
      if(Node_isFile(oNCurr))
         break;

      iStatus = Path_prefix(oPPath, i, &oPPrefix);
      if(iStatus != SUCCESS) {
         *poNFurthest = NULL;
         return iStatus;
      }

      if(Node_hasChild(oNCurr, oPPrefix, &ulChildID)) {
         Path_free(oPPrefix);
         oPPrefix = NULL;

         iStatus = Node_getChild(oNCurr, ulChildID, &oNChild);
         if(iStatus != SUCCESS) {
            *poNFurthest = NULL;
            return iStatus;
         }
         oNCurr = oNChild;
      }
      else {
         break;
      }
   }

   Path_free(oPPrefix);
   *poNFurthest = oNCurr;
   return SUCCESS;
}

/*
  Traverses the FT to find a node with absolute path pcPath.
  Returns SUCCESS and sets *poNResult if found.

  Otherwise sets *poNResult to NULL and returns:
  * INITIALIZATION_ERROR
  * BAD_PATH
  * CONFLICTING_PATH
  * NO_SUCH_PATH
  * MEMORY_ERROR
*/
static int FT_findNode(const char *pcPath, Node_T *poNResult) {
   Path_T oPPath = NULL;
   Node_T oNFound = NULL;
   int iStatus;

   assert(pcPath != NULL);
   assert(poNResult != NULL);

   if(!bIsInitialized) {
      *poNResult = NULL;
      return INITIALIZATION_ERROR;
   }

   iStatus = Path_new(pcPath, &oPPath);
   if(iStatus != SUCCESS) {
      *poNResult = NULL;
      return iStatus;
   }

   iStatus = FT_traversePath(oPPath, &oNFound);
   if(iStatus != SUCCESS) {
      Path_free(oPPath);
      *poNResult = NULL;
      return iStatus;
   }

   if(oNFound == NULL) {
      Path_free(oPPath);
      *poNResult = NULL;
      return NO_SUCH_PATH;
   }

   if(Path_comparePath(Node_getPath(oNFound), oPPath) != 0) {
      Path_free(oPPath);
      *poNResult = NULL;
      return NO_SUCH_PATH;
   }

   Path_free(oPPath);
   *poNResult = oNFound;
   return SUCCESS;
}

int FT_insertDir(const char *pcPath) {
   int iStatus;
   Path_T oPPath = NULL;
   Node_T oNFirstNew = NULL;
   Node_T oNCurr = NULL;
   size_t ulDepth;
   size_t ulIndex;
   size_t ulNewNodes = 0;

   assert(pcPath != NULL);

   if(!bIsInitialized)
      return INITIALIZATION_ERROR;

   iStatus = Path_new(pcPath, &oPPath);
   if(iStatus != SUCCESS)
      return iStatus;

   iStatus = FT_traversePath(oPPath, &oNCurr);
   if(iStatus != SUCCESS) {
      Path_free(oPPath);
      return iStatus;
   }

   if(oNCurr == NULL && oNRoot != NULL) {
      Path_free(oPPath);
      return CONFLICTING_PATH;
   }

   ulDepth = Path_getDepth(oPPath);
   if(oNCurr == NULL)
      ulIndex = 1;
   else {
      ulIndex = Path_getDepth(Node_getPath(oNCurr)) + 1;

      if(ulIndex == ulDepth + 1 &&
         !Path_comparePath(oPPath, Node_getPath(oNCurr))) {
         Path_free(oPPath);
         return ALREADY_IN_TREE;
      }

      if(Node_isFile(oNCurr)) {
         Path_free(oPPath);
         return NOT_A_DIRECTORY;
      }
   }

   while(ulIndex <= ulDepth) {
      Path_T oPPrefix = NULL;
      Node_T oNNewNode = NULL;

      iStatus = Path_prefix(oPPath, ulIndex, &oPPrefix);
      if(iStatus != SUCCESS) {
         Path_free(oPPath);
         if(oNFirstNew != NULL)
            (void)Node_free(oNFirstNew);
         return iStatus;
      }

      iStatus = Node_new(oPPrefix, oNCurr, FALSE, NULL, 0, &oNNewNode);
      if(iStatus != SUCCESS) {
         Path_free(oPPath);
         Path_free(oPPrefix);
         if(oNFirstNew != NULL)
            (void)Node_free(oNFirstNew);
         return iStatus;
      }

      Path_free(oPPrefix);
      oNCurr = oNNewNode;
      ulNewNodes++;
      if(oNFirstNew == NULL)
         oNFirstNew = oNCurr;
      ulIndex++;
   }

   Path_free(oPPath);

   if(oNRoot == NULL)
      oNRoot = oNFirstNew;
   ulCount += ulNewNodes;

   return SUCCESS;
}

boolean FT_containsDir(const char *pcPath) {
   int iStatus;
   Node_T oNFound = NULL;

   assert(pcPath != NULL);

   iStatus = FT_findNode(pcPath, &oNFound);
   return (boolean)(iStatus == SUCCESS && Node_isDir(oNFound));
}

int FT_rmDir(const char *pcPath) {
   int iStatus;
   Node_T oNFound = NULL;

   assert(pcPath != NULL);

   iStatus = FT_findNode(pcPath, &oNFound);
   if(iStatus != SUCCESS)
      return iStatus;

   if(Node_isFile(oNFound))
      return NOT_A_DIRECTORY;

   ulCount -= Node_free(oNFound);
   if(ulCount == 0)
      oNRoot = NULL;

   return SUCCESS;
}

int FT_insertFile(const char *pcPath, void *pvContents,
   size_t ulLength) {
   int iStatus;
   Path_T oPPath = NULL;
   Node_T oNFirstNew = NULL;
   Node_T oNCurr = NULL;
   size_t ulDepth;
   size_t ulIndex;
   size_t ulNewNodes = 0;

   assert(pcPath != NULL);

   if(!bIsInitialized)
      return INITIALIZATION_ERROR;

   iStatus = Path_new(pcPath, &oPPath);
   if(iStatus != SUCCESS)
      return iStatus;

   ulDepth = Path_getDepth(oPPath);

   /* file may not be the root */
   if(ulDepth == 1) {
      Path_free(oPPath);
      return CONFLICTING_PATH;
   }

   iStatus = FT_traversePath(oPPath, &oNCurr);
   if(iStatus != SUCCESS) {
      Path_free(oPPath);
      return iStatus;
   }

   if(oNCurr == NULL && oNRoot != NULL) {
      Path_free(oPPath);
      return CONFLICTING_PATH;
   }

   if(oNCurr == NULL)
      ulIndex = 1;
   else {
      ulIndex = Path_getDepth(Node_getPath(oNCurr)) + 1;

      if(ulIndex == ulDepth + 1 &&
         !Path_comparePath(oPPath, Node_getPath(oNCurr))) {
         Path_free(oPPath);
         return ALREADY_IN_TREE;
      }

      if(Node_isFile(oNCurr)) {
         Path_free(oPPath);
         return NOT_A_DIRECTORY;
      }
   }

   while(ulIndex <= ulDepth) {
      Path_T oPPrefix = NULL;
      Node_T oNNewNode = NULL;
      boolean bIsFile;
      void *pvNodeContents;
      size_t ulNodeLength;

      if(ulIndex == ulDepth)
         bIsFile = TRUE;
      else
         bIsFile = FALSE;

      if(bIsFile) {
         pvNodeContents = pvContents;
         ulNodeLength = ulLength;
      }
      else {
         pvNodeContents = NULL;
         ulNodeLength = 0;
      }

      iStatus = Path_prefix(oPPath, ulIndex, &oPPrefix);
      if(iStatus != SUCCESS) {
         Path_free(oPPath);
         if(oNFirstNew != NULL)
            (void)Node_free(oNFirstNew);
         return iStatus;
      }

      iStatus = Node_new(oPPrefix, oNCurr, bIsFile,
                         pvNodeContents, ulNodeLength, &oNNewNode);
      if(iStatus != SUCCESS) {
         Path_free(oPPath);
         Path_free(oPPrefix);
         if(oNFirstNew != NULL)
            (void)Node_free(oNFirstNew);
         return iStatus;
      }

      Path_free(oPPrefix);
      oNCurr = oNNewNode;
      ulNewNodes++;
      if(oNFirstNew == NULL)
         oNFirstNew = oNCurr;
      ulIndex++;
   }

   Path_free(oPPath);

   if(oNRoot == NULL)
      oNRoot = oNFirstNew;

   ulCount += ulNewNodes;
   return SUCCESS;
}

boolean FT_containsFile(const char *pcPath) {
   int iStatus;
   Node_T oNFound = NULL;

   assert(pcPath != NULL);

   iStatus = FT_findNode(pcPath, &oNFound);
   return (boolean)(iStatus == SUCCESS && Node_isFile(oNFound));
}

int FT_rmFile(const char *pcPath) {
   int iStatus;
   Node_T oNFound = NULL;

   assert(pcPath != NULL);

   iStatus = FT_findNode(pcPath, &oNFound);
   if(iStatus != SUCCESS)
      return iStatus;

   if(Node_isDir(oNFound))
      return NOT_A_FILE;

   ulCount -= Node_free(oNFound);
   if(ulCount == 0)
      oNRoot = NULL;

   return SUCCESS;
}

void *FT_getFileContents(const char *pcPath) {
   int iStatus;
   Node_T oNFound = NULL;

   assert(pcPath != NULL);

   iStatus = FT_findNode(pcPath, &oNFound);
   if(iStatus != SUCCESS)
      return NULL;

   if(Node_isDir(oNFound))
      return NULL;

   return Node_getContents(oNFound);
}

void *FT_replaceFileContents(const char *pcPath, void *pvNewContents,
   size_t ulNewLength) {
   int iStatus;
   Node_T oNFound = NULL;

   assert(pcPath != NULL);

   iStatus = FT_findNode(pcPath, &oNFound);
   if(iStatus != SUCCESS)
      return NULL;

   if(Node_isDir(oNFound))
      return NULL;

   return Node_replaceContents(oNFound, pvNewContents, ulNewLength);
}

int FT_stat(const char *pcPath, boolean *pbIsFile, size_t *pulSize) {
   int iStatus;
   Node_T oNFound = NULL;

   assert(pcPath != NULL);
   assert(pbIsFile != NULL);
   assert(pulSize != NULL);

   iStatus = FT_findNode(pcPath, &oNFound);
   if(iStatus != SUCCESS)
      return iStatus;

   if(Node_isFile(oNFound)) {
      *pbIsFile = TRUE;
      *pulSize = Node_getLength(oNFound);
   }
   else {
      *pbIsFile = FALSE;
   }

   return SUCCESS;
}

int FT_init(void) {

   if(bIsInitialized)
      return INITIALIZATION_ERROR;

   bIsInitialized = TRUE;
   oNRoot = NULL;
   ulCount = 0;

   return SUCCESS;
}

int FT_destroy(void) {
   if(!bIsInitialized)
      return INITIALIZATION_ERROR;

   if(oNRoot != NULL) {
      ulCount -= Node_free(oNRoot);
      oNRoot = NULL;
   }

   bIsInitialized = FALSE;

   return SUCCESS;
}

/*
   Performs a pre-order traversal of the FT rooted at oNNode, inserting
  each visited node into DynArray_T oDNodes beginning at index
  ulNextIndex. Returns the next unused index in oDNodes after all
  insertions.
*/
static size_t FT_preOrderTraversal(Node_T oNNode, DynArray_T oDNodes,
   size_t ulNextIndex) {
   size_t ulChildIndex;
   size_t ulNumChildren;

   assert(oDNodes != NULL);

   if(oNNode != NULL) {
      Node_T oNChild = NULL;
      int iStatus;

      (void)DynArray_set(oDNodes, ulNextIndex, oNNode);
      ulNextIndex++;

      if(Node_isFile(oNNode))
         return ulNextIndex;

      ulNumChildren = Node_getNumChildren(oNNode);

      /* visit children in stored lexicographic order */
      for(ulChildIndex = 0; ulChildIndex < ulNumChildren;
         ulChildIndex++) {
         iStatus = Node_getChild(oNNode, ulChildIndex, &oNChild);
         assert(iStatus == SUCCESS);
         ulNextIndex = FT_preOrderTraversal(oNChild, oDNodes,
            ulNextIndex);
      }
   }

   return ulNextIndex;
}

/*
  Alternate version of strlen that uses pulAcc as a parameter
  to accumulate a string length rather than returning the length of oNNode,
  and always adds one additional byte to the sum.
*/
static void FT_strlenAccumulate(Node_T oNNode, size_t *pulAcc) {
   assert(pulAcc != NULL);

   if(oNNode != NULL)
      *pulAcc += (Path_getStrLength(Node_getPath(oNNode)) + 1);
}

/*
  Alternate version of strcat that appends oNNode's path onto pcAcc
  and always appends a newline.
*/
static void FT_strcatAccumulate(Node_T oNNode, char *pcAcc) {
   assert(pcAcc != NULL);

   if(oNNode != NULL) {
      strcat(pcAcc, Path_getPathname(Node_getPath(oNNode)));
      strcat(pcAcc, "\n");
   }
}

char *FT_toString(void) {
   DynArray_T nodes;
   size_t totalStrlen = 1;
   char *result = NULL;

   if(!bIsInitialized)
      return NULL;

   nodes = DynArray_new(ulCount);
   if(nodes == NULL)
      return NULL;

   (void)FT_preOrderTraversal(oNRoot, nodes, 0);

   DynArray_map(nodes, (void (*)(void *, void *))FT_strlenAccumulate,
   (void *)&totalStrlen);

   result = malloc(totalStrlen);
   if(result == NULL) {
      DynArray_free(nodes);
      return NULL;
   }
   *result = '\0';

   DynArray_map(nodes, (void (*)(void *, void *))FT_strcatAccumulate,
   (void *)result);

   DynArray_free(nodes);
   return result;
}