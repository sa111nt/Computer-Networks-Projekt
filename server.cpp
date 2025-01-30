#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <string>
#include <iostream>
#include <fstream>
#include <thread>
#include <unordered_set>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>
#include <fstream>
#include <map>
#include <algorithm>
#include <arpa/inet.h>
#include <error.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <cstring>
#include <chrono>
#include <cmath>

class Player;

std::vector<std::string> Countries;
std::vector<std::string> Cities;
std::unordered_set<Player*> players;
int serverFd;
int epollFd;
bool gameIsStarted = false;
int countSeconds = 0;
char randomLetter;


std::vector<std::string> readCategoriesFromFile(std::string name);
void sendMessageToOnePlayer(int fd, char * buffer, int count);
bool verifyNickName(int fd, char * buffer);
void sendMessagetoAllActivePlayers(const char * buffer, int count);
char* removeFirstChar(const char* buffer);
bool checkIfAllAnswered();
int countActivePlayers();
void mainTimer();
void manageMessages();
void verifyAllAnswers();
void startGame();
void sendPlayersInfo();



uint16_t readPort(char * txt);
void setReuseAddr(int sock);
void ctrl_c(int);


struct eventManager {
    virtual ~eventManager(){}
    virtual void manageEvent(uint32_t events) = 0;
};

class Player : public eventManager {
    int _fd;

public:
    char* nickname;
    char* answer;
    float scores;
    bool isActive;
    bool answered;
    std::string receivedData;
    std::chrono::steady_clock::time_point answerTime;


    Player(int fd) : _fd(fd) {
        epoll_event ee {EPOLLIN|EPOLLRDHUP, {.ptr=this}};
        epoll_ctl(epollFd, EPOLL_CTL_ADD, _fd, &ee);
        scores = 0.0;
        isActive = false;
        answered = false;
        answer = nullptr;
        nickname = nullptr;
        receivedData = "";
    }

    virtual void manageEvent(uint32_t events) override {
        if(events & EPOLLIN) {
            char buffer[256];
            bzero(buffer, 256);
            ssize_t count = read(_fd, buffer, 256);
            if(count > 0){
                this->receivedData+=buffer;
            }
            else
                events |= EPOLLERR;
        }
        if(events & ~EPOLLIN){
            remove();
        }
    }
    void write(const char * buffer, int count){
        if(count != ::write(_fd, buffer, count)){
            remove();
        }
    }
    void remove() {
        printf("Usuwanie gracza %d\n", _fd);
        players.erase(this);
        delete this;

        if(countActivePlayers()<2){
            gameIsStarted = false;
        }
        sendPlayersInfo();
    }

    int fd() const {return _fd;}

    virtual ~Player(){
        epoll_ctl(epollFd, EPOLL_CTL_DEL, _fd, nullptr);
        shutdown(_fd, SHUT_RDWR);
        close(_fd);
    }

};

class : eventManager {
    public:
    virtual void manageEvent(uint32_t events) override {
        if(events & EPOLLIN){
            sockaddr_in PlayerAddr{};
            socklen_t PlayerAddrSize = sizeof(PlayerAddr);
            
            auto playerFd = accept(serverFd, (sockaddr*) &PlayerAddr, &PlayerAddrSize);
            if(playerFd == -1) error(1, errno, "accept error");
            
            printf("Nowe połączenie gracza: %s:%hu (fd: %d)\n", inet_ntoa(PlayerAddr.sin_addr), ntohs(PlayerAddr.sin_port), playerFd);
            
            players.insert(new Player(playerFd));
        }
        if(events & ~EPOLLIN){
            error(0, errno, "Event %x on server socket", events);
        }
    }
} serverManager;


int main(int argc, char ** argv){
    if(argc != 2) error(1, 0, "Required 1 argument");
    auto port = readPort(argv[1]);

    Countries = readCategoriesFromFile("categories/cities.txt");
    Cities = readCategoriesFromFile("categories/countries.txt");

    serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if(serverFd == -1) error(1, errno, "socket error");
    
    signal(SIGINT, ctrl_c);
    signal(SIGPIPE, SIG_IGN);
    
    setReuseAddr(serverFd);

    sockaddr_in serverAddr{
    .sin_family = AF_INET,
    .sin_port = htons((short)port),
    .sin_addr = {INADDR_ANY},
    .sin_zero = {0} 
    };
    
    int res = bind(serverFd, (sockaddr*) &serverAddr, sizeof(serverAddr));
    if(res) error(1, errno, "bind error");
    
    res = listen(serverFd, 1);
    if(res) error(1, errno, "listen error");


    epollFd = epoll_create1(0);
    
    epoll_event ee {EPOLLIN, {.ptr=&serverManager}};
    epoll_ctl(epollFd, EPOLL_CTL_ADD, serverFd, &ee);
    

    std::thread t1(manageMessages);
    std::thread t(mainTimer);

    while(true){
        if(-1 == epoll_wait(epollFd, &ee, 1, -1)) {
            error(0,errno,"epoll_wait error");
        }
        ((eventManager*)ee.data.ptr)->manageEvent(ee.events);
    }
}

