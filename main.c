#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <windows.h>
#include <conio.h>

#define W 180
#define H 50
#define MAX_BULLET 50
#define MAX_ENEMY 40
#define MAX_EBULLET 30
#define MAX_POWER 8
#define MAX_STAR 80
#define MAX_PARTICLE 100
#define MAX_NOTIF 5

HANDLE hCon, hBuf[2];
int curBuf = 0;
CHAR_INFO scr[H][W];
COORD scrSize = {W, H};
COORD scrCoord = {0, 0};
SMALL_RECT scrRect = {0, 0, W-1, H-1};

int screenShake = 0;
int screenFlash = 0;
int screenFlashColor = 0;

typedef struct
{
    float x, y;
    float vx, vy;
    int hp, maxHp;
    int lives, score;
    int cooldown, power;
    int invTime;
    int alive;
    int combo, comboTime;
    int maxCombo;
    char name[16];
} Player;

typedef struct
{
    float x, y;
    int active, dmg, type;
} Bullet;

typedef struct
{
    float x, y, targetY;
    int active, type, hp, maxHp;
    int dir, timer, points;
    int formation;
    float angle;
} Enemy;

typedef struct
{
    float x, y;
    int active, hp, maxHp;
    int phase, dir, timer;
    int points, pattern;
    float angle;
} Boss;

typedef struct
{
    int x, y, active, type, time;
} PowerUp;

typedef struct
{
    float x, y, vx, vy;
    int life, color;
    char ch;
} Particle;

typedef struct
{
    float x, y;
    int speed, layer;
    char ch;
} Star;

typedef struct
{
    char text[50];
    int time, color, y;
} Notif;

typedef struct
{
    int level, diff;
    int wave, maxWave;
    int enemyCount, kills, totalKills;
    int bossActive, bossDefeated;
    int paused, over;
    unsigned long tick;
    int highscore;
} Game;

Player P;
Bullet pB[MAX_BULLET];
Bullet eB[MAX_EBULLET];
Enemy E[MAX_ENEMY];
Boss B;
PowerUp PW[MAX_POWER];
Particle PT[MAX_PARTICLE];
Star S[MAX_STAR];
Notif N[MAX_NOTIF];
Game G;

int nebulaX = 0;
int nebulaY = 0;

void initConsole()
{
    hCon = GetStdHandle(STD_OUTPUT_HANDLE);

    HWND hw = GetConsoleWindow();
    ShowWindow(hw, SW_MAXIMIZE);

    COORD sz = {W, H};
    SMALL_RECT rc = {0, 0, W-1, H-1};
    SetConsoleScreenBufferSize(hCon, sz);
    SetConsoleWindowInfo(hCon, TRUE, &rc);
    SetConsoleScreenBufferSize(hCon, sz);

    hBuf[0] = CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    hBuf[1] = CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);

    CONSOLE_CURSOR_INFO ci = {1, FALSE};
    SetConsoleCursorInfo(hBuf[0], &ci);
    SetConsoleCursorInfo(hBuf[1], &ci);
    SetConsoleCursorInfo(hCon, &ci);

    SetConsoleScreenBufferSize(hBuf[0], sz);
    SetConsoleScreenBufferSize(hBuf[1], sz);

    SetConsoleTitleA("SPACE INVADERS: GALACTIC WARFARE - DELUXE EDITION");
}

void cls()
{
    memset(scr, 0, sizeof(scr));


    if (screenFlash > 0)
    {
        for (int y = 0; y < H; y++)
        {
            for (int x = 0; x < W; x++)
            {
                scr[y][x].Attributes = screenFlashColor;
            }
        }
        screenFlash--;
    }
}

void put(int x, int y, char c, int col)
{
    if (screenShake > 0)
    {
        x += (rand() % 3) - 1;
        y += (rand() % 2) - 1;
        screenShake--;
    }

    if (x >= 0 && x < W && y >= 0 && y < H)
    {
        scr[y][x].Char.AsciiChar = c;
        scr[y][x].Attributes = col;
    }
}

void puts2(int x, int y, const char* s, int col)
{
    while (*s && x < W) put(x++, y, *s++, col);
}

void drawCenter(int y, const char* s, int col)
{
    puts2((W - (int)strlen(s)) / 2, y, s, col);
}

void flip()
{
    WriteConsoleOutputA(hBuf[curBuf], (CHAR_INFO*)scr, scrSize, scrCoord, &scrRect);
    SetConsoleActiveScreenBuffer(hBuf[curBuf]);
    curBuf = 1 - curBuf;
}

void box(int x, int y, int w, int h, int col, int fill)
{
    put(x, y, 201, col);
    put(x+w-1, y, 187, col);
    put(x, y+h-1, 200, col);
    put(x+w-1, y+h-1, 188, col);

    for (int i = 1; i < w-1; i++)
    {
        put(x+i, y, 205, col);
        put(x+i, y+h-1, 205, col);
    }
    for (int i = 1; i < h-1; i++)
    {
        put(x, y+i, 186, col);
        put(x+w-1, y+i, 186, col);
    }

    if (fill)
    {
        for (int j = 1; j < h-1; j++)
            for (int i = 1; i < w-1; i++)
                put(x+i, y+j, ' ', col);
    }
}

void bar(int x, int y, int w, int val, int mx, int fg, int bg, int fancy)
{
    int fill = (val * w) / mx;
    if (fill < 0) fill = 0;
    if (fill > w) fill = w;

    for (int i = 0; i < w; i++)
    {
        char c;
        if (fancy)
        {
            if (i < fill) c = 219;
            else c = 176;
        }
        else
        {
            if (i < fill) c = '=';
            else c = '-';
        }
        put(x+i, y, c, i < fill ? fg : bg);
    }
}

void spawnParticle(float x, float y, float vx, float vy, int life, int col, char ch)
{
    for (int i = 0; i < MAX_PARTICLE; i++)
    {
        if (PT[i].life <= 0)
        {
            PT[i].x = x;
            PT[i].y = y;
            PT[i].vx = vx;
            PT[i].vy = vy;
            PT[i].life = life;
            PT[i].color = col;
            PT[i].ch = ch;
            break;
        }
    }
}

