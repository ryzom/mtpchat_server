/* Copyright, 2001 MELTING POT.
 *
 * This file is part of MTP CHAT.
 * MTP CHAT is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.

 * MTP CHAT is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with MTP CHAT; see the file COPYING. If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 */

/* Server.C */

/*

- 17 june  2004: CHANGED: can just change a password without the group
- 10 june  2004: REMOVED: whereis command
- 10 june  2004: CHANGED: don't display host in twin login and finger
- 19 april 2004: ADDED: command AddUser to add users :)
- 19 april 2004: ADDED: NEWUSERSFILE variable in database
- 19 april 2004: CHANGED: don't display ip and history for people < GM
- 09 april 2004: ADDED: don't display the user password on the chat
- 09 april 2004: ADDED: command sendata and .whoall for klients auto sent commands
- 09 april 2004: ADDED: all commands are prefixed by a '.'
-              : REMOVED: wall


*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#ifdef _WIN32

#include <io.h>
#include <Windows.h>
#include <direct.h>
#include "telnet_win32.h"
#undef DeleteFile

#else
#include <unistd.h>
#include <netdb.h>
#include <pwd.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <syslog.h>

#include <arpa/telnet.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#include "server.h"
#include "types.h"
#include "channel.h"
#include "command.h"
#include "database.h"
#include "group.h"
#include "list.h"
#include "mtp.h"
#include "object.h"
#include "socket.h"
#include "token.h"
#include "user.h"
#include "variable.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Constants */

#define FAILURE_MAX 3

#define SERVER_PATH "."

#ifndef LOG_PATH
#define LOG_PATH "log"
#endif

#ifndef MESSAGES_PATH
#define MESSAGES_PATH "messages"
#endif

#ifndef PID_FILE

#ifdef TARGET
#define PID_FILE "/var/run/"TARGET".pid"
#else
#define PID_FILE "/var/run/mtpchat.pid"
#endif

#endif

#define SERVFILE "activeserver"

#define SERVER_NAME        SERVER_HEADER" Chat v1.82f_acemod2"
#define WELCOME_MESSAGE    "Welcome to "SERVER_HEADER" Chat !"

#define PORTAL_HOST        "queen.eudil.fr"
#define HOST_PROMPT        SERVER_HEADER" Host: "

#define LOGIN_PROMPT       SERVER_HEADER" Login: "
#define INV_LOGIN_MSG      SERVER_HEADER" Invalid login, choose another one\n"
#define RESERVED_LOGIN_MSG SERVER_HEADER" Reserved login, choose another one\n"
#define TWIN_LOGIN_MSG     SERVER_HEADER" You are already logged in, try with another login name\n"
#define REG_ONLY_MSG       SERVER_HEADER" Only registered users are allowed to login at the moment...\n"

#define PASSWORD_PROMPT    SERVER_HEADER" Password: "
#define WRONG_PASSWORD_MSG SERVER_HEADER" Incorrect password\n"

/* "Constants" */

static const uchar EchoOn[]     = { IAC, WONT, TELOPT_ECHO, '\0' };
static const uchar EchoOff[]    = { IAC, WILL, TELOPT_ECHO, '\0' };

static const uchar LineOn[]     = { IAC, DO, TELOPT_LINEMODE, '\0' };
static const uchar LineOff[]    = { IAC, DONT, TELOPT_LINEMODE, '\0' };

static const uchar TimingMark[] = { IAC, WILL, TELOPT_TM, '\0' };

#ifndef _WIN32
static const int Signal[] = {
   SIGABRT, SIGALRM, SIGCHLD, SIGFPE,  SIGHUP,  SIGILL,  SIGINT,  SIGPIPE,
   SIGQUIT, SIGSEGV, SIGTERM, SIGTSTP, SIGTTIN, SIGTTOU, SIGUSR1, SIGUSR2
};
#endif

/* Variables */

time_t  RebootTime;
int     UserNb;
int     UserNbMax;
int     RegOnly;
history LogHistory[1];
history WallHistory[1];
int     statechanged=0;
int     socketinstalled=0;
char    ActiveServer[1024];


static const char **SignalName;
static list PendingList[1];

/* Prototypes */

static void  InitProcess (void);
static void  InitSignal  (void);
static void  SigHandler  (int Sig);

static void  Server      (void);

