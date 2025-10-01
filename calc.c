#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#define CLAY_IMPLEMENTATION
#include "clay.h"

#include <stdio.h>
#include <stdlib.h>

#define ENSURE(x) if (!(x)) { printf("%s:%d: Ensure failed! ENSURE(%s)\n", __FILE__, __LINE__, #x); fflush(stdout); int *y = 0; *y = 42; }
#define MOVE(x) (x)


// ------------------------------------------------------------------------------------------
// Layout
// ------------------------------------------------------------------------------------------

static const int FONT_ID_BODY_16 = 0;
static const Clay_Color COLOR_BACKGROUND = (Clay_Color) {43, 41, 51, 255 };
static const Clay_Color COLOR_NUMBER_BUTTON = (Clay_Color) {38, 38, 38, 255};
static const Clay_Color COLOR_OPERATION_BUTTON = (Clay_Color) {24, 24, 27, 255};
static const Clay_Color COLOR_SPECIAL_OPERATION_BUTTON = (Clay_Color) {30, 64, 175, 255};
static const Clay_Color COLOR_HOVERED_BUTTON = (Clay_Color) {30, 64, 175, 255};
static const Clay_Color COLOR_HOVERED_SPECIAL_BUTTON = (Clay_Color) {30, 27, 75, 255};
static const Clay_Color COLOR_CLICKED_BUTTON = (Clay_Color) {23, 37, 84, 255};
static const Clay_Color COLOR_CONTENT_BACKGROUND = { 90, 90, 90, 255 };
static Clay_Color gSpecialButtonColor = (Clay_Color) {30, 64, 175, 255};

typedef enum {
    CALC_MSG_DIGIT,
    CALC_MSG_DOT,
    CALC_MSG_PLUS,
    CALC_MSG_EQUALS
} CalcMsgType;

typedef struct {
    CalcMsgType type;
    char digit;
} CalcMsg;

typedef struct {
    char *buffer;
    size_t size;
    size_t capacity;
} CalcString;

typedef struct {
    CalcString data[16];
    size_t head;
    size_t tail;
} CalcHistory;

typedef struct {
    CalcMsg *head;
    size_t size;
    size_t capacity;
} CalcMsgQueue;

typedef struct {
    uint8_t *base;
    size_t offset;
    size_t capacity;
} CalcArena;

typedef struct {
    CalcArena appArena;
    CalcArena frameArena;

    // ------------------------------------------------------------------------
    // App data: Data that lives as long as the application does
    CalcString operand0;
    CalcString operand1;
    CalcMsgType lastMessage;

    CalcHistory history;

    // ------------------------------------------------------------------------
    // Frame data: Data that lives as long as the frame
    CalcMsgQueue msgQueue;
} CalcData;

CalcData *gAppData;

size_t AlignPow2(size_t value, size_t alignment)
{
    return (value + (alignment - 1)) & ~(alignment - 1);
}

uint8_t *CalcArenaAllocWithAlignment(CalcArena *arena, size_t size, size_t alignment) {
    size_t start = AlignPow2(arena->offset, alignment);
    ENSURE((start + size) < arena->capacity);
    arena->offset += size;

    uint8_t *ptr = arena->base + start;
    memset(ptr, 0, size);
    return ptr;
}

uint8_t *CalcArenaAlloc(CalcArena *arena, size_t size) {
    return CalcArenaAllocWithAlignment(arena, size, 8);
}

CalcString CalcArenaAllocString(CalcArena *arena, size_t capacity) {
    return (CalcString) {
        .buffer = (char*)CalcArenaAlloc(arena, capacity),
        .size = 0,
        .capacity = capacity
    };
}

void CalcStringAppend(CalcString *str, char x) {
    if (str->size < str->capacity) {
        str->buffer[str->size] = x;
        str->size += 1ll;
    }
}

void CalcStringCopy(CalcString *dest, CalcString *src) {
    ENSURE(dest->capacity > src->size);
    memcpy(dest->buffer, src->buffer, src->size);
    dest->size = src->size;
}

void CalcStringClear(CalcString *str) {
    str->size = 0;
}

double CalcStringToDouble(CalcString *str) {
    double result = 0.0;
    if (str->size > 0) {
        char buffer[17];
        buffer[str->size] = 0;
        memcpy(buffer, str->buffer, str->size);
        result = atof(buffer);
    }

    return result;
}

void CalcDoubleToString(CalcString *dest, double value) {
    int size = snprintf(dest->buffer, dest->capacity, "%f", value);
    /* while (dest->buffer[size - 1] == '0' || dest->buffer[size - 1] == '.') { */
    /*     size--; */
    /* } */

    dest->size = size;
}

