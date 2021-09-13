// Author: Jan Doležel, 3BIT
// C socket server zpracovavajici http dotazy
// 2020

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#define MAX_HTTP_BUFFER_SIZE 1024
#define MAX_COUNT_OF_PENDING_CONNECTIONS 10
#define HTTP_RESPONSE_MAX_SIZE 1024    // maximalni velikost http odpovedi
#define HTTP_ERROR_RESPONSE_SIZE 100
#define LINE_BUFSIZE 128
#define MAX_COUNT_OF_POST_QUESTIONS 100

// kontrola, jestli format retezce name odpovida formatu IP adresy (0, kdyz odpovida)
int is_it_ip(char *name)
{
    int ip[4];
    int ip_number = sscanf(name, "%d.%d.%d.%d",&ip[0], &ip[1], &ip[2], &ip[3]); // kontrola zda jsem jako parametr name dostal opravdu domain name a ne IP
    if (ip_number == 4 && ip[0] >=0 && ip[0] <= 255 && ip[1] >=0 && ip[1] <= 255 && ip[2] >=0 && ip[2] <= 255 && ip[3] >=0 && ip[3] <= 255) // je to IP adresa
    {
      return 0;
    }
    return 1;
}

// odstrani mezery ze stringu
void remove_spaces(char *string)
{
    for (int i = 0; i <= strlen(string); i++)
    {
        int string_position = 1;
        while (string[i] == 32)
        {
            string[i] = string[i + string_position];
            string[i + string_position] = 32;
            string_position++;
        }
    }
}

// odchyceni signalu na ukonceni serveru
void sighandler(int signum)
{
    if (signum == 2)
    {
        printf("Server killed\n");
        exit(0);
    }
}

// vytvoreni http odpovedi pro GET
void http_response_get_post(int client_socket, char *data)
{
    char http_response[HTTP_RESPONSE_MAX_SIZE];

    snprintf(http_response, HTTP_RESPONSE_MAX_SIZE, "HTTP/1.1 200 OK\n\
Content-Type: text/plain; charset=UTF-8\n\
Content-Length: %zu\n\
Connection: close\n\n\
%s\n", strlen(data)+1, data); // strlen(data)+1 kvuli odradkovani

    if (send(client_socket, http_response, strlen(http_response), 0) == -1)
        perror("send");
}

// vytvoreni http hlavicky s errorem 404 Not Found
void http_error_404_response(int client_socket)
{
    char http_response[HTTP_RESPONSE_MAX_SIZE];
    char *data = "404 Not Found";

    snprintf(http_response, HTTP_RESPONSE_MAX_SIZE, "HTTP/1.1 404 Not Found\n\
Content-Type: application/json\n\
Content-Length: %zu\n\
Connection: close\n\n\
%s\n", strlen(data)+1, data);

    if (send(client_socket, http_response, strlen(http_response), 0) == -1)
        perror("send");

    close(client_socket);
}

// vytvoreni http hlavicky s errorem 400 Bad Request
void http_error_400_response(int client_socket)
{
    char http_response[HTTP_RESPONSE_MAX_SIZE];
    char *data = "400 Bad Request";

    snprintf(http_response, HTTP_RESPONSE_MAX_SIZE, "HTTP/1.1 400 Bad Request\n\
Content-Type: application/json\n\
Content-Length: %zu\n\
Connection: close\n\n\
%s\n", strlen(data)+1, data);

    if (send(client_socket, http_response, strlen(http_response), 0) == -1)
        perror("send");

    close(client_socket);
}

// vytvoreni http hlavicky s errorem 405 Method Not Allowed
void http_error_405_response(int client_socket)
{
    char http_response[HTTP_RESPONSE_MAX_SIZE];
    char *data = "405 Method Not Allowed";

    snprintf(http_response, HTTP_RESPONSE_MAX_SIZE, "HTTP/1.1 405 Method Not Allowed\n\
Content-Type: application/json\n\
Content-Length: %zu\n\
Connection: close\n\n\
%s\n", strlen(data)+1, data);

    if (send(client_socket, http_response, strlen(http_response), 0) == -1)
        perror("send");

    close(client_socket);
}

