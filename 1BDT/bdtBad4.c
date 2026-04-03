/*--------------------------------------------------------------------*/
/* bdt.c (4th bad version)                                            */
/* Author: Christopher Moretti                                        */
/*--------------------------------------------------------------------*/

#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "dynarray.h"
#include "path.h"
#include "bdt.h"


/* A node in a BDT */
struct node {
   /* the object corresponding to the node's absolute path */
   Path_T oPPath;
   /* this node's parent */
   struct node *psParent;
   /* this node's children */
   struct node *psChild1, *psChild2;
};

/*
  A Directory Tree is a representation of a hierarchy of directories,
  represented as an AO with 3 state variables:
*/

/* 1. a flag for being in an initialized state (TRUE) or not (FALSE) */
static boolean bIsInitialized;
/* 2. a pointer to the root node in the hierarchy */
static struct node *psRoot;
/* 3. a counter of the number of nodes in the hierarchy */
static size_t ulCount;

/*
  Links new child psChild from psParent. If psParent already has
  two children, returns CONFLICTING_PATH, otherwise returns SUCCESS.
*/
static int BDT_addChild(struct node *psParent, struct node *psChild) {
   assert(psParent != NULL);
   assert(psChild != NULL);

   if(!psParent->psChild1)
      psParent->psChild1 = psChild;
   else if(!psParent->psChild2)
      psParent->psChild2 = psChild;
   else
      return CONFLICTING_PATH;

   return SUCCESS;
}

/*
  Compares the string representation of psfirst with a string
  pcSecond representing a node's path.
  Returns <0, 0, or >0 if psFirst is "less than", "equal to", or
  "greater than" pcSecond, respectively.
*/
static int BDT_compareNodeToString(const struct node *psFirst,
                                 const char *pcSecond) {
   assert(psFirst != NULL);
   assert(pcSecond != NULL);

   return Path_compareString(psFirst->oPPath, pcSecond);
}

/* Returns the number of children that psParent has. */
static size_t BDT_getNumChildren(struct node *psParent) {
   assert(psParent != NULL);

   return (size_t) (2 - (!psParent->psChild1 + !psParent->psChild2));
}

/*
  Returns TRUE if psParent has a child with path oPPath. Returns
  FALSE if it does not.

  If psParent has such a child, stores in *pulChildID the child's
  identifier (as used in BDT_getChild). If psParent does not have
  such a child, stores in *pulChildID the identifier that such a
  child _would_ have if inserted.
*/
static boolean BDT_hasChild(struct node *psParent, Path_T oPPath,
                         size_t *pulChildID) {
   assert(psParent != NULL);
   assert(oPPath != NULL);
   assert(pulChildID != NULL);

   if(!psParent->psChild1) {
      *pulChildID = 0;
      return FALSE;
   }
   else if(!BDT_compareNodeToString(psParent->psChild1,
                               Path_getPathname(oPPath))) {
      *pulChildID = 0;
      return TRUE;
   }
   else if(!psParent->psChild2) {
      *pulChildID = 1;
      return FALSE;
   }
   else if(!BDT_compareNodeToString(psParent->psChild2,
                               Path_getPathname(oPPath))) {
      *pulChildID = 1;
      return TRUE;
   }
   else {
      *pulChildID = 2;
      return FALSE;
   }
}