CalcData *CalcInitialize(CalcArena appArena) {
    CalcArena frameArena = { .base = (uint8_t*)malloc(1024), .offset = 0, .capacity = 1024 };

    gAppData = (CalcData*)CalcArenaAlloc(&appArena, sizeof(CalcData));

    gAppData->operand0 = CalcArenaAllocString(&appArena, 16);
    gAppData->operand1 = CalcArenaAllocString(&appArena, 16);
    gAppData->lastMessage = CALC_MSG_EQUALS;

    for (size_t i = 0; i < 16; i++) {
        gAppData->history.data[i] = CalcArenaAllocString(&appArena, 16);
    }

    gAppData->msgQueue.head = (CalcMsg*)CalcArenaAlloc(&frameArena, 128ll);
    gAppData->msgQueue.size = 0ll;
    gAppData->msgQueue.capacity = 128ll;

    gAppData->appArena = appArena;
    gAppData->frameArena = frameArena;
    return gAppData;
}

void CalcUpdate(CalcData *appData) {
    CalcMsgQueue *msgQueue = &appData->msgQueue;
    for (size_t i = 0ll; i < msgQueue->size; ++i) {
        CalcMsg *msg = msgQueue->head + i;
        switch (msg->type) {
            case CALC_MSG_DIGIT:
                CalcStringAppend(&appData->operand1, msg->digit);
                break;
            case CALC_MSG_DOT:
                CalcStringAppend(&appData->operand1, '.');
                break;
            case CALC_MSG_EQUALS:
                break;
            case CALC_MSG_PLUS:
                double op0 = CalcStringToDouble(&appData->operand0);
                double op1 = CalcStringToDouble(&appData->operand1);
                double result = op0 + op1;
                appData->lastMessage = CALC_MSG_PLUS;
                CalcDoubleToString(&appData->operand0, result);
                CalcStringClear(&appData->operand1);
                break;
        }

        printf("[Calc] Msg: %d | Operand0: %.*s, | Operand1: %.*s\n",
               msg->type,
               (int)appData->operand0.size, appData->operand0.buffer,
               (int)appData->operand1.size, appData->operand1.buffer);
    }

    msgQueue->size = 0;

}

Clay_ElementDeclaration MakeRect(Clay_ElementId id, Clay_Sizing sizing, uint16_t childGap) {
    return (Clay_ElementDeclaration) {
        .id = id,
        .layout = {
            .sizing = sizing,
            .childGap = childGap
        }
    };
}

Clay_ElementDeclaration MakePanel(Clay_ElementId id, Clay_Color color, Clay_LayoutDirection direction, uint16_t childGap, Clay_Padding padding) {
    Clay_ElementDeclaration panel = MakeRect(id, (Clay_Sizing) { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) }, childGap);
    panel.backgroundColor = color;
    panel.layout.layoutDirection = direction;
    panel.layout.padding = padding;
    return panel;
}

Clay_ElementDeclaration MakeColumn(Clay_ElementId id) {
    Clay_ElementDeclaration column = MakeRect(id, (Clay_Sizing) { .width = CLAY_SIZING_PERCENT(0.20f), .height = CLAY_SIZING_GROW(0) }, 8);
    column.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
    return column;
}

Clay_ElementDeclaration MakeOutputPanel(Clay_ElementId id) {
    Clay_ElementDeclaration panel = MakePanel(id, COLOR_CONTENT_BACKGROUND, CLAY_TOP_TO_BOTTOM, 16, (Clay_Padding) {16, 16, 0 , 0});
    panel.layout.sizing.height = CLAY_SIZING_FIXED(300);
    panel.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
    panel.cornerRadius = CLAY_CORNER_RADIUS(8);
    return panel;
}

void MakeSpacer(Clay_Color color) {
    CLAY({
        .layout = { .sizing = { .height = CLAY_SIZING_GROW(0), .width = CLAY_SIZING_GROW(0) }, .padding = { 16, 16, 8, 8 }},
        .backgroundColor = color,
    }) {
    }
}

void CalcQueueMsg(CalcMsg *msg) {
    CalcMsg *slot = gAppData->msgQueue.head + gAppData->msgQueue.size;
    memcpy(slot, msg, sizeof(CalcMsg));
    gAppData->msgQueue.size += 1ll;
}

