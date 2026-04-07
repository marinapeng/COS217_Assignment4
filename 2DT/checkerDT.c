/*--------------------------------------------------------------------*/
/* checkerDT.c                                                        */
/* Authors: Marina Peng and Allen Chen                                */
/*--------------------------------------------------------------------*/

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "checkerDT.h"
#include "dynarray.h"
#include "path.h"

/* Helper function that checks child-related invariants for a node oNNode.
Return TRUE if each child is non-NULL, that each child's parent
pointer correctly points back to this node, that there are no
duplicate children, and that children are in lexicographic order.
Return FALSE otherwise. */
static boolean CheckerDT_Node_childrenCheck(Node_T oNNode) {
   /* index variable for iterating through children */
   size_t index;
   /* number of children of this node */
   size_t numChildren;
   /* previous child's path */
   Path_T prevPath = NULL;

   numChildren = Node_getNumChildren(oNNode);

   for(index = 0; index < numChildren; index++) {
      /* check status of retrieving children*/
      int status;
      /* child node*/
      Node_T child = NULL;
      /* child's path */
      Path_T childPath = NULL;

      /* retrieve child stored at the index. if the retrieval fails,
      then there is an error in the node's children structure */
      status = Node_getChild(oNNode, index, &child);
      if(status != SUCCESS) {
         fprintf(stderr, "cannot retrieve child %lu of node\n",
                 (unsigned long)index);
         return FALSE;
      }

      /* check that child or its path is not null */
      if(child == NULL || Node_getPath(child) == NULL) {
         fprintf(stderr, "A node has a NULL child or NULL child path \n");
         return FALSE;
      }

      childPath = Node_getPath(child);

      /* check that the node and child are not the same */
      if(child == oNNode) {
         fprintf(stderr, "A node is its own child\n");
         return FALSE;
      }

      /* check that the child's parent is the node */
      if(Node_getParent(child) != oNNode) {
         fprintf(stderr, "A child's parent pointer does not point back to this node\n");
         return FALSE;
      }

      /* if there are more than 1 children:
      starting with the second child, compare each child to the one
      immediately before it to check ordering and duplicates */
      if(index > 0) {
         /* check if there are duplicate children (children have same paths)*/
         if(Path_comparePath(prevPath, childPath) == 0) {
            fprintf(stderr, "A node has duplicate children\n");
            return FALSE;
         }

         /* check that children are in lexicographic order */
         if(Path_comparePath(prevPath, childPath) > 0) {
            fprintf(stderr, "Children are not in lexicographic order\n");
            return FALSE;
         }
      }

      prevPath = childPath;
   }

   return TRUE;
}

/* Check parent-child path relationship invariants for a node by 
returning TRUE if the parent oNParent is exactly one level shallower 
and if the parent path oPPPath is exactly the prefix of the child 
path oPNPath and FALSE otherwise */
static boolean CheckerDT_Node_parentPathCheck(Node_T oNParent,
   Path_T oPNPath, Path_T oPPPath) {

   if(oNParent != NULL) {
      if(Path_getDepth(oPPPath) != Path_getDepth(oPNPath) - 1) {
         fprintf(stderr, "Parent is not exactly one level above child: (%s) (%s)\n",
                 Path_getPathname(oPPPath), Path_getPathname(oPNPath));
         return FALSE;
      }

      if(Path_getSharedPrefixDepth(oPNPath, oPPPath) != Path_getDepth(oPPPath)) {
         fprintf(stderr, "P-C nodes don't have P-C paths: (%s) (%s)\n",
                 Path_getPathname(oPPPath), Path_getPathname(oPNPath));
         return FALSE;
      }
   }

   return TRUE;
}

