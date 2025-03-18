/**
* Author: [Aayush Daftary]
* Assignment: Lunar Lander
* Date due: 2025-3-18, 11:59pm
* I pledge that I have completed this assignment without
* collaborating with anyone else, in conformance with the
* NYU School of Engineering Policies and Procedures on
* Academic Misconduct.
**/
#define LOG(argument) std::cout << argument << '\n'
#define STB_IMAGE_IMPLEMENTATION
#define GL_SILENCE_DEPRECATION
#define GL_GLEXT_PROTOTYPES 1

#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>
#include <vector>
#include "Entity.h"

// ————— STRUCTS AND ENUMS —————//
struct GameState
{
    Entity* player;
    Entity* platforms;
    Entity* messages;
};

// ————— CONSTANTS ————— //
const int WINDOW_WIDTH = 640,
WINDOW_HEIGHT = 480;

bool win_message = false;
bool lose_message = false;

// Background color components
constexpr float BG_RED     = 0.9765f,
                BG_GREEN   = 0.9726f,
                BG_BLUE    = 0.9609f,
                BG_OPACITY = 1.0f;

const int VIEWPORT_X = 0,
VIEWPORT_Y = 0,
VIEWPORT_WIDTH = WINDOW_WIDTH,
VIEWPORT_HEIGHT = WINDOW_HEIGHT;

const char V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
F_SHADER_PATH[] = "shaders/fragment_textured.glsl";

const float MILLISECONDS_IN_SECOND = 1000.0;

const char SHIP_FILEPATH[] = "ship.png";
const char PLATFORM_FILEPATH[] = "fire1.png";
const char MONSTER_FILEPATH[] = "monster1.png";
const char FULL_PATH[] = "full.png";
const char EMPTY_PATH[] = "empty.png";
const char HALF_PATH[] = "half.png";
const char LOSING_FILEPATH[] = "fail.png";
const char WINNING_FILEPATH[] = "success.png";

const int NUMBER_OF_TEXTURES = 1;  // to be generated, that is
const GLint LEVEL_OF_DETAIL = 0;  // base image level; Level n is the nth mipmap reduction image
const GLint TEXTURE_BORDER = 0;  // this value MUST be zero

// ————— VARIABLES ————— //
GameState g_state;

SDL_Window* g_display_window;
bool g_game_is_running = true;

ShaderProgram g_shader_program;
glm::mat4 g_view_matrix, g_projection_matrix;

float g_previous_ticks = 0.0f;
float g_time_accumulator = 0.0f;
constexpr float ACC_OF_GRAVITY = -0.00001f;
float g_fuel = 1000.0f;
constexpr float FIXED_TIMESTEP = 0.0166666f;
constexpr int PLATFORM_COUNT = 13;
constexpr int FINISH = 12;
GLuint g_fuel_textures[3];

// ———— GENERAL FUNCTIONS ———— //
GLuint load_texture(const char* filepath)
{
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components, STBI_rgb_alpha);

    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }

    GLuint textureID;
    glGenTextures(NUMBER_OF_TEXTURES, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(image);

    return textureID;
}

void draw_object(glm::mat4 &model_matrix, GLuint &texture_id)
{
    g_shader_program.set_model_matrix(model_matrix);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glDrawArrays(GL_TRIANGLES, 0, 6); // we are now drawing 2 triangles, so use 6, not 3
}

void renderFuelUI()
{
    GLuint fuelTexture;
    if(g_fuel < 100.0f)
    {
        fuelTexture = g_fuel_textures[0];
    }
    else if(g_fuel < 750.0f && g_fuel > 100)
    {
        fuelTexture = g_fuel_textures[1];
    }
    else
    {
        fuelTexture = g_fuel_textures[2];
    }

    glm::mat4 fuel_model = glm::mat4(1.0f);
    float fuel_ui_x = 3.5f;
    float fuel_ui_y = 3.0f;
    float fuel_ui_width = 1.0f;
    float fuel_ui_height = 1.0f;
    fuel_model = glm::translate(fuel_model, glm::vec3(fuel_ui_x, fuel_ui_y, 0.0f));
    fuel_model = glm::scale(fuel_model, glm::vec3(fuel_ui_width, fuel_ui_height, 1.0f));

    float vertices[] = {
        -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,   // Triangle 1
        -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f    // Triangle 2
    };
    float texture_coordinates[] = {
        0.0f, 1.0f,  1.0f, 1.0f,  1.0f, 0.0f,   // Triangle 1
        0.0f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f    // Triangle 2
    };

    glVertexAttribPointer(g_shader_program.get_position_attribute(), 2, GL_FLOAT, false, 0, vertices);
    glEnableVertexAttribArray(g_shader_program.get_position_attribute());
    glVertexAttribPointer(g_shader_program.get_tex_coordinate_attribute(), 2, GL_FLOAT, false, 0, texture_coordinates);
    glEnableVertexAttribArray(g_shader_program.get_tex_coordinate_attribute());

    draw_object(fuel_model, fuelTexture);

    glDisableVertexAttribArray(g_shader_program.get_position_attribute());
    glDisableVertexAttribArray(g_shader_program.get_tex_coordinate_attribute());
}