CalcMsg MakeMsgDigit(uint8_t digit) {
    CalcMsg msg;
    msg.type = CALC_MSG_DIGIT;
    msg.digit = digit;
    return msg;
}

CalcMsg MakeMsg(CalcMsgType type) {
    CalcMsg msg;
    msg.type = type;
    return msg;
}

CalcMsg *CopyMsgToFrameArena(CalcMsg msg) {
    CalcMsg *copy = (CalcMsg*)CalcArenaAlloc(&gAppData->frameArena, sizeof(CalcMsg));
    memcpy(copy, &msg, sizeof(CalcMsg));
    return copy;
}

void OnButtonHovered(Clay_ElementId elemendId, Clay_PointerData pointerInfo, intptr_t userData) {
    CalcMsg* msg = (CalcMsg*)userData;
    if (pointerInfo.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        gSpecialButtonColor = COLOR_CLICKED_BUTTON;
    } else if (pointerInfo.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
        CalcQueueMsg(msg);
        gSpecialButtonColor = COLOR_HOVERED_BUTTON;
        /* printf("[Calc] OnNumberButtonReleased: %d\n", msg->digit); */
    }
}

void MakeNumberButton(Clay_String text, CalcMsg msg) {
    CLAY({
        .layout = { .sizing = { .height = CLAY_SIZING_PERCENT(0.25f), .width = CLAY_SIZING_GROW(0) }, .padding = { 16, 16, 8, 8 }},
        .backgroundColor = Clay_Hovered() ? gSpecialButtonColor : COLOR_NUMBER_BUTTON,
        .cornerRadius = CLAY_CORNER_RADIUS(5)
    }) {
        Clay_OnHover(OnButtonHovered, (intptr_t)CopyMsgToFrameArena(msg));
        CLAY_TEXT(text, CLAY_TEXT_CONFIG({
            .fontId = FONT_ID_BODY_16,
            .fontSize = 16,
            .textColor = { 255, 255, 255, 255 }
        }));
    }
}

void MakeOperationButton(Clay_String text, CalcMsg msg) {
    CLAY({
        .layout = { .sizing = { .height = CLAY_SIZING_PERCENT(0.25f), .width = CLAY_SIZING_GROW(0) }, .padding = { 16, 16, 8, 8 }},
        .backgroundColor = Clay_Hovered() ? COLOR_HOVERED_SPECIAL_BUTTON : COLOR_OPERATION_BUTTON,
        .cornerRadius = CLAY_CORNER_RADIUS(5)
    }) {
        Clay_OnHover(OnButtonHovered, (intptr_t)CopyMsgToFrameArena(msg));
        CLAY_TEXT(text, CLAY_TEXT_CONFIG({
            .fontId = FONT_ID_BODY_16,
            .fontSize = 16,
            .textColor = { 255, 255, 255, 255 }
        }));
    }
}

void MakeSpecialOperationButton(Clay_String text, CalcMsg msg) {
    CLAY({
        .layout = { .sizing = { .height = CLAY_SIZING_PERCENT(0.5f), .width = CLAY_SIZING_GROW(0) }, .padding = { 16, 16, 8, 8 }},
        .backgroundColor = Clay_Hovered() ? COLOR_HOVERED_SPECIAL_BUTTON : COLOR_SPECIAL_OPERATION_BUTTON,
        .cornerRadius = CLAY_CORNER_RADIUS(5)
    }) {
        Clay_OnHover(OnButtonHovered, (intptr_t)CopyMsgToFrameArena(msg));
        CLAY_TEXT(text, CLAY_TEXT_CONFIG({
            .fontId = FONT_ID_BODY_16,
            .fontSize = 16,
            .textColor = { 255, 255, 255, 255 }
        }));
    }
}