/*
  Creates a new node with path oPPath and parent psParent.  Returns an
  int SUCCESS status and sets *ppsResult to be the new node if
  successful. Otherwise, sets *ppsResult to NULL and returns status:
  * MEMORY_ERROR if memory could not be allocated to complete request
  * CONFLICTING_PATH if psParent's path is not an ancestor of oPPath
                     or if psParent already has two children
  * NO_SUCH_PATH if oPPath is of depth 0
                 or psParent's path is not oPPath's direct parent
                 or psParent is NULL but oPPath is not of depth 1
  * ALREADY_IN_TREE if psParent already has a child with this path
*/
static int BDT_newNode(Path_T oPPath, struct node *psParent, struct node **ppsResult) {
   struct node *psNew;
   Path_T oPParentPath = NULL;
   Path_T oPNewPath = NULL;
   size_t ulParentDepth;
   size_t ulIndex;
   int iStatus;

   assert(oPPath != NULL);
   assert(ppsResult != NULL);
   /* psParent may be NULL -- when the new node is the root */

   /* allocate space for a new node */
   psNew = malloc(sizeof(struct node));
   if(psNew == NULL) {
      *ppsResult = NULL;
      return MEMORY_ERROR;
   }

   /* set the new node's path */
   iStatus = Path_dup(oPPath, &oPNewPath);
   if(iStatus != SUCCESS) {
      free(psNew);
      *ppsResult = NULL;
      return iStatus;
   }
   psNew->oPPath = oPNewPath;

   /* validate and set the new node's parent */
   if(psParent != NULL) {
      size_t ulSharedDepth;

      oPParentPath = psParent->oPPath;
      ulParentDepth = Path_getDepth(oPParentPath);
      ulSharedDepth = Path_getSharedPrefixDepth(psNew->oPPath,
                                                oPParentPath);
      /* parent must be an ancestor of child */
      if(ulSharedDepth < ulParentDepth) {
         Path_free(psNew->oPPath);
         free(psNew);
         *ppsResult = NULL;
         return CONFLICTING_PATH;
      }

      /* parent must be exactly one level up from child */
      if(Path_getDepth(psNew->oPPath) != ulParentDepth + 1) {
         Path_free(psNew->oPPath);
         free(psNew);
         *ppsResult = NULL;
         return NO_SUCH_PATH;
      }

      /* parent must not already have child with this path */
      if(BDT_hasChild(psParent, oPPath, &ulIndex)) {
         Path_free(psNew->oPPath);
         free(psNew);
         *ppsResult = NULL;
         return ALREADY_IN_TREE;
      }

      /* parent must not already have two children */
      if(BDT_getNumChildren(psParent) == 2) {
         Path_free(psNew->oPPath);
         free(psNew);
         *ppsResult = NULL;
         return CONFLICTING_PATH;
      }
   }
   else {
      /* new node must be root */
      /* can only create one "level" at a time */
      if(Path_getDepth(psNew->oPPath) != 1) {
         Path_free(psNew->oPPath);
         free(psNew);
         *ppsResult = NULL;
         return NO_SUCH_PATH;
      }
   }
   psNew->psParent = psParent;

   /* set the new node's children */
   psNew->psChild1 = NULL;
   psNew->psChild2 = NULL;

   /* Link into parent's child link */
   if(psParent != NULL) {
      iStatus = BDT_addChild(psParent, psNew);
      if(iStatus != SUCCESS) {
         Path_free(psNew->oPPath);
         free(psNew);
         *ppsResult = NULL;
         return iStatus;
      }
   }

   *ppsResult = psNew;
   return SUCCESS;
}

/*
  Destroys and frees all memory allocated for the subtree rooted at
  psNode, i.e., deletes this node and all its descendents. Returns the
  number of nodes deleted.
*/
static size_t BDT_freeSubtree(struct node *psNode) {
   size_t ulCount = 0;

   assert(psNode != NULL);

   /* remove from parent's list */
   if(psNode->psParent != NULL) {
      if(psNode->psParent->psChild1 == psNode) {
         psNode->psParent->psChild1 = psNode->psParent->psChild2;
         psNode->psParent->psChild2 = NULL;
      }
      else if(psNode->psParent->psChild2 == psNode) {
         psNode->psParent->psChild2 = NULL;
      }
      else {
         assert(FALSE);
      }
   }

   /* recursively remove children */
   if(psNode->psChild2)
      ulCount += BDT_freeSubtree(psNode->psChild2);
   if(psNode->psChild1)
      ulCount += BDT_freeSubtree(psNode->psChild1);

   /* remove path */
   Path_free(psNode->oPPath);

   /* finally, free the struct node */
   free(psNode);
   ulCount++;
   return ulCount;
}

/* Returns the path object representing psNode's absolute path. */
static Path_T BDT_getPath(struct node *psNode) {
   assert(psNode != NULL);

   return psNode->oPPath;
}


