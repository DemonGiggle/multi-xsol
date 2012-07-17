/*****************************************************************************/
/*  xsol.c - A solitare program for X Windows written using Motif            */
/*  Copyright (C) 1998 Brian Masney <masneyb@newwave.net>                    */
/*                                                                           */
/*  This program is free software; you can redistribute it and/or modify     */
/*  it under the terms of the GNU General Public License as published by     */
/*  the Free Software Foundation; either version 2 of the License, or        */
/*  (at your option) any later version.                                      */
/*                                                                           */
/*  This program is distributed in the hope that it will be useful,          */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/*  GNU General Public License for more details.                             */
/*                                                                           */
/*  You should have received a copy of the GNU General Public License        */
/*  along with this program; if not, write to the Free Software              */
/*  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/*****************************************************************************/

#include <Xm/Xm.h>
#include <Xm/CascadeB.h> 
#include <Xm/DrawingA.h>
#include <Xm/MainW.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/Label.h>
#include <Xm/MessageB.h>
#include <Xm/Form.h>
#include <Xm/ToggleB.h>
#include <Xm/Frame.h>
#include <Xm/Separator.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstdint>
#include "xsol.h"  

#define CARD_WIDTH	70   /* Width of the card */
#define CARD_HEIGHT	85   /* Height of the card */
#define PILE_COL	10   /* Starting x coord for piles */
#define PILE_ROW	125  /* Starting y coord for bottom row */
#define DECK_ROW	10   /* Starting y coord for top row */
#define MOUSE_DCLK	300  /* Allowable time for mouse dbl-click */
#define VERSION		"0.31"

struct stck { /* This contains all of the information about a card */
   int xcoord, ycoord;
   Pixmap under, face; /* Under is what was underneath the card */
                       /* Face is the generated card face */
   char type[3]; /* Type of card, for example 2S = Two of Spades */
   int shown; /* Can we see it? */
   struct stck *next; /* Next card in stack (works it way from bottom up) */
   struct stck *prev; /* Prev card in stack (works it way from top down) */
};

struct undo_stk { /* Undo stack */
   int xcoord, ycoord, frompile, topile, shown, numcards, score;
   struct undo_stk *next;
};

struct stck *piles[13]; /* Pile information. Piles 0-6 are the bottom 7
			 * piles. Pile 7 is the deck of cards, 8 is the
			 * cards in the deck you can see and 9-12 are
			 * the final resting areas for the cards */

void UpScore(int num);
char CardOrder(char face);
int IsValidMove(int frompile, char fromcard[3], int topile, char tocard[3]);
void init_cards(void);
void exitCB(Widget widget, XtPointer client_data, XtPointer call_data);
void newCB(Widget widget, XtPointer client_data, XtPointer call_data);
void undoCB(Widget widget, XtPointer client_data, XtPointer call_data);
void aboutCB(Widget parent, XtPointer client_data, XtPointer call_data);
void optionsCB(Widget parent, XtPointer client_data, XtPointer call_data);
void buttonCB(Widget parent, XtPointer client_data, XtPointer call_data);
void DrawCards(Display* display, Window window, int width, int height);
void drawCB(Widget widget, XtPointer client_data, XtPointer call_data);
void mousepress(Widget widget, XtPointer client_data, XButtonEvent *event, Boolean *continue_to_dispatch);
void mouserel(Widget widget, XtPointer client_data, XButtonEvent *event, Boolean *continue_to_dispatch);
void mousedrag(Widget widget, XtPointer client_data, XButtonEvent *event, Boolean *continue_to_dispatch);
void timerCB(XtPointer client_data, XtIntervalId *timer_id);
void UpdStatusBar(void);
Widget CreateMenu(Widget parent, char* name);
Widget CreateMenuItem(Widget parent, char *name, XtCallbackProc callback, XtPointer client_data);

Widget drawa, /* Drawing area widget. Public so that I can call XtWindow and
	       * XtDisplay from inside the functions. Just easier to make it 
	       * this way. I know, bad coding...will fix it soon */
       score_widget, /* Score widget */
       undomenu, /* Undo Menu Item so that it can be activated/inactived; */
       drawone_wid, /* Options dialog, Draw one */
       drawthree_wid; /* Options dialog, Draw three */
XmString newscore;
GC gc; /* Graphics content */
Pixmap cardbk, face, under; /* cardbk is pic of a card not being shown
			     * face and under are used for when a card is
			     * being dragged with the mouse */
int drawone = 1, /* 1 = Draw One,  0 = Draw Three */
    istimer = 1, /* 1 = Timer On, 0 = Timer Off */
    isscore = 1, /* 1 = Keep Score, 0 = No Score */
    iscountcrd = 0; /* 1 = Count # of cards left in deck, 0 = Don't */
int pile_distance;
int inited = 0, /* Set to false if the drawCB function needs to grab the
	         * area under the cards */
   dragged = 0, /* Set to true if the mouse is moving a card */
   diffx = -1, diffy = -1, xcoord, ycoord, /* Used to detect double click of
					    * mouse */
   frompile, /* Set when dragged is true. Keeps track of the pile of where the 
	      * moved card is from */
   ydraglen, /* Used for multiple card moves. It will keep track of the y 
	      * distance to remember when moving them */
   score, /* This isn't the score ;) */
   deckturn, /* Number of times shuffled through the deck */
   numcards; /* Number of cards left in the deck */
Time dclk_time = (Time) 0; /* Number of times shuffled through the deck. Used
			    * only for scoring purposes */
unsigned long time_elapsed; /* Number of seconds elapsed */
struct stck *moved_card; /* Pointer to the currently top moved card */
struct undo_stk *undo; /* Stack containing data for undo */