void spawnExplosion(int x, int y, int size, int type)
{
    const char chars[] = {'*', '+', 'X', '#', '@', 'o', '.', ':'};
    const int cols[] = {14, 12, 6, 4, 15, 7};

    int count = size * 8;
    for (int i = 0; i < count; i++)
    {
        float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
        float speed = (float)(rand() % (size * 2) + 1) * 0.5f;
        float vx = cosf(angle) * speed;
        float vy = sinf(angle) * speed * 0.5f;
        int life = rand() % 15 + 10;
        int col = cols[rand() % 6];
        char ch = chars[rand() % 8];
        spawnParticle((float)x, (float)y, vx, vy, life, col, ch);
    }

    if (type == 1)
    {
        screenShake = 15;
        screenFlash = 3;
        screenFlashColor = 14;
    }
    else if (size > 3)
    {
        screenShake = 5;
    }
}

void updateParticles()
{
    for (int i = 0; i < MAX_PARTICLE; i++)
    {
        if (PT[i].life > 0)
        {
            PT[i].x += PT[i].vx;
            PT[i].y += PT[i].vy;
            PT[i].vy += 0.05f;
            PT[i].life--;
        }
    }
}

void drawParticles()
{
    for (int i = 0; i < MAX_PARTICLE; i++)
    {
        if (PT[i].life > 0)
        {
            int col = PT[i].color;
            if (PT[i].life < 5) col = 8;
            put((int)PT[i].x, (int)PT[i].y, PT[i].ch, col);
        }
    }
}


void updateNotifs()
{
    for (int i = 0; i < MAX_NOTIF; i++)
    {
        if (N[i].time > 0) N[i].time--;
    }
}

void drawNotifs()
{
    for (int i = 0; i < MAX_NOTIF; i++)
    {
        if (N[i].time > 0)
        {
            int col = N[i].color;
            if (N[i].time < 20) col = 8;
            drawCenter(N[i].y + i, N[i].text, col);
        }
    }
}

void initStars()
{
    for (int i = 0; i < MAX_STAR; i++)
    {
        S[i].x = (float)(rand() % W);
        S[i].y = (float)(rand() % (H - 6) + 4);
        S[i].speed = rand() % 3 + 1;
        S[i].layer = rand() % 3;
        S[i].ch = S[i].layer == 2 ? '*' : (S[i].layer == 1 ? '+' : '.');
    }
    nebulaX = rand() % W;
    nebulaY = rand() % (H - 20) + 10;
}

void updateStars()
{
    for (int i = 0; i < MAX_STAR; i++)
    {
        S[i].y += S[i].speed * 0.15f;
        if (S[i].y >= H - 2)
        {
            S[i].y = 4;
            S[i].x = (float)(rand() % W);
        }
    }

    if (G.tick % 50 == 0)
    {
        nebulaX += rand() % 3 - 1;
        nebulaY += rand() % 3 - 1;
        if (nebulaX < 10) nebulaX = 10;
        if (nebulaX > W - 30) nebulaX = W - 30;
        if (nebulaY < 10) nebulaY = 10;
        if (nebulaY > H - 15) nebulaY = H - 15;
    }
}

void drawBackground()
{
    if (G.level >= 2)
    {
        for (int dy = -5; dy <= 5; dy++)
        {
            for (int dx = -10; dx <= 10; dx++)
            {
                float dist = sqrtf((float)(dx*dx/4 + dy*dy));
                if (dist < 5)
                {
                    int col = dist < 2 ? 5 : (dist < 4 ? 1 : 0);
                    if (col > 0)
                    {
                        int x = nebulaX + dx;
                        int y = nebulaY + dy;
                        if (x >= 0 && x < W && y >= 4 && y < H - 2)
                        {
                            char c = dist < 2 ? 176 : 177;
                            put(x, y, c, col);
                        }
                    }
                }
            }
        }
    }

    for (int i = 0; i < MAX_STAR; i++)
    {
        int col;
        switch (S[i].layer)
        {
        case 0:
            col = 8;
            break;
        case 1:
            col = 7;
            break;
        case 2:
            col = 15;
            break;
        default:
            col = 7;
        }

        if (S[i].layer == 2 && rand() % 10 == 0) col = 14;
        put((int)S[i].x, (int)S[i].y, S[i].ch, col);
    }
}

void drawStarFieldSimple()
{
    for (int i = 0; i < 120; i++)
    {
        int x = rand() % W;
        int y = rand() % H;
        int c = rand() % 3;
        put(x, y, c == 2 ? '*' : '.', c == 2 ? 15 : (c == 1 ? 7 : 8));
    }
}

void drawBigTitle(int startY, int colorSpace, int colorInvaders)
{
    const char* space[] =
    {
        " #####   #####        #       ####    ######",
        "#        #    #     #   #    #        #        ",
        "#        #    #    #     #   #        #      ",
        " #####   #####    #       #  #        #### ",
        "      #  #        # # # # #  #        #      ",
        "#     #  #        #       #  #        #      ",
        " #####   #        #       #   ####    ######"
    };

    const char* invaders[] =
    {
        "######  #     #  #      #     ##     #####    ######  #####     #####",
        "  ##    ##    #  #      #    #  #    #    #   #       #    #   #     #",
        "  ##    # #   #  #      #   #    #   #     #  #       #    #   #      ",
        "  ##    #  #  #  #      #  #      #  #     #  ####    #####     ##### ",
        "  ##    #   # #   #    #   ########  #     #  #       #    #         #",
        "  ##    #    ##    #  #    #      #  #    #   #       #     #  #     #",
        "######  #     #     ##     #      #  #####    ######  #      #  ##### "
    };

    int spaceWidth = 38;
    int invadersWidth = 60;

    int sx = (W - spaceWidth) / 2;
    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; space[i][j]; j++)
        {
            if (space[i][j] == '#')
            {
                put(sx + j, startY + i, 219, colorSpace);
            }
        }
    }

    int ix = (W - invadersWidth) / 2;
    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; invaders[i][j]; j++)
        {
            if (invaders[i][j] == '#')
            {
                put(ix + j, startY + 9 + i, 219, colorInvaders);
            }
        }
    }
}

void drawTitleFrame(int frame)
{
    int borderCol = (frame % 6 < 3) ? 11 : 3;

    for (int x = 10; x < W - 10; x++)
    {
        char c = ((x + frame) % 4 == 0) ? '=' : '-';
        put(x, 2, c, borderCol);
        put(x, H - 3, c, borderCol);
    }

    for (int y = 3; y < H - 3; y++)
    {
        char c = ((y + frame) % 3 == 0) ? '|' : ':';
        put(10, y, c, borderCol);
        put(W - 11, y, c, borderCol);
    }

    put(10, 2, '+', 15);
    put(W - 11, 2, '+', 15);
    put(10, H - 3, '+', 15);
    put(W - 11, H - 3, '+', 15);
}

int checkSkip()
{
    if (_kbhit())
    {
        char c = _getch();
        if (c == 27) return 1;
        if (c == 0 || c == -32) _getch();
    }
    return 0;
}