static void  Login       (user *User);

static void  GetActiveServer (void);


/* Functions */

/* Main() */

int main(int argc, char *argv[]) {

   int      Port;
   channel *Channel;

   InitProcess();
   InitSignal();

   srand(time(NULL));

   GetActiveServer();
   while(TRUE){
       statechanged=0;
       InitList(Lists,"Lists");
       InitList(UserList,"Users");
       AddTail(Lists,UserList);
       InitList(GroupList,"Groups");
       AddTail(Lists,GroupList);
       InitList(ChannelList,"Channels");
       AddTail(Lists,ChannelList);

       InitList(PendingList,"Pendings");

       Trace(INOUT_LOG,"[restart]");
       InitHistory(LogHistory,20);
       LoadHistory(LOG_PATH "/inout",LogHistory);
       InitHistory(WallHistory,20);
       LoadHistory(LOG_PATH "/wall",WallHistory);

       if (! ReadDataBase()) FatalError("Couldn't read database");

       Channel = NewChannel(DEFAULT_CHANNEL);
       if (Channel == NULL) FatalError("Couldn't create default channel");
       strcpy(Channel->Owner,SUPER_USER);
       strcpy(Channel->Group,"System");
       SetString(&Channel->Topic,WELCOME_MESSAGE);
       Channel->Resident = TRUE;
       AddTail(ChannelList,Channel);

       Channel = NewChannel("Dessin");
       if (Channel == NULL) {
           Error("Couldn't create Dessin channel");
       } else {
           strcpy(Channel->Owner,SUPER_USER);
           strcpy(Channel->Group,"HeadArch");
           Channel->Resident  = TRUE;
           AddTail(ChannelList,Channel);
       }

       Channel = NewChannel("BckStage");
       if (Channel == NULL) {
           Error("Couldn't create BckStage channel");
       } else {
           strcpy(Channel->Owner,SUPER_USER);
           strcpy(Channel->Group,"Guest");
           SetString(&Channel->Topic,"Guests' channel !");
           Channel->Closed    = TRUE;
           Channel->Hidden    = TRUE;
           Channel->Protected = TRUE;
           Channel->Resident  = TRUE;
           AddTail(ChannelList,Channel);
       }

       Channel = NewChannel("HeadChan");
       if (Channel == NULL) {
           Error("Couldn't create HeadChan channel");
       } else {
           strcpy(Channel->Owner,SUPER_USER);
           strcpy(Channel->Group,"HeadArch");
           SetString(&Channel->Topic,"HeadArchs' channel !");
           Channel->Hidden    = TRUE;
           Channel->Invite    = TRUE;
           Channel->Protected = TRUE;
           Channel->Resident  = TRUE;
           AddTail(ChannelList,Channel);
       }

       Channel = NewChannel("RootChan");
       if (Channel == NULL) {
           Error("Couldn't create RootChan channel");
       } else {
           strcpy(Channel->Owner,SUPER_USER);
           strcpy(Channel->Group,"System");
           SetString(&Channel->Topic,"Administrators' channel !");
           Channel->Hidden    = TRUE;
           Channel->Invite    = TRUE;
           Channel->Protected = TRUE;
           Channel->Resident  = TRUE;
           AddTail(ChannelList,Channel);
       }

       Port = (argc > 1) ? atoi(argv[1]) : PORT_NUM;
       if(!socketinstalled){
           InitSocket();
           if (InstallSocket(Port))
               socketinstalled=1;
           else FatalError("Couldn't install server socket");
       }

       RebootTime = time(NULL);
       UserNb     = 0;

       Server();

       /* XXX must free memory !
        */

       ClearList(UserList);
       ClearList(GroupList);
       ClearList(ChannelList);
       ClearList(PendingList);

       ClearList(Lists);


       ClearHistory(LogHistory,20);
       ClearHistory(WallHistory,20);

   }

   Exit();

   return EXIT_SUCCESS;
}

/* GetActiveServer () */

static void  GetActiveServer (void){
    FILE *servfile=NULL;

    servfile=fopen(SERVFILE, "r");
    ActiveServer[0]='\0';

    if(servfile){
        char *ret = fgets(ActiveServer, sizeof(ActiveServer), servfile);
        fclose(servfile);
    }

}


/* InitProcess() */

