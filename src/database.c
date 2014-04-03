
/* DataBase.C */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>

/*
#define _XOPEN_SOURCE
#include <unistd.h>
*/
extern char *crypt (const char *, const char *); /* Solashit */

#include "database.h"
#include "types.h"
#include "channel.h"
#include "group.h"
#include "list.h"
#include "object.h"
#include "mtp.h"
#include "server.h"
#include "token.h"
#include "user.h"
#include "variable.h"

/* Constants */

#define DATABASE_NAME "database"

/* Variables */

int DataBaseChanged;

#define LINE_MAXSIZE 512
static char *Line = NULL, *LinePtr;
static char NewUsersFile[80];

/* Prototypes */

static char *FirstToken (const char *String);
static char *NextToken  (void);
static char *LastToken  (void);

static int   Rand64     (void);

/* Functions */

/* ReadDataBase() */

int ReadDataBase(void) {

   FILE   *DataBase;
   char    Line[LINE_MAXSIZE], *UserName, *GroupName, *Password, *Name, *EMail, *Formation, *LeaderName, *AliasName, *Command, *Plan, *LastLoginHost, *LastFailHost;
   time_t  RegisterTime, ConnectTime, Birthday, LastLoginTime, LastFailTime;
   int     Level, KickNb, KickedNb, LoginNb, TotalTime, LineNumber, FailureNb;
   user   *User, *UId;
   group  *Group;
   alias  *Alias;

   DataBase = fopen(DATABASE_NAME,"r");
   if (DataBase == NULL) {
      Error("Couldn't open database");
      return FALSE;
   }

   do {
      if (fgets(Line,LINE_MAXSIZE,DataBase) == NULL) {
         Error("\"# <Mtp> Variables\" line not found in \"%s\"",DATABASE_NAME);
         fclose(DataBase);
         return FALSE;
      }
   } while (! SameString(Line,"# <Mtp> Variables\n") || ! SameString(Line,"# Variables\n"));

   /* # <Mtp> Variables */
   /* UserNbMax ... */

   do {
      if (fgets(Line,LINE_MAXSIZE,DataBase) == NULL) {
         Error("\"UserNbMax ...\" line not found in \"%s\"",DATABASE_NAME);
         fclose(DataBase);
         return FALSE;
      }
   } while (! StartWith(Line,"UserNbMax "));

   UserNbMax = atoi(Line+10);

   do {
      if (fgets(Line,LINE_MAXSIZE,DataBase) == NULL) {
         Error("\"NewUsersFile ...\" line not found in \"%s\"",DATABASE_NAME);
         fclose(DataBase);
         return FALSE;
      }
   } while (! StartWith(Line,"NewUsersFile "));

   strcpy(NewUsersFile, Line+13);
   if(strlen(NewUsersFile)>0 && NewUsersFile[strlen(NewUsersFile)-1]=='\n') {
      NewUsersFile[strlen(NewUsersFile)-1] = '\0';
   }

   do {
      if (fgets(Line,LINE_MAXSIZE,DataBase) == NULL) {
         Error("\"# <Mtp> Groups\" line not found in \"%s\"",DATABASE_NAME);
         fclose(DataBase);
         return FALSE;
      }
   } while (! SameString(Line,"# <Mtp> Groups\n") || ! SameString(Line,"# Groups\n"));

   /* # <Mtp> Groups */
   /* # Group|Leader|Level|Name|Symbol */
   /* group *NewGroup(const char *Id, const char *Leader, int Level, const char *Name, const char *Symbol); */

   do {

      if (fgets(Line,LINE_MAXSIZE,DataBase) == NULL) {
         Error("\"# <Mtp> Users\" line not found in \"%s\"",DATABASE_NAME);
         fclose(DataBase);
         return FALSE;
      }

      if (Line[0] != '\n' && Line[0] != '#') {

         GroupName  = FirstToken(Line);
         LeaderName = NextToken();
         Level      = atoi(NextToken());
         Name       = NextToken();
         Symbol     = LastToken();

         Group = NewGroup(GroupName,LeaderName,Level,Name,Symbol);
         if (Group != NULL) AddTail(GroupList,Group);
      }

   } while (! SameString(Line,"# <Mtp> Users\n") || ! SameString(Line,"# Users\n"));

   /* # <Mtp> Users */
   /* # User|Group|Password|Name|EMail|Formation|RegisterTime|ConnectTime|Birthday|KickNb|KickedNb|LoginNb|TotalTime|LastLoginHost|LastLoginTime|FailureNb|LastFailHost|LastFailTime */
   /* user *NewUId(const char *Id, const char *Group, const char *Password, const char *Name, const char *EMail, const char *Formation, time_t RegisterTime, time_t ConnectTime, time_t Birthday, int KickNb, int KickedNb, int LoginNb, int TotalTime, const char *LastLoginHost, time_t LastLoginTime, int FailureNb, const char *LastFailHost, time_t LastFailTime); */

   do {

      if (fgets(Line,512,DataBase) == NULL) {
         Error("\"# <Mtp> Aliases\" line not found in \"%s\"",DATABASE_NAME);
         fclose(DataBase);
         return FALSE;
      }

      if (Line[0] != '\n' && Line[0] != '#') {

         UserName      = FirstToken(Line);
         GroupName     = NextToken();
         Password      = NextToken();
         Name          = NextToken();
         EMail         = NextToken();
         Formation     = NextToken();
         RegisterTime  = (time_t) atoi(NextToken());
         ConnectTime   = (time_t) atoi(NextToken());
         Birthday      = (time_t) atoi(NextToken());
         KickNb        = atoi(NextToken());
         KickedNb      = atoi(NextToken());
         LoginNb       = atoi(NextToken());
         TotalTime     = atoi(NextToken());
         LastLoginHost = NextToken();
         LastLoginTime = (time_t) atoi(NextToken());
         FailureNb     = atoi(NextToken());
         LastFailHost  = NextToken();
         LastFailTime  = (time_t) atoi(NextToken());

         UId = NewUId(UserName,GroupName,Password,Name,EMail,Formation,RegisterTime,ConnectTime,Birthday,KickNb,KickedNb,LoginNb,TotalTime,LastLoginHost,LastLoginTime,FailureNb,LastFailHost,LastFailTime);
         if (UId != NULL) AddTail(UserList,UId);
      }

   } while (! SameString(Line,"# <Mtp> Aliases\n") || ! SameString(Line,"# Aliases\n"));

   /* # <Mtp> Aliases */
   /* # User|Alias|Command */
   /* alias *NewAlias(const char *Id, const char *Command); */

   do {

      if (fgets(Line,LINE_MAXSIZE,DataBase) == NULL) {
         Error("\"# <Mtp> Plans\" line not found in \"%s\"",DATABASE_NAME);
         fclose(DataBase);
         return FALSE;
      }

      if (Line[0] != '\n' && Line[0] != '#') {

         UserName  = FirstToken(Line);
         AliasName = NextToken();
         Command   = LastToken();

         User = SearchUId(UserName);
         if (User != NULL) {
            Alias = NewAlias(AliasName,Command);
            if (Alias != NULL) AddTail(User->Aliases,Alias);
         }
      }
   } while (! SameString(Line,"# <Mtp> Plans\n") || ! SameString(Line,"# Plans\n"));

   /* # <Mtp> Plans */
   /* # User|LineNumber|Plan */

   while (fgets(Line,LINE_MAXSIZE,DataBase) != NULL) {

      if (Line[0] != '\n' && Line[0] != '#') {

         UserName   = FirstToken(Line);
         LineNumber = atoi(NextToken());
         Plan       = LastToken();

         if (LineNumber >= 0 && LineNumber < PLAN_SIZE) {
            User = SearchUId(UserName);
            if (User != NULL) SetString(&User->Plan[LineNumber],Plan);
         }
      }
   }

   fclose(DataBase);

   DataBaseChanged = FALSE;

   return TRUE;
}