void showIntro()
{
    for (int f = 0; f < 30; f++)
    {
        if (checkSkip()) return;
        cls();
        for (int i = 0; i < 50 + f * 2; i++)
        {
            int x = W/2 + (rand() % (f * 4 + 10)) - (f * 4 + 10) / 2;
            int y = H/2 + (rand() % (f * 2 + 5)) - (f * 2 + 5) / 2;
            if (x >= 0 && x < W && y >= 0 && y < H)
            {
                put(x, y, '*', 15);
            }
        }
        drawCenter(H - 3, "[Press ESC to skip intro]", 8);
        flip();
        Sleep(30);
    }

    if (checkSkip()) return;
    cls();
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            put(x, y, ' ', 0xF0);
    flip();
    Sleep(80);

    for (int reveal = 1; reveal <= 7; reveal++)
    {
        if (checkSkip()) return;
        cls();
        drawStarFieldSimple();

        const char* space[] =
        {
            " #####   #####        #       ####    ######",
            "#        #    #     #   #    #        #        ",
            "#        #    #    #     #   #        #      ",
            " #####   #####    #       #  #        #### ",
            "      #  #        # # # # #  #        #      ",
            "#     #  #        #       #  #        #      ",
            " #####   #        #       #   ####    ######"
        };

        const char* invaders[] =
        {
            "######  #     #  #      #     ##     #####    ######  #####     #####",
            "  ##    ##    #  #      #    #  #    #    #   #       #    #   #     #",
            "  ##    # #   #  #      #   #    #   #     #  #       #    #   #      ",
            "  ##    #  #  #  #      #  #      #  #     #  ####    #####     ##### ",
            "  ##    #   # #   #    #   ########  #     #  #       #    #         #",
            "  ##    #    ##    #  #    #      #  #    #   #       #     #  #     #",
            "######  #     #     ##     #      #  #####    ######  #      #  ##### "
        };

        int sx = (W - 38) / 2;
        for (int i = 0; i < reveal; i++)
        {
            for (int j = 0; space[i][j]; j++)
            {
                if (space[i][j] == '#') put(sx + j, 6 + i, 219, 11);
            }
        }

        int ix = (W - 60) / 2;
        for (int i = 0; i < reveal; i++)
        {
            for (int j = 0; invaders[i][j]; j++)
            {
                if (invaders[i][j] == '#') put(ix + j, 15 + i, 219, 14);
            }
        }

        drawCenter(H - 3, "[Press ESC to skip intro]", 8);
        flip();
        Sleep(80);
    }

    Beep(1000, 80);

    for (int f = 0; f < 40; f++)
    {
        if (checkSkip()) return;
        cls();
        drawStarFieldSimple();
        drawTitleFrame(f);

        int spaceCol = 9 + (f / 10) % 3;
        if (f % 15 < 2) spaceCol = 15;
        drawBigTitle(6, spaceCol, 14);

        drawCenter(24, "* * * D E L U X E   E D I T I O N * * *", 15);

        // Ship
        int sx = W/2 - 10;
        puts2(sx, 28, "        /\\        ", 10);
        puts2(sx, 29, "       /##\\       ", 10);
        puts2(sx, 30, "     |/####\\|     ", 10);
        puts2(sx, 31, "   |=|######|=|   ", 11);
        puts2(sx, 32, "     \\##/\\##/     ", 10);
        puts2(sx, 33, "      VV  VV      ", (f % 4 < 2) ? 14 : 12);

        drawCenter(H - 3, "[Press ESC to skip intro]", 8);
        flip();
        Sleep(50);
    }

    if (checkSkip()) return;
    cls();
    drawStarFieldSimple();
    box(W/2-55, 12, 110, 24, 11, 1);

    drawCenter(15, "T H E   Y E A R   2 3 8 7", 14);
    drawCenter(19, "The ZORGON EMPIRE has declared war on humanity.", 15);
    drawCenter(21, "Their massive armada approaches Earth.", 15);
    drawCenter(25, "You command the legendary PHOENIX-X7 starfighter.", 11);
    drawCenter(27, "You are humanity's LAST HOPE.", 10);
    drawCenter(31, "DESTROY THE ARMADA. SAVE EARTH.", 12);
    drawCenter(H - 3, "[Press ESC to skip or any key to continue]", 8);

    flip();

    for (int i = 0; i < 60; i++)
    {
        if (_kbhit())
        {
            _getch();
            break;
        }
        Sleep(50);
    }

    int blink = 0;
    int timeout = 0;
    while (!_kbhit() && timeout < 20)
    {
        cls();
        drawStarFieldSimple();
        drawTitleFrame(blink * 5);
        drawBigTitle(6, 11, 14);
        drawCenter(24, "* * * D E L U X E   E D I T I O N * * *", 15);

        int sx = W/2 - 10;
        puts2(sx, 28, "        /\\        ", 10);
        puts2(sx, 29, "       /##\\       ", 10);
        puts2(sx, 30, "     |/####\\|     ", 10);
        puts2(sx, 31, "   |=|######|=|   ", 11);
        puts2(sx, 32, "     \\##/\\##/     ", 10);
        puts2(sx, 33, "      VV  VV      ", blink ? 14 : 12);

        if (blink)
        {
            drawCenter(40, ">>>  PRESS ANY KEY TO BEGIN  <<<", 10);
        }
        drawCenter(44, "[W/S] Navigate   [ENTER] Select   [ESC] Exit", 8);

        flip();
        blink = !blink;
        timeout++;
        Sleep(300);
    }
    if (_kbhit()) _getch();
}

void showControls()
{
    cls();
    drawStarFieldSimple();
    box(W/2-55, 6, 110, 38, 11, 1);

    drawCenter(9, "============ CONTROLS ============", 14);

    drawCenter(13, "MOVEMENT", 15);
    drawCenter(15, "[A/D] or [LEFT/RIGHT]  -  Move ship horizontally", 11);

    drawCenter(18, "WEAPONS", 15);
    drawCenter(20, "[SPACE]  -  Fire primary weapon", 14);

    drawCenter(29, "OTHER", 15);
    drawCenter(33, "[P]      -  Pause Game", 7);
    drawCenter(35, "[ESC]    -  Quit to Menu", 7);

    drawCenter(38, "!! COLLISION WITH ENEMIES DESTROYS YOUR SHIP !!", 12);

    drawCenter(41, ">>> Press ENTER to launch <<<", 10);
    flip();

    while (1)
    {
        if (_kbhit() && _getch() == 13) break;
        Sleep(30);
    }
}

