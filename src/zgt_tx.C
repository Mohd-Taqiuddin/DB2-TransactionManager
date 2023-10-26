/***************** Transaction class **********************/
/*** Implements methods that handle Begin, Read, Write, ***/
/*** Abort, Commit operations of transactions. These    ***/
/*** methods are passed as parameters to threads        ***/
/*** spawned by Transaction manager class.              ***/
/**********************************************************/

/* Required header files */
#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>
#include "zgt_def.h"
#include "zgt_tm.h"
#include "zgt_extern.h"
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <pthread.h>

//Modified at 6:35 PM 09/29/2016 by Jay D. Bodra.
// Fall 2016[jay]. Removed the TxType. Now is it initialized once in the constructor

extern void *start_operation(long, long);  //start an op with mutex lock and cond wait
extern void *finish_operation(long);        //finish an op with mutex unlock and con signal

extern void *do_commit_abort(long, char);   //commit/abort based on char value
extern void *process_read_write(long, long, int, char);

extern zgt_tm *ZGT_Sh;			// Transaction manager object

/* Transaction class constructor */
/* Initializes transaction id and status and thread id */
/* Input: Transaction id, status, thread id */

//Fall 2016[jay]. Modified zgt_tx() in zgt_tx.h
zgt_tx::zgt_tx( long tid, char Txstatus,char type, pthread_t thrid){
  this->lockmode = (char)' ';  //default
  this->Txtype = type; //Fall 2016[jay] R = read only, W=Read or Write
  this->sgno =1;
  this->tid = tid;
  this->obno = -1; //set it to a invalid value
  this->status = Txstatus;
  this->pid = thrid;
  this->head = NULL;
  this->nextr = NULL;
  this->semno = -1; //init to  an invalid sem value
}

/* Method used to obtain reference to a transaction node      */
/* Inputs the transaction id. Makes a linear scan over the    */
/* linked list of transaction nodes and returns the reference */
/* of the required node if found. Otherwise returns NULL      */

zgt_tx* get_tx(long tid1){  
  zgt_tx *txptr, *lastr1;
  
  if(ZGT_Sh->lastr != NULL){	// If the list is NOT empty
      lastr1 = ZGT_Sh->lastr;	// Initialize lastr1 to first node's ptr
      for  (txptr = lastr1; (txptr != NULL); txptr = txptr->nextr)
	    if (txptr->tid == tid1) 		// if required id is found									
	       return txptr; 
      return (NULL);			// if not found in list return NULL
   }
  return(NULL);				// if list is empty return NULL
}

/* Method that handles "BeginTx tid" in test file     */
/* Inputs a pointer to transaction id, obj pair as a struct. Creates a new  */
/* transaction node, initializes its data members and */
/* adds it to transaction list */

void *begintx(void *arg){
  //intialise a transaction object. Make sure it is 
  //done after acquiring the semaphore for the tm and making sure that 
  //the operation can proceed using the condition variable. when creating
  //the tx object, set the tx to TR_ACTIVE and obno to -1; there is no 
  //semno as yet as none is waiting on this tx.
  
  struct param *node = (struct param*)arg;// get tid and count
  start_operation(node->tid, node->count); 
    zgt_tx *tx = new zgt_tx(node->tid,TR_ACTIVE, node->Txtype, pthread_self());	// Create new tx node
 
    //Fall 2016[jay]. writes the Txtype to the file.
  
    zgt_p(0);				// Lock Tx manager; Add node to transaction list
  
    tx->nextr = ZGT_Sh->lastr;
    ZGT_Sh->lastr = tx;   
    zgt_v(0); 			// Release tx manager 
  fprintf(ZGT_Sh->logfile, "T%ld\t%c \tBeginTx\n", node->tid, node->Txtype);	// Write log record and close
    fflush(ZGT_Sh->logfile);
  finish_operation(node->tid);
  pthread_exit(NULL);				// thread exit
}

