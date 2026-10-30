#include <SDL2/SDL.h>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <string>

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
    int bombLimit;     // Maksymalna liczba jednocześnie postawionych bomb
    int explosionRange;// Zasięg płomienia bomby
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
    int range; // Zasięg tej konkretnej bomby
    bool active;
};

struct Flame {
    int x, y;
    int timer;
};

// Struktura spadającego klocka gliny ('T')
struct FallingClay {
    int x, y;
    int fallTimer;
};

// Typy power-upów
enum PowerUpType { RANGE_UP, BOMBS_UP };

struct PowerUp {
    int x, y;
    PowerUpType type;
    bool active;
};

// Globalny stan gry
struct GameState {
    Player p;
    std::vector<std::string> mapa;
    std::vector<Bomb> bombs;
    std::vector<Flame> flames;
    std::vector<Enemy> enemies;
    std::vector<FallingClay> fallingClays;
    std::vector<PowerUp> powerups;
    SDL_Renderer* renderer;
    SDL_Window* window;
    SDL_Rect btnUp, btnLeft, btnDown, btnRight, btnBomb, btnQuit;
    bool running;
    int currentLevel;
    int exitX, exitY;
    bool gameWon;
};

GameState* globalGame = nullptr;

// Definicje map
const std::vector<std::vector<std::string>> LEVEL_MAPS = {
    // Poziom 1: Klasyczna szachownica z kilkoma pułapkami z gliny
    {
        "###############",
        "#...B...B...T.#",
        "#.#.B.#.B.#.B.#",
        "#BBB...BBB...B#",
        "#.#.T.#.B.#.B.#",
        "#...B...T...B.#",
        "#.#.B.#.B.#.B.#",
        "#BBB...BBB...B#",
        "#.#.B.#.T.#.B.#",
        "#...B...B...E.#",
        "###############"
    },
    // Poziom 2: Gęstsza szachownica z większą ilością gliny blokującej przejścia
    {
        "###############",
        "#...B...B...B.#",
        "#.B##..BB.###.#",
        "#B.B.B...B.T.B#",
        "#.#B#...#.###.#",
        "#B.T....T.B.B#",
        "#.###..#B.##B.#",
        "#..B.T.B.B.T.B#",
        "#.TTT.TTT.TB#.#",
        "#...........BE#",
        "###############"
    },
    // Poziom 3: Labirynt ogrodowy z pułapkami w strategicznych miejscach
    {
        "###############",
        "#.#TBBBT#B#T#T#",
        "#...BT......BB#",
        "#T#B#T#B#T#B#B#",
        "#..BB......B..#",
        "#.#T.B#T.B.T#B.",
        "#..BB...TB....#",
        "#.#B#T#B#T#BTT#",
        "#..BB....BB...#",
        "#B#B#B#B#BBBBE#",
        "###############"
    }
};

void loadLevel(int levelIdx, GameState &g) {
    g.currentLevel = levelIdx % LEVEL_MAPS.size();
    g.p.x = 1;
    g.p.y = 1;
    g.p.alive = true;
    g.gameWon = false;
    g.p.bombLimit = 1;     // Start z 1 bombą
    g.p.explosionRange = 1;// Start z zasięgiem 1

    g.mapa = LEVEL_MAPS[g.currentLevel];
    g.bombs.clear();
    g.flames.clear();
    g.enemies.clear();
    g.fallingClays.clear();
    g.powerups.clear();

    // Szukamy wyjścia 'E' na mapie
    bool foundExit = false;
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (g.mapa[y][x] == 'E') {
                g.exitX = x;
                g.exitY = y;
                foundExit = true;
                break;
            }
        }
        if (foundExit) break;
    }
    if (!foundExit) {
        g.exitX = WIDTH - 2;
        g.exitY = HEIGHT - 2;
    }

    // Dodawanie wrogów
    if (g.currentLevel == 0) {
        g.enemies.push_back({13, 1, 0, 1, VERTICAL, true, 0});
        g.enemies.push_back({1, 9, 1, 0, HORIZONTAL, true, 0});
        g.enemies.push_back({13, 9, 0, -1, VERTICAL, true, 0});
    } else if (g.currentLevel == 1) {
        g.enemies.push_back({13, 1, 0, 1, VERTICAL, true, 0});
        g.enemies.push_back({1, 9, 1, 0, HORIZONTAL, true, 0});
        g.enemies.push_back({13, 9, -1, 0, HORIZONTAL, true, 0});
        g.enemies.push_back({7, 5, 0, 1, VERTICAL, true, 0});
    } else {
        g.enemies.push_back({13, 1, 0, 1, VERTICAL, true, 0});
        g.enemies.push_back({1, 9, 1, 0, HORIZONTAL, true, 0});
        g.enemies.push_back({13, 9, 0, -1, VERTICAL, true, 0});
        g.enemies.push_back({7, 1, 1, 0, HORIZONTAL, true, 0});
        g.enemies.push_back({7, 9, -1, 0, HORIZONTAL, true, 0});
    }
}

