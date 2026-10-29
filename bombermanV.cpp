#include <SDL2/SDL.h>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>

const int TILE_SIZE = 32; 
const int WIDTH = 15;      // 15 * 32 = 480 px
const int HEIGHT = 11;     // 11 * 32 = 352 px
const int UI_HEIGHT = 120; 

struct Player {
    int x, y;
    bool alive;
};

enum EnemyType { VERTICAL, HORIZONTAL };

struct Enemy {
    int x, y;
    int dx, dy; // Kierunek ruchu (-1, 0, 1)
    EnemyType type;
    bool alive;
    int moveTimer; // Opóźnienie ruchu bota
};

struct Bomb {
    int x, y;
    int timer; // Czas do wybuchu w klatkach
    bool active;
};

struct Flame {
    int x, y;
    int timer; // Czas trwania płomienia
};

void initGame(Player &p, std::vector<std::string> &mapa, std::vector<Enemy> &enemies) {
    p.x = 1;
    p.y = 1;
    p.alive = true;

    // # - ściana niezniszczalna, B - skrzynka, . - puste pole
    mapa = {
        "###############",
        "#...B...B...B.#",
        "#.#.B.#.B.#.B.#",
        "#BBB...BBB...B#",
        "#.#.B.#.B.#.B.#",
        "#...B...B...B.#",
        "#.#.B.#.B.#.B.#",
        "#BBB...BBB...B#",
        "#.#.B.#.B.#.B.#",
        "#...B...B...B.#",
        "###############"
    };

    // Tworzenie 3 botów w wolnych miejscach na mapie
    enemies.clear();
    enemies.push_back({13, 1, 0, 1, VERTICAL, true, 0});    // Bot w pionie (góra-dół)
    enemies.push_back({1, 9, 1, 0, HORIZONTAL, true, 0});   // Bot w poziomie (lewo-prawo)
    enemies.push_back({13, 9, 0, -1, VERTICAL, true, 0});   // Bot w pionie
}

void placeBomb(const Player &p, std::vector<Bomb> &bombs) {
    if (bombs.size() < 3) {
        bombs.push_back({p.x, p.y, 90, true}); // ~1.5 sekundy
    }
}

