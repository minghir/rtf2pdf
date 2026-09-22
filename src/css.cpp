#include "css.hpp"
#include "ConsoleManager.hpp"
#include "stringUtils.hpp"

// Presupunând că ai definit global sau ca membru al unei clase:
// std::wstring str_to_wstr(const std::string& s); 

// --- Implementare Utilitară ---

std::wstring CssDefinition::trim(const std::wstring & str) {
    const wchar_t* whitespace = L" \t\n\r";
    size_t start = str.find_first_not_of(whitespace);
    if (std::wstring::npos == start) {
        return L""; // Gol sau doar spații albe
    }
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, (end - start) + 1);
}

// --- Implementare Manager ---

void CssDefinition::addRule(const CssRule& rule) {
    m_rules.push_back(rule);
}

void CssDefinition::setProperty(const std::wstring& selector,
    const std::wstring& propertyName,
    const std::wstring& propertyValue) {

    // Caută regula existentă
    auto it = std::find_if(m_rules.begin(), m_rules.end(),
        [&selector](const CssRule& r) { return r.selector == selector; });

    // Dacă nu există, o creează
    if (it == m_rules.end()) {
        m_rules.emplace_back(selector);
        it = std::prev(m_rules.end());
    }

    // Adaugă/actualizează proprietatea
    it->properties[trim(propertyName)] = trim(propertyValue);
}

std::wstring CssDefinition::getPropertyValue(const std::wstring& selector,
    const std::wstring& propertyName) const {

    // 1. Curățăm selectorul primit pentru o căutare exactă
    std::wstring trimmedSelector = wstr_trim(selector);

    auto it = std::find_if(m_rules.begin(), m_rules.end(),
        [&trimmedSelector](const CssRule& r) { return r.selector == trimmedSelector; });

    if (it != m_rules.end()) {
        // 2. Curățăm numele proprietății primite înainte de a căuta în map (pentru potrivire exactă)
        std::wstring trimmedPropertyName = wstr_trim(propertyName);

        auto propIt = it->properties.find(trimmedPropertyName);
        if (propIt != it->properties.end()) {
            return propIt->second;
        }
    }
    return L""; // Valoare goală dacă nu este găsită
}


std::wstring CssDefinition::getAllRulesAsString() const {
    std::wstringstream ss;
    for (const auto& rule : m_rules) {
        ss << rule.toString() << L"\n";
    }
    return ss.str();
}


std::vector<std::wstring> CssDefinition::splitSelectors(const std::wstring& compositeSelector) {
    std::vector<std::wstring> selectors;
    std::wstringstream ss(compositeSelector);
    std::wstring segment;

    while (std::getline(ss, segment, L',')) {
        // Adaugă fiecare selector după ce a fost curățat de spații
        std::wstring cleaned = trim(segment);
        if (!cleaned.empty()) {
            selectors.push_back(cleaned);
        }
    }
    return selectors;
}


// În CssDefinition.cpp