/* Method to handle Readtx action in test file    */
/* Inputs a pointer to structure that contans     */
/* tx id and object no to read. Reads the object  */
/* if the object is not yet present in hash table */
/* or same tx holds a lock on it. Otherwise waits */
/* until the lock is released */

void *readtx(void *arg){
  struct param *node = (struct param*)arg;// get tid and objno and count
  //do the operations for reading. Write YOUR code

  start_operation(node->tid, node->count);
  zgt_p(0);       // Lock Tx manager

  //getting the current tx
  zgt_tx *currTx = get_tx(node->tid);

  if(currTx != NULL) {

    if(currTx->status == TR_ACTIVE){   //Tx is active
      zgt_v(0);       // Release tx manager

      //Check if sharedlock(S) can be obtained to read
      int checkLock = currTx->set_lock(node->tid, currTx-> sgno, node->obno, node->count, 'S');

      finish_operation(node->tid);
      pthread_exit(NULL);

    } else {            //Tx is inactive or in abort
      do_commit_abort(node->tid, 'A');
      zgt_v(0);         // Release transaction manager
      fprintf(ZGT_Sh->logfile, "\t Transaction %ld doesn't exist or aborted. \n", node->tid); // Write log record and close
      fflush(ZGT_Sh->logfile);
      finish_operation(node->tid);
      pthread_exit(NULL);
    }
  }

  return(0);
}


void *writetx(void *arg){ //do the operations for writing; similar to readTx
  struct param *node = (struct param*)arg;	// struct parameter that contains


  start_operation(node->tid, node->count);
  zgt_p(0);       // Lock Tx manager

  //getting the current tx
  zgt_tx *currTx = get_tx(node->tid);

  if(currTx != NULL) {

    if(currTx->status == TR_ACTIVE){   //Tx is active
      zgt_v(0);       // Release tx manager

      //Check if sharedlock(S) can be obtained to read
      int checkLock = currTx->set_lock(node->tid, currTx-> sgno, node->obno, node->count, 'X');

      finish_operation(node->tid);
      pthread_exit(NULL);

    } else {            //Tx is inactive or in abort
      do_commit_abort(node->tid, TR_ABORT);
      zgt_v(0);         // Release transaction manager
      fprintf(ZGT_Sh->logfile, "\t Transaction %ld doesn't exist or aborted. \n", node->tid); // Write log record and close
      fflush(ZGT_Sh->logfile);
      finish_operation(node->tid);
      pthread_exit(NULL);
    }

  }
  
  //do the operations for writing; similar to readTx. Write your code
  return(0);

}

//common method to process read/write: just a suggestion

void *process_read_write(long tid, long obno,  int count, char mode){

  return(0);
}

void *aborttx(void *arg)
{
  struct param *node = (struct param*)arg;// get tid and count  

  //write your code

  start_operation(node->tid, node->count); 
  zgt_p(0);
  do_commit_abort(node->tid, TR_ABORT);
  zgt_v(0);
  finish_operation(node->tid);
  

  pthread_exit(NULL);			// thread exit
}

void *committx(void *arg)
{
 
  //remove the locks/objects before committing
  struct param *node = (struct param*)arg;// get tid and count

  //write your code

  start_operation(node->tid, node->count);
  zgt_p(0);       // Lock Tx manager

  zgt_tx *currTx = get_tx(node->tid);
  currTx->status = TR_END;  //Change status to END
  do_commit_abort(node->tid,TR_END);

  zgt_v(0);       // Release tx manager
  finish_operation(node->tid);

  pthread_exit(NULL);			// thread exit
}

//suggestion as they are very similar

// called from commit/abort with appropriate parameter to do the actual
// operation. Make sure you give error messages if you are trying to
// commit/abort a non-existant tx

