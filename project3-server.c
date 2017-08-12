#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#define BUFSIZE     80       /* anything large enough for your messages */
#define MAX_USER    100
#define MAX_MESG    10

typedef int bool;
#define true 1
#define false 0

sem_t mutex1;
sem_t mutex2;
int   search_count = 0;

// message_node data structure
typedef struct message_node {
    char sender_name[BUFSIZE];
    char send_time[BUFSIZE];
    char message[BUFSIZE];
}message_node;

//user_node data structure
typedef struct user_node {
    char          user_name[BUFSIZE];
    message_node* message_list[MAX_MESG];
    sem_t         mesg_mutex;
    int           num_mesg;
    bool          connected;
    struct        user_node* next;
}user_node;

//user list data structure
typedef struct user_list {
    user_node* head;
    user_node* tail;
    int        num_user;
}user_list;

void read_message(int sd, char* mesg);
void send_message(int sd, char* mesg);
void read_char(int sd, char* c);
void send_char(int sd, char* c);

void sem_init_check(sem_t* semaphore, int init_value);
void sem_wait_check(sem_t* semaphore);
void sem_post_check(sem_t* semaphore);

void search_list_wait();
void search_list_post();

bool list_is_empty(user_list* list);
void addList(user_node* user, user_list* list);
void printList(user_list* list);
user_node* searchList(char* username, user_list* list);

void format_time(char *buffer);

void* handleClient(void *);
user_list *list;

int main(int argc, char *argv[])
{
   char     host[BUFSIZE];
   char     buf[BUFSIZE];
   int      sd, sd_current;
   int      port;
   int      *sd_client;
   int      addrlen;
   struct   sockaddr_in sin;
   struct   sockaddr_in pin;
   pthread_t tid;
   pthread_attr_t attr;

   /* check for command line arguments */
   if (argc != 2)
   {
      printf("Usage: server port\n");
      exit(1);
   }

   /* get port from argv */
   port = atoi(argv[1]);
 
 
   /* create an internet domain stream socket */
   if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      perror("Error creating socket");
      exit(1);
   }

   /* complete the socket structure */
   memset(&sin, 0, sizeof(sin));
   sin.sin_family = AF_INET;
   sin.sin_addr.s_addr = INADDR_ANY;  /* any address on this host */
   sin.sin_port = htons(port);        /* convert to network byte order */

   /* bind the socket to the address and port number */
   if (bind(sd, (struct sockaddr *) &sin, sizeof(sin)) == -1) {
      perror("Error on bind call");
      exit(1);
   }

   /* set queuesize of pending connections */ 
   if (listen(sd, 100) == -1) {
      perror("Error on listen call");
      exit(1);
   }

   /* announce server is running */
   gethostname(host, BUFSIZE);
   printf("Server is running on %s:%d\n", host, port);
    
   // create user list
   list = (user_list*)malloc(sizeof(user_list));
   list->head = NULL;
   list->tail = NULL;
   list->num_user = 0;
   // initialize semaphore mutex
   sem_init_check(&mutex1, 1);
   sem_init_check(&mutex2, 1);


   /* wait for a client to connect */
   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED); /* use detached threads */
   addrlen = sizeof(pin);
   while (1)
   {
      if ((sd_current = accept(sd, (struct sockaddr *)  &pin, (socklen_t*)&addrlen)) == -1) 
      {
	    perror("Error on accept call");
	    exit(1);
      }

      sd_client = (int*)(malloc(sizeof(sd_current)));
      *sd_client = sd_current;

      pthread_create(&tid, &attr, handleClient, sd_client);
   }

   /* close socket */
   close(sd);
}


