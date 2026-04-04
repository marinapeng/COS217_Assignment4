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

/* see checkerDT.h for specification */
boolean CheckerDT_Node_isValid(Node_T oNNode) {
   /* node's parent node */
   Node_T oNParent;
   /* node's path */
   Path_T oPNPath;
   /* node's parent's path */
   Path_T oPPPath = NULL;
   /* index variable for iterating through children */
   size_t index;

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

   /* checker for node's children */
   /* Check that each child is non-NULL, that each child's parent
   pointer correctly points back to this node, that there are no
   duplicate children, and that children are in lexicographic order. */
   for(index = 0; index < Node_getNumChildren(oNNode); index++) {
      /* check status of retrieving children*/
      int status;
      /* child node*/
      Node_T child = NULL;
      /* child's path */
      Path_T childPath;

      /* retrieve child stored at the index. if the retrieval fails,
      then there is an error in the node's children structure */
      status = Node_getChild(oNNode, index, &child);
      if(status != SUCCESS) {
         fprintf(stderr, "cannot retrieve child %lu of node\n", index);
         return FALSE;
      }

      /* check that child is not null */
      if(child == NULL) {
         fprintf(stderr, "A node has a NULL child\n");
         return FALSE;
      }

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

      /* check that the child's path is not NULL */
      childPath = Node_getPath(child);
      if(childPath == NULL) {
         fprintf(stderr, "A child has NULL path\n");
         return FALSE;
      }

      /* if there are more than 1 children:
      starting with the second child, compare each child to the one
      immediately before it to check ordering and duplicates */
      if(index > 0) {
         Node_T prevChild = NULL;
         Path_T prevPath;

         /* retrieve child stored at the previous index. if the retrieval
         fails, then there is an error in the node's children structure*/
         status = Node_getChild(oNNode, index - 1, &prevChild);
         if(status != SUCCESS) {
            fprintf(stderr, "Could not retrieve previous child %lu of node\n", index - 1);
            return FALSE;
         }

         /* check that the previous child is not null */
         if(prevChild == NULL) {
            fprintf(stderr, "A node has a NULL previous child\n");
            return FALSE;
         }

         /* check that the previous child's path is not null */
         prevPath = Node_getPath(prevChild);
         if(prevPath == NULL) {
            fprintf(stderr, "A previous child has NULL path\n");
            return FALSE;
         }

         /* check if there are duplicate children (children have same paths)*/
         if(Path_comparePath(prevPath, childPath) == 0) {
            fprintf(stderr, "A node has duplicate children\n");
            return FALSE;
         }

         /* check that children are ordered in lexicographic order */
         if(Path_comparePath(prevPath, childPath) > 0) {
            fprintf(stderr, "Children are not in lexicographic order\n");
            return FALSE;
         }
      }
   }

   /* Checks if the parent is exactly one level shallower
   and if the parent path is exactly the prefix of the child path */
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

/*
   Performs a pre-order traversal of the tree rooted at oNNode.
   Returns FALSE if a broken invariant is found and
   returns TRUE otherwise.

   You may want to change this function's return type or
   parameter list to facilitate constructing your checks.
   If you do, you should update this function comment.
*/
static boolean CheckerDT_treeCheck(Node_T oNNode) {
   size_t ulIndex;

   if(oNNode!= NULL) {

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
         if(!CheckerDT_treeCheck(oNChild))
            return FALSE;
      }
   }
   return TRUE;
}

/* Checks if a node is a root - parent node is null */
static boolean CheckerDT_isRoot(Node_T oNNode, Node_T oNRoot) {
   if(oNNode == NULL) {
      return FALSE;
   }
   return (boolean)(oNNode == oNRoot && Node_getParent(oNNode) == NULL);
}

/* see checkerDT.h for specification */
boolean CheckerDT_isValid(boolean bIsInitialized, Node_T oNRoot,
                          size_t ulCount) {

   /* Sample check on a top-level data structure invariant:
      if the DT is not initialized, its count should be 0. */
   if(!bIsInitialized)
      if(ulCount != 0) {
         fprintf(stderr, "Not initialized, but count is not 0\n");
         return FALSE;
      }

   /* Checks that if the node is the root, it does not have a parent*/
   if (oNRoot != NULL && !CheckerDT_isRoot(oNRoot, oNRoot)) {
      fprintf(stderr, "Invariant violated: root has a parent -> invalid\n");
      return FALSE;
   }

   /* Now checks invariants recursively at each node from the root. */
   return CheckerDT_treeCheck(oNRoot);
}



