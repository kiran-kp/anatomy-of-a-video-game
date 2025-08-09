#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#define CLAY_IMPLEMENTATION
#include "clay.h"

#include <stdio.h>
#include <stdlib.h>

#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>

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

static void SDL_Clay_RenderClayCommands(Clay_SDL3RendererData *rendererData, Clay_RenderCommandArray *rcommands)
{
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

const int FONT_ID_BODY_16 = 0;
Clay_Color COLOR_WHITE = { 255, 255, 255, 255};

void RenderHeaderButton(Clay_String text) {
    CLAY({
        .layout = { .padding = { 16, 16, 8, 8 }},
        .backgroundColor = { 140, 140, 140, 255 },
        .cornerRadius = CLAY_CORNER_RADIUS(5)
    }) {
        CLAY_TEXT(text, CLAY_TEXT_CONFIG({
            .fontId = FONT_ID_BODY_16,
            .fontSize = 16,
            .textColor = { 255, 255, 255, 255 }
        }));
    }
}

void RenderDropdownMenuItem(Clay_String text) {
    CLAY({.layout = { .padding = CLAY_PADDING_ALL(16)}}) {
        CLAY_TEXT(text, CLAY_TEXT_CONFIG({
            .fontId = FONT_ID_BODY_16,
            .fontSize = 16,
            .textColor = { 255, 255, 255, 255 }
        }));
    }
}

typedef struct {
    Clay_String title;
    Clay_String contents;
} Document;

typedef struct {
    Document *documents;
    uint32_t length;
} DocumentArray;

Document documentsRaw[5];

DocumentArray documents = {
    .length = 5,
    .documents = documentsRaw
};

typedef struct {
    intptr_t offset;
    intptr_t memory;
} ClayVideoDemo_Arena;

typedef struct {
    int32_t selectedDocumentIndex;
    float yOffset;
    ClayVideoDemo_Arena frameArena;
} ClayVideoDemo_Data;

typedef struct {
    int32_t requestedDocumentIndex;
    int32_t* selectedDocumentIndex;
} SidebarClickData;

void HandleSidebarInteraction(
    Clay_ElementId elementId,
    Clay_PointerData pointerData,
    intptr_t userData
) {
    SidebarClickData *clickData = (SidebarClickData*)userData;
    // If this button was clicked
    if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        if (clickData->requestedDocumentIndex >= 0 && clickData->requestedDocumentIndex < documents.length) {
            // Select the corresponding document
            *clickData->selectedDocumentIndex = clickData->requestedDocumentIndex;
        }
    }
}