bool CssDefinition::parseCssContent(const std::wstring& cssText) {
    if (cssText.empty()) {
        LOG_INFO(L"[CSS Parser] Conținutul CSS este gol. Se omite parsarea.");
        return true;
    }

    m_rules.clear();

    size_t pos = 0;

    while (pos < cssText.length()) {

        size_t selectorStart = cssText.find_first_not_of(L" \t\n\r", pos);
        if (selectorStart == std::wstring::npos) break;


        // --- Parsare regulă @page ---
        if (cssText[selectorStart] == L'@') {
            size_t braceOpen = cssText.find(L'{', selectorStart);
            size_t braceClose = cssText.find(L'}', braceOpen);
            if (braceOpen == std::wstring::npos || braceClose == std::wstring::npos) {
                LOG_WARNING(L"[CSS Parser] Regula @page este invalidă sau incompletă.");
                pos = selectorStart + 1;
                continue;
            }

            std::wstring selector = trim(cssText.substr(selectorStart, braceOpen - selectorStart));
            std::wstring propertiesBlock = cssText.substr(braceOpen + 1, braceClose - braceOpen - 1);

            LOG_INFO(L"[CSS Parser] Regula @page detectată: " + selector);

            size_t propPos = 0;
            while (propPos < propertiesBlock.length()) {
                propPos = propertiesBlock.find_first_not_of(L" \t\n\r", propPos);
                if (propPos == std::wstring::npos) break;

                size_t colon = propertiesBlock.find(L':', propPos);
                if (colon == std::wstring::npos) break;

                size_t semicolon = propertiesBlock.find(L';', colon);
                if (semicolon == std::wstring::npos) semicolon = propertiesBlock.length();

                std::wstring propName = trim(propertiesBlock.substr(propPos, colon - propPos));
                std::wstring propValue = trim(propertiesBlock.substr(colon + 1, semicolon - colon - 1));

                setProperty(selector, propName, propValue);
                propPos = semicolon + 1;
            }

            pos = braceClose + 1;
            continue;
        }




        size_t braceOpen = cssText.find(L'{', selectorStart);
        if (braceOpen == std::wstring::npos) {
            LOG_ERROR(L"[CSS Parser] Eroare: Acolada '{' de deschidere lipsește după poziția: " + std::to_wstring(pos));
            break;
        }

        std::wstring compositeSelector = cssText.substr(selectorStart, braceOpen - selectorStart);

        size_t propertiesStart = braceOpen + 1;

        size_t braceClose = cssText.find(L'}', propertiesStart);
        if (braceClose == std::wstring::npos) {
            LOG_ERROR(L"[CSS Parser] Eroare: Acolada '}' de închidere lipsește pentru selectorul: " + trim(compositeSelector));
            break;
        }

        std::wstring propertiesBlock = cssText.substr(propertiesStart, braceClose - propertiesStart);

        LOG_INFO(L"[CSS Parser] Bloc de reguli găsit. Selector(i): " + trim(compositeSelector));

        std::vector<std::wstring> individualSelectors = splitSelectors(compositeSelector);

        for (const auto& individualSelector : individualSelectors) {

            size_t propPos = 0;

            while (propPos < propertiesBlock.length()) {

                // Sări peste spațiile albe la începutul fiecărei proprietăți
                propPos = propertiesBlock.find_first_not_of(L" \t\n\r", propPos);
                if (propPos == std::wstring::npos) break;


                size_t colon = propertiesBlock.find(L':', propPos);

                if (colon == std::wstring::npos) break; // Nu mai sunt perechi nume:valoare

                size_t semicolon = propertiesBlock.find(L';', colon);

                if (semicolon == std::wstring::npos) {
                    semicolon = propertiesBlock.length();
                }

                // Extragerea numelui
                std::wstring propName = propertiesBlock.substr(propPos, colon - propPos);

                // Extragerea valorii
                std::wstring propValue = propertiesBlock.substr(colon + 1, semicolon - (colon + 1));
                LOG_INFO(L"[CSS DEBUG] Stocare: Selector='" + individualSelector +
                    L"', Prop='" + trim(propName) +
                    L"', Value='" + trim(propValue) + L"'");
                // Setează proprietatea folosind selectorul INDIVIDUAL
                setProperty(individualSelector, propName, propValue);

                // Trece la următoarea proprietate (după ';')
                propPos = semicolon + 1;
            }
        }

        // Trece la următoarea regulă (după '}')
        pos = braceClose + 1;
    }

    LOG_SUCCESS(L"[CSS Parser] Parsare CSS finalizată. Au fost extrase " + std::to_wstring(m_rules.size()) + L" reguli.");
    return true;
}

#ifndef CSS_COLOR_SELECTOR
#define CSS_COLOR_SELECTOR (FOREGROUND_INTENSITY | FOREGROUND_RED) // Roșu strălucitor
#endif
#ifndef CSS_COLOR_BRACKET
#define CSS_COLOR_BRACKET (FOREGROUND_INTENSITY) // Alb intens
#endif
#ifndef CSS_COLOR_PROPERTY
#define CSS_COLOR_PROPERTY (FOREGROUND_GREEN) // Verde
#endif
#ifndef CSS_COLOR_VALUE
#define CSS_COLOR_VALUE (FOREGROUND_INTENSITY | FOREGROUND_BLUE) // Albastru strălucitor
#endif
#ifndef CSS_COLOR_DEFAULT
#define CSS_COLOR_DEFAULT (FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE) // Alb
#endif