void* handleClient(void *arg)
{
   int      mesg_count = 0;
   int      flag = 0;
   int      i = 0;
   char     buf[BUFSIZE];  /* used for incoming string, and outgoing data */
   char     message[BUFSIZE];
   char     name[BUFSIZE];
   char     recipient[BUFSIZE];
   char     time[BUFSIZE];
   char     sys_time[BUFSIZE];
   char     mesg_count_s[3];
   char     connection_allowed = 'T';
   char     recipient_allowed = 'T';
   char     message_allowed = 'T';
   char     choice=' ';

   user_node *curr;
   user_node *user;
   message_node *mesg;

   int sd = *((int*)arg);  /* get sd from arg */
   free(arg);              /* free malloced arg */

   /* read user name from the client */
   read_message(sd, name);

   // search to see if the user is in the list
   search_list_wait();
   curr = searchList(name, list);
   search_list_post();
    
    if (curr == NULL){
        
        sem_wait_check(&mutex1);
        if(list->num_user < MAX_USER){
            // allow connection by unknown user
            send_char(sd, &connection_allowed);
            format_time(sys_time);
            printf("%s Connection by unknown user %s\n", sys_time, name);
            
            // create a new user node
            user = (user_node*)malloc(sizeof(user_node));
            strcpy(user->user_name, name);
            user->connected = true;
            user->next = NULL;
            user->num_mesg = 0;
            sem_init_check(&(user->mesg_mutex), 1);
            
            // add new user to the list
            addList(user, list);
            ++list->num_user;
            curr = user;
        }

        else{
            // user list is full, disallow connection
            format_time(sys_time);
            printf("%s Connection failed by unknown user %s\n", sys_time, name);
            // send error message to client
            connection_allowed = 'F';
            send_char(sd, &connection_allowed);
            sprintf(buf, "Cannot connect. Known user list is full\n");
            send_message(sd, buf);
            flag = 1;
        }
        sem_post_check(&mutex1);
    }
    else{
        sem_wait_check(&mutex1);
        if (curr->connected == true){
            
            //disallow connection by known user who is already connected
            format_time(sys_time);
            printf("%s Connection failed by known user %s\n", sys_time, curr->user_name);
            connection_allowed = 'F';
            send_char(sd, &connection_allowed);
            sprintf(buf, "Cannot connect. Already connected\n");
            send_message(sd, buf);
            flag = 1;
        }
        else{
            
            // allow connection by known user who is not connected
            send_char(sd, &connection_allowed);
            format_time(sys_time);
            printf("%s Connection by known user %s\n", sys_time, name);
            
            curr->connected = true;
        }
        sem_post_check(&mutex1);
    }
    
   // read choice from client
   while(flag != 1){
       read_char(sd, &choice);
       switch(choice){
          case '1': //display all known users' name
               
               // read user's name from client
               read_message(sd, name);
               format_time(sys_time);
               printf("%s %s displays all known users\n", sys_time, name);
               
               // count the number of users
               mesg_count = 0;
               
               search_list_wait();
               curr = list->head;
               while(curr != NULL){
                   ++mesg_count;
                   curr = curr->next;
               }
               search_list_post();
               
               // convert message count into string and send to client
               sprintf(mesg_count_s,"%d",mesg_count);
               send_message(sd, mesg_count_s);
               
               // send messages to client
               i = 0;
               
               search_list_wait();
               curr = list->head;
               while(i != mesg_count){
                   send_message(sd, curr->user_name);
                   ++i;
                   curr = curr->next;
               }
               search_list_post();
               
               break;
               
          case '2':  //display all currently connected users' name
               
               // read user's name from client
               read_message(sd, name);
               format_time(sys_time);
               printf("%s %s displays all connected users\n", sys_time, name);

               
               // count the message size
               mesg_count = 0;
               
               search_list_wait();
               curr = list->head;
               while(curr != NULL){
                   if(curr->connected == true)
                       ++mesg_count;
                   curr = curr->next;
               }
               search_list_post();
               
               // convert message size into string and send to client
               sprintf(mesg_count_s,"%d",mesg_count);
               send_message(sd, mesg_count_s);
               
               // send messages to client
               i = 0;
               
               search_list_wait();
               curr = list->head;
               while(i != mesg_count){
                   if(curr->connected == true){
                       send_message(sd, curr->user_name);
                       ++i;
                   }
                   curr = curr->next;
               }
               search_list_post();
               
               break;
               
          case '3':
               // read recipient's name from client
               read_message(sd, recipient);
               
               // search recipient in the user list
               search_list_wait();
               curr = searchList(recipient, list);
               search_list_post();
               
               // read sender's name from client
               read_message(sd, name);
               
               // if recipient not in the list, add to the list
               if (curr == NULL){
                   
                   // create a new user node
                   sem_wait_check(&mutex1);
                   if(list->num_user < MAX_USER){
                       recipient_allowed = 'T';
                       send_char(sd, &recipient_allowed);
                       user = (user_node*)malloc(sizeof(user_node));
                       strcpy(user->user_name, recipient);
                       user->connected = false;
                       user->next = NULL;
                       user->num_mesg = 0;
                       sem_init_check(&(user->mesg_mutex), 1);
                       
                       // add new user to the list
                       addList(user, list);
                       ++list->num_user;
                       curr = user;
                   }
                   
                   else{
                       
                       // user list is full, cannot add new recipient to the list
                       format_time(sys_time);
                       printf("%s %s failed to post a message for %s\n", sys_time, name, recipient);
                       recipient_allowed = 'F';
                       send_char(sd, &recipient_allowed);
                       sem_post_check(&mutex1);
                       break;
                   }
                   sem_post_check(&mutex1);
               }
               else{
                   recipient_allowed = 'T';
                   send_char(sd, &recipient_allowed);
               }
  
               
               // read sender's name, sent time and sent message from client
               read_message(sd, time);
               read_message(sd, message);
               
               // add the message node into message list
               sem_wait_check(&(curr->mesg_mutex));
               if(curr->num_mesg < MAX_MESG){
                   message_allowed = 'T';
                   send_char(sd, &message_allowed);
                   
                   // create a new message node and store sender's name, sent time and sent message into the node
                   mesg = (message_node*)malloc(sizeof(message_node));
                   strcpy(mesg->sender_name, name);
                   strcpy(mesg->send_time, time);
                   strcpy(mesg->message, message);
                   
                   curr->message_list[curr->num_mesg] = mesg;
                   curr->num_mesg = curr->num_mesg + 1;
               }
               
               else{
                   // recipient's message list is full
                   message_allowed = 'F';
                   send_char(sd, &message_allowed);
                   format_time(sys_time);
                   printf("%s %s failed to post a message for %s\n", sys_time, name, recipient);
                   sem_post_check(&(curr->mesg_mutex));
                   break;
               }
               sem_post_check(&(curr->mesg_mutex));
            
               format_time(sys_time);
               printf("%s %s posts a message for %s\n", sys_time, name, recipient);
 
               break;
               
          case '4':
               
               // read sender's name, sent time and sent message from client
               read_message(sd, name);
               read_message(sd, time);
               read_message(sd, message);
               
               format_time(sys_time);
               printf("%s %s is posting message to all currently connected users\n", sys_time, name);
               
               // count the number of connected users
               mesg_count = 0;
               
               search_list_wait();
               curr = list->head;
               while(curr != NULL){
                   if(curr->connected == true)
                       ++mesg_count;
                   curr = curr->next;
               }
               search_list_post();
               
               // convert message count into string and send to client
               sprintf(mesg_count_s,"%d",mesg_count);
               send_message(sd, mesg_count_s);

               search_list_wait();
               curr = list->head;
               while(curr!=NULL){
                   if(curr->connected == true){
                       
                       // add the message node into message list
                       sem_wait_check(&(curr->mesg_mutex));
                       if(curr->num_mesg < MAX_MESG){
                           
                           // create a new message node and store sender's name, sent time and sent message into the node
                           mesg = (message_node*)malloc(sizeof(message_node));
                           strcpy(mesg->sender_name, name);
                           strcpy(mesg->send_time, time);
                           strcpy(mesg->message, message);
                           
                           curr->message_list[curr->num_mesg] = mesg;
                           curr->num_mesg = curr->num_mesg + 1;
                           format_time(sys_time);
                           printf("%s %s posts a message for %s\n", sys_time, name, curr->user_name);
                           sprintf(buf, "Message posted to %s\n", curr->user_name);
                           send_message(sd, buf);
                       }
                       
                       // message list if full, message cannot be added to the list
                       else{
                           
                           format_time(sys_time);
                           printf("%s %s failed to post a message for %s\n", sys_time, name, curr->user_name);
                           sprintf(buf, "Message list is full. Cannot post message to %s\n", curr->user_name);
                           send_message(sd, buf);
                       }
                       sem_post_check(&(curr->mesg_mutex));
                   }
                   curr = curr->next;
               }
               search_list_post();
        
               break;
          case '5':
               // read sender's name, sent time and sent message from client
               read_message(sd, name);
               read_message(sd, time);
               read_message(sd, message);
               
               format_time(sys_time);
               printf("%s %s is posting message to all known users\n", sys_time, name);
               
               // count the number of connected users
               mesg_count = 0;
               
               search_list_wait();
               curr = list->head;
               while(curr != NULL){
                   ++mesg_count;
                   curr = curr->next;
               }
               search_list_post();
               
               // convert message count into string and send to client
               sprintf(mesg_count_s,"%d",mesg_count);
               send_message(sd, mesg_count_s);

               search_list_wait();
               curr = list->head;
               while(curr!=NULL){
                   
                   // add the message node into message list
                   sem_wait_check(&(curr->mesg_mutex));
                   if(curr->num_mesg < MAX_MESG){
                       
                       // create a new message node and store sender's name, sent time and sent message into the node
                       mesg = (message_node*)malloc(sizeof(message_node));
                       strcpy(mesg->sender_name, name);
                       strcpy(mesg->send_time, time);
                       strcpy(mesg->message, message);
                       
                       curr->message_list[curr->num_mesg] = mesg;
                       curr->num_mesg = curr->num_mesg + 1;
                       format_time(sys_time);
                       printf("%s %s posts a message for %s\n", sys_time, name, curr->user_name);
                       sprintf(buf, "Message posted to %s\n", curr->user_name);
                       send_message(sd, buf);
                   }
                   
                   else{
                       format_time(sys_time);
                       printf("%s %s failed to post a message for %s\n", sys_time, name, curr->user_name);
                       sprintf(buf, "Message list is full. Cannot post message to %s\n", curr->user_name);
                       send_message(sd, buf);
                   }
                   sem_post_check(&(curr->mesg_mutex));

                   curr = curr->next;
               }
               search_list_post();
               
               break;
          case '6':
               // read the name of user who wants to get his messages
               read_message(sd, name);
               // search the user in the user list
               search_list_wait();
               curr = searchList(name, list);
               search_list_post();
               
               format_time(sys_time);
               printf("%s %s gets messages\n", sys_time, name);
               
               // count the number of messages
               sem_wait_check(&(curr->mesg_mutex));
               mesg_count = curr->num_mesg;
               
               sprintf(mesg_count_s,"%d",mesg_count);
               send_message(sd, mesg_count_s);
               
               // send messages to client
               for(i = 0; i < mesg_count; ++i){
                   
                   mesg = curr->message_list[i];
                   sprintf(buf, "%d. From %s, %s, %s",i+1, mesg->sender_name, mesg->send_time, mesg->message);
                   send_message(sd, buf);
                   // delete message
                   free(mesg);
               }
               // update number of messages in the list
               curr->num_mesg = 0;
               sem_post_check(&(curr->mesg_mutex));
               
               break;
          case '7':
               flag = 1;
               // read the name of user who is going to exit
               read_message(sd, name);
               // search the user in the user list
               search_list_wait();
               curr = searchList(name, list);
               search_list_post();
               
               sem_wait_check(&mutex1);
               curr->connected = false;
               sem_post_check(&mutex1);
               
               format_time(sys_time);
               printf("%s %s exits\n", sys_time, name);
               
               break;
          default:
               printf("Wrong choice\n");
               break;
       }
   }


   /* close socket */
   close(sd); 
}