// vytvoreni http hlavicky s errorem 500 Internal Server Error
void http_error_500_response(int client_socket)
{
    char http_response[HTTP_RESPONSE_MAX_SIZE];
    char *data = "500 Internal Server Error";

    snprintf(http_response, HTTP_RESPONSE_MAX_SIZE, "HTTP/1.1 500 Internal Server Error\n\
Content-Type: application/json\n\
Content-Length: %zu\n\
Connection: close\n\n\
%s\n", strlen(data)+1, data);

    if (send(client_socket, http_response, strlen(http_response), 0) == -1)
        perror("send");

    close(client_socket);
}

int main(int argc, char const *argv[])
{
    signal(SIGINT, sighandler);

    char *array_array[MAX_COUNT_OF_POST_QUESTIONS]; // pole retezcu dotazu POST
    int status, translate, status2;
    char port[10];  // 0-65535
    struct addrinfo hints;  // pomocna struktura pro nastaveni getaddrinfo
    struct addrinfo *servinfo;  // struktura vysledku getaddrinfo pro server
    struct addrinfo *question_servinfo; // struktura vysledku getaddrinfo pro hosta
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    int server_socket, client_socket;
    char http_question_buffer[MAX_HTTP_BUFFER_SIZE];
    struct sockaddr visitor_addr;
    socklen_t addr_size;
    int backlog = MAX_COUNT_OF_PENDING_CONNECTIONS;
    //char content[HTTP_RESPONSE_MAX_SIZE];

    //zpracovani argumentu
    if (argc > 2)
    {
        fprintf(stderr, "Invalid count of arguments\n");
        return 1;
    }
    else if (argc == 1)
    {
        fprintf(stderr, "No argument given (port=xxxxx)\n");
        return 1;
    }
    else // spravny pocet argumentu
    {
        sscanf(argv[1],"%s", port);
        if ((strtod(argv[1], NULL)) > 65535 || (strtod(argv[1], NULL)) < 0)
        {
            fprintf(stderr, "Invalid port, can be 0-65535\n");
            return 1;
        }
    }

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    // int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
    if ((status = getaddrinfo(NULL, port, &hints, &servinfo)) != 0)
    {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
    }
    // int socket(int domain, int type, int protocol);
    server_socket = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);  // vytvoreni socketu
    if(server_socket == -1)
    {
        //fprintf(stderr, "Socket function error\n");
        perror("socket");
        exit(1);
    }
    // int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
    if((bind(server_socket, servinfo->ai_addr, servinfo->ai_addrlen)) == -1)    // navazani socketu na port
    {
        //fprintf(stderr, "Bind function error\n");
        perror("bind");
        exit(1);
    }
    // int listen(int sockfd, int backlog);
    if((listen(server_socket, backlog)) == -1)    // cekani na navazani spojeni
    {
        //fprintf(stderr, "Listen function error\n");
        perror("listen");
        exit(1);
    }

    while (1) // smycka prijimani dat http protokolem
    {
        // int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
        if ((client_socket = accept(server_socket, &visitor_addr, &addr_size)) == -1)   // prijmuti prvniho spojeni ve fronte (struct sockaddr *)
        {
            perror("accept");
            continue;
        }
        // ssize_t recv(int sockfd, void *buf, size_t len, int flags);
        recv(client_socket, http_question_buffer, MAX_HTTP_BUFFER_SIZE, 0);  // precteni prijmuteho http dotazu
        char question_type[20];
        char url[100];
        if ((sscanf(http_question_buffer, "%s %s", question_type, url)) != 2)    // kdyby byl spatny format dotazu (totalne, nemelo by nastat!)
        {
            http_error_500_response(client_socket);
            continue;
        }
        if (strcmp(question_type, "GET") == 0)
        {
            char content[HTTP_RESPONSE_MAX_SIZE];
            char name[100];
            char type[100];
            char *token;
            char *token1;
            char *token2; // pro kontrolu, kdyby bylo zadano vice parametru -> Bad Request podle odpovedi na foru
            const char s[2] = "&";
            token = strtok(url, s);
            token1 = strtok(NULL, s);
            token2 = strtok(NULL, s);
            if (token2 != NULL) // zadano vice parametru, nez name + type
            {
                http_error_400_response(client_socket);
                continue;
            }
            if ((token == NULL) || (token1 == NULL))    // chybi /resolve... nebo &type=...
            {
                http_error_400_response(client_socket);
                continue;
            }
            strcpy(name, token);
            strcpy(type, token1);
            if ((sscanf(name, "/resolve?name=%s", name)) != 1) // chybi domain name / IP adresa
            {
                http_error_400_response(client_socket);
                continue;
            }
            if ((sscanf(type, "type=%s", type)) != 1) // chybi typ A/PTR
            {
                http_error_400_response(client_socket);
                continue;
            }
            if (strcmp(type, "A") != 0 && strcmp(type, "PTR") != 0) // jiny typ prekladu, nez podporujeme
            {
                // 400 Bad Request
                http_error_400_response(client_socket);
                continue;
            }
            if (strcmp(type, "A") == 0) // domain name -> IP
            {
                if (is_it_ip(name) == 0) // je to IP
                {
                    http_error_400_response(client_socket);
                    continue;
                }
                // int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
                if ((status2 = getaddrinfo(name, port, &hints, &question_servinfo)) != 0)
                {
                    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status2));
                    http_error_404_response(client_socket);
                    continue;
                }
                if ((translate = getnameinfo(question_servinfo->ai_addr, sizeof(*(question_servinfo->ai_addr)), hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST)) != 0)
                {
                    fprintf(stderr, "getnameinfo error: %s\n", gai_strerror(translate));
                    http_error_404_response(client_socket);
                    continue;
                }
                snprintf(content, HTTP_RESPONSE_MAX_SIZE, "%s:%s=%s", name, type, hbuf);
                http_response_get_post(client_socket, content);
            }
            else if (strcmp(type, "PTR") == 0) // IP -> domain name
            {
                if (is_it_ip(name) == 1) // neni to IP, zadano domain name namisto
                {
                    http_error_400_response(client_socket);
                    continue;
                }
                // int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
                if ((status2 = getaddrinfo(name, port, &hints, &question_servinfo)) != 0)
                {
                    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status2));
                    http_error_404_response(client_socket);
                    continue;
                }
                if ((translate = getnameinfo(question_servinfo->ai_addr, sizeof(*(question_servinfo->ai_addr)), hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NOFQDN)) != 0)
                {
                    fprintf(stderr, "getnameinfo error: %s\n", gai_strerror(translate));
                    http_error_404_response(client_socket);
                    continue;
                }
                snprintf(content, HTTP_RESPONSE_MAX_SIZE, "%s:%s=%s", name, type, hbuf);
                http_response_get_post(client_socket, content);
            }
        }
        else if (strcmp(question_type, "POST") == 0)
        {
            if (strcmp(url, "/dns-query") != 0) // url neni spravne
            {
                http_error_400_response(client_socket);
                continue;
            }
            // jdeme parsovat POST request
            char line[LINE_BUFSIZE];
            char content[HTTP_RESPONSE_MAX_SIZE];
            content[0] = '\0';
            int line_position = 0;
            int buffer_position = 0;
            int data_length;
            while (1)   // zjisteni delky dat
            {
                while (http_question_buffer[buffer_position] != 13)
                {
                    line[line_position] = http_question_buffer[buffer_position];
                    line_position++;
                    buffer_position++;
                }
                line[line_position] = '\0'; // ukonceni radku
                if ((sscanf(line, "Content-Length: %d", &data_length)) == 1)
                {
                    break;
                }
                line_position = 0;
                buffer_position = buffer_position + 2;   // posun za CR LF
            }
            // mame zjistene data_length, budeme cist buffer do te doby, nez narazime na prazdny radek, pak se posuneme za nej(zde zacinaji data)
            buffer_position = 0;
            while (!(http_question_buffer[buffer_position] == 13 && http_question_buffer[buffer_position+1] == 10 && http_question_buffer[buffer_position+2] == 13 && http_question_buffer[buffer_position+3] == 10))
            {
                buffer_position++;
            }
            buffer_position = buffer_position + 4; // tady zacinaji data
            // od buffer_position ted budu cist radky do max velikosti data_length
            line_position = 0;
            int array_array_position = 0;
            for (int i = buffer_position; i < (buffer_position + data_length); i++)
            {
                if (http_question_buffer[i] == 10) // LF
                {
                    line[line_position] = '\0';
                    remove_spaces(line); // odstrani mezery z radku dotazu POST (na zaklade odpovedi na foru)
                    // zpracování radku POST, alokace radku a jeho ulozeni do pole poli
                    array_array[array_array_position] = (char*)(malloc(sizeof(char)*LINE_BUFSIZE));
                    if (array_array[array_array_position] == NULL) // nepovedl se malloc
                    {
                        http_error_500_response(client_socket);
                        fprintf(stderr, "Malloc error\n" );
                        exit(1);
                    }
                    strcpy(array_array[array_array_position], line);
                    array_array_position++;
                    line_position = 0;
                    continue;
                }
                line[line_position] = http_question_buffer[i];
                line_position++;
            }
            array_array[array_array_position] = NULL;  // uzavreni POST dotazu, ukonceni pole poli, aby s nim bylo mozne pracovat
            // zpracovaní postu v array_array, vezmeme radek pole poli -> rozparsujeme na name + type -> prekladame IP/ domain name
            int error_flag = 0; // kdyyb byl spatne zadany dotaz -> vede na 400
            for (int i = 0; array_array[i] != NULL; i++)
            {
                char *name;
                char *type;
                char content_tmp[HTTP_RESPONSE_MAX_SIZE];   // vysledek prekladu jednoho radku
                name = strtok(array_array[i], ":");
                if (name == NULL)
                {
                    error_flag = 1;
                    continue;
                }
                type = strtok(NULL, ":");
                if (type == NULL)
                {
                    error_flag = 1;
                    continue;
                }
                if (!((strcmp(type, "A") == 0) || (strcmp(type, "PTR") == 0))) // musime zjistit dopredu kvuli nastaveni error_flagu
                {
                    error_flag = 1;
                    continue;
                }
                if (strcmp(type, "A") == 0) // domain name -> IP
                {
                    if (is_it_ip(name) == 0) // je to IP
                    {
                        error_flag = 1;
                        continue;
                    }
                    // int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
                    if ((status2 = getaddrinfo(name, port, &hints, &question_servinfo)) != 0)
                    {
                        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status2));
                        continue;
                    }
                    if ((translate = getnameinfo(question_servinfo->ai_addr, sizeof(*(question_servinfo->ai_addr)), hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST)) != 0)
                    {
                        fprintf(stderr, "getnameinfo error: %s\n", gai_strerror(translate));
                        continue;
                    }
                    if (array_array[i+1] == NULL)
                    {
                        snprintf(content_tmp, HTTP_RESPONSE_MAX_SIZE, "%s:%s=%s", name, type, hbuf);
                        strcat(content, content_tmp);
                    }
                    else
                    {
                        snprintf(content_tmp, HTTP_RESPONSE_MAX_SIZE, "%s:%s=%s\n", name, type, hbuf);
                        strcat(content, content_tmp);
                    }
                }
                else if (strcmp(type, "PTR") == 0) // IP -> domain name
                {
                    if (is_it_ip(name) == 1) // neni to IP, zadano domain name namisto
                    {
                        error_flag = 1;
                        continue;
                    }
                    // int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
                    if ((status2 = getaddrinfo(name, port, &hints, &question_servinfo)) != 0)
                    {
                        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status2));
                        continue;
                    }
                    if ((translate = getnameinfo(question_servinfo->ai_addr, sizeof(*(question_servinfo->ai_addr)), hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NOFQDN)) != 0)
                    {
                        fprintf(stderr, "getnameinfo error: %s\n", gai_strerror(translate));
                        continue;
                    }
                    if (array_array[i+1] == NULL)
                    {
                        snprintf(content_tmp, HTTP_RESPONSE_MAX_SIZE, "%s:%s=%s", name, type, hbuf);
                        strcat(content, content_tmp);
                    }
                    else
                    {
                        snprintf(content_tmp, HTTP_RESPONSE_MAX_SIZE, "%s:%s=%s\n", name, type, hbuf);
                        strcat(content, content_tmp);
                    }
                }
                else if (!(strcmp(type, "PTR") == 0) && !(strcmp(type, "A") == 0))    // jiny typ nez A/PTR, zaznam bude pouze vynechan
                {
                    error_flag = 1;
                    continue;
                }
            }
            if (error_flag == 0)    // nedoslo k chybe formatu
            {
                if (content[0] == '\0') // zadny z dotazu nebyl nalezen
                {
                    http_error_404_response(client_socket);
                }
                else
                {
                    http_response_get_post(client_socket, content);
                }
            }
            else
            {
                http_error_400_response(client_socket);
            }
            // dealokace pole array_array s radky dotazu POST
            for (int i = 0; array_array[i] != NULL; i++)
            {
                free(array_array[i]);
            }
        }
        else
        {
            // 405 Method Not Allowed
            http_error_405_response(client_socket);
            continue;
        }
        close(client_socket);  // parent doesn't need this
    }
    close(server_socket);
    freeaddrinfo(servinfo); // free the linked-list
    return 0;
}