#ifdef _WIN32

static void InitProcess(void) {

   char UserName[NAME_SIZE+1], WorkDir[STRING_SIZE], TerminalName[NAME_SIZE+1], *TTyName;
   int    UMask;
   DWORD buffer_len;
   
   if (!GetUserNameA(UserName, &buffer_len)) {
	   Error("Couldn't get user name");
   }

   UMask = umask(_S_IREAD | _S_IWRITE);

   if (getcwd(WorkDir,STRING_SIZE) == NULL) {
      Error("Couldn't find server work directory");
   }

   TTyName = ""; // TTY name
   if (TTyName == NULL) {
      Warning("Couldn't get stdin tty name");
      strcpy(TerminalName,"<None>");
   } else {
      strncpy(TerminalName,TTyName,NAME_SIZE+1);
   }

   printf("\n");
   printf("UserName = %s\n",UserName);
   printf("UMask    = %03o\n",UMask);
   printf("WorkDir  = %s\n",WorkDir);
   printf("Terminal = %s\n",TerminalName);
   printf("\n");

   umask(_S_IREAD | _S_IWRITE);

   if (chdir(SERVER_PATH) == -1) {
      FatalError("Couldn't find server directory \"%s\"",SERVER_PATH);
   }
}

#else

static void InitProcess(void) {

   uid_t  UId, GId, EUId, EGId;
   int    Nice;
   mode_t UMask;
   char   UIdName[NAME_SIZE+1], EUIdName[NAME_SIZE+1], WorkDir[STRING_SIZE], TerminalName[NAME_SIZE+1], *TTyName;
   struct passwd *Password;

   /* Our process ID and Session ID */
   pid_t pid, sid;
        
   /* Fork off the parent process */
   pid = fork();
   if (pid < 0) {
      FatalError("Unable to fork parent process");
   }

   /* If we got a good PID, then we can exit the parent process. */
   if (pid > 0) {
      exit(EXIT_SUCCESS);
   }

   UId = getuid();
   GId = getgid();
   Password = getpwuid(UId);
   if (Password == NULL) {
      Error("Unknown UId : %d",UId);
      strcpy(UIdName,"");
   } else {
      strncpy(UIdName,Password->pw_name,NAME_SIZE+1);
   }

   EUId = geteuid();
   EGId = getegid();
   Password = getpwuid(EUId);
   if (Password == NULL) {
      Error("Unknown EUId : %d",EUId);
      strcpy(EUIdName,"");
   } else {
      strncpy(EUIdName,Password->pw_name,NAME_SIZE+1);
   }

   Nice = nice(0);

   UMask = umask((mode_t)(S_IWGRP|S_IWOTH));

   if (getcwd(WorkDir,STRING_SIZE) == NULL) {
      Error("Couldn't find server work directory");
   }

   TTyName = ttyname(STDIN_FILENO);
   if (TTyName == NULL) {
      Warning("Couldn't get stdin tty name");
      strcpy(TerminalName,"<None>");
   } else {
      strncpy(TerminalName,TTyName,NAME_SIZE+1);
   }

   printf("\n");
   printf("UId      = %s (%d)\n",UIdName,UId);
   printf("GId      = %d\n",GId);
   printf("EUId     = %s (%d)\n",EUIdName,EUId);
   printf("EGId     = %d\n",EGId);
   printf("Nice     = %+d\n",Nice);
   printf("UMask    = %03o\n",UMask);
   printf("WorkDir  = %s\n",WorkDir);
   printf("Terminal = %s\n",TerminalName);
   printf("\n");

   umask((mode_t)(S_IRWXG|S_IRWXO));

   /* Create a new SID for the child process */
   sid = setsid();
   if (sid < 0) {
      FatalError("Unable to create SID for the child process");
   }
   
   if (chdir(SERVER_PATH) == -1) {
      FatalError("Couldn't find server directory \"%s\"",SERVER_PATH);
   }

   /* Close out the standard file descriptors */
   close(STDIN_FILENO);
   close(STDOUT_FILENO);
   close(STDERR_FILENO);

   CreatePidFile();   
}

#endif