void read_message(int sd, char* mesg){
    int bytes_read = 0;
    int count = 0;
    int len = 0;
    char len_s[2];
    
    // read message size into 2-byte string
    while(bytes_read != 2){
        count = read(sd, len_s + bytes_read, 2 - bytes_read);
        if(count < 0){
            printf("read message size failed\n");
            exit(1);
        }
        bytes_read = bytes_read + count;
    }
    
    // convert string into integer
    len = atoi(len_s);
    
    // read message
    bytes_read = 0;
    count = 0;
    while(bytes_read != len + 1){
        count = read(sd, mesg + bytes_read, len + 1 - bytes_read);
        if(count < 0){
            printf("read message failed\n");
            exit(1);
        }
        bytes_read = bytes_read + count;
    }
}


void send_message(int sd, char* mesg){
    int bytes_sent = 0;
    int count = 0;
    int len = strlen(mesg);
    char len_s[BUFSIZE];
    
    // convert message size into 2-byte string and send
    sprintf(len_s,"%2d",len);
    
    // send message size
    while(bytes_sent != 2){
        count = write(sd, len_s + bytes_sent, 2 - bytes_sent);
        if(count < 0){
            printf("send message size failed\n");
            exit(1);
        }
        bytes_sent = bytes_sent + count;
    }
    
    // send message
    bytes_sent = 0;
    count = 0;
    while(bytes_sent != len + 1){
        count = write(sd, mesg + bytes_sent, len + 1 - bytes_sent);
        if(count < 0){
            printf("send message failed\n");
            exit(1);
        }
        bytes_sent = bytes_sent + count;
    }
}


