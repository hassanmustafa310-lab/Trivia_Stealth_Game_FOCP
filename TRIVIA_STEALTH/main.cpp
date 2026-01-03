#include "raylib.h"
#include <vector>
#include <string>
#include <ctime>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <random>

using namespace std;

//  Constants
const int TILE_SIZE = 60;
const int COLS = 20;
const int ROWS = 15;

const int UI_HEIGHT = 80;
const int SCREEN_WIDTH = COLS * TILE_SIZE;          // 1200 px
const int SCREEN_HEIGHT = (ROWS * TILE_SIZE) + UI_HEIGHT; // 980 px

// Colors
const Color BG_COLOR = { 20, 20, 30, 255 };
const Color COL_WALL = { 50, 50, 65, 255 };
const Color COL_WALL_SHADOW = { 10, 10, 15, 200 };
const Color COL_PLAYER = { 0, 228, 48, 255 };
const Color COL_ENEMY_SLOW = { 230, 41, 55, 255 };
const Color COL_ENEMY_FAST = { 255, 161, 0, 255 };
const Color COL_DIAMOND = { 0, 240, 255, 255 };
const Color COL_NUGGET = { 218, 165, 32, 255 }; // Goldenrod
const Color COL_INVISIBLE = { 100, 255, 218, 100 };
const Color COL_UI_PANEL = { 15, 15, 20, 255 };

//  Enums
enum GameState { MENU, PLAYING, QUIZ, FROZEN, GAME_OVER, VICTORY, HELP };
enum TileType { TILE_EMPTY = 0, TILE_WALL = 1, TILE_EXIT = 2 };

//  Structs
struct GridPos {
    int x, y;
};

struct Question {
    string text;
    string options[3];
    int correctIndex; // 0, 1, or 2
};

struct Player {
    GridPos pos;
    float moveTimer;
    float invisibleTimer;
    float freezeTimer;
    float stamina;
};

struct Enemy {
    GridPos pos;
    float moveTimer;
    float speed;
};

// --- Global Data ---
vector<vector<int>> gameGrid;
Player player;
vector<Enemy> enemies;
vector<GridPos> nuggets;
vector<GridPos> diamonds;
GameState currentState;
Question currentQuestion;

// Random engine setup
std::mt19937 rng;
vector<int> questionIndices; // To track which questions have been used

// --- Level Design ---
const string LEVEL_LAYOUT[15] = {
    "11111111111111111111",
    "19000001000000010001",
    "10111101011111010101",
    "10100000000000000101",
    "10101111101111110101",
    "10001000000000010001",
    "11101010111101010111",
    "10000010000001000001",
    "10111111111111110101",
    "10001000000000000001",
    "10101011111101111101",
    "10100000010000000101",
    "10111111010111110101",
    "10000001000000000021",
    "11111111111111111111"
};

// --- QUESTION BANK ---
vector<Question> questionBank = {
    //SPECIAL QUESTION
    {"Who is the best Computer programing Professor?", {"Jaudat Mamoon", "David Malan", "Andrew Ng"}, 0},

    // Fun General Knowledge
    {"Which planet has the most rings?", {"Saturn", "Jupiter", "Mars"}, 0},
    {"What is the largest organ on the human body?", {"Liver", "Skin", "Heart"}, 1},
    {"Who painted the Mona Lisa?", {"Van Gogh", "Picasso", "Da Vinci"}, 2},
    {"Which country gave the Statue of Liberty to the USA?", {"France", "England", "Spain"}, 0},
    {"What color is a polar bear's skin?", {"White", "Pink", "Black"}, 2},
    {"In 'The Matrix', which pill does Neo take?", {"Red", "Blue", "Green"}, 0},
    {"A group of Crows is called a...", {"Pack", "Murder", "School"}, 1},
    {"Which is the only mammal that can fly?", {"Bat", "Flying Squirrel", "Ostrich"}, 0},
};

