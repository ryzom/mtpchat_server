
/* Command.C */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "command.h"
#include "types.h"
#include "channel.h"
#include "crypt.h"
#include "database.h"
#include "group.h"
#include "list.h"
#include "mtp.h"
#include "object.h"
#include "server.h"
#include "socket.h"
#include "token.h"
#include "user.h"
#include "variable.h"

/* Constants */

#define SYSTEMALIASUSER "SoR"

#define ARG_USER        0x01 /* $User */
#define ARG_GROUP       0x02 /* $Group */
#define ARG_CHANNEL     0x04 /* $Channel */
#define ARG_OBJECT      0x08 /* $Object (User | Channel | Group) */
#define ARG_WORD        0x10 /* $Word */
#define ARG_STRING      0x20 /* $String */

#define CMD_WALL        0 /* set to 1 if you want a the wall command*/
#define CMD_WHEREIS     0
#define CMD_PREFIX      "."

/* Types */

typedef struct arglist arglist;

struct arglist {
   int      argtype;
   arglist *next;
   void    *arg;
};

typedef struct command command;

struct command {
   char Name[ID_SIZE+1];
   char Group[ID_SIZE+1];
   int  ForceOk;
   int  Trace;
   char args[80];
   void (*Function)(user *User, command *Command, arglist *ArgList);
   char Usage[60];
};

/* Macros */

#define USAGE(User,Command) SendUser((User),SERVER_HEADER" Usage : %s %s\n",(Command)->Name,(Command)->Usage)

#define DENIED(User,Command) {\
   Trace(DENIED_LOG,"%-8.8s %-8.8s %s",(User)->Id,(User)->Group,(Command)->Name);\
   SendUser((User),SERVER_HEADER" %s : Permission denied !\n",(Command)->Name);\
}

/* Prototypes */

void     KillArgList    (arglist *tete);
int      ArgsType       (const char *ArgStr);
int      AliasExpand    (user *User, alias *Alias, char *Args, char *Expand);
int      ExecuteCommand (user *User, char *UserString, int Force);
command *SearchCommand  (const char *CommandName);
char    *TimeString     (int Time);

void  AddUser  (user *User, command *Command, arglist *ArgList);
void  Alias    (user *User, command *Command, arglist *ArgList);
void  Beep     (user *User, command *Command, arglist *ArgList);
void  Birthday (user *User, command *Command, arglist *ArgList);
void  Bump     (user *User, command *Command, arglist *ArgList);
void  Channels (user *User, command *Command, arglist *ArgList);
void  Clear    (user *User, command *Command, arglist *ArgList);
void  ClearMsg (user *User, command *Command, arglist *ArgList);
void  Commands (user *User, command *Command, arglist *ArgList);
void  Date     (user *User, command *Command, arglist *ArgList);
void  Emote    (user *User, command *Command, arglist *ArgList);
void  Finger   (user *User, command *Command, arglist *ArgList);
void  ForceCmd (user *User, command *Command, arglist *ArgList);
void  Get      (user *User, command *Command, arglist *ArgList);
void  Groups   (user *User, command *Command, arglist *ArgList);
void  Help     (user *User, command *Command, arglist *ArgList);
void  History  (user *User, command *Command, arglist *ArgList);
void  Invite   (user *User, command *Command, arglist *ArgList);
void  Join     (user *User, command *Command, arglist *ArgList);
void  Kick     (user *User, command *Command, arglist *ArgList);
void  Kill     (user *User, command *Command, arglist *ArgList);
void  Leave    (user *User, command *Command, arglist *ArgList);
void  MkGroup  (user *User, command *Command, arglist *ArgList);
void  Open     (user *User, command *Command, arglist *ArgList);
void  Quit     (user *User, command *Command, arglist *ArgList);
void  Register (user *User, command *Command, arglist *ArgList);
void  Reply    (user *User, command *Command, arglist *ArgList);
void  SendData (user *User, command *Command, arglist *ArgList);
void  SendMsg  (user *User, command *Command, arglist *ArgList);
void  Set      (user *User, command *Command, arglist *ArgList);
void  Shout    (user *User, command *Command, arglist *ArgList);
void  ShowMsg  (user *User, command *Command, arglist *ArgList);
void  Shutdown (user *User, command *Command, arglist *ArgList);
void  Switch   (user *User, command *Command, arglist *ArgList);
void  Tell     (user *User, command *Command, arglist *ArgList);
void  UnAlias  (user *User, command *Command, arglist *ArgList);
void  UpTime   (user *User, command *Command, arglist *ArgList);
void  Users    (user *User, command *Command, arglist *ArgList);
#if CMD_WALL
void  Wall     (user *User, command *Command, arglist *ArgList);
#endif
#if CMD_WHEREIS
void  WhereIs  (user *User, command *Command, arglist *Arglist);
#endif
void  Who      (user *User, command *Command, arglist *ArgList);

int NodeCmp   (const user *User1, const user *User2);
int DateCmp   (const user *User1, const user *User2);
int NameCmp   (const user *User1, const user *User2);
int TimeCmp   (const user *User1, const user *User2);
int BirthCmp  (const user *User1, const user *User2);
int KickCmp   (const user *User1, const user *User2);
int KickedCmp (const user *User1, const user *User2);

int AliasCmp  (const alias *Alias1, const alias *Alias2);

/* "Constants" */

static command CommandList[] = {

/* Attention : les differentes declarations d'une commande "aux arguments pres"
 doivent * IMPERATIVEMENT* se suivre ..... */

   { CMD_PREFIX"AddUser",  "HeadArch", FALSE, FALSE, "$Word $Word $Word", AddUser,  "<User> <Password> <Group>"       },

   { CMD_PREFIX"Alias",    REGISTER_GROUP, FALSE, FALSE, "",              Alias,    "[<Alias> [<Command>]]"         },
   { CMD_PREFIX"Alias",    REGISTER_GROUP, FALSE, FALSE, "$Word",         Alias,    "[<Alias> [<Command>]]"         },
   { CMD_PREFIX"Alias",    REGISTER_GROUP, FALSE, FALSE, "$Word $String", Alias,    "[<Alias> [<Command>]]"         },

   { CMD_PREFIX"Beep",     REGISTER_GROUP, TRUE,  FALSE, "$User",         Beep,     "<User>"                        },

   { CMD_PREFIX"Birthday", DEFAULT_GROUP,  TRUE,  FALSE, "",              Birthday, "[<User>]"                      },
   { CMD_PREFIX"Birthday", DEFAULT_GROUP,  TRUE,  FALSE, "$User",         Birthday, "[<User>]"                      },

   { CMD_PREFIX"Bump",     "Guide",    TRUE,  TRUE,  "$User",         Bump,     "<User> [<Reason>]"             },
   { CMD_PREFIX"Bump",     "Guide",    TRUE,  TRUE,  "$User $String", Bump,     "<User> [<Reason>]"             },

   { CMD_PREFIX"Channels", DEFAULT_GROUP,  TRUE,  FALSE, "",              Channels, ""                              },

   { CMD_PREFIX"Clear",    DEFAULT_GROUP,  TRUE,  FALSE, "",              Clear,    ""                              },

   { CMD_PREFIX"ClearMsg", REGISTER_GROUP, FALSE, FALSE, "",              ClearMsg, ""                              },
   { CMD_PREFIX"ClearMsg", REGISTER_GROUP, FALSE, FALSE, "$Word",         ClearMsg, ""                              },
   { CMD_PREFIX"ClearMsg", REGISTER_GROUP, FALSE, FALSE, "$Word $Word",   ClearMsg, ""                              },

   { CMD_PREFIX"Commands", DEFAULT_GROUP,  TRUE,  FALSE, "",              Commands, ""                              },
   { CMD_PREFIX"Cmds",     "GM",           TRUE,  FALSE, "",              Commands, ""                              },

   { CMD_PREFIX"Date",     DEFAULT_GROUP,  TRUE,  FALSE, "",              Date,     ""                              },

   { CMD_PREFIX"Emote",    REGISTER_GROUP, TRUE,  FALSE, "$String",       Emote,    "<Sentence>"                    },

   { CMD_PREFIX"Force",    "HeadArch", TRUE,  TRUE,  "$User $String",     ForceCmd,    "<User> <Command>"              },

   { CMD_PREFIX"Finger",   DEFAULT_GROUP,  TRUE,  FALSE, "",              Finger,   "[<User>]"                      },
   { CMD_PREFIX"Finger",   DEFAULT_GROUP,  TRUE,  FALSE, "$User",         Finger,   "[<User>]"                      },

   { CMD_PREFIX"Help",     DEFAULT_GROUP,  TRUE,  TRUE,  "$Word",         Help,     "[<Topic>]"                     },
   { CMD_PREFIX"Help",     DEFAULT_GROUP,  TRUE,  FALSE, "",              Help,     "[<Topic>]"                     },

   { CMD_PREFIX"Get",      DEFAULT_GROUP,  FALSE, TRUE,  "$String",         Get,      "<User>|<Channel>|<Group> <Variable>" },

   { CMD_PREFIX"Groups",   DEFAULT_GROUP,  TRUE,  FALSE, "",              Groups,   ""                              },

   { CMD_PREFIX"Join",     DEFAULT_GROUP,  TRUE,  FALSE, "",              Join,     "[<Channel>]"                   },
   { CMD_PREFIX"Join",     DEFAULT_GROUP,  TRUE,  FALSE, "$Channel",      Join,     "[<Channel>]"                   },
   { CMD_PREFIX"Join",     DEFAULT_GROUP,  TRUE,  FALSE, "$Word",         Join,     "[<Channel>]"                   },

   { CMD_PREFIX"Kick",     "Guide",    TRUE,  TRUE,  "$User",         Kick,     "<User> [<Reason>]"             },
   { CMD_PREFIX"Kick",     "Guide",    TRUE,  TRUE,  "$User $String", Kick,     "<User> [<Reason>]"             },

   { CMD_PREFIX"Kill",     "System",   TRUE,  TRUE,  "$User",         Kill,     "<User>"                        },

   { CMD_PREFIX"History",  ADMIN_GROUP,    TRUE,  FALSE, "",              History,  ""                              },

   { CMD_PREFIX"Invite",   DEFAULT_GROUP,  TRUE,  FALSE, "$User",         Invite,   "<User>"                        },

   { CMD_PREFIX"Leave",    DEFAULT_GROUP,  TRUE,  FALSE, "",              Leave,    ""                              },

   { CMD_PREFIX"MkGroup",  "System",   TRUE,  TRUE,  "$Word",         MkGroup,  "<Group>"                       },
/* => groupe pas encore cree !!! */

   { CMD_PREFIX"Open",     "System",   TRUE,  TRUE,  "",              Open,     "[<Port>]"                      },
   { CMD_PREFIX"Open",     "System",   TRUE,  TRUE,  "$Word",         Open,     "[<Port>]"                      },

   { CMD_PREFIX"Quit",     "Banned",   TRUE,  FALSE, "$String",       Quit,     "[<Reason>]"                    },
   { CMD_PREFIX"Quit",     "Banned",   TRUE,  FALSE, "",              Quit,     "[<Reason>]"                    },

   { CMD_PREFIX"Register", DEFAULT_GROUP,  FALSE, TRUE,  "$Word",         Register, "<Password>"                    },

   { CMD_PREFIX"R",        REGISTER_GROUP, TRUE,  FALSE, "$String",       Reply,    "<Message>"                     },
   { CMD_PREFIX"Reply",    REGISTER_GROUP, TRUE,  FALSE, "$String",       Reply,    "<Message>"                     },

   { CMD_PREFIX"Set",      DEFAULT_GROUP,  FALSE, TRUE,  "$String",       Set,      "<User>|<Channel>|<Group> <Variable> <Value>" },
   { CMD_PREFIX"Set",      DEFAULT_GROUP,  FALSE, FALSE, "",              Set,      "<User>|<Channel>|<Group> <Variable> <Value>" },
/*  dans un premier temps, on laisse set se demerder.... on verra + tard.....  */

   { CMD_PREFIX"SendData", DEFAULT_GROUP, TRUE,  FALSE, "$User $String", SendData,  "<User> <Message>"             },

   { CMD_PREFIX"SendMsg",  REGISTER_GROUP, FALSE, FALSE, "$User $String", SendMsg,  "<User> <Message>"              },

   { CMD_PREFIX"ShowMsg",  REGISTER_GROUP, TRUE,  FALSE, "",              ShowMsg,  ""                              },

   { CMD_PREFIX"Shutdown", "System",   TRUE,  TRUE,  "",              Shutdown, "[Now]"                         },
   { CMD_PREFIX"Shutdown", "System",   TRUE,  TRUE,  "$Word",         Shutdown, "[Now]"                         },

   { CMD_PREFIX"Shout",    REGISTER_GROUP, TRUE,  FALSE, "$String",       Shout,    "<Message>"                     },

   { CMD_PREFIX"Switch",   DEFAULT_GROUP,  FALSE, TRUE,  "$Word",         Switch,   "[<User>|<Channel>] <Variable>" },

   { CMD_PREFIX"Tell",     REGISTER_GROUP, TRUE,  FALSE, "$User $String", Tell,     "<User> <Message>"              },

   { CMD_PREFIX"UnAlias",  REGISTER_GROUP, FALSE, FALSE, "$Word",         UnAlias,  "<Alias>"                       },

   { CMD_PREFIX"UpTime",   DEFAULT_GROUP,  TRUE,  FALSE, "",              UpTime,   "[<User>]"                      },
   { CMD_PREFIX"UpTime",   DEFAULT_GROUP,  TRUE,  FALSE, "$User",         UpTime,   "[<User>]"                      },

   { CMD_PREFIX"Users",    ADMIN_GROUP,    TRUE,  FALSE, "",              Users,    "[<Option>] [<Group>]"          },
   { CMD_PREFIX"Users",    ADMIN_GROUP,    TRUE,  FALSE, "$Group",        Users,    "[<Option>] [<Group>]"          },
   { CMD_PREFIX"Users",    ADMIN_GROUP,    TRUE,  FALSE, "$Word",         Users,    "[<Option>] [<Group>]"          },
   { CMD_PREFIX"Users",    ADMIN_GROUP,    TRUE,  FALSE, "$Word $Group",  Users,    "[<Option>] [<Group>]"          },

#if CMD_WHEREIS
   { CMD_PREFIX"WhereIs",  DEFAULT_GROUP,  TRUE,  FALSE, "$User",         WhereIs,  "<User>"                        },
#endif

#if CMD_WALL
   { CMD_PREFIX"Wall",     DEFAULT_GROUP,  TRUE,  FALSE, "",              Wall,     "[<Message>]"                   },
   { CMD_PREFIX"Wall",     WALL_GROUP, TRUE,  FALSE, "$String",       Wall,     "[<Message>]"                   },
#endif

   { CMD_PREFIX"Who",      DEFAULT_GROUP,  TRUE,  FALSE, "",              Who,      "[<Channel>|All]"               },
   { CMD_PREFIX"Who",      DEFAULT_GROUP,  TRUE,  FALSE, "$Channel",      Who,      "[<Channel>|All]"               },
   { CMD_PREFIX"Who",      DEFAULT_GROUP,  TRUE,  FALSE, "$Word",         Who,      "[<Channel>|All]"               },

   { "",         "",         0,     0,     "",              NULL,     ""                              }
};