int mainMenu()
{
    int sel = 0;
    int frame = 0;

    while (1)
    {
        cls();
        drawStarFieldSimple();
        drawTitleFrame(frame);

        int spaceCol = 9 + (frame / 15) % 3;
        drawBigTitle(4, spaceCol, 14);

        drawCenter(22, "* * * D E L U X E   E D I T I O N * * *", 15);

        box(W/2-25, 26, 50, 16, 11, 1);

        const char* opts[] = {"NEW GAME", "CONTROLS", "EXIT"};

        for (int i = 0; i < 3; i++)
        {
            char line[30];
            if (i == sel)
            {
                sprintf(line, ">>  %s  <<", opts[i]);
                drawCenter(31 + i*3, line, 14);
            }
            else
            {
                drawCenter(31 + i*3, opts[i], 7);
            }
        }

        drawCenter(44, "[W/S] Navigate   [ENTER] Select   [ESC] Exit", 8);

        flip();
        frame++;

        if (_kbhit())
        {
            char c = _getch();
            if (c == 27) return 2;
            if (c == 0 || c == -32) c = _getch();
            if (c == 'w' || c == 'W' || c == 72) sel = (sel + 2) % 3;
            else if (c == 's' || c == 'S' || c == 80) sel = (sel + 1) % 3;
            else if (c == 13)
            {
                Sleep(150);
                return sel;
            }
        }
        Sleep(40);
    }
}

void initPlayer()
{
    P.x = W / 2 - 6;
    P.y = H - 9;
    P.vx = P.vy = 0;
    P.maxHp = 100;
    P.hp = 100;
    P.lives = 3;
    P.score = 0;
    P.cooldown = 0;
    P.power = 1;
    P.invTime = 0;
    P.alive = 1;
    P.combo = 0;
    P.comboTime = 0;
    P.maxCombo = 0;
}

void initGame()
{
    G.level = 1;
    G.wave = 1;
    G.diff = 2;
    G.maxWave = 3;
    G.enemyCount = 0;
    G.kills = 0;
    G.totalKills = 0;
    G.bossActive = 0;
    G.bossDefeated = 0;
    G.paused = 0;
    G.over = 0;
    G.tick = 0;
    G.highscore = 0;

    for (int i = 0; i < MAX_BULLET; i++) pB[i].active = 0;
    for (int i = 0; i < MAX_EBULLET; i++) eB[i].active = 0;
    for (int i = 0; i < MAX_ENEMY; i++) E[i].active = 0;
    for (int i = 0; i < MAX_POWER; i++) PW[i].active = 0;
    for (int i = 0; i < MAX_PARTICLE; i++) PT[i].life = 0;
    for (int i = 0; i < MAX_NOTIF; i++) N[i].time = 0;

    B.active = 0;

    initPlayer();
    initStars();
}

void spawnEnemy(int type, int formation, float startX, float startY)
{
    for (int i = 0; i < MAX_ENEMY; i++)
    {
        if (!E[i].active)
        {
            E[i].active = 1;
            E[i].x = startX >= 0 ? startX : (float)(rand() % (W - 20) + 10);
            E[i].y = startY >= 0 ? startY : 5.0f;
            E[i].targetY = (float)(rand() % 15 + 8);
            E[i].type = type;
            E[i].dir = rand() % 2 ? 1 : -1;
            E[i].timer = rand() % 40;
            E[i].formation = formation;
            E[i].angle = 0;

            switch (type)
            {
            case 0:
                E[i].hp = 1;
                E[i].points = 10;
                break;
            case 1:
                E[i].hp = 2;
                E[i].points = 25;
                break;
            case 2:
                E[i].hp = 4;
                E[i].points = 40;
                break;
            case 3:
                E[i].hp = 6;
                E[i].points = 60;
                break;
            case 4:
                E[i].hp = 8;
                E[i].points = 80;
                break;
            }
            E[i].hp += G.level;
            E[i].maxHp = E[i].hp;
            E[i].points *= G.diff;
            G.enemyCount++;
            break;
        }
    }
}

void spawnWave()
{
    int count = 6 + G.level * 2 + G.diff * 2;
    int types = 1 + G.level / 2;
    if (types > 5) types = 5;

    int formation = G.wave % 3;

    if (formation == 0)
    {
        for (int i = 0; i < count; i++)
        {
            spawnEnemy(rand() % types, 0, -1, -1);
        }
    }
    else if (formation == 1)
    {
        int cx = W / 2;
        for (int i = 0; i < count; i++)
        {
            int row = i / 5;
            int col = i % 5 - 2;
            float x = (float)(cx + col * 12);
            float y = (float)(5 + row * 3 + abs(col) * 2);
            spawnEnemy(rand() % types, 0, x, y);
        }
    }
    else
    {
        for (int i = 0; i < count; i++)
        {
            float x = (float)(20 + (i % 10) * 14);
            float y = (float)(5 + (i / 10) * 4);
            spawnEnemy(rand() % types, 0, x, y);
        }
    }

    char msg[40];
    sprintf(msg, "WAVE %d INCOMING!", G.wave);
}

void spawnBoss()
{
    B.active = 1;
    B.x = W / 2 - 18;
    B.y = 5;
    B.maxHp = 80 + G.level * 50 + G.diff * 30;
    B.hp = B.maxHp;
    B.phase = 1;
    B.dir = 1;
    B.timer = 0;
    B.points = 2000 * G.level * G.diff;
    B.pattern = 0;
    B.angle = 0;
    G.bossActive = 1;

}

void spawnPowerup(int x, int y, int forced)
{
    if (!forced && rand() % 100 > 20) return;

    for (int i = 0; i < MAX_POWER; i++)
    {
        if (!PW[i].active)
        {
            PW[i].active = 1;
            PW[i].x = x;
            PW[i].y = y;
            PW[i].type = rand() % 4;
            PW[i].time = 350;
            break;
        }
    }
}