void reset_game() {
    win_message = false;
    lose_message = false;
    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);
    g_state.player->set_position(glm::vec3(-3.25f, 2.5f, 0.0f));
    g_state.player->set_velocity(glm::vec3(0.0f));
    //g_state.player->set_movement(glm::vec3(0.0f));
    g_state.player->set_acceleration(glm::vec3(0.0f, ACC_OF_GRAVITY * 0.1, 0.0f));
    g_state.player->set_speed(1.0f);
    g_fuel = 1000.0f;

    for (int i = 0; i < PLATFORM_COUNT; i++) {
         g_state.platforms[i].touched = false;
    }
}

void initialise()
{
    SDL_Init(SDL_INIT_VIDEO);
    g_display_window = SDL_CreateWindow("Hello, Entities!",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL);

    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);

#ifdef _WINDOWS
    glewInit();
#endif
    g_fuel_textures[0] = load_texture(EMPTY_PATH);
    g_fuel_textures[1] = load_texture(HALF_PATH);
    g_fuel_textures[2] = load_texture(FULL_PATH);

    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    g_shader_program.load(V_SHADER_PATH, F_SHADER_PATH);

    g_view_matrix = glm::mat4(1.0f);
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);

    g_shader_program.set_projection_matrix(g_projection_matrix);
    g_shader_program.set_view_matrix(g_view_matrix);

    glUseProgram(g_shader_program.get_program_id());

    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);

    // ————— PLAYER ————— //
    g_state.player = new Entity();
    g_state.player->set_position(glm::vec3(-3.25f, 3.0f, 0.0f));
    //g_state.player->set_movement(glm::vec3(0.0f));
    g_state.player->set_acceleration(glm::vec3(0.0f, ACC_OF_GRAVITY*0.1, 0.0f));
    g_state.player->set_speed(1.0f);
    g_state.player->m_texture_id = load_texture(SHIP_FILEPATH);


    // ————— PLATFORM ————— //
    g_state.platforms = new Entity[PLATFORM_COUNT];

    for (int i = 0; i < 10; i++)
    {
        g_state.platforms[i].m_texture_id = load_texture(PLATFORM_FILEPATH);
        //platforms  across the bottom
        g_state.platforms[i].set_position(glm::vec3(i - 4.5f, -3.25f, 0.0f));
        g_state.platforms[i].update(0.0f, NULL, 0);
    }
    
    //extra blocks for a col of height 3
    g_state.platforms[10].m_texture_id = load_texture(PLATFORM_FILEPATH);
    g_state.platforms[10].set_position(glm::vec3(0.5f, -2.25f, 0.0f));
    g_state.platforms[10].update(0.0f, NULL, 0);
    
    g_state.platforms[11].m_texture_id = load_texture(PLATFORM_FILEPATH);
    g_state.platforms[11].set_position(glm::vec3(0.5f, -1.25f, 0.0f));
    g_state.platforms[11].update(0.0f, NULL, 0);

    GLuint monster_texture = load_texture(MONSTER_FILEPATH);
    g_state.platforms[12].m_texture_id = monster_texture;
    g_state.platforms[12].set_position(glm::vec3(4.5f, -2.25f, 0.0f));
    g_state.platforms[12].update(0.0f, NULL, 0);
    
    g_state.messages = new Entity[2];
    //winning msg
    g_state.messages[0].m_texture_id = load_texture(WINNING_FILEPATH);
    g_state.messages[0].set_position(glm::vec3(0.0f, 0.0f, 0.0f));
    g_state.messages[0].update(0.0f, NULL, 0);
    //
    // Losing msg
    g_state.messages[1].m_texture_id = load_texture(LOSING_FILEPATH);
    g_state.messages[1].set_position(glm::vec3(0.0f, 0.0f, 0.0f));
    g_state.messages[1].update(0.0f, NULL, 0);

    // ————— GENERAL ————— //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void process_input()
{
    // VERY IMPORTANT: If nothing is pressed, we don't want to go anywhere
    g_state.player->set_movement(glm::vec3(0.0f));

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
            // End game
        case SDL_QUIT:
        case SDL_WINDOWEVENT_CLOSE:
            g_game_is_running = false;
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_q:
                // Quit the game with a keystroke
                g_game_is_running = false;
                break;
            case SDLK_r:
                reset_game();
                break;

            default:
                break;
            }

        default:
            break;
        }
    }

    const Uint8* key_state = SDL_GetKeyboardState(NULL);
    if(g_fuel > 0)
    {
        if (key_state[SDL_SCANCODE_LEFT])
        {
            g_state.player->move_left();
            //g_state.player->m_animation_indices = g_state.player->m_walking[g_state.player->LEFT];
            g_state.player->set_acceleration(glm::vec3(-1.0f, 0.0f, 0.0f));
            g_fuel--;
        }
        else if (key_state[SDL_SCANCODE_RIGHT])
        {
            g_state.player->move_right();
            g_state.player->set_acceleration(glm::vec3(1.0f, 0.0f, 0.0f));
            g_fuel--;
        }
        else if (key_state[SDL_SCANCODE_UP])
        {
            g_state.player->move_up();
            g_state.player->set_acceleration(glm::vec3(0.0f, 1.0f, 0.0f));
            g_fuel--;
        }
    }
    // This makes sure that the player can't move faster diagonally
    if (glm::length(g_state.player->get_movement()) > 1.0f)
    {
        g_state.player->set_movement(glm::normalize(g_state.player->get_movement()));
    }
}

