/*
 *
 *
 *         _             ______
     /\   | |           |  ____|
    /  \  | | _____  __ | |__ ___  _ __ ___ _   _
   / /\ \ | |/ _ \ \/ / |  __/ _ \| '__/ _ \ | | |
  / ____ \| |  __/>  <  | | | (_) | | |  __/ |_| |
 /_/    \_\_|\___/_/\_\ |_|  \___/|_|  \___|\__, |
                                             __/ |
 Glowing Square: Unifi Display              |___/
 For ESP32
 unifi.h
 *
 */

// How tall we want the graph on the display to be
#define GRAPH_HEIGHT 36

// Keep track of any failed web requests
int failed_attempts = 0;

void drawGraph(JsonDocument &json, int bottomRowHeight) {
    // Draw the graph of network activity
    // Loop along the x axis of the graph
    JsonArray points = json["graph"].as<JsonArray>();
    bool download = true;
    int x = 2;
    display.drawFastHLine(0, MATRIX_HEIGHT - 1 - bottomRowHeight, MATRIX_WIDTH, white);
    for (JsonVariant point : points) {
        int pval = point.as<int>();
        uint16_t color = download ? green : blue;

        for (int y = 0; y < pval; y++) {
            display.drawPixel(x, MATRIX_HEIGHT - 1 - y - bottomRowHeight - 1, color);
            display.drawPixel(x + 1, MATRIX_HEIGHT - 1 - y - bottomRowHeight - 1, color);
        }

        x += download ? 2 : 3;
        download = !download;
    }
}

void drawGraphStats(JsonDocument &json, boolean max) {
    int16_t  x1, y1;
    uint16_t w, h;

    const int letterWidth = 4;
    const int arrowWidth = 5;
    const int titleWidth = (max ? 3 * letterWidth : 4 * letterWidth) + 2;
    const int y = MATRIX_HEIGHT - 6;
    String rx = json[max ? "max_rx": "curr_rx"].as<String>();
    String tx = json[max ? "max_tx": "curr_tx"].as<String>();

    display.fillRect(0, y, MATRIX_WIDTH, 6, black);
    display.setFont(&TomThumb);
    display.setCursor(0, MATRIX_HEIGHT);
    display.setTextColor(white);
    display.print(max ? "MAX" : "CURR");
    display.print(":");
    display.setCursor(titleWidth + arrowWidth + 1, MATRIX_HEIGHT);
    display.setTextColor(green);
    display.print(rx);
    display.getTextBounds(rx, titleWidth + arrowWidth + 1, MATRIX_HEIGHT, &x1, &y1, &w, &h);
    display.setCursor(titleWidth + 2 * (arrowWidth + 1) + w, MATRIX_HEIGHT);
    display.setTextColor(blue);
    display.print(tx);

    drawIcon(titleWidth, y + 1, arrowWidth, arrowWidth, down_icon_sm, green_palette);
    drawIcon(titleWidth + arrowWidth + 1 + w, y + 1, arrowWidth, arrowWidth, up_icon_sm, blue_palette);

    display.setFont();
}

void drawClock() {
    int clockX = MATRIX_WIDTH - 7 * 4 + 2;
    int hours = timeClient.getHours();
    bool pm = hours >= 12;

    if (pm) {
        hours -= 12;
    }
    if (hours == 0) {
        hours = 12;
    }

    display.setFont(&TomThumb);
    display.setTextColor(white);

    if (hours < 10) {
        display.setCursor(clockX + 3, MATRIX_HEIGHT);
    } else {
        display.setCursor(clockX, MATRIX_HEIGHT);
    }
    display.print(hours);

    display.setCursor(clockX + 3 + 4, MATRIX_HEIGHT);
    display.print(":");

    if (timeClient.getMinutes() < 10) {
        display.print("0");
    }
    display.print(timeClient.getMinutes());

    display.setCursor(MATRIX_WIDTH - 7, MATRIX_HEIGHT);
    display.print(pm ? "PM" : "AM");
    display.setFont();
}