Clay_RenderCommandArray CalcRender(CalcData *appData) {
    appData->frameArena.offset = 0;

    Clay_BeginLayout();

    CLAY(MakePanel(CLAY_ID("OuterContainer"), COLOR_BACKGROUND, CLAY_TOP_TO_BOTTOM, 8, CLAY_PADDING_ALL(8))) {
        CLAY(MakeOutputPanel(CLAY_ID("OutputPanel"))) {
            CLAY(MakeColumn(CLAY_ID("Outputs"))) {
                Clay_String txt = (Clay_String){ .isStaticallyAllocated = false,
                .length = appData->operand1.size,
                .chars = appData->operand1.buffer };
                /* printf("[Calc] %.*s\n", txt.length, txt.chars); */
                CLAY_TEXT(txt, CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BODY_16, .fontSize = 16, .textColor = { 255, 255, 255, 255 } }));
            }
        }

        CLAY(MakeRect(CLAY_ID("InputControls"), (Clay_Sizing) { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) }, 8)) {
            CLAY(MakePanel(CLAY_ID("MainContent"), COLOR_CONTENT_BACKGROUND, CLAY_LEFT_TO_RIGHT, 8, CLAY_PADDING_ALL(8))) {
                CLAY(MakeColumn(CLAY_ID("ButtonsCol0"))) {
                    MakeNumberButton(CLAY_STRING("7"), MOVE(MakeMsgDigit('7')));
                    MakeNumberButton(CLAY_STRING("4"), MOVE(MakeMsgDigit('4')));
                    MakeNumberButton(CLAY_STRING("1"), MOVE(MakeMsgDigit('1')));
                    MakeNumberButton(CLAY_STRING("0"), MOVE(MakeMsgDigit('0')));
                }

                CLAY(MakeColumn(CLAY_ID("ButtonsCol1"))) {
                    MakeNumberButton(CLAY_STRING("8"), MOVE(MakeMsgDigit('8')));
                    MakeNumberButton(CLAY_STRING("5"), MOVE(MakeMsgDigit('5')));
                    MakeNumberButton(CLAY_STRING("2"), MOVE(MakeMsgDigit('2')));
                    MakeOperationButton(CLAY_STRING("."), MOVE(MakeMsg(CALC_MSG_DOT)));
                }

                CLAY(MakeColumn(CLAY_ID("ButtonsCol2"))) {
                    MakeNumberButton(CLAY_STRING("9"), MOVE(MakeMsgDigit('9')));
                    MakeNumberButton(CLAY_STRING("6"), MOVE(MakeMsgDigit('6')));
                    MakeNumberButton(CLAY_STRING("3"), MOVE(MakeMsgDigit('3')));
                    MakeOperationButton(CLAY_STRING("%"), MOVE(MakeMsg(CALC_MSG_PLUS)));
                }

                CLAY(MakeColumn(CLAY_ID("ButtonsCol3"))) {
                    MakeOperationButton(CLAY_STRING("÷"), MOVE(MakeMsg(CALC_MSG_PLUS)));
                    MakeOperationButton(CLAY_STRING("×"), MOVE(MakeMsg(CALC_MSG_PLUS)));
                    MakeOperationButton(CLAY_STRING("-"), MOVE(MakeMsg(CALC_MSG_PLUS)));
                    MakeOperationButton(CLAY_STRING("+"), MOVE(MakeMsg(CALC_MSG_PLUS)));
                }

                CLAY(MakeColumn(CLAY_ID("ButtonsCol4"))) {
                    MakeSpacer(COLOR_CONTENT_BACKGROUND);
                    MakeSpecialOperationButton(CLAY_STRING("="), MOVE(MakeMsg(CALC_MSG_EQUALS)));
                }
            }
        }
    }

    Clay_RenderCommandArray renderCommands = Clay_EndLayout();
    for (int32_t i = 0; i < renderCommands.length; i++) {
        // Clay_RenderCommandArray_Get(&renderCommands, i)->boundingBox.y += data->yOffset;
    }
    return renderCommands;
}


// ------------------------------------------------------------------------------------------
// Renderer
// ------------------------------------------------------------------------------------------
typedef struct {
    SDL_Renderer *renderer;
    TTF_TextEngine *textEngine;
    TTF_Font **fonts;
} Clay_SDL3RendererData;

/* Global for convenience. Even in 4K this is enough for smooth curves (low radius or rect size coupled with
 * no AA or low resolution might make it appear as jagged curves) */
static int NUM_CIRCLE_SEGMENTS = 16;

