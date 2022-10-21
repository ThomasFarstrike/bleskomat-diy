#include "screen/tft.h"

#include <JPEGDecoder.h>

// Return the minimum of two values a and b
#define minimum(a,b)     (((a) < (b)) ? (a) : (b))

#include "screen/bitcoin-logo.jpg.h"

namespace {

	TFT_eSPI display = TFT_eSPI();
	const auto bg_color = TFT_WHITE;
	const auto text_color = TFT_BLACK;
	const uint8_t text_font = 4;
	const uint8_t text_size = 1;
	const uint8_t margin_x = 3;
	const uint8_t margin_y = 6;
	std::string current_screen = "";

	struct BoundingBox {
		int16_t x = 0;
		int16_t y = 0;
		uint16_t w = 0;
		uint16_t h = 0;
	};

	BoundingBox amount_text_bbox;

	std::string getAmountFiatCurrencyString(const float &amount) {
		return util::floatToStringWithPrecision(amount, config::getUnsignedShort("fiatPrecision")) + " " + config::getString("fiatCurrency");
	}

	BoundingBox renderText(
		const std::string &t_text,
		const int16_t &x,
		const int16_t &y,
		const bool &center = true
	) {
		const char* text = t_text.c_str();
		display.setTextColor(text_color);
		display.setTextFont(text_font);
		display.setTextSize(text_size);
		int16_t tbw = display.textWidth(text);
		int16_t tbh = display.fontHeight(); // no need to multiply by text_size because TFT_eSPI does this automatically after setTextSize() is called
		int16_t box_x = x;
		int16_t box_y = y;
		if (center) {
			box_x -= (tbw / 2);
		}
		int16_t cursor_x = box_x;
		int16_t cursor_y = box_y;
		display.setCursor(cursor_x, cursor_y);
		display.println(text);
		BoundingBox bbox;
		bbox.x = box_x;
		bbox.y = box_y;
		bbox.w = tbw;
		bbox.h = tbh;
		return bbox;
	}

	BoundingBox renderQRCode(
		const std::string &t_data,
		const int16_t &x,
		const int16_t &y,
		const uint16_t &max_w,
		const uint16_t &max_h,
		const bool &center = true
	) {
		Serial.println("renderQRCode x,y,max_w,max_h: " + String(x) + "," + String(y) + "," + String(max_w) + "," + String(max_h)); // renderQRCode x,y,max_w,max_h: 120,32,240,208
		BoundingBox bbox;
		try {
			const char* data = t_data.c_str();
			uint8_t version = 1;
			while (version <= 40) {
				const uint16_t bufferSize = qrcode_getBufferSize(version);
				QRCode qrcode;
				uint8_t qrcodeData[bufferSize];
				const int8_t result = qrcode_initText(&qrcode, qrcodeData, version, ECC_LOW, data);
				if (result == 0) {
					// QR encoding successful.
					Serial.println("qrcode.size: " + String(qrcode.size));	// qrcode.size: around 49-53, depending on the URL size, which depends on the amount etc...
					uint8_t scale = std::min(std::floor(max_w / qrcode.size), std::floor(max_h / qrcode.size));
					uint16_t w = qrcode.size * scale;
					uint16_t h = w;
					int16_t box_x = x;
					int16_t box_y = y;
					if (center) {
						box_x -= (w / 2);
					}

					// center box vertically on the remaining space
					box_y += (max_h - h)/ 2; // max_height - actual_height / 2
					//Serial.println("box_y after vertically centering on remaining space = " + String(box_y));

					display.fillRect(box_x, box_y, w, h, bg_color);
					for (uint8_t y = 0; y < qrcode.size; y++) {
						for (uint8_t x = 0; x < qrcode.size; x++) {
							auto color = qrcode_getModule(&qrcode, x, y) ? text_color: bg_color;
							display.fillRect(box_x + scale*x, box_y + scale*y, scale, scale, color);
						}
					}
					bbox.x = box_x;
					bbox.y = box_y;
					bbox.w = w;
					bbox.h = h;
					break;
				} else if (result == -2) {
					// Data was too long for the QR code version.
					version++;
				} else if (result == -1) {
					throw std::runtime_error("Render QR code failure: Unable to detect mode");
				} else {
					throw std::runtime_error("Render QR code failure: Unknown failure case");
				}
			}
		} catch (const std::exception &e) {
			std::cerr << e.what() << std::endl;
		}
		return bbox;
	}

	void clearScreen() {
		display.fillScreen(bg_color);
	}