/*
  Returns an int SUCCESS status and sets *ppsResult to be the child
  node of psParent with identifier ulChildID, if one exists.
  Otherwise, sets *ppsResult to NULL and returns status:
  * NO_SUCH_PATH if ulChildID is not a valid child for psParent
*/
static int  BDT_getChild(struct node *psParent, size_t ulChildID,
                   struct node **ppsResult) {

   assert(psParent != NULL);
   assert(ppsResult != NULL);

   if(ulChildID == 0) {
      *ppsResult = psParent->psChild1;
      if(*ppsResult) return SUCCESS;
      else return NO_SUCH_PATH;
   }
   else if(ulChildID == 1) {
      *ppsResult = psParent->psChild2;
      if(*ppsResult) return SUCCESS;
      else return NO_SUCH_PATH;
   }
   else {
      *ppsResult = NULL;
      return NO_SUCH_PATH;
   }
}

/* --------------------------------------------------------------------

  The BDT_traversePath and BDT_findNode functions modularize the common
  functionality of going as far as possible down an BDT towards a path
  and returning either the node of however far was reached or the
  node if the full path was reached, respectively.
*/

/*
  Traverses the BDT starting at the root as far as possible towards
  absolute path oPPath. If able to traverse, returns an int SUCCESS
  status and sets *ppsFurthest to the furthest node reached (which may
  be only a prefix of oPPath, or even NULL if the root is NULL).
  Otherwise, sets *ppsFurthest to NULL and returns with status:
  * CONFLICTING_PATH if the root's path is not a prefix of oPPath
  * MEMORY_ERROR if memory could not be allocated to complete request
*/
static int BDT_traversePath(Path_T oPPath, struct node **ppsFurthest) {
   int iStatus;
   Path_T oPPrefix = NULL;
   struct node *psCurr;
   struct node *psChild = NULL;
   size_t ulDepth;
   size_t i;
   size_t ulChildID;

   assert(oPPath != NULL);
   assert(ppsFurthest != NULL);

   /* root is NULL -> won't find anything */
   if(psRoot == NULL) {
      *ppsFurthest = NULL;
      return SUCCESS;
   }

   iStatus = Path_prefix(oPPath, 1, &oPPrefix);
   if(iStatus != SUCCESS) {
      *ppsFurthest = NULL;
      return iStatus;
   }

   if(Path_comparePath(BDT_getPath(psRoot), oPPrefix)) {
      Path_free(oPPrefix);
      *ppsFurthest = NULL;
      return CONFLICTING_PATH;
   }
   Path_free(oPPrefix);
   oPPrefix = NULL;

   psCurr = psRoot;
   ulDepth = Path_getDepth(oPPath);
   for(i = 2; i <= ulDepth; i++) {
      iStatus = Path_prefix(oPPath, i, &oPPrefix);
      if(iStatus != SUCCESS) {
         *ppsFurthest = NULL;
         return iStatus;
      }
      if(BDT_hasChild(psCurr, oPPrefix, &ulChildID)) {
         /* go to that child and continue with next prefix */
         Path_free(oPPrefix);
         oPPrefix = NULL;
         iStatus = BDT_getChild(psCurr, ulChildID, &psChild);
         if(iStatus != SUCCESS) {
            *ppsFurthest = NULL;
            return iStatus;
         }
         psCurr = psChild;
      }
      else {
         /* psCurr doesn't have child with path oPPrefix:
            this is as far as we can go */
         break;
      }
   }

   Path_free(oPPrefix);
   *ppsFurthest = psCurr;
   return SUCCESS;
}

/*
  Traverses the BDT to find a node with absolute path pcPath. Returns a
  int SUCCESS status and sets *ppsResult to be the node, if found.
  Otherwise, sets *ppsResult to NULL and returns with status:
  * INITIALIZATION_ERROR if the BDT is not in an initialized state
  * BAD_PATH if pcPath does not represent a well-formatted path
  * CONFLICTING_PATH if the root's path is not a prefix of pcPath
  * NO_SUCH_PATH if no node with pcPath exists in the hierarchy
  * MEMORY_ERROR if memory could not be allocated to complete request
 */