// --- Helper: Shuffle Questions & Rig the Deck ---
void ShuffleQuestions() {
    questionIndices.clear();
    int specialIndex = -1;

    // 1. Fill list and find your special question index
    for(size_t i = 0; i < questionBank.size(); ++i) {
        questionIndices.push_back(i);
        // Identify your question by looking for "Jaudat Mamoon" in the options
        if (questionBank[i].options[0].find("Jaudat Mamoon") != string::npos) {
            specialIndex = i;
        }
    }

    // 2. Shuffle everything normally
    std::shuffle(questionIndices.begin(), questionIndices.end(), rng);

    // 3. FORCE the special question to be in the "Active Zone"
    // Since we pop from the back of the vector, the "next 3 questions" are the last 3 in the list.
    if (specialIndex != -1) {
        // Find where the shuffle put it
        int currentPos = -1;
        for(size_t i=0; i<questionIndices.size(); i++) {
            if(questionIndices[i] == specialIndex) {
                currentPos = i;
                break;
            }
        }

        // We have 3 nuggets, so we need it to be one of the last 3 items
        int poolSize = 3;
        if (questionIndices.size() < 3) poolSize = questionIndices.size();

        // Pick a random spot in the "top 3" (which is actually the bottom 3 of the vector)
        int randomOffset = rand() % poolSize; // 0, 1, or 2
        int targetPos = questionIndices.size() - 1 - randomOffset;

        // Swap it into place
        std::swap(questionIndices[currentPos], questionIndices[targetPos]);
    }
}

// --- Initialization ---
void LoadLevel() {
    gameGrid.assign(ROWS, vector<int>(COLS, TILE_WALL));
    nuggets.clear();
    diamonds.clear();
    enemies.clear();

    // Reshuffle (and rig) questions when level loads
    ShuffleQuestions();

    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            char tile = LEVEL_LAYOUT[y][x];
            if (tile == '1') gameGrid[y][x] = TILE_WALL;
            else if (tile == '0') gameGrid[y][x] = TILE_EMPTY;
            else if (tile == '2') gameGrid[y][x] = TILE_EXIT;
            else if (tile == '9') {
                gameGrid[y][x] = TILE_EMPTY;
                player.pos = {x, y};
            }
        }
    }

    // Spawn Diamonds
    int diamondCount = 0;
    while(diamondCount < 5) {
        int nx = rand() % COLS;
        int ny = rand() % ROWS;
        if (gameGrid[ny][nx] == TILE_EMPTY && !(nx == player.pos.x && ny == player.pos.y)) {
             bool exists = false;
             for(auto& d : diamonds) if(d.x == nx && d.y == ny) exists = true;
             if(!exists) { diamonds.push_back({nx, ny}); diamondCount++; }
        }
    }

    // Spawn Nuggets (Quiz triggers)
    int nuggetCount = 0;
    while (nuggetCount < 3) {
        int nx = rand() % COLS;
        int ny = rand() % ROWS;
        if (gameGrid[ny][nx] == TILE_EMPTY && !(nx == player.pos.x && ny == player.pos.y)) {
            bool exists = false;
            for (auto &d: diamonds) if (d.x == nx && d.y == ny) exists = true;
            for (auto &n: nuggets) if (n.x == nx && n.y == ny) exists = true;
            if (!exists) {
                nuggets.push_back({nx, ny});
                nuggetCount++;
            }
        }
    }

    // Spawn Enemies
    int enemyCount = 0;
    while(enemyCount < 6) {
        int ex = rand() % COLS;
        int ey = rand() % ROWS;
        if (gameGrid[ey][ex] == TILE_EMPTY && (abs(ex - player.pos.x) + abs(ey - player.pos.y) > 8)) {
             float speed = 0.28f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 0.4f));
             enemies.push_back({{ex, ey}, 0.0f, speed});
             enemyCount++;
        }
    }
}

void ResetGame() {
    player.invisibleTimer = 0;
    player.freezeTimer = 0;
    player.stamina = 100.0f;
    player.moveTimer = 0;
    currentState = PLAYING;
    LoadLevel();
}

// --- Logic ---

bool IsValidMove(int x, int y) {
    if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return false;
    if (gameGrid[y][x] == TILE_WALL) return false;
    return true;
}