#ifdef _WIN32
BOOL CtrlHandler(DWORD fdwCtrlType) { 

   Warning("Signal #%u received", fdwCtrlType);
   Trace(SIGNAL_LOG,"#%d", fdwCtrlType);
	
   switch (fdwCtrlType) {
    // Handle the CTRL-C signal.
    case CTRL_C_EVENT:
    Exit();
    return TRUE;
 
    // CTRL-CLOSE: confirm that the user wants to exit. 
    case CTRL_CLOSE_EVENT: 
    printf( "Ctrl-Close event\n\n" );
    return( TRUE ); 
 
    // Pass other signals to the next handler. 
    case CTRL_BREAK_EVENT:
    printf( "Ctrl-Break event\n\n" );
    return FALSE;

    case CTRL_LOGOFF_EVENT: 
    printf( "Ctrl-Logoff event\n\n" );
    return FALSE; 
 
    case CTRL_SHUTDOWN_EVENT: 
    printf( "Ctrl-Shutdown event\n\n" );
    return FALSE; 
 
    default: 
    return FALSE;
// RestartServer()
  }
}

#endif

/* InitSignal() */

static void InitSignal(void) {

#ifdef _WIN32
   BOOL res = SetConsoleCtrlHandler((PHANDLER_ROUTINE) CtrlHandler, TRUE);
#else
   int I, SignalNb;
   struct sigaction SigAction[1];

   SignalNb = 0;
   for (I = 0; I < (int)(sizeof(Signal)/sizeof(Signal[0])); I++) {
      if (Signal[I] >= SignalNb) SignalNb = Signal[I] + 1;
   }

   SignalName = malloc((size_t)(SignalNb*sizeof(char *)));
   if (SignalName == NULL) FatalError("Not enough memory for signal names");
   for (I = 0; I < SignalNb; I++) SignalName[I] = NULL;

   SigAction->sa_handler = SigHandler;
   sigemptyset(&SigAction->sa_mask);
   SigAction->sa_flags = SA_RESTART;

   for (I = 0; I < (int)(sizeof(Signal)/sizeof(Signal[0])); I++) {
      switch (Signal[I]) {
         case SIGABRT : SignalName[SIGABRT] = "SIGABRT"; break;
         case SIGALRM : SignalName[SIGALRM] = "SIGALRM"; break;
         case SIGCHLD : SignalName[SIGCHLD] = "SIGCHLD"; break;
         case SIGFPE  : SignalName[SIGFPE]  = "SIGFPE";  break;
         case SIGHUP  : SignalName[SIGHUP]  = "SIGHUP";  break;
         case SIGILL  : SignalName[SIGILL]  = "SIGILL";  break;
         case SIGINT  : SignalName[SIGINT]  = "SIGINT";  break;
         case SIGPIPE : SignalName[SIGPIPE] = "SIGPIPE"; break;
         case SIGQUIT : SignalName[SIGQUIT] = "SIGQUIT"; break;
         case SIGSEGV : SignalName[SIGSEGV] = "SIGSEGV"; break;
         case SIGTERM : SignalName[SIGTERM] = "SIGTERM"; break;
         case SIGTSTP : SignalName[SIGTSTP] = "SIGTSTP"; break;
         case SIGTTIN : SignalName[SIGTTIN] = "SIGTTIN"; break;
         case SIGTTOU : SignalName[SIGTTOU] = "SIGTTOU"; break;
         case SIGUSR1 : SignalName[SIGUSR1] = "SIGUSR1"; break;
         case SIGUSR2 : SignalName[SIGUSR2] = "SIGUSR2"; break;
      }
      sigaction(Signal[I],SigAction,NULL);
   }
#endif
}

/* SigHandler() */

static void RestartServer(){
   node   *Node, *NodeNext;
   user   *User;

    statechanged=1;

    GetActiveServer();

    for (Node = UserList->Head; Node != NULL; Node = NodeNext) {
        User     = (user *) Node->Object;
        NodeNext = Node->Next;
        if (User->State != NOT_LOGGED &&  IsAliveConn(User->Conn)) {
            if (User->Conn->Error == NULL) {
                SendUser(User, SERVER_HEADER" Main server moved at %s\n",
                         ActiveServer);
                Logout(User,NULL,"");
            }
        }
    }

}

#ifndef _WIN32