/* WriteDataBase() */

int WriteDataBase(void) {

   int I;
   FILE  *DataBase;
   node  *Node, *Node2;
   user  *User;
   group *Group;
   alias *Alias;

   DataBase = fopen(DATABASE_NAME,"w");
   if (DataBase == NULL) {
      perror("database write fopen");
      return FALSE;
   }

   fprintf(DataBase,"\n# <Mtp> Variables\n\n");
   fprintf(DataBase,"UserNbMax %d\n",UserNbMax);
   fprintf(DataBase,"NewUsersFile %s\n",NewUsersFile);

   fprintf(DataBase,"\n# <Mtp> Groups\n\n# Group|Leader|Level|Name|Symbol\n\n");

   for (Node = GroupList->Head; Node != NULL; Node = Node->Next) {
      Group = (group *) Node->Object;
      if (Group->Type == GROUP) {
         fprintf(DataBase,"%s|%s|%d|%s|%s\n",Group->Id,Group->Leader,Group->Level,Group->Name,Group->Symbol);
      }
   }

   fprintf(DataBase,"\n# <Mtp> Users\n\n# User|Group|Password|Name|EMail|Formation|RegisterTime|ConnectTime|Birthday|KickNb|KickedNb|LoginNb|TotalTime|LastLoginHost|LastLoginTime|FailureNb|LastFailHost|LastFailTime\n\n");
   for (Node = UserList->Head; Node != NULL; Node = Node->Next) {
      User = (user *) Node->Object;
      if (User->Type == USER && User->Registered) {
         fprintf(DataBase,"%s|%s|%s|%s|%s|%s|%d|%d|%d|%d|%d|%d|%d|%s|%d|%d|%s|%d\n",User->Id,User->Group,User->Password,User->Name,User->EMail,User->Formation,(int)User->RegisterTime,(int)User->ConnectTime,(int)User->Birthday,User->KickNb,User->KickedNb,User->LoginNb,User->TotalTime,User->LastLoginHost,(int)User->LastLoginTime,User->FailureNb,User->LastFailHost,(int)User->LastFailTime);
      }
   }

   fprintf(DataBase,"\n# <Mtp> Aliases\n\n# User|Alias|Command\n\n");

   for (Node = UserList->Head; Node != NULL; Node = Node->Next) {
      User = (user *) Node->Object;
      if (User->Type == USER && User->Registered) {
         for (Node2 = User->Aliases->Head; Node2 != NULL; Node2 = Node2->Next) {
            Alias = (alias *) Node2->Object;
            fprintf(DataBase,"%s|%s|%s\n",User->Id,Alias->Id,Alias->Command);
         }
      }
   }

   fprintf(DataBase,"\n# <Mtp> Plans\n\n# User|LineNumber|Plan\n\n");

   for (Node = UserList->Head; Node != NULL; Node = Node->Next) {
      User = (user *) Node->Object;
      if (User->Type == USER && User->Registered) {
         for (I = 0; I <PLAN_SIZE; I++) {
	   if(User->Plan[I] != NULL && User->Plan[I][0] != '\0') {
	     fprintf(DataBase,"%s|%d|%s\n",User->Id,I,User->Plan[I]);
	   }
         }
      }
   }

   fprintf(DataBase,"\n");

   fclose(DataBase);

   DataBaseChanged = FALSE;

   return TRUE;
}