void UpdatePlayer() {
    if (player.freezeTimer > 0) {
        player.freezeTimer -= GetFrameTime();
        if (player.freezeTimer <= 0) currentState = PLAYING;
        return;
    }

    if (player.invisibleTimer > 0) player.invisibleTimer -= GetFrameTime();

    // Stamina Regen
    if (!IsKeyDown(KEY_LEFT_SHIFT) && player.stamina < 100.0f) {
        player.stamina += 40.0f * GetFrameTime();
    }

    // Smoother Movement Settings
    float moveDelay = 0.12f;
    if (IsKeyDown(KEY_LEFT_SHIFT) && player.stamina > 0) {
        moveDelay = 0.06f; // speed of the player
        player.stamina -= 60.0f * GetFrameTime();
    }

    player.moveTimer += GetFrameTime();

    if (player.moveTimer >= moveDelay) {
        int dx = 0;
        int dy = 0;

        if (IsKeyDown(KEY_UP)) dy = -1;
        if (IsKeyDown(KEY_DOWN)) dy = 1;
        if (IsKeyDown(KEY_LEFT)) dx = -1;
        if (IsKeyDown(KEY_RIGHT)) dx = 1;

        if (dx != 0 || dy != 0) {
            player.moveTimer = 0;
            bool moved = false;

            if (dy != 0) {
                if (IsValidMove(player.pos.x, player.pos.y + dy)) {
                    player.pos.y += dy;
                    moved = true;
                }
            }

            if (!moved && dx != 0) {
                if (IsValidMove(player.pos.x + dx, player.pos.y)) {
                    player.pos.x += dx;
                    moved = true;
                }
            }
        }
    }

    if (player.stamina < 0) player.stamina = 0;
    if (player.stamina > 100) player.stamina = 100;

    if (gameGrid[player.pos.y][player.pos.x] == TILE_EXIT) {
        if (diamonds.empty()) currentState = VICTORY;
    }

    for (size_t i = 0; i < diamonds.size(); i++) {
        if (player.pos.x == diamonds[i].x && player.pos.y == diamonds[i].y) {
            diamonds.erase(diamonds.begin() + i);
            break;
        }
    }

    for (size_t i = 0; i < nuggets.size(); i++) {
        if (player.pos.x == nuggets[i].x && player.pos.y == nuggets[i].y) {

            // --- RANDOM LOGIC ---
            // If we ran out of unique questions, reshuffle
            if (questionIndices.empty()) ShuffleQuestions();

            // Get the next unique index
            int idx = questionIndices.back();
            questionIndices.pop_back();

            currentQuestion = questionBank[idx];
            // ------------------------

            currentState = QUIZ;
            nuggets.erase(nuggets.begin() + i);
            break;
        }
    }
}

void UpdateEnemies() {
    for (auto& enemy : enemies) {
        enemy.moveTimer += GetFrameTime();
        float currentSpeed = (currentState == FROZEN) ? enemy.speed * 0.5f : enemy.speed;

        if (enemy.moveTimer >= currentSpeed) {
            enemy.moveTimer = 0;

            GridPos bestMove = enemy.pos;
            int minDist = 9999;
            GridPos moves[4] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

            vector<int> indices = {0, 1, 2, 3};
            // Use std::shuffle for better enemy randomness when invisible
            if (player.invisibleTimer > 0) std::shuffle(indices.begin(), indices.end(), rng);

            bool moved = false;

            for (int i : indices) {
                int nx = enemy.pos.x + moves[i].x;
                int ny = enemy.pos.y + moves[i].y;

                if (nx >= 0 && nx < COLS && ny >= 0 && ny < ROWS && gameGrid[ny][nx] != TILE_WALL) {
                    if (player.invisibleTimer > 0) {
                        enemy.pos = {nx, ny};
                        moved = true;
                        break;
                    } else {
                        int dist = abs(nx - player.pos.x) + abs(ny - player.pos.y);
                        if (dist < minDist) {
                            minDist = dist;
                            bestMove = {nx, ny};
                        }
                    }
                }
            }
            if (!moved && player.invisibleTimer <= 0) enemy.pos = bestMove;
        }

        if (enemy.pos.x == player.pos.x && enemy.pos.y == player.pos.y) {
            if (player.invisibleTimer <= 0) currentState = GAME_OVER;
        }
    }
}

// --- Drawing Functions ---

void DrawGameMap() {
    DrawRectangle(0, UI_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT - UI_HEIGHT, BG_COLOR);

    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            Rectangle rect = { (float)x * TILE_SIZE, (float)y * TILE_SIZE + UI_HEIGHT, (float)TILE_SIZE, (float)TILE_SIZE };

            if (gameGrid[y][x] == TILE_WALL) {
                DrawRectangleRounded({rect.x + 4, rect.y + 4, rect.width, rect.height}, 0.2f, 4, COL_WALL_SHADOW);
                DrawRectangleRounded(rect, 0.2f, 4, COL_WALL);
                DrawRectangleRounded({rect.x + 5, rect.y + 5, rect.width - 10, rect.height/3}, 0.2f, 4, Fade(WHITE, 0.05f));
            }
            else if (gameGrid[y][x] == TILE_EXIT) {
                if (diamonds.empty()) {
                    float alpha = (sin(GetTime() * 3.0f) + 1.0f) / 2.0f;
                    DrawRectangleRec(rect, Fade(GREEN, 0.3f));
                    DrawRectangleLines(rect.x, rect.y, rect.width, rect.height, Fade(LIME, alpha));
                    DrawText("EXIT", x * TILE_SIZE + 10, y * TILE_SIZE + UI_HEIGHT + 15, 10, WHITE);
                } else {
                    DrawRectangleRec(rect, Fade(RED, 0.2f));
                    DrawRectangleLines(rect.x, rect.y, rect.width, rect.height, RED);
                    DrawText("LOCKED", x * TILE_SIZE + 2, y * TILE_SIZE + UI_HEIGHT + 20, 10, RED);
                }
            }
        }
    }
}