//all rendering is performed by a single SDL call, avoiding multiple RenderRect + plumbing choice for circles.
static void SDL_Clay_RenderFillRoundedRect(Clay_SDL3RendererData *rendererData, const SDL_FRect rect, const float cornerRadius, const Clay_Color _color) {
    const SDL_FColor color = { _color.r/255, _color.g/255, _color.b/255, _color.a/255 };

    int indexCount = 0, vertexCount = 0;

    const float minRadius = SDL_min(rect.w, rect.h) / 2.0f;
    const float clampedRadius = SDL_min(cornerRadius, minRadius);

    const int numCircleSegments = SDL_max(NUM_CIRCLE_SEGMENTS, (int) clampedRadius * 0.5f);

    int totalVertices = 4 + (4 * (numCircleSegments * 2)) + 2*4;
    int totalIndices = 6 + (4 * (numCircleSegments * 3)) + 6*4;

    SDL_Vertex vertices[totalVertices];
    int indices[totalIndices];

    //define center rectangle
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + clampedRadius, rect.y + clampedRadius}, color, {0, 0} }; //0 center TL
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + rect.w - clampedRadius, rect.y + clampedRadius}, color, {1, 0} }; //1 center TR
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + rect.w - clampedRadius, rect.y + rect.h - clampedRadius}, color, {1, 1} }; //2 center BR
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + clampedRadius, rect.y + rect.h - clampedRadius}, color, {0, 1} }; //3 center BL

    indices[indexCount++] = 0;
    indices[indexCount++] = 1;
    indices[indexCount++] = 3;
    indices[indexCount++] = 1;
    indices[indexCount++] = 2;
    indices[indexCount++] = 3;

    //define rounded corners as triangle fans
    const float step = (SDL_PI_F/2) / numCircleSegments;
    for (int i = 0; i < numCircleSegments; i++) {
        const float angle1 = (float)i * step;
        const float angle2 = ((float)i + 1.0f) * step;

        for (int j = 0; j < 4; j++) {  // Iterate over four corners
            float cx, cy, signX, signY;

            switch (j) {
                case 0: cx = rect.x + clampedRadius; cy = rect.y + clampedRadius; signX = -1; signY = -1; break; // Top-left
                case 1: cx = rect.x + rect.w - clampedRadius; cy = rect.y + clampedRadius; signX = 1; signY = -1; break; // Top-right
                case 2: cx = rect.x + rect.w - clampedRadius; cy = rect.y + rect.h - clampedRadius; signX = 1; signY = 1; break; // Bottom-right
                case 3: cx = rect.x + clampedRadius; cy = rect.y + rect.h - clampedRadius; signX = -1; signY = 1; break; // Bottom-left
                default: return;
            }

            vertices[vertexCount++] = (SDL_Vertex){ {cx + SDL_cosf(angle1) * clampedRadius * signX, cy + SDL_sinf(angle1) * clampedRadius * signY}, color, {0, 0} };
            vertices[vertexCount++] = (SDL_Vertex){ {cx + SDL_cosf(angle2) * clampedRadius * signX, cy + SDL_sinf(angle2) * clampedRadius * signY}, color, {0, 0} };

            indices[indexCount++] = j;  // Connect to corresponding central rectangle vertex
            indices[indexCount++] = vertexCount - 2;
            indices[indexCount++] = vertexCount - 1;
        }
    }

    //Define edge rectangles
    // Top edge
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + clampedRadius, rect.y}, color, {0, 0} }; //TL
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + rect.w - clampedRadius, rect.y}, color, {1, 0} }; //TR

    indices[indexCount++] = 0;
    indices[indexCount++] = vertexCount - 2; //TL
    indices[indexCount++] = vertexCount - 1; //TR
    indices[indexCount++] = 1;
    indices[indexCount++] = 0;
    indices[indexCount++] = vertexCount - 1; //TR
    // Right edge
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + rect.w, rect.y + clampedRadius}, color, {1, 0} }; //RT
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + rect.w, rect.y + rect.h - clampedRadius}, color, {1, 1} }; //RB

    indices[indexCount++] = 1;
    indices[indexCount++] = vertexCount - 2; //RT
    indices[indexCount++] = vertexCount - 1; //RB
    indices[indexCount++] = 2;
    indices[indexCount++] = 1;
    indices[indexCount++] = vertexCount - 1; //RB
    // Bottom edge
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + rect.w - clampedRadius, rect.y + rect.h}, color, {1, 1} }; //BR
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + clampedRadius, rect.y + rect.h}, color, {0, 1} }; //BL

    indices[indexCount++] = 2;
    indices[indexCount++] = vertexCount - 2; //BR
    indices[indexCount++] = vertexCount - 1; //BL
    indices[indexCount++] = 3;
    indices[indexCount++] = 2;
    indices[indexCount++] = vertexCount - 1; //BL
    // Left edge
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x, rect.y + rect.h - clampedRadius}, color, {0, 1} }; //LB
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x, rect.y + clampedRadius}, color, {0, 0} }; //LT

    indices[indexCount++] = 3;
    indices[indexCount++] = vertexCount - 2; //LB
    indices[indexCount++] = vertexCount - 1; //LT
    indices[indexCount++] = 0;
    indices[indexCount++] = 3;
    indices[indexCount++] = vertexCount - 1; //LT

    // Render everything
    SDL_RenderGeometry(rendererData->renderer, NULL, vertices, vertexCount, indices, indexCount);
}