void manageMessages(){
    char buf[256];
    char msg[256];
    bzero(buf, 256);
    bzero(msg, 256);

    while(1){
        auto it = players.begin();
        while(it!=players.end()){
            Player * player = *it;
            it++;
            if(player->receivedData.size()>0){
                for(int i=0; i<int(player->receivedData.size()); i++){
                    if(player->receivedData[i]!='\n'){
                        sprintf(buf, "%c", player->receivedData[i]);
                        strcat(msg, buf);
                    }
                    else if(player->receivedData[i]=='\n'){
                        if(msg[0]=='N'){
                            if(verifyNickName(player->fd(), removeFirstChar(msg))){
                                player->nickname=removeFirstChar(msg);
                                strcpy(buf, "valid\n");
                                sendMessageToOnePlayer(player->fd(), buf, strlen(buf));
                                player->isActive = true;
                                sendPlayersInfo();

                                if(!gameIsStarted){
                                    if(countActivePlayers()>1){
                                        startGame();
                                    }
                                }
                            }
                            else{
                                strcpy(buf, "invalid\n");
                                sendMessageToOnePlayer(player->fd(), buf, strlen(buf));
                            }
                        }
                        if(msg[0]=='A'){
                            player->answer=removeFirstChar(msg);
                            player->answered=true;
                            player->answerTime = std::chrono::steady_clock::now();
                            sendPlayersInfo();
                            if(checkIfAllAnswered()){
                                verifyAllAnswers();
                                startGame();
                                sendPlayersInfo();
                            }
                        }
                        player->receivedData.erase(0, i+1);
                        bzero(msg, 256);
                    }

                }
            }
            bzero(msg, 256);
        }
    }
}

bool verifyNickName(int fd, char * buffer){
    auto it = players.begin();
    while(it!=players.end()){
        Player * player = *it;
        it++;
        if(player->fd()!=fd && player->nickname!=0){
            if(strcmp(player->nickname, buffer)==0){
            return false;
            }
        }
    }
    return true;
}


void verifyAllAnswers(){

    std::vector<std::string> answers;
    std::string buf = "";
    std::string answer;

    std::vector<Player *> answeringPlayers;
    std::map<std::string, int> countCountryRepeats;
    std::map<std::string, int> countCityRepeats;


    auto iterator = players.begin();
    while (iterator != players.end()) {
        Player *player = *iterator;
        iterator++;
        if (player->answer != nullptr) {
            answeringPlayers.push_back(player);
        }
    }

    std::sort(answeringPlayers.begin(), answeringPlayers.end(), [](Player *a, Player *b) {
        return a->answerTime < b->answerTime;
    });


    int rank = 0;
    for (Player * player: answeringPlayers) {
        int speedscores = 0;
        if (rank == 0) speedscores = 15;
        else if (rank == 1) speedscores = 10;
        else if (rank == 2) speedscores = 5;

        player->scores += speedscores;

        rank++;
    }

    
    auto iterator2 = players.begin();
    while(iterator2!=players.end()){
        Player * player = *iterator2;
        iterator2++;
        if(player->answer==0){
            player->scores+=0;
        }
        else if(player->answer!=0){
            answer = player->answer;

            int answerSize = answer.length();
            for(int i=0; i<answerSize; i++){
                if(player->answer[i]!=';'){
                    buf+=player->answer[i];

                }
                else if(player->answer[i]==';'){
                    answers.push_back(buf);
                    buf="";
                }
            }
            answers.push_back(buf);

            buf="";

            if(answers[0][0]==randomLetter){
                for (std::string a : Countries) {
                    if(answers[0] == a){
                        if (countCountryRepeats.count(a) == 0) {
                            countCountryRepeats[a] = 1;
                        } 
                        else {
                            countCountryRepeats[a]++;
                        }
                    }
                }
            }

            if(answers[1][0]==randomLetter){
                for (std::string &a : Cities) {
                    if(answers[1] == a){
                        if (countCityRepeats.count(a) == 0) {
                            countCityRepeats[a] = 1;
                        } 
                        else {
                            countCityRepeats[a]++;
                        }
                    }
                }
            }

            answers.clear();
            
        }
    }

    auto iterator3 = players.begin();
    while(iterator3!=players.end()){
        Player * player = *iterator3;
        iterator3++;
        if(player->answer!=0){
            answer = player->answer;
            int answerSize = answer.length();
            for(int i=0; i<answerSize; i++){
                if(player->answer[i]!=';'){
                    buf+=player->answer[i];

                }
                else if(player->answer[i]==';'){
                    answers.push_back(buf);
                    buf="";
                }
            }
            answers.push_back(buf);
            buf="";

            if (countCountryRepeats.count(answers[0])) {
                int count = countCountryRepeats[answers[0]];
                player->scores += (count == 1) ? (countCountryRepeats.size() == 1 ? 15 : 10) : 5;
            }

            if (countCityRepeats.count(answers[1])) {
                int count = countCityRepeats[answers[1]];
                player->scores += (count == 1) ? (countCityRepeats.size() == 1 ? 15 : 10) : 5;
            }


            answers.clear();
        }
    }

}