/* Functions */

/* KillArgList() */

void KillArgList (arglist *ArgList) {

   arglist *Next;

   for (; ArgList != NULL; ArgList = Next) {
      Next = ArgList->next;
      if (ArgList->argtype == ARG_WORD && ArgList->arg != NULL) free(ArgList->arg);
      free(ArgList);
   }
}

/* ArgsType() */

int ArgsType(const char *ArgStr) {

   if (StartWith(ArgStr,"$User"))    return ARG_USER;
   if (StartWith(ArgStr,"$Group"))   return ARG_GROUP;
   if (StartWith(ArgStr,"$Channel")) return ARG_CHANNEL;
   if (StartWith(ArgStr,"$Object"))  return ARG_OBJECT;
   if (StartWith(ArgStr,"$Word"))    return ARG_WORD;
   if (StartWith(ArgStr,"$String"))  return ARG_STRING;

   return 0;
}

/* AliasExpand() */

/* renvoie le nombre de lignes generees */

int AliasExpand(user *User, alias *Alias, char *Args, char *Expand) {

   int nblignes = 0, expand_size=1;
   char dummy[1024];
   char *userp, *aliasp, *argsp;
   char *oldargsp, *tmpp;
   char tmpchar;
   int fullargs=1, i;

   memset(Expand, 0, STRING_SIZE+1);
   userp=Expand;
   aliasp=Alias->Command;
   argsp=Args;

   while(*aliasp != '\0' && expand_size <STRING_SIZE-10)
   {
      if(*aliasp == '$')
      {
         switch(*++aliasp)
         {
            case '$' :
                *userp++=*aliasp;
                expand_size++;
                break;
            case '*' :
                if(argsp != NULL)
                {
                    strncpy(userp, argsp, STRING_SIZE-expand_size-1);
                    expand_size+=strlen(argsp);
/*                    while(*userp!= '\0') userp++;*/
                    userp+=strlen(argsp);
                }
                fullargs=0;
                break;
            case '+' : if(argsp == NULL)
                       {
                          SendUser(User, SERVER_HEADER" alias error : \"$+\" at end of line for alias \"%s\"\n", Alias->Id);                              return 0;
                       }
                       tmpp=strpbrk(argsp, "\t ");
                       while(tmpp != NULL && *tmpp==' ') tmpp++;
                       argsp=tmpp;
                       fullargs=0;
                       break;
            case '-' : oldargsp=argsp;
                       argsp=Args;
                       if(argsp == oldargsp) break;
                       while( (tmpp=strpbrk(argsp, "\t ")) < (oldargsp-1) ) {
                          if(!tmpp) break;
                          argsp=tmpp+1;
		       }
                       fullargs=0;
                       break;
            case 'n' :
                *userp++='\0';
                expand_size++;
                nblignes++;
                break; 
            case '1' :
            case '2' :
            case '3' :
            case '4' :
            case '5' :
            case '6' :
            case '7' :
            case '8' :
            case '9' :
                if(argsp == NULL)
                {
                    SendUser(User, SERVER_HEADER" missing argument %d for alias \"%s\"\n", *aliasp-'0', Alias->Id);
                    return 0;
                }
                oldargsp=argsp;
                for(i=1;i< (*aliasp-'0');i++)
                {
                    oldargsp=strpbrk(oldargsp, "\t ");
                    while(oldargsp != NULL && 
                          (*oldargsp==' ' || *oldargsp=='\t')) oldargsp++;
                    if(oldargsp == NULL)
                    {
                        SendUser(User, SERVER_HEADER" missing argument %d for alias \"%s\"\n", *aliasp-'0', Alias->Id);
                        return 0;
                    }
                }
                tmpp=strpbrk(oldargsp, "\t ");
                if(tmpp != NULL)
                {
                    tmpchar=*tmpp;
                    *tmpp='\0';
                }

                strncpy(userp, oldargsp, STRING_SIZE-expand_size-1);
                expand_size+=strlen(oldargsp);
                userp+=strlen(oldargsp);
                if(tmpp != NULL) *tmpp=tmpchar;
                fullargs=0;

                break;
            default : SendUser(User, SERVER_HEADER" Unknown option '%c' for alias \"%s\"\n", *aliasp, Alias->Id);
                      return 0;
         }
         aliasp++;
      }
      else{
          *userp++=*aliasp++;
          expand_size++;
      }
   }
   if(expand_size<STRING_SIZE-10)
       *userp='\0';
   nblignes++;

   Expand[STRING_SIZE-50]=0;

   return nblignes;
}

/* ExecuteCommand() */

int ExecuteCommand(user *User, char *UserString, int Force) {

   int argnb;
   int found=0, exists=0;
/* exists==0 => commande inconnue
   exists==1 => * found == 0 => pas de commande correspondant aux arguments
                * found == 1 => arguments invalides (errorstring)
                * found == 2 => commande trouvee */
   char    *carg, *uargs, uch, *nextarg, errorstring[1000];
   char    *UserArgs, UserChar;
   command *Command=NULL;
   arglist *argshead=NULL, *argscurr=NULL;

   UserArgs = strpbrk(UserString, "\t ");
   if(UserArgs != NULL)
   {
      UserChar=*UserArgs;
      *UserArgs++='\0';
   }

   Command = SearchCommand(UserString); /* trouve la premiere declaration de */
                                        /* commande correspondante */
   while( Command != NULL && SameString(Command->Name,UserString) && found!=2)
   {
     /*
      if (UserLevel(User) >= GroupLevel(SearchGroup(Command->Group)) && (! Force || Command->ForceOk) )
      {
      */
      if (TRUE) { /* unregistered wall bug */
         exists = 1;
         uargs=UserArgs;
         carg=Command->args;
         nextarg=uargs;
         found=2;
         argshead=NULL;
         argnb=1;
         while(carg != NULL && carg[0]!='\0' && found == 2 )
         {
            if(uargs == NULL || uargs[0] == '\0')
            {
               found=1;/* plus d'arguments en stock alors que*/
                       /* la commande en veut encore */
               sprintf(errorstring, SERVER_HEADER" %s : missing arguments\n", UserString);
            }
            else
            {
               if(argshead==NULL)
               {
                  argshead=(arglist *) malloc(sizeof(arglist));
                  argscurr=argshead;
                  argscurr->next=NULL;
                  argscurr->arg=NULL;
               }
               else
               {
                  argscurr->next=(arglist *) malloc(sizeof(arglist));
                  argscurr=argscurr->next;
                  argscurr->next=NULL;
                  argscurr->arg=NULL;
               }
               switch(ArgsType(carg))
               {
                  case ARG_USER    :
                     nextarg=strpbrk(uargs, "\t ");
                     if(nextarg != NULL)
                     {
                        uch=*nextarg;
                        *nextarg='\0';
                     }
                     if(! IsId(uargs))
                     {
                        sprintf(errorstring, SERVER_HEADER" Invalid Login Name %s\n", uargs);
                        found=1;
                     }
                     else
                     {
                        argscurr->arg = SearchObject(UserList,uargs,USER);
                        if(argscurr->arg == NULL)
                        {
                           sprintf(errorstring, SERVER_HEADER" Unknown or unregistered user \"%s\"\n", uargs);
                           found=1;
                        }
                        argscurr->argtype=ARG_USER;
                     }
                     if(nextarg!=NULL) *nextarg=uch;
                     break;

                  case ARG_GROUP   :
                     nextarg=strpbrk(uargs, "\t ");
                     if(nextarg != NULL)
                     {
                        uch=*nextarg;
                        *nextarg='\0';
                     }
                     if(! IsId(uargs))
                     {
                        sprintf(errorstring, SERVER_HEADER" Invalid Group Name %s\n", uargs);
                        found=1;
                     }
                     else
                     {
                        argscurr->arg=SearchObject(GroupList,uargs,GROUP);
                        if(argscurr->arg == NULL)
                        {
                           sprintf(errorstring, SERVER_HEADER" Unknown Group \"%s\"\n", uargs);
                           found=1;
                        }
                        argscurr->argtype=ARG_GROUP;
                     }
                     if(nextarg!=NULL) *nextarg=uch;
                     break;

                  case ARG_CHANNEL :
                     nextarg=strpbrk(uargs, "\t ");
                     if(nextarg != NULL)
                     {
                        uch=*nextarg;
                        *nextarg='\0';
                     }
                     if(! IsId(uargs))
                     {
                        sprintf(errorstring, SERVER_HEADER" Invalid Channel Name %s\n", uargs);
                        found=1;
                     }
                     else
                     {
                        argscurr->arg=SearchObject(ChannelList, uargs, CHANNEL);
                        if(argscurr->arg == NULL)
                        {
                           sprintf(errorstring, SERVER_HEADER" Unknown Channel \"%s\"\n", uargs);
                           found=1;
                        }
                        argscurr->argtype=ARG_CHANNEL;
                     }
                     if(nextarg!=NULL) *nextarg=uch;
                     break;

                  case ARG_OBJECT  :
/* non utilise pour le moment... utilite douteuse..... */
                     break;

/* Attention : le type word est le seul argument a etre COPIE */
                  case ARG_WORD    :
                     nextarg=strpbrk(uargs, " \t");
                     if(nextarg != NULL)
                     {
                        uch=*nextarg;
                        *nextarg='\0';
                     }
                     if(! IsWord(uargs))
                     {
                        found=1;
                        sprintf(errorstring, SERVER_HEADER" Invalid Word \"%s\"\n", uargs);
                     }
                     else
                     {
                        char *tmpstr;
                        tmpstr=(char *) malloc(strlen(uargs)+1);
                        strcpy(tmpstr, uargs);
                        argscurr->arg=(void *)tmpstr;
                        argscurr->argtype=ARG_WORD;
                     }
                     if(nextarg != NULL) *nextarg=' ';

                     break;

                  case ARG_STRING    :
                     nextarg=NULL;
                     argscurr->arg=(void *) uargs;
                     argscurr->argtype=ARG_STRING;
                     break;

               }
            }
            uargs=nextarg;
            if(uargs != NULL) uargs++;
            carg=strpbrk((carg+1), "$");
         }
         if((nextarg != NULL && *nextarg!='\0') || found != 2)
         {
            if(argshead != NULL)
            {
               KillArgList(argshead);
               argshead=NULL;
            }
            Command++;
            if(found==2)
            {
               found=1;
               sprintf(errorstring, SERVER_HEADER" %s : Too many arguments\n", UserString);
            }
         }
      }
      else
      {
         found=2;
         Command++;
      }
   }
   if(exists)
   {
      if(found==0)
      {
         Command--;
         USAGE(User, Command);
         return FALSE;
         /* la commande existe, mais les arguments sont incorrects */
      }
      else if(found==1)
      {
         SendUser(User, "%s", errorstring);
         return FALSE;
      }
   }

   if (exists && Command != NULL && Command->Function != NULL 
    && UserLevel(User) >= GroupLevel(SearchGroup(Command->Group))
    && (! Force || Command->ForceOk)) {
      if (Command->Trace) {
         if (UserArgs != NULL) {
            Trace(COMMAND_LOG,"%-12.12s %-8.8s %s %s",User->Id,User->Group,UserString,UserArgs);
         } else {
            Trace(COMMAND_LOG,"%-12.12s %-8.8s %s",User->Id,User->Group,UserString);
         }
      }
      (*Command->Function)(User,Command,argshead);
      KillArgList(argshead);
      if (UserArgs != NULL) *--UserArgs = UserChar;
      return TRUE;
   }

   if (strlen(UserString) > 0 && StartWith(UserString, CMD_PREFIX)) {
		 if (exists) {
       /* command not autorised */
       sprintf(errorstring, SERVER_HEADER" %s is an unknown command\n", UserString);
		 } else {
       /* command not found */
       sprintf(errorstring, SERVER_HEADER" %s is an unknown command\n", UserString);
		 }

      SendUser(User, "%s", errorstring);
      return FALSE;
   }

   if (UserArgs != NULL) *--UserArgs = UserChar;
   if (User->Language == LANGUAGE_NONE) return TRUE;
   SendUser(User,"<%s> %s\n",User->Id,UserString);
   if (User->Language != CRYPT_NONE) UserString = Crypt(User->Language,UserString);
   SendChannel(User,"<%s> %s\n",User->Id,UserString);

   return TRUE;
}

/* Execute() */

void Execute(user *User, char *UserString, int Force) {

   int    nblignes, i;
   char  *UserArgs, UserChar, *nextline, String[STRING_SIZE+1], SysString[STRING_SIZE+1];
   char  *UArgs, UChar;
   alias *Alias;

   user *SystemAlias;

   if (UserString[0] == '\0') return;

   UserArgs = strpbrk(UserString,"\t ");
   if (UserArgs != NULL) {
      UserChar = *UserArgs;
      *UserArgs++ = '\0';
   }
   SystemAlias=(user *)SearchObject(UserList, SYSTEMALIASUSER, USER);
   if (IsWord(UserString))
       Alias = SearchAlias(User->Aliases,UserString);
   else
       Alias = NULL;
   if (Alias != NULL) {

      UserString=String;
      nblignes=AliasExpand(User, Alias, UserArgs, UserString);
      if(nblignes == 0) return;
   

      for(i=nblignes;i--;)
      {
         nextline=UserString;
         while(*nextline++ != '\0');
         if (UserString[0] != '\0')
         {
            UArgs = strpbrk(UserString,"\t ");
            if (UArgs != NULL) 
            {
               UChar = *UArgs;
               *UArgs++ = '\0';
            }
            if (IsWord(UserString))
                Alias = SearchAlias(SystemAlias->Aliases,UserString);
            else
                Alias = NULL;
            if (Alias != NULL) 
            {
               UserString=SysString;
               if(AliasExpand(User, Alias, UArgs, UserString)== 0) return;
               if (UserString[0] != '\0') ExecuteCommand(User, UserString, Force);
            }
            else
            {
               if(UArgs != NULL) *--UArgs=UChar;
               ExecuteCommand(User, UserString, Force);
            }
         }
         UserString=nextline;
      }
      return;
   }
   if (IsWord(UserString))
       Alias = SearchAlias(SystemAlias->Aliases,UserString);
   else Alias = NULL;
   if (Alias != NULL) {
      UserString=SysString;
      if(AliasExpand(User, Alias, UserArgs, UserString)== 0) return;
      if (UserString[0] != '\0') ExecuteCommand(User, UserString, Force);
   } else {
      if(UserArgs != NULL) *--UserArgs=UserChar;
      ExecuteCommand(User, UserString, Force);
   }
}

/* SearchCommand() */

command *SearchCommand(const char *CommandName) {

   command *Command;

   for (Command = CommandList; Command != NULL && Command->Function != NULL; Command++) {
      if (SameString(Command->Name,CommandName)) return Command;
   }
   return NULL;
}

/* TimeString() */

char *TimeString(int Time) {

   static char String[10];

   if (Time < 60) {    /* < 1 minute */
      sprintf(String,"%ds",Time);
      return String;
   }
   if (Time < 60*60) { /* < 1 hour */
      sprintf(String,"%dm%02ds",Time/60,Time%60);
      return String;
   }
   Time /= 60;
   if (Time < 24*60) { /* < 1 day */
      sprintf(String,"%dh%02dm",Time/60,Time%60);
      return String;
   }
   Time /= 60;
   sprintf(String,"%dd%02dh",Time/24,Time%24);
   return String;
}