static void SDL_Clay_RenderArc(Clay_SDL3RendererData *rendererData, const SDL_FPoint center, const float radius, const float startAngle, const float endAngle, const float thickness, const Clay_Color color) {
    SDL_SetRenderDrawColor(rendererData->renderer, color.r, color.g, color.b, color.a);

    const float radStart = startAngle * (SDL_PI_F / 180.0f);
    const float radEnd = endAngle * (SDL_PI_F / 180.0f);

    const int numCircleSegments = SDL_max(NUM_CIRCLE_SEGMENTS, (int)(radius * 1.5f)); //increase circle segments for larger circles, 1.5 is arbitrary.

    const float angleStep = (radEnd - radStart) / (float)numCircleSegments;
    const float thicknessStep = 0.4f; //arbitrary value to avoid overlapping lines. Changing THICKNESS_STEP or numCircleSegments might cause artifacts.

    for (float t = thicknessStep; t < thickness - thicknessStep; t += thicknessStep) {
        SDL_FPoint points[numCircleSegments + 1];
        const float clampedRadius = SDL_max(radius - t, 1.0f);

        for (int i = 0; i <= numCircleSegments; i++) {
            const float angle = radStart + i * angleStep;
            points[i] = (SDL_FPoint){
                    SDL_roundf(center.x + SDL_cosf(angle) * clampedRadius),
                    SDL_roundf(center.y + SDL_sinf(angle) * clampedRadius) };
        }
        SDL_RenderLines(rendererData->renderer, points, numCircleSegments + 1);
    }
}

SDL_Rect currentClippingRectangle;