void handleInput()
{
    if (!P.alive) return;

    float speed = 2.2f;

    if (GetAsyncKeyState(VK_LEFT) & 0x8000 || GetAsyncKeyState('A') & 0x8000)
        if (P.x > 2) P.x -= speed;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000 || GetAsyncKeyState('D') & 0x8000)
        if (P.x < W - 15) P.x += speed;

    if (GetAsyncKeyState(VK_SPACE) & 0x8000)
    {
        if (P.cooldown <= 0)
        {
            for (int i = 0; i < MAX_BULLET; i++)
            {
                if (!pB[i].active)
                {
                    pB[i].active = 1;
                    pB[i].x = P.x + 5;
                    pB[i].y = P.y - 1;
                    pB[i].dmg = P.power;
                    pB[i].type = 0;
                    break;
                }
            }

            if (P.power >= 2)
            {
                for (int side = -1; side <= 1; side += 2)
                {
                    for (int i = 0; i < MAX_BULLET; i++)
                    {
                        if (!pB[i].active)
                        {
                            pB[i].active = 1;
                            pB[i].x = P.x + 5 + side * 4;
                            pB[i].y = P.y;
                            pB[i].dmg = P.power;
                            pB[i].type = 0;
                            break;
                        }
                    }
                }
            }

            if (P.power >= 3)
            {
                for (int side = -1; side <= 1; side += 2)
                {
                    for (int i = 0; i < MAX_BULLET; i++)
                    {
                        if (!pB[i].active)
                        {
                            pB[i].active = 1;
                            pB[i].x = P.x + 5 + side * 6;
                            pB[i].y = P.y + 1;
                            pB[i].dmg = P.power;
                            pB[i].type = 0;
                            break;
                        }
                    }
                }
            }

            P.cooldown = 5 - G.diff;
            if (P.cooldown < 2) P.cooldown = 2;
        }
    }

    if (GetAsyncKeyState('P') & 0x8000)
    {
        G.paused = !G.paused;
        Sleep(200);
    }

    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
    {
        G.over = 1;
    }
}

void updatePlayer()
{
    if (P.cooldown > 0) P.cooldown--;
    if (P.invTime > 0) P.invTime--;

    if (P.comboTime > 0)
    {
        P.comboTime--;
        if (P.comboTime == 0)
        {
            if (P.combo > P.maxCombo) P.maxCombo = P.combo;
            P.combo = 0;
        }
    }
}

void updateBullets()
{
    for (int i = 0; i < MAX_BULLET; i++)
    {
        if (pB[i].active)
        {
            pB[i].y -= 3.0f;
            if (pB[i].y < 3) pB[i].active = 0;
        }
    }
    for (int i = 0; i < MAX_EBULLET; i++)
    {
        if (eB[i].active)
        {
            eB[i].y += 1.5f + G.diff * 0.2f;
            if (eB[i].y >= H - 1) eB[i].active = 0;
        }
    }
}

void updateEnemies()
{
    float speed = 0.8f + G.diff * 0.25f;

    for (int i = 0; i < MAX_ENEMY; i++)
    {
        if (!E[i].active) continue;

        if (E[i].formation == 1)
        {
            E[i].y += speed * 1.5f;
        }
        else if (E[i].formation == 2)
        {
            E[i].angle += 0.05f;
            E[i].x += cosf(E[i].angle) * 2;
            E[i].y += 0.3f;
        }
        else
        {
            E[i].x += E[i].dir * speed;
            if (E[i].x <= 3 || E[i].x >= W - 12) E[i].dir *= -1;
            if (G.tick % 30 == 0) E[i].y += 0.5f;
        }

        if (E[i].type >= 2)
        {
            E[i].timer++;
            int rate = 80 - G.diff * 15;
            if (rate < 30) rate = 30;
            if (E[i].timer >= rate)
            {
                E[i].timer = 0;
                for (int j = 0; j < MAX_EBULLET; j++)
                {
                    if (!eB[j].active)
                    {
                        eB[j].active = 1;
                        eB[j].x = E[i].x + 3;
                        eB[j].y = E[i].y + 3;
                        eB[j].dmg = 1 + E[i].type / 2;
                        break;
                    }
                }
            }
        }

        if (E[i].y >= H - 5)
        {
            E[i].active = 0;
            G.enemyCount--;
            P.hp -= 8;
        }
    }
}

void updateBoss()
{
    if (!B.active) return;

    float speed = 0.6f + B.phase * 0.2f;

    B.x += B.dir * speed;
    if (B.x <= 5 || B.x >= W - 40) B.dir *= -1;

    if (B.phase >= 3)
    {
        B.angle += 0.015f;
        B.y = 5 + sinf(B.angle) * 3;
    }

    B.timer++;
    int rate = 40 - G.diff * 5 - B.phase * 5;
    if (rate < 12) rate = 12;

    if (B.timer >= rate)
    {
        B.timer = 0;
        B.pattern = (B.pattern + 1) % 3;

        if (B.pattern == 0)
        {
            int shots = 3 + B.phase;
            for (int s = 0; s < shots; s++)
            {
                for (int j = 0; j < MAX_EBULLET; j++)
                {
                    if (!eB[j].active)
                    {
                        eB[j].active = 1;
                        eB[j].x = B.x + 15 + (s - shots/2) * 4;
                        eB[j].y = B.y + 8;
                        eB[j].dmg = B.phase;
                        break;
                    }
                }
            }
        }
        else if (B.pattern == 1)
        {
            for (int j = 0; j < MAX_EBULLET; j++)
            {
                if (!eB[j].active)
                {
                    eB[j].active = 1;
                    eB[j].x = B.x + 15;
                    eB[j].y = B.y + 8;
                    eB[j].dmg = B.phase + 1;
                    break;
                }
            }
        }
        else
        {
            for (int k = 0; k < 2 + B.phase; k++)
            {
                for (int j = 0; j < MAX_EBULLET; j++)
                {
                    if (!eB[j].active)
                    {
                        eB[j].active = 1;
                        eB[j].x = B.x + 8 + k * 5;
                        eB[j].y = B.y + 8;
                        eB[j].dmg = B.phase;
                        break;
                    }
                }
            }
        }
    }

    int pct = (B.hp * 100) / B.maxHp;
    if (pct <= 20 && B.phase < 4)
    {
        B.phase = 4;
        screenShake = 8;
    }
    else if (pct <= 40 && B.phase < 3)
    {
        B.phase = 3;
        screenShake = 5;
    }
    else if (pct <= 65 && B.phase < 2)
    {
        B.phase = 2;
    }
}

void updatePowerups()
{
    for (int i = 0; i < MAX_POWER; i++)
    {
        if (PW[i].active)
        {
            PW[i].time--;
            if (G.tick % 3 == 0) PW[i].y++;
            if (PW[i].time <= 0 || PW[i].y >= H - 2) PW[i].active = 0;
        }
    }
}

void addCombo()
{
    P.combo++;
    P.comboTime = 90;
    {
        P.score += 500;
    }
    if (P.combo == 20)
    {
        P.score += 2000;
    }
    else if (P.combo == 50)
    {
        P.score += 10000;
    }
}