/* AddUser() */
/* ok 0 */

void AddUser(user *User, command *Command, arglist *ArgList) {

  user *UId = NULL;
  char *UserName, *Password, *GroupName;

   if (UserLevel(User) < GroupLevel(SearchGroup("HeadArch"))) {
      DENIED(User,Command);
      return;
   }

   UserName = (char *)ArgList->arg;
   Password = (char *)ArgList->next->arg;
   GroupName = (char *)ArgList->next->next->arg;

   if (! IsId(UserName)) {
     SendUser(User,SERVER_HEADER" '%s' is not a valid username\n",UserName);
     return;
   }

   if (SearchUId(UserName) != NULL) {
     SendUser(User,SERVER_HEADER" User '%s' already exists\n",UserName);
     return;
   }

   if (! IsPassword(Password)) {
     SendUser(User,SERVER_HEADER" '%s' is not a valid password\n",UserName);
     return;
   }

   if (SearchGroup(GroupName) == NULL) {
     SendUser(User,SERVER_HEADER" Group '%s' doesn't exists\n",GroupName);
     return;
   }

   UId = NewUId(UserName,SearchGroup(GroupName)->Id,CryptPassword(Password),NULL,NULL,NULL,time(NULL),0,0,0,0,0,0,NULL,0,0,NULL,0);
   if (UId != NULL) {
     AddTail(UserList,UId);
     DataBaseChanged = TRUE;
     SendUser(User,SERVER_HEADER" You added user '%s'\n",UserName);
     Trace(NEWUSERS_LOG,"User '%s' '%s' '%s' added manually by '%s'", UserName, Password, GroupName, User->Id);
   } else {
     SendUser(User,SERVER_HEADER" Can't create user '%s'\n",UserName);
     Trace(NEWUSERS_LOG,"Can't create manually user '%s'", UserName);
   }
}

/* Alias() */
/* vanhu ok 0 */

void Alias(user *User, command *Command, arglist *ArgList) {

   char  *AliasName, *CommandName;
   node  *Node;
   alias *Alias;
   user *SystemAlias;

   if(ArgList != NULL ) /* on a passe un alias en parametre */
   {
      AliasName=(char *)ArgList->arg;
      if(ArgList->next != NULL ) /* 2nd parametre = expansion string */
      {
         CommandName = (char *) ArgList->next->arg;
         if (! IsWord(AliasName))
         {
            SendUser(User,SERVER_HEADER" Invalid alias name : \"%s\"\n",AliasName);
            return;
         }
         Alias = SearchAlias(User->Aliases,AliasName);
         if (Alias == NULL)
         {
            Alias = NewAlias(AliasName,CommandName);
            if (Alias == NULL)
            {
               SendUser(User,SERVER_HEADER" Couldn't set alias \"%s\" to \"%s\"\n",AliasName,CommandName);
            }
            else
            {
               SendUser(User,SERVER_HEADER" You set alias \"%s\" to \"%s\"\n",AliasName,CommandName);
               AddTail(User->Aliases,Alias);
               DataBaseChanged = TRUE;
            }
         }
         else
         {
            SendUser(User,SERVER_HEADER" You change alias \"%s\" to \"%s\"\n",Alias->Id,CommandName);
            SetString(&Alias->Command,CommandName);
            DataBaseChanged = TRUE;
         }
         SortList(User->Aliases, (CMP_FCT)AliasCmp);
      }
      else /* on veut juste connaitre la "valeur" de l'alias */
      {
         if (! IsWord(AliasName))
         {
            SendUser(User,SERVER_HEADER" Invalid alias name : \"%s\"\n",AliasName);
            return;
         }
         Alias = SearchAlias(User->Aliases,AliasName);
         if (Alias == NULL)
         {
            SystemAlias=(user *)SearchObject(UserList, SYSTEMALIASUSER, USER);
            Alias = SearchAlias(SystemAlias->Aliases,AliasName);
            if(Alias == NULL) SendUser(User,SERVER_HEADER" Unknown alias \"%s\"\n",AliasName);
            else SendUser(User,"%-8s %s (System Alias)\n",Alias->Id,Alias->Command);
         }
         else SendUser(User,"%-8s %s\n",Alias->Id,Alias->Command);
         return;
      }
   }
   else /* on veut la liste des alias */
   {

/* Liste des alias Systeme... */
      SystemAlias=(user *)SearchObject(UserList, SYSTEMALIASUSER, USER);

      if (SystemAlias->Aliases->Head != NULL) {
         SortList(SystemAlias->Aliases, (CMP_FCT)AliasCmp);
         SendUser(User,"\n"SERVER_HEADER" System Aliases : \n");
         for (Node = SystemAlias->Aliases->Head; Node != NULL; Node = Node->Next)
         {
            Alias = (alias *) Node->Object;
            SendUser(User,"%-8s %s\n",Alias->Id,Alias->Command);
         }
      }
      else SendUser(User,"\n"SERVER_HEADER" No System Aliases defined !\n");

      if (User->Aliases->Head != NULL) {
         SortList(User->Aliases, (CMP_FCT)AliasCmp);
         SendUser(User,"\n"SERVER_HEADER" User Aliases : \n");
         for (Node = User->Aliases->Head; Node != NULL; Node = Node->Next)
         {
            Alias = (alias *) Node->Object;
            SendUser(User,"%-8s %s\n",Alias->Id,Alias->Command);
         }
      }
      else SendUser(User,SERVER_HEADER" You don't have any alias !\n");

      SendUser(User,SERVER_HEADER" End of aliases\n");

      return;
   }
}

/* Beep() */
/* vanhu ok 0 */

void Beep(user *User, command *Command, arglist *ArgList) {

   user *Beep;

   Beep = (user *) ArgList->arg; /* inutile de tester: cette commande n'a qu'une possibilite */
   if (! CanSee(User,Beep))
   {
      SendUser(User,SERVER_HEADER" User %s is not logged in !\n",Beep->Id);
      return;
   }

   if (Beep->Bell) {
      SendUser(Beep,SERVER_HEADER" %s beeps you\n\a",User->Id);
   } else {
      SendUser(Beep,SERVER_HEADER" %s beeps you\n",User->Id);
   }
   SendUser(User,SERVER_HEADER" You beep %s\n",Beep->Id);
   strcpy(Beep->Tell,User->Id);
}

/* Birthday() */
/* vanhu ok 0 */

void Birthday(user *User, command *Command, arglist *ArgList) {

   user *BirthdayUser;
   char  Time[32];
   int   date;

   if (ArgList)
   {
     BirthdayUser = (user *) ArgList->arg;
     strftime(Time,(size_t)32,"%x",localtime(&BirthdayUser->Birthday));
     date = GetBirthday(BirthdayUser);

     switch(date) {
     case -1:
       SendUser(User,SERVER_HEADER" %s's birthday date is unknown\n",BirthdayUser->Id);
       break;
     case 0:
       SendUser(User,SERVER_HEADER" %s's birthday date is TODAY!!!\n",BirthdayUser->Id);
       break;
     case 1:
       SendUser(User,SERVER_HEADER" %s's birthday date is tomorrow!!!\n",BirthdayUser->Id);
       break;
     default:
       SendUser(User,SERVER_HEADER" %s's birthday is %s (in %d days)\n",BirthdayUser->Id, Time, date);
       break;
     }
   }
   else
   {
     node   *Node;
     user   *UId;
     char   Birthday[31];
     list   *SortList;

     /* initialize dbbn users */
     for (Node = UserList->Head; Node != NULL; Node = Node->Next) {
       UId = (user *) Node->Object;
       UId->dbbn = GetBirthday(UId);
     }
     /* sort depending of dbbn */
     SortList = SortList2(UserList, (CMP_FCT)BirthCmp);

     SendUser(User,"   Login             Full Name            Birthday Day Number before Birthday\n");
     SendUser(User,"------------ ---------------------------- -------- --------------------------------\n");
     for (Node = SortList->Head; Node != NULL; Node = Node->Next) {
       UId = (user *) Node->Object;
       if (UId->Registered && UId->Birthday!=(time_t)0) {
         strftime(Birthday,(size_t)32,"%x",localtime(&UId->Birthday));
         SendUser(User,"%-12.12s %-28.28s %-8.8s %d\n",UId->Id,UId->Name,Birthday, UId->dbbn);
       }
     }
     ClearList(SortList);
     SendUser(User,SERVER_HEADER" End of birthday\n");
   }
}

/* Bump() */
/* vanhu ok 0 */

void Bump(user *User, command *Command, arglist *ArgList) {

   char    *Reason, OldChannelName[ID_SIZE+1];
   user    *Bump;
   channel *Hall, *OldChannel;
   int      Hidden;

   Bump = (user *) ArgList->arg;
   if(ArgList->next) Reason=(char *) ArgList->next->arg;
   else Reason=NULL;
   if (! CanSee(User,Bump))
   {
      SendUser(User,SERVER_HEADER" User %s is not logged in !\n",Bump->Id);
      return;
   }
   if (! IsUpperUser(User,Bump)) {
      DENIED(User,Command);
      return;
   }
   if (SameString(Bump->Channel,DEFAULT_CHANNEL)) {
      SendUser(User,SERVER_HEADER" You cannot bump out of channel %s, use \"kick\" instead\n",Bump->Channel);
      return;
   }

   Hall = SearchChannel(DEFAULT_CHANNEL);
   if (Hall == NULL) {
      SendUser(User,SERVER_HEADER" Couldn't find channel \"%s\" !\n",DEFAULT_CHANNEL);
      Error("Couldn't find channel \"%s\"",DEFAULT_CHANNEL);
      return;
   }
   strcpy(OldChannelName,Bump->Channel);
   OldChannel = SearchChannel(OldChannelName);
   Hidden = OldChannel != NULL && OldChannel->Hidden;

   if (Reason != NULL) {
      SendUser(User,SERVER_HEADER" You bump %s out (%s) !\n",Bump->Id,Reason);
      SendUser(Bump,SERVER_HEADER" You are bumped out by %s (%s) !\n",User->Id,Reason);
      SendJoin(Bump,SERVER_HEADER" %s is bumped out by %s (%s) !\n",Bump->Id,User->Id,Reason);
   } else {
      SendUser(User,SERVER_HEADER" You bump %s out !\n",Bump->Id);
      SendUser(Bump,SERVER_HEADER" You are bumped out by %s !\n",User->Id);
      SendJoin(Bump,SERVER_HEADER" %s is bumped out by %s !\n",Bump->Id,User->Id);
   }

   strcpy(Bump->Invite,"");
   JoinChannel(Bump,Hall);

   if (Hidden) {
      SendJoin(Bump,SERVER_HEADER" %s is bumped from the shadows\n",Bump->Id);
   } else {
      SendJoin(Bump,SERVER_HEADER" %s is bumped from channel %s\n",Bump->Id,OldChannelName);
   }
}

/* Channels() */
/* vanhu ok 1 (pas d'arguments !!!) */

void Channels(user *User, command *Command, arglist *ArgList) {

   int      ChannelNb = 0;
   node    *Node;
   channel *Channel;

   SortList(ChannelList,(CMP_FCT)NodeCmp);

   SendUser(User,"Channel   Group   Users Flags                     Topic\n");
   SendUser(User,"-------- -------- ----- ----- ------------------------------------------------\n");
   for (Node = ChannelList->Head; Node != NULL; Node = Node->Next) {
      Channel = (channel *) Node->Object;
      if (Channel->Type == CHANNEL && (! Channel->Hidden || UserLevel(User) >= ChannelLevel(Channel))) {
         SendUser(User,"%-8.8s %-8.8s %5d %c%c%c%c%c %-48.48s\n",Channel->Id,Channel->Group,Channel->UserNb,(Channel->Closed)?'C':'-',(Channel->Hidden)?'H':'-',(Channel->Invite)?'I':'-',(Channel->Protected)?'P':'-',(Channel->Resident)?'R':'-',Channel->Topic);
         ChannelNb++;
      }
   }
   SendUser(User,SERVER_HEADER" There %s currently %d channel%s\n",(ChannelNb>1)?"are":"is",ChannelNb,(ChannelNb>1)?"s":"");
}

/* Clear() */
/* vanhu ok 1 (pas d'arguments !!!) */

void Clear(user *User, command *Command, arglist *ArgList) {

   static const char Clear[] = { 27, '[', '2', 'J', 27, '[', '0', ';', '0', 'H', '\0' };

   SendUser(User,"%s",Clear);
}

/* ClearMsg() */

void ClearMsg(user *User, command *Command, arglist *ArgList) {

   char FileName[STRING_SIZE];
   int FirstLine, LastLine;

   sprintf(FileName,"messages/%s",User->Id);
   if (! FileExists(FileName)) {
      SendUser(User,SERVER_HEADER" You have no message !\n");
      return;
   }

   if (ArgList != NULL) { /* A line number was given => first line */

      FirstLine = atoi((char *)ArgList->arg);
      if (FirstLine <= 0) {
         SendUser(User,SERVER_HEADER" Line number must be at least 1\n");
         return;
      }

      if (ArgList->next != NULL) { /* 2nd parameter is the last line */
         LastLine = atoi((char *)ArgList->next->arg);
         if (LastLine < FirstLine) {
            SendUser(User,SERVER_HEADER" Last line number must be at least %d\n",FirstLine);
            return;
         }
      } else {
         LastLine = FirstLine;
      }

      DeleteLines(FileName,FirstLine,LastLine);
      if (FirstLine != LastLine) {
         SendUser(User,SERVER_HEADER" You clear messages %d to %d !\n",FirstLine,LastLine);
      } else {
         SendUser(User,SERVER_HEADER" You clear message %d !\n",FirstLine);
      }

   } else { /* No line number => clear all messages */

      DeleteFile(FileName);
      SendUser(User,SERVER_HEADER" You clear your message(s) !\n");
   }
}

/* Commands() */

void Commands(user *User, command *Command, arglist *ArgList) {

   char lastcname[50];

   *lastcname = '\0';

   SendUser(User,"  Command     Group                     Usage\n");
   SendUser(User,"------------ -------- -----------------------------------------\n");
   for (Command = CommandList; Command->Function != NULL; Command++)
   {
      if (UserLevel(User) >= GroupLevel(SearchGroup(Command->Group)))
      {
         if(strcmp(lastcname, Command->Name))
         {
            SendUser(User,"%-12.12s %-8.8s %s %s\n",Command->Name,Command->Group,Command->Name,Command->Usage);
            strcpy(lastcname, Command->Name);
         }
      }
   }
   SendUser(User,SERVER_HEADER" End of commands\n");
}

/* Date() */
/* vanhu ok 1 (pas d'arguments !!!) */

void Date(user *User, command *Command, arglist *ArgList) {

   char TimeString[128];
   time_t   Time;

   Time = time(NULL);
   strftime(TimeString,(size_t)128,"%A %B %d %Y %X ",localtime(&Time));

   SendUser(User,SERVER_HEADER" The current date and time are %s\n",TimeString);
}

/* Emote() */
/* vanhu ok 0 */

void Emote(user *User, command *Command, arglist *ArgList) {

   if (ArgList == NULL) {
      USAGE(User,Command);
      return;
   }

   SendChannel(User,SERVER_HEADER" *%s %s*\n",User->Id, (char *) ArgList->arg);
   SendUser(User,SERVER_HEADER" *%s %s*\n",User->Id, (char *) ArgList->arg);
}