int xsol_main()
{
   XtAppContext app_context;
   Widget parent, mainwindow, menubar, gamemenu;
   Display *display;
   Arg args[20];
   int n, i;

   int argc = 0;
   char** argv = NULL;
    
   for(i=1; i<argc; i++) {
      if(strcmp(argv[i], "-notimer") == 0) istimer = 0;
      else if(strcmp(argv[i], "-timer") == 0) istimer = 1;
      else if(strcmp(argv[i], "-noscore") == 0) isscore = 0;
      else if(strcmp(argv[i], "-score") == 0) isscore = 1;
      else if(strcmp(argv[i], "-nocountcrd") == 0) iscountcrd = 0;
      else if(strcmp(argv[i], "-countcrd") == 0) iscountcrd = 1;
      else if(strcmp(argv[i], "-drawone") == 0) drawone = 1;
      else if(strcmp(argv[i], "-drawthree") == 0) drawone = 0;
      else {
         if(strcmp(argv[i], "--help") != 0) printf("Error: Option %d is invalid: %s\n\n", i, argv[i]);
         printf("X Solitare version %s. Copyright (C) 1998 Brian Masney\n", VERSION);
         printf("X Solitare is free software and comes with ABSOLUTELY NO WARRANTY\n");
         printf("Please read the COPYING file for the license agreement\n\n");	          
         printf("Options available:\n");
         printf("\t-notimer\tDisables the timer%s\n", !istimer ? " (default)" : "");
         printf("\t-timer\t\tEnables the timer%s\n", istimer ? " (default)" : "");
         printf("\t-noscore\tDoes not keep track of the score%s\n", !isscore ? " (default)" : "");
         printf("\t-score\t\tKeeps track of the score%s\n", isscore ? " (default)" : "");
         printf("\t-nocountcrd\tDoes not tell you how many cards left in deck%s\n", !iscountcrd ? " (default)" : "");
         printf("\t-countcrd\tTells you how many cards are left in deck%s\n", iscountcrd ? " (default)" : "");
         printf("\t-drawone\tWill draw one card from the deck%s\n", drawone ? " (default)" : "");
         printf("\t-drawthree\twill draw three cards from the deck%s\n", !drawone ? " (default)" : "");
         exit(-1);
      }
   }
   n = 0;
   XtSetArg(args[n], XmNtitle, "X Solitare"); n++; 
   XtSetArg(args[n], XmNwidth, 690); n++;
   XtSetArg(args[n], XmNheight, 460); n++;
   parent = XtAppInitialize(&app_context, NULL, (XrmOptionDescList) NULL, 0,
                            &argc, argv, (String*) NULL, args, n);

   n = 0;
   mainwindow = XmCreateMainWindow(parent, NULL, args, n);

   n = 0;
   drawa = XmCreateDrawingArea(mainwindow, NULL, args, n);

   XtAddCallback(drawa, XmNexposeCallback, (XtCallbackProc) drawCB, (XtPointer) NULL);
   XtAddCallback(drawa, XmNinputCallback, (XtCallbackProc) drawCB, (XtPointer) NULL);
   XtAddCallback(drawa, XmNresizeCallback, (XtCallbackProc) drawCB, (XtPointer) NULL);

   XtAddEventHandler(drawa, ButtonPressMask, 0, (XtEventHandler) mousepress, (XtPointer) NULL);
   XtAddEventHandler(drawa, ButtonReleaseMask, 0, (XtEventHandler) mouserel, (XtPointer) NULL);
   XtAddEventHandler(drawa, Button1MotionMask, 0, (XtEventHandler) mousedrag, (XtPointer) NULL);
   XtManageChild(drawa);


   n = 0;
   menubar = XmCreateMenuBar(mainwindow, (char*)"menubar", args, n);
   XtManageChild(menubar);
   gamemenu = CreateMenu(menubar, (char*)"Game");
   CreateMenuItem(gamemenu, (char*)"New Game", (XtCallbackProc) newCB, (XtPointer) NULL);
   undomenu = CreateMenuItem(gamemenu, (char*)"Undo", (XtCallbackProc) undoCB, (XtPointer) NULL);
   CreateMenuItem(gamemenu, (char*)"Options", (XtCallbackProc) optionsCB, (XtPointer) parent);
   CreateMenuItem(gamemenu, (char*)"About", (XtCallbackProc) aboutCB, (XtPointer) parent);
   CreateMenuItem(gamemenu, (char*)"Exit", (XtCallbackProc) exitCB, (XtPointer) NULL);

   n = 0;
   XtSetArg(args[n], XmNalignment, XmALIGNMENT_END); n++;
   score_widget = XmCreateLabel(mainwindow, (char*)"", args, n);
   XtAppAddTimeOut(app_context, 1000, (XtTimerCallbackProc) timerCB, (XtPointer) app_context);
   XtManageChild(mainwindow);
   XtRealizeWidget(parent);

   display = XtDisplay(drawa);
   gc = XCreateGC(display , XtWindow(drawa), 0, (XGCValues *) NULL);
   init_cards();

   XmMainWindowSetAreas(mainwindow, menubar, (Widget) NULL, (Widget) NULL, 
       (Widget) NULL, drawa);
   XtVaSetValues(mainwindow, XmNmessageWindow, score_widget, NULL);
   XtManageChild(score_widget);
   XtAppMainLoop(app_context);
   return 0;
}