/*
void CssDefinition::printRules() const {

    ConsoleManager& console = ConsoleManager::getInstance();

    // Titlu general
    console.setColor(CSS_COLOR_DEFAULT);
    std::wcout << L"\n--- CSS Rules Extracted (" << m_rules.size() << L" total) ---\n";

    if (m_rules.empty()) {
        std::wcout << L"Nu au fost găsite reguli CSS." << std::endl;
        console.resetColor();
        return;
    }

    for (const auto& rule : m_rules) {

        // 1. Afișează Selectorul (ex: html, body)
        console.setColor(CSS_COLOR_SELECTOR);
        std::wcout << rule.selector;

        // 2. Afișează acolada de deschidere
        console.setColor(CSS_COLOR_BRACKET);
        std::wcout << L" {\n";

        // 3. Afișează Proprietățile
        for (const auto& prop : rule.properties) {

            // Indentare
            std::wcout << L"    ";

            // Numele proprietății (ex: margin)
            console.setColor(CSS_COLOR_PROPERTY);
            std::wcout << prop.first;

            // Separatorul (:)
            console.setColor(CSS_COLOR_BRACKET);
            std::wcout << L": ";

            // Valoarea proprietății (ex: 0)
            console.setColor(CSS_COLOR_VALUE);
            std::wcout << prop.second;

            // Punct și virgulă (;)
            console.setColor(CSS_COLOR_BRACKET);
            std::wcout << L";\n";
        }

        // 4. Afișează acolada de închidere
        console.setColor(CSS_COLOR_BRACKET);
        std::wcout << L"}\n\n";
    }

    console.resetColor();
    std::wcout << L"--------------------------------------------------\n" << std::endl;
}
*/

void CssDefinition::printRules() const {
    // 1. Titlu general folosind macroul standard
    LOG_INFO(L"--- CSS Rules Extracted (" + std::to_wstring(m_rules.size()) + L" total) ---");

    if (m_rules.empty()) {
        LOG_WARNING(L"Nu au fost găsite reguli CSS de afișat.");
        return;
    }

    for (const auto& rule : m_rules) {
        // 2. Afișează Selectorul (ex: html, body)
        LOG_RAW(rule.selector, CSS_COLOR_SELECTOR);

        // 3. Afișează acolada de deschidere (fără newline la LOG_RAW dacă vrei să fie pe aceeași linie, 
        // dar definitia ta de LOG_RAW include std::endl, deci concatenăm)
        LOG_RAW(L" {", CSS_COLOR_BRACKET);

        // 4. Afișează Proprietățile
        for (const auto& prop : rule.properties) {
            // Construim linia de proprietate pentru a evita apeluri multiple de logare pe o singură linie
            // Format: "    margin: 0;"

            // Indentare manuală
            std::wstring propLine = L"    ";

            // Logăm pe bucăți pentru a păstra culorile diferite per element
            ConsoleManager::getInstance().setColor(CSS_COLOR_PROPERTY);
            LOG_RAW(L"    " + prop.first, CSS_COLOR_PROPERTY);

            LOG_RAW(L": ", CSS_COLOR_BRACKET);
            LOG_RAW(prop.second, CSS_COLOR_VALUE);
            LOG_RAW(L";", CSS_COLOR_BRACKET);
        }

        // 5. Afișează acolada de închidere
        LOG_RAW(L"}\n", CSS_COLOR_BRACKET);
    }

    // Linie de final
    LOG_RAW(L"--------------------------------------------------", CSS_COLOR_DEFAULT);
}

std::map<std::wstring, std::wstring> CssDefinition::getRule(const std::wstring& selector) const {
    std::wstring trimmedSelector = wstr_trim(selector);

    auto it = std::find_if(m_rules.begin(), m_rules.end(),
        [&trimmedSelector](const CssRule& r) {
            return r.selector == trimmedSelector;
        });

    if (it != m_rules.end()) {
        return it->properties;
    }

    return {}; // map gol dacă selectorul nu există
}