static void SDL_Clay_RenderClayCommands(Clay_SDL3RendererData *rendererData, Clay_RenderCommandArray *rcommands) {
    for (size_t i = 0; i < rcommands->length; i++) {
        Clay_RenderCommand *rcmd = Clay_RenderCommandArray_Get(rcommands, i);
        const Clay_BoundingBox bounding_box = rcmd->boundingBox;
        const SDL_FRect rect = { (int)bounding_box.x, (int)bounding_box.y, (int)bounding_box.width, (int)bounding_box.height };

        switch (rcmd->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                Clay_RectangleRenderData *config = &rcmd->renderData.rectangle;
                SDL_SetRenderDrawBlendMode(rendererData->renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(rendererData->renderer, config->backgroundColor.r, config->backgroundColor.g, config->backgroundColor.b, config->backgroundColor.a);
                if (config->cornerRadius.topLeft > 0) {
                    SDL_Clay_RenderFillRoundedRect(rendererData, rect, config->cornerRadius.topLeft, config->backgroundColor);
                } else {
                    SDL_RenderFillRect(rendererData->renderer, &rect);
                }
            } break;
            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                Clay_TextRenderData *config = &rcmd->renderData.text;
                TTF_Font *font = rendererData->fonts[config->fontId];
                TTF_SetFontSize(font, config->fontSize);
                TTF_Text *text = TTF_CreateText(rendererData->textEngine, font, config->stringContents.chars, config->stringContents.length);
                TTF_SetTextColor(text, config->textColor.r, config->textColor.g, config->textColor.b, config->textColor.a);
                TTF_DrawRendererText(text, rect.x, rect.y);
                TTF_DestroyText(text);
            } break;
            case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                Clay_BorderRenderData *config = &rcmd->renderData.border;

                const float minRadius = SDL_min(rect.w, rect.h) / 2.0f;
                const Clay_CornerRadius clampedRadii = {
                    .topLeft = SDL_min(config->cornerRadius.topLeft, minRadius),
                    .topRight = SDL_min(config->cornerRadius.topRight, minRadius),
                    .bottomLeft = SDL_min(config->cornerRadius.bottomLeft, minRadius),
                    .bottomRight = SDL_min(config->cornerRadius.bottomRight, minRadius)
                };
                //edges
                SDL_SetRenderDrawColor(rendererData->renderer, config->color.r, config->color.g, config->color.b, config->color.a);
                if (config->width.left > 0) {
                    const float starting_y = rect.y + clampedRadii.topLeft;
                    const float length = rect.h - clampedRadii.topLeft - clampedRadii.bottomLeft;
                    SDL_FRect line = { rect.x - 1, starting_y, config->width.left, length };
                    SDL_RenderFillRect(rendererData->renderer, &line);
                }
                if (config->width.right > 0) {
                    const float starting_x = rect.x + rect.w - (float)config->width.right + 1;
                    const float starting_y = rect.y + clampedRadii.topRight;
                    const float length = rect.h - clampedRadii.topRight - clampedRadii.bottomRight;
                    SDL_FRect line = { starting_x, starting_y, config->width.right, length };
                    SDL_RenderFillRect(rendererData->renderer, &line);
                }
                if (config->width.top > 0) {
                    const float starting_x = rect.x + clampedRadii.topLeft;
                    const float length = rect.w - clampedRadii.topLeft - clampedRadii.topRight;
                    SDL_FRect line = { starting_x, rect.y - 1, length, config->width.top };
                    SDL_RenderFillRect(rendererData->renderer, &line);
                }
                if (config->width.bottom > 0) {
                    const float starting_x = rect.x + clampedRadii.bottomLeft;
                    const float starting_y = rect.y + rect.h - (float)config->width.bottom + 1;
                    const float length = rect.w - clampedRadii.bottomLeft - clampedRadii.bottomRight;
                    SDL_FRect line = { starting_x, starting_y, length, config->width.bottom };
                    SDL_SetRenderDrawColor(rendererData->renderer, config->color.r, config->color.g, config->color.b, config->color.a);
                    SDL_RenderFillRect(rendererData->renderer, &line);
                }
                //corners
                if (config->cornerRadius.topLeft > 0) {
                    const float centerX = rect.x + clampedRadii.topLeft -1;
                    const float centerY = rect.y + clampedRadii.topLeft - 1;
                    SDL_Clay_RenderArc(rendererData, (SDL_FPoint){centerX, centerY}, clampedRadii.topLeft,
                        180.0f, 270.0f, config->width.top, config->color);
                }
                if (config->cornerRadius.topRight > 0) {
                    const float centerX = rect.x + rect.w - clampedRadii.topRight;
                    const float centerY = rect.y + clampedRadii.topRight - 1;
                    SDL_Clay_RenderArc(rendererData, (SDL_FPoint){centerX, centerY}, clampedRadii.topRight,
                        270.0f, 360.0f, config->width.top, config->color);
                }
                if (config->cornerRadius.bottomLeft > 0) {
                    const float centerX = rect.x + clampedRadii.bottomLeft -1;
                    const float centerY = rect.y + rect.h - clampedRadii.bottomLeft;
                    SDL_Clay_RenderArc(rendererData, (SDL_FPoint){centerX, centerY}, clampedRadii.bottomLeft,
                        90.0f, 180.0f, config->width.bottom, config->color);
                }
                if (config->cornerRadius.bottomRight > 0) {
                    const float centerX = rect.x + rect.w - clampedRadii.bottomRight;
                    const float centerY = rect.y + rect.h - clampedRadii.bottomRight;
                    SDL_Clay_RenderArc(rendererData, (SDL_FPoint){centerX, centerY}, clampedRadii.bottomRight,
                        0.0f, 90.0f, config->width.bottom, config->color);
                }

            } break;
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                Clay_BoundingBox boundingBox = rcmd->boundingBox;
                currentClippingRectangle = (SDL_Rect) {
                        .x = boundingBox.x,
                        .y = boundingBox.y,
                        .w = boundingBox.width,
                        .h = boundingBox.height,
                };
                SDL_SetRenderClipRect(rendererData->renderer, &currentClippingRectangle);
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
                SDL_SetRenderClipRect(rendererData->renderer, NULL);
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
                SDL_Texture *texture = (SDL_Texture *)rcmd->renderData.image.imageData;
                const SDL_FRect dest = { rect.x, rect.y, rect.w, rect.h };
                SDL_RenderTexture(rendererData->renderer, texture, NULL, &dest);
                break;
            }
            default:
                SDL_Log("Unknown render command type: %d", rcmd->commandType);
        }
    }
}


// ------------------------------------------------------------------------------------------
// SDL Application
// ------------------------------------------------------------------------------------------
static const Uint32 FONT_ID = 0;

typedef struct {
    SDL_Window *window;
    Clay_SDL3RendererData rendererData;
    CalcData* appData;
} AppState;

static inline Clay_Dimensions SDL_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    TTF_Font **fonts = userData;
    TTF_Font *font = fonts[config->fontId];
    int width, height;

    TTF_SetFontSize(font, config->fontSize);
    if (!TTF_GetStringSize(font, text.chars, text.length, &width, &height)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to measure text: %s", SDL_GetError());
    }

    return (Clay_Dimensions) { (float) width, (float) height };
}