ClayVideoDemo_Data ClayVideoDemo_Initialize() {
    documents.documents[0] = (Document){ .title = CLAY_STRING("Squirrels"), .contents = CLAY_STRING("The Secret Life of Squirrels: Nature's Clever Acrobats\n""Squirrels are often overlooked creatures, dismissed as mere park inhabitants or backyard nuisances. Yet, beneath their fluffy tails and twitching noses lies an intricate world of cunning, agility, and survival tactics that are nothing short of fascinating. As one of the most common mammals in North America, squirrels have adapted to a wide range of environments from bustling urban centers to tranquil forests and have developed a variety of unique behaviors that continue to intrigue scientists and nature enthusiasts alike.\n""\n""Master Tree Climbers\n""At the heart of a squirrel's skill set is its impressive ability to navigate trees with ease. Whether they're darting from branch to branch or leaping across wide gaps, squirrels possess an innate talent for acrobatics. Their powerful hind legs, which are longer than their front legs, give them remarkable jumping power. With a tail that acts as a counterbalance, squirrels can leap distances of up to ten times the length of their body, making them some of the best aerial acrobats in the animal kingdom.\n""But it's not just their agility that makes them exceptional climbers. Squirrels' sharp, curved claws allow them to grip tree bark with precision, while the soft pads on their feet provide traction on slippery surfaces. Their ability to run at high speeds and scale vertical trunks with ease is a testament to the evolutionary adaptations that have made them so successful in their arboreal habitats.\n""\n""Food Hoarders Extraordinaire\n""Squirrels are often seen frantically gathering nuts, seeds, and even fungi in preparation for winter. While this behavior may seem like instinctual hoarding, it is actually a survival strategy that has been honed over millions of years. Known as \"scatter hoarding,\" squirrels store their food in a variety of hidden locations, often burying it deep in the soil or stashing it in hollowed-out tree trunks.\n""Interestingly, squirrels have an incredible memory for the locations of their caches. Research has shown that they can remember thousands of hiding spots, often returning to them months later when food is scarce. However, they don't always recover every stash some forgotten caches eventually sprout into new trees, contributing to forest regeneration. This unintentional role as forest gardeners highlights the ecological importance of squirrels in their ecosystems.\n""\n""The Great Squirrel Debate: Urban vs. Wild\n""While squirrels are most commonly associated with rural or wooded areas, their adaptability has allowed them to thrive in urban environments as well. In cities, squirrels have become adept at finding food sources in places like parks, streets, and even garbage cans. However, their urban counterparts face unique challenges, including traffic, predators, and the lack of natural shelters. Despite these obstacles, squirrels in urban areas are often observed using human infrastructure such as buildings, bridges, and power lines as highways for their acrobatic escapades.\n""There is, however, a growing concern regarding the impact of urban life on squirrel populations. Pollution, deforestation, and the loss of natural habitats are making it more difficult for squirrels to find adequate food and shelter. As a result, conservationists are focusing on creating squirrel-friendly spaces within cities, with the goal of ensuring these resourceful creatures continue to thrive in both rural and urban landscapes.\n""\n""A Symbol of Resilience\n""In many cultures, squirrels are symbols of resourcefulness, adaptability, and preparation. Their ability to thrive in a variety of environments while navigating challenges with agility and grace serves as a reminder of the resilience inherent in nature. Whether you encounter them in a quiet forest, a city park, or your own backyard, squirrels are creatures that never fail to amaze with their endless energy and ingenuity.\n""In the end, squirrels may be small, but they are mighty in their ability to survive and thrive in a world that is constantly changing. So next time you spot one hopping across a branch or darting across your lawn, take a moment to appreciate the remarkable acrobat at work a true marvel of the natural world.\n") };
    documents.documents[1] = (Document){ .title = CLAY_STRING("Lorem Ipsum"), .contents = CLAY_STRING("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.") };
    documents.documents[2] = (Document){ .title = CLAY_STRING("Vacuum Instructions"), .contents = CLAY_STRING("Chapter 3: Getting Started - Unpacking and Setup\n""\n""Congratulations on your new SuperClean Pro 5000 vacuum cleaner! In this section, we will guide you through the simple steps to get your vacuum up and running. Before you begin, please ensure that you have all the components listed in the \"Package Contents\" section on page 2.\n""\n""1. Unboxing Your Vacuum\n""Carefully remove the vacuum cleaner from the box. Avoid using sharp objects that could damage the product. Once removed, place the unit on a flat, stable surface to proceed with the setup. Inside the box, you should find:\n""\n""    The main vacuum unit\n""    A telescoping extension wand\n""    A set of specialized cleaning tools (crevice tool, upholstery brush, etc.)\n""    A reusable dust bag (if applicable)\n""    A power cord with a 3-prong plug\n""    A set of quick-start instructions\n""\n""2. Assembling Your Vacuum\n""Begin by attaching the extension wand to the main body of the vacuum cleaner. Line up the connectors and twist the wand into place until you hear a click. Next, select the desired cleaning tool and firmly attach it to the wand's end, ensuring it is securely locked in.\n""\n""For models that require a dust bag, slide the bag into the compartment at the back of the vacuum, making sure it is properly aligned with the internal mechanism. If your vacuum uses a bagless system, ensure the dust container is correctly seated and locked in place before use.\n""\n""3. Powering On\n""To start the vacuum, plug the power cord into a grounded electrical outlet. Once plugged in, locate the power switch, usually positioned on the side of the handle or body of the unit, depending on your model. Press the switch to the \"On\" position, and you should hear the motor begin to hum. If the vacuum does not power on, check that the power cord is securely plugged in, and ensure there are no blockages in the power switch.\n""\n""Note: Before first use, ensure that the vacuum filter (if your model has one) is properly installed. If unsure, refer to \"Section 5: Maintenance\" for filter installation instructions.") };
    documents.documents[3] = (Document){ .title = CLAY_STRING("Article 4"), .contents = CLAY_STRING("Article 4") };
    documents.documents[4] = (Document){ .title = CLAY_STRING("Article 5"), .contents = CLAY_STRING("Article 5") };

    ClayVideoDemo_Data data = {
        .frameArena = { .memory = (intptr_t)malloc(1024) }
    };
    return data;
}