	//####################################################################################################
	// Draw a JPEG on the TFT, images will be cropped on the right/bottom sides if they do not fit
	//####################################################################################################
	// This function assumes xpos,ypos is a valid screen coordinate. For convenience images that do not
	// fit totally on the screen are cropped to the nearest MCU size and may leave right/bottom borders.
	void renderJPEG(int xpos, int ypos) {

		// retrieve infomration about the image
		uint16_t *pImg;
		uint16_t mcu_w = JpegDec.MCUWidth;
		uint16_t mcu_h = JpegDec.MCUHeight;
		uint32_t max_x = JpegDec.width;
		uint32_t max_y = JpegDec.height;

		// Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
		// Typically these MCUs are 16x16 pixel blocks
		// Determine the width and height of the right and bottom edge image blocks
		uint32_t min_w = minimum(mcu_w, max_x % mcu_w);
		uint32_t min_h = minimum(mcu_h, max_y % mcu_h);

		// save the current image block size
		uint32_t win_w = mcu_w;
		uint32_t win_h = mcu_h;

		// record the current time so we can measure how long it takes to draw an image
		uint32_t drawTime = millis();

		// save the coordinate of the right and bottom edges to assist image cropping
		// to the screen size
		max_x += xpos;
		max_y += ypos;

		// read each MCU block until there are no more
		while (JpegDec.readSwappedBytes()) {
		// save a pointer to the image block
		pImg = JpegDec.pImage ;

		// calculate where the image block should be drawn on the screen
		int mcu_x = JpegDec.MCUx * mcu_w + xpos;  // Calculate coordinates of top left corner of current MCU
		int mcu_y = JpegDec.MCUy * mcu_h + ypos;

		// check if the image block size needs to be changed for the right edge
		if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
		else win_w = min_w;

		// check if the image block size needs to be changed for the bottom edge
		if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
		else win_h = min_h;

		// copy pixels into a contiguous block
		if (win_w != mcu_w)
		{
		uint16_t *cImg;
		int p = 0;
		cImg = pImg + win_w;
		for (int h = 1; h < win_h; h++)
		{
		p += mcu_w;
		for (int w = 0; w < win_w; w++)
		{
		  *cImg = *(pImg + w + p);
		  cImg++;
		}
		}
		}

		// draw image MCU block only if it will fit on the screen
		if (( mcu_x + win_w ) <= display.width() && ( mcu_y + win_h ) <= display.height())
		{
		display.pushRect(mcu_x, mcu_y, win_w, win_h, pImg);
		}
		else if ( (mcu_y + win_h) >= display.height()) JpegDec.abort(); // Image has run off bottom of screen so abort decoding
		}

		// calculate how long it took to draw the image
		drawTime = millis() - drawTime;

		// print the results to the serial port
		Serial.print(F(  "Total render time was    : ")); Serial.print(drawTime); Serial.println(F(" ms"));
		Serial.println(F(""));
	}


	//####################################################################################################
	// Print image information to the serial port (optional)
	//####################################################################################################
	void jpegInfo() {
		Serial.println(F("==============="));
		Serial.println(F("JPEG image info"));
		Serial.println(F("==============="));
		Serial.print(F(  "Width      :")); Serial.println(JpegDec.width);
		Serial.print(F(  "Height     :")); Serial.println(JpegDec.height);
		Serial.print(F(  "Components :")); Serial.println(JpegDec.comps);
		Serial.print(F(  "MCU / row  :")); Serial.println(JpegDec.MCUSPerRow);
		Serial.print(F(  "MCU / col  :")); Serial.println(JpegDec.MCUSPerCol);
		Serial.print(F(  "Scan type  :")); Serial.println(JpegDec.scanType);
		Serial.print(F(  "MCU width  :")); Serial.println(JpegDec.MCUWidth);
		Serial.print(F(  "MCU height :")); Serial.println(JpegDec.MCUHeight);
		Serial.println(F("==============="));
	}

	//####################################################################################################
	// Draw a JPEG on the TFT pulled from a program memory array
	//####################################################################################################
	void drawArrayJpeg(const uint8_t arrayname[], uint32_t array_size, int xpos, int ypos) {

		JpegDec.decodeArray(arrayname, array_size);

		jpegInfo(); // Print information from the JPEG file (could comment this line out)

		// center x?
		if (xpos == -1) {
			xpos = (display.width() - JpegDec.width) / 2;
		}

		renderJPEG(xpos, ypos);

		Serial.println("#########################");
	}


}

namespace screen_tft {

	void init() {
		logger::write("Initializing TFT display...");
		display.begin();
		display.setRotation(config::getUnsignedShort("tftRotation"));
		clearScreen();
	}

	void showInsertFiatScreen(const float &amount) {
		if (current_screen == "insertFiat") {
			// Clear previous text by drawing a rectangle over it.
			display.fillRect(
				amount_text_bbox.x,
				amount_text_bbox.y,
				amount_text_bbox.w,
				amount_text_bbox.h,
				bg_color
			);
		} else if (current_screen == "tradeComplete") {
			// Clear the whole screen.
			clearScreen();
		}
		const std::string text = getAmountFiatCurrencyString(amount);
		const int16_t center_x = display.width() / 2;
		const int16_t text_x = center_x;
		const int16_t text_y = margin_y;
		amount_text_bbox = renderText(text, text_x, text_y, true/* center */);
		current_screen = "insertFiat";

		// draw logo below the text:
		drawArrayJpeg(logo_jpg, sizeof(logo_jpg), -1, amount_text_bbox.y + amount_text_bbox.h); // -1 means center x
	}

	void showTradeCompleteScreen(const float &amount, const std::string &qrcodeData) {
		clearScreen();
		const std::string text = getAmountFiatCurrencyString(amount);
		const int16_t center_x = display.width() / 2;
		const int16_t text_x = center_x;
		const int16_t text_y = 1; // minimal margin above text so there is maximal margin for the QR code
		amount_text_bbox = renderText(text, text_x, text_y, true/* center */);
		const int16_t qr_x = center_x;
		const int16_t qr_y = amount_text_bbox.y + amount_text_bbox.h;
		const int16_t qr_max_w = display.width();
		const int16_t qr_max_h = display.height() - qr_y;
		renderQRCode(qrcodeData, qr_x, qr_y, qr_max_w, qr_max_h, true/* center */);
		current_screen = "tradeComplete";
	}
}
