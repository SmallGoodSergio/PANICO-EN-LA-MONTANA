
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <string>
#include <iostream>
#include <vector>
#include <cmath>

using namespace std;

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
        if (cuadro + 1 < anim.cuadros) cuadro++;
        else if (anim.repetir) cuadro = 0;
        else return true;
    }
    return false;
}

struct Bala {
    float x, y;
    float vx, vy;
};

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Error SDL: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;

    if (!SDL_CreateWindowAndRenderer("Mi juego", 1280, 720, 0, &win, &ren)) {
        SDL_Log("Error ventana: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderLogicalPresentation(ren, 320, 180, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(ren, 1);
    SDL_SetDefaultTextureScaleMode(ren, SDL_SCALEMODE_NEAREST);

    SDL_Texture* texHeroe = cargarTextura(ren, "tilemap.png");
    SDL_Texture* texCielo = cargarTextura(ren, "cielo.png");
    SDL_Texture* texSuelo = cargarTextura(ren, "suelo.png");
    SDL_Texture* texArma = cargarTextura(ren, "tile_0005.png");

    if (!texHeroe || !texCielo || !texSuelo || !texArma) {
        SDL_Log("Faltan texturas");
        SDL_DestroyTexture(texHeroe);
        SDL_DestroyTexture(texCielo);
        SDL_DestroyTexture(texSuelo);
        SDL_DestroyTexture(texArma);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    SDL_FRect suelo = { 0, 140, 320, 40 };
    SDL_FRect jugador = { 40, 116, 24, 24 };

    const float VEL_CAMINAR = 90.0f;
    const float VEL_CORRER = 150.0f;
    const float VEL_BALA = 250.0f;
    const float INTERVALO_DISPARO = 0.18f;

    // Fisica del retroceso
    const float FUERZA_RETROCESO = 110.0f;
    const float GRAVEDAD = 450.0f;
    const float FRICCION = 0.90f;

    float velocidadX = 0.0f;
    float velocidadY = 0.0f;
    bool enSuelo = true;

    const int FILA_CONEJO = 0;
    const float SPRITE_ANCHO = 24.0f;
    const float SPRITE_ALTO = 24.0f;

    const float ARMA_ANCHO = 20.0f;
    const float ARMA_ALTO = 12.0f;
    const float AGARRE_X = 5.0f;
    const float AGARRE_Y = 6.0f;

    enum { QUIETO, CAMINAR, CORRER };

    const Animacion ANIM[3] = {
        { FILA_CONEJO, 1, 0.30f, true },
        { FILA_CONEJO, 2, 0.15f, true },
        { FILA_CONEJO, 2, 0.08f, true }
    };

    const char* NOMBRES[3] = { "QUIETO", "CAMINAR", "CORRER" };

    int actual = QUIETO;
    int cuadro = 0;
    float reloj = 0.0f;

    bool corriendo = true;
    bool pausa = false;
    bool mirandoIzq = false;

    vector<Bala> balas;
    float relojDisparo = INTERVALO_DISPARO;

    const bool* teclas = SDL_GetKeyboardState(NULL);
    Uint64 anterior = SDL_GetTicksNS();
    SDL_Event ev;

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    float anguloArma = 0.0f;

    while (corriendo) {
        Uint64 ahora = SDL_GetTicksNS();
        float dt = (ahora - anterior) / (float)SDL_NS_PER_SECOND;
        anterior = ahora;
        if (dt > 0.05f) dt = 0.05f;

        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) corriendo = false;

            if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat) {
                if (ev.key.key == SDLK_ESCAPE) corriendo = false;
                if (ev.key.key == SDLK_P) pausa = !pausa;
            }
        }

        float dir = 0.0f;
        if (teclas[SDL_SCANCODE_LEFT] || teclas[SDL_SCANCODE_A]) dir -= 1.0f;
        if (teclas[SDL_SCANCODE_RIGHT] || teclas[SDL_SCANCODE_D]) dir += 1.0f;

        bool rapido = teclas[SDL_SCANCODE_LSHIFT] || teclas[SDL_SCANCODE_RSHIFT];

        Uint32 botonesMouse = SDL_GetMouseState(&mouseX, &mouseY);
        SDL_RenderCoordinatesFromWindow(ren, mouseX, mouseY, &mouseX, &mouseY);

        // Posicion de la mano
        float manoX = jugador.x + jugador.w / 2.0f;
        float manoY = jugador.y + jugador.h * 0.60f;

        float dx = mouseX - manoX;
        float dy = mouseY - manoY;
        float distancia = sqrtf(dx * dx + dy * dy);

        if (distancia > 0.001f) {
            anguloArma = atan2f(dy, dx) * 180.0f / 3.14159265f;
            mirandoIzq = dx < 0;
        }

        float radianes = anguloArma * 3.14159265f / 180.0f;
        float dirX = cosf(radianes);
        float dirY = sinf(radianes);

        float puntaX = manoX + dirX * (ARMA_ANCHO - AGARRE_X);
        float puntaY = manoY + dirY * (ARMA_ANCHO - AGARRE_X);

        if (!pausa) {
            // Movimiento horizontal con teclado
            float velocidad = rapido ? VEL_CORRER : VEL_CAMINAR;

            // Animacion del conejo
            int nueva;
            if (dir == 0) nueva = QUIETO;
            else if (rapido) nueva = CORRER;
            else nueva = CAMINAR;

            if (nueva != actual) {
                cuadro = 0;
                reloj = 0.0f;
            }

            actual = nueva;

            if (actual == QUIETO) {
                cuadro = 0;
                reloj = 0.0f;
            }
            else {
                avanzarAnimacion(ANIM[actual], reloj, cuadro, dt);
            }

            // Disparar y aplicar retroceso
            relojDisparo += dt;

            if ((botonesMouse & SDL_BUTTON_LMASK) &&
                relojDisparo >= INTERVALO_DISPARO &&
                distancia > 0.001f) {

                Bala nuevaBala = {
                    puntaX,
                    puntaY,
                    dirX * VEL_BALA,
                    dirY * VEL_BALA
                };

                balas.push_back(nuevaBala);
                relojDisparo = 0.0f;

                // Impulso contrario al disparo
                velocidadX -= dirX * FUERZA_RETROCESO;
                velocidadY -= dirY * FUERZA_RETROCESO;
                enSuelo = false;
            }

            // Aplicar movimiento y fisica
            jugador.x += (dir * velocidad + velocidadX) * dt;
            jugador.y += velocidadY * dt;

            velocidadY += GRAVEDAD * dt;
            velocidadX *= powf(FRICCION, dt * 60.0f);

            // Colision con el suelo
            float limiteSuelo = suelo.y - jugador.h;

            if (jugador.y >= limiteSuelo) {
                jugador.y = limiteSuelo;
                if (velocidadY > 0.0f) velocidadY = 0.0f;
                enSuelo = true;
            }
            else {
                enSuelo = false;
            }

            // Limites horizontales
            if (jugador.x < 0.0f) {
                jugador.x = 0.0f;
                velocidadX = 0.0f;
            }

            if (jugador.x > 320.0f - jugador.w) {
                jugador.x = 320.0f - jugador.w;
                velocidadX = 0.0f;
            }

            // Limite superior
            if (jugador.y < 0.0f) {
                jugador.y = 0.0f;
                if (velocidadY < 0.0f) velocidadY = 0.0f;
            }

            // Actualizar balas
            for (Bala& bala : balas) {
                bala.x += bala.vx * dt;
                bala.y += bala.vy * dt;
            }

            // Eliminar balas fuera de pantalla
            for (int i = (int)balas.size() - 1; i >= 0; i--) {
                if (balas[i].x < 0 || balas[i].x > 320 ||
                    balas[i].y < 0 || balas[i].y > 180) {
                    balas.erase(balas.begin() + i);
                }
            }
        }

        // Recalcular posicion del arma despues del movimiento
        manoX = jugador.x + jugador.w / 2.0f;
        manoY = jugador.y + jugador.h * 0.60f;

        dx = mouseX - manoX;
        dy = mouseY - manoY;
        distancia = sqrtf(dx * dx + dy * dy);

        if (distancia > 0.001f) {
            anguloArma = atan2f(dy, dx) * 180.0f / 3.14159265f;
            mirandoIzq = dx < 0;
        }

        SDL_SetRenderDrawColor(ren, 30, 30, 45, 255);
        SDL_RenderClear(ren);

        SDL_RenderTexture(ren, texCielo, NULL, NULL);
        SDL_RenderTextureTiled(ren, texSuelo, NULL, 1.0f, &suelo);

        SDL_FRect srcHeroe = {
            cuadro * SPRITE_ANCHO,
            FILA_CONEJO * SPRITE_ALTO,
            SPRITE_ANCHO,
            SPRITE_ALTO
        };

        SDL_FRect destinoHeroe = {
            SDL_roundf(jugador.x),
            SDL_roundf(jugador.y),
            jugador.w,
            jugador.h
        };

        SDL_FlipMode flipHeroe = mirandoIzq ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

        SDL_RenderTextureRotated(ren, texHeroe, &srcHeroe, &destinoHeroe,
            0.0, NULL, flipHeroe);

        SDL_FRect arma = {
            SDL_roundf(manoX - AGARRE_X),
            SDL_roundf(manoY - AGARRE_Y),
            ARMA_ANCHO,
            ARMA_ALTO
        };

        SDL_FPoint centroRotacion = { AGARRE_X, AGARRE_Y };
        SDL_FlipMode flipArma = mirandoIzq ? SDL_FLIP_VERTICAL : SDL_FLIP_NONE;

        SDL_RenderTextureRotated(ren, texArma, NULL, &arma,
            anguloArma, &centroRotacion, flipArma);

        // Dibujar proyectiles
        SDL_SetRenderDrawColor(ren, 255, 220, 70, 255);

        for (const Bala& bala : balas) {
            SDL_FRect proyectil = {
                SDL_roundf(bala.x - 2.0f),
                SDL_roundf(bala.y - 2.0f),
                4.0f,
                4.0f
            };

            SDL_RenderFillRect(ren, &proyectil);
        }

        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderDebugTextFormat(ren, 4, 4, "ANIM: %s CUADRO: %d",
            NOMBRES[actual], cuadro);

        SDL_RenderDebugTextFormat(ren, 4, 14, "BALAS: %d", (int)balas.size());
        SDL_RenderDebugTextFormat(ren, 4, 24, "VEL X: %.0f Y: %.0f",
            velocidadX, velocidadY);

        if (pausa) SDL_RenderDebugText(ren, 140, 86, "PAUSA");

        SDL_RenderPresent(ren);
    }

    SDL_DestroyTexture(texHeroe);
    SDL_DestroyTexture(texCielo);
    SDL_DestroyTexture(texSuelo);
    SDL_DestroyTexture(texArma);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return 0;
}
