#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

int OthersPort = 6275;
int MyPort = 6225;

int State = 0; // 0 = Bekleme | 1 = Listener | 2 = Connection | -1 = Bitti
pthread_mutex_t mutexForState = PTHREAD_MUTEX_INITIALIZER;

void CheckStateAndHaltThreads(pthread_t readThread, pthread_t writeThread, int socketName, int mainSocketName) {
    do {
        if (State == -1) {
            pthread_kill(readThread, SIGINT);
            pthread_kill(writeThread, SIGINT);
            close(socketName);
            close(mainSocketName);
            break;
        } else {
            usleep(100000);
        }
    } while (1);
}

void* ReadFromOutside(void* args)
{
    char rec_msg[1024] = {0}; //Alinan Mesaj

    int *_sock = (int*)args;

    while(1==1)
    {
        memset(rec_msg, 0, sizeof(rec_msg));
        int readValue = read(*_sock, rec_msg, 1024);
        if(readValue == 0)
        {
            State = -1;
            break;
        }else if(readValue < 0)
        {
            continue;
        }

        printf("\n[Karsi Taraf]: %s \n", rec_msg);
    }
    close(*_sock);
    shutdown(*_sock, SHUT_RDWR);
    return NULL;
}

void* WriteToOutside(void* args){
    
    char sen_msg[1024] = {0}; //Alinan Mesaj

    int *_sock = (int*)args;

    while(1==1)
    {
        if(State==-1)
        {
            break;
        }

        memset(sen_msg, 0, sizeof(sen_msg));
        fgets(sen_msg, sizeof(sen_msg), stdin);
        
        if(State==-1)
        {
            break;
        }

        size_t len = strlen(sen_msg);
        if(len > 0 && sen_msg[len-1]=='\n')
        {
            sen_msg[len-1]='\0';
        }

        send(*_sock, sen_msg, strlen(sen_msg), 0);
    }

    return NULL;
}

void* ListenerSide(void* args)
{
    int listenerSock;
    struct sockaddr_in listenerAddr;

    listenerSock = socket(AF_INET, SOCK_STREAM, 0);
    if(listenerSock<0)
    {
        perror("\nSocket acilamadi");
        exit(EXIT_FAILURE);
    }

    listenerAddr.sin_family = AF_INET;
    listenerAddr.sin_addr.s_addr = INADDR_ANY;
    listenerAddr.sin_port = htons(MyPort);

    if(bind(listenerSock, (struct sockaddr*)&listenerAddr, sizeof(listenerAddr))< 0)
    {
        perror("\nBind basarisiz.");
        exit(EXIT_FAILURE);
    }

    if(listen(listenerSock, 2)<0)
    {
        perror("\nDinleme basarisiz.");
        exit(EXIT_FAILURE);
    }

    socklen_t listenerAddrLen = sizeof(listenerAddr);
    int acceptSock;
    
        pthread_t readFromOutsideThread;
        pthread_t writeToOutsideThread;

    while(State != 2)
    {
        acceptSock = accept(listenerSock, (struct sockaddr*)&listenerAddr, (socklen_t*)&listenerAddrLen);
        if(acceptSock<0)
        {
            perror("\nKabul edilmedi.");
            exit(EXIT_FAILURE);
        }

        pthread_mutex_lock(&mutexForState);
        if(State == 0)
        {
            State = 1;
            pthread_mutex_unlock(&mutexForState);
        }else{
            pthread_mutex_unlock(&mutexForState);
            return NULL;
        }

        //pthread_mutex_lock(&mutexForState);
        pthread_create(&readFromOutsideThread, NULL, ReadFromOutside, &acceptSock);
        pthread_create(&writeToOutsideThread, NULL, WriteToOutside, &acceptSock);
        //pthread_mutex_unlock(&mutexForState);
        
        CheckStateAndHaltThreads(readFromOutsideThread, writeToOutsideThread, acceptSock, listenerSock);

        pthread_join(writeToOutsideThread, NULL);
        pthread_join(readFromOutsideThread, NULL);
    }
        close(acceptSock);
        close(listenerSock);
}

void* ConnectionSide(void* args){
    struct sockaddr_in targetAddr;

    int clientSock = socket(AF_INET, SOCK_STREAM, 0);
    if(clientSock<0)
    {
        printf("\nSocket olusturulma hatasi algilandi");
        return NULL;
    }

    targetAddr.sin_family = AF_INET;
    targetAddr.sin_port = htons(OthersPort);

    if(inet_pton(AF_INET, "127.0.0.1", &targetAddr.sin_addr) <= 0)
    {
        printf("\nDesteklenmeyen veya gecersiz adres.");
        return NULL;
    }

    int connection;

    while(State != 1)
    {
        connection = connect(clientSock, (struct sockaddr*)&targetAddr, sizeof(targetAddr));
        if(connection<0)
        {
            printf("Karsi taraf bekleniyor...\n");
            usleep(700000);
            continue;
        }else{
            pthread_mutex_lock(&mutexForState);
            if(State==0)
            {
                State = 2;
                printf("Karsi tarafa baglanildi.\n");
                pthread_mutex_unlock(&mutexForState);
                break;
            }else{
                pthread_mutex_unlock(&mutexForState);
                return NULL;
            }
        }
                printf("Karsi tarafa baglanildi.\n");
    }

    pthread_t readFromOutsideThread;
    pthread_t writeToOutsideThread;
    
    pthread_mutex_lock(&mutexForState);
    if(State!=2)
    {
        pthread_mutex_unlock(&mutexForState);
        return NULL;
    }
    pthread_mutex_unlock(&mutexForState);

    pthread_create(&readFromOutsideThread, NULL, ReadFromOutside, &clientSock);
    pthread_create(&writeToOutsideThread, NULL, WriteToOutside, &clientSock);

    CheckStateAndHaltThreads(readFromOutsideThread, writeToOutsideThread, connection, clientSock);

    pthread_join(writeToOutsideThread, NULL);
    pthread_join(readFromOutsideThread, NULL);

    close(connection);
    close(clientSock);
}

int main(int argc, char *argv[])
{
    pthread_t ClientThread;
    pthread_t ListenerThread;

    pthread_create(&ClientThread, NULL, ConnectionSide, NULL);
    pthread_create(&ListenerThread, NULL, ListenerSide, NULL);

    pthread_join(ClientThread, NULL);
    pthread_join(ListenerThread, NULL);

    return 0;
}