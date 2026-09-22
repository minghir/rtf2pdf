#include "style.hpp"
#include "ConsoleManager.hpp"
#include "stringUtils.hpp"

#include <algorithm>
#include <iomanip>


// Presupunem că această funcție este membrul clasei Style
void Style::setUpStyle(const std::map<std::wstring, std::wstring>& cssProperties) {

    // NOTĂ: Dacă folosești această funcție pentru a aplica stiluri inline,
    // este esențial să NU modifici fontSize (Pasul 1) înainte de a parsa
    // marginile/padding-ul (Pasul 2) dacă acelea folosesc 'em' bazat pe fontSize-ul MOȘTENIT.
    // Dar, pentru simplitate și ca să folosim un singur loop, o lăsăm așa.

    // Font size-ul CURENT (pe care îl modificăm) este referința pentru 'em'
    double current_font_size = this->fontSize;

    for (const auto& [name, value] : cssProperties) {

        // ==========================================================
        // 1. Proprietăți care NU depind de fontSize (Culori, Simple Strings)
        // ==========================================================

        if (name == L"rule-name") {
            ruleName = value;
        }
        else if (name == L"background-color") {
            backgroundColor = ConvertUtils::parseCssColorToRgb(value);
        }
        else if (name == L"color") {
            textColor = ConvertUtils::parseCssColorToRgb(value);
        }
        else if (name == L"border-color") {
            borderColor = ConvertUtils::parseCssColorToRgb(value);
        }
        else if (name == L"border-style") {
            borderStyle = value;
        }
        else if (name == L"font-family") {
            fontFamily = value;
        }
        else if (name == L"font-weight") {
            fontWeight = value;
        }
        else if (name == L"font-style") {
            fontStyle = value;
        }
        else if (name == L"text-align") {
            textAlign = value;
        }
        else if (name == L"vertical-align") {
            verticalAlign = value;
        }
        else if (name == L"text-decoration") {
            textDecoration = value;
        }
        else if (name == L"display") {
            display = value;
        }
        else if (name == L"position") {
            position = value;
        }
        else if (name == L"line-height") {
            // Line-height poate fi un număr (factor) sau o lungime, deci folosim funcția FontLength
            double val_pt = ConvertUtils::convertCssFontLengthToPt(value, current_font_size);
            if (current_font_size > 0.0) {
                lineHeight = val_pt / current_font_size;
            }
            if (lineHeight < 0.1) lineHeight = 1.2;
        }

        // ==========================================================
        // 2. Proprietăți care depind de fontSize (Lungimi)
        // ==========================================================

        // NOTA: Datorita dependentei, ar trebui sa parsam font-size primul.
        else if (name == L"font-size") {
            // Aici nu avem parentFontSize, deci folosim fontSize-ul curent ca bază pentru em-uri,
            // deși ar fi mai bine să folosim o valoare default (ex: 12.0)
            this->fontSize = ConvertUtils::convertCssFontLengthToPt(value, current_font_size);
            // Actualizăm referința (deși e cam târziu pentru proprietățile deja parsatate)
            current_font_size = this->fontSize;
        }

        // --- Shorthands ---
        else if (name == L"border") {
            // Folosim membrii clasei Style ca referințe
            ConvertUtils::parseBorderShorthand(value,
                this->borderStyle, this->boxModel.borderTopWidth, this->borderColor, current_font_size);

            // Aplicăm lățimea pe toate laturile (deși e incorect, este necesar pentru shorthand)
            this->boxModel.borderRightWidth = this->boxModel.borderBottomWidth = this->boxModel.borderLeftWidth = this->boxModel.borderTopWidth;
        }
        else if (name == L"padding") {
            ConvertUtils::parseBoxShorthand(value, current_font_size,
                boxModel.paddingTop, boxModel.paddingRight, boxModel.paddingBottom, boxModel.paddingLeft);
        }
        else if (name == L"margin") {
            ConvertUtils::parseBoxShorthand(value, current_font_size,
                boxModel.marginTop, boxModel.marginRight, boxModel.marginBottom, boxModel.marginLeft);
        }

        // --- Lungimi Individuale ---
        else if (name == L"width") {
            width = ConvertUtils::convertCssLengthToPt(value, current_font_size);
        }
        else if (name == L"height") {
            height = ConvertUtils::convertCssLengthToPt(value, current_font_size);
        }
        else if (name == L"max-width") {
            maxWidth = ConvertUtils::convertCssLengthToPt(value, current_font_size);
        }
        else if (name == L"min-height") {
            minHeight = ConvertUtils::convertCssLengthToPt(value, current_font_size);
        }
        else if (name == L"letter-spacing") {
            letterSpacing = ConvertUtils::convertCssLengthToPt(value, current_font_size);
        }
        else if (name == L"word-spacing") {
            wordSpacing = ConvertUtils::convertCssLengthToPt(value, current_font_size);
        }
        else if (name == L"top") {
            top = ConvertUtils::convertCssLengthToPt(value, current_font_size);
        }
        else if (name == L"left") {
            left = ConvertUtils::convertCssLengthToPt(value, current_font_size);
        }
        else if (name == L"right") {
            right = ConvertUtils::convertCssLengthToPt(value, current_font_size);
        }
        else if (name == L"bottom") {
            bottom = ConvertUtils::convertCssLengthToPt(value, current_font_size);
        }
        // ... Proprietățile Box Model individuale (margin-top, border-top-width, etc.) ar trebui să fie aici...
    }
}

