// ConvertUtils.hpp
#pragma once

#include <string>
#include <map>
#include <sstream>
#include <vector>
#include <iomanip>
#include "ConsoleManager.hpp"


// Presupunem ca ColorRgb este definita in alta parte (de exemplu, in Style.hpp sau un Types.hpp)
class ColorRgb {
public:
    double r, g, b, a;
    ColorRgb() : r(-1.), g(-1.), b(-1.), a(-1.) {}

    ColorRgb(double red, double green, double blue, double alpha = 1.0)
        : r(red), g(green), b(blue), a(alpha) {}

    ColorRgb(const ColorRgb& other) = default;
    ColorRgb& operator=(const ColorRgb & other) = default;

    void print() const {
        std::wstringstream ss;
        ss << std::fixed << std::setprecision(3);
        ss << L"ColorRgb(r=" << r
            << L", g=" << g
            << L", b=" << b
            << L", a=" << a << L")";

        LOG_DEBUG(ss.str());


    }

};

class ConvertUtils {
private:
    // Utilitar privat pentru parsarea Shorthand-urilor (margin/padding)
    // Va fi folosit de parseBoxShorthand, dar nu e vizibil in exterior.
    static double parseSingleLength(const std::wstring& value, double referenceSize);

public:

    static double convertCssLengthToPt(const std::wstring& cssValue);

    // ==========================================================
    // 1. CONVERSIE LUNGIME (Dependente de Font Size)
    // ==========================================================
    /**
     * @brief Convertește Twips (unitatea de bază RTF: 1/20 dintr-un punct) în Puncte (Pt).
     * 1 punct (pt) = 20 twips.
     * @param twips Lungimea în twips (int).
     * @return Lungimea în puncte (Pt) (double).
     */
    static double twipsToPoints(int twips) {
        // Conversia standard: Twips / 20.0
        return static_cast<double>(twips) / 20.0;
    }

    /**
     * @brief Convertește CSS Length (px, mm, in, em, pt) în puncte (Pt).
     * @param cssValue Valoarea CSS (ex: "10mm", "1.5em").
     * @param currentFontSize Dimensiunea fontului elementului CURENT (pentru em).
     * @return Lungimea în puncte (Pt).
     */
    static double convertCssLengthToPt(const std::wstring& cssValue, double currentFontSize);

    /**
     * @brief Convertește CSS Length pentru proprietatea font-size.
     * @param cssValue Valoarea CSS (ex: "16px", "1.2em", "150%").
     * @param parentFontSize Dimensiunea fontului elementului PĂRINTE (pentru em/%).
     * @return Dimensiunea fontului în puncte (Pt).
     */
    static double convertCssFontLengthToPt(const std::wstring& cssValue, double parentFontSize);


    // ==========================================================
    // 2. CONVERSIE CULOARE
    // ==========================================================

    /**
     * @brief Convertește un șir de culoare CSS (hex, rgb, nume) în ColorRgb.
     * @param css_color_val Valoarea CSS (ex: "#FF0000", "black", "rgb(255, 0, 0)").
     * @return Structura ColorRgb.
     */
    static ColorRgb parseCssColorToRgb(const std::wstring& css_color_val);

    // ==========================================================
    // 3. PARSARE SHORTHAND-URI (Box Model)
    // ==========================================================

    /**
     * @brief Parsează proprietățile de tip Box (margin sau padding) pe baza a 1, 2, 3 sau 4 valori.
     * @param shorthandValue Valoarea CSS (ex: "10px", "10px 20px", "5px 10px 15px 20px").
     * @param currentFontSize Dimensiunea fontului (pentru conversia unităților relative).
     * @param top, right, bottom, left Referințe la valorile din BoxModel.
     */
    static void parseBoxShorthand(const std::wstring& shorthandValue,
        double currentFontSize,
        double& top, double& right, double& bottom, double& left);

    /**
     * @brief Parsează shorthand-ul pentru bordură (ex: "1px solid black").
     * @param value Valoarea CSS (ex: "1px solid black").
     * @param style, width, color Referințe la proprietățile din Style.
     */
    static void parseBorderShorthand(const std::wstring& value,
        std::wstring& style, double& width, ColorRgb& color,
        double currentFontSize); // Necesită font size pentru lățime
    
    static std::vector<std::wstring> tokenizeText(const std::wstring& text);
    
};