static void SigHandler(int Sig) {

   if (SignalName[Sig] != NULL) {
      Warning("%s received",SignalName[Sig]);
      Trace(SIGNAL_LOG,"%s",SignalName[Sig]);
   } else {
      Warning("Signal #%d received",Sig);
      Trace(SIGNAL_LOG,"#%d",Sig);
   }

   switch (Sig) {

      case SIGABRT :
      case SIGFPE  :
      case SIGILL  :
      case SIGINT  :
      case SIGQUIT :
      case SIGSEGV :
      case SIGTERM : Exit();
                     break;

      case SIGCHLD : wait(NULL);
                     break;
      case SIGUSR1 : RestartServer();
                     break;
   }
}

#endif

/* Server() */

static void Server(void) {

   node   *Node, *NodeNext;
   user   *User, *UId;
   object *Object;
   char   *UserString;

   while (!statechanged) {

      UpdateConn();

      for (Node = PendingList->Head; Node != NULL; Node = NodeNext) {
         User     = (user *) Node->Object;
         NodeNext = Node->Next;
         if (User->State == NOT_LOGGED || ! IsAliveConn(User->Conn)) {
            RemObject(PendingList,User);
            DeleteUser(User);
         }
      }

      for (Node = UserList->Head; Node != NULL; Node = NodeNext) {
         User     = (user *) Node->Object;
         NodeNext = Node->Next;
         if (User->State != NOT_LOGGED && ! IsAliveConn(User->Conn)) {
            if (User->Conn->Error != NULL) {
               Logout(User,NULL,SERVER_HEADER" %s disconnects (%s) !\n",User->Id,User->Conn->Error);
            } else {
               Logout(User,NULL,SERVER_HEADER" %s disconnects !\n",User->Id);
            }
         }
         if (User->State == NOT_LOGGED && ! User->Registered) {
            RemObject(UserList,User);
            DeleteUser(User);
         }
      }

	  /* Check if the file with new users exists */
	  CheckNewUsersFile();

      if (DataBaseChanged) WriteDataBase();

      for (Node = PendingList->Head; Node != NULL; Node = NodeNext) {

         User     = (user *) Node->Object;
         NodeNext = Node->Next;

         if (User->State != NOT_LOGGED && IsAliveConn(User->Conn) && User->Conn->In->Line != 0) {

            UserString = GetString(User->Conn);
            if (UserString == NULL) {
               Error("Server(): GetString() = NULL");
               break;
            }

            User->LastSendTime = time(NULL);

            switch (User->State) {

            case HOST :

               SetString(&User->Host,UserString);
               User->State = LOGIN;
               SendUser(User,LOGIN_PROMPT);
               break;

            case LOGIN :

               if (UserString[0] == '\0') {
                  SendUser(User,LOGIN_PROMPT);
                  Trace(INOUT_LOG,"NULL %-8.8s %-8.8s %s","Unknown","Unknown",User->Host);
               } else if (! IsId(UserString)) {
                  SendUser(User,INV_LOGIN_MSG);
                  SendUser(User,LOGIN_PROMPT);
                  Trace(INOUT_LOG,"INV  %-8.8s %-8.8s %s",UserString,"Unknown",User->Host);
               } else if ((Object = SearchObject(Lists,UserString,OBJECT)) != NULL
                          && Object->Type != USER) {
                  SendUser(User,RESERVED_LOGIN_MSG);
                  SendUser(User,LOGIN_PROMPT);
                  Trace(INOUT_LOG,"RES  %-8.8s %-8.8s %s",Object->Id,"Unknown",User->Host);
               } else {
                  UId = SearchUId(UserString);
                  if (UId == NULL) { /* Unregistered user */
                     if (SearchUser(UserString) != NULL) { /* Already logged in */
                        SendUser(User,TWIN_LOGIN_MSG);
                        SendUser(User,LOGIN_PROMPT);
                        Trace(INOUT_LOG,"TWIN %-8.8s %-8.8s %s",UserString,"Unknown",User->Host);
                     } else { /* New unregistered user */
                        if (RegOnly) {
                           SendUser(User,REG_ONLY_MSG);
                           SendUser(User,LOGIN_PROMPT);
                           Trace(INOUT_LOG,"BAN  %-8.8s %-8.8s %s",UserString,"Unknown",User->Host);
                        } else {
                           RemObject(PendingList,User);
                           InitUser(User,UserString);
                           AddTail(UserList,User);
                           Login(User);
                        }
                     }
                  } else {
                     strcpy(User->Id,UId->Id);
                     strcpy(User->Group,UId->Group);
                     strcpy(User->Password,UId->Password);
                     User->State = PASSWORD;
                     SendString(User->Conn,(const char *)EchoOff);
                     SendUser(User,PASSWORD_PROMPT);
                  }
               }
               break;

            case PASSWORD :

               SendString(User->Conn,(const char *)EchoOn);
               SendUser(User,"\n");

               if (! CheckPassword(UserString,User->Password)) {
                  /* Wrong password */

                  SendUser(User,WRONG_PASSWORD_MSG);
                  SendUser(User,LOGIN_PROMPT);

                  if (! IsEmptyString(UserString)) {

                     Trace(PASSWORD_LOG,"WRONG %-12.12s %-8.8s %s typed a wrong password",User->Id,User->Group,User->Host);
                     Warning("\"%s\" typed a wrong password",User->Id);

                     UId = SearchUId(User->Id);
                     if (UId == NULL) {
                        SendUser(User,SERVER_HEADER" You have been killed !\n");
                        Trace(INOUT_LOG,"KILL %-12.12s %-8.8s %s",User->Id,User->Group,User->Host);
                        Error("Couldn't find registered user \"%s\" in database",User->Id);
                        CloseConn(User->Conn);
                        continue;
                     } else {
                        UId->FailureNb++;
                        SetString(&UId->LastFailHost,User->Host);
                        UId->LastFailTime = time(NULL);
                        DataBaseChanged = TRUE;
                        if (UId->FailureNb >= FAILURE_MAX) {
                           Trace(SECURITY_LOG,"PASSWORD %-12.12s %-8.8s %s",User->Id,User->Group,User->Host);
                        }
                     }
                  }

                  InitUser(User,DEFAULT_USER);
                  User->State = LOGIN;

               } else {

                  UId = SearchUId(User->Id);
                  if (UId == NULL) { /* Unknown error */
                     SendUser(User,SERVER_HEADER" You have been killed !\n");
                     Trace(INOUT_LOG,"KILL %-12.12s %-8.8s %s",User->Id,User->Group,User->Host);
                     Error("Couldn't find registered user \"%s\" in database",User->Id);
                     CloseConn(User->Conn);
                     continue;
                  }
                  if (UId->State == LOGGED) {
                     SendUser(User,SERVER_HEADER" You kick %s out (twin login) !\n",UId->Id);
                     SendUser(UId,SERVER_HEADER" You are kicked out by %s (twin login from %s) !\n",User->Id,User->Host);
                     Logout(UId,User,SERVER_HEADER" %s is kicked out by %s (twin login) !\n",UId->Id,User->Id);
                  }

                  RemObject(PendingList,User);
                  UId->Conn = User->Conn;
                  SetString(&UId->Host,User->Host);
                  DeleteUser(User);
                  SetConnObject(UId->Conn,UId);
                  Login(UId);
               }

               break;
            }
         }
      }

      for (Node = UserList->Head; Node != NULL; Node = NodeNext) {

         User     = (user *) Node->Object;
         NodeNext = Node->Next;

         if (IsConnected(User) && User->Conn->In->Line != 0) {

            UserString = GetString(User->Conn);
            if (UserString == NULL) {
               Error("Server(): GetString() = NULL");
               continue;
            }

            User->LastSendTime = time(NULL);

            if (UserString[0] != '\0') Execute(User,UserString,FALSE); /* No force mode */
         }
      }
   }
}

