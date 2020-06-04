// gcc -o tiny_web tiny_web.c -lncurses -lform
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <ncurses.h>
#include <form.h>
#include <assert.h>
#include <dirent.h>
#include <ctype.h>
#include <math.h>

#define SERVER_PORT  8080

#define TRUE             1
#define FALSE            0

#define VERSION 23
//#define BUFSIZE 300
#define BUFSIZE 300
#define ERROR      42
#define LOG        44
#define FORBIDDEN 403
#define NOTFOUND  404
#define NOTMODIFIED  304


#define USLEEP_INTERVAL 10000

#define MAXELEMENTS 7
#define MAXSTATUSLINE 77


#define PLAYLIST_FILE_SIZE 64
#define INFO_ITEM_SIZE 64

#define POLLFDS 128
#define MAXFIELDS 8

struct list_el {
   char line[MAXSTATUSLINE];
   struct list_el * next;
};

typedef struct list_el item;

int sent[POLLFDS] = {0};
int fdsf[POLLFDS] = {0};


char playlist_file_a[PLAYLIST_FILE_SIZE] = {0};
char playlist_file_b[PLAYLIST_FILE_SIZE] = {0};

char pollfd2ipstr[POLLFDS][INET_ADDRSTRLEN];



int video_id = 1;


static WINDOW *win_body;

static FORM *form;

static FIELD *fields[MAXFIELDS + 1];
static WINDOW *win_body, *win_form;

FORM *update_form(void);



struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },  
	{"jpg", "image/jpg" }, 
	{"jpeg","image/jpeg"},
	{"png", "image/png" },  
	{"ico", "image/ico" },  
	{"zip", "image/zip" },  
	{"gz",  "image/gz"  },  
	{"tar", "image/tar" },  
	{"htm", "text/html" },  
	{"html","text/html" }, 
	{"xml","text/xml" },
	{"css","text/css" }, 
	{"mp4","video/mp4" },   
	{"json","application/json" },
	{"js","application/javascript" },      
	{0,0} };
		
FORM *update_form(void)
{
	FORM *tform;
	if(form)
	{ 
		unpost_form(form);
		free_form(form);
	}
	
	
	tform = new_form(fields);
	
	assert(tform != NULL);
	set_form_win(tform, win_form);
	//set_form_sub(tform, derwin(win_form, 10, 76, 1, 1));
	set_form_sub(tform, derwin(win_form, 8, 76, 1, 1));
	post_form(tform);
  return tform;
		
}


int list_directory()
{
	DIR *dp;
  struct dirent *ep;     
  dp = opendir (".");
	int fc = 0;
	const char *mp4 = "mp4\0";

	
  if (dp != NULL)
  {
    while ((ep = readdir (dp)) != false)
    {
    	if(ep != NULL && strstr((const char *) (ep->d_name),(const char *) mp4) != NULL)
    		{
    		if(fc < MAXFIELDS)
    		{
					set_field_buffer(fields[fc], 0, (char *) (ep->d_name));
					set_field_opts(fields[fc], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE);  	
      		fc++;
      	}
      	//fprintf(stderr,"file:%s\n", (ep->d_name));
    		}	
    }
    closedir (dp);
  	fprintf(stderr,"fc is :%d\n",fc);
  }
  else
  {
    fprintf(stderr,"Couldn't open the directory");
	}
  return 0;
}

void print_pointer_items(item * const phead)
{
	item * curs = (item *) NULL;
	curs = phead;
	fprintf(stderr, "[debug] ");
	while(curs != NULL && curs->next != NULL)
	{
		fprintf(stderr,"%p ",curs);
		curs = curs->next;
	}
	fprintf(stderr, "\n");
}

item *push_new_label(item * const phead, char *label)
{
        item * curs = (item *) NULL;
        item * first = (item *) phead;
        item * last = (item *) NULL;
        item * prev = (item *) phead->next;
        curs = phead;

        //fprintf(stderr,"[warn]");
        while(curs != (item *) NULL && curs->next != (item *) NULL)
        {
                        //fprintf(stderr,"%p ",curs);
                        //prev = curs;
                        curs = curs->next;
        }
        //fprintf(stderr,"%p ",curs);
        //fprintf(stderr,"\n");

        last = curs;


        struct timeval tv_stat;
        gettimeofday(&tv_stat, NULL);


        snprintf((char *)&curs->line, sizeof(curs->line), "%ld %s",tv_stat.tv_sec, label);

        first->next = (item *) NULL;
        last->next = first;
        fprintf(stderr,"returning :%p\n", prev);
        return prev;
}

