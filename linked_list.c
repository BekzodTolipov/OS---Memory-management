#include <stdlib.h>     //exit()
#include <stdio.h>      //printf()
#include <stdbool.h>    //bool variable
#include <stdint.h>     //for uint32_t
#include <string.h>     //str function
#include <unistd.h>     //standard symbolic constants and types

struct Node{
	int data;
	struct Node* next;
};

struct List{
	struct Node* head;
};
void push(struct Node** head_ref, int new_key);
void printList(struct Node *node);
void printInt(void *n);
void printFloat(void *f);
bool search(struct Node* head, int x);
void delete_el(struct Node** head, int x);


int main(int argc, char *argv[]){
	struct Node *start = NULL; 
  
    // Create and print an int linked list 
    unsigned int_size = sizeof(int); 
    int arr[] = {10, 20, 30, 40, 50}, i; 
    for (i=4; i>=0; i--) 
       push(&start, arr[i]); 
    printf("Created integer linked list is \n"); 
    printList(start); 

	printf("Is element (%d) in list?\n", 20);
	search(start, 20)? printf("Yes\n") : printf("No\n"); 

    printf("After delete element linked list is (%d)\n", 10); 
	delete_el(&start, 10);
    printList(start); 
  
    // Create and print a float linked list 
//    unsigned float_size = sizeof(float); 
//    start = NULL; 

  //  float arr2[] = {10.1, 20.2, 30.3, 40.4, 50.5}; 
   // for (i=4; i>=0; i--) 
   //    push(&start, &arr2[i], float_size); 

  //  printf("\n\nCreated float linked list is \n"); 
   // printList(start, printFloat); 
  
    return 0; 
}

void push(struct Node** head_ref, int new_key) 
{ 
    /* allocate node */
    struct Node* new_node = 
            (struct Node*) malloc(sizeof(struct Node)); 
  
    /* put in the key  */
    new_node->data  = new_key; 
  
    /* link the old list off the new node */
    new_node->next = (*head_ref); 
  
    /* move the head to point to the new node */
    (*head_ref)    = new_node; 
} 

/* Function to print nodes in a given linked list. fpitr is used 
   to access the function to be used for printing current node data. 
   Note that different data types need different specifier in printf() */
void printList(struct Node *node) 
{ 
    while (node != NULL) 
    { 
        fprintf(stderr, " %d", (node->data)); 
        node = node->next; 
    }
	fprintf(stderr, "\n"); 
}


/* Checks whether the value x is present in linked list */
bool search(struct Node* head, int x) 
{ 
    struct Node* current = head;  // Initialize current 
    while (current != NULL) 
    { 
        if (current->data == x) 
            return true; 
        current = current->next; 
    } 
    return false; 
} 

void delete_el(struct Node** head, int x)
{
    struct Node* current = (*head);  // Initialize current
	struct Node* prev = NULL;
    while (current != NULL)
    {
        if (current->data == x){
			if(prev != NULL){
				prev->next = current->next;
				free(current);
			}
			else{
				prev = current;
				(*head) = current->next;
				free(prev);
			}
			break;
		}
		prev = current;
        current = current->next;
    }
    
}
 