void HandleClayErrors(Clay_ErrorData errorData) {
    printf("Error: %s\n", errorData.errorText.chars);
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    if (!TTF_Init()) {
        return SDL_APP_FAILURE;
    }

    AppState *state = SDL_calloc(1, sizeof(AppState));
    if (!state) {
        return SDL_APP_FAILURE;
    }
    *appstate = state;

    if (!SDL_CreateWindowAndRenderer("Calc", 600, 800, 0, &state->window, &state->rendererData.renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetWindowResizable(state->window, true);
    SDL_SetRenderVSync(state->rendererData.renderer, 1);

    state->rendererData.textEngine = TTF_CreateRendererTextEngine(state->rendererData.renderer);
    if (!state->rendererData.textEngine) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create text engine from renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    state->rendererData.fonts = SDL_calloc(1, sizeof(TTF_Font *));
    if (!state->rendererData.fonts) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to allocate memory for the font array: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    TTF_Font *font = TTF_OpenFont("resources/Roboto-Regular.ttf", 24);
    if (!font) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to load font: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    state->rendererData.fonts[FONT_ID] = font;

    /* Initialize Clay */
    uint64_t totalMemorySize = Clay_MinMemorySize();
    Clay_Arena clayMemory = (Clay_Arena) {
        .memory = SDL_malloc(totalMemorySize),
        .capacity = totalMemorySize
    };

    int width, height;
    SDL_GetWindowSize(state->window, &width, &height);
    Clay_Initialize(clayMemory, (Clay_Dimensions) { (float) width, (float) height }, (Clay_ErrorHandler) { HandleClayErrors });
    Clay_SetMeasureTextFunction(SDL_MeasureText, state->rendererData.fonts);

    CalcArena appArena = { .base = (uint8_t*)malloc(1024), .offset = 0, .capacity = 1024 };
    state->appData = CalcInitialize(MOVE(appArena));

    *appstate = state;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    SDL_AppResult ret_val = SDL_APP_CONTINUE;

    switch (event->type) {
    case SDL_EVENT_QUIT:
      ret_val = SDL_APP_SUCCESS;
      break;
    case SDL_EVENT_KEY_UP:
      if (event->key.scancode == SDL_SCANCODE_SPACE) {
      }
      break;
    case SDL_EVENT_WINDOW_RESIZED:
      Clay_SetLayoutDimensions((Clay_Dimensions){(float)event->window.data1,
                                                 (float)event->window.data2});
      break;
    case SDL_EVENT_MOUSE_MOTION:
      Clay_SetPointerState((Clay_Vector2){event->motion.x, event->motion.y},
                           event->motion.state & SDL_BUTTON_LMASK);
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      if (event->button.button == SDL_BUTTON_LEFT) {
        Clay_SetPointerState((Clay_Vector2){event->button.x, event->button.y},
                             true);
      }
      break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (event->button.button == SDL_BUTTON_LEFT) {
        Clay_SetPointerState((Clay_Vector2){event->button.x, event->button.y},
                             false);
      }
      break;
    case SDL_EVENT_MOUSE_WHEEL:
      Clay_UpdateScrollContainers(
          true, (Clay_Vector2){event->wheel.x, event->wheel.y}, 0.01f);
      break;
    default:
      break;
    };

    return ret_val;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    AppState *state = appstate;

    CalcUpdate(state->appData);
    Clay_RenderCommandArray render_commands = CalcRender(state->appData);

    SDL_SetRenderDrawColor(state->rendererData.renderer, 0, 0, 0, 255);
    SDL_RenderClear(state->rendererData.renderer);

    SDL_Clay_RenderClayCommands(&state->rendererData, &render_commands);

    SDL_RenderPresent(state->rendererData.renderer);

    fflush(stdout);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    (void) result;

    if (result != SDL_APP_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Application failed to run");
    }

    AppState *state = appstate;

    if (state) {
        if (state->rendererData.renderer)
            SDL_DestroyRenderer(state->rendererData.renderer);

        if (state->window)
            SDL_DestroyWindow(state->window);

        if (state->rendererData.fonts) {
            for(size_t i = 0; i < sizeof(state->rendererData.fonts) / sizeof(*state->rendererData.fonts); i++) {
                TTF_CloseFont(state->rendererData.fonts[i]);
            }

            SDL_free(state->rendererData.fonts);
        }

        if (state->rendererData.textEngine)
            TTF_DestroyRendererTextEngine(state->rendererData.textEngine);

        SDL_free(state);
    }

    TTF_Quit();
}
