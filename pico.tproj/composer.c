#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: composer.c,v 1.2 2002/01/03 22:16:39 jevans Exp $";
#endif
/*
 * Program:	Pine composer routines
 *
 *
 * Michael Seibel
 * Networks and Distributed Computing
 * Computing and Communications
 * University of Washington
 * Administration Builiding, AG-44
 * Seattle, Washington, 98195, USA
 * Internet: mikes@cac.washington.edu
 *
 * Please address all bugs and comments to "pine-bugs@cac.washington.edu"
 *
 * Copyright 1991-1994  University of Washington
 *
 *  Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee to the University of
 * Washington is hereby granted, provided that the above copyright notice
 * appears in all copies and that both the above copyright notice and this
 * permission notice appear in supporting documentation, and that the name
 * of the University of Washington not be used in advertising or publicity
 * pertaining to distribution of the software without specific, written
 * prior permission.  This software is made available "as is", and
 * THE UNIVERSITY OF WASHINGTON DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT LIMITATION ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, AND IN
 * NO EVENT SHALL THE UNIVERSITY OF WASHINGTON BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, TORT
 * (INCLUDING NEGLIGENCE) OR STRICT LIABILITY, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pine and Pico are trademarks of the University of Washington.
 * No commercial use of these trademarks may be made without prior
 * written permission of the University of Washington.
 *
 *
 * NOTES:
 *
 *  - composer.c is the composer for the PINE mail system
 *
 *  - tabled 01/19/90
 *
 *  Notes: These routines aren't incorporated yet, because the composer as
 *         a whole still needs development.  These are ideas that should
 *         be implemented in later releases of PINE.  See the notes in 
 *         pico.c concerning what needs to be done ....
 *
 *  - untabled 01/30/90
 *
 *  Notes: Installed header line editing, with wrapping and unwrapping, 
 *         context sensitive help, and other mail header editing features.
 *
 *  - finalish code cleanup 07/15/91
 * 
 *  Notes: Killed/yanked header lines use emacs kill buffer.
 *         Arbitrarily large headers are handled gracefully.
 *         All formatting handled by FormatLines.
 *
 *  - Work done to optimize display painting 06/26/92
 *         Not as clean as it should be, needs more thought 
 *
 */
#include <stdio.h>
#include <ctype.h>
#include "osdep.h"
#include "pico.h"
#include "estruct.h"
#include "edef.h"
#include "efunc.h"


#ifdef	ANSI
    int InitEntryText(char *, struct headerentry *);
    int HeaderOffset(int);
    int HeaderFocus(int, int);
    int LineEdit(int);
    int header_downline(int);
    int header_upline(int);
    int FormatLines(struct hdr_line *, char *, int, int, int);
    char *strqchr(char *, int, int *);
    int PaintBody(int);
    int ComposerHelp(int);
    int NewTop(void);
    void display_delimiter(int);
    int InvertPrompt(int, int);
    int partial_entries(void);
    int physical_line(struct hdr_line *);
    int strend(char *, int);
    int KillHeaderLine(struct hdr_line *, int);
    int SaveHeaderLines(void);
    char *break_point(char *, int, int, int *);
    int hldelete(struct hdr_line *);
    int is_blank(int, int, int);
    int zotentry(struct hdr_line *);
    void zotcomma(char *);
    struct hdr_line *first_hline(int *);
    struct hdr_line *next_hline(int *, struct hdr_line *);
    struct hdr_line *prev_hline(int *, struct hdr_line *);
#else
    int InitEntryText();
    int HeaderOffset();
    int HeaderFocus();
    int LineEdit();
    int header_downline();
    int header_upline();
    int FormatLines();
    char *strqchr();
    int PaintBody();
    int ComposerHelp();
    int NewTop();
    void display_delimiter();
    int InvertPrompt();
    int partial_entries();
    int physical_line();
    int strend();
    int KillHeaderLine();
    int SaveHeaderLines();
    char *break_point();
    int hldelete();
    int is_blank();
    int zotentry();
    void zotcomma();
    struct hdr_line *first_hline();
    struct hdr_line *next_hline();
    struct hdr_line *prev_hline();
#endif


/*
 * definition header field array, structures defined in pico.h
 */
struct headerentry *headents;


/*
 * structure that keeps track of the range of header lines that are
 * to be displayed and other fun stuff about the header
 */
struct on_display ods;				/* global on_display struct */


/*
 * useful macros
 */
#define	HALLOC()	(struct hdr_line *)malloc(sizeof(struct hdr_line))
#define	LINELEN()	(term.t_ncol - headents[ods.cur_e].prlen)
#define	BOTTOM()	(term.t_nrow - 2)
#define	FULL_SCR()	(term.t_nrow-5)
#define	HALF_SCR()	(FULL_SCR()/2)


/*
 * useful declarations
 */
static int     last_key;			/* last keystroke  */


static KEYMENU menu_header[] = {
    {"^G", "Get Help"},		{"^X", "Send"}, 	{"^R", "Rich Hdr"},
    {"^Y", "PrvPg/Top"},	{"^K", "Cut Line"},	{"^O", "Postpone"},
    {"^C", "Cancel"},		{"^D", "Del Char"},	{"^J", "Attach"},
    {"^V", "NxtPg/End"},	{"^U", "UnDel Line"},	{NULL, NULL}
};
#define	TO_KEY		11


/*
 * function key mappings for header editor
 */
static int ckm[12][2] = {
    { F1,  (CTRL|'G')},
    { F2,  (CTRL|'C')},
    { F3,  (CTRL|'X')},
    { F4,  (CTRL|'D')},
    { F5,  (CTRL|'R')},
    { F6,  (CTRL|'J')},
    { F7,  0 },
    { F8,  0 },
    { F9,  (CTRL|'K')},
    { F10, (CTRL|'U')},
    { F11, (CTRL|'O')},
    { F12, (CTRL|'T')}
};


/*
 * InitMailHeader - initialize header array, and set beginning editor row 
 *                  range.  The header entry structure should look just like 
 *                  what is written on the screen, the vector 
 *                  (entry, line, offset) will describe the current cursor 
 *                  position in the header.
 */
InitMailHeader(mp)
PICO  *mp;
{
    char *addrbuf;
    struct headerentry *he;

    /*
     * initialize some of on_display structure, others below...
     */
    ods.p_off  = 0;
    ods.p_line = COMPOSER_TOP_LINE;
    ods.top_l = ods.cur_l = NULL;

    headents = mp->headents;
    /*--- initialize the fields in the headerent structure ----*/
    for(he = headents; he->name != NULL; he++){
	he->hd_text    = NULL;
	he->display_it = he->display_it ? he->display_it : !he->rich_header;
        if(he->is_attach) {
            /*--- A lot of work to do since attachments are special ---*/
            he->maxlen = 0;
	    if(mp->attachments != NULL){
		char   buf[NLINE];
                int    x = 0;
                PATMT *ap = mp->attachments;
 
                addrbuf = (char *)malloc((size_t)1024);
                addrbuf[0] = '\0';
                buf[0] = '\0';
                while(ap){
                 if(ap->filename){
                     sprintf(buf, "%d. %s %s%s%s\"%s\"%s",
                             ++x,
                             ap->filename,
                             ap->size ? "(" : "",
                             ap->size ? ap->size : "",
                             ap->size ? ") " : "",
                             ap->description ? ap->description : "", 
                             ap->next ? "," : "");
                     strcat(addrbuf, buf);
                 }
                 ap = ap->next;
                }
                InitEntryText(addrbuf, he);
                free((char *)addrbuf);
            } else {
                InitEntryText("", he);
            }
            he->realaddr = NULL;
        } else {
            addrbuf = *(he->realaddr);
            InitEntryText(addrbuf, he);
	}
    }

    /*
     * finish initialization and then figure out display layout...
     */
    ods.top_l = ods.cur_l = first_hline(&ods.cur_e);
    ods.top_e = ods.cur_e;
    UpdateHeader();
}



/*
 * InitEntryText - Add the given header text into the header entry 
 *		   line structure.
 */
InitEntryText(address, e)
char	*address;
struct headerentry *e;
{
    struct  hdr_line	*curline;
    register  int	longest;

    /*
     * get first chunk of memory, and tie it to structure...
     */
    if((curline = HALLOC()) == NULL){
        emlwrite("Unable to make room for full Header.", NULL);
        return(FALSE);
    }
    longest = term.t_ncol - e->prlen - 1;
    curline->text[0] = '\0';
    curline->next = NULL;
    curline->prev = NULL;
    e->hd_text = curline;		/* tie it into the list */

    if(FormatLines(curline, address, longest, e->break_on_comma, 0) == -1)
      return(FALSE);
    else
      return(TRUE);
}



/*
 *  ResizeHeader - Handle resizing display when SIGWINCH received.
 *
 *	notes:
 *		works OK, but needs thorough testing
 *		  
 */