void init_list(item **phead)
{
	item *temp;
	int il;
	
	//temp = *phead;
	
	for(il = 0; il <= MAXELEMENTS; il++)
	{
		temp = (item *) malloc(sizeof(struct list_el));
		memset(temp->line, 0, MAXSTATUSLINE);
		temp->next = (item *) *phead;
		*phead = temp;
	}
	
}
		
void update_status_text(item * const phead)
{
	int maxstat = 0;
	int color = 2;
	int x, y;            
	char wiper[MAXSTATUSLINE] = {0xff};
	item *curr;
	curr = phead;
	
	memset(wiper, 32, MAXSTATUSLINE);
	wiper[MAXSTATUSLINE - 1] = '\0';
		
	getyx(win_form, y, x); 
	//fprintf(stderr, "update status: x:%d y:%d\n", y, x);
	wattron(win_body, A_BOLD);
	
	while(curr && maxstat++ < MAXELEMENTS)
	{
	 if(strstr(curr->line,"[erro]") != NULL)
		color = 4;
	 if(strstr(curr->line,"[warn]") != NULL)
		color = 3;
	 if(strstr(curr->line,"[info]") != NULL)
		color = 2; 		         
	 
	 wattron(win_body,COLOR_PAIR(color));
	
	 mvwprintw(win_body, 15 + maxstat, 2, wiper);
	 mvwprintw(win_body, 15 + maxstat, 2, curr->line);

	 wattroff(win_body,COLOR_PAIR(color));

		
	 curr = curr->next;
	}
	wattroff(win_body, A_BOLD);
	wrefresh(win_body);
	
	wmove(win_form, y, x);
	wrefresh(win_form);
	pos_form_cursor(form);
}