Clay_RenderCommandArray ClayVideoDemo_CreateLayout(ClayVideoDemo_Data *data) {
    data->frameArena.offset = 0;

    Clay_BeginLayout();

    Clay_Sizing layoutExpand = {
        .width = CLAY_SIZING_GROW(0),
        .height = CLAY_SIZING_GROW(0)
    };

    Clay_Color contentBackgroundColor = { 90, 90, 90, 255 };

    // Build UI here
    CLAY({ .id = CLAY_ID("OuterContainer"),
        .backgroundColor = {43, 41, 51, 255 },
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = layoutExpand,
            .padding = CLAY_PADDING_ALL(16),
            .childGap = 16
        }
    }) {
        // Child elements go inside braces
        CLAY({ .id = CLAY_ID("HeaderBar"),
            .layout = {
                .sizing = {
                    .height = CLAY_SIZING_FIXED(60),
                    .width = CLAY_SIZING_GROW(0)
                },
                .padding = { 16, 16, 0, 0 },
                .childGap = 16,
                .childAlignment = {
                    .y = CLAY_ALIGN_Y_CENTER
                }
            },
            .backgroundColor = contentBackgroundColor,
            .cornerRadius = CLAY_CORNER_RADIUS(8)
        }) {
            // Header buttons go here
            CLAY({ .id = CLAY_ID("FileButton"),
                .layout = { .padding = { 16, 16, 8, 8 }},
                .backgroundColor = {140, 140, 140, 255 },
                .cornerRadius = CLAY_CORNER_RADIUS(5)
            }) {
                CLAY_TEXT(CLAY_STRING("File"), CLAY_TEXT_CONFIG({
                    .fontId = FONT_ID_BODY_16,
                    .fontSize = 16,
                    .textColor = { 255, 255, 255, 255 }
                }));

                bool fileMenuVisible =
                    Clay_PointerOver(Clay_GetElementId(CLAY_STRING("FileButton")))
                    ||
                    Clay_PointerOver(Clay_GetElementId(CLAY_STRING("FileMenu")));

                if (fileMenuVisible) { // Below has been changed slightly to fix the small bug where the menu would dismiss when mousing over the top gap
                    CLAY({ .id = CLAY_ID("FileMenu"),
                        .floating = {
                            .attachTo = CLAY_ATTACH_TO_PARENT,
                            .attachPoints = {
                                .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM
                            },
                        },
                        .layout = {
                            .padding = {0, 0, 8, 8 }
                        }
                    }) {
                        CLAY({
                            .layout = {
                                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                .sizing = {
                                        .width = CLAY_SIZING_FIXED(200)
                                },
                            },
                            .backgroundColor = {40, 40, 40, 255 },
                            .cornerRadius = CLAY_CORNER_RADIUS(8)
                        }) {
                            // Render dropdown items here
                            RenderDropdownMenuItem(CLAY_STRING("New"));
                            RenderDropdownMenuItem(CLAY_STRING("Open"));
                            RenderDropdownMenuItem(CLAY_STRING("Close"));
                        }
                    }
                }
            }
            RenderHeaderButton(CLAY_STRING("Edit"));
            CLAY({ .layout = { .sizing = { CLAY_SIZING_GROW(0) }}}) {}
            RenderHeaderButton(CLAY_STRING("Upload"));
            RenderHeaderButton(CLAY_STRING("Media"));
            RenderHeaderButton(CLAY_STRING("Support"));
        }

        CLAY({
            .id = CLAY_ID("LowerContent"),
            .layout = { .sizing = layoutExpand, .childGap = 16 }
        }) {
            CLAY({
                .id = CLAY_ID("Sidebar"),
                .backgroundColor = contentBackgroundColor,
                .layout = {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .padding = CLAY_PADDING_ALL(16),
                    .childGap = 8,
                    .sizing = {
                        .width = CLAY_SIZING_FIXED(250),
                        .height = CLAY_SIZING_GROW(0)
                    }
                }
            }) {
                for (int i = 0; i < documents.length; i++) {
                    Document document = documents.documents[i];
                    Clay_LayoutConfig sidebarButtonLayout = {
                        .sizing = { .width = CLAY_SIZING_GROW(0) },
                        .padding = CLAY_PADDING_ALL(16)
                    };

                    if (i == data->selectedDocumentIndex) {
                        CLAY({
                            .layout = sidebarButtonLayout,
                            .backgroundColor = {120, 120, 120, 255 },
                            .cornerRadius = CLAY_CORNER_RADIUS(8)
                        }) {
                            CLAY_TEXT(document.title, CLAY_TEXT_CONFIG({
                                .fontId = FONT_ID_BODY_16,
                                .fontSize = 20,
                                .textColor = { 255, 255, 255, 255 }
                            }));
                        }
                    } else {
                        SidebarClickData *clickData = (SidebarClickData *)(data->frameArena.memory + data->frameArena.offset);
                        *clickData = (SidebarClickData) { .requestedDocumentIndex = i, .selectedDocumentIndex = &data->selectedDocumentIndex };
                        data->frameArena.offset += sizeof(SidebarClickData);
                        CLAY({ .layout = sidebarButtonLayout, .backgroundColor = (Clay_Color) { 120, 120, 120, Clay_Hovered() ? 120 : 0 }, .cornerRadius = CLAY_CORNER_RADIUS(8) }) {
                            Clay_OnHover(HandleSidebarInteraction, (intptr_t)clickData);
                            CLAY_TEXT(document.title, CLAY_TEXT_CONFIG({
                                .fontId = FONT_ID_BODY_16,
                                .fontSize = 20,
                                .textColor = { 255, 255, 255, 255 }
                            }));
                        }
                    }
                }
            }

            CLAY({ .id = CLAY_ID("MainContent"),
                .backgroundColor = contentBackgroundColor,
                .clip = { .vertical = true, .childOffset = Clay_GetScrollOffset() },
                .layout = {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .childGap = 16,
                    .padding = CLAY_PADDING_ALL(16),
                    .sizing = layoutExpand
                }
            }) {
                Document selectedDocument = documents.documents[data->selectedDocumentIndex];
                CLAY_TEXT(selectedDocument.title, CLAY_TEXT_CONFIG({
                    .fontId = FONT_ID_BODY_16,
                    .fontSize = 24,
                    .textColor = COLOR_WHITE
                }));
                CLAY_TEXT(selectedDocument.contents, CLAY_TEXT_CONFIG({
                    .fontId = FONT_ID_BODY_16,
                    .fontSize = 24,
                    .textColor = COLOR_WHITE
                }));
            }
        }
    }

    Clay_RenderCommandArray renderCommands = Clay_EndLayout();
    for (int32_t i = 0; i < renderCommands.length; i++) {
        Clay_RenderCommandArray_Get(&renderCommands, i)->boundingBox.y += data->yOffset;
    }
    return renderCommands;
}
static const Uint32 FONT_ID = 0;