/* see checkerDT.h for specification */
boolean CheckerDT_Node_isValid(Node_T oNNode) {
   /* node's parent node */
   Node_T oNParent;
   /* node's path */
   Path_T oPNPath;
   /* node's parent's path */
   Path_T oPPPath = NULL;

   /* Sample check: a NULL pointer is not a valid node */
   if(oNNode == NULL) {
      fprintf(stderr, "A node is a NULL pointer\n");
      return FALSE;
   }

   oNParent = Node_getParent(oNNode);
   oPNPath = Node_getPath(oNNode);

   /* Check that a node's path is not NULL */
   if(oPNPath == NULL) {
      fprintf(stderr, "A node has NULL path\n");
      return FALSE;
   }

   /* Check that a node cannot be its own parent */
   if(oNNode == oNParent) {
      fprintf(stderr, "A node cannot be its own parent\n");
      return FALSE;
   }

   /* Check that the node'a parent does not have a NULL path */
   if(oNParent != NULL) {
      oPPPath = Node_getPath(oNParent);
      if(oPPPath == NULL) {
         fprintf(stderr, "A node's parent has NULL path\n");
         return FALSE;
      }
   }

   if(!CheckerDT_Node_childrenCheck(oNNode))
      return FALSE;

   if(!CheckerDT_Node_parentPathCheck(oNParent, oPNPath, oPPPath))
      return FALSE;

   return TRUE;
}

/*
   Performs a pre-order traversal of the tree rooted at oNNode.
   Increments *pulCount once for each reachable node visited.
   Returns FALSE if a broken invariant is found and
   returns TRUE otherwise.
*/
static boolean CheckerDT_treeCheck(Node_T oNNode, size_t *pulCount) {
   size_t ulIndex;

   assert (pulCount != NULL);

   if(oNNode!= NULL) {
      (*pulCount)++;
      
      /* Sample check on each node: node must be valid */
      /* If not, pass that failure back up immediately */
      if(!CheckerDT_Node_isValid(oNNode))
         return FALSE;

      /* Recur on every child of oNNode */
      for(ulIndex = 0; ulIndex < Node_getNumChildren(oNNode); ulIndex++)
      {
         Node_T oNChild = NULL;
         int iStatus = Node_getChild(oNNode, ulIndex, &oNChild);

         if(iStatus != SUCCESS) {
            fprintf(stderr, "getNumChildren claims more children than getChild returns\n");
            return FALSE;
         }

         /* if recurring down one subtree results in a failed check
            farther down, passes the failure back up immediately */
         if(!CheckerDT_treeCheck(oNChild, pulCount))
            return FALSE;
      }
   }
   return TRUE;
}

/* see checkerDT.h for specification */
boolean CheckerDT_isValid(boolean bIsInitialized, Node_T oNRoot,
                          size_t ulCount) {
   
   /* variable for number of nodes passed when traversing through tree*/
   size_t numSeenNodes;

   /* Sample check on a top-level data structure invariant:
      if the DT is not initialized, its count should be 0. */
   if(!bIsInitialized) {
      if(ulCount != 0) {
         fprintf(stderr, "Not initialized, but count is not 0\n");
         return FALSE;
      }
      if (oNRoot != NULL) {
         fprintf(stderr, "Not initialized, but root is not null\n");
         return FALSE; 
      }
      return TRUE;
   }

   /* If the root is not NULL, check that count is not 0
   and check that the root does not have a parent*/
   if (oNRoot !=NULL ) {
      if (ulCount == 0 ) {
         fprintf(stderr, "non-NULL root, but count is 0\n");
         return FALSE;
      }
      if (Node_getParent(oNRoot) != NULL) {
         fprintf(stderr, "root cannot have a parent \n");
         return FALSE;
      }
   }

   /* If the root is NULL, check that count is 0*/
   if (oNRoot == NULL) {
      if (ulCount != 0) {
         fprintf(stderr, "NULL root, but count is not 0\n");
         return FALSE;
      }
   }

   /* Now checks invariants recursively at each node from the root. */
    numSeenNodes = 0;
   if (!(CheckerDT_treeCheck(oNRoot, &numSeenNodes))) {
      return FALSE;
   }

   /* checks that number of reachable nodes equals ulCount */
   if(numSeenNodes != ulCount) {
      fprintf(stderr, "Reachable node count does not match ulCount\n");
      return FALSE;
   }

   return TRUE;
}