ResizeHeader()
{
    register struct headerentry *i;
    register int offset;

    offset = (ComposerEditing) ? HeaderOffset(ods.cur_e) : 0;

    for(i=headents; i->name; i++){		/* format each entry */
	if(FormatLines(i->hd_text, "", (term.t_ncol - i->prlen),
		       i->break_on_comma, 0) == -1){
	    return(-1);
	}
    }

    if(ComposerEditing)				/* restart at the top */
      HeaderFocus(ods.cur_e, offset);		/* fix cur_l and p_off */
    else {
      for(i = headents; i->name != NULL; i++);  /* Find last line */
      i--;
      HeaderFocus(i - headents, -1);		/* put focus on last line */
    }

    if(ComposerTopLine != COMPOSER_TOP_LINE)
      UpdateHeader();

    PaintBody(0);

    if(ComposerEditing)
      movecursor(ods.p_line, ods.p_off+headents[ods.cur_e].prlen);

    (*term.t_flush)();
    return(TRUE);
}



/*
 * HeaderOffset - return the character offset into the given header
 */
HeaderOffset(h)
int	h;
{
    register struct hdr_line *l;
    int	     i = 0;

    l = headents[h].hd_text;

    while(l != ods.cur_l){
	i += strlen(l->text);
	l = l->next;
    }
    return(i+ods.p_off);
}



/*
 * HeaderFocus - put the dot at the given offset into the given header
 */
HeaderFocus(h, offset)
int	h, offset;
{
    register struct hdr_line *l;
    register int    i;
    int	     last = 0;

    if(offset == -1)				/* focus on last line */
      last = 1;

    l = headents[h].hd_text;
    while(1){
	if(last && l->next == NULL){
	    break;
	}
	else{
	    if((i=strlen(l->text)) >= offset)
	      break;
	    else
	      offset -= i;
	}
	if((l = l->next) == NULL)
	  return(FALSE);
    }

    ods.cur_l = l;
    ods.p_len = strlen(l->text);
    ods.p_off = (last) ? 0 : offset;

    return(TRUE);
}



/*
 * HeaderEditor() - edit the mail header field by field, trapping 
 *                  important key sequences, hand the hard work off
 *                  to LineEdit().  
 *	returns:
 *              -2    if we drop out bottom *and* want to page forward
 *		-1    if we drop out the bottom 
 *		FALSE if editing is cancelled
 *		TRUE  if editing is finished
 */
HeaderEditor(f, n)
int f, n;
{
    register  int	i;
    register  int	ch;
    register  int	status;			/* return status of something*/
    register  char	*bufp;
    struct headerentry *h;
    int                 cur_e, count, retval = -1;
    char               *errmss;

    ComposerEditing = TRUE;
    display_delimiter(0);			/* provide feedback */

    /* 
     * Decide where to begin editing.  if f == TRUE begin editing
     * at the bottom.  this case results from the cursor backing
     * into the editor from the bottom.  otherwise, the user explicitly
     * requested editing the header, and we begin at the top.
     * 
     * further, if f == 1, we moved into the header by hitting the up arrow
     * in the message text, else if f == 2, we moved into the header by
     * moving past the left edge of the top line in the message.  so, make 
     * the end of the last line of the last entry the current cursor position
     * lastly, if f == 3, we moved into the header by hitting backpage() on
     * the top line of the message, so scroll a page back.  
     */
    if(f){
	if(f == 2){				/* 2 leaves cursor at end  */
	    struct hdr_line *l = ods.cur_l;
	    int              e = ods.cur_e;

	    /*--- make sure on last field ---*/
	    while(l = next_hline(&e, l))
	      if(headents[ods.cur_e].display_it){
		  ods.cur_l = l;
		  ods.cur_e = e;
	      }

	    ods.p_off = 1000;			/* and make sure at EOL    */
	}
	else{
	    /*
	     * note: assumes that ods.cur_e and ods.cur_l haven't changed
	     *       since we left...
	     */

	    /* fix postition */
	    if(curwp->w_doto < headents[ods.cur_e].prlen)
	      ods.p_off = 0;
	    else if(curwp->w_doto < ods.p_off + headents[ods.cur_e].prlen)
	      ods.p_off = curwp->w_doto - headents[ods.cur_e].prlen;
	    else
	      ods.p_off = 1000;

	    /* and scroll back if needed */
	    if(f == 3)
	      for(i = 0; header_upline(0) && i <= FULL_SCR(); i++)
		;
	}

	ods.p_line = ComposerTopLine - 2;
    }
    else{		/* use offset 0 of first line of first entry */
	ods.p_line = COMPOSER_TOP_LINE;
	ods.cur_e = ods.top_e;
        ods.cur_l = ods.top_l;
        ods.p_off = 0;
    }

    InvertPrompt(ods.cur_e, TRUE);		/* highlight header field */
    sgarbk = 1;

    do{
	if(sgarbk){
	    ShowPrompt();			/* display correct options */
	    sgarbk = 0;
	}

	ch = LineEdit(TRUE);			/* work on the current line */

        switch (ch){
	  case (CTRL|'R') :			/* Toggle header display */
            /*---- Are there any headers to expand above us? ---*/
            for(h = headents; h != &headents[ods.cur_e]; h++)
              if(h->rich_header)
                break;
            if(h->rich_header)
	      InvertPrompt(ods.cur_e, FALSE);	/* Yes, don't leave inverted */

	    if(partial_entries()){
                /*--- Just turned off all rich headers --*/
		if(headents[ods.cur_e].rich_header){
                    /*-- current header got turned off too --*/
                    /* Check below */
                    for(cur_e =ods.cur_e; headents[cur_e].name!=NULL; cur_e++)
                      if(!headents[cur_e].rich_header)
                        break;
                    if(headents[cur_e].name == NULL) {
                        /* didn't find one, check above */
                        for(cur_e =ods.cur_e; headents[cur_e].name!=NULL;
                            cur_e--)
                          if(!headents[cur_e].rich_header)
                            break;

                    }
		    ods.p_off = 0;
		    ods.cur_e = cur_e;
		    ods.cur_l = headents[ods.cur_e].hd_text;
		}
	    }

	    ods.p_line = 0;			/* force update */
	    UpdateHeader();
	    PaintHeader(COMPOSER_TOP_LINE, FALSE);
	    PaintBody(1);
	    break;

	  case (CTRL|'C') :			/* bag whole thing ?*/
	    if(abort_composer(1, 0) == TRUE)
	      return(FALSE);

	    break;

	  case (CTRL|'X') :			/* Done. Send it. */
	    i = 0;
#ifdef	ATTACHMENTS
	    if(headents[ods.cur_e].is_attach){
		/* verify the attachments, and pretty things up in case
		 * we come back to the composer due to error...
		 */
		if((i = SyncAttach()) != 0){
		    sleep(2);		/* give time for error to absorb */
		    FormatLines(headents[ods.cur_e].hd_text, "",
				term.t_ncol - headents[ods.cur_e].prlen,
				headents[ods.cur_e].break_on_comma, 0);
		}
	    }
	    else
#endif
	    if(headents[ods.cur_e].builder)	/* verify text? */
	      i = call_builder(&headents[ods.cur_e]) > 0;

	    if(wquit(1,0) == TRUE)
	      return(TRUE);

	    if(i){
		/*
		 * need to be careful here because pointers might be messed up.
		 * also, could do a better job of finding the right place to
		 * put the dot back (i.e., the addr/list that was expanded).
		 */
		ods.cur_l = headents[ods.cur_e].hd_text; /* attach cur_l */
		ods.p_off = 0;
		ods.p_line = 0;			/* force realignment */
		UpdateHeader();
		PaintHeader(COMPOSER_TOP_LINE, FALSE);
		PaintBody(1);
	    }
	    break;
	  case (CTRL|'Z') :			/* Suspend compose */
	    if(gmode&MDSSPD){			/* is it allowed? */
		bktoshell();
		PaintBody(0);
	    }
	    else{
		(*term.t_beep)();
		emlwrite("Unknown Command: ^Z", NULL);
	    }
	    break;

	  case (CTRL|'O') :			/* Suspend message */
	    if(headents[ods.cur_e].is_attach){
		if(SyncAttach() < 0){
		    if(mlyesno("Problem with attachments. Postpone anyway?",
			       FALSE) != TRUE){
			if(FormatLines(headents[ods.cur_e].hd_text, "",
				       term.t_ncol - headents[ods.cur_e].prlen,
				       headents[ods.cur_e].break_on_comma,
				       0) == -1)
			  emlwrite("\007Format lines failed!", NULL);
			UpdateHeader();
			PaintHeader(COMPOSER_TOP_LINE, FALSE);
			PaintBody(1);
			continue;
		    }
		}
	    }
	    else if(headents[ods.cur_e].builder)
	      call_builder(&headents[ods.cur_e]);

	    suspend_composer(1, 0);
	    return(TRUE);

#ifdef	ATTACHMENTS
	  case (CTRL|'J') :			/* handle attachments */
	    { char fn[NLINE], sz[32], cmt[NLINE];

	      if(AskAttach(fn, sz, cmt)){
		  int	 a_e;
		  struct hdr_line *lp;

                  /*--- Find headerentry that is attachments (only first) --*/
                  for(a_e = 0; headents[a_e].name != NULL; a_e++ )
                    if(headents[a_e].is_attach){
			/* make sure field stays displayed */
			headents[a_e].rich_header = 0;
			headents[a_e].display_it = 1;
			break;
		    }

		  /* append new attachment line */
		  for(lp = headents[a_e].hd_text; lp->next; lp=lp->next)
		    ;

		  /* build new attachment line */
		  if(lp->text[0]){		/* adding a line? */
		      strcat(lp->text, ",");	/* append delimiter */
		      if(lp->next = HALLOC()){	/* allocate new line */
			  lp->next->prev = lp;
			  lp->next->next = NULL;
			  lp = lp->next;
		      }
		      else{
			  emlwrite(
				 "\007Can't allocate line for new attachment!",
				 NULL);
			  break;
		      }
		  }

		  sprintf(lp->text, "%s (%s) \"%.*s\"", fn, sz, 80, cmt);

		  /* validate the new attachment, and reformat if needed */
		  if(status = SyncAttach()){
		      if(status < 0)
			emlwrite("\007Problem attaching: %s", fn);

		      if(FormatLines(headents[a_e].hd_text, "",
				     term.t_ncol - headents[a_e].prlen,
				     headents[a_e].break_on_comma, 0) == -1){
			  emlwrite("\007Format lines failed!", NULL);
			  break;
		      }
		  }

		  UpdateHeader();
		  PaintHeader(COMPOSER_TOP_LINE, status != 0);
		  PaintBody(1);
	      }

	      sgarbk = 1;			/* clean up prompt */
	    }
	    break;
#endif

	  case (CTRL|'I') :			/* tab */
	    ods.p_off = 0;			/* fall through... */

	  case (CTRL|'N') :
	  case K_PAD_DOWN :
	    header_downline(1);
	    break;

	  case (CTRL|'P') :
	  case K_PAD_UP :
	    header_upline(1);
	    break;

	  case (CTRL|'V') :			/* down a page */
	    cur_e = ods.cur_e;
	    if(!next_hline(&cur_e, ods.cur_l)){
		header_downline(1); 		/* cause us to return */
		retval = -2;
	    }
	    else
	      for(i = 0; i <= FULL_SCR() && header_downline(0); i++)
		;

	    break;

	  case (CTRL|'Y') :			/* up a page */
	    for(i = 0; header_upline(0) && i <= FULL_SCR(); i++)
	      if(i < 0)
		break;

	    break;

	  case (CTRL|'T') :			/* Call field selector */
            if(headents[ods.cur_e].is_attach) {
                /*--- selector for attachments ----*/
		char dir[NLINE], fn[NLINE], sz[NLINE];

		strcpy(dir, gmode&MDCURDIR ? "." : gethomedir(NULL));
		fn[0] = '\0';
		if(FileBrowse(dir, fn, sz, FB_READ) == 1){ /* got a new file */
		    char buf[NLINE];
		    sprintf(buf, "%s%c%s (%s) \"\"%s", dir, C_FILESEP, fn, sz, 
			    (!headents[ods.cur_e].hd_text->text[0]) ? "":",");
		    if(FormatLines(headents[ods.cur_e].hd_text, buf,
				   term.t_ncol - headents[ods.cur_e].prlen,
				   headents[ods.cur_e].break_on_comma,0)==-1){
			emlwrite("\007Format lines failed!", NULL);
		    }

		    UpdateHeader();
		}				/* else, nothing of interest */
            } else if (headents[ods.cur_e].selector != NULL) {
                /*---- General selector for non-attachments -----*/
                errmss = NULL;
                bufp = (*(headents[ods.cur_e].selector))(&errmss);
                if(bufp != NULL) {
                    if(headents[ods.cur_e].break_on_comma) {
                        /*--- Must be an address ---*/
                        if(ods.cur_l->text[0] != '\0'){
			    for(i = ++ods.p_len; i; i--)
			      ods.cur_l->text[i] = ods.cur_l->text[i-1];

			    ods.cur_l->text[0] = ',';
			}

                        if(FormatLines(ods.cur_l, bufp,
				      (term.t_ncol-headents[ods.cur_e].prlen), 
                                      headents[ods.cur_e].break_on_comma,
				      0) == -1){
                            emlwrite("Problem adding address to header !",
                                     NULL);
                            (*term.t_beep)();
                            break;
                        }

    		        UpdateHeader();
			free(bufp);
                    } else {
                        strcpy(headents[ods.cur_e].hd_text->text, bufp);
                    }
    	            PaintBody(0); /* Repaint entire screen */
		} else {
    	            PaintBody(0); /* Repaint entire screen,then error msg */
                    if(errmss != NULL) {
                        (*term.t_beep)();
	                emlwrite(errmss, NULL);
                    }
                }
	    } else {
                /*----- No selector -----*/
		(*term.t_beep)();
		continue;
	    }
	    PaintBody(0);
	    continue;

	  case (CTRL|'G'):			/* HELP */
	    ComposerHelp(ods.cur_e);		/* fall through... */

	  case (CTRL|'L'):			/* redraw requested */
	    PaintBody(0);
	    break;

	  default :				/* huh? */
	    if(ch&CTRL)
	      emlwrite("\007Unknown command: ^%c", (void *)(ch&0xff));
	    else
	  case BADESC:
	      emlwrite("\007Unknown command", NULL);

	  case NODATA:
	    break;
	}
    }
    while (ods.p_line < ComposerTopLine);

    display_delimiter(1);
    curwp->w_flag |= WFMODE;
    movecursor(currow, curcol);
    ComposerEditing = FALSE;
    return(retval);
}