/* FirstToken() */

static char *FirstToken(const char *String) {

   SetString(&Line,String);
   LinePtr = Line;

   return NextToken();
}

/* NextToken() */

static char *NextToken(void) {

   char *Start;

   Start = LinePtr;

   while (*LinePtr != '\0' && *LinePtr != '|' && *LinePtr != '\n') LinePtr++;
   if (*LinePtr != '\0') *LinePtr++ = '\0';
   
   return Start;
}

/* LastToken() */

static char *LastToken(void) {

   char *Start;

   Start = LinePtr;

   while (*LinePtr != '\0' && *LinePtr != '\n') LinePtr++;
   if (*LinePtr != '\0') *LinePtr++ = '\0';
   
   return Start;
}

/* CryptPassword() */

char *CryptPassword(const char *Password) {

   char Salt[3];
   static char SaltString[64] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";

   Salt[0] = SaltString[Rand64()];
   Salt[1] = SaltString[Rand64()];
   Salt[2] = '\0';

   return crypt(Password,Salt);
}

/* Rand64() */

static int Rand64(void) {

   return (int) floor(64.0*(double)rand()/((double)RAND_MAX+1.0));
}

/* CheckPassword() */

int CheckPassword(const char *String, const char *Encrypted) {

   char Salt[3];

   if (strlen(Encrypted) != ENCRYPTED_SIZE) {
      Error("CheckPassword(): \"%s\" is not a valid encrypted password",Encrypted);
      return FALSE;
   }

   Salt[0] = Encrypted[0];
   Salt[1] = Encrypted[1];
   Salt[2] = '\0';

   return strcmp(Encrypted,crypt(String,Salt)) == 0;
}