void triggerExplosion(Bomb &b, std::vector<std::string> &mapa, std::vector<Flame> &flames) {
    b.active = false;
    flames.push_back({b.x, b.y, 20});

    int dx[] = {0, 0, -1, 1};
    int dy[] = {-1, 1, 0, 0};

    for (int i = 0; i < 4; i++) {
        int nx = b.x + dx[i];
        int ny = b.y + dy[i];

        if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT) {
            if (mapa[ny][nx] == '#') continue; 

            flames.push_back({nx, ny, 20});

            if (mapa[ny][nx] == 'B') {
                mapa[ny][nx] = '.'; 
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;

    SDL_Window* window = SDL_CreateWindow("Bomberman SDL2 z Botami", 
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                          WIDTH * TILE_SIZE, (HEIGHT * TILE_SIZE) + UI_HEIGHT, 
                                          SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    srand(time(0));
    Player p;
    std::vector<std::string> mapa;
    std::vector<Bomb> bombs;
    std::vector<Flame> flames;
    std::vector<Enemy> enemies;

    initGame(p, mapa, enemies);

    bool running = true;
    SDL_Event e;

    // Przycisk kierunkowy
    SDL_Rect btnUp    = { 80,  360, 50, 40 };
    SDL_Rect btnLeft  = { 25,  405, 50, 40 };
    SDL_Rect btnDown  = { 80,  405, 50, 40 };
    SDL_Rect btnRight = { 135, 405, 50, 40 };

    // Przycisk BOMBA i WYJŚCIE
    SDL_Rect btnBomb  = { 320, 375, 70, 70 };
    SDL_Rect btnQuit  = { 425, 385, 45, 45 };

    while (running) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) running = false;
            
            else if (e.type == SDL_MOUSEBUTTONDOWN && p.alive) {
                int mx = e.button.x;
                int my = e.button.y;
                int nx = p.x, ny = p.y;

                if (mx >= btnUp.x && mx <= btnUp.x + btnUp.w && my >= btnUp.y && my <= btnUp.y + btnUp.h) ny--;
                else if (mx >= btnDown.x && mx <= btnDown.x + btnDown.w && my >= btnDown.y && my <= btnDown.y + btnDown.h) ny++;
                else if (mx >= btnLeft.x && mx <= btnLeft.x + btnLeft.w && my >= btnLeft.y && my <= btnLeft.y + btnLeft.h) nx--;
                else if (mx >= btnRight.x && mx <= btnRight.x + btnRight.w && my >= btnRight.y && my <= btnRight.y + btnRight.h) nx++;
                else if (mx >= btnBomb.x && mx <= btnBomb.x + btnBomb.w && my >= btnBomb.y && my <= btnBomb.y + btnBomb.h) {
                    placeBomb(p, bombs);
                } else if (mx >= btnQuit.x && mx <= btnQuit.x + btnQuit.w && my >= btnQuit.y && my <= btnQuit.y + btnQuit.h) {
                    running = false;
                }

                if (mapa[ny][nx] == '.') {
                    p.x = nx;
                    p.y = ny;
                }
            }
        }

        // --- RUCH BOTÓW ---
        for (auto &en : enemies) {
            if (!en.alive) continue;

            en.moveTimer++;
            if (en.moveTimer >= 25) { // Bot robi krok co 25 klatek (spowolnienie)
                en.moveTimer = 0;

                int targetX = en.x + en.dx;
                int targetY = en.y + en.dy;

                // Sprawdzamy czy bot może wykonać krok (czy przed nim jest wolna podłoga '.')
                if (targetX >= 0 && targetX < WIDTH && targetY >= 0 && targetY < HEIGHT && mapa[targetY][targetX] == '.') {
                    en.x = targetX;
                    en.y = targetY;
                } else {
                    // Odbicie od ściany/skrzynki — zmiana kierunku na przeciwny
                    en.dx = -en.dx;
                    en.dy = -en.dy;
                }
            }

            // Kolizja bota z graczem
            if (en.x == p.x && en.y == p.y) {
                p.alive = false;
            }
        }

        // --- LOGIKA BOMB I PŁOMIENI ---
        for (size_t i = 0; i < bombs.size();) {
            bombs[i].timer--;
            if (bombs[i].timer <= 0) {
                triggerExplosion(bombs[i], mapa, flames);
                bombs.erase(bombs.begin() + i);
            } else {
                i++;
            }
        }

        for (size_t i = 0; i < flames.size();) {
            flames[i].timer--;
            
            // Czy płomień zabija gracza?
            if (flames[i].x == p.x && flames[i].y == p.y) {
                p.alive = false;
            }

            // Czy płomień zabija bota?
            for (auto &en : enemies) {
                if (en.alive && flames[i].x == en.x && flames[i].y == en.y) {
                    en.alive = false;
                }
            }

            if (flames[i].timer <= 0) {
                flames.erase(flames.begin() + i);
            } else {
                i++;
            }
        }

        // --- RENDEROWANIE GRAFIKI ---
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);

        // 1. Rysowanie mapy
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                SDL_Rect rect = { x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE };
                char tile = mapa[y][x];
                
                if (tile == '#') {
                    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);   // Twarda ściana
                } else if (tile == 'B') {
                    SDL_SetRenderDrawColor(renderer, 160, 82, 45, 255);  // Skrzynka
                } else {
                    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);   // Podłoga
                }
                SDL_RenderFillRect(renderer, &rect);
            }
        }

        // 2. Rysowanie ŻÓŁTO-ZIELONYCH BOMB (Bardzo widoczne)
        SDL_SetRenderDrawColor(renderer, 180, 255, 0, 255);
        for (const auto& b : bombs) {
            SDL_Rect bRect = { b.x * TILE_SIZE + 4, b.y * TILE_SIZE + 4, TILE_SIZE - 8, TILE_SIZE - 8 };
            SDL_RenderFillRect(renderer, &bRect);
        }

        // 3. Rysowanie płomieni (Pomarańczowe)
        SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255);
        for (const auto& f : flames) {
            SDL_Rect fRect = { f.x * TILE_SIZE, f.y * TILE_SIZE, TILE_SIZE, TILE_SIZE };
            SDL_RenderFillRect(renderer, &fRect);
        }

        // 4. Rysowanie botów (Zielony = pionowy, Fioletowy = poziomy)
        for (const auto& en : enemies) {
            if (en.alive) {
                SDL_Rect enRect = { en.x * TILE_SIZE + 4, en.y * TILE_SIZE + 4, TILE_SIZE - 8, TILE_SIZE - 8 };
                if (en.type == VERTICAL) {
                    SDL_SetRenderDrawColor(renderer, 50, 205, 50, 255);  // Jasnozielony
                } else {
                    SDL_SetRenderDrawColor(renderer, 186, 85, 211, 255); // Fioletowy
                }
                SDL_RenderFillRect(renderer, &enRect);
            }
        }

        // 5. Rysowanie gracza (Niebieski, jeśli żyje)
        if (p.alive) {
            SDL_Rect playerRect = { p.x * TILE_SIZE + 2, p.y * TILE_SIZE + 2, TILE_SIZE - 4, TILE_SIZE - 4 };
            SDL_SetRenderDrawColor(renderer, 0, 150, 255, 255);
            SDL_RenderFillRect(renderer, &playerRect);
        }

        // 6. Rysowanie Pada Dotykowego
        SDL_Rect uiPanel = { 0, HEIGHT * TILE_SIZE, WIDTH * TILE_SIZE, UI_HEIGHT };
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        SDL_RenderFillRect(renderer, &uiPanel);

        // Krzyżak sterowania
        SDL_SetRenderDrawColor(renderer, 90, 90, 110, 255);
        SDL_RenderFillRect(renderer, &btnUp);
        SDL_RenderFillRect(renderer, &btnLeft);
        SDL_RenderFillRect(renderer, &btnDown);
        SDL_RenderFillRect(renderer, &btnRight);

        // Czerwony przycisk BOMB
        SDL_SetRenderDrawColor(renderer, 220, 40, 40, 255);
        SDL_RenderFillRect(renderer, &btnBomb);

        // Przycisk wyjścia
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_RenderFillRect(renderer, &btnQuit);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}