/*
void Style::print() const {
    ConsoleManager& console = ConsoleManager::getInstance();
    console.setColor(FOREGROUND_INTENSITY | FOREGROUND_GREEN);
    std::wcout << L"  ┌─ Stil vizual ───────────────────────────────\n";
    std::wcout << L"  │ rule-name:     " << ruleName << L"\n";
    console.setColor(FOREGROUND_INTENSITY | FOREGROUND_BLUE);
    std::wcout << L"  │ background-color: (" << backgroundColor.r << L", " << backgroundColor.g << L", " << backgroundColor.b << L")\n";
    std::wcout << L"  │ text-color:       (" << textColor.r << L", " << textColor.g << L", " << textColor.b << L")\n";
    std::wcout << L"  │ border-color:     (" << borderColor.r << L", " << borderColor.g << L", " << borderColor.b << L")\n";
    std::wcout << L"  │ border-style:     " << borderStyle << L"\n";

    std::wcout << L"  ├─ Dimensiuni & Spațiere ─────────────────────\n";
    std::wcout << L"  │ width: " << width << L", height: " << height << L"\n";
    std::wcout << L"  │ max-width: " << maxWidth << L", min-height: " << minHeight << L"\n";
    std::wcout << L"  │ margin: top=" << boxModel.marginTop << L", right=" << boxModel.marginRight
        << L", bottom=" << boxModel.marginBottom << L", left=" << boxModel.marginLeft << L"\n";
    std::wcout << L"  │ padding: top=" << boxModel.paddingTop << L", right=" << boxModel.paddingRight
        << L", bottom=" << boxModel.paddingBottom << L", left=" << boxModel.paddingLeft << L"\n";

    std::wcout << L"  ├─ Poziționare & Flow ────────────────────────\n";
    std::wcout << L"  │ display: " << display << L", position: " << position << L"\n";
    std::wcout << L"  │ top: " << top << L", left: " << left << L", right: " << right << L", bottom: " << bottom << L"\n";
    std::wcout << L"  │ text-align: " << textAlign << L", vertical-align: " << verticalAlign << L"\n";

    std::wcout << L"  ├─ Font & Text ───────────────────────────────\n";
    std::wcout << L"  │ font-family: " << fontFamily << L", font-size: " << fontSize << L"\n";
    std::wcout << L"  │ font-weight: " << fontWeight << L", font-style: " << fontStyle << L"\n";
    std::wcout << L"  │ line-height: " << lineHeight << L", letter-spacing: " << letterSpacing << L", word-spacing: " << wordSpacing << L"\n";
    std::wcout << L"  │ text-decoration: " << textDecoration << L"\n";

    console.setColor(FOREGROUND_INTENSITY | FOREGROUND_GREEN);
    std::wcout << L"  └─────────────────────────────────────────────\n";
    console.resetColor();
}
*/

