#include "Arduino.h"

#include "Adafruit_GFX.h"
#include "MCUFRIEND_kbv.h"

#include "constants.h"
#include "helper.h"
#include "arduino_secrets.h"

#include "touch.h"

#include "widgets/canvas.h"
#include "widgets/colorSelector.h"
#include "widgets/thicknessSelector.h"
#include "widgets/buttonGrid.h"
#include "widgets/button.h"

#include "compressor.h"

#include "canvasClient.h"


MCUFRIEND_kbv tft;

Canvas canvas(CANVAS_X, CANVAS_Y, CANVAS_W, CANVAS_H, &tft);
ColorSelector colorSelector(COLOR_SELECTOR_X, COLOR_SELECTOR_Y, &tft);
ThicknessSelector thicknessSelector(THICKNESS_SELECTOR_X, THICKNESS_SELECTOR_Y, &tft);
ButtonGrid slotSelector(SLOT_MENU_X, SLOT_MENU_Y, SLOT_MENU_W, SLOT_MENU_H, 2, 3, LOAD_W, LOAD_H, &tft);

Button loadButton(LOAD_X, LOAD_Y, LOAD_W, LOAD_H, LOAD_C, "LOAD", &tft);
Button saveButton(SAVE_X, SAVE_Y, SAVE_W, SAVE_H, SAVE_C, "SAVE", &tft);
Button clearButton(CLEAR_X, CLEAR_Y, CLEAR_W, CLEAR_H, CLEAR_C, "CLEAR", &tft);
Button infoButton(INFO_X, INFO_Y, INFO_W, INFO_H, INFO_C, "INFO", &tft);
Button connectButton(CONNECT_X, CONNECT_Y, CONNECT_W, CONNECT_H, CONNECT_C, "CONNECT", &tft);

CanvasClient client(&canvas);

GFXcanvas1 canvasBuffer(CANVAS_BUFFER_W, CANVAS_BUFFER_H);

Compressor<CANVAS_BUFFER_MAX_SEGMENTS> compressed[CANVAS_H - 2];
uint8_t rawCode[CANVAS_W - 2] {};

void clearCompressedCanvas();

void executeSlotSelection(CanvasClient *client, void (CanvasClient::*method)(uint8_t, Compressor<CANVAS_BUFFER_MAX_SEGMENTS> *));
void drawLowerWidgets();

void setup(void) {

    // Use Serial Monitor for debugging
    Serial.begin(9600);
    tft.begin(0x9486);

    client.setSSID(SECRET_SSID);
    client.setPass(SECRET_PASS);
    client.setServer(SERVER_IP, 5005);
    client.connect(8);

    tft.fillScreen(BLACK);

    canvas.draw();
    drawLowerWidgets();

    clearCompressedCanvas();
}

void loop(void) {

    unsigned widgetUpdateTime = 0;

    widgetUpdateTime = 0;

    unsigned x, y;
    getTouchCoors(&x, &y);

    {
        const uint16_t thickness = thicknessSelector.getThickness();
        const uint16_t color = colorSelector.getColor();
        const uint8_t code = color2code(color);

        canvas.setThickness(thickness);
        canvas.setColor(color);

        if (canvas.update(x, y)) {

            const uint16_t canvasX = x - CANVAS_X - 1;
            const uint16_t canvasY = y - CANVAS_Y - 1;

            const uint16_t row_lo = canvasY - thickness;
            const uint16_t row_hi = canvasY + thickness;

            const uint16_t col_lo = canvasX - thickness;
            const uint16_t col_hi = canvasX + thickness;

            canvasBuffer.fillRect(0, 0, CANVAS_BUFFER_W, CANVAS_BUFFER_H, 0);
            canvasBuffer.fillCircle(thickness, thickness, thickness, 1);

            for (unsigned i = row_lo, col_end; i <= row_hi; ++i) {

                if (compressed[i].getPrefixSize()-1 < col_lo) {
                    continue;
                }
                compressed[i].uncompress(rawCode);

                for (unsigned j = col_lo; j <= col_hi; ++j) {

                    if (canvasBuffer.getPixel(j - col_lo, i - row_lo)) {
                        rawCode[j] = code;
                        col_end = j;
                    }
                }

                compressed[i].compress(rawCode, col_end);
            }
        }
    }

    if (colorSelector.update(x, y)) {

        thicknessSelector.setColor(colorSelector.getColor());
        thicknessSelector.draw();
    }

    if (thicknessSelector.update(x, y)) {
        thicknessSelector.draw();
    }

    if (loadButton.update(x, y)) {

        slotSelector.draw();

        delay(500);

        for (;;) {

            getTouchCoors(&x, &y);

            if (slotSelector.update(x, y)) {

                executeSlotSelection(&client, &CanvasClient::loadCanvas);
                break;
            }

            delay(5);
        }
    }

    if (saveButton.update(x, y)) {

        slotSelector.draw();

        delay(500);

        for (;;) {

            getTouchCoors(&x, &y);

            if (slotSelector.update(x, y)) {

                executeSlotSelection(&client, &CanvasClient::saveCanvas);
                break;
            }

            delay(5);
        }
    }

    if (clearButton.update(x, y)) {
        canvas.clearDrawing();
        clearCompressedCanvas();
    }

    if (infoButton.update(x, y)) {

    }

    widgetUpdateTime = (micros() - widgetUpdateTime) / 1000;
    if (widgetUpdateTime < 10) { // 100 frames per second
        delay(10 - widgetUpdateTime);
    }
}

void clearCompressedCanvas() {
    memset(rawCode, 8, sizeof(rawCode));
    for (unsigned i = 0; i < CANVAS_H-2; ++i) {
        compressed[i].compress(rawCode, CANVAS_W-2);
    }
}

void executeSlotSelection(CanvasClient *client, void (CanvasClient::*method)(uint8_t, Compressor<CANVAS_BUFFER_MAX_SEGMENTS> *)) {

    uint8_t selectedSlot = slotSelector.getSlot();

    if (selectedSlot != 0) {
        (client->*method)(selectedSlot, compressed);
    }

    slotSelector.clear();
    drawLowerWidgets();
}

void drawLowerWidgets() {

    colorSelector.draw();
    thicknessSelector.draw();

    loadButton.draw();
    saveButton.draw();
    clearButton.draw();
    infoButton.draw();
    connectButton.draw();
}
