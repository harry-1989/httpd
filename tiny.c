#include "csapp.h"  
/* 
 *˵��:����telnetʱ,Hostͷ��ָ��������Դ��Intenet�����Ͷ˿ں� 
 * �����ʾ����url��ԭʼ�����������ص�λ�á� 
 * HTTP/1.1��������������ͷ�򣬷���ϵͳ����400״̬�뷵�ء� 
 */  
  
void handler(int sig);  
void doit(int fd);  
void read_requesthdrs(rio_t *rp, int *length, int is_post_method);  
int parse_uri(char *uri, char *filename, char *cgiargs);  
void serve_static(int fd, char *filename, int filesize, int is_head_method);  
void get_filetype(char *filename, char *filetype);  
void serve_dynamic(int fd, char *filename, char *cgiargs, int is_head_method);  
void post_dynamic(int fd, char *filename, int contentLength,rio_t *rp);  
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);  
  
int main(int argc, char **argv, char **environ)  
{  
    int listenfd, connfd, port, clientlen;  
    struct sockaddr_in clientaddr;  
  
    /* ��������в��� */  
    if (argc != 2) {  
        fprintf(stderr, "usage: %s <port>\n", argv[0]);  
        exit(0);  
    }  
    port = atoi(argv[1]);  
  
    listenfd = Open_listenfd(port);  
    printf("Tiny Web Server...\n");  
    while (1) {  
        clientlen = sizeof(clientaddr);  
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  
      
    Signal(SIGCHLD, handler);     
  
    /* fork�ӽ��̴������� */  
    if (Fork() == 0) {  
        Close(listenfd);  /* �رռ��������� */  
        doit(connfd);       
            Close(connfd);    /* �ر������������� */  
        exit(0);  
    }  
    Close(connfd);   /* �����̹ر�������������,��ʱ�ļ���������ü���Ϊ0,���ͻ��˵�������ֹ */  
          
    }  
    exit(0);  
}  
  
  
void handler(int sig)  
{  
    pid_t pid;  
  
    if ((pid = waitpid(-1, NULL, 0)) < 0)  
        unix_error("waitpid error");  
    //printf("Handler reaped child %d\n", (int)pid);  
    return;  
}  
  
  
void doit(int fd)  
{  
    int is_static, head = 0, post = 0, contentLength = 0;  
    struct stat sbuf;  
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];  
    char filename[MAXLINE], cgiargs[MAXLINE];  
    rio_t rio;  
  
    Rio_readinitb(&rio, fd); /* ��ʼ�������� */  
    Rio_readlineb(&rio, buf, MAXLINE); /* ���������ڵ�������(һ��)���ݶ���buf�� */  
  
    /* �������� */  
    sscanf(buf, "%s %s %s", method, uri, version);  
    printf("%s %s %s\r\n", method, uri, version);                  //11.6����չA  
  
    if (!strcasecmp(method, "HEAD")) {  
        head = 1;  
    }  
    else if (!strcasecmp(method, "POST")) {   /* ���Դ�Сд�Ƚ��ַ��� */  
        post = 1;  
  
    }  
    else if (!strcasecmp(method, "GET"));  
    else {  
        clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");  
        return;  
    }  
  
    read_requesthdrs(&rio, &contentLength, post);   /* ��ȡ����������ͷ */  
  
    /* ��������GET�����URI */  
    is_static = parse_uri(uri, filename, cgiargs);   /* is_static�ж�������ļ��Ƿ�Ϊ��̬�ļ� */  
    if (stat(filename, &sbuf) < 0) {  
        clienterror(fd, filename, "404", "Not found", "Tiny couldn't read the file");  
        return;  
    }  
  
    if (is_static) {  
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {  
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");  
            return;  
        }  
        //printf("yes\n");  
        serve_static(fd, filename, sbuf.st_size, head);    /* ����̬���� */  
    }  
    else {  
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {  
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");  
            return;  
        }  
        if (!post)  
            serve_dynamic(fd, filename, cgiargs, head);       /* ����̬���� */  
        else  
            post_dynamic(fd, filename, contentLength, &rio);  
    }  
}  
  
  
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)  
{  
    char buf[MAXLINE], body[MAXLINE];  
  
    /* ����HTTP��Ӧ���� */  
    sprintf(body, "<html><head><meta charset='UTF-8'><title>Tiny Error</title></head>");  
    sprintf(body, "%s<body bgcolor='#ffffff'>\r\n", body);  
    sprintf(body, "%s<h2>%s: %s</h2>\r\n", body, errnum, shortmsg);  
    sprintf(body, "%s<p>%s: %s</p>\r\n", body, longmsg, cause);  
    sprintf(body, "%s<hr><em>The Tiny Web server</em></body></html>\r\n", body);  /* <hr>��ǩ��ˮƽ�ָ��� */  
  
    /* ��ӡHTTP��Ӧ */  
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);  
    Rio_writen(fd, buf, strlen(buf));  
    sprintf(buf, "Content-type: text/html\r\n");  
    Rio_writen(fd, buf, strlen(buf));  
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));  
    Rio_writen(fd, buf, strlen(buf));  
    Rio_writen(fd, body, strlen(body));  
}  
  
  
void read_requesthdrs(rio_t *rp, int *length, int is_post_method) {  
    char buf[MAXLINE];  
    char *p;  
  
    Rio_readlineb(rp, buf, MAXLINE);  
    while (strcmp(buf, "\r\n")) {    /* �������\r\n�����ѭ��,��Ϊ����ͷ�Ե�����\r\nһ�н��� */  
        Rio_readlineb(rp, buf, MAXLINE);  
        if (is_post_method){  
            if(strncasecmp(buf,"Content-Length:",15)==0) {  
                p=&buf[15];  
                p+=strspn(p," \t");  
                *length=atol(p);  
            }  
        }  
        printf("%s", buf);  
        //printf("%d", *length);  
    }  
    //Rio_readlineb(rp, buf, MAXLINE);  
    //Rio_readlineb(rp, buf, MAXLINE);  
  
    return;  
}  
  
  
int parse_uri(char *uri, char *filename, char *cgiargs)  
{  
    char *ptr;  
  
    /* �����������̬�ļ�  */  
    if (!strstr(uri, "cgi-bin")) {  
        strcpy(cgiargs, "");  
        strcpy(filename, ".");  
        strcat(filename, uri);   /* ��filename��uri��������,���������filename��,����uriת��Ϊ���·�� */  
        if (uri[strlen(uri)-1] == '/')  
            strcat(filename, "index.html");  /* ���uri��"/"��β,��Ĭ�ϵ��ļ������غ��� */  
        return 1;  
    }  
    else {  
        ptr = index(uri, '?');  
        if (ptr) {  
            strcpy(cgiargs, ptr+1); /* ��ȡ������ */  
            *ptr = '\0';  
        }  
        else {  
            strcpy(cgiargs, "");  
        }  
        strcpy(filename, ".");  
        strcat(filename, uri); /* ��uriת��Ϊ���·�� */  
        return 0;  
    }  
}  
  
  
void serve_static(int fd, char *filename, int filesize, int is_head_method)  
{  
    int srcfd;  
    char *srcp, filetype[MAXLINE], buf[MAXLINE];  
  
    /* ������Ӧ��ͷ���ͻ��� */  
    get_filetype(filename, filetype);  
    sprintf(buf, "HTTP/1.0 200 OK\r\n");  
    sprintf(buf, "%sServer: Tiny Web Server\r\n",  buf);  
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);  
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);  
    //int fd2;                                                     11.6����չB  
    //fd2 = Open("test.txt", O_WRONLY, 0);                         11.6����չB  
    Rio_writen(fd, buf, strlen(buf));  
    //Rio_writen(fd2, buf, strlen(buf));                           11.6����չB  
    /* ������Ӧ������ͻ��� */  
    if (is_head_method)  
        return;  
  
    srcfd = Open(filename, O_RDONLY, 0);  
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);  
    Close(srcfd);  /* �����������ַ�ռ�Ͳ���Ҫread/write��,Ҳ�Ͳ���Ҫ�ļ������� */  
  
    //srcp = (char *)Malloc(filesize * sizeof(char));              11.9��  
    //Rio_readn(srcfd, srcp, filesize);                            11.9��  
    Rio_writen(fd, srcp, filesize);  
    //free(srcp);                                                  11.9��  
  
    //Rio_writen(fd2, srcp, filesize);                             11.6����չB  
    Munmap(srcp, filesize);  
}  
  
  
void get_filetype(char *filename, char *filetype)  
{  
    if (strstr(filename, ".html"))  
        strcpy(filetype, "text/html");  
    else if (strstr(filename, ".gif"))  
        strcpy(filetype, "image/gif");  
    else if (strstr(filename, ".jpg") || strstr(filename, ".jpeg"))  
        strcpy(filetype,  "image/jpeg");  
    else if (strstr(filename, ".mpg"))                        //11.6����չC  
        strcpy(filetype, "video/mpg");  
    else  
        strcpy(filetype, "text/plain");  
}  
  
  
void serve_dynamic(int fd, char *filename, char *cgiargs, int is_head_method)  
{  
    char buf[MAXLINE], *emptylist[] = {NULL};  
  
    /* ��ӡһ����HTTP��Ӧ��Ϣ */  
    sprintf(buf, "HTTP/1.0 200 OK\r\n");  
    sprintf(buf, "%sServer: Tiny Web Server\r\n",  buf);  
    Rio_writen(fd, buf, strlen(buf));  
  
    if (is_head_method)  
        return;  
  
    Signal(SIGCHLD, handler);  
  
    if (Fork() == 0) {  
        setenv("QUERY_STRING", cgiargs, 1);  /* ֻ�Ե�ǰ������Ч */  
        Dup2(fd, STDOUT_FILENO);  
        Execve(filename, emptylist, environ);  
  
    }  
    //Wait(NULL);  /* �����̼����ӽ��� */  
}  
  