void DrawEntities() {
    float offset = TILE_SIZE / 2.0f;

    // Diamonds
    for (const auto& d : diamonds) {
        Vector2 center = { d.x * TILE_SIZE + offset, d.y * TILE_SIZE + offset + UI_HEIGHT };
        float rot = GetTime() * 2.0f;
        DrawPoly(center, 4, 15, rot * 50, COL_DIAMOND);
        DrawPolyLines(center, 4, 17, rot * 50, WHITE);
    }

    // Nuggets
    for (const auto& n : nuggets) {
        float scale = (sin(GetTime() * 5.0f) + 2.0f) / 2.0f;
        Vector2 center = { n.x * TILE_SIZE + offset, n.y * TILE_SIZE + offset + UI_HEIGHT };
        DrawCircleV(center, 8 * scale, Fade(COL_NUGGET, 0.4f));
        DrawCircleV(center, 7, COL_NUGGET);
    }

    // Player
    Color pColor = COL_PLAYER;
    if (player.invisibleTimer > 0) pColor = COL_INVISIBLE;
    if (player.freezeTimer > 0) pColor = SKYBLUE;

    Rectangle pRect = {
        player.pos.x * TILE_SIZE + 6.0f,
        player.pos.y * TILE_SIZE + 6.0f + UI_HEIGHT,
        TILE_SIZE - 12.0f,
        TILE_SIZE - 12.0f
    };

    DrawRectangleRounded({pRect.x + 3, pRect.y + 3, pRect.width, pRect.height}, 0.3f, 6, Fade(BLACK, 0.4f));
    DrawRectangleRounded(pRect, 0.3f, 6, pColor);
    DrawCircle(pRect.x + 12, pRect.y + 12, 4, BLACK);
    DrawCircle(pRect.x + 28, pRect.y + 12, 4, BLACK);

    // Enemies
    for (const auto& e : enemies) {
        Vector2 center = { e.pos.x * TILE_SIZE + offset, e.pos.y * TILE_SIZE + offset + UI_HEIGHT };
        Color eColor = (e.speed > 0.45f) ? COL_ENEMY_SLOW : COL_ENEMY_FAST;

        DrawCircleV({center.x + 3, center.y + 3}, 18, Fade(BLACK, 0.4f));
        DrawCircleV(center, 18, eColor);
        DrawCircleLines(center.x, center.y, 18, BLACK);
        DrawLineEx({center.x - 8, center.y - 4}, {center.x - 2, center.y + 4}, 3, BLACK);
        DrawLineEx({center.x + 8, center.y - 4}, {center.x + 2, center.y + 4}, 3, BLACK);
    }
}