void destroyPlayer()
{
    spawnExplosion((int)P.x + 5, (int)P.y + 2, 5, 0);
    P.lives--;
    screenShake = 12;
    screenFlash = 4;
    screenFlashColor = 12;

    if (P.combo > P.maxCombo) P.maxCombo = P.combo;
    P.combo = 0;

    if (P.lives <= 0)
    {
        G.over = 1;
        P.alive = 0;
    }
    else
    {
        P.x = W / 2 - 6;
        P.y = H - 9;
        P.hp = P.maxHp;
        P.invTime = 120;
        P.power = 1;
    }
}

void checkCollisions()
{
    int px1 = (int)P.x, px2 = (int)P.x + 11;
    int py1 = (int)P.y - 1, py2 = (int)P.y + 4;

    for (int i = 0; i < MAX_BULLET; i++)
    {
        if (!pB[i].active) continue;
        int bx = (int)pB[i].x, by = (int)pB[i].y;

        for (int j = 0; j < MAX_ENEMY; j++)
        {
            if (!E[j].active) continue;
            int ex = (int)E[j].x, ey = (int)E[j].y;

            if (bx >= ex-1 && bx <= ex+7 && by >= ey && by <= ey+3)
            {
                pB[i].active = 0;
                E[j].hp -= pB[i].dmg;

                if (E[j].hp <= 0)
                {
                    spawnExplosion(ex+3, ey+1, 2 + E[j].type, 0);
                    spawnPowerup(ex, ey, 0);
                    P.score += E[j].points * (1 + P.combo / 10);
                    E[j].active = 0;
                    G.enemyCount--;
                    G.kills++;
                    G.totalKills++;
                    addCombo();
                }
                break;
            }
        }

        if (B.active && pB[i].active)
        {
            if (bx >= (int)B.x && bx <= (int)B.x+32 && by >= (int)B.y && by <= (int)B.y+8)
            {
                pB[i].active = 0;
                B.hp -= pB[i].dmg;

                if (B.hp <= 0)
                {
                    spawnExplosion((int)B.x+15, (int)B.y+4, 8, 1);
                    P.score += B.points;
                    B.active = 0;
                    G.bossDefeated = 1;
                    G.bossActive = 0;
                }
            }
        }
    }

    if (P.invTime <= 0 && P.alive)
    {
        for (int i = 0; i < MAX_EBULLET; i++)
        {
            if (!eB[i].active) continue;
            int bx = (int)eB[i].x, by = (int)eB[i].y;

            if (bx >= px1 && bx <= px2 && by >= py1 && by <= py2)
            {
                eB[i].active = 0;
                P.hp -= 10 * eB[i].dmg;
                if (P.hp <= 0) destroyPlayer();
            }
        }
    }

    if (P.invTime <= 0 && P.alive)
    {
        for (int i = 0; i < MAX_ENEMY; i++)
        {
            if (!E[i].active) continue;
            int ex1 = (int)E[i].x, ex2 = (int)E[i].x + 7;
            int ey1 = (int)E[i].y, ey2 = (int)E[i].y + 3;

            if (px2 > ex1 && px1 < ex2 && py2 > ey1 && py1 < ey2)
            {
                spawnExplosion(ex1+3, ey1+1, 2, 0);
                E[i].active = 0;
                G.enemyCount--;
                destroyPlayer();
            }
        }
    }

    if (B.active && P.invTime <= 0 && P.alive)
    {
        if (px2 > (int)B.x && px1 < (int)B.x+32 && py2 > (int)B.y && py1 < (int)B.y+8)
        {
            destroyPlayer();
        }
    }

    for (int i = 0; i < MAX_POWER; i++)
    {
        if (!PW[i].active) continue;

        if (PW[i].x >= px1-2 && PW[i].x <= px2+2 && PW[i].y >= py1-1 && PW[i].y <= py2+1)
        {
            switch (PW[i].type)
            {
            case 0:
                P.hp += 30;
                if (P.hp > P.maxHp) P.hp = P.maxHp;
                break;
            case 1:
                P.power++;
                if (P.power > 3) P.power = 3;
                break;
            case 2:
                P.lives++;
                break;
            case 3:
                P.score += 500;
                break;
            }
            PW[i].active = 0;
            Beep(1000, 20);
        }
    }
}

void drawHUD()
{
    for (int x = 0; x < W; x++) put(x, 0, 205, 3);
    for (int x = 0; x < W; x++) put(x, 2, 196, 1);

    char buf[60];

    sprintf(buf, "LEVEL %d", G.level);
    puts2(3, 1, buf, 14);
    sprintf(buf, "WAVE %d/%d", G.wave, G.maxWave);
    puts2(16, 1, buf, 7);

    sprintf(buf, "SCORE: %010d", P.score);
    drawCenter(1, buf, 10);

    puts2(W-35, 1, "LIVES:", 14);
    for (int i = 0; i < P.lives && i < 5; i++) put(W-28+i*2, 1, 3, 12);

    puts2(3, 2, "HP", 12);
    put(6, 2, '[', 7);
    int hpCol = P.hp > 60 ? 10 : (P.hp > 30 ? 14 : 12);
    bar(7, 2, 20, P.hp, P.maxHp, hpCol, 8, 1);
    put(27, 2, ']', 7);

    puts2(56, 2, "PWR:", 14);
    for (int i = 0; i < 3; i++) put(61+i*2, 2, i < P.power ? 254 : 250, i < P.power ? 14 : 8);

    if (P.combo > 0)
    {
        sprintf(buf, "%dx COMBO!", P.combo);
        puts2(W-30, 2, buf, P.combo >= 10 ? 14 : 6);
    }

    const char* diff[] = {"CADET", "WARRIOR", "VETERAN", "NIGHTMARE"};
    int dcol[] = {10, 11, 14, 12};
    puts2(W-20, 1, diff[G.diff-1], dcol[G.diff-1]);
    puts2(W-16, 2, P.name, 13);

    for (int x = 0; x < W; x++) put(x, H-1, 205, 3);
    sprintf(buf, "KILLS: %d", G.totalKills);
    puts2(W-15, H-1, buf, 14);
}

