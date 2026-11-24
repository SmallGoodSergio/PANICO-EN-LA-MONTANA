#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <string>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>

using namespace std;

// ------------------------------------------------------------
// Utilidades
// ------------------------------------------------------------
SDL_Texture* cargarTextura(SDL_Renderer* ren, const char* archivo) {
    string ruta = string(SDL_GetBasePath()) + "assets/" + archivo;
    SDL_Surface* surf = SDL_LoadPNG(ruta.c_str());

    if (!surf) {
        SDL_Log("No se pudo cargar %s: %s", archivo, SDL_GetError());
        return nullptr;
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_DestroySurface(surf);
    if (tex) SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    return tex;
}

// Sprite de la escopeta dibujado en pixel art
SDL_Texture* crearTexturaEscopeta(SDL_Renderer* ren) {
    SDL_Surface* surf = SDL_CreateSurface(30, 14, SDL_PIXELFORMAT_RGBA32);
    if (!surf) return nullptr;

    SDL_FillSurfaceRect(surf, NULL, SDL_MapSurfaceRGBA(surf, 0, 0, 0, 0));

    auto rect = [&](Uint8 r, Uint8 g, Uint8 b, int x, int y, int w, int h) {
        SDL_Rect rc = { x, y, w, h };
        SDL_FillSurfaceRect(surf, &rc, SDL_MapSurfaceRGBA(surf, r, g, b, 255));
        };

    rect(43, 37, 64, 0, 4, 30, 8);
    rect(139, 90, 43, 1, 5, 9, 6);
    rect(59, 59, 82, 10, 5, 7, 5);
    rect(154, 160, 180, 17, 5, 12, 2);
    rect(201, 207, 224, 17, 5, 12, 1);
    rect(110, 70, 32, 18, 8, 7, 3);
    rect(255, 184, 74, 3, 6, 2, 2);

    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_DestroySurface(surf);
    if (tex) SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    return tex;
}

float aleatorio() {
    return rand() / (float)RAND_MAX;
}

bool choca(const SDL_FRect& a, const SDL_FRect& b) {
    return a.x < b.x + b.w && a.x + a.w > b.x &&
        a.y < b.y + b.h && a.y + a.h > b.y;
}

void textoCentrado(SDL_Renderer* ren, float y, const char* texto) {
    float ancho = (float)strlen(texto) * 8.0f;
    SDL_RenderDebugText(ren, (640.0f - ancho) / 2.0f, y, texto);
}

void dibujarCirculo(SDL_Renderer* ren, float cx, float cy, float r) {
    const int N = 24;

    for (int i = 0; i < N; i++) {
        float a1 = i * 2.0f * 3.14159265f / N;
        float a2 = (i + 1) * 2.0f * 3.14159265f / N;

        SDL_RenderLine(ren,
            cx + cosf(a1) * r,
            cy + sinf(a1) * r,
            cx + cosf(a2) * r,
            cy + sinf(a2) * r);
    }
}

// ------------------------------------------------------------
// Animacion
// ------------------------------------------------------------
struct Animacion {
    int fila;
    int cuadros;
    float duracion;
    bool repetir;
};

bool avanzarAnimacion(const Animacion& anim, float& reloj, int& cuadro, float dt) {
    reloj += dt;

    while (reloj >= anim.duracion) {
        reloj -= anim.duracion;

        if (cuadro + 1 < anim.cuadros)
            cuadro++;
        else if (anim.repetir)
            cuadro = 0;
        else
            return true;
    }

    return false;
}

// ------------------------------------------------------------
// Estructuras del juego
// ------------------------------------------------------------
struct Bala {
    float x, y;
    float vx, vy;
    float vida;
    int dano;
    float tam;
    Uint8 r, g, b;
};

struct Arma {
    const char* nombre;
    SDL_Texture* tex;
    float largo, alto;
    float agarreX, agarreY;
    int costo;
    float intervalo;
    float retroceso;
    int perdigones;
    float dispersion;
    float velBala;
    float vida;
    int dano;
    float tam;
    Uint8 r, g, b;
};

struct Caja {
    float x, y, w, h;
    int hp;
    float flash;
};

struct Particula {
    float x, y;
    float vx, vy;
    float vida;
    float s;
    Uint8 r, g, b;
};

struct Peligro {
    float x, y;
    SDL_FRect hit;
};

const float CAJA = 18.0f;

vector<Caja> crearCajas() {
    vector<Caja> lista;

    auto pila = [&](float x, float yBase, int n) {
        for (int i = 1; i <= n; i++) {
            lista.push_back({
                x, yBase - i * CAJA,
                CAJA, CAJA, 4, 0.0f
                });
        }
        };

    pila(300, 342, 4);
    pila(250, 252, 4);
    pila(540, 144, 8);

    return lista;
}

// ------------------------------------------------------------
// Monedas
// ------------------------------------------------------------
struct Moneda {
    float x, y;
    bool recogida;
};

vector<Moneda> crearMonedas() {
    return {
        { 330, 318, false },
        { 345, 216, false },
        { 320, 10, false }
    };
}

// ------------------------------------------------------------
// Municion
// ------------------------------------------------------------
const float AMMO_ANCHO = 35.0f;
const float AMMO_ALTO = 33.0f;

struct CajaMunicion {
    float x, y;
    bool recogida;
};

vector<CajaMunicion> crearRecargas() {
    return {
        { 430, 342 - AMMO_ALTO, false },
        { 450, 252 - AMMO_ALTO, false },
        { 62, 252 - AMMO_ALTO, false },
        { 490, 144 - AMMO_ALTO, false }
    };
}

// ------------------------------------------------------------
// Romper cajas
// ------------------------------------------------------------
void romperCaja(const Caja& c, vector<Particula>& particulas) {
    for (int i = 0; i < 10; i++) {
        bool clara = aleatorio() < 0.5f;

        particulas.push_back({
            c.x + aleatorio() * c.w,
            c.y + aleatorio() * c.h,
            (aleatorio() - 0.5f) * 140.0f,
            -aleatorio() * 120.0f,
            0.6f + aleatorio() * 0.4f,
            2.0f + floorf(aleatorio() * 3.0f),
            (Uint8)(clara ? 176 : 110),
            (Uint8)(clara ? 122 : 70),
            (Uint8)(clara ? 60 : 32)
            });
    }
}

// ------------------------------------------------------------
// Dibujar cajas
// ------------------------------------------------------------
void dibujarCaja(SDL_Renderer* ren, SDL_Texture* texCaja, const Caja& c) {
    float x = SDL_roundf(c.x);
    float y = SDL_roundf(c.y);

    SDL_FRect destino = { x, y, c.w, c.h };
    SDL_RenderTexture(ren, texCaja, NULL, &destino);

    if (c.flash > 0) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 150);
        SDL_RenderFillRect(ren, &destino);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    }

    SDL_SetRenderDrawColor(ren, 43, 37, 64, 255);

    if (c.hp <= 3)
        SDL_RenderLine(ren, x + 4, y + 1, x + 7, y + 8);

    if (c.hp <= 2)
        SDL_RenderLine(ren, x + 13, y + 17, x + 10, y + 10);

    if (c.hp <= 1) {
        SDL_RenderLine(ren, x + 1, y + 12, x + 8, y + 10);
        SDL_RenderLine(ren, x + 17, y + 6, x + 11, y + 8);
    }
}

