/*
 * File: main.cpp
 * Author: rgarcia
 *
 * Created on 11. April 2014, 09:14
 */

#include <cstdlib>
#include <stdio.h>
#include <signal.h>
#include <iostream> // cin/cout
#include <string>
#include <cstring>
#include <sys/wait.h> // für wait(&status) sonst kommt fehler das status nicht initialisiert ist
#include <unistd.h>
#include <vector> //fork
#include <algorithm>
#include <iterator>

using namespace std;

/**
 * Liest Command und Parameter liste ein
 * @param com commando
 * @param par parameter liste
 * @return true => Vater prozess soll warten
 * false => Vater prozess soll nicht warten
 */

int childPid = 0;
int stoppedPid = 0;
vector<int> childCounter;

void zhandler(int sig) {
    //    while (waitpid(-1, NULL, WNOHANG) > 0) {
    //    }
    for (pid_t pid = waitpid(-1, NULL, WNOHANG); pid != 0 && pid != -1; pid = waitpid(-1, NULL, WNOHANG)) {
        for (int i = 0; i < childCounter.size(); ++i) {
            if (childCounter.at(i) == pid)
                childCounter.erase(childCounter.begin() + i);
        }
        kill(pid, SIGKILL);
    }


}

void handler_fg(int sig) {

    switch (sig) {
        case SIGINT:
            printf(" [caught SIGINT]\n");
            kill(childPid, SIGINT);
            for (int i = 0; i < childCounter.size(); ++i) {
                if (childCounter.at(i) == childPid)
                    childCounter.erase(childCounter.begin() + i);
            }
            break;
        case SIGTSTP:
            printf(" [caught SIGTSTP]\n");
            if (stoppedPid == 0) {
                kill(childPid, SIGSTOP);
                stoppedPid = childPid;
                bool found = false;
                for (int i = 0; i < childCounter.size(); ++i) {
                    if (childCounter.at(i) == childPid) {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    childCounter.push_back(childPid);
                else
                    found = false;
            }
            break;
    }
}

void handler() {
    cout << endl << "All done. Good bye!\n" << endl;
    exit(0);
}

bool read_command(char *com, char *par[]) {
    string tmp;
    int i = 0;
    char test;
    cout << "$ ";

    while (1) {
        test = cin.get(); //erstes char auslesen
        if (test == '\n') // wenn Return gedrückt wurde
            break;

        cin.putback(test); // zeichen war kein Return also zeichen zurück legen

        cin >> tmp;
        if (tmp == "logout") {//beenden mit logout
            if (childCounter.size() == 0)
                handler(); //exit meldung
            else
                cout << "Fehler es existieren noch " << childCounter.size() << " Kindprozess/e\n";
        }

        if (i == 0)
            strcpy(com, tmp.c_str()); // nur beim ersten mal (commando)

        par[i] = new char;
        strcpy(par[i], tmp.c_str());
        i++;
    }
    par[i] = NULL; //execvp need NULL Pointer


    if (i > 0) { //sonst geht return nicht
        if (strcmp(par[i - 1], "&") == 0) {
            delete par[i - 1];
            par[i - 1] = NULL;

            return true;
        } else
            return false;
    }
}

int main(int argc, char** argv) {

    int status; // enthält exit() code vom child
    char command[20] = ""; //enthält den befehl als ganzen
    char* parameters[60]; //enthält alle übergeben parameter werte bspw. ps -o %p %r %c %x %t...
    bool flagNotWait; //hintergrund prozess oder nicht

    for (int i = 0; i < 60; i++) // parameter werte wieder auf null setzen
        parameters[i] = NULL;
    int bgPid = 0;
    string bgCommand;

    // signal(SIGCHLD, zhandler);
    signal(SIGCHLD, SIG_IGN);
    cout << "Welcome to myshell. Exit with \"logout\"." << endl;


    while (1) {
        signal(SIGINT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);

        for (int i = 0; i < 60; i++) {
            if (parameters[i] != NULL)
                delete parameters[i]; // weil immer neue chars erstellt werden

            parameters[i] = NULL;
        }

        flagNotWait = read_command(command, parameters);

        if (!flagNotWait) {
            signal(SIGINT, handler_fg);
            signal(SIGTSTP, handler_fg);
        }

        string command_str = (const char*) command;
        if (command_str == "fg") {
            //            if (bgPid != 0) {
            //                childPid = bgPid;
            //                bgPid = 0;
            //                kill(childPid, SIGSTOP);
            //                signal(SIGINT, handler_fg);
            //                signal(SIGTSTP, handler_fg);
            //                kill(childPid, SIGCONT);               
            //                cout << bgCommand << endl;
            //                waitpid(childPid, &status, WUNTRACED);
            //            }
            if (stoppedPid != 0) { //für gestoppte Prozesse
                childPid = stoppedPid;
                stoppedPid = 0;
                signal(SIGINT, handler_fg);
                signal(SIGTSTP, handler_fg);
                kill(childPid, SIGCONT);
                waitpid(childPid, &status, WUNTRACED);
                if (childCounter.size() != 0) {
                    for (int i = 0; i < childCounter.size(); ++i) {
                        if (childCounter.at(i) == childPid)
                            childCounter.erase(childCounter.begin() + i);
                    }
                }

            }

        }
        if (command_str == "bg") {
            signal(SIGINT, SIG_IGN);
            signal(SIGTSTP, SIG_IGN);
            kill(childPid, SIGCONT);
            bool found = false;
            for (int i = 0; i < childCounter.size(); ++i) {
                if (childCounter.at(i) == childPid) {
                    found = true;
                    break;
                }
            }
            if (!found)
                childCounter.push_back(childPid);
            else
                found = false;
        }


        if (parameters[0] != NULL && command_str != "fg" && command_str != "bg") { //sonst speicherzugriffsfehler!
            if ((childPid = fork()) == -1) {
                cout << "can't fork\n";
            } else if (childPid == 0) { // child process
                //cout << "I am child, my pid is %d\n" + getpid();
                execvp(command, parameters);
                fprintf(stderr, (char*) (": command not found\n"));
                exit(0);
            } else { // parent process
                if (!flagNotWait)
                    waitpid(childPid, &status, WUNTRACED);
                else {
                    cout << "[" << childPid << "]" << endl;
                    bgPid = childPid;
                    bgCommand = (const char*) command;
                    childCounter.push_back(childPid);
                }

            }
        }
    }

    return 0;
}