void read_char(int sd, char* c){
    int bytes_read = 0;
    int count = 0;
    while(bytes_read != 1){
        count = read(sd, c + bytes_read, 1 - bytes_read);
        if(count < 0){
            printf("read char failed\n");
            exit(1);
        }
        bytes_read = bytes_read + count;
    }
}

void send_char(int sd, char* c){
    int bytes_sent = 0;
    int count = 0;
    while(bytes_sent != 1){
        count = write(sd, c + bytes_sent, 1 - bytes_sent);
        if(count < 0){
            printf("send char failed\n");
            exit(1);
        }
        bytes_sent = bytes_sent + count;
    }
}


//function to initialize semaphore and check its status
void sem_init_check(sem_t* semaphore, int init_value) {
    if (sem_init(semaphore, 0, init_value) == -1)
    {
        printf("Init semaphore failed\n");
        exit(1);
    }
}

//function to wait semaphore and check its status
void sem_wait_check(sem_t* semaphore) {
    if (sem_wait(semaphore) == -1)
    {
        printf("Wait on semaphore failed\n");
        exit(1);
    }
}

//function to post semaphore and check its status
void sem_post_check(sem_t* semaphore) {
    if (sem_post(semaphore) == -1)
    {
        printf("Post semaphore failed\n");
        exit(1);
    }
}