/*
 *
 */
int
header_downline(beyond)
     int beyond;
{
    struct hdr_line *new_l;
    int    new_e, status, fullpaint, len;

    /* calculate the next line: physical *and* logical */
    status    = 0;
    new_e     = ods.cur_e;
    if((new_l = next_hline(&new_e, ods.cur_l)) == NULL && !beyond)
      return(0);

    fullpaint = ++ods.p_line >= BOTTOM();	/* force full redraw?       */

    /* expand what needs expanding */
    if(new_e != ods.cur_e || !new_l){		/* new (or last) field !    */
	if(new_l)
	  InvertPrompt(ods.cur_e, FALSE);	/* turn off current entry   */

	if(headents[ods.cur_e].is_attach) {	/* varify data ?	    */
	    if(status = SyncAttach()){		/* fixup if 1 or -1	    */
		headents[ods.cur_e].rich_header = 0;
		if(FormatLines(headents[ods.cur_e].hd_text, "",
			       term.t_ncol-headents[new_e].prlen,
			       headents[ods.cur_e].break_on_comma, 0) == -1)
		  emlwrite("\007Format lines failed!", NULL);
	    }
	} else if(headents[ods.cur_e].builder) { /* expand addresses	    */
	    if(status = (call_builder(&headents[ods.cur_e]) > 0)){
		struct hdr_line *l;		/* fixup ods.cur_l */
		ods.p_line = 0;			/* force top line recalc */
		for(l = headents[ods.cur_e].hd_text; l; l = l->next)
		  ods.cur_l = l;

		NewTop();			/* get new top_l */
	    }
	}

	if(new_l){				/* if one below, turn it on */
	    InvertPrompt(new_e, TRUE);
	    sgarbk = 1;				/* paint keymenu too	    */
	}
    }

    if(new_l){					/* fixup new pointers	    */
	ods.cur_l = (ods.cur_e != new_e) ? headents[new_e].hd_text : new_l;
	ods.cur_e = new_e;
	if(ods.p_off > (len = strlen(ods.cur_l->text)))
	  ods.p_off = len;
    }

    if(!new_l || status || fullpaint){		/* handle big screen paint  */
	UpdateHeader();
	PaintHeader(COMPOSER_TOP_LINE, FALSE);
	PaintBody(1);

	if(!new_l){				/* make sure we're done     */
	    ods.p_line = ComposerTopLine;
	    InvertPrompt(ods.cur_e, FALSE);	/* turn off current entry   */
	}
    }

    return(new_l ? 1 : 0);
}


/*
 *
 */
int
header_upline(gripe)
    int gripe;
{
    struct hdr_line *new_l;
    int    new_e, status, fullpaint, len;

    /* calculate the next line: physical *and* logical */
    status    = 0;
    fullpaint = ods.p_line-- == COMPOSER_TOP_LINE;
    new_e     = ods.cur_e;
    if(!(new_l = prev_hline(&new_e, ods.cur_l))){	/* all the way up! */
	ods.p_line = COMPOSER_TOP_LINE;
	if(gripe)
	  emlwrite("Can't move beyond top of header", NULL);

	return(0);
    }

    if(new_e != ods.cur_e){			/* new field ! */
	InvertPrompt(ods.cur_e, FALSE);
	if(headents[ods.cur_e].is_attach){
	    if(status = SyncAttach()){		/* non-zero ? reformat field */
		headents[ods.cur_e].rich_header = 0;
		if(FormatLines(headents[ods.cur_e].hd_text, "",
			       term.t_ncol - headents[ods.cur_e].prlen,
			       headents[ods.cur_e].break_on_comma,0) == -1)
		  emlwrite("\007Format lines failed!", NULL);
	    }
	}
	else if(headents[ods.cur_e].builder){
	    status = call_builder(&headents[ods.cur_e]) > 0;
	}

	InvertPrompt(new_e, TRUE);
	sgarbk = 1;
    }

    ods.cur_e = new_e;				/* update pointers */
    ods.cur_l = new_l;
    if(ods.p_off > (len = strlen(ods.cur_l->text)))
      ods.p_off = len;

    if(status || fullpaint){
	UpdateHeader();
	PaintHeader(COMPOSER_TOP_LINE, FALSE);
	PaintBody(1);
    }

    return(1);
}