// Function that runs from loop() every 30 seconds
boolean downloadAndDisplayNetworkInfo() {

    Serial.print("Fetching info... ");

    HTTPClient http;

    // This is the API endpoint that we fetch new departures for our station from
    char requestURL[256];
    sprintf(requestURL, "%s?width=%i&height=%i", script_url, 2 * MATRIX_WIDTH / 5, GRAPH_HEIGHT);

    // Fetch the data from the server
    http.begin(requestURL);
    int httpCode = http.GET();

    // Check that the server gave a valid response
    if (httpCode != 200) {
        Serial.printf("Failed to get info with HTTP Code %i\n", httpCode); // Code -11 means the request timed out

        // Mark us as offline so the little icon will be drawn next time
        failed_attempts++;
        return false;

    } else {
        // All is good
        failed_attempts = 0;
    }

    // Fetch and parse the web request
    String payload = http.getString();

    StaticJsonDocument<3072> json;
    deserializeJson(json, payload);

    // Start writing to the display
    display.clearDisplay();

    // Draw header
    drawIcon(0, 0, MATRIX_WIDTH, 11, banner, logo_palette_default);

    const int rowHeight = 8;
    const int firstRow = 12;
    const int secondRow = firstRow + 8;
    const int thirdRow = secondRow + 8;
    const int dataRow = 12;
    const int bottomRowHeight = 6;

    const int charWidth = 6;


    // Draw the download icon and stats
    drawIcon(0, dataRow, 5, 7, down_icon, green_palette);
    display.setCursor(6, dataRow);
    display.setTextColor(green);
    display.print(json["month_rx"].as<String>());

    // Draw the upload icon and stats
    drawIcon(6 * charWidth, dataRow, 5, 7, up_icon, blue_palette);
    display.setCursor(7 * charWidth, dataRow);
    display.setTextColor(blue);
    display.print(json["month_tx"].as<String>());

    // Draw wireless client count
    drawIcon(MATRIX_WIDTH - 7, dataRow, 7, 7, client_icon, white_palette);
    display.setTextColor(white);
    drawRightAlignedText(MATRIX_WIDTH - 7, dataRow, json["wireless"].as<String>());

    int wirelessWidth = 7 + charWidth * (json["wireless"].as<String>().length() + 1);

    // Draw wired client count
    drawIcon(MATRIX_WIDTH - 7 - wirelessWidth, dataRow, 7, 7, wired_icon, white_palette);
    display.setTextColor(white);
    drawRightAlignedText(MATRIX_WIDTH - 7 - wirelessWidth, dataRow, json["wired"].as<String>());

    drawGraph(json, bottomRowHeight);


    const long animationStart = millis();
    long now = animationStart;
    while (now - animationStart < INFO_UPDATE_INTERVAL) {
        ArduinoOTA.handle();    // In case we want to upload a new sketch

        drawGraphStats(json, now / 6000 % 2 == 0);
        drawClock();

        // draw the last connected device
        if (json["min_uptime"] < (INFO_UPDATE_INTERVAL * 4 / 1000)) {
            display.drawFastHLine(0, secondRow, MATRIX_WIDTH, red);
            display.drawFastHLine(0, secondRow + rowHeight + 2, MATRIX_WIDTH, red);
            display.fillRect(0, secondRow + 1, MATRIX_WIDTH, rowHeight + 1, black);
            drawStaticAndScrollingText(secondRow + 2, animationStart, now, "New:", red, json["newest"].as<String>(), white);
        }

        // Draw the updates to the display
        display.showBuffer();

        // Copy the updates to the second buffer to avoid flashing
        display.copyBuffer();

        // Pause so we can set the speed of the animation
        delay(SCROLL_DELAY);
        yield();
        now = millis();
    }

    display.showBuffer();

    Serial.printf("%s down %s up\n", json["month_rx"].as<String>(), json["month_tx"].as<String>());

}