/* IsAllowedConn() */

int IsAllowedConn(const char *HostAddr, const char *HostName) {

   return TRUE;
}

/* AddConn() */

void AddConn(conn *Conn, const char *HostAddr, const char *HostName) {

   user *User;

   User = NewUser(DEFAULT_USER);
   if (User == NULL) {
      Error("AddConn(): Couldn't add new user");
      CloseConn(Conn);
      return;
   }

   SetString(&User->Host,(HostName!=NULL)?HostName:HostAddr);
   User->State = (SameString(User->Host,PORTAL_HOST)) ? HOST : LOGIN;
   User->Conn  = Conn;
   AddTail(PendingList,User);

   SetConnObject(Conn,User);

   SendHelpFile(User,"logo");
   SendUser(User,SERVER_NAME "\n");
   switch (User->State) {
   case HOST :
      SendUser(User,HOST_PROMPT);
      break;
   case LOGIN :
       if(*ActiveServer){
           SendUser(User,SERVER_HEADER" Active server is at %s\n", ActiveServer);
           Logout(User,NULL,"");
       }else SendUser(User,LOGIN_PROMPT);
      break;
   }
}

/* RemConn() */

void RemConn(const conn *Conn, void *Object) {

   user *User;

   User = Object;

   if (User == NULL) {
/*
      Error("RemConn(): User = NULL");
*/
      return;
   } else if (User->Conn != Conn) {
      Error("RemConn(): User->Conn != Conn");
      return;
   }

   if (User->Conn->Error != NULL) {
      Logout(User,NULL,SERVER_HEADER" %s disconnects (%s) !\n",User->Id,User->Conn->Error);
   } else {
      Logout(User,NULL,SERVER_HEADER" %s disconnects !\n",User->Id);
   }

   User->Conn = NULL;
}