/*
 * LineEdit - the idea is to manage 7 bit ascii character only input.
 *            Always use insert mode and handle line wrapping
 *
 *	returns:
 *		Any characters typed in that aren't printable 
 *		(i.e. commands)
 *
 *	notes: 
 *		Assume we are guaranteed that there is sufficiently 
 *		more buffer space in a line than screen width (just one 
 *		less thing to worry about).  If you want to change this,
 *		then pputc will have to be taught to check the line buffer
 *		length, and HALLOC() will probably have to become a func.
 */
LineEdit(allowedit)
int	allowedit;
{
    register struct	hdr_line   *lp;		/* temporary line pointer    */
    register int	i;
    register int	ch = 0;
    register int	status;			/* various func's return val */
    register char	*tbufp;			/* temporary buffer pointers */
	     int	j;
	     int	skipmove = 0;
             char	*strng;

    strng   = ods.cur_l->text;			/* initialize offsets */
    ods.p_len = strlen(strng);
    if(ods.p_off < 0)				/* offset within range? */
      ods.p_off = 0;
    else if(ods.p_off > ods.p_len)
      ods.p_off = ods.p_len;
    else if(ods.p_off > LINELEN())		/* shouldn't happen, but */
        ods.p_off = LINELEN();			/* you never know...     */

    while(1){					/* edit the line... */

	if(skipmove)
	  skipmove = 0;
	else
	  movecursor(ods.p_line, ods.p_off+headents[ods.cur_e].prlen);

	last_key = ch;

	(*term.t_flush)();			/* get everything out */

        ch = GetKey();

	if(ch == NODATA || time_to_check()){	/* new mail ? */
	    if((*Pmaster->newmail)(&j, 0, ch == NODATA ? 0 : 2) >= 0){
		mlerase();
		(*Pmaster->showmsg)(ch);
		mpresf = 1;
	    }

	    if(j || mpresf){
		(*term.t_move)(ods.p_line,ods.p_off+headents[ods.cur_e].prlen);
		(Pmaster->clearcur)();
	    }

	    if(ch == NODATA)			/* GetKey timed out */
	      continue;
	}

        if(mpresf){				/* blast old messages */
	    if(mpresf++ > NMMESSDELAY){		/* every few keystrokes */
		mlerase();
		movecursor(ods.p_line, ods.p_off+headents[ods.cur_e].prlen);
	    }
        }


        if(ch > 0x1f && ch < 0x7f){		/* char input */
            /*
             * if we are allowing editing, insert the new char
             * end up leaving tbufp pointing to newly
             * inserted character in string, and offset to the
             * index of the character after the inserted ch ...
             */
            if(allowedit){
		if(headents[ods.cur_e].only_file_chars && !fallowc((char)ch)){
		    /* no garbage in filenames */
		    emlwrite("\007Can't have a '%c' in folder name",(void *)ch);
		    continue;
		}
		else if(headents[ods.cur_e].is_attach && intag(strng,ods.p_off)){
		    emlwrite("\007Can't edit attachment number!", NULL);
		    continue;
		}

		if(headents[ods.cur_e].single_space){
		    if(ch == ' ' 
		       && (strng[ods.p_off]==' ' || strng[ods.p_off-1]==' '))
		      continue;
		}

		/*
		 * go ahead and add the character...
		 */
		tbufp = &strng[++ods.p_len];	/* find the end */
		do{
		    *tbufp = tbufp[-1];
		} while(--tbufp > &strng[ods.p_off]);	/* shift right */
		strng[ods.p_off++] = ch;	/* add char to str */

		/* make this entry sticky */
		headents[ods.cur_e].sticky = 1;

		/*
		 * then find out where things fit...
		 */
		if(ods.p_len < LINELEN()){
		    CELL c;

		    c.a = 0;
		    c.c = ch;
		    if(pinsert(c)){		/* add char to str */
			skipmove++;		/* must'a been optimal */
			continue; 		/* on to the next! */
		    }
		}
		else{
                    if((status = FormatLines(ods.cur_l, "", LINELEN(), 
    			        headents[ods.cur_e].break_on_comma,0)) == -1){
                        (*term.t_beep)();
                        continue;
                    }
                    else{
			/*
			 * during the format, the dot may have moved
			 * down to the next line...
			 */
			if(ods.p_off >= strlen(strng)){
			    ods.p_line++;
			    ods.p_off -= strlen(strng);
			    ods.cur_l = ods.cur_l->next;
			    strng = ods.cur_l->text;
			}
			ods.p_len = strlen(strng);
		    }
		    UpdateHeader();
		    PaintHeader(COMPOSER_TOP_LINE, FALSE);
		    PaintBody(1);
                    continue;
		}
            }
            else{  
                (*term.t_beep)();
                continue;
            } 
        }
        else {					/* interpret ch as a command */
            switch (ch = normal(ch, ckm, 2)) {
	      case (CTRL|'@') :		/* word skip */
		while(strng[ods.p_off] && !isspace(strng[ods.p_off]))
		  ods.p_off++;		/* skip any text we're in */

		while(strng[ods.p_off] && isspace(strng[ods.p_off]))
		  ods.p_off++;		/* skip any whitespace after it */

		if(strng[ods.p_off] == '\0'){
		    ods.p_off = 0;	/* end of line, let caller handle it */
		    return(K_PAD_DOWN);
		}

		continue;

	      case (CTRL|'K') :			/* kill line cursor's on */
		lp = ods.cur_l;
		ods.p_off = 0;

		if(ods.cur_l->next != NULL && ods.cur_l->prev != NULL)
		  ods.cur_l = next_hline(&ods.cur_e, ods.cur_l);
		else if(ods.cur_l->prev != NULL)
		  ods.cur_l = prev_hline(&ods.cur_e, ods.cur_l);

		if(KillHeaderLine(lp, (last_key == (CTRL|'K')))){
		    if(optimize && 
		       !(ods.cur_l->prev==NULL && ods.cur_l->next==NULL))
		      scrollup(wheadp, ods.p_line, 1);

		    if(ods.cur_l->next == NULL)
		      zotcomma(ods.cur_l->text);
		    
		    i = (ods.p_line == COMPOSER_TOP_LINE);
		    UpdateHeader();
		    PaintHeader(i ? COMPOSER_TOP_LINE: ods.p_line, FALSE);
		    PaintBody(1);
		}
		strng = ods.cur_l->text;
		ods.p_len = strlen(strng);
		headents[ods.cur_e].sticky = 1;
		continue;

	      case (CTRL|'U') :			/* un-delete deleted lines */
		if(SaveHeaderLines()){
		    UpdateHeader();
		    PaintHeader(COMPOSER_TOP_LINE, FALSE);
		    PaintBody(1);

		    ods.p_off = 0;		/* dot hasn't moved! */
		    strng = ods.cur_l->text;
		    ods.p_len = strlen(strng);
		    headents[ods.cur_e].sticky = 1;
		}
		else
		  emlwrite("Problem Unkilling text", NULL);
		continue;

	      case (CTRL|'F') :
	      case K_PAD_RIGHT:			/* move character right */
		if(ods.p_off < ods.p_len){
		    pputc(pscr(ods.p_line, 
			       (ods.p_off++)+headents[ods.cur_e].prlen)->c,0);
		    skipmove++;
		    continue;
		}
		ods.p_off = 0;
		return(K_PAD_DOWN);

	      case (CTRL|'B') :
	      case K_PAD_LEFT	:		/* move character left */
		if(ods.p_off > 0){
		    ods.p_off--;
		    continue;
		}
		if(ods.p_line != COMPOSER_TOP_LINE)
		  ods.p_off = 1000;		/* put cursor at end of line */
		return(K_PAD_UP);

	      case (CTRL|'M') :			/* goto next field */
		ods.p_off = 0;
		return(K_PAD_DOWN);

	      case K_PAD_HOME :
	      case (CTRL|'A') :			/* goto beginning of line */
		ods.p_off = 0;
		continue;

	      case K_PAD_END  :
	      case (CTRL|'E') :			/* goto end of line */
		ods.p_off = ods.p_len;
		continue;

	      case (CTRL|'D')   :		/* blast this char */
	      case K_PAD_DELETE :
		if(!allowedit){
		    (*term.t_beep)();
		    continue;
		}
		else if(ods.p_off >= strlen(strng))
		  continue;

		if(headents[ods.cur_e].is_attach && intag(strng, ods.p_off)){
		    emlwrite("\007Can't edit attachment number!", NULL);
		    continue;
		}

		headents[ods.cur_e].sticky = 1;
		pputc(strng[ods.p_off++], 0); 	/* drop through and rubout */

	      case 0x7f       :			/* blast previous char */
	      case (CTRL|'H') :
		if(!allowedit){
		    (*term.t_beep)();
		    continue;
		}

		if(headents[ods.cur_e].is_attach && intag(strng, ods.p_off-1)){
		    emlwrite("\007Can't edit attachment number!", NULL);
		    continue;
		}

		if(ods.p_off > 0){		/* just shift left one char */
		    ods.p_len--;
		    tbufp = &strng[--ods.p_off];
		    while(*tbufp++ != '\0')
		      tbufp[-1] = *tbufp;
		    tbufp = &strng[ods.p_off];
		    if(pdel())			/* physical screen delete */
		      skipmove++;		/* must'a been optimal */
		}
		else{				/* may have work to do */
		    if(ods.cur_l->prev == NULL){  
			(*term.t_beep)();	/* no erase into next field */
			continue;
		    }

		    ods.p_line--;
		    ods.cur_l = ods.cur_l->prev;
		    strng = ods.cur_l->text;
		    if((i=strlen(strng)) > 0){
			strng[i-1] = '\0';	/* erase the character */
			ods.p_off = i-1;
		    }
		    else
		      ods.p_off = 0;
		    
		    tbufp = &strng[ods.p_off];
		}

		if((status = FormatLines(ods.cur_l, "", LINELEN(), 
				   headents[ods.cur_e].break_on_comma,0))==-1){
		    (*term.t_beep)();
		    continue;
		}
		else{
		    /*
		     * beware, the dot may have moved...
		     */
		    while((ods.p_len=strlen(strng)) < ods.p_off){
			ods.p_line++;
			ods.p_off -= strlen(strng);
			ods.cur_l = ods.cur_l->next;
			strng = ods.cur_l->text;
			ods.p_len = strlen(strng);
			tbufp = &strng[ods.p_off];
			status = TRUE;
		    }
		    UpdateHeader();
		    PaintHeader(COMPOSER_TOP_LINE, FALSE);
		    if(status == TRUE)
		      PaintBody(1);
		}

		movecursor(ods.p_line, ods.p_off+headents[ods.cur_e].prlen);

		headents[ods.cur_e].sticky = 1;

		if(skipmove)
		  continue;

		break;

              default   :
		return(ch);
            }
        }

	while (*tbufp != '\0')		/* synchronizing loop */
	  pputc(*tbufp++, 0);

	if(ods.p_len < LINELEN())
	  peeol();

    }
}