void *do_commit_abort(long t, char status){

  // write your code

  zgt_tx *currTx=get_tx(t);
  currTx->print_tm();

  if(currTx != NULL){

    // frees all locks held on the object by current transaction
      currTx->free_locks();
    //free operation on semaphore
      zgt_v(currTx->tid);
    //Remove the transaction node from TM
      int check_semno = currTx->semno;
      currTx->end_tx();

      //currentTx->print_tm();

    /* Tell all the waiting transactions in queue about release of locks by current Txn*/
      if(check_semno > -1){
        int numWaitTx = zgt_nwait(check_semno); //No of txns waiting on the current tx
        // printf("numberOfTransaction:: %d currentTx->Semno %d \n", numberOfTransaction, check_semno);

        if(numWaitTx > 0){
          for (int i = 0; i < numWaitTx; ++i)
          {   zgt_v(check_semno);   } //Release all semaphores of waiting txns

          numWaitTx = zgt_nwait(check_semno); //Double check for any waiting txns
          // printf("numberOfTransaction:: %d currentTx->Semno %d \n", numWaitTx, check_semno);
        }
      }else{
        printf("\ncheck_semno is -1\n");
      }


    // Commiting or Aborting the Transaction
      if(status== TR_ABORT){
        fprintf(ZGT_Sh->logfile, "T%ld\t  \tAbortTx \t \n",t);
      }else{
        fprintf(ZGT_Sh->logfile, "T%ld\t  \tCommitTx \t \n",t);
      }
      fflush(ZGT_Sh->logfile);

  }else{
    //Transaction doesn't exist
      fprintf(ZGT_Sh->logfile, "\t Transaction %ld doesn't exist. \n", t);
      fflush(ZGT_Sh->logfile);
  }
  return(0);
}

int zgt_tx::remove_tx ()
{
  //remove the transaction from the TM
  
  zgt_tx *txptr, *lastr1;
  lastr1 = ZGT_Sh->lastr;
  for(txptr = ZGT_Sh->lastr; txptr != NULL; txptr = txptr->nextr){	// scan through list
	  if (txptr->tid == this->tid){		// if correct node is found          
		 lastr1->nextr = txptr->nextr;	// update nextr value; done
		 //delete this;
         return(0);
	  }
	  else lastr1 = txptr->nextr;			// else update prev value
   }
  fprintf(ZGT_Sh->logfile, "Trying to Remove a Tx:%ld that does not exist\n", this->tid);
  fflush(ZGT_Sh->logfile);
  printf("Trying to Remove a Tx:%ld that does not exist\n", this->tid);
  fflush(stdout);
  return(-1);
}

/* this method sets lock on objno1 with lockmode1 for a tx*/