static int BDT_findNode(const char *pcPath, struct node **ppsResult) {
   Path_T oPPath = NULL;
   struct node *psFound = NULL;
   int iStatus;

   assert(pcPath != NULL);
   assert(ppsResult != NULL);

   if(!bIsInitialized) {
      *ppsResult = NULL;
      return INITIALIZATION_ERROR;
   }

   iStatus = Path_new(pcPath, &oPPath);
   if(iStatus != SUCCESS) {
      *ppsResult = NULL;
      return iStatus;
   }

   iStatus = BDT_traversePath(oPPath, &psFound);
   if(iStatus != SUCCESS)
   {
      Path_free(oPPath);
      *ppsResult = NULL;
      return iStatus;
   }

   if(psFound == NULL) {
      Path_free(oPPath);
      *ppsResult = NULL;
      return NO_SUCH_PATH;
   }

   if(Path_comparePath(BDT_getPath(psFound), oPPath) != 0) {
      Path_free(oPPath);
      *ppsResult = NULL;
      return NO_SUCH_PATH;
   }

   Path_free(oPPath);
   *ppsResult = psFound;
   return SUCCESS;
}
/*--------------------------------------------------------------------*/


int BDT_insert(const char *pcPath) {
   int iStatus;
   Path_T oPPath = NULL;
   struct node *psFirstNew = NULL;
   struct node *psCurr = NULL;
   size_t ulDepth, ulIndex;
   size_t ulNewNodes = 0;

   assert(pcPath != NULL);

   /* validate pcPath and generate a Path_T for it */
   if(!bIsInitialized)
      return INITIALIZATION_ERROR;

   iStatus = Path_new(pcPath, &oPPath);
   if(iStatus != SUCCESS)
      return iStatus;

   /* find the closest ancestor of oPPath already in the tree */
   iStatus= BDT_traversePath(oPPath, &psCurr);
   if(iStatus != SUCCESS)
   {
      Path_free(oPPath);
      return iStatus;
   }

   /* no ancestor node found, so if root is not NULL,
      pcPath isn't underneath root. */
   if(psCurr == NULL && psRoot != NULL) {
      Path_free(oPPath);
      return CONFLICTING_PATH;
   }

   ulDepth = Path_getDepth(oPPath);
   if(psCurr == NULL) /* new root! */
      ulIndex = 1;
   else {
      ulIndex = Path_getDepth(BDT_getPath(psCurr))+1;

      /* psCurr is the node we're trying to insert */
      if(ulIndex == ulDepth+1 && !Path_comparePath(oPPath,
                                       BDT_getPath(psCurr))) {
         Path_free(oPPath);
         return ALREADY_IN_TREE;
      }
   }

   /* starting at psCurr, build rest of the path one level at a time */
   while(ulIndex <= ulDepth) {
      Path_T oPPrefix = NULL;
      struct node *psNewNode = NULL;

      /* generate a Path_T for this level */
      iStatus = Path_prefix(oPPath, ulIndex, &oPPrefix);
      if(iStatus != SUCCESS) {
         Path_free(oPPath);
         if(psFirstNew != NULL)
            (void) BDT_freeSubtree(psFirstNew);
         return iStatus;
      }

      /* insert the new node for this level */
      iStatus = BDT_newNode(oPPrefix, psCurr, &psNewNode);
      if(iStatus != SUCCESS) {
         Path_free(oPPath);
         Path_free(oPPrefix);
         if(psFirstNew != NULL)
            (void) BDT_freeSubtree(psFirstNew);
         return iStatus;
      }

      /* set up for next level */
      Path_free(oPPrefix);
      psCurr = psNewNode;
      ulNewNodes++;
      if(psFirstNew == NULL)
         psFirstNew = psCurr;
      ulIndex++;
   }

   Path_free(oPPath);
   /* update BDT state variables to reflect insertion */
   if(psRoot == NULL)
      psRoot = psFirstNew;
   ulCount += ulNewNodes;

   return SUCCESS;
}