/*
 * FormatLines - Place the given text at the front of the given line->text
 *               making sure to properly format the line, then check
 *               all lines below for proper format.
 *
 *	notes:
 *		Not much optimization at all.  Right now, it recursively
 *		fixes all remaining lines in the entry.  Some speed might
 *		gained if this was built to iteratively scan the lines.
 *
 *	returns:
 *		-1 on error
 *		FALSE if only this line is changed
 *		TRUE  if text below the first line is changed
 */
FormatLines(h, instr, maxlen, break_on_comma, quoted)
struct  hdr_line  *h;				/* where to begin formatting */
char	*instr;					/* input string */
int	maxlen;					/* max chars on a line */
int	break_on_comma;				/* break lines on commas */
int	quoted;					/* this line inside quotes */
{
    int		retval = FALSE;
    register	int	i, l;
    char	*ostr;				/* pointer to output string */
    register	char	*breakp;		/* pointer to line break */
    register	char	*bp, *tp;		/* temporary pointers */
    char	*buf;				/* string to add later */
    struct hdr_line	*nlp, *lp;

    ostr = h->text;
    nlp = h->next;
    l = strlen(instr) + strlen(ostr);
    if((buf = (char *)malloc(l+10)) == NULL)
      return(-1);

    if(l >= maxlen){				/* break then fixup below */
	if(strlen(instr) < maxlen){		/* room for more */

	    if(break_on_comma && (bp = (char *)strqchr(instr, ',', &quoted))){
		bp += (bp[1] == ' ') ? 2 : 1;
		for(tp = bp; *tp && *tp == ' '; tp++)
		  ;

		strcpy(buf, tp);
		strcat(buf, ostr);
		for(i = 0; &instr[i] < bp; i++)
		  ostr[i] = instr[i];
		ostr[i] = '\0';
		retval = TRUE;
	    }
	    else{
		breakp = break_point(ostr, maxlen-strlen(instr),
				     break_on_comma ? ',' : ' ',
				     break_on_comma ? &quoted : NULL);

		if(breakp == ostr){	/* no good breakpoint */
		    if(break_on_comma && *breakp == ','){
			breakp = ostr + 1;
			retval = TRUE;
		    }
		    else if(strchr(instr,(break_on_comma && !quoted)?',':' ')){
			strcpy(buf, ostr);
			strcpy(ostr, instr);
		    }
		    else{		/* instr's broken as we can get it */
			breakp = &ostr[maxlen-strlen(instr)-1];
			retval = TRUE;
		    }
		}
		else
		  retval = TRUE;
	    
		if(retval){
		    strcpy(buf, breakp);	/* save broken line  */
		    if(breakp == ostr){
			strcpy(ostr, instr);	/* simple if no break */
		    }
		    else{
			*breakp = '\0';		/* more work to break it */
			i = strlen(instr);
			/*
			 * shift ostr i chars
			 */
			for(bp=breakp; bp >= ostr && i; bp--)
			  *(bp+i) = *bp;
			for(tp=ostr, bp=instr; *bp != '\0'; tp++, bp++)
			  *tp = *bp;		/* then add instr */
		    }
		}
	    }
	}
	else{					/* instr > maxlen ! */
	    if(break_on_comma){
		breakp = (!(bp = strqchr(instr, ',', &quoted))
			  || bp - instr >= maxlen)
			   ? &instr[maxlen]
			   : bp + ((bp[1] == ' ') ? 2 : 1);
	    }
	    else{
		breakp = break_point(instr, maxlen, ' ', NULL);

		if(breakp == instr)		/* no good break point */
		  breakp = &instr[maxlen - 1];
	    }
	    
	    strcpy(buf, breakp);		/* save broken line */
	    strcat(buf, ostr);			/* add line that was there */
	    for(tp=ostr,bp=instr; bp < breakp; tp++, bp++)
	      *tp = *bp;

	    *tp = '\0';
	}

	if(nlp == NULL){			/* no place to add below? */
	    if((lp = HALLOC()) == NULL){
		emlwrite("Can't allocate any more lines for header!", NULL);
		free(buf);
		return(-1);
	    }

	    if(optimize && (i = physical_line(h)) != -1)
	      scrolldown(wheadp, i - 1, 1);

	    h->next = lp;			/* fix up links */
	    lp->prev = h;
	    lp->next = NULL;
	    lp->text[0] = '\0';
	    nlp = lp;
	    retval = TRUE;
	}
	else
	    retval = FALSE;
    }
    else{					/* combined length < max */
	if(*instr){
	    strcpy(buf, instr);			/* insert instr before ostr */
	    strcat(buf, ostr);
	    strcpy(ostr, buf);
	}

	*buf = '\0';
	breakp = NULL;

	if(break_on_comma && (breakp = strqchr(ostr, ',', &quoted))){
	    breakp += (breakp[1] == ' ') ? 2 : 1;
	    strcpy(buf, breakp);
	    *breakp = '\0';

	    if(strlen(buf) && !nlp){
		if((lp = HALLOC()) == NULL){
		    emlwrite("Can't allocate any more lines for header!",NULL);
		    free(buf);
		    return(-1);
		}

		if(optimize && (i = physical_line(h)) != -1)
		  scrolldown(wheadp, i - 1, 1);

		h->next = lp;		/* fix up links */
		lp->prev = h;
		lp->next = NULL;
		lp->text[0] = '\0';
		nlp = lp;
		retval = TRUE;
	    }
	}

	if(nlp){
	    if(!strlen(buf) && !breakp){
		if(strlen(ostr) + strlen(nlp->text) >= maxlen){
		    breakp = break_point(nlp->text, maxlen-strlen(ostr), 
					 break_on_comma ? ',' : ' ',
					 break_on_comma ? &quoted : NULL);
		    
		    if(breakp == nlp->text){	/* commas this line? */
			for(tp=ostr; *tp  && *tp != ' '; tp++)
			  ;

			if(!*tp){		/* no commas, get next best */
			    breakp += maxlen - strlen(ostr) - 1;
			    retval = TRUE;
			}
			else
			  retval = FALSE;
		    }
		    else
		      retval = TRUE;

		    if(retval){			/* only if something to do */
			for(tp = &ostr[strlen(ostr)],bp=nlp->text; bp<breakp; 
			tp++, bp++)
			  *tp = *bp;		/* add breakp to this line */
			*tp = '\0';
			for(tp=nlp->text, bp=breakp; *bp != '\0'; tp++, bp++)
			  *tp = *bp;		/* shift next line to left */
			*tp = '\0';
		    }
		}
		else{
		    strcat(ostr, nlp->text);

		    if(optimize && (i = physical_line(nlp)) != -1)
		      scrollup(wheadp, i, 1);

		    hldelete(nlp);

		    if(!(nlp = h->next)){
			free(buf);
			return(TRUE);		/* can't go further */
		    }
		    else
		      retval = TRUE;		/* more work to do? */
		}
	    }
	}
	else{
	    free(buf);
	    return(FALSE);
	}

    }

    i = FormatLines(nlp, buf, maxlen, break_on_comma, quoted);
    free(buf);
    switch(i){
      case -1:					/* bubble up worst case */
	return(-1);
      case FALSE:
	if(retval == FALSE)
	  return(FALSE);
      default:
	return(TRUE);
    }
}