/* Force() */
/* vanhu ok 0 */

void ForceCmd(user *User, command *Command, arglist *ArgList) {

   char *CommandName;
   user *Force;

   Force = (user *) ArgList->arg;
/*   if (ArgList->next == NULL)
   {
      USAGE(User,Command);
      return;
   }*/
   CommandName=(char *) ArgList->next->arg;
   if (! CanSee(User,Force))
   {
      SendUser(User,SERVER_HEADER" User %s is not logged in !\n",Force->Id);
      return;
   }
   if (! IsUpperUser(User,Force)) {
      DENIED(User,Command);
      return;
   }

   SendUser(User,SERVER_HEADER" You force %s to %s\n",Force->Id,CommandName);
   Execute(Force,CommandName,!IsSuperUser(User)); /* Execute in force mode except for Root */
}

/* Finger() */
/* vanhu ok 0 */

void Finger(user *User, command *Command, arglist *ArgList) {
   int I;
   user *UId;
   char  Time[32];

   if (ArgList == NULL) UId = User;
   else UId = (user *) ArgList->arg;

   SendUser(User,"Login      : %s\n",UId->Id);
   SendUser(User,"Group      : %s\n",UId->Group);
   if (UId->Name[0]  != '\0') SendUser(User,"Name       : %s\n",UId->Name);
   if (UId->EMail[0] != '\0') SendUser(User,"EMail      : %s\n",UId->EMail);
   if (UId->Formation[0] != '\0') SendUser(User,"Formation  : %s\n",UId->Formation);
   if (UId->Registered) {
      strftime(Time,(size_t)47,"%x %X",localtime(&UId->RegisterTime));
      SendUser(User,"Registered : %s\n",Time);
   }
   if(UId->Birthday == (time_t) 0) {
     strcpy(Time, "<Unknown>");
   } else {
     strftime(Time,(size_t)47,"%x ",localtime(&UId->Birthday));
   }
   SendUser(User,"Birthday   : %s\n",Time);
   strftime(Time,(size_t)47,"%x %X",localtime(&UId->ConnectTime));
   if (UId->KickNb != 0) {
      SendUser(User,"KickNb     : %d\n",UId->KickNb);
   }
   if (UId->KickedNb != 0) {
      SendUser(User,"KickedNb   : %d\n",UId->KickedNb);
   }
   if (CanSee(User,UId)) {
      SendUser(User,"On since   : %s\n",Time);
      if (UserLevel(User) < GroupLevel(SearchGroup("GM")))
      {
         SendUser(User,"From host  : <Unknown>\n");
      }
      else
      {
         SendUser(User,"From host  : %s\n",UId->Host);
      }
      SendUser(User,"Client     : %s\n",UId->Client);
      SendUser(User,"On channel : %s\n",UId->Channel);
      if (UId->Away) {
         SendUser(User,"Idle time  : *Away*\n");
      } else {
         SendUser(User,"Idle time  : %s\n",TimeString((int)(time(NULL)-UId->LastSendTime)));
      }
      strcpy(Time,TimeString((int)(UId->TotalTime+time(NULL)-UId->ConnectTime)));
   } else {
      SendUser(User,"Last on    : %s\n",Time);
      strcpy(Time,TimeString(UId->TotalTime));
   }
   SendUser(User,"Login nb   : %d\n",UId->LoginNb);
   SendUser(User,"Total Time : %s\n",Time);
   if (UId != User && UId->Language != 0) {
      SendUser(User,"Language   : %s\n",LanguageName(UId->Language));
   }
   SendUser(User,"Plan       : \n");
   for(I = 0; I < PLAN_SIZE; I++) {
     if (! IsEmptyString(UId->Plan[I])) {
        SendUser(User," %d : %s\n",I,UId->Plan[I]);
     }
   }
   SendUser(User,SERVER_HEADER" End of finger\n");
}

void Get(user *User, command *Command, arglist *ArgList) {
  char    *ObjectName, *Variable, *Args;
  object  *Object;
  user    *UId;
  group   *Group;
  channel *Channel;

  Args=(char *)ArgList->arg;
  ObjectName = Args;

  if (StartWith(ObjectName,"Login")
      || StartWith(ObjectName,"Id")
      || StartWith(ObjectName,"Password")
      || StartWith(ObjectName,"Name")
      || StartWith(ObjectName,"EMail")
      || StartWith(ObjectName,"Formation")
      || StartWith(ObjectName,"Away")
      || StartWith(ObjectName,"Bell")
      || StartWith(ObjectName,"Plan")
      || StartWith(ObjectName,"Birthday")
      || StartWith(ObjectName,"Client")
      || StartWith(ObjectName,"InOut")
      || StartWith(ObjectName,"Join")
      || StartWith(ObjectName,"Shout")) {

    Variable   = ObjectName;
    ObjectName = User->Id;

  } else if (StartWith(ObjectName,"Leader")
             || StartWith(ObjectName,"Level")) {

    Variable   = ObjectName;
    ObjectName = User->Group;

  } else if (StartWith(ObjectName,"Group")
             || StartWith(ObjectName,"Topic")
             || StartWith(ObjectName,"Closed")
             || StartWith(ObjectName,"Invite")
             || StartWith(ObjectName,"Protected")
             || StartWith(ObjectName,"Hidden")
             || StartWith(ObjectName,"Resident")) {

    Variable   = ObjectName;
    ObjectName = User->Channel;

  } else if (StartWith(ObjectName,"RegOnly")) {
    Variable = ObjectName;
    SendUser(User,SERVER_HEADER" regonly is %s\n",(RegOnly)?"on":"off");
    return;

  }
  else {

    Variable = strpbrk(ObjectName,"\t .");
    if (Variable == NULL) {
      USAGE(User,Command);
      return;
    }
    *Variable++ = '\0';
  }

/*   if (! IsId(ObjectName)) {
     SendUser(User,SERVER_HEADER" Invalid indentifier : \"%s\"\n",ObjectName);
     return;
     }
*/
  Object = SearchObject(Lists,ObjectName,OBJECT);
  if (Object == NULL) {
    SendUser(User,SERVER_HEADER" Unknown object \"%s\"\n",ObjectName);
    return;
  }

  switch (Object->Type) {

  case USER :

    UId = (user *) Object;

    if (SameString(Variable,"Login") ||
        SameString(Variable,"Id")) {
      SendUser(User,SERVER_HEADER" %s's login is %s ...\n",UId->Id,UId->Id);
    } else if (SameString(Variable,"Group")) {
      if (! UId->Registered) {
        SendUser(User,SERVER_HEADER" Unregistered user %s\n",ObjectName);
        return;
      }
      SendUser(User,SERVER_HEADER" %s'group is %s\n",UId->Id,UId->Group);

    } else if (SameString(Variable,"Password")) {

      DENIED(User,Command);

    } else if (SameString(Variable,"Name")) {
      SendUser(User,SERVER_HEADER" %s's full name is \"%s\"\n",UId->Id,UId->Name);
    } else if (SameString(Variable,"EMail")) {
      SendUser(User,SERVER_HEADER" %s's email address is \"%s\"\n",UId->Id,UId->EMail);
    } else if (SameString(Variable,"Formation")) {
      SendUser(User,SERVER_HEADER" %s's formation is \"%s\"\n",UId->Id,UId->Formation);
    } else if (SameString(Variable,"Crypt")) {
      if (IsConnected(UId))
        SendUser(User,SERVER_HEADER" %s is currently in mode %s\n",UId->Id,LanguageName(UId->Language));
      else
        SendUser(User,SERVER_HEADER" %s is not logged in !\n",UId->Id);
    } else if (SameString(Variable,"Away")) {
      if (IsConnected(UId))
        SendUser(User,SERVER_HEADER" %s is %s\n",UId->Id,(UId->Away)?"away":"there");
      else
        SendUser(User,SERVER_HEADER" %s is not logged in !\n",UId->Id);
    } else if (SameString(Variable,"Bell")) {
      if (IsConnected(UId))
        SendUser(User,SERVER_HEADER" %s's bell is %s\n",UId->Id,(UId->Bell)?"on":"off");
      else
        SendUser(User,SERVER_HEADER" %s is not logged in !\n",UId->Id);
    } else if (SameString(Variable,"Birthday")) {
      char DateString[128];
      strftime(DateString,(size_t)127,"%A %B %d %Y",localtime(&UId->Birthday));
      SendUser(User,SERVER_HEADER" %s's birthday is the %s\n",UId->Id,DateString);
    } else if (StartWith(Variable,"Plan")) {
      int LineNumber = Variable[4]-'0';
      if (LineNumber < 0 || LineNumber >= PLAN_SIZE) {
        SendUser(User,SERVER_HEADER" \"Plan\" variable must be between 0 and %d\n",PLAN_SIZE-1);
        return;
      }
      SendUser(User," %d : %s\n",LineNumber,UId->Plan[LineNumber]);
    } else if (SameString(Variable,"Client")) {
      if (IsConnected(UId))
        SendUser(User,SERVER_HEADER" %s's client is \"%s\"\n",UId->Id,UId->Client);
      else
        SendUser(User,SERVER_HEADER" %s is not logged in !\n",UId->Id);
    } else if (SameString(Variable,"InOut")) {
      if (IsConnected(UId))
        SendUser(User,SERVER_HEADER" %s's inout is %s\n",UId->Id,(UId->InOut)?"on":"off");
      else
        SendUser(User,SERVER_HEADER" %s is not logged in !\n",UId->Id);
    } else if (SameString(Variable,"Join")) {
      if (IsConnected(UId))
        SendUser(User,SERVER_HEADER" %s's join is %s\n",UId->Id,(UId->Join)?"on":"off");
      else
        SendUser(User,SERVER_HEADER" %s is not logged in !\n",UId->Id);
    } else if (SameString(Variable,"Shout")) {
      if (IsConnected(UId))
        SendUser(User,SERVER_HEADER" %s's shout is %s\n",UId->Id,(UId->Shout)?"on":"off");
      else
        SendUser(User,SERVER_HEADER" %s is not logged in !\n",UId->Id);
    } else {
      SendUser(User,SERVER_HEADER" Unknown variable : \"%s\"\n",Variable);
    }
    break;

  case GROUP :

    Group = (group *) Object;

    if (SameString(Variable,"Id")) {
      SendUser(User,SERVER_HEADER" group %s'id is %s ...\n",Group->Id,Group->Id);
    } else if (SameString(Variable,"Leader")) {
      SendUser(User,SERVER_HEADER" Leader of group %s is %s\n",Group->Id,Group->Leader);
    } else if (SameString(Variable,"Level")) {
      SendUser(User,SERVER_HEADER" Level of group %s is %d\n",Group->Id,Group->Level);
    } else if (SameString(Variable,"Name")) {
      SendUser(User,SERVER_HEADER" Name of group %s is \"%s\"\n",Group->Id,Group->Name);
    } else {
      SendUser(User,SERVER_HEADER" Unknown variable : \"%s\"\n",Variable);
    }
    break;

  case CHANNEL :

    Channel = (channel *) Object;

    if (SameString(Variable,"Id")) {
      SendUser(User,SERVER_HEADER" Name of channel %s is %s ...\n",Channel->Id,Channel->Id);
    } else if (SameString(Variable,"Group")) {
      SendUser(User,SERVER_HEADER" Group of channel %s is %s\n",Channel->Id,Channel->Group);
    } else if (SameString(Variable,"Topic")) {
      SendUser(User,SERVER_HEADER" Topic of channel %s is %s\n",Channel->Id,Channel->Topic);
    } else if (SameString(Variable,"Closed")) {
      SendUser(User,SERVER_HEADER" Channel %s is %s\n",Channel->Id,(Channel->Closed)?"closed":"opened");
    } else if (SameString(Variable,"Hidden")) {
      SendUser(User,SERVER_HEADER" Channel %s is %s\n",Channel->Id,(Channel->Hidden)?"hidden":"shown");
    } else if (SameString(Variable,"Invite")) {
      SendUser(User,SERVER_HEADER" Channel %s is %s\n",Channel->Id,(Channel->Invite)?"invite":"non invite");
    } else if (SameString(Variable,"Protected")) {
      SendUser(User,SERVER_HEADER" Channel %s is %s\n",Channel->Id,(Channel->Protected)?"protected":"non protected");
    } else if (SameString(Variable,"Resident")) {
      SendUser(User,SERVER_HEADER" Channel %s is %s\n",Channel->Id,(Channel->Resident)?"resident":"non resident");
    } else {
      SendUser(User,SERVER_HEADER" Unknown variable : \"%s\"\n",Variable);
    }
    break;
  }
}

/* Groups() */
/* vanhu ok 1 (pas d'arguments !!!) */

void Groups(user *User, command *Command, arglist *ArgList) {

   int    GroupNb = 0;
   node  *Node;
   group *Group;

   SortList(GroupList,(CMP_FCT)NodeCmp);

   /* don't display symbol because CeB doesn't support it */
   if (TRUE) {
      SendUser(User," Group      Leader    Lv            Full Name\n");
      SendUser(User,"-------- ------------ -- --------------------------------\n");
      for (Node = GroupList->Head; Node != NULL; Node = Node->Next) {
         Group = (group *) Node->Object;
         if (Group->Type == GROUP) {
            SendUser(User,"%-8.8s %-12.12s %2d %-32.32s\n",Group->Id,Group->Leader,Group->Level,Group->Name);
            GroupNb++;
        }
     }
   } else {
      SendUser(User," Group      Leader    Lv            Full Name             S\n");
      SendUser(User,"-------- ------------ -- -------------------------------- -\n");
      for (Node = GroupList->Head; Node != NULL; Node = Node->Next) {
         Group = (group *) Node->Object;
         if (Group->Type == GROUP) {
            SendUser(User,"%-8.8s %-12.12s %2d %-32.32s %-1.1s\n",Group->Id,Group->Leader,Group->Level,Group->Name,Group->Symbol);
            GroupNb++;
         }
      }
   }

   SendUser(User,SERVER_HEADER" There are %d groups\n",GroupNb);
}

/* Help() */

void Help(user *User, command *Command, arglist *ArgList) {

   char *Args;

   if (ArgList) Args=(char *) ArgList->arg;
   else Args = "help";

   if (! IsFile(Args)) {
      SendUser(User,SERVER_HEADER" Invalid help topic \"%s\"\n",Args);
      return;
   }

   SendUser(User,SERVER_HEADER" Help for \"%s\" :\n\n",Args);

   for (Command = CommandList; Command->Function != NULL; Command++) {
      if (SameString(Command->Name,Args)) {
	 SendUser(User,"%s %s (Group : %s)\n\n",Command->Name,Command->Usage,Command->Group);
	 break;
      }
   }

   if (SendHelpFile(User,Args)) {
      SendUser(User,SERVER_HEADER" End of help\n");
   } else {
      SendUser(User,SERVER_HEADER" No help available for \"%s\"\n",Args);
   }
}

/* History() */
/* pas de pb !!!! */

void History(user *User, command *Command, arglist *ArgList) {

   SendHistory(User,"History",LogHistory);
}

/* Invite() */
/* syntaxe a reetudier ??? invite -> set ..... */
/*  ok 0 */

