#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <time.h>


/* Run as:  client host port
 *
 * where host is the machine to connect to and port is its port number */
#define BUFSIZE     80       /* anything large enough for your messages */
typedef int bool;
#define true 1
#define false 0

void  printMenu();
void  clearInputBuffer();
void  read_message(int sd, char* mesg);
void  send_message(int sd, char* mesg);
void  read_char(int sd, char* c);
void  send_char(int sd, char* c);
void  format_time(char *buffer);

int main(int argc, char *argv[])
{
   int  sd;
   int  port;
   int  mesg_count = 0;
   int  flag = 0;
   int  i = 0;
   char hostname[100];
   char buf[BUFSIZE];
   char name[BUFSIZE];
   char recipient[BUFSIZE];
   char time[BUFSIZE];
   char mesg_count_s[3];
   char choice = ' ';
   char connection_allowed = 'T';
   char message_allowed = 'T';
   char recipient_allowed = 'T';
   struct sockaddr_in pin;
   struct hostent *hp;

   /* check for command line arguments */
   if (argc != 3)
   {
      printf("Usage: client host port\n");
      exit(1);
   }

   /* get hostname and port from argv */
   strncpy(hostname, argv[1], sizeof(hostname));
   hostname[99] = '\0';
   port = atoi(argv[2]);

   /* create an Internet domain stream socket */
   if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      perror("Error creating socket");
      exit(1);
   }

   /* lookup host machine information */
   if ((hp = gethostbyname(hostname)) == 0) {
      perror("Error on gethostbyname call");
      exit(1);
   }

   /* fill in the socket address structure with host information */
   memset(&pin, 0, sizeof(pin));
   pin.sin_family = AF_INET;
   pin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
   pin.sin_port = htons(port); /* convert to network byte order */


   printf("Connecting to %s:%d\n\n", hostname, port); 

   /* connect to port on host */
   if (connect(sd,(struct sockaddr *)  &pin, sizeof(pin)) == -1) {
      perror("Error on connect call");
      exit(1);
   }

   /* ask user for name */
   printf("Enter your name: ");
   fgets(name, sizeof(name), stdin);

   /* send the name to the server */
   // remove '\n'
   name[strlen(name) - 1] = '\0';
   send_message(sd, name);
   
   read_char(sd, &connection_allowed);
    
   if(connection_allowed == 'F'){
       read_message(sd, buf);
       printf("%s", buf);
       flag = 1;
   }
    
   while(flag != 1){
        printMenu();
        scanf(" %c", &choice);
        clearInputBuffer();
       
        //send choice to client
        send_char(sd, &choice);
       
        switch(choice){
           
           case '1':
                
                // send user's name to server
                send_message(sd, name);
                
                // read message count from server
                read_message(sd, mesg_count_s);
                
                // convert message count into integer
                mesg_count = atoi(mesg_count_s);
                
                // read message from server
                i = 0;
                printf("\nKnown users:\n");
                while(i != mesg_count){
                    read_message(sd, buf);
                    printf("%d. %s\n", i+1, buf);
                    ++i;
                }
                break;

           case '2':
                
                // send user's name to server
                send_message(sd, name);
                
                // read message count from server
                read_message(sd, mesg_count_s);
                
                // convert message count into integer
                mesg_count = atoi(mesg_count_s);
                
                // read message from server
                i = 0;
                printf("\nConnected users:\n");
                while(i != mesg_count){
                    read_message(sd, buf);
                    printf("%d. %s\n", i+1, buf);
                    ++i;
                }
               break;

           case '3':
                
                //ask user for recipient's name
                printf("\nEnter recipient's name: ");
                fgets(recipient, sizeof(recipient), stdin);
                recipient[strlen(recipient) - 1] = '\0';
                
                //send recipient name and user name to server
                send_message(sd, recipient);
                send_message(sd, name);
                
                read_char(sd, &recipient_allowed);
                if(recipient_allowed == 'F'){
                    printf("\nKnown user list is full. Cannot post message to %s\n", recipient);
                    break;
                }
                //get system time
                format_time(time);
                
                //ask user for message
                printf("Enter a message: ");
                fgets(buf, sizeof(buf), stdin);
                
                //send time and message to server
                send_message(sd, time);
                send_message(sd, buf);
                
                read_char(sd, &message_allowed);
                if(message_allowed == 'T')
                    printf("\nMessage posted to %s\n", recipient);
                else
                    printf("\nMessage list is full. Cannot post message to %s\n", recipient);

                break;
                
           case '4':
                
                //ask user for message
                printf("Enter a message: ");
                fgets(buf, sizeof(buf), stdin);
                
                //get system time
                format_time(time);
                
                //send user's name, time, and message to server
                send_message(sd, name);
                send_message(sd, time);
                send_message(sd, buf);
                
                // read message count from server
                read_message(sd, mesg_count_s);
                
                // convert message count into integer
                mesg_count = atoi(mesg_count_s);
                
                // read sending status from server
                printf("\n");
                printf("Posting message to all currently connected users...\n");
                i = 0;
                while(i != mesg_count){
                    read_message(sd, buf);
                    printf("%s", buf);
                    ++i;
                }
                break;
                
           case '5':
                
                //ask user for message
                printf("Enter a message: ");
                fgets(buf, sizeof(buf), stdin);
                
                //get system time
                format_time(time);
                
                //send user's name, time, and message to server
                send_message(sd, name);
                send_message(sd, time);
                send_message(sd, buf);
                
                // read message count from server
                read_message(sd, mesg_count_s);
                
                // convert message count into integer
                mesg_count = atoi(mesg_count_s);
                
                // read sending status from server
                printf("\n");
                printf("Posting message to all known users...\n");
                i = 0;
                while(i != mesg_count){
                    read_message(sd, buf);
                    printf("%s", buf);
                    ++i;
                }
                
               break;
                
           case '6':
                // send the name of the user who wants to get his messages
                send_message(sd, name);
                
                // read message count from server
                read_message(sd, mesg_count_s);
                // convert message count into integer
                mesg_count = atoi(mesg_count_s);
                
                // read message from server
                i = 0;
                printf("\nYour messages:\n");
                while(i != mesg_count){
                    read_message(sd, buf);
                    printf("   %s", buf);
                    ++i;
                }
                printf("\n");
               break;
                
           case '7':
               flag = 1;
               // send the name of the user who is going to exit
               send_message(sd, name);
               break;
           default:
               printf("Wrong choice\n");
               break;
        }
       
    }

   /* close the socket */
   close(sd);
}



void  printMenu(){
    printf("\n1. Display the names of all known users.\n"
           "2. Display the names of all currently connected users.\n"
           "3. Send a text message to a particular user.\n"
           "4. Send a text message to all currently connected users.\n"
           "5. Send a text message to all known users.\n"
           "6. Get my messages.\n"
           "7. Exit.\n"
           "Enter your choice: ");
}
                       
void clearInputBuffer(){
    char c;
    do{
        c = getchar();
    }while(c != '\n' && c != EOF);
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

void format_time(char *buffer){
    time_t timer;
    struct tm* tm_info;
    
    time(&timer);
    tm_info = localtime(&timer);
    
    strftime(buffer, 26, "%m/%d/%y %I:%M %p", tm_info);
}






