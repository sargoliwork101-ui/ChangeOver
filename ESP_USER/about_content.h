/**
 * @file about_content.h
 * @brief Editable About content, compiled into the ESP firmware.
 */
#pragma once
// Edit these values, then rebuild and flash ESP_USER. Keep the HTML tags.
static const char SIMPLE_ABOUT_HTML[] PROGMEM = R"about(
<p>تماس با ما: <bdi>09367969890</bdi></p>
<p>آدرس سایت: <bdi></bdi></p>
<p>آدرس دفتر نمایندگی: <span>تهران-تهران</span></p>
)about";