void Invite(user *User, command *Command, arglist *ArgList) {

   user *Invite;

   Invite = ArgList->arg;
   if (! CanSee(User,Invite)) {
      SendUser(User,SERVER_HEADER" User %s is not logged in !\n",Invite->Id);
      return;
   }

   strcpy(Invite->Invite,User->Channel);

   SendUser(User,SERVER_HEADER" You invite %s in channel %s\n",Invite->Id,Invite->Invite);
   SendUser(Invite,SERVER_HEADER" You are invited by %s in channel %s, type \"join\" to accept\n",User->Id,Invite->Invite);
}

/* Join() */
/* ok 0 */

void Join(user *User, command *Command, arglist *ArgList) {

   channel *Channel, *OldChannel;
   object  *Object;
   char     OldChannelName[ID_SIZE+1];
   int      Hidden;
   char *Args=NULL;

   if (ArgList == NULL) {
      if (User->Invite[0] == '\0') {
         SendUser(User,SERVER_HEADER" You are not invited to any channel\n");
         return;
      }
      Channel = SearchChannel(User->Invite);
      if (Channel == NULL) {
         SendUser(User,SERVER_HEADER" Unknown channel \"%s\"\n",User->Invite);
         return;
      }
   }
   else if(ArgList->argtype==ARG_CHANNEL) Channel=(channel *)ArgList->arg;
   else /* WORD => creation du channel */
   {
      Args=(char *) ArgList->arg;
      if (! IsId(Args)) {
         SendUser(User,SERVER_HEADER" Invalid channel name : \"%s\"\n",Args);
         return;
      }
      Object = SearchObject(Lists,Args,OBJECT);
      if (Object != NULL) {
	 SendUser(User,SERVER_HEADER" Object \"%s\" already exists\n",Object->Id);
	 return;
      }
      Channel = NewChannel(Args);
      if (Channel == NULL) {
         SendUser(User,SERVER_HEADER" Couldn't create channel \"%s\"\n",Args);
         return;
      }
      strcpy(Channel->Owner,User->Id);
      strcpy(Channel->Group,User->Group);
      AddTail(ChannelList,Channel);
   }
   if (SameString(Channel->Id,User->Channel)) {
      SendUser(User,SERVER_HEADER" You are already in channel %s !\n",User->Channel);
      return;
   }
   if (UserLevel(User) < ChannelLevel(Channel))
   {
      if (Channel->Closed) {
         SendUser(User,SERVER_HEADER" Channel %s is closed\n",Channel->Id);
         return;
      }
      if (Channel->Invite && ! SameString(User->Invite,Channel->Id)) {
         SendUser(User,SERVER_HEADER" You need an invitation to join channel %s\n",Channel->Id);
         return;
      }
   }

   strcpy(User->Invite,"");
   strcpy(OldChannelName,User->Channel);

   if (Channel->Hidden) {
      SendJoin(User,SERVER_HEADER" %s fades into the shadows\n",User->Id);
   } else {
      SendJoin(User,SERVER_HEADER" %s joined channel %s\n",User->Id,Channel->Id);
   }
   OldChannel = SearchChannel(OldChannelName);
   Hidden = OldChannel != NULL && OldChannel->Hidden;
   SendUser(User,SERVER_HEADER" You join channel %s\n",Channel->Id);
   JoinChannel(User,Channel);
   if (Hidden) {
      SendJoin(User,SERVER_HEADER" %s appears from the shadows\n",User->Id);
   } else {
      SendJoin(User,SERVER_HEADER" %s comes in from channel %s\n",User->Id,OldChannelName);
   }
}

/* Kick() */
/* ok 0 */

void Kick(user *User, command *Command, arglist *ArgList) {

   char *Reason;
   user *Kick;

   if(ArgList->next != NULL) Reason = (char *) ArgList->next->arg;
   else Reason=NULL;
   Kick = (user *) ArgList->arg;
   if (! CanSee(User,Kick)) {
      SendUser(User,SERVER_HEADER" User %s is not logged in !\n",Kick->Id);
      return;
   }
   if (! IsUpperUser(User,Kick)) {
      DENIED(User,Command);
      return;
   }

   User->KickNb++;
   Kick->KickedNb++;
   DataBaseChanged = TRUE;

   if (Reason != NULL) {
      SendUser(User,SERVER_HEADER" You kick %s out (%s) !\n",Kick->Id,Reason);
      SendUser(Kick,SERVER_HEADER" You are kicked out by %s (%s) !\n",User->Id,Reason);
      Logout(Kick,User,SERVER_HEADER" %s is kicked out by %s (%s) !\n",Kick->Id,User->Id,Reason);
   } else {
      SendUser(User,SERVER_HEADER" You kick %s out !\n",Kick->Id);
      SendUser(Kick,SERVER_HEADER" You are kicked out by %s !\n",User->Id);
      Logout(Kick,User,SERVER_HEADER" %s is kicked out by %s !\n",Kick->Id,User->Id);
   }
}

/* Kill() */
/* ok 0 */

void Kill(user *User, command *Command, arglist *ArgList) {

   user *Kill;
   char FileName[STRING_SIZE];

   /*if (! IsSuperUser(User)) {
      DENIED(User,Command);
      return;
      }*/

   if (UserLevel(User) < GroupLevel(SearchGroup("HeadArch"))) {
      DENIED(User,Command);
      return;
   }

   Kill = (user *)ArgList->arg;

   if (!SameString(User->Group, "HeadArch") && SameString(Kill->Group, "HeadArch")) {
      DENIED(User,Command);
      return;
   }

   if (! IsUpperEqUser(User,Kill)) {
      DENIED(User,Command);
      return;
   }

   /* Kick the user */

   if (IsConnected(Kill)) {
      SendUser(User,SERVER_HEADER" You kick %s out !\n",Kill->Id);
      SendUser(Kill,SERVER_HEADER" You are killed by %s !!!\n",User->Id);
      Logout(Kill,User,SERVER_HEADER" %s is kicked out by %s !\n",Kill->Id,User->Id);
   }

   /* Announce the kill */

   SendUser(User,SERVER_HEADER" You kill %s !!!\n",Kill->Id);
   SendUsers(User,SERVER_HEADER" %s is killed by %s !!!\n",Kill->Id,User->Id);

   /* Remove the user from the database */

   Kill->Registered = FALSE;
   DataBaseChanged = TRUE;

   /* Remove the user's messages */

   sprintf(FileName,"messages/%s",Kill->Id);
   if (FileExists(FileName)) DeleteFile(FileName);
}

/* Leave() */
/* ok 1 : pas d'args... */

void Leave(user *User, command *Command, arglist *ArgList) {

   channel *Hall, *OldChannel;
   char     OldChannelName[ID_SIZE+1];
   int      Hidden;

   if (SameString(User->Channel,DEFAULT_CHANNEL)) {
      SendUser(User,SERVER_HEADER" You cannot leave channel %s, use \"quit\" instead\n",User->Channel);
      return;
   }
   Hall = SearchChannel(DEFAULT_CHANNEL);
   if (Hall == NULL) {
      SendUser(User,SERVER_HEADER" Couldn't find channel \"%s\" !\n",DEFAULT_CHANNEL);
      Error("Couldn't find channel \"%s\"",DEFAULT_CHANNEL);
      return;
   }

   strcpy(OldChannelName,User->Channel);

   SendJoin(User,SERVER_HEADER" %s left channel %s\n",User->Id,OldChannelName);
   OldChannel = SearchChannel(OldChannelName);
   Hidden = OldChannel != NULL && OldChannel->Hidden;
   SendUser(User,SERVER_HEADER" You leave channel %s\n",OldChannelName);
   JoinChannel(User,Hall);
   if (Hidden) {
      SendJoin(User,SERVER_HEADER" %s appears from the shadows\n",User->Id);
   } else {
      SendJoin(User,SERVER_HEADER" %s comes in from channel %s\n",User->Id,OldChannelName);
   }
}

/* MkGroup() */
/* ok 0 */

void MkGroup(user *User, command *Command, arglist *ArgList) {

   char   *GroupName;
   object *Object;
   group  *Group;

   if (! IsSuperUser(User)) {
      DENIED(User,Command);
      return;
   }

   GroupName = (char *) ArgList->arg;
   if (! IsId(GroupName)) {
      SendUser(User,SERVER_HEADER" Invalid group name : \"%s\"\n",GroupName);
      return;
   }
   Object = SearchObject(Lists,GroupName,OBJECT);
   if (Object != NULL) {
      SendUser(User,SERVER_HEADER" Object \"%s\" already exists\n",Object->Id);
      return;
   }

   Group = NewGroup(GroupName,User->Id,UserLevel(User),"New group"," ");
   if (Group == NULL) {
      SendUser(User,SERVER_HEADER" Couldn't create group \"%s\"\n",GroupName);
      return;
   }

   AddTail(GroupList,Group);
   DataBaseChanged = TRUE;

   SendUser(User,SERVER_HEADER" You create the group %s !\n",GroupName);
   SendChannel(User,SERVER_HEADER" %s creates the group %s !\n",User->Id,GroupName);
}

/* Open() */
/* ok 0 */

void Open(user *User, command *Command, arglist *ArgList) {

   int PortNum;

   if (! IsSuperUser(User)) {
      DENIED(User,Command);
      return;
   }
/*
   if (ServerSocket != NO_SOCKET) {
      SendUser(User,SERVER_HEADER" Server socket already opened !\n");
      return;
   }
*/

   PortNum = (ArgList != NULL) ? atoi((char *) ArgList->arg) : PORT_NUM;

   if (! InstallSocket(PortNum)) {
      SendUser(User,SERVER_HEADER" Couldn't open server socket !\n");
   } else {
      SendUser(User,SERVER_HEADER" Server socket is now open on port %d\n",PortNum);
      SendUsers(User,SERVER_HEADER" %s opened connection socket on port %d\n",User->Id,PortNum);
   }
}

/* Quit() */
/* ok 0 */

void Quit(user *User, command *Command, arglist *ArgList) {

   SendUser(User,SERVER_HEADER" You leave "SERVER_HEADER" Chat !\n");

   if (ArgList != NULL) {
      Logout(User,NULL,SERVER_HEADER" %s leaves (%s) !\n",User->Id,(char *)ArgList->arg);
   } else {
      Logout(User,NULL,SERVER_HEADER" %s leaves !\n",User->Id);
   }
}

/* Register() */
/* ok 0 */

void Register(user *User, command *Command, arglist *ArgList) {
   char *Args;

   if (User->Registered) {
      SendUser(User,SERVER_HEADER" You are already registered !\n");
      return;
   }
   Args=(char *)ArgList->arg;
   if (! IsPassword(Args)) {
      SendUser(User,SERVER_HEADER" Invalid password : \"%s\"\n",Args);
      return;
   }
   if (! isalpha(User->Name[0])) {
      SendUser(User,SERVER_HEADER" Fill in your name with \"set name <Name>\" first !\n");
      SendUser(User,SERVER_HEADER" Type \"users\" to see the name of other users\n");
      return;
   }
   if (! isalnum(User->EMail[0])) {
      SendUser(User,SERVER_HEADER" Fill in your email address with \"set email <EMail>\" first !\n");
      SendUser(User,SERVER_HEADER" Type \"users\" to see the email address of other users\n");
      return;
   }

   strcpy(User->Group,REGISTER_GROUP);
   strcpy(User->Password,CryptPassword(Args));
   User->Registered   = TRUE;
   User->RegisterTime = time(NULL);
   DataBaseChanged = TRUE;

   SendUser(User,SERVER_HEADER" You are now registered\n");
   SendChannel(User,SERVER_HEADER" %s is now registered\n",User->Id);
}

/* Reply() */
/* ok 0 */

void Reply(user *User, command *Command, arglist *ArgList) {

   user *Reply;
   char *Args;

   if (User->Tell[0] == '\0') {
      SendUser(User,SERVER_HEADER" Nobody to reply\n");
      return;
   }
   Reply = SearchUser(User->Tell);
   if  (Reply == NULL || ! CanSee(User,Reply)) {
      SendUser(User,SERVER_HEADER" User %s is not logged in !\n",User->Tell);
      return;
   }
   Args=(char *)ArgList->arg;

   if (Args[strlen(Args)-1] == '?') {
      SendUser(Reply,SERVER_HEADER" %s asks you: %s\n",User->Id,Args);
      SendUser(User,SERVER_HEADER" You ask %s: %s\n",Reply->Id,Args);
   } else {
      SendUser(Reply,SERVER_HEADER" %s replies: %s\n",User->Id,Args);
      SendUser(User,SERVER_HEADER" You reply to %s: %s\n",Reply->Id,Args);
   }
   strcpy(Reply->Tell,User->Id);

   if (Reply->Away) SendUser(User,SERVER_HEADER" %s is away and may not be hearing you\n",Reply->Id);
}

/* SendData() */

void SendData(user *User, command *Command, arglist *ArgList) {

   char *Message;
   user *Tell;

   Tell = (user *) ArgList->arg;
   if (! CanSee(User,Tell)) return;

   Message = (char *) ArgList->next->arg;
   SendUser(Tell,"|%s| %s\n",User->Id,Message);
}

/* SendMsg() */

void SendMsg(user *User, command *Command, arglist *ArgList) {

   user *Tell;
   char FileName[STRING_SIZE], Message[STRING_SIZE];

   Tell = (user *) ArgList->arg;
   if (! Tell->Registered) {
      SendUser(User,SERVER_HEADER" User %s is not registered !\n",Tell->Id);
      return;
   }

   sprintf(FileName,"messages/%s",Tell->Id);
   sprintf(Message,"%s : %s",User->Id,(char *)ArgList->next->arg);

   AppendFile(FileName,"%s", Message);
   if (CanSee(User,Tell)) {
      SendUser(Tell,SERVER_HEADER" %s sends a message to you: %s\n",User->Id,(char *)ArgList->next->arg);
   }
   SendUser(User,SERVER_HEADER" You send a message to %s: %s\n",Tell->Id,(char *)ArgList->next->arg);
}

/* Set() */
/* ok 0 mais a modifier pour d'autres raisons */

