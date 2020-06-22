/*
 *
 *  sptree.c:  The following code implements the basic operations on
 *  an event-set or priority-queue implemented using splay trees:
 *
 *  SYMBOL *splook( key, *tree, flag )	Find (/make if flag) node with key.
 *  void    spscan( tree, function  )	Apply function to each node.
 *
 *  The implementation used here is based on the implementation
 *  which was used in the tests of splay trees reported in:
 *
 *    An Empirical Comparison of Priority-Queue and Event-Set Implementations,
 *	by Douglas W. Jones, Comm. ACM 29, 4 (Apr. 1986) 300-311.
 *
 *  The basic splay tree algorithms were originally
 *  presented in:
 *
 *	Self Adjusting Binary Trees,
 *		by D. D. Sleator and R. E. Tarjan,
 *			Proc. ACM SIGACT Symposium on Theory
 *			of Computing (Boston, Apr 1983) 235-245.
 *
 *  base on routines Written by:
 *    Douglas W. Jones
 *
 *  Translated to C by:
 *    David Brower, daveb@rtech.uucp
 *
 */

#ifdef dll
#include "dllfile.h"
#endif
# include "sload.h"
#include "symb.h"
#include "sptree.h"

/*----------------
 *
 *  splook() -- insert item in a tree, unless its already there.
 *
 *  Find (or create if flag true) a node n in tree q with key value key; when
 *  done, n will be the root of the splay tree represented by q, all nodes
 *  in q with keys less than or equal to that of n will be in the
 *  left subtree, all with greater keys will be in the right subtree;
 *  the tree is split into these subtrees from the top down, with rotations
 *  performed along the way to shorten the left branch of the right subtree
 *  and the right branch of the left subtree
 */

SYMBOL *splook(char *key, SYMBOLPTR *q, int flag )
{
  register SYMBOL * left;	/* the rightmost node in the left tree */
  register SYMBOL * right;	/* the leftmost node in the right tree */
  register SYMBOL * next;	/* the root of the unsplit part */
  register SYMBOL * temp;
  static   SYMBOL search;

  register int Sct = 1;		/* Strcmp value */

  Tune(lookups++;)
    next = *q;
    left = &search;
    right = &search;
    search.s_left = NULL;
    search.s_right = NULL;
    if ( next != NULL ) {	/* non-trivial case */

      /* search's left and right children will hold the right and left
	 splayed trees resulting from splitting on key;
	 note that the children will be reversed! */

      Tune(lkcmps++;)
        if ( STRCMP( next->s_name, key ) > 0 )
	  goto two;
	if (! Sct) return next;		/* found our root */

    one:	/* assert next->s_name <= key */

	do	/* walk to the right in the left tree */
	  {
	    if (! Sct) goto done;
            temp = next->s_right;
            if( temp == NULL )
	      {
                left->s_right = next;
                right->s_left = NULL;
                goto done;	/* job done, entire tree split */
	      }

	    Tune(lkcmps++;)
	      if( STRCMP( temp->s_name, key ) >= 0 )
		{
		  left->s_right = next;
		  left = next;
		  next = temp;
		  goto two;	/* change sides */
		}

	      next->s_right = temp->s_left;
	      left->s_right = temp;
	      temp->s_left = next;
	      left = temp;
	      next = temp->s_right;
	      if( next == NULL )
		{
		  right->s_left = NULL;
		  goto done;	/* job done, entire tree split */
		}

	      Tune(lkcmps++;)

		} while( STRCMP( next->s_name, key ) < 0 );	/* change sides */

    two:	/* assert next->s_name > key */

	do	/* walk to the left in the right tree */
	  {
	    if (! Sct) goto done;
            temp = next->s_left;
            if( temp == NULL )
	      {
                right->s_left = next;
                left->s_right = NULL;
                goto done;	/* job done, entire tree split */
	      }

	    Tune(lkcmps++;)
	      if( STRCMP( temp->s_name, key ) <= 0 )
		{
		  right->s_left = next;
		  right = next;
		  next = temp;
		  goto one;	/* change sides */
		}
	      next->s_left = temp->s_right;
	      right->s_left = temp;
	      temp->s_right = next;
	      right = temp;
	      next = temp->s_left;
	      if( next == NULL )
		{
		  left->s_right = NULL;
		  goto done;	/* job done, entire tree split */
		}

	      Tune(lkcmps++;)

		} while( STRCMP( next->s_name, key ) > 0 );	/* change sides */

        goto one;
    }

 done:		/* split is done, branches of n need reversal */

    if (Sct) {			/* not found */
      if (flag) {
	Tune(adds++;)
	  next = newsymbol(key);
      }
      else {			/* cannot add, so fix the tree */
	if(right->s_left) {
	  left->s_right = search.s_left;
	  *q = search.s_right ? search.s_right : search.s_left;
	}
	else {
	  right->s_left = search.s_right;
	  *q = search.s_left ? search.s_left : search.s_right;
	}
	return NULL;
      }
    }
    else {
      left->s_right = next->s_left;
      right->s_left = next->s_right;
    }
    next->s_left = search.s_right;
    next->s_right = search.s_left;
    *q = next;

    return( next );

} /* spenq */

void spscan(SYMBOLPTR tree, void (*function)())
{
  /* uses Robison traversal for constant space overhead */

  register SYMBOLPTR pres = tree;
  register SYMBOLPTR prev = tree;
  register SYMBOLPTR next, aval;
  register SYMBOLPTR stak = NULL;
  register SYMBOLPTR top  = NULL;

  if (! pres) return;		/* done if nothing to do */
 two:
  if (! pres->s_left) {	/* left empty? */
    (*function)(pres);	/* visit the node */
    if (!pres->s_right) {	/* and right? */
      aval = pres;
      goto five;
    }
    next = pres->s_right;
    pres->s_right = prev;
    prev = pres;
    pres = next;
    goto two;
  } else {
    next = pres->s_left;
    pres->s_left = prev;
    prev = pres;
    pres = next;
    goto two;
  }
 five:
  if (pres == tree) return;
  if (! prev->s_right) {	/* no right link */
    (*function)(prev);
    next = prev->s_left;
    prev->s_left = pres;
    pres = prev;
    prev = next;
    goto five;
  }
  if (! prev->s_left) {
    next = prev->s_right;
    prev->s_right = pres;
    pres = prev;
    prev = next;
    goto five;
  }
  if (prev == top) {
    next = stak;
    top = stak->s_right;
    stak = stak->s_left;
    next->s_left = next->s_right = NULL;
    next = prev->s_left;
    prev->s_left = prev->s_right;
    prev->s_right = pres;
    pres = prev;
    prev = next;
    goto five;
  } else {
    (*function)(prev);
    aval->s_left = stak;
    aval->s_right = top;
    stak = aval;
    top = prev;
    next = prev->s_right;
    prev->s_right = pres;
    pres = next;
    goto two;
  }
}