void placeBomb(const Player &p, std::vector<Bomb> &bombs) {
    int activeBombsCount = 0;
    for (const auto& b : bombs) {
        if (b.active) activeBombsCount++;
    }

    if (activeBombsCount < p.bombLimit) {
        bombs.push_back({p.x, p.y, 90, p.explosionRange, true});
    }
}

void triggerExplosion(Bomb &b, GameState &g, std::vector<Flame> &flames) {
    b.active = false;
    flames.push_back({b.x, b.y, 20});

    int dx[] = {0, 0, -1, 1};
    int dy[] = {-1, 1, 0, 0};

    for (int i = 0; i < 4; i++) {
        for (int r = 1; r <= b.range; r++) {
            int nx = b.x + dx[i] * r;
            int ny = b.y + dy[i] * r;

            if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT) {
                char tile = g.mapa[ny][nx];
                if (tile == '#') break; // Twarda ściana całkowicie blokuje promień ognia
                if (tile == 'E') break; // Wyjście jest odporne na płomień

                flames.push_back({nx, ny, 20});

                if (tile == 'B') {
                    g.mapa[ny][nx] = '.'; // Skrzynka znika
                    // 35% szans na wypadnięcie power-upa ze zniszczonej skrzynki
                    if (rand() % 100 < 35) {
                        PowerUpType pt = (rand() % 2 == 0) ? RANGE_UP : BOMBS_UP;
                        g.powerups.push_back({nx, ny, pt, true});
                    }
                    break; // Skrzynka zatrzymuje dalszy bieg płomienia w tym kierunku
                } 
                else if (tile == 'T') {
                    // Pułapka z gliny zostaje zniszczona i uruchamia grawitację
                    g.mapa[ny][nx] = '.';
                    g.fallingClays.push_back({nx, ny, 0});
                    break; // Glina zatrzymuje dalszy bieg płomienia
                }
            } else {
                break;
            }
        }
    }
}

void tryMovePlayer(int nx, int ny, GameState &g) {
    if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT) {
        char tile = g.mapa[ny][nx];
        if (tile == '.' || tile == 'E') {
            g.p.x = nx;
            g.p.y = ny;

            // Sprawdzenie zebrania power-upa
            for (size_t i = 0; i < g.powerups.size();) {
                if (g.powerups[i].active && g.powerups[i].x == g.p.x && g.powerups[i].y == g.p.y) {
                    if (g.powerups[i].type == RANGE_UP) {
                        if (g.p.explosionRange < 3) g.p.explosionRange++; // Max zasięg 3
                    } else if (g.powerups[i].type == BOMBS_UP) {
                        if (g.p.bombLimit < 5) g.p.bombLimit++; // Max bomb 5
                    }
                    g.powerups.erase(g.powerups.begin() + i);
                } else {
                    i++;
                }
            }

            // Sprawdzenie wejścia na wyjście
            if (tile == 'E') {
                int nextLevel = g.currentLevel + 1;
                if (nextLevel >= LEVEL_MAPS.size()) {
                    g.gameWon = true;
                } else {
                    loadLevel(nextLevel, g);
                }
            }
        }
    }
}