void Set(user *User, command *Command, arglist *ArgList) {
   int I;
   char    *ObjectName, *Variable, *Value;
   object  *Object;
   user    *UId, *OldUId, *NewUId;
   group   *Group;
   channel *Channel;
   char *Args;

   if(ArgList == NULL)
   { /* afficher les variables */
      SendUser(User, "Your variables: \n");

      return;
   }
   Args=(char *)ArgList->arg;
   ObjectName = Args;

   if (StartWith(ObjectName,"Login")
    || StartWith(ObjectName,"Id")
    || StartWith(ObjectName,"Password")
    || StartWith(ObjectName,"Name")
    || StartWith(ObjectName,"EMail")
    || StartWith(ObjectName,"Formation")
    || StartWith(ObjectName,"Away")
    || StartWith(ObjectName,"Bell")
    || StartWith(ObjectName,"Plan")
    || StartWith(ObjectName,"Birthday")
    || StartWith(ObjectName,"Client")
    || StartWith(ObjectName,"InOut")
    || StartWith(ObjectName,"Join")
    || StartWith(ObjectName,"Shout")) {

      Variable   = ObjectName;
      ObjectName = User->Id;

   } else if (StartWith(ObjectName,"Leader")
           || StartWith(ObjectName,"Level")) {

      Variable   = ObjectName;
      ObjectName = User->Group;

   } else if (StartWith(ObjectName,"Group")
           || StartWith(ObjectName,"Topic")
           || StartWith(ObjectName,"Closed")
           || StartWith(ObjectName,"Invite")
           || StartWith(ObjectName,"Protected")
           || StartWith(ObjectName,"Hidden")
           || StartWith(ObjectName,"Resident")) {

      Variable   = ObjectName;
      ObjectName = User->Channel;

   } else if (StartWith(ObjectName,"RegOnly")) {

      if (! IsSuperUser(User)) {
         DENIED(User,Command);
         return;
      }

      Variable = ObjectName;
      Value    = strpbrk(Variable,"\t ");
      if (Value == NULL) Value = ""; else *Value++ = '\0';

      RegOnly = Boolean(Value);
      SendUser(User,SERVER_HEADER" regonly is now %s\n",(RegOnly)?"on":"off");

      return;

   } else {

      Variable = strpbrk(ObjectName,"\t .");
      if (Variable == NULL) {
         USAGE(User,Command);
         return;
      }
      *Variable++ = '\0';
   }

   Value = strpbrk(Variable,"\t ");
   if (Value == NULL) Value = ""; else *Value++ = '\0';
   if (! IsId(ObjectName)) {
      SendUser(User,SERVER_HEADER" Invalid indentifier : \"%s\"\n",ObjectName);
      return;
   }
   Object = SearchObject(Lists,ObjectName,OBJECT);
   if (Object == NULL) {
      SendUser(User,SERVER_HEADER" Unknown object \"%s\"\n",ObjectName);
      return;
   }

   switch (Object->Type) {

   case USER :

      UId = (user *) Object;
      if (User == UId) {
      } else {
         if (SameString(Variable,"Group")) {
            if (! IsUpperEqUser(User,UId)) {
               DENIED(User,Command);
               return;
            }
         } else {
            if (! IsUpperUser(User,UId)) {
               DENIED(User,Command);
               return;
            }
         }
      }

      if (SameString(Variable,"Login") || SameString(Variable,"Id")) {
         /* if (User != UId && ! IsSuperUser(User)) { */
         if (! IsSuperUser(User)) {
            DENIED(User,Command);
            return;
         }
         if (! IsId(Value)) {
            SendUser(User,SERVER_HEADER" Invalid login name : \"%s\"\n",Value);
         } else if (! SameString(Value,UId->Id) && (NewUId = SearchObject(Lists,Value,OBJECT)) != NULL) {
            SendUser(User,SERVER_HEADER" \"%s\" is a reserved word\n",NewUId->Id);
         } else {
            if (IsConnected(UId)) {
               SendUser(UId,SERVER_HEADER" Your login name is now %s\n",Value);
               SendChannel(UId,SERVER_HEADER" User %s is now known as %s\n",UId->Id,Value);
            } else {
               SendUser(User,SERVER_HEADER" User %s is now known as %s\n",UId->Id,Value);
            }
            SetString(&UId->Id,Value);
            if (UId->Registered) DataBaseChanged = TRUE;
         }
      } else if (SameString(Variable,"Group")) {
         if (! IsId(Value)) {
            SendUser(User,SERVER_HEADER" Invalid group name : \"%s\"\n",Value);
            return;
         }
         if (! UId->Registered) {
            SendUser(User,SERVER_HEADER" Unregistered user %s\n",ObjectName);
            return;
         }
         Group = SearchGroup(Value);
         if (Group == NULL) {
            SendUser(User,SERVER_HEADER" Unknown group \"%s\"\n",Value);
            return;
         }
         /* no more leader
         if (User == UId || UserLevel(User) < GroupLevel(Group) || (UserLevel(User) == GroupLevel(Group) && ! SameString(User->Id,Group->Leader))) {
         */
         if (User == UId || UserLevel(User) < GroupLevel(Group)) {
            DENIED(User,Command);
            return;
         }
         strcpy(UId->Group,Group->Id);
         DataBaseChanged = TRUE;
         if (IsConnected(UId)) {
            SendUser(UId,SERVER_HEADER" %s put you in group %s\n",User->Id,UId->Group);
            SendUsers(UId,SERVER_HEADER" %s put %s in group %s\n",User->Id,UId->Id,UId->Group);
         } else {
            SendUser(User,SERVER_HEADER" You put %s in group %s\n",UId->Id,UId->Group);
         }
      } else if (SameString(Variable,"Password")) {
         if (/*User != UId &&*/ ! IsSuperUser(User)) {
            DENIED(User,Command);
            return;
         }
         if (! IsPassword(Value)) {
            SendUser(User,SERVER_HEADER" Invalid password : \"%s\"\n",Value);
         } else if (! UId->Registered) {
            SendUser(User,SERVER_HEADER" You are not registered yet, use \"Register <Password>\" instead.\n");
         } else {
            strcpy(UId->Password,CryptPassword(Value));
            DataBaseChanged = TRUE;
            if (IsConnected(UId)) {
               SendUser(UId,SERVER_HEADER" You changed your password\n");
            }
            if (User != UId) SendUser(User,SERVER_HEADER" %s's password is changed\n",UId->Id);
         }
      } else if (SameString(Variable,"Name")) {
         if (User != UId && ! IsSuperUser(User)) {
            DENIED(User,Command);
            return;
         }
         if (Value[0] == '\0') Value = "<Unknown>";
         SetString(&UId->Name,Value);
         if (UId->Registered) DataBaseChanged = TRUE;
         if (IsConnected(UId)) {
            SendUser(UId,SERVER_HEADER" Your full name is now \"%s\"\n",UId->Name);
            SendChannel(UId,SERVER_HEADER" %s's full name is now \"%s\"\n",UId->Id,UId->Name);
         } else {
            SendUser(User,SERVER_HEADER" %s's full name is now \"%s\"\n",UId->Id,UId->Name);
         }
      } else if (SameString(Variable,"EMail")) {
         if (User != UId && ! IsSuperUser(User)) {
            DENIED(User,Command);
            return;
         }
         if (Value[0] == '\0') Value = "<Unknown>";
         SetString(&UId->EMail,Value);
         if (UId->Registered) DataBaseChanged = TRUE;
         if (IsConnected(UId)) {
            SendUser(UId,SERVER_HEADER" Your email address is now \"%s\"\n",UId->EMail);
            SendChannel(UId,SERVER_HEADER" %s's email address is now \"%s\"\n",UId->Id,UId->EMail);
         } else {
            SendUser(User,SERVER_HEADER" %s's email address is now \"%s\"\n",UId->Id,UId->EMail);
         }
      } else if (SameString(Variable,"Formation")) {
         if (User != UId && ! IsSuperUser(User)) {
            DENIED(User,Command);
            return;
         }
         if (Value[0] == '\0') Value = "<Unknown>";
         SetString(&UId->Formation,Value);
         if (UId->Registered) DataBaseChanged = TRUE;
         if (IsConnected(UId)) {
            SendUser(UId,SERVER_HEADER" Your formation is now \"%s\"\n",UId->Formation);
            SendChannel(UId,SERVER_HEADER" %s's formation is now \"%s\"\n",UId->Id,UId->Formation);
         } else {
            SendUser(User,SERVER_HEADER" %s's formation is now \"%s\"\n",UId->Id,UId->Formation);
         }
      } else if (SameString(Variable,"Crypt")) {
         //if (! IsUpperUser(User,UId)) {
         if (UserLevel(User) < GroupLevel(SearchGroup("SnrGuide"))) {
            DENIED(User,Command);
            return;
         }
         UId->Language = atoi(Value);
         if (UId->Language < LANGUAGE_MIN) UId->Language = LANGUAGE_MIN;
         if (UId->Language > LANGUAGE_MAX) UId->Language = LANGUAGE_MAX;
         if (IsConnected(UId)) {
	    SendChannel(UId,SERVER_HEADER" %s sets %s to mode %s\n",User->Id,UId->Id,LanguageName(UId->Language));
         } else {
	    SendUser(User,SERVER_HEADER" You set %s to mode %s\n",UId->Id,LanguageName(UId->Language));
         }
      } else if (SameString(Variable,"Away")) {
         UId->Away = Boolean(Value);
         if (IsConnected(UId)) {
            SendUser(UId,SERVER_HEADER" You are %s\n",(UId->Away)?"away":"back");
            SendChannel(UId,SERVER_HEADER" %s is %s\n",UId->Id,(UId->Away)?"away":"back");
         } else {
            SendUser(User,SERVER_HEADER" %s is %s\n",UId->Id,(UId->Away)?"away":"back");
         }
      } else if (SameString(Variable,"Bell")) {
         UId->Bell = Boolean(Value);
         if (IsConnected(UId)) {
            SendUser(UId,SERVER_HEADER" Your bell is now %s\n",(UId->Bell)?"on":"off");
            SendChannel(UId,SERVER_HEADER" %s's bell is now %s\n",UId->Id,(UId->Bell)?"on":"off");
         } else {
            SendUser(User,SERVER_HEADER" %s's bell is now %s\n",UId->Id,(UId->Bell)?"on":"off");
         }
      } else if (SameString(Variable,"Birthday")) {
	 char DateString[128];
	 time_t DateValue;
         if (User != UId && ! IsSuperUser(User)) {
            DENIED(User,Command);
            return;
         }
	 if(!StringToDate(Value,&DateValue)) {
	    SendUser(User,SERVER_HEADER" Incorrect date format (mm/dd/yy) (month *before* day)\n");
	    return;
	 }
	 UId->Birthday = DateValue;
	 strftime(DateString,(size_t)127,"%A %B %d %Y",localtime(&DateValue));
	 if (UId->Registered) DataBaseChanged = TRUE;
	 if (IsConnected(UId)) {
	   SendUser(UId,SERVER_HEADER" Your birthday is now the %s\n",DateString);
	   SendChannel(UId,SERVER_HEADER" %s's birthday is now the %s\n",UId->Id,DateString);
	 } else {
	   SendUser(User,SERVER_HEADER" %s's birthday is now the %s\n",UId->Id,DateString);
	 }
      } else if (StartWith(Variable,"Plan")) {
	int LineNumber = Variable[4]-'0';
	if (User != UId && ! IsSuperUser(User)) {
	  DENIED(User,Command);
	  return;
	}
	if (LineNumber < 0 || LineNumber >= PLAN_SIZE) {
	    SendUser(User,SERVER_HEADER" \"Plan\" variable must be between 0 and %d\n",PLAN_SIZE-1);
	    return;
	 }

        SetString(&UId->Plan[LineNumber],Value);

	if (UId->Registered) DataBaseChanged = TRUE;
	if (IsConnected(UId)) {
	  SendUser(UId,SERVER_HEADER" Your plan is now :\n");
	  for (I = 0; I < PLAN_SIZE; I++) {
            if (! IsEmptyString(UId->Plan[I])) {
	      SendUser(UId," %d : %s\n",I,UId->Plan[I]);
	    }
	  }
	  SendChannel(UId,SERVER_HEADER" %s's plan changes\n", UId->Id);
	} else {
	  SendUser(User,SERVER_HEADER" %s's plan is now :\n",UId->Id);
	  for (I = 0; I < PLAN_SIZE; I++) {
            if (! IsEmptyString(UId->Plan[I])) {
	      SendUser(User," %d : %s\n",I,UId->Plan[I]);
	    }
	  }
	}
      } else if (SameString(Variable,"Client")) {
         if (User != UId && ! IsSuperUser(User)) {
            DENIED(User,Command);
            return;
         }
         if (Value[0] == '\0') Value = "<Unknown>";
         SetString(&UId->Client,Value);
         if (IsConnected(UId)) SendUser(UId,SERVER_HEADER" Your client is now \"%s\"\n",UId->Client);
      } else if (SameString(Variable,"InOut")) {
         if (User != UId && ! IsSuperUser(User)) {
            DENIED(User,Command);
            return;
         }
         UId->InOut = Boolean(Value);
         if (IsConnected(UId)) SendUser(UId,SERVER_HEADER" Your inout is now %s\n",(UId->InOut)?"on":"off");
      } else if (SameString(Variable,"Join")) {
         if (User != UId && ! IsSuperUser(User)) {
            DENIED(User,Command);
            return;
         }
         UId->Join = Boolean(Value);
         if (IsConnected(UId)) SendUser(UId,SERVER_HEADER" Your join is now %s\n",(UId->Join)?"on":"off");
      } else if (SameString(Variable,"Shout")) {
         UId->Shout = Boolean(Value);
         if (IsConnected(UId)) SendUser(UId,SERVER_HEADER" Your shout is now %s\n",(UId->Shout)?"on":"off");
      } else {
         SendUser(User,SERVER_HEADER" Unknown variable : \"%s\"\n",Variable);
      }
      break;

   case GROUP :

      Group = (group *) Object;
      if ((UserLevel(User) < GroupLevel(Group) || (UserLevel(User) == GroupLevel(Group) && ! SameString(User->Id,Group->Leader))) && ! IsSuperUser(User)) {
         DENIED(User,Command);
         return;
      }

      if (SameString(Variable,"Id")) {
         if (! IsSuperUser(User)) {
            DENIED(User,Command);
            return;
         }
         if (! IsId(Value)) {
            SendUser(User,SERVER_HEADER" Invalid group name : \"%s\"\n",Value);
         } else if (! SameString(Value,Group->Id) && (NewUId = SearchObject(Lists,Value,OBJECT)) != NULL) {
            SendUser(User,SERVER_HEADER" \"%s\" is a reserved word\n",NewUId->Id);
         } else {
            SendUser(User,SERVER_HEADER" group %s is now known as %s\n",Group->Id,Value);
            SetString(&Group->Id,Value);
            DataBaseChanged = TRUE;
         }
      } else if (SameString(Variable,"Leader")) {
         if (! IsId(Value)) {
            SendUser(User,SERVER_HEADER" Invalid login name : \"%s\"\n",Value);
            return;
         }
         UId = SearchUId(Value);
         if (UId == NULL) {
            SendUser(User,SERVER_HEADER" Unknown or unregistered user \"%s\"\n",Value);
            return;
         }
         if ((! IsUpperUser(User,UId) && User != UId) || UserLevel(UId) < GroupLevel(Group)) {
            DENIED(User,Command);
            return;
         }
         OldUId = SearchUId(Group->Leader);
         if (OldUId != NULL && OldUId != User && ! IsUpperUser(User,OldUId)) {
            DENIED(User,Command);
            return;
         }
         strcpy(Group->Leader,UId->Id);
         DataBaseChanged = TRUE;
         if (IsConnected(UId)) {
            SendUser(UId,SERVER_HEADER" %s promoted you as leader of group %s\n",User->Id,Group->Id);
         }
         SendChannel(UId,SERVER_HEADER" %s promoted %s as leader of group %s\n",User->Id,UId->Id,Group->Id);
      } else if (SameString(Variable,"Level")) {
         if (! IsSuperUser(User) || UserLevel(User) <= atoi(Value)) {
            DENIED(User,Command);
            return;
         }
         Group->Level = atoi(Value);
         DataBaseChanged = TRUE;
         SendUser(User,SERVER_HEADER" You set %s level to %d\n",Group->Id,Group->Level);
      } else if (SameString(Variable,"Name")) {
         if (! IsSuperUser(User)) {
            DENIED(User,Command);
            return;
         }
         SetString(&Group->Name,Value);
         DataBaseChanged = TRUE;
         SendUser(User,SERVER_HEADER" You set %s name to \"%s\"\n",Group->Id,Group->Name);
      } else {
         SendUser(User,SERVER_HEADER" Unknown variable : \"%s\"\n",Variable);
      }
      break;

   case CHANNEL :

      Channel = (channel *) Object;

      if (SameString(Variable,"Id")) {
         if (UserLevel(User) < ChannelLevel(Channel)) {
            DENIED(User,Command);
            return;
         }
         if (! IsId(Value)) {
            SendUser(User,SERVER_HEADER" Invalid channel name : \"%s\"\n",Value);
         } else if (! SameString(Value,Channel->Id) && (NewUId = SearchObject(Lists,Value,OBJECT)) != NULL) {
            SendUser(User,SERVER_HEADER" \"%s\" is a reserved word\n",NewUId->Id);
         } else {
            SendUser(User,SERVER_HEADER" channel %s is now known as %s\n",Channel->Id,Value);
            SetString(&Channel->Id,Value);
         }
      } else if (SameString(Variable,"Group")) {
         if (UserLevel(User) < ChannelLevel(Channel)) {
            DENIED(User,Command);
            return;
         }
         if (! IsId(Value)) {
            SendUser(User,SERVER_HEADER" Invalid group name : \"%s\"\n",Value);
            return;
         }
         Group = SearchGroup(Value);
         if (Group == NULL) {
            SendUser(User,SERVER_HEADER" Unknown group \"%s\"\n",Value);
            return;
         }
         if (UserLevel(User) < GroupLevel(Group)) {
            DENIED(User,Command);
            return;
         }
         strcpy(Channel->Group,Group->Id);
         SendChannel(User,SERVER_HEADER" %s put channel %s in group %s\n",User->Id,Channel->Id,Channel->Group);
         SendUser(User,SERVER_HEADER" You put channel %s in group %s\n",Channel->Id,Channel->Group);
      } else if (SameString(Variable,"Topic")) {
         if (/*Channel->Protected && ace: il faut etre du groupe du channel pour changer le topic*/ UserLevel(User) < ChannelLevel(Channel)) {
            DENIED(User,Command);
            return;
         }
         SetString(&Channel->Topic,Value);
         SendChannel(User,SERVER_HEADER" %s set channel %s topic to %s\n",User->Id,Channel->Id,Channel->Topic);
         SendUser(User,SERVER_HEADER" You set channel %s topic to %s\n",Channel->Id,Channel->Topic);
      } else if (SameString(Variable,"Closed")) {
         if (UserLevel(User) < ChannelLevel(Channel)) {
            DENIED(User,Command);
            return;
         }
         Channel->Closed = Boolean(Value);
         SendChannel(User,SERVER_HEADER" %s %ss channel %s\n",User->Id,(Channel->Closed)?"close":"open",Channel->Id);
         SendUser(User,SERVER_HEADER" You %s channel %s\n",(Channel->Closed)?"close":"open",Channel->Id);
      } else if (SameString(Variable,"Hidden")) {
         if (UserLevel(User) < ChannelLevel(Channel)) {
            DENIED(User,Command);
            return;
         }
         Channel->Hidden = Boolean(Value);
         SendChannel(User,SERVER_HEADER" %s %ss channel %s\n",User->Id,(Channel->Hidden)?"hide":"show",Channel->Id);
         SendUser(User,SERVER_HEADER" You %s channel %s\n",(Channel->Hidden)?"hide":"show",Channel->Id);
      } else if (SameString(Variable,"Invite")) {
         if (UserLevel(User) < ChannelLevel(Channel)) {
            DENIED(User,Command);
            return;
         }
         Channel->Invite = Boolean(Value);
         SendChannel(User,SERVER_HEADER" %s sets channel %s to %s\n",User->Id,Channel->Id,(Channel->Invite)?"invite":"non invite");
         SendUser(User,SERVER_HEADER" You set channel %s to %s\n",Channel->Id,(Channel->Invite)?"invite":"non invite");
      } else if (SameString(Variable,"Protected")) {
         if (UserLevel(User) < ChannelLevel(Channel)) {
            DENIED(User,Command);
            return;
         }
         Channel->Protected = Boolean(Value);
         SendChannel(User,SERVER_HEADER" %s sets channel %s topic to %s\n",User->Id,Channel->Id,(Channel->Protected)?"protected":"non protected");
         SendUser(User,SERVER_HEADER" You set channel %s topic to %s\n",Channel->Id,(Channel->Protected)?"protected":"non protected");
      } else if (SameString(Variable,"Resident")) {
         if (UserLevel(User) < ChannelLevel(Channel)) {
            DENIED(User,Command);
            return;
         }
         Channel->Resident = Boolean(Value);
         SendChannel(User,SERVER_HEADER" %s sets channel %s to %s\n",User->Id,Channel->Id,(Channel->Resident)?"resident":"non resident");
         SendUser(User,SERVER_HEADER" You set channel %s to %s\n",Channel->Id,(Channel->Resident)?"resident":"non resident");
         if (Channel->UserNb == 0 && ! Channel->Resident) {
            RemObject(ChannelList,Channel);
            DeleteChannel(Channel);
         }
      } else {
         SendUser(User,SERVER_HEADER" Unknown variable : \"%s\"\n",Variable);
      }
      break;
   }
}