/* Login() */

static void Login(user *User) {

   channel *Channel;
   char FileName[STRING_SIZE], TimeString[32];
   int MsgNb;

   Trace(INOUT_LOG,"Login()");
   Channel = SearchChannel(DEFAULT_CHANNEL);
   if (Channel == NULL) {
      SendUser(User,SERVER_HEADER" Couldn't find default channel !\n");
      Trace(INOUT_LOG,"CHAN %-12.12s %-8.8s %s",User->Id,User->Group,User->Host);
      FatalError("Couldn't find default channel");
   }

   User->State        = LOGGED;
   User->ConnectTime  = time(NULL);
   User->LastSendTime = time(NULL);
   User->LoginNb++;

   if (User->Registered) {

      InitUId(User);

      if (! IsEmptyString(User->LastLoginHost)) {
         strftime(TimeString,(size_t)31,"%x %X",localtime(&User->LastLoginTime));
         SendUser(User,SERVER_HEADER" Last login: %s from %s\n",TimeString,User->LastLoginHost);
      }

      if (User->FailureNb > 0) {
         SendUser(User,SERVER_HEADER" %d login failure%s since last login\n",User->FailureNb,(User->FailureNb>1)?"s":"");
         if (! IsEmptyString(User->LastFailHost)) {
            strftime(TimeString,(size_t)31,"%x %X",localtime(&User->LastFailTime));
            SendUser(User,SERVER_HEADER" Last failure: %s from %s\n",TimeString,User->LastFailHost);
         }
         User->FailureNb = 0;
      }

      DataBaseChanged = TRUE;
   }

   SendUser(User,SERVER_HEADER" Welcome, %s.\n",User->Id);
   Trace(INOUT_LOG,"IN   %-12.12s %-8.8s %s",User->Id,User->Group,User->Host);
   AddHistory(LogHistory,"IN   %-12.12s %-8.8s %s",User->Id,User->Group,User->Host);

   if (! SendHelpFile(User,"motd")) {
      SendUser(User,SERVER_HEADER" No motd\n");
   }

   if (User->Registered) {
      sprintf(FileName,MESSAGES_PATH"/%s",User->Id);
      if (FileExists(FileName)) {
	 SendUser(User,SERVER_HEADER" Your message(s) :\n");
	 MsgNb = SendFileWithLineNb(User,FileName);
	 if (MsgNb > 1) {
	    SendUser(User,SERVER_HEADER" You have %d messages\n",MsgNb);
	 } else if (MsgNb == 1) {
	    SendUser(User,SERVER_HEADER" You have %d message\n",MsgNb);
	 } else {
	    Warning("Empty message file \"%s\"",FileName);
	    SendUser(User,SERVER_HEADER" You have no message !\n");
	 }
      } else {
	 SendUser(User,SERVER_HEADER" You have no message !\n");
      }
   }

   strcpy(User->Channel,"");
   JoinChannel(User,Channel);
   SendInOut(User,SERVER_HEADER" %s comes in !\n",User->Id);

   UserNb++;
   if (UserNb > UserNbMax) {
      UserNbMax = UserNb;
      DataBaseChanged = TRUE;
      SendInOut(NULL,SERVER_HEADER" Record broken : %d user%s !\n",UserNbMax,(UserNbMax>1)?"s":"");
   }
}