int zgt_tx::set_lock(long tid1, long sgno1, long obno1, int count, char lockmode1){
  //if the thread has to wait, block the thread on a semaphore from the
  //sempool in the transaction manager. Set the appropriate parameters in the
  //transaction list if waiting.
  //if successful  return(0); else -1
  
  // //write your code
  zgt_hlink *obj_ptr = ZGT_Ht->find(sgno1,obno1); //search if object is present in hashtable
 
  bool lock = false;

  while(!lock){
    zgt_p(0);
    zgt_tx *currentTx = get_tx(tid1);

    if(obj_ptr == NULL){
      //Object is not present in the Hash table
      ZGT_Ht->add(currentTx, currentTx->sgno, obno1, currentTx->lockmode);

      currentTx->status = 'P';
      zgt_v(0);       // Release tx manager
      return(1);

    }else{
  //Current transaction already has the lock and is trying to access the object
    if(obj_ptr->tid == tid1){
      currentTx->status = 'P';
      zgt_v(0);
      return(1);
    }else{
      zgt_tx *secondaryTx = get_tx(obj_ptr->tid);  //Txn holding the object
      zgt_hlink *otherTx = others_lock(obj_ptr, sgno1, obno1);  //Other txns wanting the object.

      if(currentTx->Txtype=='R' && secondaryTx->Txtype=='R' && otherTx->lockmode!='X'){
      //Check if current txn and txn holding object have shared read.
      //Also check if any other txn is waiting on the object for a write. This takes care that txn doesnt unfairly wait.
        lock = true;  //Lock granted

        if(currentTx->head == NULL){
          //Add current transaction to object in hash table, as it has no pointers to hash table
            ZGT_Ht->add(currentTx, currentTx->sgno, obno1, currentTx->lockmode);
            obj_ptr=ZGT_Ht->find(currentTx-> sgno, obno1);
            //printf("In here new obj head\n");
            currentTx->head = obj_ptr;            //Point the head to this object node
            currentTx->status = 'P';
            //currentTx->semno = -1;
            zgt_v(0);
            return(1);
        }else{
          //iterate to the end of object list to add new required object node at the end
          zgt_hlink *temp = currentTx->head;
          while(temp->nextp != NULL){
            temp = temp->nextp;
          }
          temp->nextp = obj_ptr;               // Point the nextp of last object node to the new node

          currentTx->status = 'P';
          //currentTx->semno = -1;
          zgt_v(0);
          return(1);
        }
      }
      else{
          //If one of the transaction is not in shared mode
            currentTx->status = 'W';         //Make status of current txn as WAIT
            currentTx->obno = obno1;
            currentTx->lockmode = lockmode1;
            if(get_tx(obj_ptr->tid))
            currentTx->setTx_semno(obj_ptr->tid, obj_ptr->tid); //Set semaphore on current tx
                                                              //so that txn holding object knows a new txn is waiting for the object.
            else{
            //In case txn holding object ceases, grant lock.
              currentTx->status = 'P';
              zgt_v(0);
              return(1);

            }

            printf("Tx %ld is waiting on:T%ld\n", currentTx->tid, obj_ptr->tid);
            currentTx->print_tm();
            zgt_v(0);
            zgt_p(obj_ptr->tid); //Hold txn with the object.
            lock = false;
          }
        }// different thread hold Object
      }//object does exist
    }

  return(0);
}

int zgt_tx::free_locks()
{
  
  // this part frees all locks owned by the transaction
  // that is, remove the objects from the hash table
  // and release all Tx's waiting on this Tx

  zgt_hlink* temp = head;  //first obj of tx
  
  for(temp;temp != NULL;temp = temp->nextp){	// SCAN Tx obj list

      fprintf(ZGT_Sh->logfile, "%ld : %d, ", temp->obno, ZGT_Sh->objarray[temp->obno]->value);
      fflush(ZGT_Sh->logfile);
      
      if (ZGT_Ht->remove(this,1,(long)temp->obno) == 1){
	   printf(":::ERROR:node with tid:%ld and onjno:%ld was not found for deleting", this->tid, temp->obno);		// Release from hash table
	   fflush(stdout);
      }
      else {
#ifdef TX_DEBUG
	   printf("\n:::Hash node with Tid:%ld, obno:%ld lockmode:%c removed\n",
                            temp->tid, temp->obno, temp->lockmode);
	   fflush(stdout);
#endif
      }
    }
  fprintf(ZGT_Sh->logfile, "\n");
  fflush(ZGT_Sh->logfile);
  
  return(0);
}		

// check which other transaction has the lock on the same obno
// returns the hash node
zgt_hlink *zgt_tx::others_lock(zgt_hlink *hnodep, long sgno1, long obno1)
{
  zgt_hlink *ep;
  ep=ZGT_Ht->find(sgno1,obno1);
  while (ep)				// while ep is not null
    {
      if ((ep->obno == obno1)&&(ep->sgno ==sgno1)&&(ep->tid !=this->tid)) 
	return (ep);			// return the hashnode that holds the lock
      else  ep = ep->next;		
    }					
  return (NULL);			//  Return null otherwise 
  
}


// CURRENTLY Not USED
// USED to COMMIT
// remove the transaction and free all associate dobjects. For the time being
// this can be used for commit of the transaction.

