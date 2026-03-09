#ifndef __GENERIC_LIST
#define __GENERIC_LIST

/*
 * Author:    Peter Dickman
 * Version:   1.1
 * Created:   1997-10-16
 * Edited:    2000-01-17
 * Edited:    2025-10-17  Paul Harvey
 *
 *
 * Generic package for a one-and-a-half ended list
 * (can add and remove at front and add at rear)
 *
 */

/*
 * packages which use this list simply see glists,
 * the lists simply hold glist_elements
 *
 * two way decoupling encourages information hiding
 *
 */

typedef        void *     glist;
typedef        void *     glist_element;

/********************************/
/* Constructors and Destructors */
/********************************/

/* can create and destroy empty glists */
/* destroy returns true if successful  */

glist create_glist (void); 
int   destroy_glist(glist);


/***********/
/* Methods */
/***********/

/* Can interrogate list for length */

int            glist_empty    (glist);
int            glist_nonempty (glist);
unsigned long  glist_length   (glist);

/* Can add to front and rear of list or remove from front */
/* in each case return true if successful, false if not   */

int glist_add_to_front    (glist, glist_element);
int glist_add_to_rear     (glist, glist_element);

int glist_take_from_front (glist, glist_element *);

/* Can also discard list contents */

void glist_purge (glist);


/********************/
/* Advanced Methods */
/********************/

/* NB: do not expect beginners to understand or use these */

/*---------------------------------------------------------*/
/* map applies the function provided as second arg to each */
/* value stored in the list in turn, from front to rear.   */
/* Note that the function whose address is passed must     */
/* take a glist_element as argument and return nothing.    */

void glist_map(glist, void (*fp)(glist_element));

/*---------------------------------------------------------*/

/* applies cleaner to stored items before discarding references */
/* implemented using map, so cleaner must match fp above        */

void glist_purge_carefully(glist, void (*cleaner)(glist_element));

/*---------------------------------------------------------*/


#endif