void logger(int type, char *s1, char *s2, int socket_fd)
{
	int fd ;
	char logbuffer[BUFSIZE*10];

	switch (type) {
	case ERROR: (void)sprintf(logbuffer,"ERROR: %s:%s Errno=%d exiting pid=%d",s1, s2, errno,getpid()); 
		break;
	case FORBIDDEN:
		fprintf(stderr, "write call forbidden\n"); 
		(void)write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n",271);
		(void)sprintf(logbuffer,"FORBIDDEN: %s:%s",s1, s2); 
		break;
	case NOTFOUND: 
		fprintf(stderr, "write call not found\n");
		(void)write(socket_fd, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n",224);
		(void)sprintf(logbuffer,"NOT FOUND: %s:%s",s1, s2); 
		break;
	case NOTMODIFIED: 
		fprintf(stderr, "write call not found\n");
		(void)write(socket_fd, "HTTP/1.1 304 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n",224);
		(void)sprintf(logbuffer,"NOT FOUND: %s:%s",s1, s2); 
		break;	
	case LOG: (void)sprintf(logbuffer," INFO: %s:%s:%d",s1, s2,socket_fd); break;
	}	
	/* No checks here, nothing can be done with a failure anyway */
	if((fd = open("eweb.log", O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) {
		//fprintf(stderr, "write call error in log?!\n");
		(void)write(fd,logbuffer,strlen(logbuffer)); 
		(void)write(fd,"\n",1);      
		(void)close(fd);
	}
	//if(type == ERROR || type == NOTFOUND || type == FORBIDDEN) exit(3);
}

static char* trim_whitespaces(char *str)
{
	char *end;

	while(isspace((unsigned char) *str))
		str++;

	if(*str == 0) 
		return str;

	end = str + strnlen(str, 128) - 1;

	while(end > str && isspace((unsigned char) *end))
		end--;

	*(end+1) = '\0';

	return str;
}


int remove_questionmark(char *a)
{
	char *remove_q = NULL;
  remove_q = strchr((const char *) a, '?');
  
  if(remove_q)
  	*remove_q = 0;
  
  return 0;
}


int remove_asterisc(char *a)
{
	char *remove_q = NULL;
  remove_q = strchr((const char *) a, '*');
  
  if(remove_q)
  	*remove_q = 0;
  
  return 0;
}

int replace_equals(char *a)
{
	char *remove_q = NULL;
  remove_q = strchr((const char *) a, '=');
  
  if(remove_q)
  	*remove_q = 32;
  
  return 0;
}


int remove_dies(char *a)
{
	char *remove_q = NULL;
  remove_q = strchr((const char *) a, '#');
  
  if(remove_q)
  	*remove_q = 0;
  
  return 0;
}


int replace_video_file(char *a, char *b)
{
	char *remove_q = NULL;
  remove_q = strchr((const char *) a, 'X');
  
  char *playlist_idx = NULL;
  playlist_idx = strchr((const char *) b, '.');
  
  
  int idx = 0;
  if(playlist_idx)
  	idx = atoi((char *)(playlist_idx - 1));
  
  if(idx == 0)
  	idx = 1;
  
  fprintf(stderr,"parsed file from %s %s to %d\n", a, b, idx); 
  
  
  
  if(remove_q)
  	*remove_q = (char) (0x30 + idx);
  
  return 0;
}

int replace_video_id(char *a)
{
	char *remove_q = NULL;
  remove_q = strchr((const char *) a, 'X');
  
  if(remove_q)
  	*remove_q = (char) (0x30 + video_id);
  
  return 0;
}


/* this is a child web server process, so we can exit on errors */
int web(int fd, int hit, int pollin)
{
	time_t t;
  struct tm *tmp;

  t = time(NULL);
  tmp = localtime(&t);
	
	int playlist = 0;
	
	//fprintf(stdout,"\rcalled on fd:%d client:%d size:%d\n",fd,hit,sent[hit]);
	int j, file_fd, buflen;
	long i, ret, len;
	char * fstr;
	static char buffer[BUFSIZE+1]; /* static so zero filled */
	static char inbuffer[BUFSIZE*4+1];
	long range_pos = 0;

	if(sent[hit] == 0 || pollin == 1)
	{
	ret =read(fd,inbuffer,BUFSIZE*4); 	/* read Web request in one go */
	if(ret == 0 || ret == -1) {	/* read failure stop now */
		//todo bofh logger(FORBIDDEN,"failed to read browser request","",fd);
		fprintf(stderr,"unable to read from socket %d \n",hit);
		return 0;
	}
	if(ret > 0 && ret < BUFSIZE*4)	/* return code is valid chars */
		inbuffer[ret]=0;		/* terminate the buffer */
	else inbuffer[0]=0;
	for(i=0;i<ret;i++)	/* remove CF and LF characters */
		if(inbuffer[i] == '\r' || inbuffer[i] == '\n')
			inbuffer[i]='#';
	logger(LOG,"request",inbuffer,hit);

  char *range = "Range: bytes=";
	char *bytes_pos = NULL;

	if(strstr((char *) &inbuffer,range) != NULL)
		{
			char *bytes = "bytes=";
			if((bytes_pos = strstr((char *) &inbuffer, bytes)) != NULL)
			{
				
				fprintf(stderr, "range request:%s,%ld\n", bytes_pos, atol(bytes_pos+6));
				range_pos = atol(bytes_pos+6);
				sent[hit] = range_pos;
				remove_asterisc(bytes_pos);
				replace_equals(bytes_pos);
				remove_dies(bytes_pos);
			}
		}


	if( strncmp(inbuffer,"GET ",4) && strncmp(inbuffer,"get ",4) ) {
		//todo bofh logger(FORBIDDEN,"Only simple GET operation supported",inbuffer,fd);
		return 0;
	}
	for(i=4;i<BUFSIZE*4;i++) { /* null terminate after the second space to ignore extra stuff */
		if(inbuffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
			inbuffer[i] = 0;
			break;
		}
	}
	for(j=0;j<i-1;j++) 	/* check for illegal parent directory use .. */
		if(inbuffer[j] == '.' && inbuffer[j+1] == '.') {
			logger(FORBIDDEN,"Parent directory (..) path names not supported",inbuffer,fd);
		}
		
	remove_questionmark((char *) &inbuffer);	
		
		
	if( !strncmp(&inbuffer[0],"GET /\0",6) || !strncmp(&inbuffer[0],"get /\0",6) ) /* convert no filename to index file */
		(void)strcpy(inbuffer,"GET /index.html");

	/* work out the file type and check we support it */
	buflen=strlen(inbuffer);
	fstr = (char *)0;
	for(i=0;extensions[i].ext != 0;i++) {
		len = strlen(extensions[i].ext);
		if( !strncmp(&inbuffer[buflen-len], extensions[i].ext, len)) {
			fstr =extensions[i].filetype;
			break;
		}
	}
	if(fstr == 0) logger(FORBIDDEN,"file extension type not supported",inbuffer,fd);

	if(( file_fd = open(&inbuffer[5],O_RDONLY)) == -1) {  /* open the file for reading */
		logger(NOTFOUND, "failed to open file",&inbuffer[5],fd);
		fprintf(stderr, "file not found: %s\n",(char *) &inbuffer);
	}
	fdsf[hit] = file_fd;
	
	
	logger(LOG,"SEND",&inbuffer[5],hit);
	len = (long)lseek(file_fd, (off_t)0, SEEK_END); /* lseek to the file end to find the length */
	      (void)lseek(file_fd, (off_t)0, SEEK_SET); /* lseek back to the file start ready for reading */
  char http_date[64] = {0};
  
  if(strncmp(&inbuffer[5], "playlist.json", 13) == 0)
  	playlist = 1; 
  
  fprintf(stderr, "serving(%d): %s\n",playlist, inbuffer);
  
  
  
  //todo check?!
  len--;
  
  //if(playlist == 1)
  //	len = (PLAYLIST_FILE_SIZE > strlen(playlist_file) - 1) ? strlen(playlist_file) - 1 : PLAYLIST_FILE_SIZE;
  	
  	
  
  strftime((char *) &http_date, sizeof(http_date), "%a, %d %b %Y %H:%M:%S GMT", tmp);
  
  	
  	if(range_pos == 0 && bytes_pos == NULL)
  	{
  		(void)sprintf(buffer,
  		"HTTP/1.1 200 OK\nServer: ecard/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", 
  		VERSION,
  		len, 
  		fstr); /* Header + a blank line */
  	}else
  	{
  		if(strncmp(bytes_pos, "0-\0", 3))
  			{
  				fprintf(stderr,"incomplete bytes pos, should append\n");
  			(void)sprintf(buffer,
  			"HTTP/1.1 206 Partial Content\nDate: %s\nExpires: %s\nAccept-Ranges: bytes\nServer: ecard/%d.0\nContent-Length: %ld\nContent-Range: %s%ld/%ld\nConnection: close\nContent-Type: %s\n\n",
  			http_date, 
  			http_date, 
  			VERSION,
  			(len - range_pos),
  			bytes_pos,
  			len,
  			len, 
  			fstr); /* Header + a blank line */	
  				
  			}else
  			{
  		(void)sprintf(buffer,
  		"HTTP/1.1 206 Partial Content\nDate: %s\nExpires: %s\nAccept-Ranges: bytes\nServer: ecard/%d.0\nContent-Length: %ld\nContent-Range: %s\nConnection: close\nContent-Type: %s\n\n",
  		http_date, 
  		http_date, 
  		VERSION,
  		(len - range_pos),
  		bytes_pos, 
  		fstr); /* Header + a blank line */	
  	
  			}
  		
  	}
  	
		logger(LOG,"Header",buffer,hit);
		(void)write(fd,buffer,strlen(buffer));
		fprintf(stderr,"write call header\n");
	} // if sent[hit] that is we have initial request
	else
	{
		//len = (long)lseek(file_fd, (off_t)0, sent[hit]); /* lseek to the pos */
		//logger(LOG,"Seaking ",buffer,hit);
	}
	/* send file in 8KB block - last block may be smaller */
	int wlen = 0;
	
	if(range_pos != 0)
	{
		fprintf(stderr,"seeking to %ld\n", range_pos);
		lseek(fdsf[hit], (off_t) range_pos, SEEK_SET);
		sent[hit] = (int) range_pos;
	}

	
	//if(!strncmp(&inbuffer[0],"playlist.json",13))
	//{
		while (	(ret = read(fdsf[hit], buffer, BUFSIZE)) > 0 ) {
		//wlen += write(fd,buffer,ret);
		
		  //if(playlist == 1 && video_id > 0)
		  //{
		  //	replace_video_id((char *) &buffer);
				//fprintf(stderr,"playlist(%ld):%s\n",len,buffer);
				//fprintf(stderr,"endplaylist\n");
			//}
			fprintf(stderr,"connection from %s on %d\n", pollfd2ipstr[fd], fd);
			if(playlist == 1)
			{
				
				if(strncmp(pollfd2ipstr[fd], "192.18.116.162", 14))
					replace_video_file((char *) &buffer, (char *) &playlist_file_a);
				else 
				if(strncmp(pollfd2ipstr[fd], "192.18.116.164", 14))
					replace_video_file((char *) &buffer, (char *) &playlist_file_b);
				else
					replace_video_file((char *) &buffer, (char *) &playlist_file_a);
			}
			
			wlen += send(fd, buffer, ret, 0);
		
			sent[hit] += wlen;
			return wlen;
		}
	//}
	//else
	//{
	//	char playlist[64] = {0};
	//	snprintf((char *) &playlist, sizeof(playlist), "[{\"filename\":\"video%d.mp4\"}]",2);
	//	fprintf(stderr,"sending custom playlist: %s\n",playlist);
	//	wlen += send(fd, buffer, ret, 0);
		
	//	sent[hit] += wlen;
	//	return wlen;	
			
	//}

	
	if(fdsf[hit])
		close(fdsf[hit]);
	sent[hit] = 0;	
	
	// should perform close on those here
	
	//sleep(1);	/* allow socket to drain before signalling the socket is closed */
	//close(fd);
	usleep(1);

	return 0;
	//exit(1);
}

int main (void)
{
  //int    len = 0;
  int		 rc, on = 1;
  int    listen_sd = -1, new_sd = -1;
  //int    desc_ready, 
  int 	 end_server = FALSE; //, compress_array = FALSE;
  int    close_conn = 0;
//  char   buffer[80];
  struct sockaddr_in   addr;
  int    timeout;
  struct pollfd fds[POLLFDS];

  
  
  int    nfds = POLLFDS, current_size = 0, i;
	int ch;
	char 	info_item[INFO_ITEM_SIZE] = {0};
	item * head_status_item;


  /*************************************************************/
  /* Create an AF_INET6 stream socket to receive incoming      */
  /* connections on                                            */
  /*************************************************************/
  
  (void)signal(SIGCLD, SIG_IGN); /* ignore child death */
	(void)signal(SIGHUP, SIG_IGN); /* ignore terminal hangups */
	(void)signal(SIGPIPE, SIG_IGN); /* ignore terminal hangups */


	freopen("weberr.txt", "w", stderr );
  setbuf(stderr, NULL);


	initscr();
	noecho();
	cbreak();
	keypad(stdscr, TRUE);
	
	
	start_color(); /*  Initialize colours  */
  init_pair(1, COLOR_BLACK, COLOR_RED);
  init_pair(2, COLOR_GREEN, COLOR_BLACK);
  init_pair(3, COLOR_YELLOW, COLOR_BLACK);
  init_pair(4, COLOR_RED, COLOR_BLACK);
 
  use_default_colors();

	win_body = newwin(24, 80, 0, 0);
	assert(win_body != NULL);


	box(win_body, 0, 0);
	
	
	win_form = derwin(win_body, 10, 78, 6, 1);
	assert(win_form != NULL);
	box(win_form, 0, 0);

	mvwprintw(win_body, 1, 30, "[D]\t\tAssign video to left TV");
	mvwprintw(win_body, 2, 30, "[E]\t\tAssign video to right TV");
  mvwprintw(win_body, 3, 30, "[F10]\t\tQuit program");
	


	int nf;
	for(nf = 0; nf < MAXFIELDS; nf++)
	{
		//FIELD *new_field(int height, int width, int toprow, int leftcol, int offscreen, int nbuffers);
		fields[nf] = new_field(1, 64, nf, 0, 0, 0); 
		set_field_buffer(fields[nf], 0, "");
		set_field_opts(fields[nf], O_VISIBLE | O_PUBLIC | O_EDIT);  	
		//set_field_back(fields[nf], A_UNDERLINE);
		

	}
	
	fields[nf] = NULL;
	
	head_status_item = NULL;
 	
	init_list(&head_status_item);
	
	
	list_directory();
	
	form = update_form();
		
	refresh();

 	
 	wrefresh(win_form);
 	wrefresh(win_body);

	wmove(win_form, 1, 1);

	
  listen_sd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_sd < 0)
  {
    perror("socket() failed");
    exit(-1);
  }
  
  nfds = 1;

  /*************************************************************/
  /* Allow socket descriptor to be reuseable                   */
  /*************************************************************/
  rc = setsockopt(listen_sd, SOL_SOCKET,  SO_REUSEADDR,
                  (char *)&on, sizeof(on));
  if (rc < 0)
  {
    perror("setsockopt() failed");
    close(listen_sd);
    exit(-1);
  }

  /*************************************************************/
  /* Set socket to be nonblocking. All of the sockets for      */
  /* the incoming connections will also be nonblocking since   */
  /* they will inherit that state from the listening socket.   */
  /*************************************************************/
  
  //rc = ioctl(listen_sd, FIONBIO, (char *)&on);
  //if (rc < 0)
 	//{
  //  perror("ioctl() failed");
  //  close(listen_sd);
  //  exit(-1);
 	//}

	int flags = fcntl(listen_sd, F_GETFL, 0);
	fcntl(listen_sd, F_SETFL, flags | O_NONBLOCK);



  /*************************************************************/
  /* Bind the socket                                           */
  /*************************************************************/
  memset(&addr, 0, sizeof(addr));
  addr.sin_family      = AF_INET;
  memcpy(&addr.sin_addr, &in6addr_any, sizeof(in6addr_any));
  addr.sin_port        = htons(SERVER_PORT);
  rc = bind(listen_sd,
            (struct sockaddr *)&addr, sizeof(addr));
  if (rc < 0)
  {
    perror("bind() failed");
    close(listen_sd);
    exit(-1);
  }

  /*************************************************************/
  /* Set the listen back log                                   */
  /*************************************************************/
  //rc = listen(listen_sd, 32);
  rc = listen(listen_sd, 1);
  if (rc < 0)
  {
    perror("listen() failed");
    close(listen_sd);
    exit(-1);
  }

  /*************************************************************/
  /* Initialize the pollfd structure                           */
  /*************************************************************/
  memset(fds, 0 , sizeof(fds));


 	fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;

  fds[1].fd = listen_sd;
  fds[1].events = POLLIN;
 
  nfds++;

  if(nfds > POLLFDS)
    nfds = 1;
 
  timeout = (30 * 60 * 1000);

  /*************************************************************/
  /* Loop waiting for incoming connects or for incoming data   */
  /* on any of the connected sockets.                          */
  /*************************************************************/
  do
  {
    /***********************************************************/
    /* Call poll() and wait 3 minutes for it to complete.      */
    /***********************************************************/
    //printf("Waiting on poll()...\n");
    rc = poll(fds, POLLFDS, timeout);

    /***********************************************************/
    /* Check to see if the poll call failed.                   */
    /***********************************************************/
    if (rc < 0)
    {
      perror("  poll() failed");
      break;
    }

    /***********************************************************/
    /* Check to see if the 3 minute time out expired.          */
    /***********************************************************/
    if (rc == 0)
    {
      fprintf(stderr, "  poll() timed out.  End program.\n");
      break;
    }


		if(fds[0].revents & POLLIN)
  	{
  			ch = getch();
  			int fx, fy;
  			char *f2;
  		
  			
  			switch (ch)
  			{
	  			case KEY_F(10):
	  				
					  end_server = true;	  				
	  				
	  				break;
	  			case 32:
	  				video_id++;
	  				
	  				if(video_id > 4)
	  					video_id = 1;
	  						
						//info_item[64] = {0};
	          memset(info_item, '\0', INFO_ITEM_SIZE);
	          snprintf((char *) &info_item, INFO_ITEM_SIZE , "[info] updating video to %d", video_id);
	          head_status_item = push_new_label(head_status_item, info_item);	
				  	update_status_text(head_status_item);
	  				break;
	  			case 100: // key D			    	
						//fx = 0; fy = 0;
						getyx(win_body,fy, fx);
						fprintf(stderr, "cursors x y are (%d, %d)\n",fx, fy);
					
						if(fy > 6 && fy < 18)
						{
							fy -= 7;
							f2 = trim_whitespaces(field_buffer(fields[fy],0));
		
							//snprintf((char *) &passline, sizeof(passline) - 1,"%s,%s",f1, f2);
							//send_data = 1;
							memset(info_item, '\0', INFO_ITEM_SIZE);
					  	snprintf((char *) &info_item, INFO_ITEM_SIZE, "[warn] updating left tv video to %s", f2);
	        
	          	head_status_item = push_new_label(head_status_item, info_item);	
				  		update_status_text(head_status_item);
	  					
	  					
	  					memset(playlist_file_a, 0, PLAYLIST_FILE_SIZE);
	  					//snprintf((char *) &playlist_file, PLAYLIST_FILE_SIZE, "[{\"filename\":\"%s\"}]", f2);
	  					snprintf((char *) &playlist_file_a, PLAYLIST_FILE_SIZE, "%s", f2);
	  				
							fprintf(stderr, "input(fy%d):%s\n",fy, f2);
							//form_driver(form, REQ_NEXT_FIELD);				
					}
					break;
	  			case 101: // key E			    	
						//fx = 0; fy = 0;
					
						getyx(win_body,fy, fx);
					
						fprintf(stderr, "cursors x y are (%d, %d)\n",fx, fy);
					
						if(fy > 6 && fy < 18)
						{
							fy -= 7;
							f2 = trim_whitespaces(field_buffer(fields[fy],0));
		
							//snprintf((char *) &passline, sizeof(passline) - 1,"%s,%s",f1, f2);
							//send_data = 1;
							memset(info_item, '\0', INFO_ITEM_SIZE);
					  	snprintf((char *) &info_item, sizeof(info_item), "[warn] updating right tv video to %s", f2);
	        
	          	head_status_item = push_new_label(head_status_item, info_item);	
				  		update_status_text(head_status_item);
	  					
	  					
	  					memset(playlist_file_b, 0, PLAYLIST_FILE_SIZE);
	  					//snprintf((char *) &playlist_file, PLAYLIST_FILE_SIZE, "[{\"filename\":\"%s\"}]", f2);
	  					snprintf((char *) &playlist_file_b, PLAYLIST_FILE_SIZE, "%s", f2);
	  				
							fprintf(stderr, "input(fy%d):%s\n",fy, f2);
							//form_driver(form, REQ_NEXT_FIELD);				
					}
					break;

			
	  				
	  				
	  			case KEY_DOWN:
						form_driver(form, REQ_NEXT_FIELD);
						//form_driver(form, REQ_END_LINE);
						break;

					case KEY_UP:
						form_driver(form, REQ_PREV_FIELD);
						//form_driver(form, REQ_END_LINE);
						break;
  			}
				wrefresh(win_form);

  			
  			//fprintf(stderr, "got key:%d\n",ch);
  	}
  		


    /***********************************************************/
    /* One or more descriptors are readable.  Need to          */
    /* determine which ones they are.                          */
    /***********************************************************/
    
    
    
    
    current_size = POLLFDS;
    for (i = 1; i < current_size; i++)
    {
    	if(fds[i].fd <= 0)
    		continue;
    
      if(fds[i].revents == 0)
        continue;

      if (fds[i].fd == listen_sd)
      {
        
        fprintf(stderr, "  Listening socket is readable with events:%d\n", fds[i].revents);

        do
        {
        	struct sockaddr_storage incomming_addr;
					socklen_t incomming_len;
					
        	incomming_len = sizeof incomming_addr;
					
        
          new_sd = accept(listen_sd, (struct sockaddr*) &incomming_addr, &incomming_len);
          if (new_sd < 0)
          {
            if (errno != EWOULDBLOCK)
            {
              perror("  accept() failed");
              end_server = TRUE;
            }
            break;
          }
          
          
          //todo bofh wtf?!
          //int flag = 1; 
					//setsockopt(new_sd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));

        	char ipstr[INET_ADDRSTRLEN];

          struct sockaddr_in *incomming_s = (struct sockaddr_in *)&incomming_addr;
			    inet_ntop(AF_INET, &incomming_s->sin_addr, ipstr, sizeof ipstr);
			    
			    strlcpy((char *) &pollfd2ipstr[new_sd], (const char *) &ipstr, INET_ADDRSTRLEN);

          print_pointer_items(head_status_item);

          fprintf(stderr,"  New incoming connection - %d (%s) (%s)\n", new_sd, ipstr, pollfd2ipstr[new_sd]);
          
					memset(info_item, '\0', INFO_ITEM_SIZE);          
          
          print_pointer_items(head_status_item);
          
          
          snprintf((char *) &info_item, sizeof(info_item), "[info] connection %d %s", nfds, ipstr);
          head_status_item = push_new_label(head_status_item, info_item);	
			  	update_status_text(head_status_item);
			  
          fds[nfds].fd = new_sd;
          fds[nfds].events = POLLIN | POLLOUT | POLLHUP | POLLERR;
          nfds++;

        } while (new_sd != -1);
      } 
      else
      { // existing connection
      	
        //printf("  Descriptor %d is readable\n", fds[i].fd);
        //close_conn = FALSE;
        /*******************************************************/
        /* Receive all incoming data on this socket            */
        /* before we loop back and call poll again.            */
        /*******************************************************/

		    //if(fds[i].revents & POLLIN || fds[i].revents & POLLOUT)
		    if(fds[i].revents & POLLHUP)
		    	fprintf(stderr, "received pollhup\n");
		    if(fds[i].revents & POLLERR)
		    	fprintf(stderr, "received pollerr\n");
		    	
        if(fds[i].revents & POLLOUT && sent[i] != 0)
        {
        	//fprintf(stderr,"pollout, lets send the data\n");
        	int pollout_sent = 0;
        	pollout_sent  = web(fds[i].fd,i, 0); 
        	//fprintf(stderr,"\rsending %d out of %d\n",pollout_sent, sent[i]);
        	if(pollout_sent <= 0)
        		{
							fprintf(stderr,"closed socket %d %d %s\n", pollout_sent, errno, strerror(errno));
							//close_conn = TRUE;
        			close(fds[i].fd);
							fds[i].fd = -1;
        			fds[i].events = 0;
        			sent[i] = 0;
        			close(fdsf[i]);
        			nfds--;
        			break;
        		}
        }
        	
        if(fds[i].revents & POLLIN)
        //do
        {
        	//sent[i] = 0;
         	int sent_web_bytes = web(fds[i].fd,i, 1); 
         	fprintf(stderr, "sent:%d position:%d\n",sent_web_bytes,sent[i]); /* never returns */
         	if(sent_web_bytes <= 0)
         	{
						fprintf(stderr,"closed socket %d %d %s\n", sent_web_bytes, errno, strerror(errno));
						
         		//close_conn = TRUE;
        		close(fds[i].fd);
        		fds[i].fd = -1;
        		fds[i].events = 0;
        		sent[i] = 0;
        		close(fdsf[i]);
        		nfds--;
						break;
					}
					
        }// while(TRUE);

      }  /* End of existing connection is readable             */
    } /* End of loop through pollable descriptors              */
  } while (end_server == FALSE); /* End of serving running.    */
  fprintf(stderr,"end_server:%d\n", end_server);
  /*************************************************************/
  /* Clean up all of the sockets that are open                 */
  /*************************************************************/
  
  for (i = 1; i < nfds; i++)
  {
    if(fds[i].fd >= 0)
      close(fds[i].fd);
  }
  
  
  unpost_form(form);
	free_form(form);
	for(i = 0; i < MAXFIELDS; i++)
		free_field(fields[i]);
  
	delwin(win_body);
	delwin(win_form);
	
	endwin();
 	
 	return 0;
}