static const Clay_Color COLOR_ORANGE    = (Clay_Color) {225, 138, 50, 255};
static const Clay_Color COLOR_BLUE      = (Clay_Color) {111, 173, 162, 255};
static const Clay_Color COLOR_LIGHT     = (Clay_Color) {224, 215, 210, 255};

typedef struct app_state {
    SDL_Window *window;
    Clay_SDL3RendererData rendererData;
    ClayVideoDemo_Data demoData;
} AppState;

static inline Clay_Dimensions SDL_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData)
{
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
    printf("%s", errorData.errorText.chars);
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
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

    if (!SDL_CreateWindowAndRenderer("Clay Demo", 1920, 1080, 0, &state->window, &state->rendererData.renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetWindowResizable(state->window, true);

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

    state->demoData = ClayVideoDemo_Initialize();

    *appstate = state;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
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
            Clay_SetLayoutDimensions((Clay_Dimensions) { (float) event->window.data1, (float) event->window.data2 });
            break;
        case SDL_EVENT_MOUSE_MOTION:
            Clay_SetPointerState((Clay_Vector2) { event->motion.x, event->motion.y },
                                 event->motion.state & SDL_BUTTON_LMASK);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            Clay_SetPointerState((Clay_Vector2) { event->button.x, event->button.y },
                                 event->button.button == SDL_BUTTON_LEFT);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            Clay_UpdateScrollContainers(true, (Clay_Vector2) { event->wheel.x, event->wheel.y }, 0.01f);
            break;
        default:
            break;
    };

    return ret_val;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *state = appstate;

    Clay_RenderCommandArray render_commands = ClayVideoDemo_CreateLayout(&state->demoData);

    SDL_SetRenderDrawColor(state->rendererData.renderer, 0, 0, 0, 255);
    SDL_RenderClear(state->rendererData.renderer);

    SDL_Clay_RenderClayCommands(&state->rendererData, &render_commands);

    SDL_RenderPresent(state->rendererData.renderer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
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