void search_list_wait(){
    sem_wait_check(&mutex2);
    search_count++;
    if(search_count == 1)
        sem_wait_check(&mutex1);
    sem_post_check(&mutex2);
}

void search_list_post(){
    sem_wait_check(&mutex2);
    search_count--;
    if(search_count == 0)
        sem_post_check(&mutex1);
    sem_post_check(&mutex2);
}


bool list_is_empty(user_list* list) {
    if (list->head == NULL && list->tail == NULL)
        return true;
    else
        return false;
}

void addList(user_node* user, user_list* list) {
    if (list_is_empty(list)) {
        list->head = user;
        list->tail = user;
    }
    else {
        list->tail->next = user;
        list->tail = user;
    }
}

void printList(user_list* list){
    char name[BUFSIZE];
    user_node* curr = list->head;
    message_node* mesg;
    printf("Known users:\n");
    int i = 1;
    int j = 0;
    while(curr!=NULL){
        printf("%d. %s", i, curr->user_name);
        for(j = 0; j < curr->num_mesg; ++j){
            mesg = curr->message_list[j];
            strcpy(name, mesg->sender_name);
            name[strlen(name) - 1] = '\0';
            printf("    %d. %s %s %s",j+1, name, mesg->send_time, mesg->message);
        }
        curr = curr->next;
        ++i;
    }
}

user_node* searchList(char* username, user_list* list){
    user_node* curr = list->head;
    while(curr!=NULL){
        if(strcmp(username, curr->user_name) == 0)
            return curr;
        else
            curr = curr->next;
    }
    return curr;
}

void format_time(char *buffer){
    time_t timer;
    struct tm* tm_info;
    
    time(&timer);
    tm_info = localtime(&timer);
    
    strftime(buffer, 26, "%m/%d/%y, %I:%M %p,", tm_info);
}