void startGame(){
    char msg[256];
    char buf[256];
    bzero(buf, 256);
    bzero(msg, 256);

    auto it = players.begin();
    while(it!=players.end()){
        Player * player = *it;
        it++;
        player->answered=false;
        player->answer=nullptr;
    }

    srand(time(0));
    randomLetter = 'A' + rand() % 26; 
    sprintf(msg, "start%c\n", randomLetter);

    sendMessagetoAllActivePlayers(msg, strlen(msg));
    gameIsStarted = true;
    countSeconds = 0;


}


void sendPlayersInfo() { 
    std::string msg;

    for (auto player : players) {
        if (player->isActive) {
            msg += player->nickname;
            msg += " " + std::to_string(player->scores) + " ";
            msg += (player->answered ? "Tak;" : "Nie;");
        }
    }
    msg += "\n";
    sendMessagetoAllActivePlayers(msg.c_str(), msg.length());
}



void mainTimer() {
    while(1){
        if(gameIsStarted){
            std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::seconds(1));
            countSeconds++;
            if(countSeconds==30){
                verifyAllAnswers();
                startGame();
                sendPlayersInfo();
           }
        }
    }
}



std::vector<std::string> readCategoriesFromFile(std::string name) {
    std::vector<std::string> strings;
    std::ifstream file(name);
    if (!file.is_open()) {
        std::cerr << "Błąd przy otwarciu pliku " << name << std::endl;
        return strings;
    }
    std::string line;
    while (file >> line) {
        strings.push_back(line);
    }
    file.close();
    return strings;
}


bool checkIfAllAnswered() {
    return std::all_of(players.begin(), players.end(), [](Player* p) {
        return p->answered;
    });
}

void sendMessageToOnePlayer(int fd, char* buffer, int count) {
    for (Player* player : players) {
        if (player->fd() == fd) {
            player->write(buffer, count);
            break; 
        }
    }
}


void sendMessagetoAllActivePlayers(const char* buffer, int count) {
    for (Player* player : players) {
        if (player->isActive) {
            player->write(buffer, count);
        }
    }
}

int countActivePlayers() {
    int isActive = 0;
    for (Player* player : players) {
        if (player->isActive) {
            isActive++;
        }
    }
    return isActive;
}

char* removeFirstChar(const char* buffer) {
    int len = strlen(buffer);
    char* buffer2 = new char[len - 1];

    int i, j = 0;
    for (i = 0; i < len; i++) {
        if (i == 0) continue;
        buffer2[j++] = buffer[i];
    }
    buffer2[j] = '\0';
    return buffer2;
}

uint16_t readPort(char * txt){
    char * ptr;
    auto port = strtol(txt, &ptr, 10);
    if(*ptr!=0 || port<1 || (port>((1<<16)-1))) error(1,0,"forbidden argument %s", txt);
    return port;
}

void setReuseAddr(int sock){
    const int one = 1;
    int res = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if(res) error(1,errno, "setsockopt error");
}

void ctrl_c(int){
    for(Player * player : players)
        delete player;
    printf("Zakończenie gry\n");
    close(serverFd);
    exit(0);
}