void DrawUI() {
    float time = GetTime(); // For animations

    if (currentState == MENU) {
        // --- 1. Background with Gradient ---
        DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BG_COLOR, {10, 10, 15, 255});

        // Background Deco (Spinning large diamond)
        DrawPolyLines({SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f + 50}, 4, 300, time * 10.0f, Fade(COL_DIAMOND, 0.05f));
        DrawPolyLines({SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f + 50}, 4, 280, time * -15.0f, Fade(COL_DIAMOND, 0.05f));

        // --- 2. Title with Shadow ---
        const char* title1 = "MAZE RUNNER";
        const char* title2 = "DIAMOND HEIST";
        int t1Width = MeasureText(title1, 40);
        int t2Width = MeasureText(title2, 50);

        DrawText(title1, SCREEN_WIDTH/2 - t1Width/2 + 4, 124, 40, BLACK);
        DrawText(title1, SCREEN_WIDTH/2 - t1Width/2, 120, 40, LIGHTGRAY);
        DrawText(title2, SCREEN_WIDTH/2 - t2Width/2 + 4, 164, 50, BLACK);
        DrawText(title2, SCREEN_WIDTH/2 - t2Width/2, 160, 50, COL_DIAMOND);

        // --- 3. Info Panel ---
        Rectangle panel = { SCREEN_WIDTH/2.0f - 220, 260, 440, 240 };
        DrawRectangleRounded(panel, 0.1f, 10, Fade(COL_UI_PANEL, 0.8f));
        DrawRectangleRoundedLines(panel, 0.1f, 10, Fade(COL_DIAMOND, 0.3f));

        DrawText("MISSION OBJECTIVES", panel.x + 110, panel.y + 20, 20, YELLOW);
        DrawRectangle(panel.x + 40, panel.y + 50, 360, 2, Fade(WHITE, 0.2f));

        DrawText("- Collect 5 Diamonds to Open Exit", panel.x + 40, panel.y + 70, 20, WHITE);
        DrawText("- Answer Trivia for Speed Boosts", panel.x + 40, panel.y + 110, 20, WHITE);
        DrawText("- Use SHIFT to Sprint (Costs Stamina)", panel.x + 40, panel.y + 150, 20, WHITE);
        DrawText("- Avoid the Guards!", panel.x + 40, panel.y + 190, 20, COL_ENEMY_FAST);

        // --- 4. Start Prompt (Pulsing) ---
        float pulse = (sin(time * 5.0f) + 1.0f) / 2.0f; // 0 to 1
        Color startColor = Fade(WHITE, 0.5f + (pulse * 0.5f));
        const char* startText = "PRESS [ENTER] TO START";
        int sWidth = MeasureText(startText, 30);

        DrawText(startText, SCREEN_WIDTH/2 - sWidth/2, 560, 30, startColor);

        // --- 5. Help Button Look ---
        Rectangle helpRect = { SCREEN_WIDTH/2.0f - 120, 630, 240, 40 };
        DrawRectangleRounded(helpRect, 0.5f, 6, Fade(SKYBLUE, 0.2f));
        DrawRectangleRoundedLines(helpRect, 0.5f, 6, SKYBLUE);
        DrawText("PRESS [H] FOR TIPS", helpRect.x + 35, helpRect.y + 10, 20, SKYBLUE);
    }
    else if (currentState == HELP) {
        DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, {15, 10, 10, 255}, BLACK);

        DrawText("STRUGGLING TO WIN?", 50, 50, 80, Fade(RED, 0.2f));
        DrawText("SURVIVAL GUIDE", SCREEN_WIDTH/2 - MeasureText("SURVIVAL GUIDE", 40)/2, 100, 40, GOLD);

        int startY = 200;
        int spacing = 60;
        int fontSize = 20;
        int xPos = 100;

        auto DrawTip = [&](int index, const char* text) {
            DrawRectangle(xPos - 20, startY + (index*spacing) + 5, 10, 10, COL_DIAMOND);
            DrawText(text, xPos, startY + (index*spacing), fontSize, WHITE);
        };

        DrawTip(0, "Use [SHIFT] to sprint out of sticky situations.");
        DrawTip(1, "Answer trivia questions correctly to enter ghost mode for 4 seconds.");
        DrawTip(2, "Increase distance from enemies to hide.");
        DrawTip(3, "Don't get cornered in dead ends.");
        DrawTip(4, "Enemies track you within a specific radius.");

        DrawText("PRESS [H] OR [ENTER] TO RETURN", SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT - 100, 20, LIGHTGRAY);
    }
    else {
        // Draw the top bar background for game
        if (currentState == PLAYING || currentState == FROZEN) {
            DrawRectangle(0, 0, SCREEN_WIDTH, UI_HEIGHT, COL_UI_PANEL);
            DrawLine(0, UI_HEIGHT, SCREEN_WIDTH, UI_HEIGHT, WHITE);

            // Diamonds
            DrawText("DIAMONDS:", 20, 30, 20, WHITE);
            for(int i=0; i<5; i++) {
                Color dCol = (i < (int)diamonds.size()) ? DARKGRAY : COL_DIAMOND;
                DrawRectangle(140 + (i*30), 25, 20, 30, dCol);
                DrawRectangleLines(140 + (i*30), 25, 20, 30, WHITE);
            }

            // Stamina
            DrawText("STAMINA:", 350, 30, 20, WHITE);
            DrawRectangle(460, 25, 200, 30, DARKGRAY);
            DrawRectangle(460, 25, (int)(player.stamina * 2.0f), 30, COL_PLAYER);
            DrawRectangleLines(460, 25, 200, 30, WHITE);

            // Right Info
            DrawText("MOVE: ARROWS", 720, 20, 10, LIGHTGRAY);
            DrawText("RUN: SHIFT", 720, 40, 10, LIGHTGRAY);

            if (player.invisibleTimer > 0)
                DrawText(TextFormat("GHOST: %.1f", player.invisibleTimer), 820, 30, 20, COL_DIAMOND);

            if (player.freezeTimer > 0) {
                 DrawText(TextFormat("FROZEN! %.1f", player.freezeTimer), SCREEN_WIDTH/2 - 60, SCREEN_HEIGHT/2 - 50, 40, RED);
            }
            if (gameGrid[player.pos.y][player.pos.x] == TILE_EXIT && !diamonds.empty()) {
                 DrawText("LOCKED!", SCREEN_WIDTH/2 - 50, SCREEN_HEIGHT - 60, 20, RED);
            }
        }
        else if (currentState == QUIZ) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, {0, 0, 0, 220});
            Rectangle box = { SCREEN_WIDTH/2.0f - 300, SCREEN_HEIGHT/2.0f - 200, 600, 400 };
            DrawRectangleRounded(box, 0.1f, 10, COL_UI_PANEL);
            DrawRectangleRoundedLines(box, 0.1f, 10, WHITE);

            DrawText("BONUS QUESTION", box.x + 180, box.y + 30, 30, COL_NUGGET);
            DrawText(currentQuestion.text.c_str(), box.x + 50, box.y + 100, 20, WHITE);
            DrawText(("1. " + currentQuestion.options[0]).c_str(), box.x + 50, box.y + 180, 20, WHITE);
            DrawText(("2. " + currentQuestion.options[1]).c_str(), box.x + 50, box.y + 230, 20, WHITE);
            DrawText(("3. " + currentQuestion.options[2]).c_str(), box.x + 50, box.y + 280, 20, WHITE);
            DrawText("Press 1, 2, or 3", box.x + 220, box.y + 350, 20, LIGHTGRAY);
        }
        else if (currentState == VICTORY) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(GREEN, 0.9f));
            DrawText("HEIST SUCCESSFUL!", SCREEN_WIDTH/2 - 180, SCREEN_HEIGHT/2 - 20, 40, WHITE);
            DrawText("[ENTER] to Play Again", SCREEN_WIDTH/2 - 120, SCREEN_HEIGHT/2 + 40, 20, BLACK);
        }
        else if (currentState == GAME_OVER) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(MAROON, 0.9f));
            DrawText("BUSTED!", SCREEN_WIDTH/2 - 80, SCREEN_HEIGHT/2 - 20, 40, WHITE);
            DrawText("[ENTER] to Retry", SCREEN_WIDTH/2 - 90, SCREEN_HEIGHT/2 + 40, 20, LIGHTGRAY);
        }
    }
}