int zgt_tx::end_tx()  //2016: not used
{
  zgt_tx *linktx, *prevp;
  
  // USED to COMMIT 
  //remove the transaction and free all associate dobjects. For the time being 
  //this can be used for commit of the transaction.
  
  linktx = prevp = ZGT_Sh->lastr;
  
  while (linktx){
    if (linktx->tid  == this->tid) break;
    prevp  = linktx;
    linktx = linktx->nextr;
  }
  if (linktx == NULL) {
    printf("\ncannot remove a Tx node; error\n");
    fflush(stdout);
    return (1);
  }
  if (linktx == ZGT_Sh->lastr) ZGT_Sh->lastr = linktx->nextr;
  else {
    prevp = ZGT_Sh->lastr;
    while (prevp->nextr != linktx) prevp = prevp->nextr;
    prevp->nextr = linktx->nextr;    
  }
  return(0);
}

// currently not used
int zgt_tx::cleanup()
{
  return(0);
  
}

// routine to print the tx list
// TX_DEBUG should be defined in the Makefile to print
void zgt_tx::print_tm(){
  
  zgt_tx *txptr;
  
#ifdef TX_DEBUG
  printf("printing the tx  list \n");
  printf("Tid\tTxType\tThrid\t\tobjno\tlock\tstatus\tsemno\n");
  fflush(stdout);
#endif
  txptr=ZGT_Sh->lastr;
  while (txptr != NULL) {
#ifdef TX_DEBUG
    printf("%ld\t%c\t%ld\t%ld\t%c\t%c\t%d\n", txptr->tid, txptr->Txtype, txptr->pid, txptr->obno, txptr->lockmode, txptr->status, txptr->semno);
    fflush(stdout);
#endif
    txptr = txptr->nextr;
  }
  fflush(stdout);
}

//need to be called for printing
void zgt_tx::print_wait(){

  //route for printing for debugging
  
  printf("\n    SGNO        TxType       OBNO        TID        PID         SEMNO   L\n");
  printf("\n");
  return;
}

void zgt_tx::print_lock(){
  //routine for printing for debugging
  
  printf("\n    SGNO        OBNO        TID        PID   L\n");
  printf("\n");
  return;
}

// routine to perform the acutual read/write operation
// based  on the lockmode

void zgt_tx::perform_readWrite(long tid,long obno, char lockmode){
  
  //write your code

}

// routine that sets the semno in the Tx when another tx waits on it.
// the same number is the same as the tx number on which a Tx is waiting
int zgt_tx::setTx_semno(long tid, int semno){
  zgt_tx *txptr;
  
  txptr = get_tx(tid);
  if (txptr == NULL){
    printf("\n:::ERROR:Txid %ld wants to wait on sem:%d of tid:%ld which does not exist\n", this->tid, semno, tid);
    fflush(stdout);
    exit(1);
  }
  if ((txptr->semno == -1)|| (txptr->semno == semno)){  //just to be safe
    txptr->semno = semno;
    return(0);
  }
  else if (txptr->semno != semno){
#ifdef TX_DEBUG
    printf(":::ERROR Trying to wait on sem:%d, but on Tx:%ld\n", semno, txptr->tid);
    fflush(stdout);
#endif
    exit(1);
  }
  return(0);
}

void *start_operation(long tid, long count){
  
  pthread_mutex_lock(&ZGT_Sh->mutexpool[tid]);	// Lock mutex[t] to make other
  // threads of same transaction to wait
  
  while(ZGT_Sh->condset[tid] != count)		// wait if condset[t] is != count
    pthread_cond_wait(&ZGT_Sh->condpool[tid],&ZGT_Sh->mutexpool[tid]);
  return(0);
}

// Otherside of teh start operation;
// signals the conditional broadcast

void *finish_operation(long tid){
  ZGT_Sh->condset[tid]--;	// decr condset[tid] for allowing the next op
  pthread_cond_broadcast(&ZGT_Sh->condpool[tid]);// other waiting threads of same tx
  pthread_mutex_unlock(&ZGT_Sh->mutexpool[tid]); 
  return(0);
}