/* Logout() */

void Logout(user *User, const user *Except, const char *Message, ...) {

   va_list Args;
   char    String[STRING_SIZE];

   if (User->State == NOT_LOGGED) {
      Error("Logout(): User->State == NOT_LOGGED");
      return;
   }

   SetConnObject(User->Conn,NULL);

   if (IsAliveConn(User->Conn)) {
/*
      if (User->Conn->Error != NULL) SendUser(User,SERVER_HEADER" You disconnect (%s)\n",User->Conn->Error);
*/
      CloseConn(User->Conn);
   }

   if (User->Registered) {
      User->TotalTime += (int) (time(NULL) - User->ConnectTime);
      SetString(&User->LastLoginHost,User->Host);
      User->LastLoginTime = User->ConnectTime;
      DataBaseChanged = TRUE;
   }

   if (User->State == LOGGED) {
      if (User->Channel[0] != '\0') LeaveChannel(User);
      va_start(Args,Message);
      vsprintf(String,Message,Args);
      va_end(Args);
      SendInOut(Except,"%s",String);
      UserNb--;
      AddHistory(LogHistory,"OUT  %-12.12s %-8.8s %s",User->Id,User->Group,User->Host);
      Trace(INOUT_LOG,"OUT  %-12.12s %-8.8s %s",User->Id,User->Group,User->Host);
   }

   User->State = NOT_LOGGED;

}

/* InitHistory() */

void InitHistory(history *History, int Size) {

   int I;

   History->Size = Size;
   History->String = malloc((size_t)(Size*sizeof(char *)));
   if (History->String == NULL) FatalError("Not enough memory for history");
   for (I = 0; I < Size; I++) History->String[I] = NULL;
}

/* ClearHistory() */

void ClearHistory(history *History, int Size){
    int i;

    for (i = 0; i < Size; i++)
        if(History->String[i]) free(History->String[i]) ;
    free(History->String);
}

/* LoadHistory() */

void LoadHistory(const char *FileName, history *History) {

   int I;
   FILE *File;
   char String[STRING_SIZE];

   File = fopen(FileName,"r");
   if (File == NULL) return;

   printf("Loading file \"%s\"...\n",FileName);
   while (fgets(String,STRING_SIZE,File) != NULL) {
      if (String[0] != '\0' && String[strlen(String)-1] == '\n') {
         String[strlen(String)-1] = '\0';
      }
      DeleteString(&History->String[0]);
      for (I = 0; I < History->Size-1; I++) {
         History->String[I] = History->String[I+1];
      }
      History->String[History->Size-1] = NewString(String);
   }

   fclose(File);
}

/* AddHistory() */

void AddHistory(history *History, const char *Message, ...) {

   int     I;
   char    TimeString[32];
   time_t  Time;
   va_list Args;
   char    String[STRING_SIZE];

   DeleteString(&History->String[0]);
   for (I = 0; I < History->Size-1; I++) {
      History->String[I] = History->String[I+1];
   }

   Time = time(NULL);
   strftime(TimeString,(size_t)31,"%x %X ",localtime(&Time));

   va_start(Args,Message);
   vsprintf(String,Message,Args);
   va_end(Args);

   History->String[History->Size-1] = malloc(strlen(TimeString)+strlen(String)+1);
   if (History->String[History->Size-1] == NULL) {
      Error("Not enough memory for history");
   } else {
      strcpy(History->String[History->Size-1],TimeString);
      strcat(History->String[History->Size-1],String);
   }
}

/* SendHistory() */

void SendHistory(user *User, const char *HistoryName, const history *History) {

   int I;

   SendUser(User,SERVER_HEADER" %s :\n",HistoryName);
   for (I = 0; I < History->Size; I++) {
      if (History->String[I] != NULL) SendUser(User,"%s\n",History->String[I]);
   }
   SendUser(User,SERVER_HEADER" End of %s\n",HistoryName);
}

void CreatePidFile() {
#ifndef _WIN32
   FILE* pidfile = fopen(PID_FILE, "wb");
   if (pidfile) {
      fprintf(pidfile, "%ld\n", (long)getpid());
      fclose(pidfile);
   }
#endif
}

void DeletePidFile() {
#ifndef _WIN32
   unlink(PID_FILE);
#endif
}

/* End of Server.C */