boolean BDT_contains(const char *pcPath) {
   int iStatus;
   struct node *psFound = NULL;

   assert(pcPath != NULL);

   iStatus = BDT_findNode(pcPath, &psFound);
   return (boolean) (iStatus == SUCCESS);
}


int BDT_rm(const char *pcPath) {
   int iStatus;
   struct node *psFound = NULL;

   assert(pcPath != NULL);

   iStatus = BDT_findNode(pcPath, &psFound);

   if(iStatus != SUCCESS)
       return iStatus;

   ulCount -= BDT_freeSubtree(psFound);
   if(ulCount == 0)
      psRoot = NULL;

   return SUCCESS;
}

int BDT_init(void) {

   if(bIsInitialized)
      return INITIALIZATION_ERROR;

   bIsInitialized = TRUE;
   psRoot = NULL;
   ulCount = 0;

   return SUCCESS;
}

int BDT_destroy(void) {


   if(!bIsInitialized)
      return INITIALIZATION_ERROR;

   if(psRoot) {
      ulCount -= BDT_freeSubtree(psRoot);
      psRoot = NULL;
   }

   bIsInitialized = FALSE;

   return SUCCESS;
}


/* --------------------------------------------------------------------

  The following auxiliary functions are used for generating the
  string representation of the BDT.
*/

/*
  Performs a pre-order traversal of the tree rooted at psNode,
  inserting each payload into oDNodes beginning at index ulIndex.
  Returns the next unused index in d after the insertion(s).
*/
static size_t BDT_preOrderTraversal(struct node *psNode,
                                    DynArray_T oDNodes,
                                    size_t ulIndex) {
   size_t c;;

   assert(oDNodes != NULL);
   /* psNode may be NULL, e.g. recursive call from leaf, guard below */

   if(psNode != NULL) {
      (void) DynArray_set(oDNodes, ulIndex, psNode);
      ulIndex++;
      for(c = 0; c < BDT_getNumChildren(psNode); c++) {
         int iStatus;
         struct node *psChild = NULL;
         iStatus = BDT_getChild(psNode, c, &psChild);
         assert(iStatus == SUCCESS);
         ulIndex = BDT_preOrderTraversal(psChild, oDNodes, ulIndex);
      }
   }
   return ulIndex;
}

/*
  Alternate version of strlen that uses pulAcc as an in-out parameter
  to accumulate a string length, rather than returning the length of
  psNode's path, and also always adds one addition byte to the sum.
*/
static void BDT_strlenAccumulate(struct node *psNode, size_t *pulAcc) {
   assert(pulAcc != NULL);
   /* psNode can be NULL -- guarded below */

   if(psNode != NULL)
      *pulAcc += (Path_getStrLength(BDT_getPath(psNode)));
}

/*
  Alternate version of strcat that inverts the typical argument
  order, appending psNode's path onto pcAcc, and also always adds one
  newline at the end of the concatenated string.
*/
static void BDT_strcatAccumulate(struct node *psNode, char *pcAcc) {
   assert(pcAcc != NULL);
   /* psNode can be NULL -- guarded below */

   if(psNode != NULL) {
      strcat(pcAcc, Path_getPathname(BDT_getPath(psNode)));
      strcat(pcAcc, "\n");
   }
}
/*--------------------------------------------------------------------*/

char *BDT_toString(void) {
   DynArray_T nodes;
   size_t totalStrlen = 1;
   char *result = NULL;

   if(!bIsInitialized)
      return NULL;

   nodes = DynArray_new(ulCount);
   (void) BDT_preOrderTraversal(psRoot, nodes, 0);

   DynArray_map(nodes, (void (*)(void *, void*)) BDT_strlenAccumulate,
                (void*) &totalStrlen);

   result = malloc(totalStrlen);
   if(result == NULL) {
      DynArray_free(nodes);
      return NULL;
   }
   *result = '\0';

   DynArray_map(nodes, (void (*)(void *, void*)) BDT_strcatAccumulate,
                (void *) result);

   DynArray_free(nodes);

   return result;
}