/*
 * PaintHeader - do the work of displaying the header from the given 
 *               physical screen line the end of the header.
 *
 *       17 July 91 - fixed reshow to deal with arbitrarily large headers.
 */
void
PaintHeader(line, clear)
    int	line;					/* physical line on screen   */
    int	clear;					/* clear before painting */
{
    register struct hdr_line	*lp;
    register char	*bufp;
    register int	curline;
    register int	curoffset;
    char     buf[NLINE];
    int      e;

    if(clear)
      pclear(COMPOSER_TOP_LINE, ComposerTopLine);

    curline   = COMPOSER_TOP_LINE;
    curoffset = 0;

    for(lp = ods.top_l, e = ods.top_e; ; curline++){
	if((curline == line) || ((lp = next_hline(&e, lp)) == NULL))
	  break;
    }

    while(headents[e].name != NULL){			/* begin to redraw */
	while(lp != NULL){
	    buf[0] = '\0';
            if((!lp->prev || curline == COMPOSER_TOP_LINE) && !curoffset){
	        if(InvertPrompt(e, (e == ods.cur_e && ComposerEditing)) == -1
		   && !is_blank(curline, 0, headents[e].prlen))
		   sprintf(buf, "          ");
	    }
	    else if(!is_blank(curline, 0, headents[e].prlen))
	      sprintf(buf, "          ");

	    if(*(bufp = buf) != '\0'){		/* need to paint? */
		movecursor(curline, 0);		/* paint the line... */
		while(*bufp != '\0')
		  pputc(*bufp++, 0);
	    }

	    bufp = &(lp->text[curoffset]);	/* skip chars already there */
	    curoffset += headents[e].prlen;
	    while(*bufp == pscr(curline, curoffset)->c && *bufp != '\0'){
		bufp++;
		if(++curoffset >= term.t_ncol)
		  break;
	    }

	    if(*bufp != '\0'){			/* need to move? */
		movecursor(curline, curoffset);
		while(*bufp != '\0'){		/* display what's not there */
		    pputc(*bufp++, 0);
		    curoffset++;
		}
	    }

	    if(curoffset < term.t_ncol 
	       && !is_blank(curline, curoffset, term.t_ncol - curoffset)){
		movecursor(curline, curoffset);
		peeol();
	    }
	    curline++;

            curoffset = 0;
	    if(curline >= BOTTOM())
	      break;

	    lp = lp->next;
        }

	if(curline >= BOTTOM())
	  return;				/* don't paint delimiter */

	while(headents[++e].name != NULL)
	  if(headents[e].display_it){
	      lp = headents[e].hd_text;
	      break;
	  }
    }

    display_delimiter(ComposerEditing ? 0 : 1);
}




/*
 * PaintBody() - generic call to handle repainting everything BUT the 
 *		 header
 *
 *	notes:
 *		The header redrawing in a level 0 body paint gets done
 *		in update()
 */
PaintBody(level)
int	level;
{
    curwp->w_flag |= WFHARD;			/* make sure framing's right */
    if(level == 0)				/* specify what to update */
        sgarbf = TRUE;

    update();					/* display message body */

    if(level == 0 && ComposerEditing){
	mlerase();				/* clear the error line */
	ShowPrompt();
    }
}



/*
 * ArrangeHeader - set up display parm such that header is reasonably 
 *                 displayed
 */
ArrangeHeader()
{
    int      e;
    register struct hdr_line *l;

    ods.p_line = ods.p_off = 0;
    e = ods.top_e = 0;
    l = ods.top_l = headents[e].hd_text;
    while(headents[e+1].name || (l && l->next))
      if(l = next_hline(&e, l)){
	  ods.cur_l = l;
	  ods.cur_e = e;
      }

    UpdateHeader();
}


/*
 * ComposerHelp() - display mail help in a context sensitive way
 *                  based on the level passed ...
 */
ComposerHelp(level)
int	level;
{
    char buf[80];

    curwp->w_flag |= WFMODE;
    sgarbf = TRUE;

    if(level < 0 || !headents[level].name){
	(*term.t_beep)();
	emlwrite("Sorry, I can't help you with that.", NULL);
	sleep(2);
	return(FALSE);
    }

    sprintf(buf, "Help for Composer %.40s Field", headents[level].name);
    (*Pmaster->helper)(headents[level].help, buf, 1);
}



/*
 * ToggleHeader() - set or unset pico values to the full screen size
 *                  painting header if need be.
 */
ToggleHeader(show)
int show;
{
    /*
     * check to see if we need to display the header... 
     */
    if(show){
	UpdateHeader();				/* figure bounds  */
	PaintHeader(COMPOSER_TOP_LINE, FALSE);	/* draw it */
    }
    else{
        /*
         * set bounds for no header display
         */
        curwp->w_toprow = ComposerTopLine = COMPOSER_TOP_LINE;
        curwp->w_ntrows = BOTTOM() - ComposerTopLine;
    }
    return(TRUE);
}



/*
 * HeaderLen() - return the length in lines of the exposed portion of the
 *               header
 */
HeaderLen()
{
    register struct hdr_line *lp;
    int      e;
    int      i;
    
    i = 1;
    lp = ods.top_l;
    e  = ods.top_e;
    while(lp != NULL){
	lp = next_hline(&e, lp);
	i++;
    }
    return(i);
}



/*
 * first_hline() - return a pointer to the first displayable header line
 * 
 *	returns:
 *		1) pointer to first displayable line in header and header
 *                 entry, via side effect, that the first line is a part of
 *              2) NULL if no next line, leaving entry at LASTHDR
 */
struct hdr_line *
first_hline(entry)
    int *entry;
{
    /* init *entry so we're sure to start from the top */
    for(*entry = 0; headents[*entry].name; (*entry)++)
      if(headents[*entry].display_it)
	return(headents[*entry].hd_text);

    *entry = 0;
    return(NULL);		/* this shouldn't happen */
}



/*
 * next_hline() - return a pointer to the next line structure
 * 
 *	returns:
 *		1) pointer to next displayable line in header and header
 *                 entry, via side effect, that the next line is a part of
 *              2) NULL if no next line, leaving entry at LASTHDR
 */
struct hdr_line *
next_hline(entry, line)
    int *entry;
    struct hdr_line *line;
{
    if(line == NULL)
      return(NULL);

    if(line->next == NULL){
	while(headents[++(*entry)].name != NULL){
	    if(headents[*entry].display_it)
	      return(headents[*entry].hd_text);
	}
	--(*entry);
	return(NULL);
    }
    else
      return(line->next);
}



/*
 * prev_hline() - return a pointer to the next line structure back
 * 
 *	returns:
 *              1) pointer to previous displayable line in header and 
 *                 the header entry that the next line is a part of 
 *                 via side effect
 *              2) NULL if we can't go back further
 */
struct hdr_line *
prev_hline(entry, line)
    int *entry;
    struct hdr_line *line;
{
    if(line == NULL)
      return(NULL);

    if(line->prev == NULL){
	while(--(*entry) >= 0){
	    if(headents[*entry].display_it){
		line = headents[*entry].hd_text;
		while(line->next != NULL)
		  line = line->next;
		return(line);
	    }
	}
	++(*entry);
	return(NULL);
    }
    else
      return(line->prev);
}



/*
 * UpdateHeader() - determines the best range of lines to be displayed 
 *                  using the global ods value for the current line and the
 *		    top line, also sets ComposerTopLine and pico limits
 *                    
 *      notes:
 *	        This is pretty ugly because it has to keep the current line
 *		on the screen in a reasonable location no matter what.
 *		There are also a couple of rules to follow:
 *                 1) follow paging conventions of pico (ie, half page 
 *		      scroll)
 *                 2) if more than one page, always display last half when 
 *                    pline is toward the end of the header
 * 
 *      returns:
 *             TRUE  if anything changed (side effects: new p_line, top_l
 *		     top_e, and pico parms)
 *             FALSE if nothing changed 
 *             
 */
UpdateHeader()
{
    register struct	hdr_line	*lp;
    int	     i, le;
    int      ret = FALSE;
    int      old_top = ComposerTopLine;
    int      old_p = ods.p_line;

    if(ods.p_line < COMPOSER_TOP_LINE || ods.p_line >= BOTTOM()){
	NewTop();				/* get new top_l */
	ret = TRUE;
    }
    else{					/* make sure p_line's OK */
	i = COMPOSER_TOP_LINE;
	lp = ods.top_l;
	le = ods.top_e;
	while(lp != ods.cur_l){
	    /*
	     * this checks to make sure cur_l is below top_l and that
	     * cur_l is on the screen...
	     */
	    if((lp = next_hline(&le, lp)) == NULL || ++i >= BOTTOM()){
		NewTop();
		ret = TRUE;
		break;
	    }
	}
    }

    ods.p_line = COMPOSER_TOP_LINE;		/* find  p_line... */
    lp = ods.top_l;
    le = ods.top_e;
    while(lp && lp != ods.cur_l){
	lp = next_hline(&le, lp);
	ods.p_line++;
    }

    if(!ret)
      ret = !(ods.p_line == old_p);

    ComposerTopLine = ods.p_line;		/* figure top composer line */
    while(lp && ComposerTopLine <= BOTTOM()){
	lp = next_hline(&le, lp);
	ComposerTopLine += (lp) ? 1 : 2;	/* allow for delim at end   */
    }

    if(!ret)
      ret = !(ComposerTopLine == old_top);

    if(wheadp->w_toprow != ComposerTopLine){	/* update pico params... */
        wheadp->w_toprow = ComposerTopLine;
        wheadp->w_ntrows = ((i = BOTTOM() - ComposerTopLine) > 0) ? i : 0;
	ret = TRUE;
    }
    return(ret);
}