/* ���CGI��ȡPOST�����Ĺؼ��������stdin��ָ��������, 
 * ����Dup2(fd, 0)�Ǵ��,CGI���򲻿��ܴ��׽������ж�ȡ, 
 * ��Ϊ�Ѿ�ȫ��������rio��������,����ֻ�ܴӻ�������ȡ 
 * ʣ�µ���������,��CGI������κ�ԭ���Ľ�����ϵ�Ӷ��� 
 * ������������?�𰸾��ǹܵ�,����dup2��������׼������ 
 * �ض��򵽹ܵ��Ķ���,����������������д���ܵ�д��.�� 
 * ���Һó�ʱ�����  
 */  
  
void post_dynamic(int fd, char *filename, int contentLength,rio_t *rp)  
{  
    char buf[MAXLINE], data[MAXLINE], length[32], *emptylist[] = {NULL};  
    int pipe_fd[2];  
  
    sprintf(length,"%d",contentLength);  
    memset(data,0,MAXLINE);  
  
    Signal(SIGCHLD, handler);  
  
     /* �����ܵ� */  
    if (pipe(pipe_fd) < 0) {  
        perror("pipe failed\n");  
        exit(errno);  
    }  
  
    if (Fork() == 0) {   /* �����ӽ��� */  
        Close(pipe_fd[0]);  /* �رչܵ����� */  
        Rio_readnb(rp, data, contentLength);  /* �ӻ������ж�ȡ����,��ʱ������������������ͷ�Ѿ� 
                                               * ��read_requesthdrs����������,ʣ�µ��������������� 
                                               * ���ǲ�Ҫʹ��Rio_readlineb,��Ϊ�������һ��ֻ��һ�� 
                                               * ��Rio_readnb��ȡȫ���������� 
                                               */  
        Rio_writen(pipe_fd[1], data, contentLength); /* ������д���ܵ� */  
        exit(0); /* ��һ���ӽ����������,�˳� */  
    }  
    else {  
        /* ��ӡһ����HTTP��Ӧ��Ϣ */  
        sprintf(buf, "HTTP/1.0 200 OK\r\n");  
        sprintf(buf, "%sServer: Tiny Web Server\r\n",  buf);  
        Rio_writen(fd, buf, strlen(buf));  
  
        if (Fork() == 0) {  
            Close(pipe_fd[1]);   /* �ӽ��̹رչܵ�д�� */  
            Dup2(pipe_fd[0], STDIN_FILENO); /* ���ӽ��̲��ٴӱ�׼�����ȡ,���Ǵӹܵ���ȡ */  
            Close(pipe_fd[0]);  /* �رչܵ����� */  
            setenv("CONTENT_LENGTH", length, 1);  
            Dup2(fd, STDOUT_FILENO); /* �ض����׼������ͻ���,�����ָ�������ӵ������� */  
            Execve(filename, emptylist, environ);  
        }  
        else {  
            /* �����̹رն��˺�д�� */  
            close(pipe_fd[0]);  
            close(pipe_fd[1]);  
        }  
    }  
  
}