void CheckNewUsersFile(void) {

	static time_t LastCheckTime = 0;
	struct stat   buf;
	FILE   *fp = NULL;
	char    Line[256], *UserName, *GroupName, *Password, *Name, *EMail, *Formation, *LeaderName, *AliasName, *Command, *Plan, *LastLoginHost, *LastFailHost;
	time_t  RegisterTime, ConnectTime, Birthday, LastLoginTime, LastFailTime;
	int     Level, KickNb, KickedNb, LoginNb, TotalTime, LineNumber, FailureNb;
	user   *UId = NULL;
	group  *Group = NULL;
	alias  *Alias = NULL;
	char   *Action = NULL;

	/* Only check every minutes */
	if (time(NULL) <= LastCheckTime + 60)
		return;

	LastCheckTime = time(NULL);

	/*	printf("check file now\n");*/
	
	if(NewUsersFile==NULL) {
	  printf("file is null\n");
	  return;
	}

	if (stat (NewUsersFile,&buf) == 0) {

		/* format "+|login8|cryptpass|group\n"; */
		fp = fopen(NewUsersFile,"rb");
		if (fp == NULL)
			return;

		do {

			if (fgets(Line,256,fp) == NULL)
				break;

			if (Line[0] != '\n' && Line[0] != '#') {

			        Action    = FirstToken(Line);
				UserName  = NextToken();
				Password  = NextToken();
				GroupName = NextToken();
				
				if(strlen(Action) == 0) {
				 
				  Trace(NEWUSERS_LOG,"Empty action");

				} else if(Action[0]=='+') {
				  /* Add or modify a user */

				  UId = SearchUId(UserName);
				  if(UId == NULL) {
				    /* User not found, it's a new one, add it */

				    Name          = NULL;
				    EMail         = NULL;
				    Formation     = NULL;
				    RegisterTime  = time(NULL);
				    ConnectTime   = (time_t) 0;
				    Birthday      = (time_t) 0;
				    KickNb        = 0;
				    KickedNb      = 0;
				    LoginNb       = 0;
				    TotalTime     = 0;
				    LastLoginHost = NULL;
				    LastLoginTime = (time_t) 0;
				    FailureNb     = 0;
				    LastFailHost  = NULL;
				    LastFailTime  = (time_t) 0;
				    
				    UId = NewUId(UserName,GroupName,Password,Name,EMail,Formation,RegisterTime,ConnectTime,Birthday,KickNb,KickedNb,LoginNb,TotalTime,LastLoginHost,LastLoginTime,FailureNb,LastFailHost,LastFailTime);
				    if (UId != NULL) {
				      AddTail(UserList,UId);
				      Trace(NEWUSERS_LOG,"User '%s' '%s' '%s' added", UserName, Password, GroupName);
				    } else {
				      Trace(NEWUSERS_LOG,"Can't create user '%s'", UserName);
				    }
				  } else {
				    /* User already exists, modify it */

				    Trace(NEWUSERS_LOG,"User '%s' changed from '%s' '%s' to '%s' '%s'", UserName, UId->Password, UId->Group, Password, GroupName);
				    strcpy(UId->Password,Password);
				    if(strlen(GroupName)>0)
				       strcpy(UId->Group,GroupName);
				  }				  
				} else if(Action[0]=='-') {

				  /* Remove the user from the database */
				  char FileName[STRING_SIZE];
				  UId = SearchUId(UserName);
				  if(UId == NULL) {
				    Trace(NEWUSERS_LOG,"Can't Delete, user '%s' is not registered", UserName);
				  } else {
				    Trace(NEWUSERS_LOG,"Deleting user '%s'", UserName);
				    UId->Registered = FALSE;

				    /* Remove the user's messages */
				    
				    sprintf(FileName,"messages/%s",UId->Id);
				    if (FileExists(FileName)) DeleteFile(FileName);
				  }
				} else {
				  Trace(NEWUSERS_LOG,"Unknown action '%s'", Action);
				}
			}
		} while (TRUE);

		fclose(fp);
		unlink(NewUsersFile);

		DataBaseChanged = TRUE;
	}
	/* debug only else
	{
	  Trace(NEWUSERS_LOG,"File '%s' doesn't exists", NewUsersFile);
	  }*/
}


/* End of DataBase.C */