/*
 * NewTop() - calculate a new top_l based on the cur_l
 *
 *	returns:
 *		with ods.top_l and top_e pointing at a reasonable line
 *		entry
 */
NewTop()
{
    register struct hdr_line *lp;
    register int i;
    int      e;

    lp = ods.cur_l;
    e  = ods.cur_e;
    i  = HALF_SCR();

    while(lp != NULL && i--){
	ods.top_l = lp;
	ods.top_e = e;
	lp = prev_hline(&e, lp);
    }
}



/*
 * display_delimiter() - just paint the header/message body delimiter with
 *                       inverse value specified by state.
 */
void
display_delimiter(state)
int	state;
{
    register char    *bufp;
    static   short   ps   = 0;			/* previous state */

    if(ComposerTopLine - 1 >= BOTTOM())		/* silently forget it */
      return;

    bufp = "----- Message Text -----";

    if(state == ps){				/* optimize ? */
	for(ps = 0; bufp[ps] && pscr(ComposerTopLine-1,ps)->c == bufp[ps];ps++)
	  ;

	if(bufp[ps] == '\0'){
	    ps = state;
	    return;				/* already displayed! */
	}
    }

    ps = state;

    movecursor(ComposerTopLine - 1, 0);
    if(state)
      (*term.t_rev)(1);

    while(*bufp != '\0')
      pputc(*bufp++, 0);

    if(state)
      (*term.t_rev)(0);

    peeol();
}



/*
 * InvertPrompt() - invert the prompt associated with header entry to state
 *                  state (true if invert, false otherwise).
 *	returns:
 *		non-zero if nothing done
 *		0 if prompt inverted successfully
 *
 *	notes:
 *		come to think of it, this func and the one above could
 *		easily be combined
 */
InvertPrompt(entry, state)
int	entry, state;
{
    register char   *bufp;
    register int    i;
    static   short  ps = 0; 			/* prev state of entry e */

    bufp = headents[entry].prompt;		/* fresh prompt paint */
    if((i = entry_line(entry, FALSE)) == -1)
      return(-1);				/* silently forget it */

    if((ps&(1<<entry)) == (state ? 1<<entry : 0)){	/* optimize ? */
	int j;

	for(j = 0; bufp[j] && pscr(i, j)->c == bufp[j]; j++)
	  ;

	if(bufp[j] == '\0'){
	    if(state)
	      ps |= 1<<entry;
	    else
	      ps &= ~(1<<entry);
	    return(0);				/* already displayed! */
	}
    }

    if(state)
      ps |= 1<<entry;
    else
      ps &= ~(1<<entry);

    movecursor(i, 0);
    if(state)
      (*term.t_rev)(1);

    while(*bufp && *(bufp + 1))
      pputc(*bufp++, 1);			/* putc upto last char */

    if(state)
      (*term.t_rev)(0);

    pputc(*bufp, 0);				/* last char not inverted */
    return(TRUE);
}




/*
 * partial_entries() - toggle display of the bcc and fcc fields.
 *
 *	returns:
 *		TRUE if there are partial entries on the display
 *		FALSE otherwise.
 */
partial_entries()
{
    register struct headerentry *h;
    int                          is_on;
  
    /*---- find out status of first rich header ---*/
    for(h = headents; !h->rich_header && h->name != NULL; h++)
      ;

    is_on = h->display_it;
    for(h = headents; h->name != NULL; h++) 
      if(h->rich_header) 
        h->display_it = ! is_on;

    return(is_on);
}



/*
 * entry_line() - return the physical line on the screen associated
 *                with the given header entry field.  Note: the field
 *                may span lines, so if the last char is set, return
 *                the appropriate value.
 *
 *	returns:
 *             1) physical line number of entry
 *             2) -1 if entry currently not on display
 */
entry_line(entry, lastchar)
int	entry, lastchar;
{
    register int    p_line = COMPOSER_TOP_LINE;
    int    i;
    register struct hdr_line    *line;

    for(line=ods.top_l, i=ods.top_e; headents[i].name && i <= entry; p_line++){
	if(p_line >= BOTTOM())
	  break;
	if(i == entry){
	    if(lastchar){
		if(line->next == NULL)
		  return(p_line);
	    }
	    else if(line->prev == NULL)
	      return(p_line);
	    else
	      return(-1);
	}
	line = next_hline(&i, line);
    }
    return(-1);
}



/*
 * physical_line() - return the physical line on the screen associated
 *                   with the given header line pointer.
 *
 *	returns:
 *             1) physical line number of entry
 *             2) -1 if entry currently not on display
 */
physical_line(l)
struct hdr_line *l;
{
    register int    p_line = COMPOSER_TOP_LINE;
    register struct hdr_line    *lp;
    int    i;

    for(lp=ods.top_l, i=ods.top_e; headents[i].name && lp != NULL; p_line++){
	if(p_line >= BOTTOM())
	  break;

	if(lp == l)
	  return(p_line);

	lp = next_hline(&i, lp);
    }
    return(-1);
}



/*
 * call_builder() - resolve any nicknames in the address book associated
 *                  with the given entry...
 *
 *    NOTES:
 * 
 *      BEWARE: this function can cause cur_l and top_l to get lost so BE 
 *              CAREFUL before and after you call this function!!!
 * 
 *      There could to be something here to resolve cur_l and top_l
 *      reasonably into the new linked list for this entry.  
 *
 *      The reason this would mostly work without it is resolve_niks gets
 *      called for the most part in between fields.  Since we're moving
 *      to the beginning or end (i.e. the next/prev pointer in the old 
 *      freed cur_l is NULL) of the next entry, we get a new cur_l
 *      pointing at a good line.  Then since top_l is based on cur_l in
 *      NewTop() we have pretty much lucked out.
 * 
 *      Where we could get burned is in a canceled exit (ctrl|x).  Here
 *      nicknames get resolved into addresses, which invalidates cur_l
 *      and top_l.  Since we don't actually leave, we could begin editing
 *      again with bad pointers.  This would usually results in a nice 
 *      core dump.
 *
 *	RETURNS:
 *              TRUE if any names where resolved, otherwise
 *              FALSE if not, or
 *		-1 on error
 */
call_builder(entry)
struct headerentry *entry;
{
    register    int     retval = FALSE;
    register	int	i;
    register    struct  hdr_line  *line = entry->hd_text;
    char	*sbuf;
    char	*errmsg, *s = NULL, *fcc = NULL;

    if(!entry->builder)
      return(0);

    line = entry->hd_text;
    i = 0;
    while(line != NULL){
	i += term.t_ncol;
        line = line->next;
    }
    if((sbuf=(char *)malloc((unsigned) i)) == NULL){
	emlwrite("Can't malloc space to expand address", NULL);
	return(-1);
    }
    
    *sbuf = '\0';
    /*
     * cat the whole entry into one string...
     */
    line = entry->hd_text;
    while(line != NULL){
	i = strlen(line->text);
	/*
	 * to keep pine address builder happy, addresses should be separated
	 * by ", ".  Add this space if needed, otherwise...
	 *
	 * if this line is NOT a continuation of the previous line, add
	 * white space for pine's address builder if its not already there...
	 *
	 * also if it's not a continuation (i.e., there's already and addr on 
	 * the line), and there's another line below, treat the new line as
	 * an implied comma
	 */
        if(line->text[i-1] == ',')
	  strcat(line->text, " ");		/* help address builder */
	else if(line->next != NULL && !strend(line->text, ',')){
	    if(strqchr(line->text, ',', NULL))
	      strcat(line->text, ", ");		/* implied comma */
	}
	else if(line->prev != NULL && line->next != NULL){
	    if(strchr(line->prev->text, ' ') != NULL 
	       && line->text[i-1] != ' ')
	      strcat(line->text, " ");
	}
	strcat(sbuf, line->text);
        line = line->next;
    }

    errmsg = NULL;
    retval = (*entry->builder)(sbuf, &s, &errmsg,
			       entry->affected_entry ? &fcc : NULL);
    if(errmsg){
	if(*errmsg){
	    char err[500];

	    sprintf(err, "%s field: %s", entry->name, errmsg);
	    (*term.t_beep)();
	    emlwrite(err, NULL);
	}
	else
	    mlerase();

	free(errmsg);
    }

    if(retval != -1){
	if(strcmp(sbuf, s)){
	    line = entry->hd_text;
	    InitEntryText(s, entry);		/* arrange new one */
	    zotentry(line); 			/* blast old list o'entries */
	}

	if(fcc && !entry->affected_entry->sticky){
	    line = entry->affected_entry->hd_text;
	    if(strcmp(line->text, fcc)){ /* make sure they see it if changed */
		entry->affected_entry->display_it = 1;
		InitEntryText(fcc, entry->affected_entry);
		zotentry(line);			/* blast old list o'entries */
	    }
	}

        retval = TRUE;
    }