int main(int argc, char **argv) {
	xsol_main();
}
/*****************************************************************************/
void timerCB(XtPointer client_data, XtIntervalId *timer_id) {

/* This function gets called every second */

   time_elapsed++;
   if(istimer) UpdStatusBar();
   XtAppAddTimeOut((XtAppContext) client_data, 1000, (XtTimerCallbackProc) timerCB, client_data);
}
/*****************************************************************************/
void exitCB(Widget widget, XtPointer client_data, XtPointer call_data) {  

/* Menu callback for File, Exit */

   struct stck *temp;
   struct undo_stk *temp_undo;
   int i;
   
   for(i=0; i<13; i++) {
      while(piles[i] != NULL) {
         temp = piles[i];
         piles[i] = piles[i]->next;
         free(temp);
      }
   }
   while(undo != NULL) {
      temp_undo = undo;
      undo = undo->next;
      free(temp_undo);
   }
   exit(0);
}
/*****************************************************************************/
void aboutCB(Widget parent, XtPointer client_data, XtPointer call_data) {

/* The about dialog box */

   Widget info, label;
   XmString inf_lbl, push_lbl;
   Arg args[20];
   int n;
      
   n = 0;
   XtSetArg(args[n], XmNtitle, "About"); n++;
   XtSetArg(args[n], XmNdialogStyle, XmDIALOG_SYSTEM_MODAL); n++;
   info = XmCreateFormDialog(*(Widget *) client_data, NULL, args, n);
   inf_lbl = XmStringCreateLocalized((char*)"\nThis program was written by\n\nBrian Masney\nmasneyb@newwave.net\n\nPlease feel free to email me any comments,\nsuggestions, or bugs about this program.\n");
   label = XtVaCreateManagedWidget(NULL, xmLabelWidgetClass, info,
      XmNtopAttachment, XmATTACH_FORM,
      XmNtopOffset, 10, 
      XmNleftAttachment, XmATTACH_FORM,
      XmNleftOffset, 10, 
      XmNrightAttachment, XmATTACH_FORM,
      XmNrightOffset, 10, 
      XmNlabelString, inf_lbl,
      NULL);
   push_lbl = XmStringCreateLocalized((char*)"OK");
   XtVaCreateManagedWidget(NULL, xmPushButtonWidgetClass, info,  
      XmNlabelString, push_lbl,
      XmNtopAttachment, XmATTACH_WIDGET,
      XmNtopWidget, label,
      XmNbottomAttachment, XmATTACH_FORM, 
      XmNbottomOffset, 10, 
      XmNleftAttachment, XmATTACH_POSITION, 
      XmNleftPosition, 20, 
      XmNrightAttachment, XmATTACH_POSITION, 
      XmNrightPosition, 80,  
      NULL);
   XtManageChild(info);
}
/*****************************************************************************/
void optionsCB(Widget parent, XtPointer client_data, XtPointer call_data) {

/* The options dialog box. */

   Widget dialog, frame, rowcol, tim_ck, score_ck, num_ck, title, ok;
   Arg args[20];
   int n;
   
   n = 0;
   XtSetArg(args[n], XmNtitle, "Settings"); n++;
   XtSetArg(args[n], XmNdialogStyle, XmDIALOG_SYSTEM_MODAL); n++;
   dialog = XmCreateFormDialog(*(Widget *) client_data, NULL, args, n);
   frame = XtVaCreateManagedWidget("frame", xmFrameWidgetClass, dialog, 
      XmNtopAttachment, XmATTACH_FORM,
      XmNtopOffset, 5,
      XmNleftAttachment, XmATTACH_FORM,
      XmNleftOffset, 10,
      XmNrightAttachment, XmATTACH_FORM,
      XmNrightOffset, 10,
      NULL);
   title = XtVaCreateManagedWidget("Settings", xmLabelWidgetClass, frame, 
             XmNchildType, XmFRAME_TITLE_CHILD,
             XmNchildHorizontalAlignment, XmALIGNMENT_BEGINNING,
             XmNchildVerticalAlignment, XmALIGNMENT_BASELINE_BOTTOM, NULL);   

   rowcol = XtVaCreateManagedWidget("rowcol", xmRowColumnWidgetClass, frame, NULL);

   tim_ck = XtVaCreateManagedWidget("Timed Game", xmToggleButtonWidgetClass, rowcol, 
      XmNindicatorType, XmONE_OF_MANY, 
      XmNset, istimer, 
      NULL);
   XtAddCallback(tim_ck, XmNvalueChangedCallback, buttonCB, (XtPointer) 1);

   score_ck = XtVaCreateManagedWidget("Keep Score", xmToggleButtonWidgetClass, rowcol, 
      XmNindicatorType, XmONE_OF_MANY, 
      XmNset, isscore, 
      NULL);
   XtAddCallback(score_ck, XmNvalueChangedCallback, buttonCB, (XtPointer) 2);

   num_ck = XtVaCreateManagedWidget("Show number of cards in deck", xmToggleButtonWidgetClass, rowcol, 
      XmNindicatorType, XmONE_OF_MANY, 
      XmNset, iscountcrd, 
      NULL);
   XtAddCallback(num_ck, XmNvalueChangedCallback, buttonCB, (XtPointer) 3);

   XtVaCreateManagedWidget("separator", xmSeparatorWidgetClass, rowcol, NULL);

   drawone_wid = XtVaCreateManagedWidget("Draw One", xmToggleButtonWidgetClass, rowcol, 
      XmNindicatorType, XmONE_OF_MANY, 
      XmNset, drawone, 
      NULL);
   XtAddCallback(drawone_wid, XmNvalueChangedCallback, buttonCB, (XtPointer) 4);


   drawthree_wid = XtVaCreateManagedWidget("Draw Three", xmToggleButtonWidgetClass, rowcol, 
      XmNindicatorType, XmONE_OF_MANY, 
      XmNset, !drawone, 
      NULL);
   XtAddCallback(drawthree_wid, XmNvalueChangedCallback, buttonCB, (XtPointer) 5);

   ok = XtVaCreateManagedWidget("OK", xmPushButtonWidgetClass, dialog, 
      XmNtopAttachment, XmATTACH_WIDGET,
      XmNtopWidget, frame,
      XmNtopOffset, 10,
      XmNbottomAttachment, XmATTACH_FORM, 
      XmNbottomOffset, 10, 
      XmNleftAttachment, XmATTACH_POSITION, 
      XmNleftPosition, 20, 
      XmNrightAttachment, XmATTACH_POSITION, 
      XmNrightPosition, 80,  
      NULL);

   XtManageChild(dialog);
}
/*****************************************************************************/
void buttonCB(Widget parent, XtPointer client_data, XtPointer call_data) {

/* Callback for radio buttons in dialog box. Will set the options */
/* I need to modify this, make it neater */

   XmToggleButtonCallbackStruct * ptr;
   
   ptr = (XmToggleButtonCallbackStruct *) call_data;
   switch((uintptr_t) client_data) {
      case 1 : istimer = ptr->set; break;
      case 2 : isscore = ptr->set; break;
      case 3 : iscountcrd = ptr->set; break;
      case 4 : if(!ptr->set) {
                  XtVaSetValues(drawone_wid, XmNset, drawone, NULL); 
                  break;
               }
               drawone = True; 
               XtVaSetValues(drawthree_wid, XmNset, !drawone, NULL); 
               XtVaSetValues(drawone_wid, XmNset, drawone, NULL); 
               break;
      case 5 : if(!ptr->set) {
                  XtVaSetValues(drawthree_wid, XmNset, !drawone, NULL);
                  break;
               }
               drawone = False; 
               XtVaSetValues(drawone_wid, XmNset, drawone, NULL);
               XtVaSetValues(drawthree_wid, XmNset, !drawone, NULL);
               break;
      case 6 : XtUnmanageChild(parent);
   }
   UpdStatusBar();
}
/*****************************************************************************/
void undoCB(Widget widget, XtPointer client_data, XtPointer call_data) {  

/* Menu callback for File, Undo */

   struct undo_stk *temp_undo;
   struct stck *temp;
   int i;
   Display *display;
   Window window;
      
   display = XtDisplay(drawa);
   window = XtWindow(drawa);
   if(undo == NULL) return;
   temp_undo = undo;
   temp = piles[temp_undo->topile];
   for(i=0; i<undo->numcards; i++) {
      XCopyArea(display, temp->under, window, gc, 0, 0, CARD_WIDTH, CARD_HEIGHT, temp->xcoord, temp->ycoord);
      temp = temp->next;
   }                  
   temp = piles[temp_undo->topile];
   for(i=0; i<undo->numcards-1; i++) temp = temp->next;
   if(piles[temp_undo->topile] != NULL) piles[temp_undo->topile]->prev = NULL;
   if(piles[temp_undo->frompile] != NULL) piles[temp_undo->frompile]->prev = temp;
   piles[temp_undo->topile] = temp->next;
   for(i=0; i<undo->numcards; i++) {
      temp->xcoord = PILE_COL + (pile_distance*(temp_undo->frompile < 7 ? temp_undo->frompile : temp_undo->frompile < 9 ? temp_undo->frompile - 7 : temp_undo->frompile - 6));
      temp->ycoord = undo->ycoord + (temp_undo->frompile < 7 ? i*15 : 0);
      temp->shown = temp_undo->shown;
      XCopyArea(display, window, temp->under, gc, temp->xcoord, temp->ycoord, CARD_WIDTH, CARD_HEIGHT, 0, 0);
      if(temp->shown) XCopyArea(display, temp->face, window, gc, 0, 0, CARD_WIDTH, CARD_HEIGHT, temp->xcoord, temp->ycoord);
      else XCopyArea(display, cardbk, window, gc, 0, 0, CARD_WIDTH, CARD_HEIGHT, temp->xcoord, temp->ycoord);
      temp->next = piles[temp_undo->frompile];
      piles[temp_undo->frompile] = temp;
      temp = temp->prev;
      if(temp_undo->frompile == 7) numcards++;
      else if(temp_undo->topile == 7) numcards--;
   }
   if(temp_undo->score != 0) UpScore(temp_undo->score * -1);
   undo = undo->next;
   free(temp_undo);
   XtSetSensitive(undomenu, undo != NULL);
   UpdStatusBar();
}
/*****************************************************************************/
void newCB(Widget widget, XtPointer client_data, XtPointer call_data) {  

/* Menu callback for File, New, blah blah blah blah
 * Clears all the piles and undo stack */

   Display *display;
   Window window;
   struct stck *temp;
   struct undo_stk *temp_undo;
   int i;

   display = XtDisplay(drawa);
   window = XtWindow(drawa);   
   for(i=0; i<13; i++) {
      while(piles[i] != NULL) {
         temp = piles[i];
         XCopyArea(display, temp->under, window, gc, 0, 0, CARD_WIDTH, CARD_HEIGHT, temp->xcoord, temp->ycoord);
         piles[i] = piles[i]->next;
         free(temp);
      }
   }
   while(undo != NULL) {
      temp_undo = undo;
      undo = undo->next;
      free(temp_undo);
   }
   init_cards();
   DrawCards(display, window, 0, 0);
}
/*****************************************************************************/
void DrawCards(Display *display, Window window, int width, int height) {

/* This function will draw the cards onto the screen */

   int i;
   struct stck *temp;

   for(i=0; i<13; i++) { 
      temp = piles[i];
      if(piles[i] == NULL && i != 7 && i != 8 && gc != NULL) 
         XDrawRectangle(XtDisplay(drawa), XtWindow(drawa), gc, i < 7 ? PILE_COL + (pile_distance*i) : PILE_COL + (pile_distance*(i-6)), i < 7 ? PILE_ROW : DECK_ROW, CARD_WIDTH-1, CARD_HEIGHT-1);
      while(temp != NULL && temp->next != NULL) temp = temp->next;
      while(temp != NULL) {
         if(!inited) XCopyArea(display, window, temp->under, gc, temp->xcoord, temp->ycoord, CARD_WIDTH, CARD_HEIGHT, 0, 0);
         if(temp->shown) XCopyArea(display, temp->face, window, gc, 0, 0, CARD_WIDTH, CARD_HEIGHT, temp->xcoord, temp->ycoord);
         else XCopyArea(display, cardbk, window, gc, 0, 0, CARD_WIDTH, CARD_HEIGHT, temp->xcoord, temp->ycoord);
         temp = temp->prev;
      }
   }
   inited = 1;
   XFlush(display);
}
/*****************************************************************************/
void drawCB(Widget widget, XtPointer client_data, XtPointer call_data) {

/* This function gets called when the window needs redrawn */

   XmDrawingAreaCallbackStruct* ptr;
   Dimension width, height;
   int i;
   struct stck *temp;
    
   ptr = (XmDrawingAreaCallbackStruct*) call_data;
   if(ptr->reason == XmCR_EXPOSE && ptr->event->xexpose.count == 0) {
      XtVaGetValues(widget, XmNwidth,  &width, XmNheight, &height, NULL);
      DrawCards(XtDisplay(widget), XtWindow(widget), (int) width, (int) height);
   }
   else if(ptr->reason == XmCR_RESIZE) {
      XtVaGetValues(widget, XmNwidth,  &width, XmNheight, &height, NULL);
      if(width >= (CARD_WIDTH * 7) + PILE_COL) pile_distance = ((width - PILE_COL - (CARD_WIDTH * 7)) / 7) + CARD_WIDTH;
      else pile_distance = CARD_WIDTH;
      for(i=0; i<13; i++) { 
         temp = piles[i];
         while(temp != NULL) {
            temp->xcoord = PILE_COL + (pile_distance*(i < 7 ? i : i < 9 ? i - 7 : i - 6));
            temp = temp->next;
         }
      }
      if(gc != NULL) {
         XClearWindow(XtDisplay(widget), XtWindow(widget));
         for(i=0; i<13; i++) {
            if(i != 7 && i != 8) 
               XDrawRectangle(XtDisplay(widget), XtWindow(widget), gc, i < 7 ? PILE_COL + (pile_distance*i) : PILE_COL + (pile_distance*(i-6)), i < 7 ? PILE_ROW : DECK_ROW, CARD_WIDTH-1, CARD_HEIGHT-1);
         }
      }
      DrawCards(XtDisplay(widget), XtWindow(widget), (int) width, (int) height);
   }
}
/*****************************************************************************/
void mousepress(Widget widget, XtPointer client_data, XButtonEvent *event, Boolean *continue_to_dispatch) {

/* This function gets called when the user clicks the mouse button. If the 
 * user clicks it on a card, then the dragged variable gets set so indicating
 * that we are moving a card. This function also handles turning over cards,
 * and shuffling through the deck of cards */

   Display *display;
   Window window;
   struct stck *temp;
   struct undo_stk *temp_undo;
   int i, done, depth;
   
   XtVaGetValues(drawa, XmNdepth, &depth, NULL);
   display = XtDisplay(drawa);
   window = XtWindow(drawa);
   frompile = -1;
   if(PILE_COL <= event->x && PILE_COL+CARD_WIDTH >= event->x && DECK_ROW <= event->y && DECK_ROW+CARD_HEIGHT >= event->y && !(piles[7] == NULL && piles[8] == NULL)) {
      temp_undo = (struct undo_stk*)malloc(sizeof(struct undo_stk));
      if(temp_undo == NULL) {
         printf("Failed to alloc memory\n");
         exitCB(NULL, NULL, NULL);
      }
      temp_undo->next = undo;
      undo = temp_undo;
      temp_undo->score = 0;
      if(piles[7] != NULL) {
         temp_undo->xcoord = PILE_COL + (pile_distance*7);
         temp_undo->ycoord = DECK_ROW;
         temp_undo->frompile = 7;
         temp_undo->topile = 8;
         temp_undo->shown = 0;
         temp_undo->numcards = 0;
         for(i=0; i<(drawone ? 1 : 3); i++) {
            temp = piles[7];
            if(piles[7] == NULL) break;
            temp_undo->numcards++;
            numcards--;
            XCopyArea(display, temp->under, window, gc, 0, 0, CARD_WIDTH, CARD_HEIGHT, temp->xcoord, temp->ycoord); 
            piles[7] = piles[7]->next;
            if(piles[7] != NULL) piles[7]->prev = NULL;
            temp->next = piles[8];
            if(piles[8] != NULL) piles[8]->prev = temp;
            temp->xcoord += pile_distance + (30*i);
            temp->shown = 1;
            piles[8] = temp;      
            XCopyArea(display, window, temp->under, gc, temp->xcoord, temp->ycoord, CARD_WIDTH, CARD_HEIGHT, 0, 0); 
            XCopyArea(display, temp->face, window, gc, 0, 0, CARD_WIDTH, CARD_HEIGHT, temp->xcoord, temp->ycoord);
         }
      }
      else {
         temp_undo->xcoord = PILE_COL + (pile_distance*8);
         temp_undo->ycoord = DECK_ROW;
         temp_undo->frompile = 8;
         temp_undo->topile = 7;
         temp_undo->shown = 1;
         temp_undo->numcards = 0;
         while(piles[8] != NULL) {
            numcards++;
            temp = piles[8];
            XCopyArea(display, temp->under, window, gc, 0, 0, CARD_WIDTH, CARD_HEIGHT, temp->xcoord, temp->ycoord); 
            temp_undo->numcards++;
            piles[8] = piles[8]->next;
            if(piles[8] != NULL) piles[8]->prev = NULL;
            temp->next = piles[7];
            if(piles[7] != NULL) piles[7]->prev = temp;
            temp->xcoord = PILE_COL;
            temp->shown = 0;
            piles[7] = temp;      
            XCopyArea(display, window, temp->under, gc, temp->xcoord, temp->ycoord, CARD_WIDTH, CARD_HEIGHT, 0, 0); 
            XCopyArea(display, cardbk, window, gc, 0, 0, CARD_WIDTH, CARD_HEIGHT, temp->xcoord, temp->ycoord);
         }
         deckturn++;
         if(drawone && deckturn > 1) {
            UpScore(-100);
            temp_undo->score = -100;
         }
         else if(!drawone && deckturn > 3) {
            UpScore(-20);
            temp_undo->score = -20;
         }
      }   
      UpdStatusBar();
   }
   for(i=8; i<13; i++) {
      if(piles[i] != NULL && piles[i]->xcoord <= event->x && piles[i]->xcoord+CARD_WIDTH >= event->x && piles[i]->ycoord <= event->y && piles[i]->ycoord+CARD_HEIGHT >= event->y) {
         moved_card = piles[i];
         dragged = 1;
         ydraglen = CARD_HEIGHT;
         diffx = event->x - moved_card->xcoord;
         diffy = event->y - moved_card->ycoord;
         xcoord = moved_card->xcoord;
         ycoord = moved_card->ycoord;    
         frompile = i;
         face = XCreatePixmap(display, window, CARD_WIDTH, CARD_HEIGHT, depth);
         under = XCreatePixmap(display, window, CARD_WIDTH, CARD_HEIGHT, depth);              
         XCopyArea(display, moved_card->face, face, gc, 0, 0, CARD_WIDTH, CARD_HEIGHT, 0, 0);
         XCopyArea(display, moved_card->under, under, gc, 0, 0, CARD_WIDTH, CARD_HEIGHT, 0, 0);
      }
   }   
   for(i=0; i<7; i++) {
      if(PILE_COL + (pile_distance*i) <= event->x && PILE_COL + (pile_distance*i) + CARD_WIDTH >= event->x) {
         temp = piles[i];
         while(temp != NULL) {
            if(temp->ycoord <= event->y && temp->ycoord + CARD_HEIGHT >= event->y) {
               if(temp->shown) {
                  dragged = 1;
                  diffx = event->x - temp->xcoord;
                  diffy = event->y - temp->ycoord;
                  xcoord = temp->xcoord;
                  ycoord = temp->ycoord;    
                  frompile = i;
                  moved_card = temp;
                  while(temp->prev != NULL) temp = temp->prev;
                  ydraglen = CARD_HEIGHT-15;
                  done = 0;
                  while(!done) {
                     if(strcmp(temp->type, moved_card->type) == 0) done = 1;
                     ydraglen += 15;
                     XCopyArea(display, temp->under, window, gc, 0, 0, CARD_WIDTH, CARD_HEIGHT, temp->xcoord, temp->ycoord);
                     temp = temp->next;
                  }
                  face = XCreatePixmap(display, window, CARD_WIDTH, ydraglen, depth);
                  under = XCreatePixmap(display, window, CARD_WIDTH, ydraglen, depth);              
                  XCopyArea(display, window, under, gc, moved_card->xcoord, moved_card->ycoord, CARD_WIDTH, ydraglen, 0, 0);
                  temp = moved_card;
                  while(temp != NULL) {
                     XCopyArea(display, temp->face, window, gc, 0, 0, CARD_WIDTH, CARD_HEIGHT, temp->xcoord, temp->ycoord);
                     temp = temp->prev;
                  }
                  XCopyArea(display, window, face, gc, moved_card->xcoord, moved_card->ycoord, CARD_WIDTH, ydraglen, 0, 0);
                  break;
               }
               else if(!strcmp(piles[i]->type, temp->type)) {
                  temp_undo = (struct undo_stk*)malloc(sizeof(struct undo_stk));
                  if(temp_undo == NULL) {
                     printf("Failed to alloc memory\n");
                     exitCB(NULL, NULL, NULL);
                  }
                  temp_undo->next = undo;
                  undo = temp_undo;
                  temp_undo->xcoord = temp->xcoord;
                  temp_undo->ycoord = temp->ycoord;
                  temp_undo->frompile = i;
                  temp_undo->topile = i;
                  temp_undo->shown = 0;
                  temp_undo->numcards = 1;
                  temp_undo->score = 5;
                  UpScore(5);
                  temp->shown = 1;
                  XCopyArea(display, temp->face, window, gc, 0, 0, CARD_WIDTH, CARD_HEIGHT, temp->xcoord, temp->ycoord);
                  dragged = 0;
               }
            }
            temp = temp->next; 
         }
      }
   }
   XFlush(display);
}
/*****************************************************************************/
void mouserel(Widget widget, XtPointer client_data, XButtonEvent *event, Boolean *continue_to_dispatch) {

/* This function gets called when the user releases the mouse button. It will
 * handle moving the card to a separate pile if the user was dragging a card */

   struct stck *temp;
   struct undo_stk *temp_undo;
   int i, topile = -1, newy, yc;
   Display *display;
   Window window;

   display = XtDisplay(drawa);
   window = XtWindow(drawa);
   for(i=0; i<4; i++) {
      if( ( (PILE_COL + (pile_distance*(i+3)) <= event->x-diffx && PILE_COL + (pile_distance*(i+3)) + CARD_WIDTH >= event->x-diffx) ||
        (PILE_COL + (pile_distance*(i+3)) <= event->x-diffx+CARD_WIDTH && PILE_COL + (pile_distance*(i+3)) + CARD_WIDTH >= event->x-diffx+CARD_WIDTH)) &&
        ((DECK_ROW <= event->y-diffy && DECK_ROW + CARD_HEIGHT >= event->y-diffy) || (DECK_ROW <= event->y-diffy+CARD_HEIGHT && DECK_ROW + CARD_HEIGHT >= event->y-diffy+CARD_HEIGHT))) {
            if(event->x <= (PILE_COL + (pile_distance*(i+3)) + PILE_COL + (pile_distance*(i+4)) + CARD_WIDTH)/2) {
               topile = i+9;
               break;
            }        
            else topile = i+9;
      }
   }
   if(topile == -1) {
      for(i=0; i<7; i++) {
         yc = piles[i] == NULL ? PILE_ROW : piles[i]->ycoord;
         if(((PILE_COL + (pile_distance*i) <= event->x-diffx && PILE_COL + (pile_distance*i) + CARD_WIDTH >= event->x-diffx) ||
            (PILE_COL + (pile_distance*i) <= event->x-diffx+CARD_WIDTH && PILE_COL + (pile_distance*i) + CARD_WIDTH >= event->x-diffx+CARD_WIDTH)) &&
            ((yc <= event->y-diffy && yc + CARD_HEIGHT >= event->y-diffy) || (yc <= event->y-diffy+CARD_HEIGHT && yc + CARD_HEIGHT >= event->y-diffy+CARD_HEIGHT))) {
            if(event->x <= (PILE_COL + (pile_distance*i) + PILE_COL + (pile_distance*(i+1)) + CARD_WIDTH)/2) {
               topile = i;
               break;
            }        
            else topile = i;
         }
      }
   }
   if(dragged) {
      XCopyArea(display, under, window, gc, 0, 0, CARD_WIDTH, ydraglen, xcoord, ycoord);
      if((frompile == 8 ? 8 : topile) == frompile && event->time < (dclk_time + MOUSE_DCLK) && event->time > (dclk_time - MOUSE_DCLK)) {
         for(i=9; i<13; i++) {
            if(IsValidMove(frompile, moved_card->type, i, piles[i] == NULL ? (char*)"ZZ" : piles[i]->type)) {
               dclk_time = -1;
               topile = i;
               break;
            }
         }
      }
      else dclk_time = event->time;
   }  
   if(topile > 7 && dragged && frompile != -1 && strcmp(moved_card->type, piles[frompile]->type) == 0 && IsValidMove(frompile, moved_card->type, topile, piles[topile] == NULL ? (char*)"ZZ" : piles[topile]->type)) {
      temp_undo = (struct undo_stk*)malloc(sizeof(struct undo_stk));
      if(temp_undo == NULL) {
         printf("Failed to alloc memory\n");
         exitCB(NULL, NULL, NULL);
      }
      temp_undo->next = undo;
      undo = temp_undo;
      if(frompile < 9) {
         UpScore(10);
         temp_undo->score = 10;
      }
      else temp_undo->score = 0;
      temp = moved_card;
      temp_undo->xcoord = moved_card->xcoord;
      temp_undo->ycoord = moved_card->ycoord;
      temp_undo->frompile = frompile;
      temp_undo->topile = topile;
      temp_undo->shown = moved_card->shown;
      temp_undo->numcards = 1;
      piles[frompile] = piles[frompile]->next;
      if(piles[frompile] != NULL) piles[frompile]->prev = NULL;
      moved_card->next = piles[topile];
      moved_card->xcoord = PILE_COL + (pile_distance*(topile-6));
      moved_card->ycoord = DECK_ROW;
      XCopyArea(display, window, moved_card->under, gc, moved_card->xcoord, moved_card->ycoord, CARD_WIDTH, CARD_HEIGHT, 0, 0); 
      XCopyArea(display, moved_card->face, window, gc, 0, 0, CARD_WIDTH, CARD_HEIGHT, moved_card->xcoord, moved_card->ycoord);
      if(piles[topile] != NULL) piles[topile]->prev = moved_card;
      piles[topile] = moved_card;
   }
   if(topile != -1 && frompile != -1 && topile < 7 && frompile != topile && dragged && IsValidMove(frompile, moved_card->type, topile, piles[topile] == NULL ? (char*)"ZZ" : piles[topile]->type)) {
      temp = moved_card;
      temp_undo = (struct undo_stk*)malloc(sizeof(struct undo_stk));
      if(temp_undo == NULL) {
         printf("Failed to alloc memory\n");
         exitCB(NULL, NULL, NULL);
      }
      temp_undo->next = undo;
      temp_undo->xcoord = moved_card->xcoord;
      temp_undo->ycoord = moved_card->ycoord;
      temp_undo->frompile = frompile;
      temp_undo->topile = topile;
      temp_undo->shown = moved_card->shown;
      temp_undo->numcards = 0;
      undo = temp_undo;
      newy = piles[topile] == NULL ? PILE_ROW : piles[topile]->ycoord + 15;
      while(temp != NULL) {
         temp->xcoord = PILE_COL + (pile_distance*topile);
         temp->ycoord = newy;
         newy += 15;
         temp_undo->numcards++;
         XCopyArea(display, window, temp->under, gc, temp->xcoord, temp->ycoord, CARD_WIDTH, CARD_HEIGHT, 0, 0); 
         XCopyArea(display, temp->face, window, gc, 0, 0, CARD_WIDTH, CARD_HEIGHT, temp->xcoord, temp->ycoord);
         temp = temp->prev;
      }
      piles[frompile] = moved_card->next;;
      if(piles[frompile] != NULL) piles[frompile]->prev = NULL;
      moved_card->next = piles[topile];
      if(piles[topile] != NULL) piles[topile]->prev = moved_card;
      temp = moved_card;
      while(temp->prev != NULL) temp = temp->prev;
      piles[topile] = temp;
      if(frompile == 8) { 
         UpScore(5);
         temp_undo->score = 5;
      }
      else if(frompile > 8) {
         UpScore(-15);
         temp_undo->score = -15;
      }
      else temp_undo->score = 0;

   }      
   if(dragged) {
      XCopyArea(display, face, window, gc, 0, 0, CARD_WIDTH, ydraglen, moved_card->xcoord, moved_card->ycoord);
      XFreePixmap(display, face);
      XFreePixmap(display, under);
      dragged = 0;
   }
   XtSetSensitive(undomenu, undo != NULL);
   XFlush(display);
}
/*****************************************************************************/
void mousedrag(Widget widget, XtPointer client_data, XButtonEvent *event, Boolean *continue_to_dispatch) {

/* I really do need to do something with this function. This is the function
 * that gets called when the mouse is being dragged and if the mouse is on a
 * card, then it moves it */

   Display *display;
   Window window;
   int screen;

   display = XtDisplay(widget);
   window = XtWindow(widget);
   screen = DefaultScreen(display);
   if(dragged) {
      XCopyArea(display, under, window, gc, 0, 0, CARD_WIDTH, ydraglen, xcoord, ycoord);
      xcoord = event->x - diffx;
      ycoord = event->y - diffy;
      XCopyArea(display, window, under, gc, xcoord, ycoord, CARD_WIDTH, ydraglen, 0, 0); 
      XCopyArea(display, face, window, gc, 0, 0, CARD_WIDTH, ydraglen, xcoord, ycoord);
   }
   XFlush(display);
}
/*****************************************************************************/
Widget CreateMenu(Widget parent, char* name) {

/* Take a wild guess */

   Widget menu, cascade;
   Arg args[20];
   int n;

   n = 0;
   XtSetArg(args[n], XmNtearOffModel, XmTEAR_OFF_ENABLED); n++;
   menu = XmCreatePulldownMenu(parent, name, args, n);
   n = 0;
   XtSetArg(args[n], XmNsubMenuId, menu); n++;
   cascade = XmCreateCascadeButton(parent, name, args, n);
   XtManageChild(cascade);
   return menu;
}
/*****************************************************************************/
Widget CreateMenuItem(Widget parent, char *name, XtCallbackProc callback, XtPointer client_data) {

/* This is a tough one too */

   Widget push;
   
   push = XmCreatePushButton(parent, name, NULL, 0);
   XtAddCallback(push, XmNactivateCallback, callback, client_data);
   XtManageChild(push);
   return(push);
}
/*****************************************************************************/
void init_cards(void) {

/* Shuffles the cards, generates the card faces, and puts the cards in their
 * respective piles */

   Pixmap dummy;
   XColor xcolor;
   Colormap colormap;
   Dimension width;
   struct stck *temp;
   int i, j, newpos, num, fam, depth, status;
   char test[4];
   Display *display;
   Window window;
   char cards[52][3] = {"AH", "2H", "3H", "4H", "5H", "6H", "7H", "8H", "9H", "TH", "JH", "QH", "KH",
                        "AD", "2D", "3D", "4D", "5D", "6D", "7D", "8D", "9D", "TD", "JD", "QD", "KD",
                        "AS", "2S", "3S", "4S", "5S", "6S", "7S", "8S", "9S", "TS", "JS", "QS", "KS",
                        "AC", "2C", "3C", "4C", "5C", "6C", "7C", "8C", "9C", "TC", "JC", "QC", "KC"};
   int card_pattern[7][8] = {{1,-1,-1,-1,-1,-1,-1,-1},
                             {0,1,-1,-1,-1,-1,-1,-1},
                             {0,0,1,-1,-1,-1,-1,-1},
                             {0,0,0,1,-1,-1,-1,-1},
                             {0,0,0,0,1,-1,-1,-1},
                             {0,0,0,0,0,1,-1,-1},
                             {0,0,0,0,0,0,1,-1}};

   inited = 0;
   score = 0;
   deckturn = 0;
   time_elapsed = 0;
   numcards = 52;
   undo = NULL;
   XtVaGetValues(drawa, XmNwidth,  &width, XmNdepth, &depth, NULL);
   if(width >= (CARD_WIDTH * 7) + PILE_COL) pile_distance = ((width - PILE_COL - (CARD_WIDTH * 7)) / 7) + CARD_WIDTH;
   else pile_distance = CARD_WIDTH;
   display = XtDisplay(drawa);
   window = XtWindow(drawa);
   colormap = DefaultColormap(display, DefaultScreen(display));
   xcolor.flags = DoRed | DoGreen | DoBlue;
   xcolor.red = 65535;
   xcolor.green = 0;
   xcolor.blue = 0;
   status = XAllocColor(display, colormap, &xcolor);
   XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
   XSetBackground(display, gc, WhitePixel(display, DefaultScreen(display)));
   for(i=0; i<13; i++) {
      piles[i] = NULL;
      if(i != 7 && i != 8) 
         XDrawRectangle(display, window, gc, i < 7 ? PILE_COL + (pile_distance*i) : PILE_COL + (pile_distance*(i-6)), i < 7 ? PILE_ROW : DECK_ROW, CARD_WIDTH-1, CARD_HEIGHT-1);
   }
   cardbk = XCreatePixmap(display, window, CARD_WIDTH, CARD_HEIGHT, depth);
   dummy = XCreateBitmapFromData(display, window, (char *)card_back, CARD_WIDTH, CARD_HEIGHT);
   XCopyPlane(display, dummy, cardbk , gc, 0, 0, CARD_WIDTH, CARD_HEIGHT, 0, 0, 0x01);

   srand(time(NULL));
   for(i=0; i<52; i++) {
      newpos = 1+(int) (51.0*rand()/(RAND_MAX+1.0));;
      strcpy(test,cards[i]);
      strcpy(cards[i],cards[newpos]);
      strcpy(cards[newpos],test);
   }
   for(i=0; i<52; i++) {
      if(!(temp = (struct stck*)malloc(sizeof(struct stck)))) {
         printf("Failed to alloc memory\n");
         exitCB(NULL, NULL, NULL);
      }
      temp->xcoord = PILE_COL;
      temp->ycoord = DECK_ROW;
      temp->shown = 0;
      strcpy(temp->type, cards[i]);
      temp->face = XCreatePixmap(display, window, CARD_WIDTH, CARD_HEIGHT, depth);
      temp->under = XCreatePixmap(display, window, CARD_WIDTH, CARD_HEIGHT, depth);
      XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
      XFillRectangle(display, temp->face, gc, 0, 0, CARD_WIDTH, CARD_HEIGHT);
      XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
      XDrawRectangle(display, temp->face, gc, 0, 0, CARD_WIDTH-1, CARD_HEIGHT-1);
      if(temp->type[1] == 'S' || temp->type[1] == 'C') XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
      else XSetForeground(display, gc, xcolor.pixel);
      switch(temp->type[0]) {
         case 'T' : num = 9; break;
         case 'J' : num = 10; break;
         case 'Q' : num = 11; break;
         case 'K' : num = 12; break;
         case 'A' : num = 13; break;
         default :
            sprintf(test, "%c", temp->type[0]);
            num = atoi(test)-2;
            break;
      }
      switch(temp->type[1]) {
         case 'C' : fam = 0; break;
         case 'S' : fam = 1; break;
         case 'D' : fam = 2; break;
         case 'H' : fam = 3; break;
      }
      dummy = XCreateBitmapFromData(display, window, (char*)card_nums[num], 15, 15);
      XCopyPlane(display, dummy, temp->face , gc, 0, 0, 15, 15, 1, 1, 0x01);
      XCopyPlane(display, dummy, temp->face, gc, 0, 0, 15, 15, CARD_WIDTH-31, CARD_HEIGHT-16, 0x01);
      dummy = XCreateBitmapFromData(display, window, (char*)card_family[fam], 15, 15);
      XCopyPlane(display, dummy, temp->face, gc, 0, 0, 15, 15, 15, 1, 0x01);
      XCopyPlane(display, dummy, temp->face, gc, 0, 0, 15, 15, CARD_WIDTH-16, CARD_HEIGHT-16, 0x01);
      temp->next = piles[7];
      temp->prev = NULL;
      if(piles[7] != NULL) piles[7]->prev = temp;
      else temp->next = NULL;
      piles[7] = temp;
   }

   for(i=0; i<7; i++) {
      j=0;
      while(card_pattern[i][j] != -1) {
         numcards--;
         piles[7]->xcoord = PILE_COL + (pile_distance*i);
         piles[7]->ycoord = PILE_ROW + (15*j);
         temp = piles[7];
         piles[7] = piles[7]->next;
         piles[7]->prev = NULL;
         temp->next = piles[i];
         temp->prev = NULL;
         temp->shown = card_pattern[i][j];
         if(piles[i] != NULL) piles[i]->prev = temp;
         else temp->next = NULL;
         piles[i] = temp;
         j++;
      }
   }
   XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
   XtSetSensitive(undomenu, undo != NULL);
   UpdStatusBar();
}
/*****************************************************************************/
int IsValidMove(int frompile, char fromcard[3], int topile, char tocard[3]) {

/* Gets called after each move to see if it is valid
 * Note: 'Z' is my dummy NULL or empty pile */

   if(frompile == topile) return(0);
   if(topile > 8) {
      if(fromcard[0] == 'A' && tocard[0] == 'Z') return(1);
      else if(fromcard[1] == tocard[1]) {
         if(CardOrder(tocard[0]) == fromcard[0]) return(1);
         else return(0);
      }
      else return(0);
   }
   else {
      if(fromcard[0] == 'K' && tocard[0] == 'Z') return(1);
      if(CardOrder(fromcard[0]) == tocard[0]) {
         if( (fromcard[1] == 'S' || fromcard[1] == 'C') && (tocard[1] == 'H' || tocard[1] == 'D')) return(1);
         else if( (fromcard[1] == 'H' || fromcard[1] == 'D') && (tocard[1] == 'S' || tocard[1] == 'C')) return(1);
         else return(0);
      }
      else return(0);
   }
}
/*****************************************************************************/
char CardOrder(char face) {

/* What would the next card be? for example after 2, comes 3. After ten,
 * comes the jack, etc, etc, etc */

   if(face == 'Z') return('A');
   else if(face == 'A') return('2');
   else if(face >= '2' && face <= '8') return(face+1);
   else if(face == '9') return('T');
   else if(face == 'T') return('J');
   else if(face == 'J') return('Q');
   else if(face == 'Q') return('K');
   else return(-1); /* Shouldn't happen */
}
/*****************************************************************************/
void UpScore(int num) {

/* Increments the score and updates the display. I need to make a status bar
 * on the main window */

   score += num;
   UpdStatusBar();
}   
/*****************************************************************************/
void UpdStatusBar(void) {
   char temp[80] = "";
   char temp1[40];
   
   if(istimer) {
      sprintf(temp1, "Time: %ld ", time_elapsed);
      strcat(temp, temp1);
   }
   if(isscore) {
      sprintf(temp1, "Score: %d ", score);
      strcat(temp, temp1);
   }
   if(iscountcrd) {
      sprintf(temp1, "Cards In Deck: %d", numcards);
      strcat(temp, temp1);
   }
   newscore = XmStringCreateSimple(temp);
   XtVaSetValues(score_widget, XmNlabelString, newscore, NULL);
}
/*****************************************************************************/