// --- GŁÓWNA FUNKCJA KLATKI ---
void main_loop_iteration() {
    GameState &g = *globalGame;
    if (!g.running) return;

    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            g.running = false;
        } 
        else if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_r) {
                if (!g.p.alive || g.gameWon) {
                    loadLevel(g.currentLevel, g);
                }
            }
            else if (g.p.alive && !g.gameWon) {
                int nx = g.p.x, ny = g.p.y;
                switch (e.key.keysym.sym) {
                    case SDLK_w: case SDLK_UP:    ny--; break;
                    case SDLK_s: case SDLK_DOWN:  ny++; break;
                    case SDLK_a: case SDLK_LEFT:  nx--; break;
                    case SDLK_d: case SDLK_RIGHT: nx++; break;
                    case SDLK_SPACE: placeBomb(g.p, g.bombs); break;
                    case SDLK_q: case SDLK_ESCAPE: g.running = false; break;
                }
                tryMovePlayer(nx, ny, g);
            }
        }
        else if (e.type == SDL_MOUSEBUTTONDOWN && g.p.alive && !g.gameWon) {
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

            tryMovePlayer(nx, ny, g);
        }
    }

    // --- AKTUALIZACJA TYTUŁU OKNA Z STATYSTYKAMI ---
    std::string title = "Ogrody Krolowej [Poz: " + std::to_string(g.currentLevel + 1) + 
                        "] | Bomby: " + std::to_string(g.p.bombLimit) + 
                        " | Zasieg: " + std::to_string(g.p.explosionRange);
    if (g.gameWon) title = "Gratulacje! Przeszedles cale ogrody! [Wcisnij R]";
    else if (!g.p.alive) title = "Koniec gry! [Wcisnij R, aby restartowac]";
    SDL_SetWindowTitle(g.window, title.c_str());

    if (g.p.alive && !g.gameWon) {
        // --- GRAWITACJA KLOCKÓW GLINY ('T') ---
        for (size_t i = 0; i < g.fallingClays.size();) {
            g.fallingClays[i].fallTimer++;
            if (g.fallingClays[i].fallTimer >= 8) {
                g.fallingClays[i].fallTimer = 0;
                
                int cx = g.fallingClays[i].x;
                int cy = g.fallingClays[i].y;
                int nextY = cy + 1;

                bool canFall = false;
                if (nextY < HEIGHT) {
                    char targetTile = g.mapa[nextY][cx];
                    if (targetTile == '.') {
                        bool bombPresent = false;
                        for (const auto& b : g.bombs) {
                            if (b.x == cx && b.y == nextY) { bombPresent = true; break; }
                        }
                        if (!bombPresent) canFall = true;
                    }
                }

                if (canFall) {
                    g.fallingClays[i].y = nextY;

                    if (g.p.x == cx && g.p.y == nextY) {
                        g.p.alive = false;
                    }
                    for (auto &en : g.enemies) {
                        if (en.alive && en.x == cx && en.y == nextY) {
                            en.alive = false;
                        }
                    }
                } else {
                    if (cy >= 0 && cy < HEIGHT && cx >= 0 && cx < WIDTH) {
                        if (g.mapa[cy][cx] == '.') {
                            g.mapa[cy][cx] = '#';
                        }
                    }
                    g.fallingClays.erase(g.fallingClays.begin() + i);
                    continue;
                }
            }
            i++;
        }

        // --- RUCH BOTÓW ---
        for (auto &en : g.enemies) {
            if (!en.alive) continue;

            en.moveTimer++;
            if (en.moveTimer >= 25) { 
                en.moveTimer = 0;

                int targetX = en.x + en.dx;
                int targetY = en.y + en.dy;

                if (targetX >= 0 && targetX < WIDTH && targetY >= 0 && targetY < HEIGHT && 
                    (g.mapa[targetY][targetX] == '.' || g.mapa[targetY][targetX] == 'E')) {
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
                triggerExplosion(g.bombs[i], g, g.flames);
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
            } else if (tile == 'T') {
                SDL_SetRenderDrawColor(g.renderer, 204, 119, 34, 255); // Pułapka z gliny
            } else if (tile == 'E') {
                SDL_SetRenderDrawColor(g.renderer, 255, 215, 0, 255);  // Wyjście
            } else {
                SDL_SetRenderDrawColor(g.renderer, 30, 30, 30, 255);   // Podłoga
            }
            SDL_RenderFillRect(g.renderer, &rect);
        }
    }

    // Power-upy leżące na ziemi
    for (const auto& pu : g.powerups) {
        if (!pu.active) continue;
        SDL_Rect puRect = { pu.x * TILE_SIZE + 8, pu.y * TILE_SIZE + 8, TILE_SIZE - 16, TILE_SIZE - 16 };
        if (pu.type == RANGE_UP) {
            SDL_SetRenderDrawColor(g.renderer, 255, 69, 0, 255);   // Czerwony (Zasięg)
        } else {
            SDL_SetRenderDrawColor(g.renderer, 0, 191, 255, 255);  // Niebieski (Więcej bomb)
        }
        SDL_RenderFillRect(g.renderer, &puRect);
    }

    // Spadające klocki gliny
    SDL_SetRenderDrawColor(g.renderer, 204, 119, 34, 255);
    for (const auto& fc : g.fallingClays) {
        SDL_Rect fcRect = { fc.x * TILE_SIZE + 2, fc.y * TILE_SIZE + 2, TILE_SIZE - 4, TILE_SIZE - 4 };
        SDL_RenderFillRect(g.renderer, &fcRect);
    }

    // Bomby
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
                SDL_SetRenderDrawColor(g.renderer, 50, 205, 50, 255);  
            } else {
                SDL_SetRenderDrawColor(g.renderer, 186, 85, 211, 255); 
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

    game.window = SDL_CreateWindow("Ogrody Krolowej - Bomberman", 
                                   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                   WIDTH * TILE_SIZE, (HEIGHT * TILE_SIZE) + UI_HEIGHT, 
                                   SDL_WINDOW_SHOWN);
    game.renderer = SDL_CreateRenderer(game.window, -1, SDL_RENDERER_ACCELERATED);

    srand(time(0));
    loadLevel(0, game);
    game.running = true;

    // Pozycje przycisków
    game.btnUp    = { 80,  360, 50, 40 };
    game.btnLeft  = { 25,  405, 50, 40 };
    game.btnDown  = { 80,  405, 50, 40 };
    game.btnRight = { 135, 405, 50, 40 };
    game.btnBomb  = { 320, 375, 70, 70 };
    game.btnQuit  = { 425, 385, 45, 45 };

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop_iteration, 0, 1);
#else
    while (game.running) {
        main_loop_iteration();
        SDL_Delay(16);
    }
#endif

    SDL_DestroyRenderer(game.renderer);
    SDL_DestroyWindow(game.window);
    SDL_Quit();

    return 0;
}