void update()
{
    // ————— DELTA TIME ————— //
    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND; // get the current number of ticks
    float delta_time = ticks - g_previous_ticks; // the delta time is the difference from the last frame
    g_previous_ticks = ticks;

    // ————— FIXED TIMESTEP ————— //
    // STEP 1: Keep track of how much time has passed since last step
    delta_time += g_time_accumulator;

    // STEP 2: Accumulate the ammount of time passed while we're under our fixed timestep
    if (delta_time < FIXED_TIMESTEP)
    {
        g_time_accumulator = delta_time;
        return;
    }

    // STEP 3: Once we exceed our fixed timestep, apply that elapsed time into the objects' update function invocation
    while (delta_time >= FIXED_TIMESTEP)
    {
        
        // Notice that we're using FIXED_TIMESTEP as our delta time
        g_state.player->update(FIXED_TIMESTEP, g_state.platforms, PLATFORM_COUNT);
        glm::vec3 currentVelocity = g_state.player->get_velocity();
        glm::vec3 currentAcceleration = g_state.player->get_acceleration();
        //if not moving / no input
        if (fabs(currentAcceleration.x) < 0.000000001f)
        {
            currentVelocity.x *= 0.99f;
            g_state.player->set_velocity(currentVelocity);
        }
        
        float monster_x = 3.0f + sin(ticks) * 0.85f;
        g_state.platforms[FINISH].set_position(glm::vec3(monster_x, -2.25f, 0.0f));
        g_state.platforms[FINISH].update(0.0f, NULL, 0);

        for (int i = 0; i < PLATFORM_COUNT; i++)
        {
            if (g_state.platforms[i].touched == true) {
                if (i == FINISH) {
                    win_message = true;
                    glClearColor(0.0f, BG_GREEN, 0.0f, BG_OPACITY);
                }
                else {
                    lose_message = true;
                    glClearColor(0.0f, 0.0f, BG_BLUE, BG_OPACITY);
                }
            }
        }
        delta_time -= FIXED_TIMESTEP;
    }

    g_time_accumulator = delta_time;
}

void render()
{
    // ————— GENERAL ————— //
    glClear(GL_COLOR_BUFFER_BIT);

    // ————— PLAYER ————— //
    g_state.player->render(&g_shader_program);

    // ————— PLATFORM ————— //
    for (int i = 0; i < PLATFORM_COUNT; i++) g_state.platforms[i].render(&g_shader_program);

    if (win_message == true)
    {
        g_state.messages[0].render(&g_shader_program);
    }
    else if (lose_message == true)
    {
        g_state.messages[1].render(&g_shader_program);
    }

    renderFuelUI();
    // ————— GENERAL ————— //
    SDL_GL_SwapWindow(g_display_window);


}

void shutdown() { SDL_Quit(); }

// ————— GAME LOOP ————— /
int main(int argc, char* argv[])
{
    initialise();

    while (g_game_is_running)
    {
        process_input();
        update();
        render();
    }

    shutdown();
    return 0;
}