// --- Main Loop ---
int main() {
    // Seed standard rand() for map generation
    srand(static_cast<unsigned int>(time(0)));

    // Seed modern RNG for questions
    std::random_device rd;
    rng.seed(rd());

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Maze Runner: Diamond Heist");
    SetTargetFPS(60);

    currentState = MENU;

    while (!WindowShouldClose()) {
        switch (currentState) {
            case MENU:
                if (IsKeyPressed(KEY_ENTER)) ResetGame();
                if (IsKeyPressed(KEY_H)) currentState = HELP;
                break;

            case HELP:
                if (IsKeyPressed(KEY_H) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
                    currentState = MENU;
                }
                break;

            case PLAYING:
                UpdatePlayer();
                UpdateEnemies();
                break;

            case FROZEN:
                UpdatePlayer();
                UpdateEnemies();
                break;

            case QUIZ:
            {
                int choice = -1;
                if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1)) choice = 0;
                if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2)) choice = 1;
                if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) choice = 2;

                if (choice != -1) {
                    if (choice == currentQuestion.correctIndex) {
                        player.invisibleTimer = 5.0f;
                        player.stamina = 100.0f;
                        currentState = PLAYING;
                    } else {
                        player.freezeTimer = 3.0f;
                        currentState = FROZEN;
                    }
                }
            }
            break;

            case GAME_OVER:
            case VICTORY:
                if (IsKeyPressed(KEY_ENTER)) currentState = MENU;
                break;
        }

        BeginDrawing();
            if (currentState != MENU && currentState != HELP) {
                // Game Background
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BG_COLOR);
                DrawGameMap();
                DrawEntities();
            }
            DrawUI();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
