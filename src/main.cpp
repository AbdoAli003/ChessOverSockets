#include "../include/core.hpp"
#include <arpa/inet.h>
#include <ctime>
#include <iostream>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

char player_name[1024];
char guest_name[1024];
int timeout;

void Print_ASKII_Art() {
  cout << "    _____ _                          \n"
       << "   / ____| |                         \n"
       << "  | |    | |__   ___  ___ ___        \n"
       << "  | |    | '_ \\ / _ \\/ __/ __|       \n"
       << "  | |____| | | |  __/\\__ \\__ \\       \n"
       << "   \\_____|_| |_|\\___||___/___/       \n"
       << "    / __ \\                           \n"
       << "   | |  | |_   _____ _ __            \n"
       << "   | |  | \\ \\ / / _ \\ '__|           \n"
       << "   | |__| |\\ V /  __/ |              \n"
       << "   _\\____/  \\_/ \\___|_|      _       \n"
       << "  / ____|          | |      | |      \n"
       << " | (___   ___   ___| | _____| |_ ___ \n"
       << "  \\___ \\ / _ \\ / __| |/ / _ \\ __/ __|\n"
       << "  ____) | (_) | (__|   <  __/ |_\\__ \\\n"
       << " |_____/ \\___/ \\___|_|\\_\\___|\\__|___/\n";
}

int main() {
  Print_ASKII_Art();
  cout << "Enter your name: ";
  cin >> player_name;
  int op;

  while (true) {
    cout << "1- Search for players\n2- Invite a player\nEnter (1,2): ";
    cin >> op;
    Chess game;
    if (op == 1) {
      game.Search_for_players();
      recv(game.getSocket(), &timeout, sizeof(timeout), 0);
    } else if (op == 2) {
      int chk = -1;
      while (chk == -1) {
        cout << "Enter the player IPv4: " << endl;
        char IP[15];
        cin >> IP;
        cout << "Enter timeout in seconds: " << endl;
        cin >> timeout;
        chk = game.Invite_guest(IP);
      }
      send(game.getSocket(), &timeout, sizeof(timeout), 0);
    } else {
      cout << "Enter a valid choice!" << endl;
      continue;
    }

    game.init_board();
    game.draw_board();
    bool ord = (op == 1 ? 1 : 0);
    bool f = true;

    while (true) {
      fd_set readfds;
      time_t turn_start_time = time(nullptr);
      bool turn_completed = false;

      if (ord) {
        if (f) {
          king_status myst = game.update_status();
          if (myst == lose) {
            cout << "You lose!" << endl;
            game.sendmv({-1, -1}, {-1, -1});
            break;
          } else if (myst == win) {
            cout << "You win!" << endl;
            break;
          } else if (myst == draw) {
            cout << "draw" << endl;
            game.sendmv({-2, -2}, {-2, -2});
            break;
          }
          f = false;
        }

        cout << "Enter a move in form like: d2 d4 (Timeout : " << timeout
             << " s )" << endl;

        while (!turn_completed) {
          FD_ZERO(&readfds);
          FD_SET(STDIN_FILENO, &readfds);
          FD_SET(game.getSocket(), &readfds);

          // Calculate remaining time
          int remaining = timeout - (time(nullptr) - turn_start_time);
          if (remaining <= 0) {
            cout << "\nTime's up! You lose by timeout." << endl;
            game.sendmv({-1, -1}, {-1, -1});
            return 0;
          }

          // cout << "\rTime remaining: " << remaining << "s " << flush;

          struct timeval sel_timeout;
          sel_timeout.tv_sec = 1; // Check every second
          sel_timeout.tv_usec = 0;

          int ret = select(game.getSocket() + 1, &readfds, NULL, NULL,
                           (struct timeval *)&sel_timeout);

          if (ret < 0) {
            perror("select error");
            close(game.getSocket());
            exit(1);
          }

          if (ret > 0) {
            if (FD_ISSET(STDIN_FILENO, &readfds)) {
              char from[3], to[3];
              cin >> from >> to;
              int y1 = from[0] - 'a', x1 = (8 - (from[1] - '0'));
              int y2 = to[0] - 'a', x2 = (8 - (to[1] - '0'));

              if (game.make_move({x1, y1}, {x2, y2})) {
                game.draw_board();
                game.sendmv({x1, y1}, {x2, y2});
                ord = !ord;
                f = true;
                turn_completed = true;
              } else {
                cout << "Invalid move! " << remaining << "s remaining" << endl;
              }
            } else if (FD_ISSET(game.getSocket(), &readfds)) {
              char buffer[1024];
              game.recvmv();
              game.draw_board();
              ord = !ord;
              turn_completed = true;
            }
          }
        }
      } else {
        cout << guest_name << "'s turn. Waiting for move..." << endl;

        while (!turn_completed) {
          FD_ZERO(&readfds);
          FD_SET(game.getSocket(), &readfds);

          // Calculate remaining time
          int remaining = timeout - (time(nullptr) - turn_start_time);
          if (remaining <= 0) {
            cout << "\n" << guest_name << " timed out! You win!" << endl;
            return 0;
          }

          // cout << "\rTime remaining: " << remaining << "s " << flush;

          struct timeval sel_timeout;
          sel_timeout.tv_sec = 1;
          sel_timeout.tv_usec = 0;

          int ret = select(game.getSocket() + 1, &readfds, NULL, NULL,
                           (struct timeval *)&sel_timeout);

          if (ret < 0) {
            perror("select error");
            close(game.getSocket());
            exit(1);
          }

          if (ret > 0 && FD_ISSET(game.getSocket(), &readfds)) {
            char buffer[1024];
            game.recvmv();
            game.draw_board();
            ord = !ord;
            turn_completed = true;
          }
        }
      }
    }
  }
  return 0;
}