void drawShip()
{
    if (!P.alive) return;
    if (P.invTime > 0 && G.tick % 5 < 2) return;

    int c1 = 10;
    int c2 = 2;
    int x = (int)P.x, y = (int)P.y;

    put(x+5, y-1, '^', c1);

    put(x+4, y, '/', c1);
    put(x+5, y, 'A', 15);
    put(x+6, y, '\\', c1);

    put(x+1, y+1, '<', c2);
    put(x+2, y+1, '=', c2);
    put(x+3, y+1, '#', c1);
    put(x+4, y+1, '=', c1);
    put(x+5, y+1, '#', c1);
    put(x+6, y+1, '=', c1);
    put(x+7, y+1, '#', c1);
    put(x+8, y+1, '=', c2);
    put(x+9, y+1, '>', c2);

    put(x+2, y+2, '\\', c1);
    put(x+3, y+2, '=', c1);
    put(x+4, y+2, '=', c1);
    put(x+5, y+2, '=', c1);
    put(x+6, y+2, '=', c1);
    put(x+7, y+2, '=', c1);
    put(x+8, y+2, '/', c1);

    int fc = (G.tick % 4 < 2) ? 14 : 12;
    put(x+4, y+3, 'V', fc);
    put(x+6, y+3, 'V', fc);
}

void drawEnemies()
{
    for (int i = 0; i < MAX_ENEMY; i++)
    {
        if (!E[i].active) continue;

        int c;
        switch (E[i].type)
        {
        case 0:
            c = 6;
            break;

        default:
            c = 7;
        }

        if (E[i].hp < E[i].maxHp && G.tick % 4 < 2) c = 15;

        int x = (int)E[i].x, y = (int)E[i].y;

        put(x+2, y, '/', c);
        put(x+3, y, 'V', c+8);
        put(x+4, y, '\\', c);

        put(x+1, y+1, '|', c);
        put(x+2, y+1, '#', c+8);
        put(x+3, y+1, '#', c+8);
        put(x+4, y+1, '#', c+8);
        put(x+5, y+1, '|', c);

        put(x+2, y+2, '\\', c);
        put(x+3, y+2, 'v', c+8);
        put(x+4, y+2, '/', c);
    }
}

void drawBoss()
{
    if (!B.active) return;

    int c = B.phase == 4 ? 15 : (B.phase == 3 ? 14 : (B.phase == 2 ? 12 : 4));
    int x = (int)B.x, y = (int)B.y;

    const char* names[] = {"SCOUT", "DESTROYER", "CRUISER", "DREADNOUGHT", "OVERLORD"};
    int ni = G.level-1;
    if (ni > 4) ni = 4;
    char buf[40];
    sprintf(buf, "<<< %s >>>", names[ni]);
    drawCenter(y-1, buf, c);

    put(x+5, y, '[', 7);
    bar(x+6, y, 22, B.hp, B.maxHp, c, 8, 1);
    put(x+28, y, ']', 7);

    puts2(x+6, y+1, "/==========\\", c);

    put(x+3, y+2, '<', c);
    put(x+4, y+2, '[', c);
    for (int i = 0; i < 12; i++) put(x+5+i, y+2, '#', c);
    put(x+17, y+2, ']', c);
    put(x+18, y+2, '>', c);

    put(x+3, y+3, '<', c);
    put(x+4, y+3, '[', c);
    for (int i = 0; i < 12; i++) put(x+5+i, y+3, '#', c);
    put(x+17, y+3, ']', c);
    put(x+18, y+3, '>', c);

    put(x+3, y+4, '<', c);
    put(x+4, y+4, '[', c);
    for (int i = 0; i < 12; i++) put(x+5+i, y+4, '=', c);
    put(x+17, y+4, ']', c);
    put(x+18, y+4, '>', c);

    puts2(x+6, y+5, "\\===VVVV===/", c);

    sprintf(buf, "PHASE %d", B.phase);
    puts2(x+8, y+6, buf, c);
}

void drawBullets()
{
    for (int i = 0; i < MAX_BULLET; i++)
    {
        if (pB[i].active)
        {
            int c = pB[i].dmg >= 3 ? 11 : (pB[i].dmg >= 2 ? 10 : 14);
            put((int)pB[i].x, (int)pB[i].y, '|', c);
        }
    }

    for (int i = 0; i < MAX_EBULLET; i++)
    {
        if (eB[i].active)
        {
            put((int)eB[i].x, (int)eB[i].y, 'o', 12);
        }
    }
}

void drawPowerups()
{
    const char sym[] = {'+', 'P', '1', '$'};
    const int col[] = {10, 14, 12, 13};

    for (int i = 0; i < MAX_POWER; i++)
    {
        if (PW[i].active)
        {
            int c = (PW[i].time < 80 && G.tick % 6 < 3) ? 8 : col[PW[i].type];
            put(PW[i].x, PW[i].y, '[', c);
            put(PW[i].x+1, PW[i].y, sym[PW[i].type], c);
            put(PW[i].x+2, PW[i].y, ']', c);
        }
    }
}

void render2()
{
    cls();
    drawBackground();
    drawHUD();
    drawPowerups();
    drawEnemies();
    drawBoss();
    drawBullets();
    drawParticles();
    drawNotifs();
    if (P.alive) drawShip();
    flip();
}

void showLevelIntro()
{
    for (int f = 0; f < 30; f++)
    {
        cls();
        G.tick = f;
        drawStarFieldSimple();
        flip();
        Sleep(25);
    }

    cls();
    drawStarFieldSimple();
    box(W/2-40, 15, 80, 18, 11, 1);

    char buf[50];
    sprintf(buf, "======== LEVEL %d ========", G.level);
    drawCenter(19, buf, 14);

    const char* names[] = {"OUTER RIM", "ASTEROID FIELD", "NEBULA SECTOR", "DARK VOID", "ZORGON CORE"};
    drawCenter(23, names[(G.level-1)%5], 11);

    sprintf(buf, "Waves: %d    Difficulty: %s", G.maxWave, G.diff==1?"CADET":G.diff==2?"WARRIOR":G.diff==3?"VETERAN":"NIGHTMARE");
    drawCenter(26, buf, 7);

    flip();
    Sleep(2000);

    for (int i = 3; i > 0; i--)
    {
        cls();
        drawStarFieldSimple();
        box(W/2-10, 20, 20, 8, 14, 1);
        sprintf(buf, "%d", i);
        drawCenter(23, buf, 12);
        flip();
        Beep(600, 100);
        Sleep(500);
    }

    cls();
    drawStarFieldSimple();
    box(W/2-12, 20, 24, 8, 10, 1);
    drawCenter(23, "LAUNCH!", 10);
    flip();
    Beep(1000, 150);
    Sleep(400);
}

void showPause()
{
    box(W/2-30, 17, 60, 14, 14, 1);
    drawCenter(21, "=== PAUSED ===", 14);
    drawCenter(25, "[P] Resume    [ESC] Quit", 7);
    flip();

    Sleep(200);
    while (G.paused && !G.over)
    {
        if (GetAsyncKeyState('P') & 0x8000)
        {
            G.paused = 0;
            Sleep(200);
        }
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) G.over = 1;
        Sleep(30);
    }
}