void Style::print() const {
    // Culori predefinite pentru lizibilitate
    const WORD colorFrame = FOREGROUND_INTENSITY | FOREGROUND_GREEN;
    const WORD colorData = FOREGROUND_INTENSITY | FOREGROUND_BLUE;

    // 1. Header Stil vizual
    LOG_RAW(L"  ┌─ Stil vizual ───────────────────────────────", colorFrame);
    LOG_RAW(L"  │ rule-name:      " + ruleName, colorFrame);

    // 2. Culori (Blue Intensity)
    LOG_RAW(L"  │ background-color: (" + std::to_wstring(backgroundColor.r) + L", " +
        std::to_wstring(backgroundColor.g) + L", " + std::to_wstring(backgroundColor.b) + L")", colorData);
    LOG_RAW(L"  │ text-color:       (" + std::to_wstring(textColor.r) + L", " +
        std::to_wstring(textColor.g) + L", " + std::to_wstring(textColor.b) + L")", colorData);
    LOG_RAW(L"  │ border-color:     (" + std::to_wstring(borderColor.r) + L", " +
        std::to_wstring(borderColor.g) + L", " + std::to_wstring(borderColor.b) + L")", colorData);
    LOG_RAW(L"  │ border-style:     " + borderStyle, colorData);

    // 3. Dimensiuni & Spațiere
    LOG_RAW(L"  ├─ Dimensiuni & Spațiere ─────────────────────", colorFrame);
    LOG_RAW(L"  │ width: " + std::to_wstring(width) + L", height: " + std::to_wstring(height), colorData);
    LOG_RAW(L"  │ max-width: " + std::to_wstring(maxWidth) + L", min-height: " + std::to_wstring(minHeight), colorData);

    // Margin & Padding (construim string-uri pentru concatenare curată)
    std::wstring marginStr = L"  │ margin: top=" + std::to_wstring(boxModel.marginTop) + L", right=" + std::to_wstring(boxModel.marginRight) +
        L", bottom=" + std::to_wstring(boxModel.marginBottom) + L", left=" + std::to_wstring(boxModel.marginLeft);
    LOG_RAW(marginStr, colorData);

    std::wstring paddingStr = L"  │ padding: top=" + std::to_wstring(boxModel.paddingTop) + L", right=" + std::to_wstring(boxModel.paddingRight) +
        L", bottom=" + std::to_wstring(boxModel.paddingBottom) + L", left=" + std::to_wstring(boxModel.paddingLeft);
    LOG_RAW(paddingStr, colorData);

    // 4. Poziționare & Flow
    LOG_RAW(L"  ├─ Poziționare & Flow ────────────────────────", colorFrame);
    LOG_RAW(L"  │ display: " + display + L", position: " + position, colorData);

    std::wstring posStr = L"  │ top: " + std::to_wstring(top) + L", left: " + std::to_wstring(left) +
        L", right: " + std::to_wstring(right) + L", bottom: " + std::to_wstring(bottom);
    LOG_RAW(posStr, colorData);

    LOG_RAW(L"  │ text-align: " + textAlign + L", vertical-align: " + verticalAlign, colorData);

    // 5. Font & Text
    LOG_RAW(L"  ├─ Font & Text ───────────────────────────────", colorFrame);
    LOG_RAW(L"  │ font-family: " + fontFamily + L", font-size: " + std::to_wstring(fontSize), colorData);
    LOG_RAW(L"  │ font-weight: " + fontWeight + L", font-style: " + fontStyle, colorData);

    std::wstring textSpecs = L"  │ line-height: " + std::to_wstring(lineHeight) + L", letter-spacing: " +
        std::to_wstring(letterSpacing) + L", word-spacing: " + std::to_wstring(wordSpacing);
    LOG_RAW(textSpecs, colorData);

    LOG_RAW(L"  │ text-decoration: " + textDecoration, colorData);

    // 6. Footer
    LOG_RAW(L"  └─────────────────────────────────────────────", colorFrame);
}