    if(s)
	free(s);
    if(fcc)
	free(fcc);
    free(sbuf);
    return(retval);
}


/*
 * strend - neglecting white space, returns TRUE if c is at the
 *          end of the given line.  otherwise FALSE.
 */
strend(s, ch)
char *s;
int   ch;
{
    register char *b;
    register char  c;

    c = (char)ch;

    if(s == NULL)
      return(FALSE);

    if(*s == '\0')
      return(FALSE);

    b = &s[strlen(s)];
    while(isspace(*--b)){
	if(b == s)
	  return(FALSE);
    }

    return(*b == c);
}


/*
 * strqchr - returns pointer to first non-quote-enclosed occurance of c in 
 *           the given string.  otherwise NULL.
 */
char *
strqchr(s, ch, q)
    char *s;
    int   ch;
    int  *q;
{
    int	 quoted = (q) ? *q : 0;

    for(; s && *s; s++){
	if(*s == '"'){
	    quoted = !quoted;
	    if(q)
	      *q = quoted;
	}

	if(!quoted && *s == ch)
	  return(s);
    }

    return(NULL);
}


/*
 * KillHeaderLine() - kill a line in the header
 *
 *	notes:
 *		This is pretty simple.  Just using the emacs kill buffer
 *		and its accompanying functions to cut the text from lines.
 *
 *	returns:
 *		TRUE if hldelete worked
 *		FALSE otherwise
 */
KillHeaderLine(l, append)
struct	hdr_line    *l;
int     append;
{
    register char	*c;

    if(!append)
	kdelete();

    c = l->text;
    while(*c != '\0')				/* splat out the line */
      kinsert(*c++);

    kinsert('\n');				/* helpful to yank in body */

    return(hldelete(l));			/* blast it  */
}



/*
 * SaveHeaderLines() - insert the saved lines in the list before the 
 *                     current line in the header
 *
 *	notes:
 *		Once again, just using emacs' kill buffer and its 
 *              functions.
 *
 *	returns:
 *		TRUE if something good happend
 *		FALSE otherwise
 */
SaveHeaderLines()
{
    extern   unsigned	kused;			/* length of kill buffer */
    char     *buf;				/* malloc'd copy of buffer */
    register char       *bp;			/* pointer to above buffer */
    register unsigned	i;			/* index */
    
    if(kused){
	if((bp = buf = (char *)malloc(kused+5)) == NULL){
	    emlwrite("Can't malloc space for saved text", NULL);
	    return(FALSE);
	}
    }
    else
      return(FALSE);

    for(i=0; i < kused; i++)
      if(kremove(i) != '\n')			/* filter out newlines */
	*bp++ = kremove(i);
    *bp = '\0';

    while(--bp >= buf)				/* kill trailing white space */
      if(*bp != ' '){
	  if(ods.cur_l->text[0] != '\0'){
	      if(*bp == '>'){			/* inserting an address */
		  *++bp = ',';			/* so add separator */
		  *++bp = '\0';
	      }
	  }
	  else{					/* nothing in field yet */
	      if(*bp == ','){			/* so blast any extra */
		  *bp = '\0';			/* separators */
	      }
	  }
	  break;
      }

    if(FormatLines(ods.cur_l, buf, LINELEN(),
		   headents[ods.cur_e].break_on_comma, 0) == -1)
      i = FALSE;
    else
      i = TRUE;

    free(buf);
    return(i);
}




/*
 * break_point - Break the given line s at the most reasonable character c
 *               within l max characters.
 *
 *	returns:
 *		Pointer to the best break point in s, or
 *		Pointer to the beginning of s if no break point found
 */
char *
break_point(s, l, ch, q)
    char *s;
    int   l, ch, *q;
{
    register char *b = s + l;
    int            quoted = (q) ? *q : 0;

    while(b != s){
	if(ch == ',' && *b == '"')		/* don't break on quoted ',' */
	  quoted = !quoted;			/* toggle quoted state */

	if(*b == ch){
	    if(ch == ' '){
		if(b + 1 < s + l){
		    b++;			/* leave the ' ' */
		    break;
		}
	    }
	    else{
		/*
		 * if break char isn't a space, leave a space after
		 * the break char.
		 */
		if(!(b+1 >= s+l || (b[1] == ' ' && b+2 == s+l))){
		    b += (b[1] == ' ') ? 2 : 1;
		    break;
		}
	    }
	}
	b--;
    }

    if(q)
      *q = quoted;

    return((quoted) ? s : b);
}




/*
 * hldelete() - remove the header line pointed to by l from the linked list
 *              of lines.
 *
 *	notes:
 *		the case of first line in field is kind of bogus.  since
 *              the array of headers has a pointer to the first line, and 
 *		i don't want to worry about this too much, i just copied 
 *		the line below and removed it rather than the first one
 *		from the list.
 *
 *	returns:
 *		TRUE if it worked 
 *		FALSE otherwise
 */
hldelete(l)
struct hdr_line  *l;
{
    register struct hdr_line *lp;

    if(l == NULL)
      return(FALSE);

    if(l->next == NULL && l->prev == NULL){	/* only one line in field */
	l->text[0] = '\0';
	return(TRUE);				/* no free only line in list */
    }
    else if(l->next == NULL){			/* last line in field */
	l->prev->next = NULL;
    }
    else if(l->prev == NULL){			/* first line in field */
	strcpy(l->text, l->next->text);
	lp = l->next;
	if((l->next = lp->next) != NULL)
	  l->next->prev = l;
	l = lp;
    }
    else{					/* some where in field */
	l->prev->next = l->next;
	l->next->prev = l->prev;
    }

    l->next = NULL;
    l->prev = NULL;
    free((char *)l);
    return(TRUE);
}



/*
 * is_blank - returns true if the next n chars from coordinates row, col
 *           on display are spaces
 */
is_blank(row, col, n)
int row, col, n;
{
    n += col;
    for( ;col < n; col++){
	if(pscr(row, col)->c != ' ')
	  return(0);
    }
    return(1);
}


/*
 * ShowPrompt - display key help corresponding to the current header entry
 */
ShowPrompt()
{
    int new_e = ods.cur_e;

    if(headents[ods.cur_e].key_label) {
	menu_header[TO_KEY].name  = "^T";
	menu_header[TO_KEY].label = headents[ods.cur_e].key_label;
    }
    else
      menu_header[TO_KEY].name  = NULL;

    wkeyhelp(menu_header);
}


/*
 * packheader - packup all of the header fields for return to caller. 
 *              NOTE: all of the header info passed in, including address
 *                    of the pointer to each string is contained in the
 *                    header entry array "headents".
 */
packheader()
{
    register int	i = 0;		/* array index */
    register int	count;		/* count of chars in a field */
    register int	retval = TRUE;	/* count of chars in a field */
    register char	*bufp;		/* */
    register struct	hdr_line *line;

    while(headents[i].name != NULL){
#ifdef	ATTACHMENTS
	/*
	 * attachments are special case, already in struct we pass back
	 */
	if(headents[i].is_attach){
	    i++;
	    continue;
	}
#endif

        /*
         * count chars to see if we need a new malloc'd space for our
         * array.
         */
        line = headents[i].hd_text;
        count = 0;
        while(line != NULL){
            /*
             * add one for possible concatination of a ' ' character ...
             */
            count += (strlen(line->text) + 1);
            line = line->next;
        }
        line = headents[i].hd_text;
        if(count < headents[i].maxlen){		
            *headents[i].realaddr[0] = '\0';
        }
        else{
            /*
             * don't forget to include space for the null terminator!!!!
             */
            if((bufp = (char *)malloc((count+1) * sizeof(char))) != NULL){
                *bufp = '\0';

                free(*headents[i].realaddr);
                *headents[i].realaddr = bufp;
            }
            else{
                emlwrite("Can't make room to pack header field.", NULL);
                retval = FALSE;
            }
        }

        if(retval != FALSE){
	    while(line != NULL){
                strcat(*headents[i].realaddr, line->text);
		if(line->text[strlen(line->text)-1] == ',')
		  strcat(*headents[i].realaddr, " ");
                line = line->next;
            }
        }

        i++;
    }
    return(retval);    
}



/*
 * zotheader - free all malloc'd lines associated with the header structs
 */
zotheader()
{
    register struct headerentry *i;
  
    for(i=headents; i->name != NULL; i++){
	zotentry(i->hd_text);
      }
}


/*
 * zotentry - free malloc'd space associated with the given linked list
 */
zotentry(l)
register struct hdr_line *l;
{
    register struct hdr_line *ld, *lf = l;

    while((ld = lf) != NULL){
	lf = ld->next;
	ld->next = ld->prev = NULL;
	free((char *) ld);
    }
}



/*
 * zotcomma - blast any trailing commas and white space from the end 
 *	      of the given line
 */
void
zotcomma(s)
char *s;
{
    register char *p;

    p = &s[strlen(s)];
    while(--p >= s){
	if(*p != ' '){
	    if(*p == ',')
	      *p = '\0';
	    return;
	}
    }
}