void showVictory()
{
    for (int f = 0; f < 80; f++)
    {
        cls();
        for (int i = 0; i < 25; i++)
            put(rand()%W, rand()%H, "*+#"[rand()%3], (rand()%8)+8);
        flip();
        Sleep(20);
    }

    cls();
    drawStarFieldSimple();
    box(W/2-40, 12, 80, 24, 14, 1);

    drawCenter(16, "*** VICTORY! ***", 14);
    drawCenter(19, "LEVEL COMPLETE!", 10);

    char buf[60];
    sprintf(buf, "Score: %d", P.score);
    drawCenter(23, buf, 15);

    sprintf(buf, "Enemies destroyed: %d", G.kills);
    drawCenter(25, buf, 11);

    sprintf(buf, "Max Combo: %dx", P.maxCombo);
    drawCenter(27, buf, 6);

    int bonus = G.kills * 25 + P.hp * 8 + P.lives * 200 + P.maxCombo * 50;
    P.score += bonus;
    sprintf(buf, "Level Bonus: +%d", bonus);
    drawCenter(30, buf, 13);

    drawCenter(34, "Press ENTER to continue", 7);
    flip();

    while (1)
    {
        if (_kbhit() && _getch() == 13) break;
        Sleep(30);
    }
}

void showGameOver()
{
    for (int f = 0; f < 30; f++)
    {
        cls();
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                if (rand()%25 == 0) put(x, y, '#', 4);
        flip();
        Sleep(40);
    }

    cls();
    drawStarFieldSimple();
    box(W/2-40, 12, 80, 24, 12, 1);

    drawCenter(16, "=== GAME OVER ===", 12);

    char buf[60];
    sprintf(buf, "Final Score: %d", P.score);
    drawCenter(21, buf, 14);

    sprintf(buf, "Level Reached: %d", G.level);
    drawCenter(24, buf, 11);

    sprintf(buf, "Total Kills: %d", G.totalKills);
    drawCenter(26, buf, 10);

    sprintf(buf, "Max Combo: %dx", P.maxCombo);
    drawCenter(28, buf, 6);

    drawCenter(34, "Press ENTER", 7);
    flip();

    while (1)
    {
        if (_kbhit() && _getch() == 13) break;
        Sleep(30);
    }
}

void showFinalVictory()
{
    for (int f = 0; f < 120; f++)
    {
        cls();
        for (int i = 0; i < 35; i++)
            put(rand()%W, rand()%H, "*+#@%"[rand()%5], (rand()%8)+8);
        flip();
        Sleep(20);
    }

    cls();
    drawStarFieldSimple();
    box(W/2-50, 8, 100, 32, 14, 1);

    drawCenter(12, "*** C O N G R A T U L A T I O N S ***", 14);

    char buf[70];
    sprintf(buf, "Commander %s", P.name);
    drawCenter(16, buf, 11);

    drawCenter(19, "YOU HAVE SAVED HUMANITY!", 10);
    drawCenter(21, "The Zorgon Empire has been annihilated!", 15);

    sprintf(buf, "FINAL SCORE: %d", P.score);
    drawCenter(25, buf, 14);

    const char* ranks[] = {"Rookie", "Pilot", "Ace", "Commander", "Admiral", "Fleet Admiral", "LEGEND"};
    int ri = P.score / 10000;
    if (ri > 6) ri = 6;
    sprintf(buf, "RANK: %s", ranks[ri]);
    drawCenter(28, buf, 13);

    sprintf(buf, "Enemies Destroyed: %d    Max Combo: %dx", G.totalKills, P.maxCombo);
    drawCenter(31, buf, 15);

    drawCenter(37, "Press ENTER", 7);
    flip();

    while (1)
    {
        if (_kbhit() && _getch() == 13) break;
        Sleep(30);
    }
}

void playLevel()
{
    // Reset
    for (int i = 0; i < MAX_BULLET; i++) pB[i].active = 0;
    for (int i = 0; i < MAX_EBULLET; i++) eB[i].active = 0;
    for (int i = 0; i < MAX_ENEMY; i++) E[i].active = 0;
    for (int i = 0; i < MAX_POWER; i++) PW[i].active = 0;
    for (int i = 0; i < MAX_PARTICLE; i++) PT[i].life = 0;

    B.active = 0;
    G.wave = 1;
    G.enemyCount = 0;
    G.kills = 0;
    G.bossActive = 0;
    G.bossDefeated = 0;
    G.maxWave = 3 + G.diff + (G.level - 1);
    P.alive = 1;
    P.maxCombo = 0;
    P.combo = 0;

    showLevelIntro();
    spawnWave();

    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    double targetTime = 1.0 / 60.0;

    while (!G.over && !G.bossDefeated)
    {
        QueryPerformanceCounter(&start);
        G.tick++;

        handleInput();

        if (G.paused)
        {
            showPause();
            continue;
        }

        updatePlayer();
        updateStars();
        updateBullets();
        updateEnemies();
        updateBoss();
        updatePowerups();
        updateParticles();
        updateNotifs();
        checkCollisions();

        if (!B.active && G.enemyCount == 0 && P.alive)
        {
            if (G.wave < G.maxWave)
            {
                G.wave++;
                Sleep(300);
                spawnWave();
            }
            else if (!G.bossActive)
            {
                spawnBoss();
            }
        }

        render2();

        QueryPerformanceCounter(&end);
        double elapsed = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart;
        if (elapsed < targetTime)
        {
            Sleep((DWORD)((targetTime - elapsed) * 1000));
        }
    }

    if (G.bossDefeated && !G.over)
    {
        showVictory();
    }
}

int main()
{
    srand((unsigned)time(NULL));
    initConsole();

    showIntro();

    int running = 1;
    while (running)
    {
        int choice = mainMenu();

        switch (choice)
        {
        case 0:
            showControls();
            initGame();

            while (G.level <= 5 && !G.over)
            {
                playLevel();
                if (G.bossDefeated && !G.over)
                {
                    G.level++;
                    P.hp = P.maxHp;
                    G.bossDefeated = 0;
                }
            }

            if (G.over && P.lives <= 0) showGameOver();
            else if (G.level > 5) showFinalVictory();
            break;

        case 1:
            showControls();
            break;

        case 2:
            running = 0;
            break;
        }
    }

    SetConsoleActiveScreenBuffer(hCon);
    CloseHandle(hBuf[0]);
    CloseHandle(hBuf[1]);

    return 0;
}