/* Shout() */
/* ok 0 */

void Shout(user *User, command *Command, arglist *ArgList) {

   SendShout(User,SERVER_HEADER" %s shouts: %s\n",User->Id,(char *)ArgList->arg);
   SendUser(User,SERVER_HEADER" You shout: %s\n",(char *)ArgList->arg);
}

/* ShowMsg() */

void ShowMsg(user *User, command *Command, arglist *ArgList) {

   char FileName[STRING_SIZE];
   int  MsgNb;

   sprintf(FileName,"messages/%s",User->Id);
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

/* Shutdown() */
/* ok 0 */

void Shutdown(user *User, command *Command, arglist *ArgList) {

   if (! IsSuperUser(User)) {
      DENIED(User,Command);
      return;
   }

/*
   if (ServerSocket == NO_SOCKET) {
      SendUser(User,SERVER_HEADER" Server socket already closed !\n");
   } else {
      if (! CloseSocket(ServerSocket)) {
         SendUser(User,SERVER_HEADER" Couldn't close server socket !\n");
      } else {
         ServerSocket = NO_SOCKET;
         SendUser(User,SERVER_HEADER" Server socket is now closed !\n");
         SendUsers(User,SERVER_HEADER" %s closed connection socket !\n",User->Id);
      }
   }
*/

   if (ArgList != NULL && SameString((char *)ArgList->arg,"Now")) {
      SendUsers(User,SERVER_HEADER" %s shouts: SHUTDOWN NOW !!!\n",User->Id);
      Exit();
   }
}

/* Switch() */

void Switch(user *User, command *Command, arglist *ArgList) {

   char    *ObjectName, *Variable, *Args;
   object  *Object;
   user    *UId;
   group   *Group;
   channel *Channel;

   Args=(char *)ArgList->arg;
   ObjectName = Args;

/* on checke les variables "switchables" + tard */

   if (StartWith(ObjectName,"Login")
    || StartWith(ObjectName,"Id")
    || StartWith(ObjectName,"Password")
    || StartWith(ObjectName,"Name")
    || StartWith(ObjectName,"EMail")
    || StartWith(ObjectName,"Away")
    || StartWith(ObjectName,"Bell")
    || StartWith(ObjectName,"Plan")
    || StartWith(ObjectName,"Birthday")
    || StartWith(ObjectName,"InOut")
    || StartWith(ObjectName,"Join")
    || StartWith(ObjectName,"Shout")) {

      Variable   = ObjectName;
      ObjectName = User->Id;

   } else if (StartWith(ObjectName,"Leader")
           || StartWith(ObjectName,"Level")) {

      Variable   = ObjectName;
      ObjectName = User->Group;

   } else if (StartWith(ObjectName,"Group")
           || StartWith(ObjectName,"Topic")
           || StartWith(ObjectName,"Closed")
           || StartWith(ObjectName,"Invite")
           || StartWith(ObjectName,"Protected")
           || StartWith(ObjectName,"Hidden")
           || StartWith(ObjectName,"Resident")) {

      Variable   = ObjectName;
      ObjectName = User->Channel;

   } else if (StartWith(ObjectName,"RegOnly")) {

      if (! IsSuperUser(User)) {
         DENIED(User,Command);
         return;
      }

      RegOnly = ! RegOnly;
      SendUser(User,SERVER_HEADER" regonly is now %s\n",(RegOnly)?"on":"off");

      return;

   } else {

      Variable = strpbrk(ObjectName,"\t .");
      if (Variable == NULL) {
         USAGE(User,Command);
         return;
      }
      *Variable++ = '\0';
   }

/*   Value = strpbrk(Variable,"\t ");
   if (Value == NULL) Value = ""; else *Value++ = '\0';
   if (! IsId(ObjectName)) {
      SendUser(User,SERVER_HEADER" Invalid indentifier : \"%s\"\n",ObjectName);
      return;
   }*/
   Object = SearchObject(Lists,ObjectName,OBJECT);
   if (Object == NULL) {
      SendUser(User,SERVER_HEADER" Unknown object \"%s\"\n",ObjectName);
      return;
   }

   switch (Object->Type) {

   case USER :

      UId = (user *) Object;
      if (! IsUpperUser(User,UId) && User != UId) {
         DENIED(User,Command);
         return;
      }

      if (SameString(Variable,"Login") 
          || SameString(Variable,"Id")
          || SameString(Variable,"Group")
          || SameString(Variable,"Password")
          || SameString(Variable,"Name")
          || SameString(Variable,"EMail")
          || SameString(Variable,"Crypt")
          || SameString(Variable,"Birthday")
          || StartWith(Variable,"Plan") ) {
         SendUser(User, SERVER_HEADER" Variable \"%s\" is not switchable\n", Variable);
         return;
      } else if (SameString(Variable,"Away")) {
         UId->Away = ! UId->Away;
         if (IsConnected(UId)) {
            SendUser(UId,SERVER_HEADER" You are %s\n",(UId->Away)?"away":"back");
            SendChannel(UId,SERVER_HEADER" %s is %s\n",UId->Id,(UId->Away)?"away":"back");
         } else {
            SendUser(User,SERVER_HEADER" %s is %s\n",UId->Id,(UId->Away)?"away":"back");
         }
      } else if (SameString(Variable,"Bell")) {
         UId->Bell = ! UId->Bell;
         if (IsConnected(UId)) {
            SendUser(UId,SERVER_HEADER" Your bell is now %s\n",(UId->Bell)?"on":"off");
            SendChannel(UId,SERVER_HEADER" %s's bell is now %s\n",UId->Id,(UId->Bell)?"on":"off");
         } else {
            SendUser(User,SERVER_HEADER" %s's bell is now %s\n",UId->Id,(UId->Bell)?"on":"off");
         }
      } else if (SameString(Variable,"InOut")) {
         UId->InOut = ! UId->InOut;
         if (IsConnected(UId)) SendUser(UId,SERVER_HEADER" Your inout is now %s\n",(UId->InOut)?"on":"off");
      } else if (SameString(Variable,"Join")) {
         UId->Join = ! UId->Join;
         if (IsConnected(UId)) SendUser(UId,SERVER_HEADER" Your join is now %s\n",(UId->Join)?"on":"off");
      } else if (SameString(Variable,"Shout")) {
         UId->Shout = ! UId->Shout;
         if (IsConnected(UId)) SendUser(UId,SERVER_HEADER" Your shout is now %s\n",(UId->Shout)?"on":"off");
      } else {
         SendUser(User,SERVER_HEADER" Unknown variable : \"%s\"\n",Variable);
      }
      break;

   case GROUP :

      Group = (group *) Object;
      if ((UserLevel(User) < GroupLevel(Group) || (UserLevel(User) == GroupLevel(Group) && ! SameString(User->Id,Group->Leader))) && ! IsSuperUser(User)) {
         DENIED(User,Command);
         return;
      }

      if (SameString(Variable,"Id")
       || SameString(Variable,"Leader")
       || SameString(Variable,"Level")
       || SameString(Variable,"Name") ){
         SendUser(User, SERVER_HEADER" Variable \"%s\" is not switchable\n", Variable);
         return;
      }else {
         SendUser(User,SERVER_HEADER" Unknown variable : \"%s\"\n",Variable);
      }
      break;

   case CHANNEL :

      Channel = (channel *) Object;

      if (SameString(Variable,"Id")
       || SameString(Variable,"Group")
       || SameString(Variable,"Topic") ) {
         SendUser(User, SERVER_HEADER" Variable \"%s\" is not switchable\n", Variable);
         return;
      }  else if (SameString(Variable,"Closed")) {
         if (UserLevel(User) < ChannelLevel(Channel)) {
            DENIED(User,Command);
            return;
         }
         Channel->Closed = ! Channel->Closed;
         SendChannel(User,SERVER_HEADER" %s %ss channel %s\n",User->Id,(Channel->Closed)?"close":"open",Channel->Id);
         SendUser(User,SERVER_HEADER" You %s channel %s\n",(Channel->Closed)?"close":"open",Channel->Id);
      } else if (SameString(Variable,"Hidden")) {
         if (UserLevel(User) < ChannelLevel(Channel)) {
            DENIED(User,Command);
            return;
         }
         Channel->Hidden = ! Channel->Hidden;
         SendChannel(User,SERVER_HEADER" %s %ss channel %s\n",User->Id,(Channel->Hidden)?"hide":"show",Channel->Id);
         SendUser(User,SERVER_HEADER" You %s channel %s\n",(Channel->Hidden)?"hide":"show",Channel->Id);
      } else if (SameString(Variable,"Invite")) {
         if (UserLevel(User) < ChannelLevel(Channel)) {
            DENIED(User,Command);
            return;
         }
         Channel->Invite = ! Channel->Invite;
         SendChannel(User, SERVER_HEADER" %s sets channel %s to %s\n",User->Id,Channel->Id,(Channel->Invite)?"invite":"non invite");
         SendUser(User,SERVER_HEADER" You set channel %s to %s\n",Channel->Id,(Channel->Invite)?"invite":"non invite");
      } else if (SameString(Variable,"Protected")) {
         if (UserLevel(User) < ChannelLevel(Channel)) {
            DENIED(User,Command);
            return;
         }
         Channel->Protected = ! Channel->Protected;
         SendChannel(User,SERVER_HEADER" %s sets channel %s topic to %s\n",User->Id,Channel->Id,(Channel->Protected)?"protected":"non protected");
         SendUser(User,SERVER_HEADER" You set channel %s topic to %s\n",Channel->Id,(Channel->Protected)?"protected":"non protected");
      } else if (SameString(Variable,"Resident")) {
         if (UserLevel(User) < ChannelLevel(Channel)) {
            DENIED(User,Command);
            return;
         }
         Channel->Resident = ! Channel->Resident;
         SendChannel(User,SERVER_HEADER" %s sets channel %s to %s\n",User->Id,Channel->Id,(Channel->Resident)?"resident":"non resident");
         SendUser(User,SERVER_HEADER" You set channel %s to %s\n",Channel->Id,(Channel->Resident)?"resident":"non resident");
         if (Channel->UserNb == 0 && ! Channel->Resident) {
            RemObject(ChannelList,Channel);
            DeleteChannel(Channel);
         }
      } else {
         SendUser(User,SERVER_HEADER" Unknown variable : \"%s\"\n",Variable);
      }
      break;
   }
}

/* Tell() */
/* ok 0 */

void Tell(user *User, command *Command, arglist *ArgList) {

   char *Message;
   user  *Tell;

   Tell = (user *) ArgList->arg;
   if (! CanSee(User,Tell)) {
      SendUser(User,SERVER_HEADER" User %s is not logged in !\n",Tell->Id);
      return;
   }

   Message = (char *)ArgList->next->arg;

   if (Message[strlen(Message)-1] == '?') {
      SendUser(Tell,SERVER_HEADER" %s asks you: %s\n",User->Id,Message);
      SendUser(User,SERVER_HEADER" You ask %s: %s\n",Tell->Id,Message);
   } else {
      SendUser(Tell,SERVER_HEADER" %s tells you: %s\n",User->Id,Message);
      SendUser(User,SERVER_HEADER" You tell %s: %s\n",Tell->Id,Message);
   }
   strcpy(Tell->Tell,User->Id);

   if (Tell->Away) SendUser(User,SERVER_HEADER" %s is away and may not be hearing you\n",Tell->Id);
}

/* UnAlias() */
/* ok 0 */

void UnAlias(user *User, command *Command, arglist *ArgList) {

   char *AliasName;
   alias  *Alias;

   AliasName = (char *)ArgList->arg;
   Alias = SearchAlias(User->Aliases,AliasName);
   if (Alias == NULL) {
      SendUser(User,SERVER_HEADER" Unknown alias \"%s\"\n",AliasName);
      return;
   }

   RemObject(User->Aliases,Alias);
   DeleteAlias(Alias);
   DataBaseChanged = TRUE;

   SendUser(User,SERVER_HEADER" You unalias \"%s\"\n",AliasName);
}

/* UpTime() */

void UpTime(user *User, command *Command, arglist *ArgList) {

   debug  *Debug;
   user   *UpUser;
   time_t  Now;
   char    Time[128];
   double  UpTime, Total, Buffer, Speed, Average;

   if (ArgList == NULL) {
      Debug = MtpDebug;
      SendUser(User,SERVER_HEADER" UpTime :\n");
   } else { 
      UpUser = ArgList->arg;
      if (! CanSee(User,UpUser)) {
         SendUser(User,SERVER_HEADER" User %s is not logged in !\n",UpUser->Id);
         return;
      }
      Debug = UpUser->Conn->Debug;
      SendUser(User,SERVER_HEADER" UpTime for %s :\n",UpUser->Id);
   }

   Now    = time(NULL);
   UpTime = difftime(Now,Debug->Start);

   strftime(Time,(size_t)128,"%A %B %d %Y %X ",localtime(&Debug->Start));
   SendUser(User,"Start time    : %s\n",Time);
   strftime(Time,(size_t)128,"%A %B %d %Y %X ",localtime(&Now));
   SendUser(User,"Current time  : %s\n",Time);
   SendUser(User,"Up for        : %s\n",TimeString((int)UpTime));

   Total   = Debug->InByteNb;
   Buffer  = Debug->InBufSize;
   Speed   = (UpTime != 0.0)      ? Debug->InByteNb / UpTime        : 0.0;
   Average = (Debug->InOpNb != 0) ? Debug->InByteNb / Debug->InOpNb : 0.0;
   SendUser(User,"Bytes read    : %8.0f (total), %5.0f (largest buffer), %6.2f/s, %6.2f/call\n",Total,Buffer,Speed,Average);

   Total   = Debug->OutByteNb;
   Buffer  = Debug->OutBufSize;
   Speed   = (UpTime != 0.0)       ? Debug->OutByteNb / UpTime         : 0.0;
   Average = (Debug->OutOpNb != 0) ? Debug->OutByteNb / Debug->OutOpNb : 0.0;
   SendUser(User,"Bytes written : %8.0f (total), %5.0f (largest buffer), %6.2f/s, %6.2f/call\n",Total,Buffer,Speed,Average);

   SendUser(User,SERVER_HEADER" End of UpTime\n");
}

/* Users() */
/* ok 0 */

void Users(user *User, command *Command, arglist *ArgList) {

   int    I;
   node  *Node;
   user  *UId;
   group *Group;
   int    UserNb = 0;
   list  *SortedList=NULL;
   char   Plan;
   char *Args=NULL;
 
   if (ArgList!= NULL) {
      if(ArgList->argtype==ARG_WORD)
      {
         Args=(char *)ArgList->arg;
         if (SameString(Args,"D")) {
            SortedList = SortList2(UserList,(CMP_FCT)DateCmp);
            Args = NULL;
         } else if (SameString(Args,"N")) {
            SortedList = SortList2(UserList,(CMP_FCT)NameCmp);
            Args = NULL;
         } else if (SameString(Args,"T")) {
            SortedList = SortList2(UserList,(CMP_FCT)TimeCmp);
            Args = NULL;
         } else if (SameString(Args,"K")) {
            SortedList = SortList2(UserList,(CMP_FCT)KickCmp);
            Args = NULL;
         } else if (SameString(Args,"KD")) {
            SortedList = SortList2(UserList,(CMP_FCT)KickedCmp);
            Args = NULL;
         } else {
            SendUser(User,SERVER_HEADER" Invalid option : \"%s\"\n",Args);
            return;
         }
         ArgList=ArgList->next;
      }
      if(ArgList !=NULL)
      {
         Group = (group *) ArgList->arg;
         Args = Group->Id;
         if(SortedList == NULL)
            SortedList = SortList2(UserList,(CMP_FCT)NodeCmp);
      }
   } else {
      SortedList = SortList2(UserList,(CMP_FCT)NodeCmp);
   }

   SendUser(User,"    Login     Group   P          Full Name                   EMail address\n");
   SendUser(User,"------------ -------- - --------------------------- -------------------------------\n");
   for (Node = SortedList->Head; Node != NULL; Node = Node->Next) {
      UId = (user *) Node->Object;
      if (UId->Type == USER && UId->Registered && (Args == NULL || SameString(UId->Group,Args))) {
         Plan = ' ';
         for (I = 0; I < PLAN_SIZE; I++) {
            if (! IsEmptyString(UId->Plan[I])) Plan = '*';
	 }
         SendUser(User,"%-12.12s %-8.8s %c %-27.27s %-31.31s\n",UId->Id,UId->Group,Plan,UId->Name,UId->EMail);
         UserNb++;
      }
   }
   if (Args != NULL) {
      SendUser(User,SERVER_HEADER" There %s %d registered user%s in group %s\n",(UserNb>1)?"are":"is",UserNb,(UserNb>1)?"s":"",Args);
   } else {
      SendUser(User,SERVER_HEADER" There %s %d registered user%s\n",(UserNb>1)?"are":"is",UserNb,(UserNb>1)?"s":"");
   }

   ClearList(SortedList);
}


#if CMD_WALL

/* Wall() */
/* ok 0 */

void Wall(user *User, command *Command, arglist *ArgList) {
   char *Args;

   Args = ArgList == NULL ? NULL : (char *)ArgList->arg;

   if (Args == NULL) {
      SendHistory(User,"Wall",WallHistory);
      return;
   }

   if (UserLevel(User) < GroupLevel(SearchGroup(WALL_GROUP))) {
      DENIED(User,Command);
      return;
   }
   
   Trace(WALL_LOG,"%-12.12s %s",User->Id,Args);
   AddHistory(WallHistory,"%-12.12s %s",User->Id,Args);
   SendUsers(User,SERVER_HEADER" %s writes to the wall: %s\n",User->Id,Args);
   SendUser(User,SERVER_HEADER" You write to the wall: %s\n",Args);
}

#endif

#if CMD_WHEREIS

/* WhereIs() */

void WhereIs(user *User, command *Command, arglist *ArgList) {
   int i;
   user *WhereIsUser;
   char  FormationTemp[STRING_SIZE], FullFormation[STRING_SIZE], Result[STRING_SIZE];

   if (ArgList)
   {
     WhereIsUser = (user *) ArgList->arg;

       for (i=0; i<strlen(WhereIsUser->Formation); i++) {
	 FormationTemp[i] = tolower(WhereIsUser->Formation[i]);
       }
       FormationTemp[i] = '\0';
       
       strcpy(FullFormation, "help/formation/");
       strcat(FullFormation, FormationTemp);

       GetCalendarNow(FullFormation, Result, STRING_SIZE);
       
       if (CanSee(User, WhereIsUser)) {
         if (IsAdmin(User)) {
            SendUser(User, SERVER_HEADER" %s : %s\n"SERVER_HEADER" (and is logged on %s)\n", WhereIsUser->Id, Result,WhereIsUser->Host);
				 } else {
            SendUser(User, SERVER_HEADER" %s : %s\n"SERVER_HEADER" (and is logged)\n", WhereIsUser->Id, Result);
         }
       } else {
          SendUser(User, SERVER_HEADER" %s : %s\n", WhereIsUser->Id, Result);
       }
   }
}

#endif

/* Who() */

void Who(user *User, command *Command, arglist *ArgList) {

   int      UserNb = 0;
   time_t   Time;
   node    *Node;
   user    *Logged;
   char    *ChannelName, Time1[10], Time2[10];
   list    *SortedList;

   if (ArgList == NULL) {
      ChannelName = User->Channel;
   } else if (ArgList->argtype == ARG_WORD)
   {
      if(SameString((char *)ArgList->arg,"All")) ChannelName = NULL;
      else
      {
         USAGE(User, Command);
         return;
      }
   } else {
      ChannelName = ((channel *)ArgList->arg)->Id;
   }

   SortedList = SortList2(UserList,(CMP_FCT)NodeCmp);

   Time = time(NULL);
   SendUser(User," Login        Group   Channel   Idle  On For C              From host\n");
   SendUser(User,"------------ -------- -------- ------ ------ - ------------------------------------\n");

   for (Node = SortedList->Head; Node != NULL; Node = Node->Next) {
      Logged = (user *) Node->Object;
      if ((ChannelName == NULL || SameString(Logged->Channel,ChannelName))
       && CanSee(User,Logged)) {
	 if (Logged->Away) {
            strcpy(Time1,"*Away*");
         } else {
            strcpy(Time1,TimeString((int)(Time-Logged->LastSendTime)));
         }
         strcpy(Time2,TimeString((int)(Time-Logged->ConnectTime)));
         SendUser(User,"%-12.12s %-8.8s %-8.8s %6s %6s %-1.1s %-36.36s\n",Logged->Id,Logged->Group,Logged->Channel,Time1,Time2,SearchGroup(Logged->Group)->Symbol, IsAdmin(User) ? Logged->Host:"<Unknown>");
         UserNb++;
      }
   }
   if (ChannelName != NULL) {
      if (UserNb > 1) {
         SendUser(User,SERVER_HEADER" There are currently %d users in channel %s\n",UserNb,ChannelName);
      } else if (UserNb == 1) {
         SendUser(User,SERVER_HEADER" There is currently 1 user in channel %s\n",ChannelName);
      } else {
         SendUser(User,SERVER_HEADER" There is nobody in channel %s\n",ChannelName);
      }
   } else {
      if (UserNb > 1) {
         SendUser(User,SERVER_HEADER" There are currently %d users\n",UserNb);
      } else if (UserNb == 1) {
         SendUser(User,SERVER_HEADER" There is currently 1 user\n");
      } else {
         SendUser(User,SERVER_HEADER" There is nobody !\n");
      }
   }
   ClearList(SortedList);
}

/* NodeCmp() */

int NodeCmp(const user *User1, const user *User2) {

   if (User1->Type != User2->Type) return User1->Type - User2->Type;

   switch (User1->Type) {
      case USER    : if (UserLevel(User1) != UserLevel(User2)) {
                        return UserLevel(User2) - UserLevel(User1);
                     }
                     if (! SameString(User1->Group,User2->Group)) {
                        return strcmp(User1->Group,User2->Group);
                     }
                     break;
      case GROUP   : if (GroupLevel((const group *)User1) != GroupLevel((const group *)User2)) {
                        return GroupLevel((const group *)User2) - GroupLevel((const group *)User1);
                     }
                     break;
      case CHANNEL : if (ChannelLevel((const channel *)User1) != ChannelLevel((const channel *)User2)) {
                        return ChannelLevel((const channel *)User2) - ChannelLevel((const channel *)User1);
                     }
                     break;
   }

   return strcmp(User1->Id,User2->Id);
}

/* BirthCmp() */

int BirthCmp(const user *User1, const user *User2) {

  return User1->dbbn < User2->dbbn;
}

/* AliasCmp() */

int AliasCmp(const alias *Alias1, const alias *Alias2){

   return strcmp(Alias1->Id, Alias2->Id);
}

/* KickCmp() */

int KickCmp(const user *User1, const user *User2) {

   return User1->KickNb > User2->KickNb;
}

/* KickedCmp() */

int KickedCmp(const user *User1, const user *User2) {

   return User1->KickedNb > User2->KickedNb;
}

/* DateCmp() */

int DateCmp(const user *User1, const user *User2) {

   if (User1->Type != User2->Type) return User1->Type - User2->Type;

   switch (User1->Type) {
      case USER    : return (int) difftime(User2->ConnectTime,User1->ConnectTime);
                     break;
      case GROUP   : if (GroupLevel((const group *)User1) != GroupLevel((const group *)User2)) {
                        return GroupLevel((const group *)User2) - GroupLevel((const group *)User1);
                     }
                     break;
      case CHANNEL : if (ChannelLevel((const channel *)User1) != ChannelLevel((const channel *)User2)) {
                        return ChannelLevel((const channel *)User2) - ChannelLevel((const channel *)User1);
                     }
                     break;
   }

   return strcmp(User1->Id,User2->Id);
}

/* NameCmp() */

int NameCmp(const user *User1, const user *User2) {

   if (User1->Type != User2->Type) return User1->Type - User2->Type;

   switch (User1->Type) {
      case GROUP   : if (GroupLevel((const group *)User1) != GroupLevel((const group *)User2)) {
                        return GroupLevel((const group *)User2) - GroupLevel((const group *)User1);
                     }
                     break;
      case CHANNEL : if (ChannelLevel((const channel *)User1) != ChannelLevel((const channel *)User2)) {
                        return ChannelLevel((const channel *)User2) - ChannelLevel((const channel *)User1);
                     }
                     break;
   }

   return strcmp(User1->Id,User2->Id);
}

/* TimeCmp() */

int TimeCmp(const user *User1, const user *User2) {

   if (User1->Type != User2->Type) return User1->Type - User2->Type;

   switch (User1->Type) {
      case USER    : return User2->TotalTime - User1->TotalTime;
                     break;
      case GROUP   : if (GroupLevel((const group *)User1) != GroupLevel((const group *)User2)) {
                        return GroupLevel((const group *)User2) - GroupLevel((const group *)User1);
                     }
                     break;
      case CHANNEL : if (ChannelLevel((const channel *)User1) != ChannelLevel((const channel *)User2)) {
                        return ChannelLevel((const channel *)User2) - ChannelLevel((const channel *)User1);
                     }
                     break;
   }

   return strcmp(User1->Id,User2->Id);
}

/* End of Command.C */

