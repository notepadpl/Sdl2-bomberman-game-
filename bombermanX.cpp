#include <SDL2/SDL.h>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

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
    int moveTimer;
};

struct Bomb {
    int x, y;
    int timer; // Czas do wybuchu w klatkach
    bool active;
};

struct Flame {
    int x, y;
    int timer;
};

// Globalny stan gry (potrzebny dla pętli Emscriptena)
struct GameState {
    Player p;
    std::vector<std::string> mapa;
    std::vector<Bomb> bombs;
    std::vector<Flame> flames;
    std::vector<Enemy> enemies;
    SDL_Renderer* renderer;
    SDL_Rect btnUp, btnLeft, btnDown, btnRight, btnBomb, btnQuit;
    bool running;
};

GameState* globalGame = nullptr;

void initGame(Player &p, std::vector<std::string> &mapa, std::vector<Enemy> &enemies) {
    p.x = 1;
    p.y = 1;
    p.alive = true;

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

    enemies.clear();
    enemies.push_back({13, 1, 0, 1, VERTICAL, true, 0});
    enemies.push_back({1, 9, 1, 0, HORIZONTAL, true, 0});
    enemies.push_back({13, 9, 0, -1, VERTICAL, true, 0});
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

// --- GŁÓWNA FUNKCJA KLATKI (WYWOŁYWANA W KAŻDYM CYKLU PRZEGLĄDARKI) ---
void main_loop_iteration() {
    GameState &g = *globalGame;
    if (!g.running) return;

    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            g.running = false;
        } 
        else if (e.type == SDL_KEYDOWN && g.p.alive) {
            int nx = g.p.x, ny = g.p.y;
            switch (e.key.keysym.sym) {
                case SDLK_w: case SDLK_UP:    ny--; break;
                case SDLK_s: case SDLK_DOWN:  ny++; break;
                case SDLK_a: case SDLK_LEFT:  nx--; break;
                case SDLK_d: case SDLK_RIGHT: nx++; break;
                case SDLK_SPACE: placeBomb(g.p, g.bombs); break;
                case SDLK_q: case SDLK_ESCAPE: g.running = false; break;
            }
            if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT && g.mapa[ny][nx] == '.') {
                g.p.x = nx;
                g.p.y = ny;
            }
        }
        else if (e.type == SDL_MOUSEBUTTONDOWN && g.p.alive) {
            int mx = e.button.x;
            int my = e.button.y;
            int nx = g.p.x, ny = g.p.y;

            if (mx >= g.btnUp.x && mx <= g.btnUp.x + g.btnUp.w && my >= g.btnUp.y && my <= g.btnUp.y + g.btnUp.h) ny--;
            else if (mx >= g.btnDown.x && mx <= g.btnDown.x + g.btnDown.w && my >= g.btnDown.y && my <= g.btnDown.y + g.btnDown.h) ny++;
            else if (mx >= g.btnLeft.x && mx <= g.btnLeft.x + g.btnLeft.w && my >= g.btnLeft.y && my <= g.btnLeft.y + g.btnLeft.h) nx--;
            else if (mx >= g.btnRight.x && mx <= g.btnRight.x + g.btnRight.w && my >= g.btnRight.y && my <= g.btnRight.y + g.btnRight.h) nx++;
            else if (mx >= g.btnBomb.x && mx <= g.btnBomb.x + g.btnBomb.w && my >= g.btnBomb.y && my <= g.btnBomb.y + g.btnBomb.h) {
                placeBomb(g.p, g.bombs);
            } else if (mx >= g.btnQuit.x && mx <= g.btnQuit.x + g.btnQuit.w && my >= g.btnQuit.y && my <= g.btnQuit.y + g.btnQuit.h) {
                g.running = false;
            }

            if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT && g.mapa[ny][nx] == '.') {
                g.p.x = nx;
                g.p.y = ny;
            }
        }
    }

    // --- RUCH BOTÓW ---
    for (auto &en : g.enemies) {
        if (!en.alive) continue;

        en.moveTimer++;
        if (en.moveTimer >= 25) { 
            en.moveTimer = 0;

            int targetX = en.x + en.dx;
            int targetY = en.y + en.dy;

            if (targetX >= 0 && targetX < WIDTH && targetY >= 0 && targetY < HEIGHT && g.mapa[targetY][targetX] == '.') {
                en.x = targetX;
                en.y = targetY;
            } else {
                en.dx = -en.dx;
                en.dy = -en.dy;
            }
        }

        if (en.x == g.p.x && en.y == g.p.y) {
            g.p.alive = false;
        }
    }

    // --- LOGIKA BOMB I PŁOMIENI ---
    for (size_t i = 0; i < g.bombs.size();) {
        g.bombs[i].timer--;
        if (g.bombs[i].timer <= 0) {
            triggerExplosion(g.bombs[i], g.mapa, g.flames);
            g.bombs.erase(g.bombs.begin() + i);
        } else {
            i++;
        }
    }

    for (size_t i = 0; i < g.flames.size();) {
        g.flames[i].timer--;
        
        if (g.flames[i].x == g.p.x && g.flames[i].y == g.p.y) {
            g.p.alive = false;
        }

        for (auto &en : g.enemies) {
            if (en.alive && g.flames[i].x == en.x && g.flames[i].y == en.y) {
                en.alive = false;
            }
        }

        if (g.flames[i].timer <= 0) {
            g.flames.erase(g.flames.begin() + i);
        } else {
            i++;
        }
    }

    // --- RENDEROWANIE ---
    SDL_SetRenderDrawColor(g.renderer, 20, 20, 20, 255);
    SDL_RenderClear(g.renderer);

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            SDL_Rect rect = { x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE };
            char tile = g.mapa[y][x];
            
            if (tile == '#') {
                SDL_SetRenderDrawColor(g.renderer, 80, 80, 80, 255);   // Twarda ściana
            } else if (tile == 'B') {
                SDL_SetRenderDrawColor(g.renderer, 160, 82, 45, 255);  // Skrzynka
            } else {
                SDL_SetRenderDrawColor(g.renderer, 30, 30, 30, 255);   // Podłoga
            }
            SDL_RenderFillRect(g.renderer, &rect);
        }
    }

    // Żółto-zielone bomby
    SDL_SetRenderDrawColor(g.renderer, 180, 255, 0, 255);
    for (const auto& b : g.bombs) {
        SDL_Rect bRect = { b.x * TILE_SIZE + 4, b.y * TILE_SIZE + 4, TILE_SIZE - 8, TILE_SIZE - 8 };
        SDL_RenderFillRect(g.renderer, &bRect);
    }

    // Płomienie
    SDL_SetRenderDrawColor(g.renderer, 255, 140, 0, 255);
    for (const auto& f : g.flames) {
        SDL_Rect fRect = { f.x * TILE_SIZE, f.y * TILE_SIZE, TILE_SIZE, TILE_SIZE };
        SDL_RenderFillRect(g.renderer, &fRect);
    }

    // Boty
    for (const auto& en : g.enemies) {
        if (en.alive) {
            SDL_Rect enRect = { en.x * TILE_SIZE + 4, en.y * TILE_SIZE + 4, TILE_SIZE - 8, TILE_SIZE - 8 };
            if (en.type == VERTICAL) {
                SDL_SetRenderDrawColor(g.renderer, 50, 205, 50, 255);  // Zielony
            } else {
                SDL_SetRenderDrawColor(g.renderer, 186, 85, 211, 255); // Fioletowy
            }
            SDL_RenderFillRect(g.renderer, &enRect);
        }
    }

    // Gracz
    if (g.p.alive) {
        SDL_Rect playerRect = { g.p.x * TILE_SIZE + 2, g.p.y * TILE_SIZE + 2, TILE_SIZE - 4, TILE_SIZE - 4 };
        SDL_SetRenderDrawColor(g.renderer, 0, 150, 255, 255);
        SDL_RenderFillRect(g.renderer, &playerRect);
    }

    // Panel dotykowy UI
    SDL_Rect uiPanel = { 0, HEIGHT * TILE_SIZE, WIDTH * TILE_SIZE, UI_HEIGHT };
    SDL_SetRenderDrawColor(g.renderer, 40, 40, 40, 255);
    SDL_RenderFillRect(g.renderer, &uiPanel);

    SDL_SetRenderDrawColor(g.renderer, 90, 90, 110, 255);
    SDL_RenderFillRect(g.renderer, &g.btnUp);
    SDL_RenderFillRect(g.renderer, &g.btnLeft);
    SDL_RenderFillRect(g.renderer, &g.btnDown);
    SDL_RenderFillRect(g.renderer, &g.btnRight);

    SDL_SetRenderDrawColor(g.renderer, 220, 40, 40, 255);
    SDL_RenderFillRect(g.renderer, &g.btnBomb);

    SDL_SetRenderDrawColor(g.renderer, 100, 100, 100, 255);
    SDL_RenderFillRect(g.renderer, &g.btnQuit);

    SDL_RenderPresent(g.renderer);
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;

    GameState game;
    globalGame = &game;

    SDL_Window* window = SDL_CreateWindow("Bomberman WebAssembly", 
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                          WIDTH * TILE_SIZE, (HEIGHT * TILE_SIZE) + UI_HEIGHT, 
                                          SDL_WINDOW_SHOWN);
    game.renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    srand(time(0));
    initGame(game.p, game.mapa, game.enemies);
    game.running = true;

    // Pozycje przycisków w panelu dolnym
    game.btnUp    = { 80,  360, 50, 40 };
    game.btnLeft  = { 25,  405, 50, 40 };
    game.btnDown  = { 80,  405, 50, 40 };
    game.btnRight = { 135, 405, 50, 40 };
    game.btnBomb  = { 320, 375, 70, 70 };
    game.btnQuit  = { 425, 385, 45, 45 };

#ifdef __EMSCRIPTEN__
    // Emscripten obsługuje pętlę przez callback przeglądarki
    emscripten_set_main_loop(main_loop_iteration, 0, 1);
#else
    // Klasyczna pętla lokalna na komputer
    while (game.running) {
        main_loop_iteration();
        SDL_Delay(16);
    }
#endif

    SDL_DestroyRenderer(game.renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}