// ============================================================
// PROGRAMA PRINCIPAL
// ============================================================
int main(int argc, char* argv[]) {

    srand((unsigned)time(NULL));

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Error SDL: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;

    if (!SDL_CreateWindowAndRenderer(
        "PÁNICO EN LA MONTAÑA",
        1280, 720, 0, &win, &ren)) {

        SDL_Log("Error ventana: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    const float W = 640.0f;
    const float H = 360.0f;

    const float MARGEN_X = 80.0f;
    const float MARGEN_Y = 45.0f;

    const SDL_Rect areaNivel = { 80, 45, 640, 360 };

    SDL_SetRenderLogicalPresentation(
        ren, 800, 450,
        SDL_LOGICAL_PRESENTATION_LETTERBOX
    );

    SDL_SetRenderVSync(ren, 1);
    SDL_SetDefaultTextureScaleMode(ren, SDL_SCALEMODE_NEAREST);

    // --------------------------------------------------------
    // CARGAR TEXTURAS
    // --------------------------------------------------------
    SDL_Texture* texHeroe = cargarTextura(ren, "tilemap.png");
    SDL_Texture* texCielo = cargarTextura(ren, "cielo.png");
    SDL_Texture* texSuelo = cargarTextura(ren, "suelo.png");
    SDL_Texture* texArma = cargarTextura(ren, "tile_0005.png");
    SDL_Texture* texCactus = cargarTextura(ren, "cactus.png");
    SDL_Texture* texPlanta = cargarTextura(ren, "planta.png");
    SDL_Texture* texCaja = cargarTextura(ren, "caja.png");
    SDL_Texture* texMoneda = cargarTextura(ren, "moneda.png");
    SDL_Texture* texMunicion = cargarTextura(ren, "municion.png");
    SDL_Texture* texEscopeta = crearTexturaEscopeta(ren);

    if (!texHeroe || !texCielo || !texSuelo || !texArma ||
        !texCactus || !texPlanta || !texCaja ||
        !texMoneda || !texMunicion || !texEscopeta) {

        SDL_Log("Faltan texturas");

        SDL_DestroyTexture(texHeroe);
        SDL_DestroyTexture(texCielo);
        SDL_DestroyTexture(texSuelo);
        SDL_DestroyTexture(texArma);
        SDL_DestroyTexture(texCactus);
        SDL_DestroyTexture(texPlanta);
        SDL_DestroyTexture(texCaja);
        SDL_DestroyTexture(texMoneda);
        SDL_DestroyTexture(texMunicion);
        SDL_DestroyTexture(texEscopeta);

        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();

        return 1;
    }

    // ========================================================
    // MENU PRINCIPAL
    // ========================================================
    // El personaje camina hasta que el jugador presione ENTER.
    // ========================================================

    bool salirDesdeMenu = false;

    {
        bool mostrarMenu = true;

        float jugadorMenuX = 80.0f;
        float velocidadMenu = 75.0f;

        int cuadroMenu = 0;
        float relojMenu = 0.0f;

        Uint64 tiempoAnteriorMenu = SDL_GetTicksNS();

        SDL_Event eventoMenu;

        while (mostrarMenu) {

            Uint64 ahoraMenu = SDL_GetTicksNS();

            float dtMenu = (ahoraMenu - tiempoAnteriorMenu) /
                (float)SDL_NS_PER_SECOND;

            tiempoAnteriorMenu = ahoraMenu;

            if (dtMenu > 0.05f)
                dtMenu = 0.05f;

            // --------------------------------------------
            // EVENTOS DEL MENU
            // --------------------------------------------
            while (SDL_PollEvent(&eventoMenu)) {

                if (eventoMenu.type == SDL_EVENT_QUIT) {
                    mostrarMenu = false;
                    salirDesdeMenu = true;
                }

                if (eventoMenu.type == SDL_EVENT_KEY_DOWN &&
                    !eventoMenu.key.repeat) {

                    if (eventoMenu.key.key == SDLK_RETURN ||
                        eventoMenu.key.key == SDLK_KP_ENTER) {

                        mostrarMenu = false;
                    }

                    if (eventoMenu.key.key == SDLK_ESCAPE) {
                        mostrarMenu = false;
                        salirDesdeMenu = true;
                    }
                }
            }

            if (!mostrarMenu)
                break;

            // --------------------------------------------
            // MOVIMIENTO DEL PERSONAJE
            // --------------------------------------------
            jugadorMenuX += velocidadMenu * dtMenu;

            if (jugadorMenuX >= 696.0f) {
                jugadorMenuX = 696.0f;
                velocidadMenu = -75.0f;
            }
            else if (jugadorMenuX <= 80.0f) {
                jugadorMenuX = 80.0f;
                velocidadMenu = 75.0f;
            }

            // --------------------------------------------
            // ANIMACION DEL PERSONAJE
            // --------------------------------------------
            relojMenu += dtMenu;

            if (relojMenu >= 0.15f) {
                relojMenu = 0.0f;
                cuadroMenu = (cuadroMenu + 1) % 2;
            }

            // --------------------------------------------
            // DIBUJAR MENU
            // --------------------------------------------
            SDL_SetRenderViewport(ren, NULL);

            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);

            // Fondo
            SDL_FRect fondoMenu = { 0, 0, 800, 450 };

            SDL_RenderTexture(
                ren, texCielo, NULL, &fondoMenu
            );

            // --------------------------------------------
            // SUELO
            // --------------------------------------------
            for (float x = 0; x < 800; x += 18.0f) {

                SDL_FRect destinoSueloMenu = {
                    x, 420, 18, 30
                };

                SDL_FRect origenSueloMenu = {
                    0, 0, 18, 18
                };

                SDL_RenderTexture(
                    ren,
                    texSuelo,
                    &origenSueloMenu,
                    &destinoSueloMenu
                );
            }

            // --------------------------------------------
            // SOMBRA DEL PERSONAJE
            // --------------------------------------------
            SDL_SetRenderDrawColor(
                ren, 30, 25, 35, 255
            );

            SDL_FRect sombraMenu = {
                jugadorMenuX + 5.0f,
                417.0f,
                30.0f,
                4.0f
            };

            SDL_RenderFillRect(ren, &sombraMenu);

            // --------------------------------------------
            // PERSONAJE CAMINANDO
            // --------------------------------------------
            SDL_FRect origenHeroeMenu = {
                cuadroMenu * 25.0f,
                0,
                24,
                24
            };

            SDL_FRect destinoHeroeMenu = {
                jugadorMenuX,
                390,
                36,
                36
            };

            SDL_FlipMode giroMenu =
                velocidadMenu < 0
                ? SDL_FLIP_HORIZONTAL
                : SDL_FLIP_NONE;

            SDL_RenderTextureRotated(
                ren,
                texHeroe,
                &origenHeroeMenu,
                &destinoHeroeMenu,
                0.0,
                NULL,
                giroMenu
            );

            // --------------------------------------------
            // PANEL DEL TITULO
            // --------------------------------------------
            SDL_SetRenderDrawBlendMode(
                ren, SDL_BLENDMODE_BLEND
            );

            SDL_SetRenderDrawColor(
                ren, 0, 0, 0, 155
            );

            SDL_FRect panelTitulo = {
                100, 105, 600, 105
            };

            SDL_RenderFillRect(ren, &panelTitulo);

            SDL_SetRenderDrawBlendMode(
                ren, SDL_BLENDMODE_NONE
            );

            // --------------------------------------------
            // TITULO DEL JUEGO
            // --------------------------------------------
            SDL_SetRenderDrawColor(
                ren, 255, 220, 70, 255
            );

            textoCentrado(
                ren,
                145,
                "PÁNICO EN LA MONTAÑA"
            );

            // --------------------------------------------
            // INDICACION PARA COMENZAR
            // --------------------------------------------
            SDL_SetRenderDrawColor(
                ren, 255, 255, 255, 255
            );

            textoCentrado(
                ren,
                180,
                "PRESIONA ENTER PARA COMENZAR"
            );

            // --------------------------------------------
            // INDICACION PARA SALIR
            // --------------------------------------------
            SDL_SetRenderDrawColor(
                ren, 220, 220, 220, 255
            );

            textoCentrado(
                ren,
                230,
                "ESC: SALIR"
            );

            SDL_RenderPresent(ren);
        }
    }

    // ========================================================
    // SALIR DESDE EL MENU
    // ========================================================
    if (salirDesdeMenu) {

        SDL_DestroyTexture(texHeroe);
        SDL_DestroyTexture(texCielo);
        SDL_DestroyTexture(texSuelo);
        SDL_DestroyTexture(texArma);
        SDL_DestroyTexture(texCactus);
        SDL_DestroyTexture(texPlanta);
        SDL_DestroyTexture(texCaja);
        SDL_DestroyTexture(texMoneda);
        SDL_DestroyTexture(texMunicion);
        SDL_DestroyTexture(texEscopeta);

        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);

        SDL_Quit();

        return 0;
    }

    // ========================================================
    // NIVEL 1: TRES PASILLOS EN ZIGZAG
    // ========================================================

    const vector<SDL_FRect> SOLIDOS = {
        {   0, 342, 640, 18 },
        {   0, 252, 330, 18 },
        { 372, 252, 168, 18 },
        { 100, 144, 330, 18 },
        { 472, 144, 168, 18 },
        { 260,   0,  18, 54 }
    };

    const float POS_CACTUS[][2] = {
        { 130, 324 }, { 200, 324 }, { 218, 324 }, { 400, 324 },
        { 140, 234 }, { 158, 234 }, { 500, 234 },
        { 180, 126 }, { 350, 126 }, { 368, 126 }
    };

    const float POS_PLANTAS[][2] = {
        { 60, 324 }, { 480, 324 }, { 40, 234 },
        { 410, 234 }, { 130, 126 }, { 400, 126 }
    };

    vector<Peligro> peligros;

    for (const auto& p : POS_CACTUS) {
        peligros.push_back({
            p[0], p[1],
            { p[0] + 3, p[1] + 3, 12, 15 }
            });
    }

    const SDL_FRect META = { 596, 120, 24, 24 };

    const float INICIO_X = 16.0f;
    const float INICIO_Y = 318.0f;

    SDL_FRect jugador = {
        INICIO_X, INICIO_Y, 24, 24
    };

    const float VEL_CAMINAR = 90.0f;
    const float VEL_CORRER = 150.0f;

    // Fisica
    const float GRAVEDAD = 450.0f;
    const float FRICCION = 0.90f;

    // Municion
    const float MUNICION_INICIAL = 30.0f;
    const float MUNICION_MAX = 40.0f;
    const float MUNICION_CAJA = 12.0f;

    float velocidadX = 0.0f;
    float velocidadY = 0.0f;

    bool enSuelo = true;

    // ========================================================
    // ARMAS
    // ========================================================

    Arma ARMAS[2] = {
        {
            "PISTOLA", texArma,
            20, 12, 5, 6,
            1, 0.18f, 110.0f,
            1, 0.0f, 250.0f,
            3.0f, 1, 4,
            255, 220, 70
        },

        {
            "ESCOPETA", texEscopeta,
            30, 14, 9, 8,
            3, 0.65f, 300.0f,
            6, 0.30f, 240.0f,
            0.35f, 1, 3,
            255, 150, 60
        }
    };

    int armaActual = 0;

    // ========================================================
    // SPRITES
    // ========================================================

    const int FILA_CONEJO = 0;

    const float SPRITE_ANCHO = 24.0f;
    const float SPRITE_ALTO = 24.0f;
    const float PASO_SPRITE = 25.0f;

    enum {
        QUIETO,
        CAMINAR,
        CORRER
    };

    const Animacion ANIM[3] = {
        { FILA_CONEJO, 1, 0.30f, true },
        { FILA_CONEJO, 2, 0.15f, true },
        { FILA_CONEJO, 2, 0.08f, true }
    };

    int actual = QUIETO;
    int cuadro = 0;

    float reloj = 0.0f;

    bool corriendo = true;
    bool pausa = false;
    bool mirandoIzq = false;
    bool ganado = false;

    // ========================================================
    // VECTORES Y VARIABLES
    // ========================================================

    vector<Bala> balas;
    vector<Caja> cajas = crearCajas();
    vector<Particula> particulas;

    vector<Moneda> monedas = crearMonedas();

    int monedasRecogidas = 0;

    vector<CajaMunicion> recargas = crearRecargas();

    float relojDisparo = 1.0f;
    float municion = MUNICION_INICIAL;

    float tiempo = 0.0f;
    float tGanado = 0.0f;
    float t = 0.0f;

    int muertes = 0;

    // ========================================================
    // REAPARECER
    // ========================================================

    auto reaparecer = [&]() {

        jugador.x = INICIO_X;
        jugador.y = INICIO_Y;

        velocidadX = 0.0f;
        velocidadY = 0.0f;

        balas.clear();

        municion = max(
            municion,
            MUNICION_INICIAL
        );
        };

    // ========================================================
    // REINICIAR
    // ========================================================

    auto reiniciar = [&]() {

        reaparecer();

        tiempo = 0.0f;
        muertes = 0;

        ganado = false;
        tGanado = 0.0f;

        cajas = crearCajas();

        particulas.clear();

        monedas = crearMonedas();
        monedasRecogidas = 0;

        recargas = crearRecargas();

        municion = MUNICION_INICIAL;
        };

    // ========================================================
    // VARIABLES DE ENTRADA
    // ========================================================

    const bool* teclas = SDL_GetKeyboardState(NULL);

    Uint64 anterior = SDL_GetTicksNS();

    SDL_Event ev;

    float mouseX = 0.0f;
    float mouseY = 0.0f;

    float anguloArma = 0.0f;

    // ========================================================
    // BUCLE PRINCIPAL DEL JUEGO
    // ========================================================

    while (corriendo) {

        Uint64 ahora = SDL_GetTicksNS();

        float dt = (ahora - anterior) /
            (float)SDL_NS_PER_SECOND;

        anterior = ahora;

        if (dt > 0.05f)
            dt = 0.05f;

        t += dt;

        // ----------------------------------------------------
        // EVENTOS
        // ----------------------------------------------------

        while (SDL_PollEvent(&ev)) {

            if (ev.type == SDL_EVENT_QUIT)
                corriendo = false;

            if (ev.type == SDL_EVENT_KEY_DOWN &&
                !ev.key.repeat) {

                if (ev.key.key == SDLK_ESCAPE)
                    corriendo = false;

                if (ev.key.key == SDLK_P && !ganado)
                    pausa = !pausa;

                if (ev.key.key == SDLK_R) {
                    pausa = false;
                    reiniciar();
                }

                if (ev.key.key == SDLK_1)
                    armaActual = 0;

                if (ev.key.key == SDLK_2)
                    armaActual = 1;

                if (ev.key.key == SDLK_Q)
                    armaActual = (armaActual + 1) % 2;
            }

            // Rueda del mouse
            if (ev.type == SDL_EVENT_MOUSE_WHEEL) {
                if (ev.wheel.y != 0)
                    armaActual = (armaActual + 1) % 2;
            }

            // Clic en pantalla de victoria
            if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                ganado && tGanado > 0.6f) {

                pausa = false;
                reiniciar();
            }
        }

        // ----------------------------------------------------
        // MOVIMIENTO
        // ----------------------------------------------------

        float dir = 0.0f;

        if (teclas[SDL_SCANCODE_LEFT] ||
            teclas[SDL_SCANCODE_A])
            dir -= 1.0f;

        if (teclas[SDL_SCANCODE_RIGHT] ||
            teclas[SDL_SCANCODE_D])
            dir += 1.0f;

        bool rapido =
            teclas[SDL_SCANCODE_LSHIFT] ||
            teclas[SDL_SCANCODE_RSHIFT];

        // ----------------------------------------------------
        // MOUSE
        // ----------------------------------------------------

        SDL_SetRenderViewport(ren, NULL);

        Uint32 botonesMouse = SDL_GetMouseState(
            &mouseX, &mouseY
        );

        SDL_RenderCoordinatesFromWindow(
            ren, mouseX, mouseY,
            &mouseX, &mouseY
        );

        mouseX -= MARGEN_X;
        mouseY -= MARGEN_Y;

        if (ganado)
            tGanado += dt;

        // Posicion de la mano
        float manoX = jugador.x + jugador.w / 2.0f;
        float manoY = jugador.y + jugador.h * 0.60f;

        float dx = mouseX - manoX;
        float dy = mouseY - manoY;

        float distancia = sqrtf(dx * dx + dy * dy);

        if (distancia > 0.001f) {
            anguloArma = atan2f(dy, dx) *
                180.0f / 3.14159265f;

            mirandoIzq = dx < 0;
        }

        float radianes =
            anguloArma * 3.14159265f / 180.0f;

        float dirX = cosf(radianes);
        float dirY = sinf(radianes);

        // ====================================================
        // ACTUALIZACION DEL JUEGO
        // ====================================================

        if (!pausa && !ganado) {

            tiempo += dt;

            float velocidad =
                rapido ? VEL_CORRER : VEL_CAMINAR;

            // ------------------------------------------------
            // ANIMACION
            // ------------------------------------------------

            int nueva;

            if (dir == 0)
                nueva = QUIETO;
            else if (rapido)
                nueva = CORRER;
            else
                nueva = CAMINAR;

            if (nueva != actual) {
                cuadro = 0;
                reloj = 0.0f;
            }

            actual = nueva;

            if (actual == QUIETO || !enSuelo) {
                cuadro = 0;
                reloj = 0.0f;
            }
            else {
                avanzarAnimacion(
                    ANIM[actual],
                    reloj,
                    cuadro,
                    dt
                );
            }

            // ------------------------------------------------
            // DISPAROS
            // ------------------------------------------------

            relojDisparo += dt;

            const Arma& A = ARMAS[armaActual];

            if ((botonesMouse & SDL_BUTTON_LMASK) &&
                relojDisparo >= A.intervalo &&
                distancia > 0.001f &&
                municion >= (float)A.costo) {

                float punta = A.largo - A.agarreX;

                bool varios = A.perdigones > 1;

                for (int i = 0; i < A.perdigones; i++) {

                    float ang = radianes +
                        (varios
                            ? (aleatorio() - 0.5f) *
                            2.0f * A.dispersion
                            : 0.0f);

                    float vel = A.velBala *
                        (varios
                            ? 0.85f + aleatorio() * 0.3f
                            : 1.0f);

                    Bala nuevaBala = {
                        manoX + dirX * punta,
                        manoY + dirY * punta,
                        cosf(ang) * vel,
                        sinf(ang) * vel,
                        A.vida,
                        A.dano,
                        A.tam,
                        A.r,
                        A.g,
                        A.b
                    };

                    balas.push_back(nuevaBala);
                }

                relojDisparo = 0.0f;

                municion -= (float)A.costo;

                // Retroceso
                velocidadX -= dirX * A.retroceso;
                velocidadY -= dirY * A.retroceso;

                enSuelo = false;
            }

            // ------------------------------------------------
            // MOVIMIENTO HORIZONTAL Y COLISIONES
            // ------------------------------------------------

            jugador.x +=
                (dir * velocidad + velocidadX) * dt;

            velocidadX *= powf(FRICCION, dt * 60.0f);

            {
                vector<SDL_FRect> obstaculos = SOLIDOS;

                for (const Caja& c : cajas)
                    obstaculos.push_back({
                        c.x, c.y, c.w, c.h
                        });

                for (const SDL_FRect& s : obstaculos) {

                    if (choca(jugador, s)) {

                        float centroJ =
                            jugador.x + jugador.w / 2.0f;

                        float centroS =
                            s.x + s.w / 2.0f;

                        jugador.x =
                            (centroJ < centroS)
                            ? s.x - jugador.w
                            : s.x + s.w;

                        velocidadX = 0.0f;
                    }
                }
            }

            // Limites horizontales
            if (jugador.x < 0.0f) {
                jugador.x = 0.0f;
                velocidadX = 0.0f;
            }

            if (jugador.x > W - jugador.w) {
                jugador.x = W - jugador.w;
                velocidadX = 0.0f;
            }

            // ------------------------------------------------
            // MOVIMIENTO VERTICAL
            // ------------------------------------------------

            jugador.y += velocidadY * dt;

            velocidadY += GRAVEDAD * dt;

            enSuelo = false;

            {
                vector<SDL_FRect> obstaculos = SOLIDOS;

                for (const Caja& c : cajas)
                    obstaculos.push_back({
                        c.x, c.y, c.w, c.h
                        });

                for (const SDL_FRect& s : obstaculos) {

                    if (choca(jugador, s)) {

                        if (velocidadY > 0.0f) {
                            jugador.y = s.y - jugador.h;
                            enSuelo = true;
                        }
                        else {
                            jugador.y = s.y + s.h;
                        }

                        velocidadY = 0.0f;
                    }
                }
            }

            // Limite superior
            if (jugador.y < 0.0f) {

                jugador.y = 0.0f;

                if (velocidadY < 0.0f)
                    velocidadY = 0.0f;
            }

            // ------------------------------------------------
            // CAJAS DE MUNICION
            // ------------------------------------------------

            for (CajaMunicion& r : recargas) {

                SDL_FRect hit = {
                    r.x + 4,
                    r.y + 4,
                    AMMO_ANCHO - 8,
                    AMMO_ALTO - 8
                };

                if (!r.recogida &&
                    municion < MUNICION_MAX &&
                    choca(jugador, hit)) {

                    r.recogida = true;

                    municion = min(
                        MUNICION_MAX,
                        municion + MUNICION_CAJA
                    );
                }
            }

            // ------------------------------------------------
            // BALAS
            // ------------------------------------------------

            for (int i = (int)balas.size() - 1; i >= 0; i--) {

                Bala& b = balas[i];

                b.x += b.vx * dt;
                b.y += b.vy * dt;

                b.vida -= dt;

                bool eliminar =
                    b.vida <= 0.0f ||
                    b.x < 0 ||
                    b.x > W ||
                    b.y < 0 ||
                    b.y > H;

                if (!eliminar) {

                    for (Caja& c : cajas) {

                        if (b.x >= c.x &&
                            b.x <= c.x + c.w &&
                            b.y >= c.y &&
                            b.y <= c.y + c.h) {

                            c.hp -= b.dano;
                            c.flash = 0.08f;

                            eliminar = true;
                            break;
                        }
                    }
                }

                if (!eliminar) {

                    for (const SDL_FRect& s : SOLIDOS) {

                        if (b.x >= s.x &&
                            b.x <= s.x + s.w &&
                            b.y >= s.y &&
                            b.y <= s.y + s.h) {

                            eliminar = true;
                            break;
                        }
                    }
                }

                if (eliminar)
                    balas.erase(balas.begin() + i);
            }

            // ------------------------------------------------
            // CAJAS Y PARTICULAS
            // ------------------------------------------------

            for (Caja& c : cajas) {
                if (c.flash > 0.0f)
                    c.flash -= dt;
            }

            for (int i = (int)cajas.size() - 1; i >= 0; i--) {

                if (cajas[i].hp <= 0) {

                    romperCaja(
                        cajas[i],
                        particulas
                    );

                    cajas.erase(cajas.begin() + i);
                }
            }

            for (int i = (int)particulas.size() - 1; i >= 0; i--) {

                Particula& p = particulas[i];

                p.vy += GRAVEDAD * dt;

                p.x += p.vx * dt;
                p.y += p.vy * dt;

                p.vida -= dt;

                if (p.vida <= 0.0f)
                    particulas.erase(particulas.begin() + i);
            }

            // ------------------------------------------------
            // CAIDA FUERA DE LA PANTALLA
            // ------------------------------------------------

            if (jugador.y > H + 8.0f) {

                muertes++;

                reaparecer();
            }

            // ------------------------------------------------
            // CACTUS
            // ------------------------------------------------

            SDL_FRect cuerpo = {
                jugador.x + 4,
                jugador.y + 4,
                jugador.w - 8,
                jugador.h - 4
            };

            for (const Peligro& p : peligros) {

                if (choca(cuerpo, p.hit)) {

                    muertes++;

                    reaparecer();

                    break;
                }
            }

            // ------------------------------------------------
            // MONEDAS
            // ------------------------------------------------

            for (Moneda& m : monedas) {

                SDL_FRect hit = {
                    m.x + 3,
                    m.y + 3,
                    12,
                    12
                };

                if (!m.recogida &&
                    choca(jugador, hit)) {

                    m.recogida = true;

                    monedasRecogidas++;
                }
            }

            // ------------------------------------------------
            // META
            // ------------------------------------------------

            if (choca(jugador, META)) {

                ganado = true;

                tGanado = 0.0f;
            }
        }

        // ====================================================
        // POSICION DEL ARMA
        // ====================================================

        manoX = jugador.x + jugador.w / 2.0f;
        manoY = jugador.y + jugador.h * 0.60f;

        dx = mouseX - manoX;
        dy = mouseY - manoY;

        distancia = sqrtf(dx * dx + dy * dy);

        if (distancia > 0.001f) {

            anguloArma = atan2f(dy, dx) *
                180.0f / 3.14159265f;

            mirandoIzq = dx < 0;
        }

        // ====================================================
        // DIBUJO DEL JUEGO
        // ====================================================

        SDL_SetRenderViewport(ren, NULL);

        SDL_SetRenderDrawColor(
            ren, 0, 0, 0, 255
        );

        SDL_RenderClear(ren);

        SDL_SetRenderViewport(
            ren, &areaNivel
        );

        // Fondo
        SDL_RenderTexture(
            ren, texCielo, NULL, NULL
        );

        // ----------------------------------------------------
        // PLANTAS
        // ----------------------------------------------------

        for (const auto& p : POS_PLANTAS) {

            SDL_FRect destino = {
                p[0], p[1], 18, 18
            };

            SDL_RenderTexture(
                ren,
                texPlanta,
                NULL,
                &destino
            );
        }

        // ----------------------------------------------------
        // PLATAFORMAS
        // ----------------------------------------------------

        for (const SDL_FRect& s : SOLIDOS) {

            for (float x = s.x; x < s.x + s.w; x += 18.0f) {

                for (float y = s.y; y < s.y + s.h; y += 18.0f) {

                    float w = min(
                        18.0f,
                        s.x + s.w - x
                    );

                    float h = min(
                        18.0f,
                        s.y + s.h - y
                    );

                    SDL_FRect src = {
                        0, 0, w, h
                    };

                    SDL_FRect dst = {
                        x, y, w, h
                    };

                    SDL_RenderTexture(
                        ren,
                        texSuelo,
                        &src,
                        &dst
                    );
                }
            }
        }

        // ----------------------------------------------------
        // CACTUS
        // ----------------------------------------------------

        for (const Peligro& p : peligros) {

            SDL_FRect destino = {
                p.x, p.y, 18, 18
            };

            SDL_RenderTexture(
                ren,
                texCactus,
                NULL,
                &destino
            );
        }

        // ----------------------------------------------------
        // CAJAS Y PARTICULAS
        // ----------------------------------------------------

        for (const Caja& c : cajas)
            dibujarCaja(ren, texCaja, c);

        for (const Particula& p : particulas) {

            SDL_SetRenderDrawColor(
                ren, p.r, p.g, p.b, 255
            );

            SDL_FRect trozo = {
                SDL_roundf(p.x),
                SDL_roundf(p.y),
                p.s,
                p.s
            };

            SDL_RenderFillRect(
                ren, &trozo
            );
        }

        // ----------------------------------------------------
        // MONEDAS
        // ----------------------------------------------------

        for (const Moneda& m : monedas) {

            if (m.recogida)
                continue;

            SDL_FRect destino = {
                m.x,
                SDL_roundf(
                    m.y + sinf(t * 4.0f + m.x) * 1.5f
                ),
                18,
                18
            };

            SDL_RenderTexture(
                ren,
                texMoneda,
                NULL,
                &destino
            );
        }

        // ----------------------------------------------------
        // CAJAS DE MUNICION
        // ----------------------------------------------------

        for (const CajaMunicion& r : recargas) {

            if (r.recogida)
                continue;

            SDL_FRect destino = {
                r.x,
                r.y,
                AMMO_ANCHO,
                AMMO_ALTO
            };

            SDL_RenderTexture(
                ren,
                texMunicion,
                NULL,
                &destino
            );
        }

        // ----------------------------------------------------
        // META
        // ----------------------------------------------------

        float flota = sinf(t * 3.0f) * 2.0f;

        SDL_SetRenderDrawBlendMode(
            ren, SDL_BLENDMODE_BLEND
        );

        SDL_SetRenderDrawColor(
            ren,
            255,
            220,
            70,
            (Uint8)(
                (0.5f + 0.4f * sinf(t * 4.0f)) * 255.0f
                )
        );

        dibujarCirculo(
            ren,
            META.x + 12,
            META.y + 12 + flota,
            15.0f
        );

        SDL_SetRenderDrawBlendMode(
            ren, SDL_BLENDMODE_NONE
        );

        SDL_FRect srcMeta = {
            0,
            2 * PASO_SPRITE,
            24,
            24
        };

        SDL_FRect dstMeta = {
            META.x,
            SDL_roundf(META.y + flota),
            24,
            24
        };

        SDL_RenderTexture(
            ren,
            texHeroe,
            &srcMeta,
            &dstMeta
        );

        SDL_SetRenderDrawColor(
            ren, 255, 220, 70, 255
        );

        SDL_RenderDebugText(
            ren,
            META.x + 12 - 16,
            META.y - 12 + flota,
            "META"
        );

        // ----------------------------------------------------
        // HEROE
        // ----------------------------------------------------

        SDL_FRect srcHeroe = {
            cuadro * PASO_SPRITE,
            FILA_CONEJO * PASO_SPRITE,
            SPRITE_ANCHO,
            SPRITE_ALTO
        };

        SDL_FRect destinoHeroe = {
            SDL_roundf(jugador.x),
            SDL_roundf(jugador.y),
            jugador.w,
            jugador.h
        };

        SDL_FlipMode flipHeroe =
            mirandoIzq
            ? SDL_FLIP_HORIZONTAL
            : SDL_FLIP_NONE;

        SDL_RenderTextureRotated(
            ren,
            texHeroe,
            &srcHeroe,
            &destinoHeroe,
            0.0,
            NULL,
            flipHeroe
        );

        // ----------------------------------------------------
        // ARMA
        // ----------------------------------------------------

        const Arma& AD = ARMAS[armaActual];

        SDL_FRect arma = {
            SDL_roundf(manoX - AD.agarreX),
            SDL_roundf(manoY - AD.agarreY),
            AD.largo,
            AD.alto
        };

        SDL_FPoint centroRotacion = {
            AD.agarreX,
            AD.agarreY
        };

        SDL_FlipMode flipArma =
            mirandoIzq
            ? SDL_FLIP_VERTICAL
            : SDL_FLIP_NONE;

        SDL_RenderTextureRotated(
            ren,
            AD.tex,
            NULL,
            &arma,
            anguloArma,
            &centroRotacion,
            flipArma
        );

        // ----------------------------------------------------
        // PROYECTILES
        // ----------------------------------------------------

        for (const Bala& bala : balas) {

            SDL_SetRenderDrawColor(
                ren,
                bala.r,
                bala.g,
                bala.b,
                255
            );

            SDL_FRect proyectil = {
                SDL_roundf(bala.x - bala.tam / 2.0f),
                SDL_roundf(bala.y - bala.tam / 2.0f),
                bala.tam,
                bala.tam
            };

            SDL_RenderFillRect(
                ren, &proyectil
            );
        }

        // ====================================================
        // HUD
        // ====================================================

        SDL_SetRenderDrawColor(
            ren, 255, 255, 255, 255
        );

        SDL_RenderDebugTextFormat(
            ren,
            4,
            4,
            "MUNICION %d/%d",
            (int)municion,
            (int)MUNICION_MAX
        );

        SDL_FRect marco = {
            4, 14, 62, 7
        };

        SDL_RenderRect(
            ren, &marco
        );

        if (municion >= (float)ARMAS[armaActual].costo)
            SDL_SetRenderDrawColor(
                ren, 255, 220, 70, 255
            );
        else
            SDL_SetRenderDrawColor(
                ren, 220, 80, 80, 255
            );

        SDL_FRect relleno = {
            5,
            15,
            SDL_roundf(60.0f * municion / MUNICION_MAX),
            5
        };

        SDL_RenderFillRect(
            ren, &relleno
        );

        SDL_SetRenderDrawColor(
            ren, 255, 255, 255, 255
        );

        SDL_RenderDebugTextFormat(
            ren,
            4,
            24,
            "TIEMPO %.1fs  MUERTES %d",
            tiempo,
            muertes
        );

        SDL_RenderDebugTextFormat(
            ren,
            4,
            34,
            "ARMA [1/2/Q]: %s (cuesta %d)",
            ARMAS[armaActual].nombre,
            ARMAS[armaActual].costo
        );

        SDL_RenderDebugTextFormat(
            ren,
            4,
            44,
            "MONEDAS %d/%d",
            monedasRecogidas,
            (int)monedas.size()
        );

        // ----------------------------------------------------
        // PAUSA
        // ----------------------------------------------------

        if (pausa)
            textoCentrado(
                ren,
                H / 2.0f,
                "PAUSA"
            );

        // ====================================================
        // PANTALLA DE VICTORIA
        // ====================================================

        if (ganado) {

            SDL_SetRenderDrawBlendMode(
                ren, SDL_BLENDMODE_BLEND
            );

            SDL_SetRenderDrawColor(
                ren, 20, 20, 35, 178
            );

            SDL_FRect velo = {
                0, 0, W, H
            };

            SDL_RenderFillRect(
                ren, &velo
            );

            SDL_SetRenderDrawBlendMode(
                ren, SDL_BLENDMODE_NONE
            );

            char linea[96];

            SDL_SetRenderDrawColor(
                ren, 255, 220, 70, 255
            );

            textoCentrado(
                ren,
                H / 2.0f - 20.0f,
                "NIVEL COMPLETADO!"
            );

            SDL_SetRenderDrawColor(
                ren, 255, 255, 255, 255
            );

            SDL_snprintf(
                linea,
                sizeof(linea),
                "Tiempo: %.1fs   Muertes: %d",
                tiempo,
                muertes
            );

            textoCentrado(
                ren,
                H / 2.0f - 4.0f,
                linea
            );

            SDL_snprintf(
                linea,
                sizeof(linea),
                "Monedas recogidas: %d de %d",
                monedasRecogidas,
                (int)monedas.size()
            );

            if (monedasRecogidas == (int)monedas.size())
                SDL_SetRenderDrawColor(
                    ren, 255, 220, 70, 255
                );

            textoCentrado(
                ren,
                H / 2.0f + 14.0f,
                linea
            );

            SDL_SetRenderDrawColor(
                ren, 255, 255, 255, 255
            );

            textoCentrado(
                ren,
                H / 2.0f + 32.0f,
                "Clic o R para jugar de nuevo"
            );
        }

        SDL_RenderPresent(ren);
    }

    // ========================================================
    // LIBERAR MEMORIA
    // ========================================================

    SDL_DestroyTexture(texHeroe);
    SDL_DestroyTexture(texCielo);
    SDL_DestroyTexture(texSuelo);
    SDL_DestroyTexture(texArma);
    SDL_DestroyTexture(texCactus);
    SDL_DestroyTexture(texPlanta);
    SDL_DestroyTexture(texCaja);
    SDL_DestroyTexture(texMoneda);
    SDL_DestroyTexture(texMunicion);
    SDL_DestroyTexture(texEscopeta);

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);

    SDL_Quit();

    return 0;
}